/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PreallocatedProcessManager_h
#define mozilla_PreallocatedProcessManager_h

#include "base/basictypes.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"

namespace mozilla {
namespace dom {
class ContentParent;
}

/**
 * This class manages a ContentParent that it starts up ahead of any particular
 * need.  You can then call Take() to get this process and use it.  Since we
 * already started it up, it should be ready for use faster than if you'd
 * created the process when you needed it.
 *
 * This class watches the dom.ipc.processPrelaunch.enabled pref.  If it changes
 * from false to true, it preallocates a process.  If it changes from true to
 * false, it kills the preallocated process, if any.
 *
 * We don't expect this pref to flip between true and false in production, but
 * flipping the pref is important for tests.
 *
 * The static methods here are implemented by forwarding calls on to a
 * PreallocatedProcessManagerImpl singleton class, so if you add a new static
 * method here, you'll need to write a corresponding public method on the
 * singleton.
 */
class PreallocatedProcessManager final
{
  typedef mozilla::dom::ContentParent ContentParent;

public:
  /**
   * Create a process after a delay.  We wait for a period of time (specified
   * by the dom.ipc.processPrelaunch.delayMs pref), then wait for this process
   * to go idle, then allocate the new process.
   *
   * If the dom.ipc.processPrelaunch.enabled pref is false, or if we already
   * have a preallocated process, this function does nothing.
   */
  static void AllocateAfterDelay();

  /**
   * Create a process once this process goes idle.
   *
   * If the dom.ipc.processPrelaunch.enabled pref is false, or if we already
   * have a preallocated process, this function does nothing.
   */
  static void AllocateOnIdle();

  /**
   * Create a process right now.
   *
   * If the dom.ipc.processPrelaunch.enabled pref is false, or if we already
   * have a preallocated process, this function does nothing.
   */
  static void AllocateNow();

  /**
   * Take the preallocated process, if we have one.  If we don't have one, this
   * returns null.
   *
   * If you call Take() twice in a row, the second call is guaranteed to return
   * null.
   *
   * After you Take() the preallocated process, you need to call one of the
   * Allocate* functions (or change the dom.ipc.processPrelaunch pref from
   * false to true) before we'll create a new process.
   */
  static already_AddRefed<ContentParent> Take();

#ifdef MOZ_NUWA_PROCESS
  static void PublishSpareProcess(ContentParent* aContent);
  static void MaybeForgetSpare(ContentParent* aContent);
  static bool IsNuwaReady();
  static void OnNuwaReady();
  static bool PreallocatedProcessReady();
#endif

private:
  PreallocatedProcessManager();
  DISALLOW_EVIL_CONSTRUCTORS(PreallocatedProcessManager);
};

} // namespace mozilla

#endif // defined mozilla_PreallocatedProcessManager_h
