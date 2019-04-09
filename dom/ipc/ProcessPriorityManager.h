/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ProcessPriorityManager_h_
#define mozilla_ProcessPriorityManager_h_

#include "mozilla/HalTypes.h"

namespace mozilla {
namespace dom {
class ContentParent;
class BrowserParent;
}  // namespace dom

/**
 * This class sets the priority of subprocesses in response to explicit
 * requests and events in the system.
 *
 * A process's priority changes e.g. when it goes into the background via
 * mozbrowser's setVisible(false).  Process priority affects CPU scheduling and
 * also which processes get killed when we run out of memory.
 *
 * After you call Initialize(), the only thing you probably have to do is call
 * SetProcessPriority on processes immediately after creating them in order to
 * set their initial priority.  The ProcessPriorityManager takes care of the
 * rest.
 */
class ProcessPriorityManager final {
 public:
  /**
   * Initialize the ProcessPriorityManager machinery, causing the
   * ProcessPriorityManager to actively manage the priorities of all
   * subprocesses.  You should call this before creating any subprocesses.
   *
   * You should also call this function even if you're in a child process,
   * since it will initialize ProcessPriorityManagerChild.
   */
  static void Init();

  /**
   * Set the process priority of a given ContentParent's process.
   *
   * Note that because this method takes a ContentParent*, you can only set the
   * priority of your subprocesses.  In fact, because we don't support nested
   * content processes (bug 761935), you can only call this method from the
   * main process.
   *
   * It probably only makes sense to call this function immediately after a
   * process is created.  At this point, the process priority manager doesn't
   * have enough context about the processs to know what its priority should
   * be.
   *
   * Eventually whatever priority you set here can and probably will be
   * overwritten by the process priority manager.
   */
  static void SetProcessPriority(dom::ContentParent* aContentParent,
                                 hal::ProcessPriority aPriority);

  /**
   * Returns true iff this process's priority is FOREGROUND*.
   *
   * Note that because process priorities are set in the main process, it's
   * possible for this method to return a stale value.  So be careful about
   * what you use this for.
   */
  static bool CurrentProcessIsForeground();

  static void TabActivityChanged(dom::BrowserParent* aBrowserParent,
                                 bool aIsActive);

 private:
  ProcessPriorityManager();
  DISALLOW_EVIL_CONSTRUCTORS(ProcessPriorityManager);
};

}  // namespace mozilla

#endif
