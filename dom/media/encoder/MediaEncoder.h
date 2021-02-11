/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaEncoder_h_
#define MediaEncoder_h_

#include "ContainerWriter.h"
#include "CubebUtils.h"
#include "MediaQueue.h"
#include "MediaTrackGraph.h"
#include "MediaTrackListener.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/MozPromise.h"
#include "mozilla/UniquePtr.h"
#include "nsIMemoryReporter.h"
#include "TrackEncoder.h"

namespace mozilla {

class DriftCompensator;
class Muxer;
class Runnable;
class TaskQueue;

namespace dom {
class AudioNode;
class AudioStreamTrack;
class BlobImpl;
class MediaStreamTrack;
class MutableBlobStorage;
class VideoStreamTrack;
}  // namespace dom

class DriftCompensator;
class MediaEncoder;

class MediaEncoderListener {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaEncoderListener)
  /**
   * All tracks have received data.
   */
  virtual void Started() = 0;
  /**
   * There was a fatal error in an encoder.
   */
  virtual void Error() = 0;
  /**
   * The MediaEncoder has been shut down.
   */
  virtual void Shutdown() = 0;

 protected:
  virtual ~MediaEncoderListener() = default;
};

/**
 * MediaEncoder is the framework of encoding module, it controls and manages
 * procedures between ContainerWriter and TrackEncoder. ContainerWriter packs
 * the encoded track data with a specific container (e.g. ogg, webm).
 * AudioTrackEncoder and VideoTrackEncoder are subclasses of TrackEncoder, and
 * are responsible for encoding raw data coming from MediaTrackGraph.
 *
 * MediaEncoder solves threading issues by doing message passing to a TaskQueue
 * (the "encoder thread") as passed in to the constructor. Each
 * MediaStreamTrack to be recorded is set up with a MediaTrackListener.
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
 *    been started and when there's data available.
 *    => encoder->RegisterListener(listener);
 *
 * 3) Connect the sources to be recorded. Either through:
 *    => encoder->ConnectAudioNode(node);
 *    or
 *    => encoder->ConnectMediaStreamTrack(track);
 *    These should not be mixed. When connecting MediaStreamTracks there is
 *    support for at most one of each kind.
 *
 * 4) When the MediaEncoderListener is notified that the MediaEncoder has
 *    data available, we can encode data. This also encodes metadata on its
 *    first invocation.
 *    => encoder->GetEncodedData(...);
 *
 * 5) To stop encoding, there are multiple options:
 *
 *    5.1) Stop() for a graceful stop.
 *         => encoder->Stop();
 *
 *    5.2) Cancel() for an immediate stop, if you don't need the data currently
 *         buffered.
 *         => encoder->Cancel();
 *
 *    5.3) When all input tracks end, the MediaEncoder will automatically stop
 *         and shut down.
 */
class MediaEncoder {
 private:
  class AudioTrackListener;
  class VideoTrackListener;
  class EncoderListener;

 public:
  using BlobPromise =
      MozPromise<RefPtr<dom::BlobImpl>, nsresult, false /* IsExclusive */>;
  using SizeOfPromise = MozPromise<size_t, size_t, true /* IsExclusive */>;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaEncoder)

  MediaEncoder(RefPtr<TaskQueue> aEncoderThread,
               RefPtr<DriftCompensator> aDriftCompensator,
               UniquePtr<ContainerWriter> aWriter,
               UniquePtr<AudioTrackEncoder> aAudioEncoder,
               UniquePtr<VideoTrackEncoder> aVideoEncoder,
               UniquePtr<MediaQueue<EncodedFrame>> aEncodedAudioQueue,
               UniquePtr<MediaQueue<EncodedFrame>> aEncodedVideoQueue,
               TrackRate aTrackRate, const nsAString& aMIMEType,
               uint64_t aMaxMemory, TimeDuration aTimeslice);

  /**
   * Called on main thread from MediaRecorder::Pause.
   */
  void Suspend();

  /**
   * Called on main thread from MediaRecorder::Resume.
   */
  void Resume();

  /**
   * Disconnects the input tracks, causing the encoding to stop.
   */
  void DisconnectTracks();

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
      RefPtr<TaskQueue> aEncoderThread, const nsAString& aMimeType,
      uint32_t aAudioBitrate, uint32_t aVideoBitrate, uint8_t aTrackTypes,
      TrackRate aTrackRate, uint64_t aMaxMemory, TimeDuration aTimeslice);

  /**
   * Encodes raw data for all tracks to aOutputBufs. The buffer of container
   * data is allocated in ContainerWriter::GetContainerData().
   *
   * On its first call, metadata is also encoded. TrackEncoders must have been
   * initialized before this is called.
   */
  nsresult GetEncodedData(nsTArray<nsTArray<uint8_t>>* aOutputBufs);

  /**
   * Asserts that Shutdown() has been called. Reasons are encoding
   * complete, encounter an error, or being canceled by its caller.
   */
  void AssertShutdownCalled() { MOZ_ASSERT(mShutdownPromise); }

  /**
   * Stops (encoding any data currently buffered) the encoding and shuts down
   * the encoder using Shutdown().
   */
  RefPtr<GenericNonExclusivePromise> Stop();

  /**
   * Cancels (discarding any data currently buffered) the encoding and shuts
   * down the encoder using Shutdown().
   */
  RefPtr<GenericNonExclusivePromise> Cancel();

  bool HasError();

  static bool IsWebMEncoderEnabled();

  const nsString& MimeType() const;

  /**
   * Updates internal state when track encoders are all initialized.
   */
  void UpdateInitialized();

  /**
   * Updates internal state when track encoders are all initialized, and
   * notifies listeners that this MediaEncoder has been started.
   */
  void UpdateStarted();

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
  RefPtr<SizeOfPromise> SizeOfExcludingThis(
      mozilla::MallocSizeOf aMallocSizeOf);

  /**
   * Encode, mux and store into blob storage what has been buffered until now,
   * then return the blob backed by that storage.
   */
  RefPtr<BlobPromise> RequestData();

 protected:
  ~MediaEncoder();

 private:
  /**
   * Sets mGraphTrack if not already set, using a new stream from aTrack's
   * graph.
   */
  void EnsureGraphTrackFrom(MediaTrack* aTrack);

  /**
   * Takes a regular runnable and dispatches it to the graph wrapped in a
   * ControlMessage.
   */
  void RunOnGraph(already_AddRefed<Runnable> aRunnable);

  /**
   * Calls Shutdown() if there is no remaining live track encoder.
   */
  void MaybeShutdown();

  /**
   * Shuts down the MediaEncoder and cleans up track encoders.
   * Listeners will be notified of the shutdown unless we were Cancel()ed first.
   */
  RefPtr<GenericNonExclusivePromise> Shutdown();

  /**
   * Sets mError to true, notifies listeners of the error if mError changed,
   * and stops encoding.
   */
  void SetError();

  /**
   * Creates a new MutableBlobStorage if one doesn't exist.
   */
  void MaybeCreateMutableBlobStorage();

  // Extracts encoded and muxed data into the current blob storage, creating one
  // if it doesn't exist. The returned promise resolves when data has been
  // stored into the blob.
  RefPtr<GenericPromise> Extract();

  // Stops gathering data into the current blob and resolves when the current
  // blob is available. Future data will be stored in a new blob.
  // Should a previous async GatherBlob() operation still be in progress, we'll
  // wait for it to finish before starting this one.
  RefPtr<BlobPromise> GatherBlob();

  RefPtr<BlobPromise> GatherBlobImpl();

  const RefPtr<nsISerialEventTarget> mMainThread;
  const RefPtr<TaskQueue> mEncoderThread;
  const RefPtr<DriftCompensator> mDriftCompensator;

  const UniquePtr<MediaQueue<EncodedFrame>> mEncodedAudioQueue;
  const UniquePtr<MediaQueue<EncodedFrame>> mEncodedVideoQueue;

  const UniquePtr<Muxer> mMuxer;
  const UniquePtr<AudioTrackEncoder> mAudioEncoder;
  const RefPtr<AudioTrackListener> mAudioListener;
  const UniquePtr<VideoTrackEncoder> mVideoEncoder;
  const RefPtr<VideoTrackListener> mVideoListener;
  const RefPtr<EncoderListener> mEncoderListener;

 public:
  const nsString mMimeType;

  // Max memory to use for the MutableBlobStorage.
  const uint64_t mMaxMemory;

  // The interval of passing encoded data from MutableBlobStorage to
  // onDataAvailable handler.
  const TimeDuration mTimeslice;

 private:
  nsTArray<RefPtr<MediaEncoderListener>> mListeners;

  MediaEventListener mAudioFinishListener;
  MediaEventListener mVideoFinishListener;

  // The AudioNode we are encoding.
  // Will be null when input is media stream or destination node.
  RefPtr<dom::AudioNode> mAudioNode;
  // Pipe-track for allowing a track listener on a non-destination AudioNode.
  // Will be null when input is media stream or destination node.
  RefPtr<AudioNodeTrack> mPipeTrack;
  // Input port that connect mAudioNode to mPipeTrack.
  // Will be null when input is media stream or destination node.
  RefPtr<MediaInputPort> mInputPort;
  // An audio track that we are encoding. Will be null if the input stream
  // doesn't contain audio on start() or if the input is an AudioNode.
  RefPtr<dom::AudioStreamTrack> mAudioTrack;
  // A video track that we are encoding. Will be null if the input stream
  // doesn't contain video on start() or if the input is an AudioNode.
  RefPtr<dom::VideoStreamTrack> mVideoTrack;

  // A stream to keep the MediaTrackGraph alive while we're recording.
  RefPtr<SharedDummyTrack> mGraphTrack;

  // A buffer to cache muxed encoded data.
  RefPtr<dom::MutableBlobStorage> mMutableBlobStorage;
  // If set, is a promise for the latest GatherBlob() operation. Allows
  // GatherBlob() operations to be serialized in order to avoid races.
  RefPtr<BlobPromise> mBlobPromise;
  // Timestamp of the last fired dataavailable event.
  TimeStamp mLastBlobTimeStamp;

  TimeStamp mStartTime;
  bool mInitialized;
  bool mStarted;
  bool mCompleted;
  bool mError;
  // Set when shutdown starts.
  RefPtr<GenericNonExclusivePromise> mShutdownPromise;
  // Get duration from create encoder, for logging purpose
  double GetEncodeTimeStamp() {
    TimeDuration decodeTime;
    decodeTime = TimeStamp::Now() - mStartTime;
    return decodeTime.ToMilliseconds();
  }
};

}  // namespace mozilla

#endif
