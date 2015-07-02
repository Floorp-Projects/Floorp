/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DecodedStream_h_
#define DecodedStream_h_

#include "nsRefPtr.h"
#include "nsTArray.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/gfx/Point.h"

namespace mozilla {

class MediaInputPort;
class SourceMediaStream;
class ProcessedMediaStream;
class DecodedStream;
class DecodedStreamGraphListener;
class OutputStreamListener;
class ReentrantMonitor;
class MediaStreamGraph;

namespace layers {
class Image;
}

/*
 * All MediaStream-related data is protected by the decoder's monitor.
 * We have at most one DecodedStreamDaata per MediaDecoder. Its stream
 * is used as the input for each ProcessedMediaStream created by calls to
 * captureStream(UntilEnded). Seeking creates a new source stream, as does
 * replaying after the input as ended. In the latter case, the new source is
 * not connected to streams created by captureStreamUntilEnded.
 */
class DecodedStreamData {
public:
  explicit DecodedStreamData(SourceMediaStream* aStream);
  ~DecodedStreamData();
  bool IsFinished() const;
  int64_t GetPosition() const;
  void SetPlaying(bool aPlaying);

  /* The following group of fields are protected by the decoder's monitor
   * and can be read or written on any thread.
   */
  // Count of audio frames written to the stream
  int64_t mAudioFramesWritten;
  // mNextVideoTime is the end timestamp for the last packet sent to the stream.
  // Therefore video packets starting at or after this time need to be copied
  // to the output stream.
  int64_t mNextVideoTime; // microseconds
  int64_t mNextAudioTime; // microseconds
  // The last video image sent to the stream. Useful if we need to replicate
  // the image.
  nsRefPtr<layers::Image> mLastVideoImage;
  gfx::IntSize mLastVideoImageDisplaySize;
  // This is set to true when the stream is initialized (audio and
  // video tracks added).
  bool mStreamInitialized;
  bool mHaveSentFinish;
  bool mHaveSentFinishAudio;
  bool mHaveSentFinishVideo;

  // The decoder is responsible for calling Destroy() on this stream.
  const nsRefPtr<SourceMediaStream> mStream;
  nsRefPtr<DecodedStreamGraphListener> mListener;
  bool mPlaying;
  // True if we need to send a compensation video frame to ensure the
  // StreamTime going forward.
  bool mEOSVideoCompensation;
};

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
public:
  DecodedStream();
  DecodedStreamData* GetData() const;
  void DestroyData();
  void RecreateData(MediaStreamGraph* aGraph);
  nsTArray<OutputStreamData>& OutputStreams();
  ReentrantMonitor& GetReentrantMonitor() const;
  void Connect(ProcessedMediaStream* aStream, bool aFinishWhenEnded);
  void SetPlaying(bool aPlaying);

private:
  void Connect(OutputStreamData* aStream);

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
};

} // namespace mozilla

#endif // DecodedStream_h_
