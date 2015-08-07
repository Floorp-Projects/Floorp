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
class ProcessedMediaStream;
class ReentrantMonitor;

template <class T> class MediaQueue;

namespace layers {
class Image;
} // namespace layers

class OutputStreamData {
public:
  ~OutputStreamData();
  void Init(DecodedStream* aDecodedStream, ProcessedMediaStream* aStream);
  nsRefPtr<ProcessedMediaStream> mStream;
  // mPort connects DecodedStreamData::mStream to our mStream.
  nsRefPtr<MediaInputPort> mPort;
  nsRefPtr<OutputStreamListener> mListener;
};

class DecodedStream {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DecodedStream);
public:
  DecodedStream(MediaQueue<MediaData>& aAudioQueue,
                MediaQueue<MediaData>& aVideoQueue);

  // Mimic MDSM::StartAudioThread.
  // Must be called before any calls to SendData().
  void StartPlayback(int64_t aStartTime, const MediaInfo& aInfo);
  // Mimic MDSM::StopAudioThread.
  void StopPlayback();

  void DestroyData();
  void RecreateData();
  void Connect(ProcessedMediaStream* aStream, bool aFinishWhenEnded);
  void Remove(MediaStream* aStream);

  void SetPlaying(bool aPlaying);
  void SetVolume(double aVolume);

  int64_t AudioEndTime() const;
  int64_t GetPosition() const;
  bool IsFinished() const;
  bool HasConsumers() const;

  // Return true if stream is finished.
  bool SendData(bool aIsSameOrigin);

protected:
  virtual ~DecodedStream();

private:
  ReentrantMonitor& GetReentrantMonitor() const;
  void RecreateData(MediaStreamGraph* aGraph);
  void Connect(OutputStreamData* aStream);
  nsTArray<OutputStreamData>& OutputStreams();
  void InitTracks();
  void AdvanceTracks();
  void SendAudio(double aVolume, bool aIsSameOrigin);
  void SendVideo(bool aIsSameOrigin);

  UniquePtr<DecodedStreamData> mData;
  // Data about MediaStreams that are being fed by the decoder.
  nsTArray<OutputStreamData> mOutputStreams;

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

  Maybe<int64_t> mStartTime;
  MediaInfo mInfo;

  MediaQueue<MediaData>& mAudioQueue;
  MediaQueue<MediaData>& mVideoQueue;
};

} // namespace mozilla

#endif // DecodedStream_h_
