/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_TRACKBUFFER_H_
#define MOZILLA_TRACKBUFFER_H_

#include "SourceBuffer.h"
#include "SourceBufferDecoder.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/mozalloc.h"
#include "mozilla/Maybe.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nscore.h"

namespace mozilla {

class ContainerParser;
class MediaSourceDecoder;
class LargeDataBuffer;

namespace dom {

class TimeRanges;

} // namespace dom

class TrackBuffer MOZ_FINAL {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TrackBuffer);

  TrackBuffer(MediaSourceDecoder* aParentDecoder, const nsACString& aType);

  nsRefPtr<ShutdownPromise> Shutdown();

  // Append data to the current decoder.  Also responsible for calling
  // NotifyDataArrived on the decoder to keep buffered range computation up
  // to date.  Returns false if the append failed.
  nsRefPtr<TrackBufferAppendPromise> AppendData(LargeDataBuffer* aData,
                                                int64_t aTimestampOffset /* microseconds */);

  // Evicts data held in the current decoders SourceBufferResource from the
  // start of the buffer through to aPlaybackTime. aThreshold is used to
  // bound the data being evicted. It will not evict more than aThreshold
  // bytes. aBufferStartTime contains the new start time of the current
  // decoders buffered data after the eviction. Returns true if data was
  // evicted.
  bool EvictData(double aPlaybackTime, uint32_t aThreshold, double* aBufferStartTime);

  // Evicts data held in all the decoders SourceBufferResource from the start
  // of the buffer through to aTime.
  void EvictBefore(double aTime);

  // Returns the highest end time of all of the buffered ranges in the
  // decoders managed by this TrackBuffer, and returns the union of the
  // decoders buffered ranges in aRanges. This may be called on any thread.
  double Buffered(dom::TimeRanges* aRanges);

  // Mark the current decoder's resource as ended, clear mCurrentDecoder and
  // reset mLast{Start,End}Timestamp.  Main thread only.
  void DiscardCurrentDecoder();
  // Mark the current decoder's resource as ended.
  void EndCurrentDecoder();

  void Detach();

  // Returns true if an init segment has been appended.
  bool HasInitSegment();

  // Returns true iff mParser->HasInitData() and the decoder using that init
  // segment has successfully initialized by setting mHas{Audio,Video}..
  bool IsReady();

  // Returns true if any of the decoders managed by this track buffer
  // contain aTime in their buffered ranges.
  bool ContainsTime(int64_t aTime, int64_t aTolerance);

  void BreakCycles();

  // Call ResetDecode() on each decoder in mDecoders.
  void ResetDecode();

  // Run MSE Reset Parser State Algorithm.
  // 3.5.2 Reset Parser State
  // http://w3c.github.io/media-source/#sourcebuffer-reset-parser-state
  void ResetParserState();

  // Returns a reference to mInitializedDecoders, used by MediaSourceReader
  // to select decoders.
  // TODO: Refactor to a cleaner interface between TrackBuffer and MediaSourceReader.
  const nsTArray<nsRefPtr<SourceBufferDecoder>>& Decoders();

  // Runs MSE range removal algorithm.
  // http://w3c.github.io/media-source/#sourcebuffer-coded-frame-removal
  // Implementation is only partial, we can only trim a buffer.
  // Returns true if data was evicted.
  // Times are in microseconds.
  bool RangeRemoval(int64_t aStart, int64_t aEnd);

  // Abort any pending appendBuffer by rejecting any pending promises.
  void AbortAppendData();

  // Return the size used by all decoders managed by this TrackBuffer.
  int64_t GetSize();

  // Return true if we have a partial media segment being appended that is
  // currently not playable.
  bool HasOnlyIncompleteMedia();

#ifdef MOZ_EME
  nsresult SetCDMProxy(CDMProxy* aProxy);
#endif

#if defined(DEBUG)
  void Dump(const char* aPath);
#endif

private:
  friend class DecodersToInitialize;
  ~TrackBuffer();

  // Create a new decoder, set mCurrentDecoder to the new decoder and
  // returns it. The new decoder must be queued using QueueInitializeDecoder
  // for initialization.
  // The decoder is not considered initialized until it is added to
  // mInitializedDecoders.
  already_AddRefed<SourceBufferDecoder> NewDecoder(int64_t aTimestampOffset /* microseconds */);

  // Helper for AppendData, ensures NotifyDataArrived is called whenever
  // data is appended to the current decoder's SourceBufferResource.
  bool AppendDataToCurrentResource(LargeDataBuffer* aData,
                                   uint32_t aDuration /* microseconds */);

  // Queue execution of InitializeDecoder on mTaskQueue.
  bool QueueInitializeDecoder(SourceBufferDecoder* aDecoder);

  // Runs decoder initialization including calling ReadMetadata.  Runs as an
  // event on the decode thread pool.
  void InitializeDecoder(SourceBufferDecoder* aDecoder);
  // Once decoder has been initialized, set mediasource duration if required
  // and resolve any pending InitializationPromise.
  // Setting the mediasource duration must be done on the main thread.
  // TODO: Why is that so?
  void CompleteInitializeDecoder(SourceBufferDecoder* aDecoder);

  // Adds a successfully initialized decoder to mDecoders and (if it's the
  // first decoder initialized), initializes mHasAudio/mHasVideo.  Called
  // from the decode thread pool.  Return true if the decoder was
  // successfully registered.
  bool RegisterDecoder(SourceBufferDecoder* aDecoder);

  // Returns true if aInfo is considered a supported or the same format as
  // the TrackBuffer was initialized as.
  bool ValidateTrackFormats(const MediaInfo& aInfo);

  // Remove aDecoder from mDecoders and dispatch an event to the main thread
  // to clean up the decoder.  If aDecoder was added to
  // mInitializedDecoders, it must have been removed before calling this
  // function.
  void RemoveDecoder(SourceBufferDecoder* aDecoder);

  // Remove all empty decoders from the provided list;
  void RemoveEmptyDecoders(nsTArray<SourceBufferDecoder*>& aDecoders);

  nsAutoPtr<ContainerParser> mParser;

  // A task queue using the shared media thread pool.  Used exclusively to
  // initialize (i.e. call ReadMetadata on) decoders as they are created via
  // NewDecoder.
  RefPtr<MediaTaskQueue> mTaskQueue;

  // All of the decoders managed by this TrackBuffer.  Access protected by
  // mParentDecoder's monitor.
  nsTArray<nsRefPtr<SourceBufferDecoder>> mDecoders;

  // During shutdown, we move decoders from mDecoders to mShutdownDecoders after
  // invoking Shutdown. This is all so that we can avoid destroying the decoders
  // off-main-thread. :-(
  nsTArray<nsRefPtr<SourceBufferDecoder>> mShutdownDecoders;

  // Contains only the initialized decoders managed by this TrackBuffer.
  // Access protected by mParentDecoder's monitor.
  nsTArray<nsRefPtr<SourceBufferDecoder>> mInitializedDecoders;

  // Decoders which are waiting on a Content Decryption Module to be able to
  // finish ReadMetadata.
  nsTArray<nsRefPtr<SourceBufferDecoder>> mWaitingDecoders;

  // The decoder that the owning SourceBuffer is currently appending data to.
  // Modified on the main thread only.
  nsRefPtr<SourceBufferDecoder> mCurrentDecoder;

  nsRefPtr<MediaSourceDecoder> mParentDecoder;
  const nsCString mType;

  // The last start and end timestamps added to the TrackBuffer via
  // AppendData.  Accessed on the main thread only.
  int64_t mLastStartTimestamp;
  Maybe<int64_t> mLastEndTimestamp;
  void AdjustDecodersTimestampOffset(int32_t aOffset);

  // The timestamp offset used by our current decoder, in microseconds.
  int64_t mLastTimestampOffset;
  int64_t mAdjustedTimestamp;

  // Set when the first decoder used by this TrackBuffer is initialized.
  // Protected by mParentDecoder's monitor.
  MediaInfo mInfo;

  void ContinueShutdown();
  MediaPromiseHolder<ShutdownPromise> mShutdownPromise;
  bool mDecoderPerSegment;
  bool mShutdown;

  MediaPromiseHolder<TrackBufferAppendPromise> mInitializationPromise;

};

} // namespace mozilla
#endif /* MOZILLA_TRACKBUFFER_H_ */
