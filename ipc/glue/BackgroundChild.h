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

class nsIDOMBlob;
class nsIIPCBackgroundChildCreateCallback;

namespace mozilla {
namespace dom {

class BlobImpl;
class ContentChild;
class ContentParent;
class PBlobChild;

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
// Creation of PBackground is asynchronous. GetForCurrentThread() will return
// null until the sequence is complete. GetOrCreateForCurrentThread() will start
// the creation sequence and will call back via the
// nsIIPCBackgroundChildCreateCallback interface when completed. Thereafter
// (assuming success) GetForCurrentThread() will return the same actor every
// time. SynchronouslyCreateForCurrentThread() will spin the event loop until
// the BackgroundChild until the creation sequence is complete.
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
  static bool
  GetOrCreateForCurrentThread(nsIIPCBackgroundChildCreateCallback* aCallback);

  // See above.
  static PBackgroundChild*
  SynchronouslyCreateForCurrentThread();

  static mozilla::dom::PBlobChild*
  GetOrCreateActorForBlob(PBackgroundChild* aBackgroundActor,
                          nsIDOMBlob* aBlob);

  static mozilla::dom::PBlobChild*
  GetOrCreateActorForBlobImpl(PBackgroundChild* aBackgroundActor,
                              mozilla::dom::BlobImpl* aBlobImpl);

  // See above.
  static void
  CloseForCurrentThread();

private:
  // Only called by ContentChild or ContentParent.
  static void
  Startup();

  // Only called by ContentChild.
  static PBackgroundChild*
  Alloc(Transport* aTransport, ProcessId aOtherProcess);
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_backgroundchild_h__
