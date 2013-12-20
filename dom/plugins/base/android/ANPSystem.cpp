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
anp_system_getApplicationDataDirectory(NPP instance)
{
  static const char *dir = nullptr;
  static const char *privateDir = nullptr;

  bool isPrivate = false;

  if (!dir) {
    dir = getenv("ANDROID_PLUGIN_DATADIR");
  }

  if (!privateDir) {
    privateDir = getenv("ANDROID_PLUGIN_DATADIR_PRIVATE");
  }

  if (!instance) {
    return dir;
  }

  nsNPAPIPluginInstance* pinst = static_cast<nsNPAPIPluginInstance*>(instance->ndata);
  if (pinst && NS_SUCCEEDED(pinst->IsPrivateBrowsing(&isPrivate)) && isPrivate) {
    return privateDir;
  }

  return dir;
}

const char*
anp_system_getApplicationDataDirectory()
{
  return anp_system_getApplicationDataDirectory(nullptr);
}

jclass anp_system_loadJavaClass(NPP instance, const char* classNameStr)
{
  LOG("%s", __PRETTY_FUNCTION__);

  nsNPAPIPluginInstance* pinst = static_cast<nsNPAPIPluginInstance*>(instance->ndata);
  mozilla::PluginPRLibrary* lib = static_cast<mozilla::PluginPRLibrary*>(pinst->GetPlugin()->GetLibrary());

  NS_ConvertUTF8toUTF16 className(classNameStr);

  nsCString libNameUtf8;
  lib->GetLibraryPath(libNameUtf8);
  NS_ConvertUTF8toUTF16 libName(libNameUtf8);

  return GeckoAppShell::LoadPluginClass(className, libName);
}

void anp_system_setPowerState(NPP instance, ANPPowerState powerState)
{
  nsNPAPIPluginInstance* pinst = nsNPAPIPluginInstance::GetFromNPP(instance);

  if (pinst) {
    pinst->SetWakeLock(powerState == kScreenOn_ANPPowerState);
  }
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
