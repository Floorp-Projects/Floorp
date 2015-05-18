/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DecodedStream_h_
#define DecodedStream_h_

#include "nsRefPtr.h"
#include "mozilla/gfx/Point.h"

namespace mozilla {

class MediaDecoder;
class MediaInputPort;
class SourceMediaStream;
class ProcessedMediaStream;
class DecodedStreamGraphListener;
class OutputStreamData;
class OutputStreamListener;

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
  DecodedStreamData(MediaDecoder* aDecoder, int64_t aInitialTime,
                    SourceMediaStream* aStream);
  ~DecodedStreamData();
  bool IsFinished() const;
  int64_t GetClock() const;

  /* The following group of fields are protected by the decoder's monitor
   * and can be read or written on any thread.
   */
  // Count of audio frames written to the stream
  int64_t mAudioFramesWritten;
  // Saved value of aInitialTime. Timestamp of the first audio and/or
  // video packet written.
  const int64_t mInitialTime; // microseconds
  // mNextVideoTime is the end timestamp for the last packet sent to the stream.
  // Therefore video packets starting at or after this time need to be copied
  // to the output stream.
  int64_t mNextVideoTime; // microseconds
  int64_t mNextAudioTime; // microseconds
  MediaDecoder* mDecoder;
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
  // True when we've explicitly blocked this stream because we're
  // not in PLAY_STATE_PLAYING. Used on the main thread only.
  bool mHaveBlockedForPlayState;
  // We also have an explicit blocker on the stream when
  // mDecoderStateMachine is non-null and MediaDecoderStateMachine is false.
  bool mHaveBlockedForStateMachineNotPlaying;
  // True if we need to send a compensation video frame to ensure the
  // StreamTime going forward.
  bool mEOSVideoCompensation;
};

class OutputStreamData {
public:
  // Compiler-generated default constructor needs the complete definition
  // of OutputStreamListener when constructing OutputStreamData. Provide our
  // own default constructor for forward declaration of OutputStreamListener
  // to work.
  OutputStreamData();
  ~OutputStreamData();
  void Init(MediaDecoder* aDecoder, ProcessedMediaStream* aStream);
  nsRefPtr<ProcessedMediaStream> mStream;
  // mPort connects DecodedStreamData::mStream to our mStream.
  nsRefPtr<MediaInputPort> mPort;
  nsRefPtr<OutputStreamListener> mListener;
};

} // namespace mozilla

#endif // DecodedStream_h_
