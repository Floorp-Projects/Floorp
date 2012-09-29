/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */

/* This Source Code Form Is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DASH - Dynamic Adaptive Streaming over HTTP
 *
 * DASH is an adaptive bitrate streaming technology where a multimedia file is
 * partitioned into one or more segments and delivered to a client using HTTP.
 *
 * see nsDASHDecoder.cpp for info on DASH interaction with the media engine.*/

#if !defined(nsDASHRepDecoder_h_)
#define nsDASHRepDecoder_h_

#include "Representation.h"
#include "ImageLayers.h"
#include "nsDASHDecoder.h"
#include "nsWebMDecoder.h"
#include "nsWebMReader.h"
#include "nsBuiltinDecoder.h"

class nsDASHDecoder;

class nsDASHRepDecoder : public nsBuiltinDecoder
{
public:
  typedef mozilla::net::Representation Representation;
  typedef mozilla::net::SegmentBase SegmentBase;
  typedef mozilla::layers::ImageContainer ImageContainer;

  // Constructor takes a ptr to the main decoder.
  nsDASHRepDecoder(nsDASHDecoder* aMainDecoder) :
    mMainDecoder(aMainDecoder),
    mMPDRepresentation(nullptr),
    mMetadataChunkCount(0),
    mCurrentByteRange(),
    mSubsegmentIdx(0),
    mReader(nullptr)
  {
    MOZ_COUNT_CTOR(nsDASHRepDecoder);
  }

  ~nsDASHRepDecoder()
  {
    MOZ_COUNT_DTOR(nsDASHRepDecoder);
  }

  // Clone not supported; just return nullptr.
  virtual nsMediaDecoder* Clone() { return nullptr; }

  // Called by the main decoder at creation time; points to the main state
  // machine managed by the main decoder. Called on the main thread only.
  nsresult SetStateMachine(nsDecoderStateMachine* aSM);

private:
  // Overridden to return the ptr set by SetStateMachine. Called on the main
  // thread only.
  nsDecoderStateMachine* CreateStateMachine();

public:
  // Called by nsDASHDecoder at creation time; points to the media resource
  // for this decoder's |Representation|. Called on the main thread only.
  void SetResource(MediaResource* aResource);

  // Sets the |Representation| object for this decoder. Called on the main
  // thread.
  void SetMPDRepresentation(Representation const * aRep);

  // Called from nsDASHDecoder on main thread; Starts media stream download.
  nsresult Load(MediaResource* aResource = nullptr,
                nsIStreamListener** aListener = nullptr,
                nsMediaDecoder* aCloneDonor = nullptr);

  // Loads the next byte range (or first one on first call). Called on the main
  // thread only.
  void LoadNextByteRange();

  // Calls from nsDASHRepDecoder. Called on the main thread only.
  void SetReader(nsWebMReader* aReader);

  // Called if the media file encounters a network error. Call on the main
  // thread only.
  void NetworkError();

  // Set the duration of the media resource in units of seconds.
  // This is called via a channel listener if it can pick up the duration
  // from a content header. Must be called from the main thread only.
  virtual void SetDuration(double aDuration);

  // Set media stream as infinite. Called on the main thread only.
  void SetInfinite(bool aInfinite);

  // Sets media stream as seekable. Called on main thread only.
  void SetSeekable(bool aSeekable);

  // Fire progress events if needed according to the time and byte
  // constraints outlined in the specification. aTimer is true
  // if the method is called as a result of the progress timer rather
  // than the result of downloaded data.
  void Progress(bool aTimer);

  // Called as data arrives on the stream and is read into the cache.  Called
  // on the main thread only.
  void NotifyDataArrived(const char* aBuffer,
                         uint32_t aLength,
                         int64_t aOffset);

  // Called by MediaResource when some data has been received.
  // Call on the main thread only.
  void NotifyBytesDownloaded();

  // Notify that a byte range request has been completed by the media resource.
  // Called on the main thread only.
  void NotifyDownloadEnded(nsresult aStatus);

  // Called by MediaResource when the "cache suspended" status changes.
  // If MediaResource::IsSuspendedByCache returns true, then the decoder
  // should stop buffering or otherwise waiting for download progress and
  // start consuming data, if possible, because the cache is full.
  void NotifySuspendedStatusChanged();

  // Gets a byte range containing the byte offset. Call on main thread only.
  nsresult GetByteRangeForSeek(int64_t const aOffset,
                               MediaByteRange& aByteRange);

  // Returns true if the current thread is the state machine thread.
  bool OnStateMachineThread() const;

  // Returns true if the current thread is the decode thread.
  bool OnDecodeThread() const;

  // Returns main decoder's monitor for synchronised access.
  ReentrantMonitor& GetReentrantMonitor();

  // Return the current decode state, according to the main decoder. The
  // decoder monitor must be obtained before calling this.
  nsDecoderStateMachine::State GetDecodeState();

  // Called on the decode thread from nsWebMReader.
  ImageContainer* GetImageContainer();

  // Called when Metadata has been read; notifies that index data is read.
  // Called on the decode thread only.
  void OnReadMetadataCompleted();

  // Overridden to cleanup ref to |nsDASHDecoder|. Called on main thread only.
  void Shutdown() {
    NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
    // Call parent class shutdown.
    nsBuiltinDecoder::Shutdown();
    NS_ENSURE_TRUE(mShuttingDown, );
    // Cleanup ref to main decoder.
    mMainDecoder = nullptr;
  }

  // Drop reference to state machine and mReader (owned by state machine).
  // Only called during shutdown dance.
  void ReleaseStateMachine();

  // Notifies the element that decoding has failed.
  void DecodeError();

private:
  // The main decoder.
  nsRefPtr<nsDASHDecoder> mMainDecoder;
  // This decoder's MPD |Representation| object.
  Representation const * mMPDRepresentation;

  // Countdown var for loading metadata byte ranges.
  uint16_t        mMetadataChunkCount;

  // All the byte ranges for this |Representation|.
  nsTArray<MediaByteRange> mByteRanges;

  // Byte range for the init and index bytes.
  MediaByteRange  mInitByteRange;
  MediaByteRange  mIndexByteRange;

  // The current byte range being requested.
  MediaByteRange  mCurrentByteRange;
  // Index of the current byte range.
  uint64_t        mSubsegmentIdx;

  // Ptr to the reader object for this |Representation|. Owned by state
  // machine.
  nsBuiltinDecoderReader*   mReader;
};

#endif //nsDASHRepDecoder_h_
