/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_IPCStreamSource_h
#define mozilla_ipc_IPCStreamSource_h

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/dom/WorkerRef.h"

class nsIAsyncInputStream;

namespace mozilla {

namespace dom {
class nsIContentChild;
class nsIContentParent;
} // dom namespace

namespace wr {
struct ByteBuffer;
} // wr namespace

namespace ipc {

class PBackgroundChild;
class PBackgroundParent;
class PChildToParentStreamChild;
class PParentToChildStreamParent;

// The IPCStream IPC actor is designed to push an nsIInputStream from child to
// parent or parent to child incrementally.  This is mainly needed for streams
// such as nsPipe that may not yet have all their data available when the
// stream must be sent across an IPC boundary.  While many streams are handled
// by SerializeInputStream(), these streams cannot be serialized and must be
// sent using this actor.
//
// The IPCStream actor only support async, non-blocking streams because they
// must be read inline on the main thread and Worker threads.
//
// In general, the creation and handling of the IPCStream actor cannot be
// abstracted away behind SerializeInputStream() because the actor must be
// carefully managed.  Specifically:
//
//  1) The data flow must be explicitly initiated by calling
//     IPCStreamSource{Child,Parent}::Start() after the actor has been sent to
//     the other-side actor.
//  2) If the actor is never sent to the other-side, then this code must
//     call IPCStreamSource{Child,Parent}::StartDestroy() to avoid memory leaks.
//  3) The IPCStreamSource actor can only be used on threads that can be
//     guaranteed to stay alive as long as the actor is alive.  Right now
//     this limits IPCStream to the main thread and Worker threads.
//
// In general you should probably use the AutoIPCStreamSource RAII class
// defined in InputStreamUtils.h instead of using IPCStreamSource directly.
class IPCStreamSource
{
public:
  // Create a IPCStreamSource using a PContent IPC manager on the
  // main thread.  This can return nullptr if the provided stream is
  // blocking.
  static PChildToParentStreamChild*
  Create(nsIAsyncInputStream* aInputStream, dom::nsIContentChild* aManager);

  // Create a IPCStreamSource using a PBackground IPC manager on the
  // main thread or a Worker thread.  This can return nullptr if the provided
  // stream is blocking or if the Worker thread is already shutting down.
  static PChildToParentStreamChild*
  Create(nsIAsyncInputStream* aInputStream, PBackgroundChild* aManager);

  // Create a IPCStreamSource using a PContent IPC manager on the
  // main thread.  This can return nullptr if the provided stream is
  // blocking.
  static PParentToChildStreamParent*
  Create(nsIAsyncInputStream* aInputStream, dom::nsIContentParent* aManager);

  // Create a IPCStreamSource using a PBackground IPC manager on the
  // main thread or a Worker thread.  This can return nullptr if the provided
  // stream is blocking or if the Worker thread is already shutting down.
  static PParentToChildStreamParent*
  Create(nsIAsyncInputStream* aInputStream, PBackgroundParent* aManager);

  static IPCStreamSource*
  Cast(PChildToParentStreamChild* aActor);

  static IPCStreamSource*
  Cast(PParentToChildStreamParent* aActor);

  // Start reading data from the nsIAsyncInputStream used to create the actor.
  // This must be called after the actor is passed to the parent.  If you
  // use AutoIPCStream this is handled automatically.
  void
  Start();

  // Start cleaning up the actor.  This must be called if the actor is never
  // sent to the other side.  If you use AutoIPCStream this is handled
  // automatically.
  void
  StartDestroy();

protected:
  IPCStreamSource(nsIAsyncInputStream* aInputStream);
  virtual ~IPCStreamSource();

  bool Initialize();

  void ActorDestroyed();

  void OnEnd(nsresult aRv);

  virtual void
  Close(nsresult aRv) = 0;

  virtual void
  SendData(const wr::ByteBuffer& aBuffer) = 0;

  void
  ActorConstructed();

private:
  class Callback;

  void DoRead();

  void Wait();

  void OnStreamReady(Callback* aCallback);

  nsCOMPtr<nsIAsyncInputStream> mStream;
  RefPtr<Callback> mCallback;

  RefPtr<dom::StrongWorkerRef> mWorkerRef;

#ifdef DEBUG
protected:
#endif

  enum {
   ePending,
   eActorConstructed,
   eClosed
  } mState;

private:
  NS_DECL_OWNINGTHREAD
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_IPCStreamSource_h
