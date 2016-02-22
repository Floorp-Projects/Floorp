/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OutputStreamManager_h
#define OutputStreamManager_h

#include "mozilla/RefPtr.h"
#include "nsTArray.h"

namespace mozilla {

class MediaInputPort;
class MediaStream;
class MediaStreamGraph;
class OutputStreamManager;
class ProcessedMediaStream;

class OutputStreamData {
public:
  ~OutputStreamData();
  void Init(OutputStreamManager* aOwner, ProcessedMediaStream* aStream);

  // Connect mStream to the input stream.
  // Return false is mStream is already destroyed, otherwise true.
  bool Connect(MediaStream* aStream);
  // Disconnect mStream from its input stream.
  // Return false is mStream is already destroyed, otherwise true.
  bool Disconnect();
  // Return true if aStream points to the same object as mStream.
  // Used by OutputStreamManager to remove an output stream.
  bool Equals(MediaStream* aStream) const;
  // Return the graph mStream belongs to.
  MediaStreamGraph* Graph() const;

private:
  OutputStreamManager* mOwner;
  RefPtr<ProcessedMediaStream> mStream;
  // mPort connects our mStream to an input stream.
  RefPtr<MediaInputPort> mPort;
};

class OutputStreamManager {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(OutputStreamManager);

public:
  // Add the output stream to the collection.
  void Add(ProcessedMediaStream* aStream, bool aFinishWhenEnded);
  // Remove the output stream from the collection.
  void Remove(MediaStream* aStream);
  // Return true if the collection empty.
  bool IsEmpty() const
  {
    MOZ_ASSERT(NS_IsMainThread());
    return mStreams.IsEmpty();
  }
  // Connect all output streams in the collection to the input stream.
  void Connect(MediaStream* aStream);
  // Disconnect all output streams from the input stream.
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
  nsTArray<OutputStreamData> mStreams;
};

} // namespace mozilla

#endif // OutputStreamManager_h
