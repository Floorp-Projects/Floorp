/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamGraph.h"
#include "OutputStreamManager.h"

namespace mozilla {

OutputStreamData::~OutputStreamData()
{
  MOZ_ASSERT(NS_IsMainThread());
  // Break the connection to the input stream if necessary.
  if (mPort) {
    mPort->Destroy();
  }
}

void
OutputStreamData::Init(OutputStreamManager* aOwner, ProcessedMediaStream* aStream)
{
  mOwner = aOwner;
  mStream = aStream;
}

bool
OutputStreamData::Connect(MediaStream* aStream)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mPort, "Already connected?");

  if (mStream->IsDestroyed()) {
    return false;
  }

  mPort = mStream->AllocateInputPort(aStream);
  return true;
}

bool
OutputStreamData::Disconnect()
{
  MOZ_ASSERT(NS_IsMainThread());

  // During cycle collection, DOMMediaStream can be destroyed and send
  // its Destroy message before this decoder is destroyed. So we have to
  // be careful not to send any messages after the Destroy().
  if (mStream->IsDestroyed()) {
    return false;
  }

  // Disconnect the existing port if necessary.
  if (mPort) {
    mPort->Destroy();
    mPort = nullptr;
  }
  return true;
}

bool
OutputStreamData::Equals(MediaStream* aStream) const
{
  return mStream == aStream;
}

MediaStreamGraph*
OutputStreamData::Graph() const
{
  return mStream->Graph();
}

void
OutputStreamManager::Add(ProcessedMediaStream* aStream, bool aFinishWhenEnded)
{
  MOZ_ASSERT(NS_IsMainThread());
  // All streams must belong to the same graph.
  MOZ_ASSERT(!Graph() || Graph() == aStream->Graph());

  // Ensure that aStream finishes the moment mDecodedStream does.
  if (aFinishWhenEnded) {
    aStream->QueueSetAutofinish(true);
  }

  OutputStreamData* p = mStreams.AppendElement();
  p->Init(this, aStream);

  // Connect to the input stream if we have one. Otherwise the output stream
  // will be connected in Connect().
  if (mInputStream) {
    p->Connect(mInputStream);
  }
}

void
OutputStreamManager::Remove(MediaStream* aStream)
{
  MOZ_ASSERT(NS_IsMainThread());
  for (int32_t i = mStreams.Length() - 1; i >= 0; --i) {
    if (mStreams[i].Equals(aStream)) {
      mStreams.RemoveElementAt(i);
      break;
    }
  }
}

void
OutputStreamManager::Connect(MediaStream* aStream)
{
  MOZ_ASSERT(NS_IsMainThread());
  mInputStream = aStream;
  for (int32_t i = mStreams.Length() - 1; i >= 0; --i) {
    if (!mStreams[i].Connect(aStream)) {
      // Probably the DOMMediaStream was GCed. Clean up.
      mStreams.RemoveElementAt(i);
    }
  }
}

void
OutputStreamManager::Disconnect()
{
  MOZ_ASSERT(NS_IsMainThread());
  mInputStream = nullptr;
  for (int32_t i = mStreams.Length() - 1; i >= 0; --i) {
    if (!mStreams[i].Disconnect()) {
      // Probably the DOMMediaStream was GCed. Clean up.
      mStreams.RemoveElementAt(i);
    }
  }
}

} // namespace mozilla
