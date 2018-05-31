/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_IPCStreamDestination_h
#define mozilla_ipc_IPCStreamDestination_h

#include "mozilla/AlreadyAddRefed.h"

class nsIInputStream;
class nsIAsyncInputStream;
class nsIAsyncOutputStream;

namespace mozilla {

namespace wr {
struct ByteBuffer;
} // wr namespace

namespace ipc {

class PChildToParentStreamParent;
class PParentToChildStreamChild;

// On the destination side, you must simply call TakeReader() upon receiving a
// reference to the IPCStream{Child,Parent} actor.  You do not need to maintain
// a reference to the actor itself.
class IPCStreamDestination
{
public:
  static IPCStreamDestination*
  Cast(PChildToParentStreamParent* aActor);

  static IPCStreamDestination*
  Cast(PParentToChildStreamChild* aActor);

  void
  SetDelayedStart(bool aDelayedStart);

  void
  SetLength(int64_t aLength);

  already_AddRefed<nsIInputStream>
  TakeReader();

  bool
  IsOnOwningThread() const;

  void
  DispatchRunnable(already_AddRefed<nsIRunnable>&& aRunnable);

protected:
  IPCStreamDestination();
  virtual ~IPCStreamDestination();

  nsresult Initialize();

  // The implementation of the actor should call these methods.

  void
  ActorDestroyed();

  void
  BufferReceived(const wr::ByteBuffer& aBuffer);

  void
  CloseReceived(nsresult aRv);

#ifdef DEBUG
  bool
  HasDelayedStart() const
  {
    return mDelayedStart;
  }
#endif

  // These methods will be implemented by the actor.

  virtual void
  StartReading() = 0;

  virtual void
  RequestClose(nsresult aRv) = 0;

  virtual void
  TerminateDestination() = 0;

private:
  nsCOMPtr<nsIAsyncInputStream> mReader;
  nsCOMPtr<nsIAsyncOutputStream> mWriter;

  // This is created by TakeReader() if we need to delay the reading of data.
  // We keep a reference to the stream in order to inform it when the actor goes
  // away. If that happens, the reading of data will not be possible anymore.
  class DelayedStartInputStream;
  RefPtr<DelayedStartInputStream> mDelayedStartInputStream;

  nsCOMPtr<nsIThread> mOwningThread;
  bool mDelayedStart;

#ifdef MOZ_DEBUG
  bool mLengthSet;
#endif
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_IPCStreamDestination_h
