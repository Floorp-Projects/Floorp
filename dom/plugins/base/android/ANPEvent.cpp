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
 *   Doug Turner <dougt@mozilla.com>
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

#include "assert.h"
#include "ANPBase.h"
#include <android/log.h>
#include "nsThreadUtils.h"
#include "nsNPAPIPluginInstance.h"
#include "AndroidBridge.h"
#include "nsNPAPIPlugin.h"

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoPlugins" , ## args)
#define ASSIGN(obj, name)   (obj)->name = anp_event_##name

class PluginEventRunnable : public nsRunnable
{
public:
  PluginEventRunnable(NPP inst, ANPEvent* event, NPPluginFuncs* aFuncs)
    : mInstance(inst), mEvent(*event), mFuncs(aFuncs) {}
  virtual nsresult Run() {
    (*mFuncs->event)(mInstance, &mEvent);
    return NS_OK;
  }
private:
  NPP mInstance;
  ANPEvent mEvent;
  NPPluginFuncs* mFuncs;
};

void
anp_event_postEvent(NPP inst, const ANPEvent* event)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!mozilla::AndroidBridge::Bridge()) {
    LOG("no bridge in %s!!!!", __PRETTY_FUNCTION__);
    return;
  }
  nsNPAPIPluginInstance* pinst = static_cast<nsNPAPIPluginInstance*>(inst->ndata);
  NPPluginFuncs* pluginFunctions = pinst->GetPlugin()->PluginFuncs();
  mozilla::AndroidBridge::Bridge()->PostToJavaThread(
    new PluginEventRunnable(inst, const_cast<ANPEvent*>(event), pluginFunctions), PR_TRUE);
  LOG("returning from %s", __PRETTY_FUNCTION__);
}


void InitEventInterface(ANPEventInterfaceV0 *i) {
  _assert(i->inSize == sizeof(*i));
  ASSIGN(i, postEvent);
}
