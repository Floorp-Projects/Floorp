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
 * see DASHDecoder.cpp for info on DASH interaction with the media engine.*/

#if !defined(DASHDecoder_h_)
#define DASHDecoder_h_

#include "nsTArray.h"
#include "nsIURI.h"
#include "nsITimer.h"
#include "nsThreadUtils.h"
#include "MediaDecoder.h"
#include "DASHReader.h"

namespace mozilla {
namespace net {
class IMPDManager;
class nsDASHMPDParser;
class Representation;
}// net

class DASHRepDecoder;

class DASHDecoder : public MediaDecoder
{
public:
  typedef class mozilla::net::IMPDManager IMPDManager;
  typedef class mozilla::net::nsDASHMPDParser nsDASHMPDParser;
  typedef class mozilla::net::Representation Representation;

  // XXX Arbitrary max file size for MPD. 50MB seems generously large.
  static const uint32_t DASH_MAX_MPD_SIZE = 50*1024*1024;

  DASHDecoder();
  ~DASHDecoder();

  // Clone not supported; just return nullptr.
  MediaDecoder* Clone() { return nullptr; }

  // Creates a single state machine for all stream decoders.
  // Called from Load on the main thread only.
  MediaDecoderStateMachine* CreateStateMachine();

  // Loads the MPD from the network and subsequently loads the media streams.
  // Called from the main thread only.
  nsresult Load(MediaResource* aResource,
                nsIStreamListener** aListener,
                MediaDecoder* aCloneDonor);

  // Notifies download of MPD file has ended.
  // Called on the main thread only.
  void NotifyDownloadEnded(nsresult aStatus);

  // Notifies that a byte range download has ended. As per the DASH spec, this
  // allows for stream switching at the boundaries of the byte ranges.
  // Called on the main thread only.
  void NotifyDownloadEnded(DASHRepDecoder* aRepDecoder,
                           nsresult aStatus,
                           int32_t const aSubsegmentIdx);

  // Notification from an |MediaDecoderReader| class that metadata has been
  // read. Declared here to allow overloading.
  void OnReadMetadataCompleted() MOZ_OVERRIDE { }

  // Notification from |DASHRepDecoder| that a metadata has been read.
  // |DASHDecoder| will initiate load of data bytes for active audio/video
  // decoders. Called on the decode thread.
  void OnReadMetadataCompleted(DASHRepDecoder* aRepDecoder);

  // Refers to downloading data bytes, i.e. non metadata.
  // Returns true if |aRepDecoder| is an active audio or video sub decoder AND
  // if metadata for all audio or video decoders has been read.
  // Could be called from any thread; enters decoder monitor.
  bool IsDecoderAllowedToDownloadData(DASHRepDecoder* aRepDecoder);

  // Refers to downloading data bytes during SEEKING.
  // Returns true if |aRepDecoder| is the active audio sub decoder, OR if
  // it is a video decoder and is allowed to download this subsegment.
  // Returns false if there is still some metadata to download.
  // Could be called from any thread; enters decoder monitor.
  bool IsDecoderAllowedToDownloadSubsegment(DASHRepDecoder* aRepDecoder,
                                            int32_t const aSubsegmentIdx);

  // Determines if rep/sub decoders should be switched, and if so switches
  // them. Notifies |DASHReader| if and when it should switch readers.
  // Returns a pointer to the new active decoder.
  // Called on the main thread.
  nsresult PossiblySwitchDecoder(DASHRepDecoder* aRepDecoder);

  // Sets the byte range index for audio|video downloads. Will only increment
  // for current active decoders. Could be called from any thread.
  // Requires monitor because of write to |mAudioSubsegmentIdx| or
  // |mVideoSubsegmentIdx|.
  void SetSubsegmentIndex(DASHRepDecoder* aRepDecoder,
                          uint32_t aSubsegmentIdx)
  {
    ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
    if (aRepDecoder == AudioRepDecoder()) {
      mAudioSubsegmentIdx = aSubsegmentIdx;
    } else if (aRepDecoder == VideoRepDecoder()) {
      mVideoSubsegmentIdx = aSubsegmentIdx;
    }
  }
private:
  // Increments the byte range index for audio|video downloads. Will only
  // increment for current active decoders. Could be called from any thread.
  // Requires monitor because of write to |mAudioSubsegmentIdx| or
  // |mVideoSubsegmentIdx|.
  void IncrementSubsegmentIndex(DASHRepDecoder* aRepDecoder)
  {
    ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
    if (aRepDecoder == AudioRepDecoder()) {
      mAudioSubsegmentIdx++;
    } else if (aRepDecoder == VideoRepDecoder()) {
      mVideoSubsegmentIdx++;
    }
  }
public:
  // Gets the byte range index for audio|video downloads. Will only increment
  // for current active decoders. Could be called from any thread. Will enter
  // monitor for read access off the decode thread.
  int32_t GetSubsegmentIndex(DASHRepDecoder* aRepDecoder)
  {
    ReentrantMonitorConditionallyEnter mon(!OnDecodeThread(),
                                           GetReentrantMonitor());
    if (aRepDecoder == AudioRepDecoder()) {
      return mAudioSubsegmentIdx;
    } else if (aRepDecoder == VideoRepDecoder()) {
      return mVideoSubsegmentIdx;
    }
    return (-1);
  }

  // Returns the index of the rep decoder used to load a subsegment. Will enter
  // monitor for read access off the decode thread.
  int32_t GetRepIdxForVideoSubsegmentLoad(int32_t aSubsegmentIdx)
  {
    NS_ASSERTION(0 < aSubsegmentIdx, "Subsegment index should not be negative.");
    ReentrantMonitorConditionallyEnter mon(!OnDecodeThread(),
                                           GetReentrantMonitor());
    if ((uint32_t)aSubsegmentIdx < mVideoSubsegmentLoads.Length()) {
      return mVideoSubsegmentLoads[aSubsegmentIdx];
    } else {
      // If it hasn't been downloaded yet, use the lowest bitrate decoder.
      return 0;
    }
  }

  // Drop reference to state machine and tell sub-decoders to do the same.
  // Only called during shutdown dance, on main thread only.
  void ReleaseStateMachine();

  // Overridden to forward |Shutdown| to sub-decoders.
  // Called on the main thread only.
  void Shutdown();

  // Called by sub-decoders when load has been aborted. Will notify media
  // element only once. Called on the main thread only.
  void LoadAborted();

  // Notifies the element that decoding has failed. On main thread, call is
  // forwarded to |MediaDecoder|::|Error| immediately. On other threads,
  // a call is dispatched for execution on the main thread.
  void DecodeError();

private:
  // Reads the MPD data from resource to a byte stream.
  // Called on the MPD reader thread.
  void ReadMPDBuffer();

  // Called when MPD data is completely read.
  // On the main thread.
  void OnReadMPDBufferCompleted();

  // Parses the copied MPD byte stream.
  // On the main thread: DOM APIs complain when off the main thread.
  nsresult ParseMPDBuffer();

  // Creates the sub-decoders for a |Representation|, i.e. media streams.
  // On the main thread.
  nsresult CreateRepDecoders();

  // Creates audio/video decoders for individual |Representation|s.
  // On the main thread.
  nsresult CreateAudioRepDecoder(nsIURI* aUrl, Representation const * aRep);
  nsresult CreateVideoRepDecoder(nsIURI* aUrl, Representation const * aRep);

  // Get audio sub-decoder for current audio |Representation|. Will return
  // nullptr for out of range indexes.
  // Enters monitor for read access off the decode thread.
  // XXX Note: Although an array of audio decoders is provided, audio stream
  // switching is not yet supported.
  DASHRepDecoder* AudioRepDecoder() {
    ReentrantMonitorConditionallyEnter mon(!OnDecodeThread(),
                                           GetReentrantMonitor());
    NS_ENSURE_TRUE((uint32_t)mAudioRepDecoderIdx < mAudioRepDecoders.Length(),
                   nullptr);
    if (mAudioRepDecoderIdx < 0) {
      return nullptr;
    } else {
      return mAudioRepDecoders[mAudioRepDecoderIdx];
    }
  }

  // Get video sub-decoder for current video |Representation|. Will return
  // nullptr for out of range indexes.
  // Enters monitor for read access off the decode thread.
  DASHRepDecoder* VideoRepDecoder() {
    ReentrantMonitorConditionallyEnter mon(!OnDecodeThread(),
                                           GetReentrantMonitor());
    NS_ENSURE_TRUE((uint32_t)mVideoRepDecoderIdx < mVideoRepDecoders.Length(),
                   nullptr);
    if (mVideoRepDecoderIdx < 0) {
      return nullptr;
    } else {
      return mVideoRepDecoders[mVideoRepDecoderIdx];
    }
  }

  // Creates audio/video resources for individual |Representation|s.
  // On the main thread.
  MediaResource* CreateAudioSubResource(nsIURI* aUrl,
                                        MediaDecoder* aAudioDecoder);
  MediaResource* CreateVideoSubResource(nsIURI* aUrl,
                                        MediaDecoder* aVideoDecoder);

  // Creates an http channel for a |Representation|.
  // On the main thread.
  nsresult CreateSubChannel(nsIURI* aUrl, nsIChannel** aChannel);

  // Loads the media |Representations|, i.e. the media streams.
  // On the main thread.
  nsresult LoadRepresentations();

  // True when media element has already been notified of an aborted load.
  bool mNotifiedLoadAborted;

  // Ptr for the MPD data.
  nsAutoArrayPtr<char>         mBuffer;
  // Length of the MPD data.
  uint32_t                     mBufferLength;
  // Ptr to the MPD Reader thread.
  nsCOMPtr<nsIThread>          mMPDReaderThread;
  // Document Principal.
  nsCOMPtr<nsIPrincipal>       mPrincipal;

  // MPD Manager provides access to the MPD information.
  nsAutoPtr<IMPDManager>       mMPDManager;

  // Main reader object; manages all sub-readers for |Representation|s. Owned by
  // state machine; destroyed in state machine's destructor.
  DASHReader* mDASHReader;

  // Sub-decoder vars. Note: For all following members, the decode monitor
  // should be held for write access on decode thread, and all read/write off
  // the decode thread.

  // Index of the video |AdaptationSet|.
  int32_t mVideoAdaptSetIdx;

  // Indexes for the current audio and video decoders.
  int32_t mAudioRepDecoderIdx;
  int32_t mVideoRepDecoderIdx;

  // Array of pointers for the |Representation|s in the audio/video
  // |AdaptationSet|.
  nsTArray<nsRefPtr<DASHRepDecoder> > mAudioRepDecoders;
  nsTArray<nsRefPtr<DASHRepDecoder> > mVideoRepDecoders;

  // Current index of subsegments downloaded for audio/video decoder.
  int32_t mAudioSubsegmentIdx;
  int32_t mVideoSubsegmentIdx;

  // Count for the number of readers which have called |OnReadMetadataCompleted|.
  // Initialised to 0; incremented for every decoder which has |Load| called;
  // and decremented for every call to |OnReadMetadataCompleted|. When it is
  // zero again, all metadata has been read for audio or video, and data bytes
  // can be downloaded.
  uint32_t mAudioMetadataReadCount;
  uint32_t mVideoMetadataReadCount;

  // Array records the index of the decoder/Representation which loaded each
  // subsegment.
  nsTArray<int32_t> mVideoSubsegmentLoads;
};

} // namespace mozilla

#endif
