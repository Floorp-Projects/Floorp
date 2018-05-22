/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_backgroundchild_h__
#define mozilla_ipc_backgroundchild_h__

#include "base/process.h"
#include "mozilla/Attributes.h"
#include "mozilla/ipc/Transport.h"

namespace mozilla {
namespace dom {

class BlobImpl;
class ContentChild;
class ContentParent;

} // namespace dom

namespace ipc {

class PBackgroundChild;

// This class allows access to the PBackground protocol. PBackground allows
// communication between any thread (in the parent or a child process) and a
// single background thread in the parent process. Each PBackgroundChild
// instance is tied to the thread on which it is created and must not be shared
// across threads. Each PBackgroundChild is unique and valid as long as its
// designated thread lives.
//
// Creation of PBackground is synchronous. GetOrCreateForCurrentThread will
// create the actor if it doesn't exist yet. Thereafter (assuming success)
// GetForCurrentThread() will return the same actor every time.
//
// CloseForCurrentThread() will close the current PBackground actor.  Subsequent
// calls to GetForCurrentThread will return null.  CloseForCurrentThread() may
// only be called exactly once for each thread-specific actor.  Currently it is
// illegal to call this before the PBackground actor has been created.
//
// The PBackgroundChild actor and all its sub-protocol actors will be
// automatically destroyed when its designated thread completes.
class BackgroundChild final
{
  friend class mozilla::dom::ContentChild;
  friend class mozilla::dom::ContentParent;

  typedef base::ProcessId ProcessId;
  typedef mozilla::ipc::Transport Transport;

public:
  // See above.
  static PBackgroundChild*
  GetForCurrentThread();

  // See above.
  static PBackgroundChild*
  GetOrCreateForCurrentThread();

  // See above.
  static void
  CloseForCurrentThread();

private:
  // Only called by ContentChild or ContentParent.
  static void
  Startup();
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_backgroundchild_h__
