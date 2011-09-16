#include "base/basictypes.h"


#include "assert.h"
#include "ANPBase.h"
#include <android/log.h>
#include "nsNPAPIPluginInstance.h"
#include "AndroidBridge.h"
#include "nsNPAPIPlugin.h"
#include "PluginPRLibrary.h"

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoPlugins" , ## args)
#define ASSIGN(obj, name)   (obj)->name = anp_system_##name

const char*
anp_system_getApplicationDataDirectory()
{
  LOG("getApplicationDataDirectory return /data/data/org.mozilla.%s", MOZ_APP_NAME);
  return "/data/data/org.mozilla." MOZ_APP_NAME;
}

jclass anp_system_loadJavaClass(NPP instance, const char* className)
{
  LOG("%s", __PRETTY_FUNCTION__);

  JNIEnv* env = GetJNIForThread();
  jclass cls = env->FindClass("org/mozilla/gecko/GeckoAppShell");
  jmethodID method = env->GetStaticMethodID(cls,
                                            "loadPluginClass",
                                            "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/Class;");

  // pass libname and classname, gotta create java strings
  nsNPAPIPluginInstance* pinst = static_cast<nsNPAPIPluginInstance*>(instance->ndata);
  mozilla::PluginPRLibrary* lib = static_cast<mozilla::PluginPRLibrary*>(pinst->GetPlugin()->GetLibrary());

  nsCString libName;
  lib->GetLibraryPath(libName);

  jstring jclassName = env->NewStringUTF(className);
  jstring jlibName = env->NewStringUTF(libName.get());
  jobject obj = env->CallStaticObjectMethod(cls, method, jclassName, jlibName);
  return reinterpret_cast<jclass>(obj);
}

void InitSystemInterface(ANPSystemInterfaceV0 *i) {
  _assert(i->inSize == sizeof(*i));
  ASSIGN(i, getApplicationDataDirectory);
  ASSIGN(i, loadJavaClass);
}
