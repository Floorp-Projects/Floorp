/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_TRACKBUFFER_H_
#define MOZILLA_TRACKBUFFER_H_

#include "SourceBufferDecoder.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/mozalloc.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nscore.h"

namespace mozilla {

class MediaSourceDecoder;

namespace dom {

class TimeRanges;

} // namespace dom

class TrackBuffer MOZ_FINAL {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TrackBuffer);

  TrackBuffer(MediaSourceDecoder* aParentDecoder, const nsACString& aType);

  void Shutdown();

  // Append data to the current decoder.  Also responsible for calling
  // NotifyDataArrived on the decoder to keep buffered range computation up
  // to date.  Returns false if the append failed.
  bool AppendData(const uint8_t* aData, uint32_t aLength);
  bool EvictData(uint32_t aThreshold);
  void EvictBefore(double aTime);

  // Returns the highest end time of all of the buffered ranges in the
  // decoders managed by this TrackBuffer, and returns the union of the
  // decoders buffered ranges in aRanges.
  double Buffered(dom::TimeRanges* aRanges);

  // Create a new decoder, set mCurrentDecoder to the new decoder, and queue
  // the decoder for initialization.  The decoder is not considered
  // initialized until it is added to mDecoders.
  bool NewDecoder();

  // Mark the current decoder's resource as ended, clear mCurrentDecoder and
  // reset mLast{Start,End}Timestamp.
  void DiscardDecoder();

  void Detach();

  // Returns true if an init segment has been appended.
  bool HasInitSegment();

  // Returns true iff HasInitSegment() and the decoder using that init
  // segment has successfully initialized by setting mHas{Audio,Video}..
  bool IsReady();

  // Query and update mLast{Start,End}Timestamp.
  void LastTimestamp(double& aStart, double& aEnd);
  void SetLastStartTimestamp(double aStart);
  void SetLastEndTimestamp(double aEnd);

  // Returns true if any of the decoders managed by this track buffer
  // contain aTime in their buffered ranges.
  bool ContainsTime(double aTime);

  void BreakCycles();

  // Call ResetDecode() on each decoder in mDecoders.
  void ResetDecode();

  // Returns a reference to mInitializedDecoders, used by MediaSourceReader
  // to select decoders.
  // TODO: Refactor to a cleaner interface between TrackBuffer and MediaSourceReader.
  const nsTArray<nsRefPtr<SourceBufferDecoder>>& Decoders();

#if defined(DEBUG)
  void Dump(const char* aPath);
#endif

private:
  ~TrackBuffer();

  // Queue execution of InitializeDecoder on mTaskQueue.
  bool QueueInitializeDecoder(nsRefPtr<SourceBufferDecoder> aDecoder);

  // Runs decoder initialization including calling ReadMetadata.  Runs as an
  // event on the decode thread pool.
  void InitializeDecoder(nsRefPtr<SourceBufferDecoder> aDecoder);

  // Adds a successfully initialized decoder to mDecoders and (if it's the
  // first decoder initialized), initializes mHasAudio/mHasVideo.  Called
  // from the decode thread pool.
  void RegisterDecoder(nsRefPtr<SourceBufferDecoder> aDecoder);

  // A task queue using the shared media thread pool.  Used exclusively to
  // initialize (i.e. call ReadMetadata on) decoders as they are created via
  // NewDecoder.
  RefPtr<MediaTaskQueue> mTaskQueue;

  // All of the decoders managed by this TrackBuffer.  Access protected by
  // mParentDecoder's monitor.
  nsTArray<nsRefPtr<SourceBufferDecoder>> mDecoders;

  // Contains only the initialized decoders managed by this TrackBuffer.
  // Access protected by mParentDecoder's monitor.
  nsTArray<nsRefPtr<SourceBufferDecoder>> mInitializedDecoders;

  // The decoder that the owning SourceBuffer is currently appending data to.
  nsRefPtr<SourceBufferDecoder> mCurrentDecoder;

  nsRefPtr<MediaSourceDecoder> mParentDecoder;
  const nsCString mType;

  // The last start and end timestamps added to the TrackBuffer via
  // AppendData.  Accessed on the main thread only.
  double mLastStartTimestamp;
  double mLastEndTimestamp;

  // Set when the initialization segment is first seen and cached (implied
  // by new decoder creation).  Protected by mParentDecoder's monitor.
  bool mHasInit;

  // Set when the first decoder used by this TrackBuffer is initialized.
  // Protected by mParentDecoder's monitor.
  bool mHasAudio;
  bool mHasVideo;
};

} // namespace mozilla
#endif /* MOZILLA_TRACKBUFFER_H_ */
