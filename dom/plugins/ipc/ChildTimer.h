/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_plugins_ChildTimer_h
#define mozilla_plugins_ChildTimer_h

#include "PluginMessageUtils.h"
#include "npapi.h"
#include "base/timer.h"

namespace mozilla {
namespace plugins {

class PluginInstanceChild;
typedef void (*TimerFunc)(NPP npp, uint32_t timerID);

class ChildTimer
{
public:
  /**
   * If initialization failed, ID() will return 0.
   */
  ChildTimer(PluginInstanceChild* instance,
             uint32_t interval,
             bool repeat,
             TimerFunc func);
  ~ChildTimer() { }

  uint32_t ID() const { return mID; }

  class IDComparator
  {
  public:
    bool Equals(ChildTimer* t, uint32_t id) const {
      return t->ID() == id;
    }
  };

private:
  PluginInstanceChild* mInstance;
  TimerFunc mFunc;
  bool mRepeating;
  uint32_t mID;
  base::RepeatingTimer<ChildTimer> mTimer;

  void Run();

  static uint32_t gNextTimerID;
};

} // namespace plugins
} // namespace mozilla

#endif // mozilla_plugins_ChildTimer_h
