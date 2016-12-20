/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../ppapi/out/rpc.cc"

typedef void (*RpcFromPlugin)(const char*, bool, char**);
RpcFromPlugin gRpcFromPlugin;
PP_GetInterface_Func gGetInterface;

void ToHost(std::stringstream &s, bool abortIfNonMainThread) {
  gRpcFromPlugin(s.str().c_str(), abortIfNonMainThread, nullptr);
}

std::string ToHostWithResult(std::stringstream &s, bool abortIfNonMainThread) {
  char* result;
  gRpcFromPlugin(s.str().c_str(), abortIfNonMainThread, &result);
  return result;
}

static const void* GetInterfaceForRPC(const char* interfaceName) {
  const void* _interface = gInterfaces[interfaceName];
  if (!_interface) {
    printf("MISSING INTERFACE %s\n", interfaceName);
  }
  return _interface;
}

void
Fail(const char *reason, const char *data)
{
  fprintf(stdout, reason, data);
  fflush(stdout);
  exit(-1);
}

char*
CallFromJSON(JSONIterator& iterator)
{
  iterator.expectObjectAndGotoFirstProperty();
  const Token& propertyName = iterator.getCurrentStringAndGotoNext();
  if (!propertyName.value().compare("__interface")) {
    const Token& interfaceNameToken = iterator.getCurrentStringAndGotoNext();
    string interfaceName = interfaceNameToken.value();
    const Token& instanceToken = iterator;
    const void* _interface;
    if (!instanceToken.value().compare("__instance")) {
      iterator.skip();
      void* instance;
      FromJSON_mem_t(iterator, instance);
      _interface = instance;
    } else {
      _interface = gGetInterface(interfaceName.c_str());
    }
    if (!_interface) {
      printf("Missing interface %s\n", interfaceName.c_str());
      return nullptr;
    }
    return gInterfaceMemberCallers[interfaceName](_interface, iterator);
  }

  if (!propertyName.value().compare("__callback")) {
    const Token& callbackNameToken = iterator.getCurrentAndGotoNext();
    if (!callbackNameToken.isString()) {
      return nullptr;
    }
    if (!callbackNameToken.value().compare("PP_CompletionCallback")) {
      iterator.skip();
      PP_CompletionCallback callback;
      FromJSON_PP_CompletionCallback(iterator, callback);
      iterator.skip();
      int32_t result;
      FromJSON_int32_t(iterator, result);
      PP_RunCompletionCallback(&callback, result);
      return nullptr;
    }
    if (!callbackNameToken.value().compare("PPB_Audio_Callback_1_0")) {
      iterator.skip();
      PPB_Audio_Callback_1_0 callback;
      FromJSON_PPB_Audio_Callback(iterator, callback);
      iterator.skip();
      void* sample_buffer;
      FromJSON_mem_t(iterator, sample_buffer);
      iterator.skip();
      uint32_t buffer_size_in_bytes;
      FromJSON_uint32_t(iterator, buffer_size_in_bytes);
      iterator.skip();
      void* user_data;
      FromJSON_mem_t(iterator, user_data);
      callback(sample_buffer, buffer_size_in_bytes, user_data);
      return nullptr;
    }

    Fail("Don't have code for callback ", callbackNameToken.value().c_str());
    return nullptr;
  }

  Fail("Don't know what to do with a call for ", propertyName.value().c_str());
  return nullptr;
}

#ifdef __cplusplus
extern "C" {
#endif

PP_EXPORT char*
CallFromJSON(const char* json)
{
  JSONIterator iterator(json);

  const Token& item = iterator;
  if (item.isArray()) {
    iterator.skip();
    std::string result("[");
    for (size_t i = 0; i < item.children(); ++i) {
      const char* r = CallFromJSON(iterator);
      if (!r) {
        result.append("null");
      } else {
        result.append(r);
        delete r;
      }
      if (i + 1 != item.children()) {
        result.append(", ");
      }
    }
    result.append("]");
    return strdup(result.c_str());
  }

  return CallFromJSON(iterator);
}

PP_EXPORT void
Initialize(RpcFromPlugin aRpcFromPlugin,
           PP_GetInterface_Func aGetInterface,
           PP_InitializeModule_Func aInitializeModule) {
  gRpcFromPlugin = aRpcFromPlugin;
  gGetInterface = aGetInterface;
  InitializeInterfaceList();
  aInitializeModule(1, GetInterfaceForRPC);
}

#ifdef __cplusplus
}  /* extern "C" */
#endif

void FromJSON_PP_Flash_Menu(JSONIterator& iterator, PP_Flash_Menu *&value) {
  const JSON::Token& pointer = iterator.getCurrentAndGotoNext();
  if (!pointer.isObject()) {
    if (!pointer.isPrimitive() || !pointer.value().compare("null")) {
      Fail("Not a pointer value!", "");
    }
    value = nullptr;
  } else {
    value = new PP_Flash_Menu();
    FromJSON_PP_Flash_Menu(iterator, *value);
  }
}

void FromJSON_PP_DirContents_Dev(JSONIterator& iterator, PP_DirContents_Dev *&value)
{
  value = new PP_DirContents_Dev();
  const JSON::Token& current = iterator.getCurrentAndGotoNext();
  if (current.isPrimitive() && !current.value().compare("null")) {
    return;
  }

  if (!current.isObject()) {
    Fail("Expected object!", "");
  }

  iterator.skip();
  FromJSON_int32_t(iterator, value->count);

  iterator.skip();
  size_t children = iterator.expectArrayAndGotoFirstItem();
  if (children > value->count) {
    Fail("Too many items in array\n", "");
  }

  value->entries = new PP_DirEntry_Dev[value->count];
  for (uint32_t _n = 0; _n < children; ++_n) {
    FromJSON_PP_DirEntry_Dev(iterator, value->entries[_n]);
  }
  // FIXME Null out remaining items?
}
