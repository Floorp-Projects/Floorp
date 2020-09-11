/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PreallocatedProcessManager_h
#define mozilla_PreallocatedProcessManager_h

#include "base/basictypes.h"
#include "mozilla/AlreadyAddRefed.h"
#include "nsStringFwd.h"

namespace mozilla {
namespace dom {
class ContentParent;
}  // namespace dom

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
 */
class PreallocatedProcessManagerImpl;

class PreallocatedProcessManager final {
  typedef mozilla::dom::ContentParent ContentParent;

 public:
  static PreallocatedProcessManagerImpl* GetPPMImpl();

  /**
   * Before first paint we don't want to allocate any processes in the
   * background. To avoid that, the PreallocatedProcessManager won't start up
   * any processes while there is a blocker active.
   */
  static void AddBlocker(const nsACString& aRemoteType, ContentParent* aParent);
  static void RemoveBlocker(const nsACString& aRemoteType,
                            ContentParent* aParent);

  /**
   * Take the preallocated process, if we have one, or a recycled
   * process cached via Provide().  Currently we only cache
   * DEFAULT_REMOTE_TYPE ('web') processes and only reuse them for that
   * type.  If we don't have a process to return (cached or preallocated),
   * this returns null.
   *
   * If we use a preallocated process, it will schedule the start of
   * another on Idle (AllocateOnIdle()).
   */
  static already_AddRefed<ContentParent> Take(const nsACString& aRemoteType);

  /**
   * Cache a process (currently only DEFAULT_REMOTE_TYPE) for reuse later
   * via Take().  Returns true if we cached the process, and false if
   * another process is already cached (so the caller knows to destroy it).
   * This takes a reference to the ContentParent if it is cached.
   */
  static bool Provide(ContentParent* aParent);
  static void Erase(ContentParent* aParent);

 private:
  PreallocatedProcessManager();
  DISALLOW_EVIL_CONSTRUCTORS(PreallocatedProcessManager);
};

}  // namespace mozilla

#endif  // defined mozilla_PreallocatedProcessManager_h
