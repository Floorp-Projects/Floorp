/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_plugins_ChildAsyncCall_h
#define mozilla_plugins_ChildAsyncCall_h

#include "PluginMessageUtils.h"
#include "base/task.h"

namespace mozilla {
namespace plugins {

typedef void (*PluginThreadCallback)(void*);

class PluginInstanceChild;

class ChildAsyncCall : public CancelableTask
{
public:
  ChildAsyncCall(PluginInstanceChild* instance,
                 PluginThreadCallback aFunc, void* aUserData);

  NS_OVERRIDE void Run();
  NS_OVERRIDE void Cancel();
  
protected:
  PluginInstanceChild* mInstance;
  PluginThreadCallback mFunc;
  void* mData;

  void RemoveFromAsyncList();
};

} // namespace plugins
} // namespace mozilla

#endif // mozilla_plugins_ChildAsyncCall_h
