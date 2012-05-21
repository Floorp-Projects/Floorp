/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  static char *dir = NULL;

  if (!dir) {
    dir = getenv("ANDROID_PLUGIN_DATADIR");
  }

  LOG("getApplicationDataDirectory return %s", dir);
  return dir;
}

const char*
anp_system_getApplicationDataDirectory(NPP instance)
{
  return anp_system_getApplicationDataDirectory();
}

jclass anp_system_loadJavaClass(NPP instance, const char* className)
{
  LOG("%s", __PRETTY_FUNCTION__);

  JNIEnv* env = GetJNIForThread();
  if (!env)
    return nsnull;

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

void anp_system_setPowerState(NPP instance, ANPPowerState powerState)
{
  NOT_IMPLEMENTED();
}

void InitSystemInterface(ANPSystemInterfaceV0 *i) {
  _assert(i->inSize == sizeof(*i));
  ASSIGN(i, getApplicationDataDirectory);
  ASSIGN(i, loadJavaClass);
}

void InitSystemInterfaceV1(ANPSystemInterfaceV1 *i) {
  _assert(i->inSize == sizeof(*i));
  ASSIGN(i, getApplicationDataDirectory);
  ASSIGN(i, loadJavaClass);
  ASSIGN(i, setPowerState);
}

void InitSystemInterfaceV2(ANPSystemInterfaceV2 *i) {
  _assert(i->inSize == sizeof(*i));
  ASSIGN(i, getApplicationDataDirectory);
  ASSIGN(i, loadJavaClass);
  ASSIGN(i, setPowerState);
}
