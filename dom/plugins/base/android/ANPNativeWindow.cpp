/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Android NPAPI support code
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   James Willcox <jwillcox@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// must include config.h first for webkit to fiddle with new/delete
#include <android/log.h>
#include "AndroidBridge.h"
#include "AndroidMediaLayer.h"
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

  nsPluginInstanceOwner* owner;
  if (NS_FAILED(pinst->GetOwner((nsIPluginInstanceOwner**)&owner))) {
    return NULL;
  }


  ANPNativeWindow window = owner->Layer()->GetNativeWindowForContent();
  owner->Invalidate();

  return window;
}

static void anp_native_window_invertPluginContent(NPP instance, bool isContentInverted) {
  nsNPAPIPluginInstance* pinst = static_cast<nsNPAPIPluginInstance*>(instance->ndata);

  nsPluginInstanceOwner* owner;
  if (NS_FAILED(pinst->GetOwner((nsIPluginInstanceOwner**)&owner))) {
    return;
  }

  owner->Layer()->SetInverted(isContentInverted);
}


void InitNativeWindowInterface(ANPNativeWindowInterfaceV0* i) {
    ASSIGN(i, acquireNativeWindow);
    ASSIGN(i, invertPluginContent);
}
