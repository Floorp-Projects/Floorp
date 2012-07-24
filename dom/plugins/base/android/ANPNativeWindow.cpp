/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// must include config.h first for webkit to fiddle with new/delete
#include <android/log.h>
#include "AndroidBridge.h"
#include "ANPBase.h"
#include "nsIPluginInstanceOwner.h"
#include "nsPluginInstanceOwner.h"
#include "nsNPAPIPluginInstance.h"
#include "gfxRect.h"

using namespace mozilla;
using namespace mozilla;

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoPlugins" , ## args)
#define ASSIGN(obj, name)   (obj)->name = anp_native_window_##name

static ANPNativeWindow anp_native_window_acquireNativeWindow(NPP instance) {
  nsNPAPIPluginInstance* pinst = static_cast<nsNPAPIPluginInstance*>(instance->ndata);
  return pinst->AcquireContentWindow();
}

static void anp_native_window_invertPluginContent(NPP instance, bool isContentInverted) {
  nsNPAPIPluginInstance* pinst = static_cast<nsNPAPIPluginInstance*>(instance->ndata);
  pinst->SetInverted(isContentInverted);
}


void InitNativeWindowInterface(ANPNativeWindowInterfaceV0* i) {
    ASSIGN(i, acquireNativeWindow);
    ASSIGN(i, invertPluginContent);
}
