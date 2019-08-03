/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaEncoder_h_
#define MediaEncoder_h_

#include "ContainerWriter.h"
#include "CubebUtils.h"
#include "MediaStreamGraph.h"
#include "MediaStreamListener.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/UniquePtr.h"
#include "nsIMemoryReporter.h"
#include "TrackEncoder.h"

namespace mozilla {

class DriftCompensator;
class Runnable;
class TaskQueue;

namespace dom {
class AudioNode;
class AudioStreamTrack;
class MediaStreamTrack;
class VideoStreamTrack;
}  // namespace dom

class DriftCompensator;
class MediaEncoder;

class MediaEncoderListener {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaEncoderListener)
  virtual void Initialized() = 0;
  virtual void DataAvailable() = 0;
  virtual void Error() = 0;
  virtual void Shutdown() = 0;

 protected:
  virtual ~MediaEncoderListener() {}
};

/**
 * MediaEncoder is the framework of encoding module, it controls and manages
 * procedures between ContainerWriter and TrackEncoder. ContainerWriter packs
 * the encoded track data with a specific container (e.g. ogg, webm).
 * AudioTrackEncoder and VideoTrackEncoder are subclasses of TrackEncoder, and
 * are responsible for encoding raw data coming from MediaStreamGraph.
 *
 * MediaEncoder solves threading issues by doing message passing to a TaskQueue
 * (the "encoder thread") as passed in to the constructor. Each
 * MediaStreamTrack to be recorded is set up with a MediaStreamTrackListener.
 * Typically there are a non-direct track listeners for audio, direct listeners
 * for video, and there is always a non-direct listener on each track for
 * time-keeping. The listeners forward data to their corresponding TrackEncoders
 * on the encoder thread.
 *
 * The MediaEncoder listens to events from all TrackEncoders, and in turn
 * signals events to interested parties. Typically a MediaRecorder::Session.
 * The event that there's data available in the TrackEncoders is what typically
 * drives the extraction and muxing of data.
 *
 * MediaEncoder is designed to be a passive component, neither does it own or is
 * in charge of managing threads. Instead this is done by its owner.
 *
 * For example, usage from MediaRecorder of this component would be:
 * 1) Create an encoder with a valid MIME type.
 *    => encoder = MediaEncoder::CreateEncoder(aMIMEType);
 *    It then creates a ContainerWriter according to the MIME type
 *
 * 2) Connect a MediaEncoderListener to be notified when the MediaEncoder has
 *    been initialized and when there's data available.
 *    => encoder->RegisterListener(listener);
 *
 * 3) Connect the MediaStreamTracks to be recorded.
 *    => encoder->ConnectMediaStreamTrack(track);
 *    This creates the corresponding TrackEncoder and connects the track and
 *    the TrackEncoder through a track listener. This also starts encoding.
 *
 * 4) When the MediaEncoderListener is notified that the MediaEncoder is
 *    initialized, we can encode metadata.
 *    => encoder->GetEncodedMetadata(...);
 *
 * 5) When the MediaEncoderListener is notified that the MediaEncoder has
 *    data available, we can encode data.
 *    => encoder->GetEncodedData(...);
 *
 * 6) To stop encoding, there are multiple options:
 *
 *    6.1) Stop() for a graceful stop.
 *         => encoder->Stop();
 *
 *    6.2) Cancel() for an immediate stop, if you don't need the data currently
 *         buffered.
 *         => encoder->Cancel();
 *
 *    6.3) When all input tracks end, the MediaEncoder will automatically stop
 *         and shut down.
 */
class MediaEncoder {
 private:
  class AudioTrackListener;
  class VideoTrackListener;
  class EncoderListener;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaEncoder)

  MediaEncoder(TaskQueue* aEncoderThread,
               RefPtr<DriftCompensator> aDriftCompensator,
               UniquePtr<ContainerWriter> aWriter,
               AudioTrackEncoder* aAudioEncoder,
               VideoTrackEncoder* aVideoEncoder, TrackRate aTrackRate,
               const nsAString& aMIMEType);

  /**
   * Called on main thread from MediaRecorder::Pause.
   */
  void Suspend();

  /**
   * Called on main thread from MediaRecorder::Resume.
   */
  void Resume();

  /**
   * Stops the current encoding, and disconnects the input tracks.
   */
  void Stop();

  /**
   * Connects an AudioNode with the appropriate encoder.
   */
  void ConnectAudioNode(dom::AudioNode* aNode, uint32_t aOutput);

  /**
   * Connects a MediaStreamTrack with the appropriate encoder.
   */
  void ConnectMediaStreamTrack(dom::MediaStreamTrack* aTrack);

  /**
   * Removes a connected MediaStreamTrack.
   */
  void RemoveMediaStreamTrack(dom::MediaStreamTrack* aTrack);

  /**
   * Creates an encoder with a given MIME type. Returns null if we are unable
   * to create the encoder. For now, default aMIMEType to "audio/ogg" and use
   * Ogg+Opus if it is empty.
   */
  static already_AddRefed<MediaEncoder> CreateEncoder(
      TaskQueue* aEncoderThread, const nsAString& aMIMEType,
      uint32_t aAudioBitrate, uint32_t aVideoBitrate, uint8_t aTrackTypes,
      TrackRate aTrackRate);

  /**
   * Encodes raw metadata for all tracks to aOutputBufs. aMIMEType is the valid
   * mime-type for the returned container data. The buffer of container data is
   * allocated in ContainerWriter::GetContainerData().
   *
   * Should there be insufficient input data for either track encoder to infer
   * the metadata, or if metadata has already been encoded, we return an error
   * and the output arguments are undefined. Otherwise we return NS_OK.
   */
  nsresult GetEncodedMetadata(nsTArray<nsTArray<uint8_t>>* aOutputBufs,
                              nsAString& aMIMEType);
  /**
   * Encodes raw data for all tracks to aOutputBufs. The buffer of container
   * data is allocated in ContainerWriter::GetContainerData().
   *
   * This implies that metadata has already been encoded and that all track
   * encoders are still active. Should either implication break, we return an
   * error and the output argument is undefined. Otherwise we return NS_OK.
   */
  nsresult GetEncodedData(nsTArray<nsTArray<uint8_t>>* aOutputBufs);

  /**
   * Return true if MediaEncoder has been shutdown. Reasons are encoding
   * complete, encounter an error, or being canceled by its caller.
   */
  bool IsShutdown();

  /**
   * Cancels the encoding and shuts down the encoder using Shutdown().
   * Listeners are not notified of the shutdown.
   */
  void Cancel();

  bool HasError();

#ifdef MOZ_WEBM_ENCODER
  static bool IsWebMEncoderEnabled();
#endif

  /**
   * Notifies listeners that this MediaEncoder has been initialized.
   */
  void NotifyInitialized();

  /**
   * Notifies listeners that this MediaEncoder has data available in some
   * TrackEncoders.
   */
  void NotifyDataAvailable();

  /**
   * Registers a listener to events from this MediaEncoder.
   * We hold a strong reference to the listener.
   */
  void RegisterListener(MediaEncoderListener* aListener);

  /**
   * Unregisters a listener from events from this MediaEncoder.
   * The listener will stop receiving events synchronously.
   */
  bool UnregisterListener(MediaEncoderListener* aListener);

  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)
  /*
   * Measure the size of the buffer, and heap memory in bytes occupied by
   * mAudioEncoder and mVideoEncoder.
   */
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf);

  /**
   * Set desired video keyframe interval defined in milliseconds.
   */
  void SetVideoKeyFrameInterval(int32_t aVideoKeyFrameInterval);

 protected:
  ~MediaEncoder();

 private:
  /**
   * Takes a regular runnable and dispatches it to the graph wrapped in a
   * ControlMessage.
   */
  void RunOnGraph(already_AddRefed<Runnable> aRunnable);

  /**
   * Shuts down the MediaEncoder and cleans up track encoders.
   * Listeners will be notified of the shutdown unless we were Cancel()ed first.
   */
  void Shutdown();

  /**
   * Sets mError to true, notifies listeners of the error if mError changed,
   * and stops encoding.
   */
  void SetError();

  // Get encoded data from trackEncoder and write to muxer
  nsresult WriteEncodedDataToMuxer(TrackEncoder* aTrackEncoder);
  // Get metadata from trackEncoder and copy to muxer
  nsresult CopyMetadataToMuxer(TrackEncoder* aTrackEncoder);

  const RefPtr<TaskQueue> mEncoderThread;
  const RefPtr<DriftCompensator> mDriftCompensator;

  UniquePtr<ContainerWriter> mWriter;
  RefPtr<AudioTrackEncoder> mAudioEncoder;
  RefPtr<AudioTrackListener> mAudioListener;
  RefPtr<VideoTrackEncoder> mVideoEncoder;
  RefPtr<VideoTrackListener> mVideoListener;
  RefPtr<EncoderListener> mEncoderListener;
  nsTArray<RefPtr<MediaEncoderListener>> mListeners;

  // The AudioNode we are encoding.
  // Will be null when input is media stream or destination node.
  RefPtr<dom::AudioNode> mAudioNode;
  // Pipe-stream for allowing a track listener on a non-destination AudioNode.
  // Will be null when input is media stream or destination node.
  RefPtr<AudioNodeStream> mPipeStream;
  // Input port that connect mAudioNode to mPipeStream.
  // Will be null when input is media stream or destination node.
  RefPtr<MediaInputPort> mInputPort;
  // An audio track that we are encoding. Will be null if the input stream
  // doesn't contain audio on start() or if the input is an AudioNode.
  RefPtr<dom::AudioStreamTrack> mAudioTrack;
  // A video track that we are encoding. Will be null if the input stream
  // doesn't contain video on start() or if the input is an AudioNode.
  RefPtr<dom::VideoStreamTrack> mVideoTrack;
  TimeStamp mStartTime;
  nsString mMIMEType;
  bool mInitialized;
  bool mMetadataEncoded;
  bool mCompleted;
  bool mError;
  bool mCanceled;
  bool mShutdown;
  // Get duration from create encoder, for logging purpose
  double GetEncodeTimeStamp() {
    TimeDuration decodeTime;
    decodeTime = TimeStamp::Now() - mStartTime;
    return decodeTime.ToMilliseconds();
  }
};

}  // namespace mozilla

#endif
