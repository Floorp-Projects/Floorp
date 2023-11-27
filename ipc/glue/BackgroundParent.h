/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_backgroundparent_h__
#define mozilla_ipc_backgroundparent_h__

#include "base/process.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/ContentParent.h"
#include "nsStringFwd.h"
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

class BackgroundStarterParent;
class PBackgroundParent;
class PBackgroundStarterParent;

template <class PFooSide>
class Endpoint;

// This class is not designed for public consumption beyond the few static
// member functions.
class BackgroundParent final {
  friend class mozilla::ipc::BackgroundStarterParent;
  friend class mozilla::dom::ContentParent;
  friend class mozilla::net::SocketProcessBridgeParent;
  friend class mozilla::net::SocketProcessParent;

  using ProcessId = base::ProcessId;
  using BlobImpl = mozilla::dom::BlobImpl;
  using ContentParent = mozilla::dom::ContentParent;
  using ThreadsafeContentParentHandle =
      mozilla::dom::ThreadsafeContentParentHandle;

 public:
  // This function allows the caller to determine if the given parent actor
  // corresponds to a child actor from another process or a child actor from a
  // different thread in the same process.
  // This function may only be called on the background thread.
  static bool IsOtherProcessActor(PBackgroundParent* aBackgroundActor);

  // This function returns a handle to the ContentParent associated with the
  // parent actor if the parent actor corresponds to a child actor from another
  // content process. If the parent actor corresponds to a child actor from a
  // different thread in the same process then this function returns null.
  //
  // This function may only be called on the background thread.
  static ThreadsafeContentParentHandle* GetContentParentHandle(
      PBackgroundParent* aBackgroundActor);

  static uint64_t GetChildID(PBackgroundParent* aBackgroundActor);

  static void KillHardAsync(PBackgroundParent* aBackgroundActor,
                            const nsACString& aReason);

 private:
  // Only called by ContentParent for cross-process actors.
  static bool AllocStarter(ContentParent* aContent,
                           Endpoint<PBackgroundStarterParent>&& aEndpoint);

  // Called by SocketProcessBridgeParent and SocketProcessParent for
  // cross-process actors.
  static bool AllocStarter(Endpoint<PBackgroundStarterParent>&& aEndpoint);
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

}  // namespace ipc
}  // namespace mozilla

#endif  // mozilla_ipc_backgroundparent_h__
