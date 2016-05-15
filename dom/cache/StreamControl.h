/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_StreamControl_h
#define mozilla_dom_cache_StreamControl_h

#include "mozilla/dom/cache/ReadStream.h"
#include "mozilla/RefPtr.h"
#include "nsTObserverArray.h"

struct nsID;

namespace mozilla {
namespace ipc {
class AutoIPCStream;
} // namespace ipc
namespace dom {
namespace cache {

class CacheReadStream;

// Abstract class to help implement the stream control Child and Parent actors.
// This provides an interface to partly help with serialization of IPC types,
// but also an implementation for tracking ReadStream objects.
class StreamControl
{
public:
  // abstract interface that must be implemented by child class
  virtual void
  SerializeControl(CacheReadStream* aReadStreamOut) = 0;

  virtual void
  SerializeStream(CacheReadStream* aReadStreamOut, nsIInputStream* aStream,
                  nsTArray<UniquePtr<mozilla::ipc::AutoIPCStream>>& aStreamCleanupList) = 0;

  // inherited implementation of the ReadStream::Controllable list

  // Begin controlling the given ReadStream.  This causes a strong ref to
  // be held by the control.  The ReadStream must call NoteClosed() or
  // ForgetReadStream() to release this ref.
  void
  AddReadStream(ReadStream::Controllable* aReadStream);

  // Forget the ReadStream without notifying the actor.
  void
  ForgetReadStream(ReadStream::Controllable* aReadStream);

  // Forget the ReadStream and then notify the actor the stream is closed.
  void
  NoteClosed(ReadStream::Controllable* aReadStream, const nsID& aId);

protected:
  ~StreamControl();

  void
  CloseReadStreams(const nsID& aId);

  void
  CloseAllReadStreams();

  void
  CloseAllReadStreamsWithoutReporting();

  bool
  HasEverBeenRead() const;

  // protected parts of the abstract interface
  virtual void
  NoteClosedAfterForget(const nsID& aId) = 0;

#ifdef DEBUG
  virtual void
  AssertOwningThread() = 0;
#else
  void AssertOwningThread() { }
#endif

private:
  // Hold strong references to ReadStream object.  When the stream is closed
  // it should call NoteClosed() or ForgetReadStream() to release this ref.
  typedef nsTObserverArray<RefPtr<ReadStream::Controllable>> ReadStreamList;
  ReadStreamList mReadStreamList;
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_StreamControl_h
