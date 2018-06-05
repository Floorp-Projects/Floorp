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
  for (RefPtr<MediaInputPort>& port : mPorts) {
    port->Destroy();
  }
}

void
OutputStreamData::Init(OutputStreamManager* aOwner,
                       ProcessedMediaStream* aStream,
                       TrackID aNextAvailableTrackID)
{
  mOwner = aOwner;
  mStream = aStream;
  mNextAvailableTrackID = aNextAvailableTrackID;
}

bool
OutputStreamData::Connect(MediaStream* aStream,
                          TrackID aInputAudioTrackID,
                          TrackID aInputVideoTrackID)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPorts.IsEmpty(), "Already connected?");

  if (mStream->IsDestroyed()) {
    return false;
  }

  for (TrackID tid : {aInputAudioTrackID, aInputVideoTrackID}) {
    if (tid == TRACK_NONE) {
      continue;
    }
    MOZ_ASSERT(IsTrackIDExplicit(tid));
    mPorts.AppendElement(mStream->AllocateInputPort(
        aStream, tid, mNextAvailableTrackID++));
  }
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

  // Disconnect any existing port.
  for (RefPtr<MediaInputPort>& port : mPorts) {
    port->Destroy();
  }
  mPorts.Clear();
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

TrackID
OutputStreamData::NextAvailableTrackID() const
{
  return mNextAvailableTrackID;
}

void
OutputStreamManager::Add(ProcessedMediaStream* aStream,
                         TrackID aNextAvailableTrackID,
                         bool aFinishWhenEnded)
{
  MOZ_ASSERT(NS_IsMainThread());
  // All streams must belong to the same graph.
  MOZ_ASSERT(!Graph() || Graph() == aStream->Graph());

  // Ensure that aStream finishes the moment mDecodedStream does.
  if (aFinishWhenEnded) {
    aStream->QueueSetAutofinish(true);
  }

  OutputStreamData* p = mStreams.AppendElement();
  p->Init(this, aStream, aNextAvailableTrackID);

  // Connect to the input stream if we have one. Otherwise the output stream
  // will be connected in Connect().
  if (mInputStream) {
    p->Connect(mInputStream, mInputAudioTrackID, mInputVideoTrackID);
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
OutputStreamManager::Clear()
{
  MOZ_ASSERT(NS_IsMainThread());
  mStreams.Clear();
}

TrackID
OutputStreamManager::NextAvailableTrackIDFor(MediaStream* aOutputStream) const
{
  MOZ_ASSERT(NS_IsMainThread());
  for (const OutputStreamData& out : mStreams) {
    if (out.Equals(aOutputStream)) {
      return out.NextAvailableTrackID();
    }
  }
  return TRACK_INVALID;
}

void
OutputStreamManager::Connect(MediaStream* aStream,
                             TrackID aAudioTrackID,
                             TrackID aVideoTrackID)
{
  MOZ_ASSERT(NS_IsMainThread());
  mInputStream = aStream;
  mInputAudioTrackID = aAudioTrackID;
  mInputVideoTrackID = aVideoTrackID;
  for (int32_t i = mStreams.Length() - 1; i >= 0; --i) {
    if (!mStreams[i].Connect(aStream, mInputAudioTrackID, mInputVideoTrackID)) {
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
  mInputAudioTrackID = TRACK_INVALID;
  mInputVideoTrackID = TRACK_INVALID;
  for (int32_t i = mStreams.Length() - 1; i >= 0; --i) {
    if (!mStreams[i].Disconnect()) {
      // Probably the DOMMediaStream was GCed. Clean up.
      mStreams.RemoveElementAt(i);
    }
  }
}

} // namespace mozilla
