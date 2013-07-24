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

  MediaDecoder* Clone() MOZ_OVERRIDE {
    if (!IsDASHEnabled()) {
      return nullptr;
    }
    return new DASHDecoder();
  }

  // Creates a single state machine for all stream decoders.
  // Called from Load on the main thread only.
  MediaDecoderStateMachine* CreateStateMachine();

  // Loads the MPD from the network and subsequently loads the media streams.
  // Called from the main thread only.
  virtual nsresult Load(nsIStreamListener** aListener,
                        MediaDecoder* aCloneDonor) MOZ_OVERRIDE;

  // Notifies download of MPD file has ended.
  // Called on the main thread only.
  void NotifyDownloadEnded(nsresult aStatus);

  // Notification from |DASHReader| that a seek has occurred in
  // |aSubsegmentIdx|. Passes notification onto subdecoder which downloaded
  // the subsegment already, if download is in the past. Otherwise, it returns.
  void NotifySeekInVideoSubsegment(int32_t aRepDecoderIdx,
                                   int32_t aSubsegmentIdx);
  void NotifySeekInAudioSubsegment(int32_t aSubsegmentIdx);

  // Notifies that a byte range download has ended. As per the DASH spec, this
  // allows for stream switching at the boundaries of the byte ranges.
  // Called on the main thread only.
  void NotifyDownloadEnded(DASHRepDecoder* aRepDecoder,
                           nsresult aStatus,
                           int32_t const aSubsegmentIdx);

  // Notification from an |MediaDecoderReader| class that metadata has been
  // read. Declared here to allow overloading.
  void OnReadMetadataCompleted() MOZ_OVERRIDE { }

  // Seeks to aTime in seconds
  nsresult Seek(double aTime) MOZ_OVERRIDE;

  // Notification from |DASHRepDecoder| that a metadata has been read.
  // |DASHDecoder| will initiate load of data bytes for active audio/video
  // decoders. Called on the decode thread.
  void OnReadMetadataCompleted(DASHRepDecoder* aRepDecoder);

  // Returns true if all subsegments from current decode position are
  // downloaded. Must be in monitor. Call from any thread.
  bool IsDataCachedToEndOfResource() MOZ_OVERRIDE;

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
                          int32_t aSubsegmentIdx);

  // Suspend any media downloads that are in progress. Called by the
  // media element when it is sent to the bfcache, or when we need
  // to throttle the download. Call on the main thread only. This can
  // be called multiple times, there's an internal "suspend count".
  void Suspend() MOZ_OVERRIDE;

  // Resume any media downloads that have been suspended. Called by the
  // media element when it is restored from the bfcache, or when we need
  // to stop throttling the download. Call on the main thread only.
  // The download will only actually resume once as many Resume calls
  // have been made as Suspend calls. When aForceBuffering is true,
  // we force the decoder to go into buffering state before resuming
  // playback.
  void Resume(bool aForceBuffering) MOZ_OVERRIDE;
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

  // Returns the total number of subsegments that have been loaded. Will enter
  // monitor for read access off the decode thread.
  uint32_t GetNumSubsegmentLoads() {
    ReentrantMonitorConditionallyEnter mon(!OnDecodeThread(),
                                           GetReentrantMonitor());
    return mVideoSubsegmentLoads.Length();
  }

  // Returns the index of the rep decoder used to load a subsegment. Will enter
  // monitor for read access off the decode thread.
  int32_t GetRepIdxForVideoSubsegmentLoad(int32_t aSubsegmentIdx)
  {
    NS_ASSERTION(0 <= aSubsegmentIdx, "Subsegment index should not be negative.");
    ReentrantMonitorConditionallyEnter mon(!OnDecodeThread(),
                                           GetReentrantMonitor());
    if ((uint32_t)aSubsegmentIdx < mVideoSubsegmentLoads.Length()) {
      return mVideoSubsegmentLoads[aSubsegmentIdx];
    } else {
      // If it hasn't been downloaded yet, use the lowest bitrate decoder.
      return 0;
    }
  }

  // Returns the index of the rep decoder used to load a subsegment, after a
  // seek. Called on the decode thread, and will block if the subsegment
  // previous to the one specified has not yet been loaded. This ensures that
  // |DASHDecoder| has had a chance to determine which decoder should load the
  // next subsegment, in the case where |DASHRepReader|::|DecodeToTarget| has
  // read all the data for the current subsegment from the cache, and needs to
  // know which reader (including itself) to use next.
  int32_t GetRepIdxForVideoSubsegmentLoadAfterSeek(int32_t aSubsegmentIndex);

  int32_t GetSwitchCountAtVideoSubsegment(int32_t aSubsegmentIdx)
  {
    ReentrantMonitorConditionallyEnter mon(!OnDecodeThread(),
                                           GetReentrantMonitor());
    NS_ASSERTION(0 <= aSubsegmentIdx, "Subsegment index should not be negative.");
    if (aSubsegmentIdx == 0) {
      // Do the zeroeth switch next.
      return 0;
    }
    int32_t switchCount = 0;
    for (uint32_t i = 1;
         i < mVideoSubsegmentLoads.Length() &&
         i <= (uint32_t)aSubsegmentIdx;
         i++) {
      if (mVideoSubsegmentLoads[i-1] != mVideoSubsegmentLoads[i]) {
        switchCount++;
      }
    }
    return switchCount;
  }

  // The actual playback rate computation. The monitor must be held.
  // XXX Computes playback for the current video rep decoder only.
  double ComputePlaybackRate(bool* aReliable) MOZ_OVERRIDE;

  // Something has changed that could affect the computed playback rate,
  // so recompute it. The monitor must be held. Will be forwarded to current
  // audio and video rep decoders.
  void UpdatePlaybackRate() MOZ_OVERRIDE;

  // Stop updating the bytes downloaded for progress notifications. Called
  // when seeking to prevent wild changes to the progress notification.
  // Forwarded to sub-decoders. Must be called with the decoder monitor held.
  void StopProgressUpdates() MOZ_OVERRIDE;

  // Allow updating the bytes downloaded for progress notifications.
  // Forwarded to sub-decoders. Must be called with the decoder monitor held.
  void StartProgressUpdates() MOZ_OVERRIDE;

  // Used to estimate rates of data passing through the decoder's channel.
  // Records activity starting on the channel. The monitor must be held.
  virtual void NotifyPlaybackStarted() MOZ_OVERRIDE;

  // Used to estimate rates of data passing through the decoder's channel.
  // Records activity stopping on the channel. The monitor must be held.
  virtual void NotifyPlaybackStopped() MOZ_OVERRIDE;

  // Return statistics. This is used for progress events and other things.
  // This can be called from any thread. It's only a snapshot of the
  // current state, since other threads might be changing the state
  // at any time.
  // XXX Stats are calculated based on the current video rep decoder, with the
  // exception of download rate, which is based on all video downloads.
  virtual Statistics GetStatistics() MOZ_OVERRIDE;

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
    if (0 == mAudioRepDecoders.Length()) {
      return nullptr;
    }
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
    if (0 == mVideoRepDecoders.Length()) {
      return nullptr;
    }
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

  // True when Seek is called; will block any downloads until
  // |NotifySeekInSubsegment| is called, which will set it to false, and will
  // start a new series of downloads from the seeked subsegment.
  bool mSeeking;

  // Mutex for statistics.
  Mutex mStatisticsLock;
  // Stores snapshot statistics, such as download rate, for the audio|video
  // data streams. |mStatisticsLock| must be locked for access.
  nsRefPtr<MediaChannelStatistics> mAudioStatistics;
  nsRefPtr<MediaChannelStatistics> mVideoStatistics;
};

} // namespace mozilla

#endif
