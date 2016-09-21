/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_SendStream_h
#define mozilla_ipc_SendStream_h

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/ipc/PSendStreamChild.h"
#include "mozilla/ipc/PSendStreamParent.h"

class nsIInputStream;
class nsIAsyncInputStream;

namespace mozilla {

namespace dom {
class nsIContentChild;
} // dom namespace

namespace ipc {

class PBackgroundChild;

// The SendStream IPC actor is designed to push an nsIInputStream from child to
// parent incrementally.  This is mainly needed for streams such as nsPipe that
// may not yet have all their data available when the stream must be sent across
// an IPC boundary.  While many streams are handled by SerializeInputStream(),
// these streams cannot be serialized and must be sent using this actor.
//
// The SendStream actor only supports sending data from child to parent.
//
// The SendStream actor only support async, non-blocking streams because they
// must be read inline on the main thread and Worker threads.
//
// In general, the creation and handling of the SendStream actor cannot be
// abstracted away behind SerializeInputStream() because the actor must be
// carefully managed.  Specifically:
//
//  1) The data flow must be explicitly initiated by calling
//     SendStreamChild::Start() after the actor has been sent to the parent.
//  2) If the actor is never sent to the parent, then the child code must
//     call SendStreamChild::StartDestroy() to avoid memory leaks.
//  3) The SendStreamChild actor can only be used on threads that can be
//     guaranteed to stay alive as long as the actor is alive.  Right now
//     this limits SendStream to the main thread and Worker threads.
//
// In general you should probably use the AutoIPCStreamChild RAII class
// defined in InputStreamUtils.h instead of using SendStreamChild directly.
class SendStreamChild : public PSendStreamChild
{
public:
  // Create a SendStreamChild using a PContent IPC manager on the
  // main thread.  This can return nullptr if the provided stream is
  // blocking.
  static SendStreamChild*
  Create(nsIAsyncInputStream* aInputStream, dom::nsIContentChild* aManager);

  // Create a SendStreamChild using a PBackground IPC manager on the
  // main thread or a Worker thread.  This can return nullptr if the provided
  // stream is blocking or if the Worker thread is already shutting down.
  static SendStreamChild*
  Create(nsIAsyncInputStream* aInputStream, PBackgroundChild* aManager);

  // Start reading data from the nsIAsyncInputStream used to create the actor.
  // This must be called after the actor is passed to the parent.  If you
  // use AutoIPCStreamChild this is handled automatically.
  virtual void
  Start() = 0;

  // Start cleaning up the actor.  This must be called if the actor is never
  // sent to the parent.  If you use AutoIPCStreamChild this is handled
  // automatically.
  virtual void
  StartDestroy() = 0;

protected:
  virtual
  ~SendStreamChild() = 0;
};

// On the parent side, you must simply call TakeReader() upon receiving a
// reference to the SendStreamParent actor.  You do not need to maintain a
// reference to the actor itself.
class SendStreamParent : public PSendStreamParent
{
public:
  virtual already_AddRefed<nsIInputStream>
  TakeReader() = 0;

protected:
  virtual
  ~SendStreamParent() = 0;
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_SendStream_h
