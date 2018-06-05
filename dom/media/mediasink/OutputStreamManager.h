/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OutputStreamManager_h
#define OutputStreamManager_h

#include "mozilla/RefPtr.h"
#include "nsTArray.h"
#include "TrackID.h"

namespace mozilla {

class MediaInputPort;
class MediaStream;
class MediaStreamGraph;
class OutputStreamManager;
class ProcessedMediaStream;

class OutputStreamData {
public:
  ~OutputStreamData();
  void Init(OutputStreamManager* aOwner,
            ProcessedMediaStream* aStream,
            TrackID aNextAvailableTrackID);

  // Connect the given input stream's audio and video tracks to mStream.
  // Return false is mStream is already destroyed, otherwise true.
  bool Connect(MediaStream* aStream, TrackID aAudioTrackID, TrackID aVideoTrackID);
  // Disconnect mStream from its input stream.
  // Return false is mStream is already destroyed, otherwise true.
  bool Disconnect();
  // Return true if aStream points to the same object as mStream.
  // Used by OutputStreamManager to remove an output stream.
  bool Equals(MediaStream* aStream) const;
  // Return the graph mStream belongs to.
  MediaStreamGraph* Graph() const;
  // The next TrackID that will not cause a collision in mStream.
  TrackID NextAvailableTrackID() const;

private:
  OutputStreamManager* mOwner;
  RefPtr<ProcessedMediaStream> mStream;
  // mPort connects an input stream to our mStream.
  nsTArray<RefPtr<MediaInputPort>> mPorts;
  // For guaranteeing TrackID uniqueness in our mStream.
  TrackID mNextAvailableTrackID = TRACK_INVALID;
};

class OutputStreamManager {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(OutputStreamManager);

public:
  // Add the output stream to the collection.
  void Add(ProcessedMediaStream* aStream,
           TrackID aNextAvailableTrackID,
           bool aFinishWhenEnded);
  // Remove the output stream from the collection.
  void Remove(MediaStream* aStream);
  // Clear all output streams from the collection.
  void Clear();
  // The next TrackID that will not cause a collision in aOutputStream.
  TrackID NextAvailableTrackIDFor(MediaStream* aOutputStream) const;
  // Return true if the collection empty.
  bool IsEmpty() const
  {
    MOZ_ASSERT(NS_IsMainThread());
    return mStreams.IsEmpty();
  }
  // Connect the given input stream's tracks to all output streams.
  void Connect(MediaStream* aStream,
               TrackID aAudioTrackID,
               TrackID aVideoTrackID);
  // Disconnect the input stream to all output streams.
  void Disconnect();
  // Return the graph these streams belong to or null if empty.
  MediaStreamGraph* Graph() const
  {
    MOZ_ASSERT(NS_IsMainThread());
    return !IsEmpty() ? mStreams[0].Graph() : nullptr;
  }

private:
  ~OutputStreamManager() {}
  // Keep the input stream so we can connect the output streams that
  // are added after Connect().
  RefPtr<MediaStream> mInputStream;
  TrackID mInputAudioTrackID = TRACK_INVALID;
  TrackID mInputVideoTrackID = TRACK_INVALID;
  nsTArray<OutputStreamData> mStreams;
};

} // namespace mozilla

#endif // OutputStreamManager_h
