/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_backgroundparent_h__
#define mozilla_ipc_backgroundparent_h__

#include "base/process.h"
#include "mozilla/Attributes.h"
#include "mozilla/ipc/Transport.h"
#include "nsTArrayForwardDeclare.h"

#ifdef DEBUG
#  include "nsXULAppAPI.h"
#endif

template <class>
struct already_AddRefed;

namespace mozilla {

namespace net {

class SocketProcessBridgeParent;
class SocketProcessParent;

}  // namespace net

namespace dom {

class BlobImpl;
class ContentParent;

}  // namespace dom

namespace ipc {

class PBackgroundParent;

template <class PFooSide>
class Endpoint;

// This class is not designed for public consumption beyond the few static
// member functions.
class BackgroundParent final {
  friend class mozilla::dom::ContentParent;

  typedef base::ProcessId ProcessId;
  typedef mozilla::dom::BlobImpl BlobImpl;
  typedef mozilla::dom::ContentParent ContentParent;
  typedef mozilla::ipc::Transport Transport;
  friend class mozilla::net::SocketProcessBridgeParent;
  friend class mozilla::net::SocketProcessParent;

 public:
  // This function allows the caller to determine if the given parent actor
  // corresponds to a child actor from another process or a child actor from a
  // different thread in the same process.
  // This function may only be called on the background thread.
  static bool IsOtherProcessActor(PBackgroundParent* aBackgroundActor);

  // This function returns the ContentParent associated with the parent actor if
  // the parent actor corresponds to a child actor from another process. If the
  // parent actor corresponds to a child actor from a different thread in the
  // same process then this function returns null.
  // This function may only be called on the background thread. However,
  // ContentParent is not threadsafe and the returned pointer may not be used on
  // any thread other than the main thread. Callers must take care to use (and
  // release) the returned pointer appropriately.
  static already_AddRefed<ContentParent> GetContentParent(
      PBackgroundParent* aBackgroundActor);

  // Get a value that represents the ContentParent associated with the parent
  // actor for comparison. The value is not guaranteed to uniquely identify the
  // ContentParent after the ContentParent has died. This function may only be
  // called on the background thread.
  static intptr_t GetRawContentParentForComparison(
      PBackgroundParent* aBackgroundActor);

  static uint64_t GetChildID(PBackgroundParent* aBackgroundActor);

  static bool GetLiveActorArray(PBackgroundParent* aBackgroundActor,
                                nsTArray<PBackgroundParent*>& aLiveActorArray);

 private:
  // Only called by ContentParent for cross-process actors.
  static bool Alloc(ContentParent* aContent,
                    Endpoint<PBackgroundParent>&& aEndpoint);

  // Called by SocketProcessBridgeParent and SocketProcessParent for
  // cross-process actors.
  static bool Alloc(Endpoint<PBackgroundParent>&& aEndpoint);
};

// Implemented in BackgroundImpl.cpp.
bool IsOnBackgroundThread();

#ifdef DEBUG

// Implemented in BackgroundImpl.cpp.
void AssertIsOnBackgroundThread();

#else

inline void AssertIsOnBackgroundThread() {}

#endif  // DEBUG

inline void AssertIsInMainProcess() { MOZ_ASSERT(XRE_IsParentProcess()); }

inline void AssertIsInMainOrSocketProcess() {
  MOZ_ASSERT(XRE_IsParentProcess() || XRE_IsSocketProcess());
}

}  // namespace ipc
}  // namespace mozilla

#endif  // mozilla_ipc_backgroundparent_h__
