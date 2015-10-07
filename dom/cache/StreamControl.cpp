/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/StreamControl.h"

namespace mozilla {
namespace dom {
namespace cache {

void
StreamControl::AddReadStream(ReadStream::Controllable* aReadStream)
{
  AssertOwningThread();
  MOZ_ASSERT(aReadStream);
  MOZ_ASSERT(!mReadStreamList.Contains(aReadStream));
  mReadStreamList.AppendElement(aReadStream);
}

void
StreamControl::ForgetReadStream(ReadStream::Controllable* aReadStream)
{
  AssertOwningThread();
  MOZ_ALWAYS_TRUE(mReadStreamList.RemoveElement(aReadStream));
}

void
StreamControl::NoteClosed(ReadStream::Controllable* aReadStream,
                          const nsID& aId)
{
  AssertOwningThread();
  ForgetReadStream(aReadStream);
  NoteClosedAfterForget(aId);
}

StreamControl::~StreamControl()
{
  // owning thread only, but can't call virtual AssertOwningThread in destructor
  MOZ_ASSERT(mReadStreamList.IsEmpty());
}

void
StreamControl::CloseReadStreams(const nsID& aId)
{
  AssertOwningThread();
  DebugOnly<uint32_t> closedCount = 0;

  ReadStreamList::ForwardIterator iter(mReadStreamList);
  while (iter.HasMore()) {
    nsRefPtr<ReadStream::Controllable> stream = iter.GetNext();
    if (stream->MatchId(aId)) {
      stream->CloseStream();
      closedCount += 1;
    }
  }

  MOZ_ASSERT(closedCount > 0);
}

void
StreamControl::CloseAllReadStreams()
{
  AssertOwningThread();

  ReadStreamList::ForwardIterator iter(mReadStreamList);
  while (iter.HasMore()) {
    iter.GetNext()->CloseStream();
  }
}

void
StreamControl::CloseAllReadStreamsWithoutReporting()
{
  AssertOwningThread();

  ReadStreamList::ForwardIterator iter(mReadStreamList);
  while (iter.HasMore()) {
    nsRefPtr<ReadStream::Controllable> stream = iter.GetNext();
    // Note, we cannot trigger IPC traffic here.  So use
    // CloseStreamWithoutReporting().
    stream->CloseStreamWithoutReporting();
  }
}

bool
StreamControl::HasEverBeenRead() const
{
  ReadStreamList::ForwardIterator iter(mReadStreamList);
  while (iter.HasMore()) {
    if (iter.GetNext()->HasEverBeenRead()) {
      return true;
    }
  }
  return false;
}

} // namespace cache
} // namespace dom
} // namespace mozilla
