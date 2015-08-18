/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DecodedStream_h_
#define DecodedStream_h_

#include "nsTArray.h"
#include "MediaInfo.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"
#include "mozilla/nsRefPtr.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/gfx/Point.h"

namespace mozilla {

class DecodedStream;
class DecodedStreamData;
class MediaData;
class MediaInputPort;
class MediaStream;
class MediaStreamGraph;
class OutputStreamListener;
class OutputStreamManager;
class ProcessedMediaStream;
class ReentrantMonitor;

template <class T> class MediaQueue;

namespace layers {
class Image;
} // namespace layers

class OutputStreamData {
public:
  ~OutputStreamData();
  void Init(OutputStreamManager* aOwner, ProcessedMediaStream* aStream);

  // Connect mStream to the input stream.
  void Connect(MediaStream* aStream);
  // Disconnect mStream from its input stream.
  // Return false is mStream is already destroyed, otherwise true.
  bool Disconnect();
  // Called by OutputStreamListener to remove self from the output streams
  // managed by OutputStreamManager.
  void Remove();
  // Return true if aStream points to the same object as mStream.
  // Used by OutputStreamManager to remove an output stream.
  bool Equals(MediaStream* aStream)
  {
    return mStream == aStream;
  }

private:
  OutputStreamManager* mOwner;
  nsRefPtr<ProcessedMediaStream> mStream;
  // mPort connects our mStream to an input stream.
  nsRefPtr<MediaInputPort> mPort;
  nsRefPtr<OutputStreamListener> mListener;
};

class OutputStreamManager {
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

private:
  // Keep the input stream so we can connect the output streams that
  // are added after Connect().
  nsRefPtr<MediaStream> mInputStream;
  nsTArray<OutputStreamData> mStreams;
};

class DecodedStream {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DecodedStream);
public:
  DecodedStream(MediaQueue<MediaData>& aAudioQueue,
                MediaQueue<MediaData>& aVideoQueue);

  // Mimic MDSM::StartAudioThread.
  // Must be called before any calls to SendData().
  //
  // Return a promise which will be resolved when the stream is finished
  // or rejected if any error.
  nsRefPtr<GenericPromise> StartPlayback(int64_t aStartTime,
                                         const MediaInfo& aInfo);
  // Mimic MDSM::StopAudioThread.
  void StopPlayback();

  void DestroyData();
  void RecreateData();
  void Connect(ProcessedMediaStream* aStream, bool aFinishWhenEnded);
  void Remove(MediaStream* aStream);

  void SetPlaying(bool aPlaying);
  void SetVolume(double aVolume);
  void SetSameOrigin(bool aSameOrigin);

  int64_t AudioEndTime() const;
  int64_t GetPosition() const;
  bool IsFinished() const;
  bool HasConsumers() const;

  void SendData();

protected:
  virtual ~DecodedStream();

private:
  ReentrantMonitor& GetReentrantMonitor() const;
  void RecreateData(MediaStreamGraph* aGraph);
  void InitTracks();
  void AdvanceTracks();
  void SendAudio(double aVolume, bool aIsSameOrigin);
  void SendVideo(bool aIsSameOrigin);

  UniquePtr<DecodedStreamData> mData;
  // Data about MediaStreams that are being fed by the decoder.
  OutputStreamManager mOutputStreamManager;

  // TODO: This is a temp solution to get rid of decoder monitor on the main
  // thread in MDSM::AddOutputStream and MDSM::RecreateDecodedStream as
  // required by bug 1146482. DecodedStream needs to release monitor before
  // calling back into MDSM functions in order to prevent deadlocks.
  //
  // Please move all capture-stream related code from MDSM into DecodedStream
  // and apply "dispatch + mirroring" to get rid of this monitor in the future.
  mutable ReentrantMonitor mMonitor;

  bool mPlaying;
  double mVolume;
  bool mSameOrigin;

  Maybe<int64_t> mStartTime;
  MediaInfo mInfo;

  MediaQueue<MediaData>& mAudioQueue;
  MediaQueue<MediaData>& mVideoQueue;
};

} // namespace mozilla

#endif // DecodedStream_h_
