/***************************************************************************
 *
 * Project         _____    __   ____   _      _
 *                (  _  )  /__\ (_  _)_| |_  _| |_
 *                 )(_)(  /(__)\  )( (_   _)(_   _)
 *                (_____)(__)(__)(__)  |_|    |_|
 *
 *
 * Copyright 2018-present, Leonid Stryzhevskyi <lganzzzo@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************************/

#include "Environment.hpp"
#include <iostream>
#include <cstring>
#include <stdarg.h>

namespace oatpp { namespace base {

Logger* Environment::m_logger = nullptr;
std::unordered_map<std::string, std::unordered_map<std::string, void*>> Environment::m_components;

v_atomicCounter Environment::m_objectsCount(0);
v_atomicCounter Environment::m_objectsCreated(0);
thread_local v_counter Environment::m_threadLocalObjectsCount = 0;
thread_local v_counter Environment::m_threadLocalObjectsCreated = 0;

void Environment::init(){
  checkTypes();

  m_objectsCount = 0;
  m_objectsCreated = 0;
  m_threadLocalObjectsCount = 0;
  m_threadLocalObjectsCreated = 0;

  if(m_components.size() > 0) {
    throw std::runtime_error("[oatpp::base::Environment]: Invalid state. Components were created before call to Environment::init()");
  }
}

void Environment::destroy(){
  if(m_components.size() > 0) {
    throw std::runtime_error("[oatpp::base::Environment]: Invalid state. Leaking components");
  }
}
  
void Environment::checkTypes(){
  
  OATPP_ASSERT(sizeof(v_char8) == 1);
  OATPP_ASSERT(sizeof(v_int16) == 2);
  OATPP_ASSERT(sizeof(v_word16) == 2);
  OATPP_ASSERT(sizeof(v_int32) == 4);
  OATPP_ASSERT(sizeof(v_int64) == 8);
  OATPP_ASSERT(sizeof(v_word32) == 4);
  OATPP_ASSERT(sizeof(v_word64) == 8);
  OATPP_ASSERT(sizeof(v_float64) == 8);
  
  v_int32 vInt32 = ~1;
  v_int64 vInt64 = ~1;
  v_word32 vWord32 = ~1;
  v_word64 vWord64 = ~1;
  
  OATPP_ASSERT(vInt32 < 0);
  OATPP_ASSERT(vInt64 < 0);
  OATPP_ASSERT(vWord32 > 0);
  OATPP_ASSERT(vWord64 > 0);
  
}

void Environment::incObjects(){
  m_objectsCount ++;
  m_objectsCreated ++;
  m_threadLocalObjectsCount ++;
  m_threadLocalObjectsCreated ++;
}

void Environment::decObjects(){
  m_objectsCount --;
  m_threadLocalObjectsCount --;
}

v_counter Environment::getObjectsCount(){
  return m_objectsCount;
}

v_counter Environment::getObjectsCreated(){
  return m_objectsCreated;
}
  
v_counter Environment::getThreadLocalObjectsCount(){
  return m_threadLocalObjectsCount;
}

v_counter Environment::getThreadLocalObjectsCreated(){
  return m_threadLocalObjectsCreated;
}

void Environment::setLogger(Logger* logger){
  if(m_logger != nullptr){
    delete m_logger;
  }
  m_logger = logger;
}

void Environment::printCompilationConfig() {

#ifdef OATPP_DISABLE_ENV_OBJECT_COUNTERS
  OATPP_LOGD("oatpp/Config", "OATPP_DISABLE_ENV_OBJECT_COUNTERS");
#endif

#ifdef OATPP_DISABLE_POOL_ALLOCATIONS
  OATPP_LOGD("oatpp/Config", "OATPP_DISABLE_POOL_ALLOCATIONS");
#endif

#ifdef OATPP_THREAD_HARDWARE_CONCURRENCY
  OATPP_LOGD("oatpp/Config", "OATPP_THREAD_HARDWARE_CONCURRENCY=%d", OATPP_THREAD_HARDWARE_CONCURRENCY);
#endif

  OATPP_LOGD("oatpp/Config", "OATPP_THREAD_DISTRIBUTED_MEM_POOL_SHARDS_COUNT=%d", OATPP_THREAD_DISTRIBUTED_MEM_POOL_SHARDS_COUNT);
  OATPP_LOGD("oatpp/Config", "OATPP_ASYNC_EXECUTOR_THREAD_NUM_DEFAULT=%d\n", OATPP_ASYNC_EXECUTOR_THREAD_NUM_DEFAULT);

}

void Environment::log(v_int32 priority, const std::string& tag, const std::string& message) {
  if(m_logger != nullptr) {
    m_logger->log(priority, tag, message);
  }
}
  
void Environment::logFormatted(v_int32 priority, const std::string& tag, const char* message, ...) {
  if(message == nullptr) {
    message = "[null]";
  }
  char buffer[4097];
  va_list args;
  va_start (args, message);
  vsnprintf(buffer, 4096, message, args);
  log(priority, tag, buffer);
  va_end(args);
}
  
void Environment::registerComponent(const std::string& typeName, const std::string& componentName, void* component) {
  auto& bucket = m_components[typeName];
  auto it = bucket.find(componentName);
  if(it != bucket.end()){
    throw std::runtime_error("[oatpp::base::Environment]: Component with given name already exists: name='" + componentName + "'");
  }
  bucket[componentName] = component;
}
  
void Environment::unregisterComponent(const std::string& typeName, const std::string& componentName) {
  auto bucketIt = m_components.find(typeName);
  if(bucketIt == m_components.end() || bucketIt->second.size() == 0) {
    throw std::runtime_error("[oatpp::base::Environment]: Component of given type does't exist: type='" + typeName + "'");
  }
  auto& bucket = bucketIt->second;
  auto componentIt = bucket.find(componentName);
  if(componentIt == bucket.end()) {
    throw std::runtime_error("[oatpp::base::Environment]: Component with given name does't exist: name='" + componentName + "'");
  }
  bucket.erase(componentIt);
  if(bucket.size() == 0) {
    m_components.erase(bucketIt);
  }
}

void* Environment::getComponent(const std::string& typeName) {
  auto bucketIt = m_components.find(typeName);
  if(bucketIt == m_components.end() || bucketIt->second.size() == 0) {
    throw std::runtime_error("[oatpp::base::Environment]: Component of given type does't exist: type='" + typeName + "'");
  }
  auto bucket = bucketIt->second;
  if(bucket.size() > 1){
    throw std::runtime_error("[oatpp::base::Environment]: Ambiguous component reference. Multiple components exist for a given type: type='" + typeName + "'");
  }
  return bucket.begin()->second;
}

void* Environment::getComponent(const std::string& typeName, const std::string& componentName) {
  auto bucketIt = m_components.find(typeName);
  if(bucketIt == m_components.end() || bucketIt->second.size() == 0) {
    throw std::runtime_error("[oatpp::base::Environment]: Component of given type does't exist: type='" + typeName + "'");
  }
  auto bucket = bucketIt->second;
  auto componentIt = bucket.find(componentName);
  if(componentIt == bucket.end()) {
    throw std::runtime_error("[oatpp::base::Environment]: Component with given name does't exist: name='" + componentName + "'");
  }
  return componentIt->second;
}
  
v_int64 Environment::getMicroTickCount(){
  std::chrono::microseconds ms = std::chrono::duration_cast<std::chrono::microseconds>
  (std::chrono::system_clock::now().time_since_epoch());
  return ms.count();
}
  
}}
