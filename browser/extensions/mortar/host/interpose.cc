/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../ppapi/out/rpc.cc"
#include <dlfcn.h>

#define REAL_PLUGIN_PATH "/Applications/Google Chrome.app/Contents/Versions/42.0.2311.135/Google Chrome Framework.framework/Internet Plug-Ins/PepperFlash/PepperFlashPlayer.plugin/Contents/MacOS/PepperFlashPlayer"
//#define REAL_PLUGIN_PATH "/usr/lib/pepperflashplugin-nonfree/libpepflashplayer.so"

static PPB_GetInterface _real_PPB_GetInterface;

const void* RealGetInterface(const char* interfaceName) {
  if (!strcmp(interfaceName, "PPB_Flash_File_FileRef;2.0")) {
    interfaceName = "PPB_Flash_File_FileRef;2";
  } else if (!strcmp(interfaceName, "PPB_Flash_File_ModuleLocal;3.0")) {
    interfaceName = "PPB_Flash_File_ModuleLocal;3";
  }
  return _real_PPB_GetInterface(interfaceName);
}
const void* GetInterfaceForRPC(const char* interfaceName) {
  const void* interface = gInterfaces[interfaceName];
  //printf("GetInterfaceForRPC %s\n", interfaceName);
  if (!interface) {
    printf("MISSING INTERFACE %s\n", interfaceName);
  }
  return interface;
}

void
Fail(const char *reason, const char *data)
{
  fprintf(stdout, reason, data);
  fflush(stdout);
  exit(-1);
}

void Logging_PP_CompletionCallback(void* user_data, int32_t result)
{
  PP_CompletionCallback* _real_PP_CompletionCallback = static_cast<PP_CompletionCallback*>(user_data);
  printf("callFromJSON: > {\"__callback\":\"PP_CompletionCallback\",\"__callbackStruct\":{\"func\":%lu,\"user_data\":%lu,\"flags\":%i},\"result\":%i}\n",
         uintptr_t(_real_PP_CompletionCallback->func), uintptr_t(_real_PP_CompletionCallback->user_data), _real_PP_CompletionCallback->flags, result);
  _real_PP_CompletionCallback->func(_real_PP_CompletionCallback->user_data, result);
  printf("callFromJSON: < {\"__callback\":\"PP_CompletionCallback\",\"__callbackStruct\":{\"func\":%lu,\"user_data\":%lu,\"flags\":%i},\"result\":%i}\n",
         uintptr_t(_real_PP_CompletionCallback->func), uintptr_t(_real_PP_CompletionCallback->user_data), _real_PP_CompletionCallback->flags, result);
  delete _real_PP_CompletionCallback;
}

void Logging_PPB_Audio_Callback_1_0(void* sample_buffer,
                                    uint32_t buffer_size_in_bytes,
                                    void* user_data)
{
  Logging_PPB_Audio_Callback_1_0_holder* holder = static_cast<Logging_PPB_Audio_Callback_1_0_holder*>(user_data);
  printf("callFromJSON: > {\"__callback\":\"PPB_Audio_Callback_1_0\",\"__callbackStruct\":{\"func\":%lu,\"sample_buffer\":%lu,\"buffer_size_in_bytes\":%i},\"user_data\":%lu}\n",
         uintptr_t(holder->func), uintptr_t(sample_buffer), buffer_size_in_bytes, uintptr_t(holder->user_data));
  holder->func(sample_buffer, buffer_size_in_bytes, holder->user_data);
  printf("callFromJSON: < {\"__callback\":\"PPB_Audio_Callback_1_0\",\"__callbackStruct\":{\"func\":%lu,\"sample_buffer\":%lu,\"buffer_size_in_bytes\":%i},\"user_data\":%lu}\n",
         uintptr_t(holder->func), uintptr_t(sample_buffer), buffer_size_in_bytes, uintptr_t(holder->user_data));
}

static void *_real_PepperFlash = nullptr;

typedef int32_t (*PPP_InitializeBroker_Func)(PP_ConnectInstance_Func* connect_instance_func);
typedef void (*PPP_ShutdownBroker_Func)(void);

static PP_InitializeModule_Func _real_PPP_InitializeModule;
static PP_GetInterface_Func _real_PPP_GetInterface;
static PPP_InitializeBroker_Func _real_PPP_InitializeBroker;
static PPP_ShutdownBroker_Func _real_PPP_ShutdownBroker;

static void
LoadRealPepperFlash()
{
  if (!_real_PepperFlash) {
    _real_PepperFlash = dlopen(REAL_PLUGIN_PATH, RTLD_LAZY);
    _real_PPP_InitializeModule = (PP_InitializeModule_Func)dlsym(_real_PepperFlash, "PPP_InitializeModule");
    _real_PPP_GetInterface = (PP_GetInterface_Func)dlsym(_real_PepperFlash, "PPP_GetInterface");
    _real_PPP_InitializeBroker = (PPP_InitializeBroker_Func)dlsym(_real_PepperFlash, "PPP_InitializeBroker");
    _real_PPP_ShutdownBroker = (PPP_ShutdownBroker_Func)dlsym(_real_PepperFlash, "PPP_ShutdownBroker");
    InitializeInterfaceList();
  }
}

struct Logging_PPP_Class_Deprecated_holder;

PP_Bool
Logging_HasProperty(const void* object,
                    PP_Var name,
                    PP_Var* exception)
{
  uint32_t varNameLength;
  const char* varName = ((PPB_Var_Deprecated_0_3*)RealGetInterface("PPB_Var(Deprecated);0.3"))->VarToUtf8(name, &varNameLength);
  printf("Logging_HasProperty for ");
  for (uint32_t i = 0; i < varNameLength; ++i) {
    printf("%c", varName[i]);
  }
  printf("\n");
  const Logging_PPP_Class_Deprecated_holder* holder = static_cast<const Logging_PPP_Class_Deprecated_holder*>(object);
  PP_Bool result = holder->_real_PPP_Class_Deprecated->HasProperty(holder->object, name, exception);
  printf("Logging_HasProperty for ");
  for (uint32_t i = 0; i < varNameLength; ++i) {
    printf("%c", varName[i]);
  }
  printf(" returns %s\n", result == PP_TRUE ? "true" : "false");
  return result;
}

PP_Bool
Logging_HasMethod(const void* object,
                  PP_Var name,
                  PP_Var* exception)
{
  uint32_t varNameLength;
  const char* varName = ((PPB_Var_Deprecated_0_3*)RealGetInterface("PPB_Var(Deprecated);0.3"))->VarToUtf8(name, &varNameLength);
  printf("Logging_HasMethod for ");
  for (uint32_t i = 0; i < varNameLength; ++i) {
    printf("%c", varName[i]);
  }
  printf("\n");
  const Logging_PPP_Class_Deprecated_holder* holder = static_cast<const Logging_PPP_Class_Deprecated_holder*>(object);
  PP_Bool result = holder->_real_PPP_Class_Deprecated->HasMethod(holder->object, name, exception);
  printf("Logging_HasMethod for ");
  for (uint32_t i = 0; i < varNameLength; ++i) {
    printf("%c", varName[i]);
  }
  printf(" returns %s\n", result == PP_TRUE ? "true" : "false");
  return result;
}

PP_Var
Logging_GetProperty(const void* object,
                    PP_Var name,
                    PP_Var* exception)
{
  printf("Logging_GetProperty\n");
  const Logging_PPP_Class_Deprecated_holder* holder = static_cast<const Logging_PPP_Class_Deprecated_holder*>(object);
  return holder->_real_PPP_Class_Deprecated->GetProperty(holder->object, name, exception);
}

void
Logging_GetAllPropertyNames(const void* object,
                            uint32_t* property_count,
                            PP_Var** properties,
                            PP_Var* exception)
{
  printf("Logging_GetAllPropertyNames\n");
  const Logging_PPP_Class_Deprecated_holder* holder = static_cast<const Logging_PPP_Class_Deprecated_holder*>(object);
  holder->_real_PPP_Class_Deprecated->GetAllPropertyNames(holder->object, property_count, properties, exception);
}

void
Logging_SetProperty(const void* object,
                    PP_Var name,
                    PP_Var value,
                    PP_Var* exception)
{
  printf("Logging_SetProperty\n");
  const Logging_PPP_Class_Deprecated_holder* holder = static_cast<const Logging_PPP_Class_Deprecated_holder*>(object);
  holder->_real_PPP_Class_Deprecated->SetProperty(holder->object, name, value, exception);
}

void
Logging_RemoveProperty(const void* object,
                       PP_Var name,
                       PP_Var* exception)
{
  printf("Logging_RemoveProperty\n");
  const Logging_PPP_Class_Deprecated_holder* holder = static_cast<const Logging_PPP_Class_Deprecated_holder*>(object);
  holder->_real_PPP_Class_Deprecated->RemoveProperty(holder->object, name, exception);
}

PP_Var
Logging_Call(const void* object,
             PP_Var method_name,
             uint32_t argc,
             const PP_Var argv[],
             PP_Var* exception)
{
  printf("Logging_Call\n");
  const Logging_PPP_Class_Deprecated_holder* holder = static_cast<const Logging_PPP_Class_Deprecated_holder*>(object);
  return holder->_real_PPP_Class_Deprecated->Call(holder->object, method_name, argc, argv, exception);
}

PP_Var
Logging_Construct(const void* object,
                  uint32_t argc,
                  const PP_Var argv[],
                  PP_Var* exception)
{
  printf("Logging_Construct\n");
  const Logging_PPP_Class_Deprecated_holder* holder = static_cast<const Logging_PPP_Class_Deprecated_holder*>(object);
  return holder->_real_PPP_Class_Deprecated->Construct(holder->object, argc, argv, exception);
}

void
Logging_Deallocate(const void* object)
{
  printf("Logging_Deallocate\n");
  const Logging_PPP_Class_Deprecated_holder* holder = static_cast<const Logging_PPP_Class_Deprecated_holder*>(object);
  holder->_real_PPP_Class_Deprecated->Deallocate(holder->object);
  delete holder;
}

const PPP_Class_Deprecated _interpose_PPP_Class_Deprecated_1_0 = {
  Logging_HasProperty,
  Logging_HasMethod,
  Logging_GetProperty,
  Logging_GetAllPropertyNames,
  Logging_SetProperty,
  Logging_RemoveProperty,
  Logging_Call,
  Logging_Construct,
  Logging_Deallocate,
};

#ifdef __cplusplus
extern "C" {
#endif

static PP_Bool
Logging_HandleInputEvent(PP_Instance instance, PP_Resource input_event)
{
  const PPP_InputEvent_0_1* _real_PPP_InputEvent = static_cast<const PPP_InputEvent_0_1*>(_real_PPP_GetInterface("PPP_InputEvent;0.1"));
  printf("callFromJSON: > {\"__interface\":\"PPP_InputEvent;0.1\",\"__member\":\"HandleInputEvent\",\"instance\":%i,\"input_event\":%i}\n", instance, input_event);
  PP_Bool result = _real_PPP_InputEvent->HandleInputEvent(instance, input_event);
  printf("callFromJSON: < \"%s\"\n", ToString_PP_Bool(result).c_str());
  return result;
}

static const PPP_InputEvent_0_1 _interpose_PPP_InputEvent_0_1 = {
  Logging_HandleInputEvent
};

static PP_Bool
Logging_DidCreate(PP_Instance instance,
                  uint32_t argc,
                  const char* argn[],
                  const char* argv[])
{
  const PPP_Instance_1_1* _real_PPP_Instance = static_cast<const PPP_Instance_1_1*>(_real_PPP_GetInterface("PPP_Instance;1.1"));
  printf("callFromJSON: > {\"__interface\":\"PPP_Instance;1.1\",\"__member\":\"DidCreate\",\"instance\":%i,\"argc\":%i,\"argn\":[", instance, argc);
  for (uint32_t i = 0; i < argc; ++i) {
    if (i > 0) {
      printf(",");
    }
    printf("\"%s\"", argn[i]);
  }
  printf("],\"argv\":[");
  for (uint32_t i = 0; i < argc; ++i) {
    if (i > 0) {
      printf(",");
    }
    printf("\"%s\"", argv[i]);
  }
  printf("]}\n");
  PP_Bool result = _real_PPP_Instance->DidCreate(instance, argc, argn, argv);
  printf("callFromJSON: < \"%s\"\n", ToString_PP_Bool(result).c_str());
  return result;
}
static void
Logging_DidDestroy(PP_Instance instance)
{
  const PPP_Instance_1_1* _real_PPP_Instance = static_cast<const PPP_Instance_1_1*>(_real_PPP_GetInterface("PPP_Instance;1.1"));
  printf("callFromJSON: > {\"__interface\":\"PPP_Instance;1.1\",\"__member\":\"DidDestroy\",\"instance\":%i}\n", instance);
  _real_PPP_Instance->DidDestroy(instance);
}
static void
Logging_DidChangeView(PP_Instance instance, PP_Resource view)
{
  const PPP_Instance_1_1* _real_PPP_Instance = static_cast<const PPP_Instance_1_1*>(_real_PPP_GetInterface("PPP_Instance;1.1"));
  printf("callFromJSON: > {\"__interface\":\"PPP_Instance;1.1\",\"__member\":\"DidChangeView\",\"instance\":%i,\"view\":%i}\n", instance, view);
  _real_PPP_Instance->DidChangeView(instance, view);
}
static void
Logging_DidChangeFocus(PP_Instance instance, PP_Bool has_focus)
{
  const PPP_Instance_1_1* _real_PPP_Instance = static_cast<const PPP_Instance_1_1*>(_real_PPP_GetInterface("PPP_Instance;1.1"));
  printf("callFromJSON: > {\"__interface\":\"PPP_Instance;1.1\",\"__member\":\"DidChangeFocus\",\"instance\":%i,\"has_focus\":%s}\n", instance, has_focus ? "PP_TRUE" : "PP_FALSE");
  _real_PPP_Instance->DidChangeFocus(instance, has_focus);
}
static PP_Bool
Logging_HandleDocumentLoad(PP_Instance instance, PP_Resource url_loader)
{
  const PPP_Instance_1_1* _real_PPP_Instance = static_cast<const PPP_Instance_1_1*>(_real_PPP_GetInterface("PPP_Instance;1.1"));
  printf("callFromJSON: > {\"__interface\":\"PPP_Instance;1.1\",\"__member\":\"HandleDocumentLoad\",\"instance\":%i,\"url_loader\":%i}\n", instance, url_loader);
  PP_Bool result = _real_PPP_Instance->HandleDocumentLoad(instance, url_loader);
  printf("callFromJSON: < \"%s\"\n", ToString_PP_Bool(result).c_str());
  return result;
}

static const PPP_Instance_1_1 _interpose_PPP_Instance_1_1 = {
  Logging_DidCreate,
  Logging_DidDestroy,
  Logging_DidChangeView,
  Logging_DidChangeFocus,
  Logging_HandleDocumentLoad
};

const void *
PPP_GetInterface(const char *interface_name)
{
//printf("PPP_GetInterface %s\n", interface_name);
  LoadRealPepperFlash();
  if (!strcmp(interface_name, "PPP_InputEvent;0.1")) {
    return &_interpose_PPP_InputEvent_0_1;
  }
  if (!strcmp(interface_name, "PPP_Instance;1.1")) {
    return &_interpose_PPP_Instance_1_1;
  }
  return _real_PPP_GetInterface(interface_name);
}

int32_t
PPP_InitializeModule(PP_Module module, PPB_GetInterface get_browser_interface)
{
  LoadRealPepperFlash();
  _real_PPB_GetInterface = get_browser_interface;
  return _real_PPP_InitializeModule(module, GetInterfaceForRPC);
}

int32_t
PPP_InitializeBroker(PP_ConnectInstance_Func *connect_instance_func)
{
  return _real_PPP_InitializeBroker(connect_instance_func);
}

void
PPP_ShutdownBroker()
{
  return _real_PPP_ShutdownBroker();
}

#ifdef __cplusplus
}  /* extern "C" */
#endif
