/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "assert.h"
#include "ANPBase.h"
#include <android/log.h>
#include "nsThreadUtils.h"
#include "nsNPAPIPluginInstance.h"
#include "nsNPAPIPlugin.h"

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoPlugins" , ## args)
#define ASSIGN(obj, name)   (obj)->name = anp_event_##name

void
anp_event_postEvent(NPP instance, const ANPEvent* event)
{
  LOG("%s", __PRETTY_FUNCTION__);

  nsNPAPIPluginInstance* pinst = static_cast<nsNPAPIPluginInstance*>(instance->ndata);
  pinst->PostEvent((void*) event);

  LOG("returning from %s", __PRETTY_FUNCTION__);
}


void InitEventInterface(ANPEventInterfaceV0 *i) {
  _assert(i->inSize == sizeof(*i));
  ASSIGN(i, postEvent);
}
