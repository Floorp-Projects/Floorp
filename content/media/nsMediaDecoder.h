/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Double <chris.double@double.co.nz>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#if !defined(nsMediaDecoder_h_)
#define nsMediaDecoder_h_

#include "ImageLayers.h"
#include "mozilla/ReentrantMonitor.h"
#include "VideoFrameContainer.h"

class nsHTMLMediaElement;
class nsIStreamListener;
class nsTimeRanges;
class nsIMemoryReporter;
class nsIPrincipal;
class nsITimer;

namespace mozilla {
class MediaResource;
}

// The size to use for audio data frames in MozAudioAvailable events.
// This value is per channel, and is chosen to give ~43 fps of events,
// for example, 44100 with 2 channels, 2*1024 = 2048.
static const PRUint32 FRAMEBUFFER_LENGTH_PER_CHANNEL = 1024;

// The total size of the framebuffer used for MozAudioAvailable events
// has to be within the following range.
static const PRUint32 FRAMEBUFFER_LENGTH_MIN = 512;
static const PRUint32 FRAMEBUFFER_LENGTH_MAX = 16384;

// All methods of nsMediaDecoder must be called from the main thread only
// with the exception of GetVideoFrameContainer and GetStatistics,
// which can be called from any thread.
class nsMediaDecoder : public nsIObserver
{
public:
  typedef mozilla::MediaResource MediaResource;
  typedef mozilla::ReentrantMonitor ReentrantMonitor;
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::TimeDuration TimeDuration;
  typedef mozilla::VideoFrameContainer VideoFrameContainer;
  typedef mozilla::layers::Image Image;
  typedef mozilla::layers::ImageContainer ImageContainer;

  nsMediaDecoder();
  virtual ~nsMediaDecoder();

  // Create a new decoder of the same type as this one.
  virtual nsMediaDecoder* Clone() = 0;

  // Perform any initialization required for the decoder.
  // Return true on successful initialisation, false
  // on failure.
  virtual bool Init(nsHTMLMediaElement* aElement);

  // Get the current MediaResource being used. Its URI will be returned
  // by currentSrc. Returns what was passed to Load(), if Load() has been called.
  virtual MediaResource* GetResource() = 0;

  // Return the principal of the current URI being played or downloaded.
  virtual already_AddRefed<nsIPrincipal> GetCurrentPrincipal() = 0;

  // Return the time position in the video stream being
  // played measured in seconds.
  virtual double GetCurrentTime() = 0;

  // Seek to the time position in (seconds) from the start of the video.
  virtual nsresult Seek(double aTime) = 0;

  // Called by the element when the playback rate has been changed.
  // Adjust the speed of the playback, optionally with pitch correction,
  // when this is called.
  virtual nsresult PlaybackRateChanged() = 0;

  // Return the duration of the video in seconds.
  virtual double GetDuration() = 0;

  // A media stream is assumed to be infinite if the metadata doesn't
  // contain the duration, and range requests are not supported, and
  // no headers give a hint of a possible duration (Content-Length,
  // Content-Duration, and variants), and we cannot seek in the media
  // stream to determine the duration.
  //
  // When the media stream ends, we can know the duration, thus the stream is
  // no longer considered to be infinite.
  virtual void SetInfinite(bool aInfinite) = 0;

  // Return true if the stream is infinite (see SetInfinite).
  virtual bool IsInfinite() = 0;

  // Pause video playback.
  virtual void Pause() = 0;

  // Set the audio volume. It should be a value from 0 to 1.0.
  virtual void SetVolume(double aVolume) = 0;

  // Start playback of a video. 'Load' must have previously been
  // called.
  virtual nsresult Play() = 0;

  // Start downloading the media. Decode the downloaded data up to the
  // point of the first frame of data.
  // aResource is the media stream to use. Ownership of aResource passes to
  // the decoder, even if Load returns an error.
  // This is called at most once per decoder, after Init().
  virtual nsresult Load(MediaResource* aResource,
                        nsIStreamListener **aListener,
                        nsMediaDecoder* aCloneDonor) = 0;

  // Called when the video file has completed downloading.
  virtual void ResourceLoaded() = 0;

  // Called if the media file encounters a network error.
  virtual void NetworkError() = 0;

  // Call from any thread safely. Return true if we are currently
  // seeking in the media resource.
  virtual bool IsSeeking() const = 0;

  // Return true if the decoder has reached the end of playback.
  // Call in the main thread only.
  virtual bool IsEnded() const = 0;

  // Called when a "MozAudioAvailable" event listener is added. This enables
  // the decoder to only dispatch "MozAudioAvailable" events when a
  // handler exists, reducing overhead. Called on the main thread.
  virtual void NotifyAudioAvailableListener() = 0;

  struct Statistics {
    // Estimate of the current playback rate (bytes/second).
    double mPlaybackRate;
    // Estimate of the current download rate (bytes/second). This
    // ignores time that the channel was paused by Gecko.
    double mDownloadRate;
    // Total length of media stream in bytes; -1 if not known
    PRInt64 mTotalBytes;
    // Current position of the download, in bytes. This is the offset of
    // the first uncached byte after the decoder position.
    PRInt64 mDownloadPosition;
    // Current position of decoding, in bytes (how much of the stream
    // has been consumed)
    PRInt64 mDecoderPosition;
    // Current position of playback, in bytes
    PRInt64 mPlaybackPosition;
    // If false, then mDownloadRate cannot be considered a reliable
    // estimate (probably because the download has only been running
    // a short time).
    bool mDownloadRateReliable;
    // If false, then mPlaybackRate cannot be considered a reliable
    // estimate (probably because playback has only been running
    // a short time).
    bool mPlaybackRateReliable;
  };

  // Frame decoding/painting related performance counters.
  // Threadsafe.
  class FrameStatistics {
  public:
    
    FrameStatistics() :
        mReentrantMonitor("nsMediaDecoder::FrameStats"),
        mParsedFrames(0),
        mDecodedFrames(0),
        mPresentedFrames(0) {}

    // Returns number of frames which have been parsed from the media.
    // Can be called on any thread.
    PRUint32 GetParsedFrames() {
      mozilla::ReentrantMonitorAutoEnter mon(mReentrantMonitor);
      return mParsedFrames;
    }

    // Returns the number of parsed frames which have been decoded.
    // Can be called on any thread.
    PRUint32 GetDecodedFrames() {
      mozilla::ReentrantMonitorAutoEnter mon(mReentrantMonitor);
      return mDecodedFrames;
    }

    // Returns the number of decoded frames which have been sent to the rendering
    // pipeline for painting ("presented").
    // Can be called on any thread.
    PRUint32 GetPresentedFrames() {
      mozilla::ReentrantMonitorAutoEnter mon(mReentrantMonitor);
      return mPresentedFrames;
    }

    // Increments the parsed and decoded frame counters by the passed in counts.
    // Can be called on any thread.
    void NotifyDecodedFrames(PRUint32 aParsed, PRUint32 aDecoded) {
      if (aParsed == 0 && aDecoded == 0)
        return;
      mozilla::ReentrantMonitorAutoEnter mon(mReentrantMonitor);
      mParsedFrames += aParsed;
      mDecodedFrames += aDecoded;
    }

    // Increments the presented frame counters.
    // Can be called on any thread.
    void NotifyPresentedFrame() {
      mozilla::ReentrantMonitorAutoEnter mon(mReentrantMonitor);
      ++mPresentedFrames;
    }

  private:

    // ReentrantMonitor to protect access of playback statistics.
    ReentrantMonitor mReentrantMonitor;

    // Number of frames parsed and demuxed from media.
    // Access protected by mStatsReentrantMonitor.
    PRUint32 mParsedFrames;

    // Number of parsed frames which were actually decoded.
    // Access protected by mStatsReentrantMonitor.
    PRUint32 mDecodedFrames;

    // Number of decoded frames which were actually sent down the rendering
    // pipeline to be painted ("presented"). Access protected by mStatsReentrantMonitor.
    PRUint32 mPresentedFrames;
  };

  // Stack based class to assist in notifying the frame statistics of
  // parsed and decoded frames. Use inside video demux & decode functions
  // to ensure all parsed and decoded frames are reported on all return paths.
  class AutoNotifyDecoded {
  public:
    AutoNotifyDecoded(nsMediaDecoder* aDecoder, PRUint32& aParsed, PRUint32& aDecoded)
      : mDecoder(aDecoder), mParsed(aParsed), mDecoded(aDecoded) {}
    ~AutoNotifyDecoded() {
      mDecoder->GetFrameStatistics().NotifyDecodedFrames(mParsed, mDecoded);
    }
  private:
    nsMediaDecoder* mDecoder;
    PRUint32& mParsed;
    PRUint32& mDecoded;
  };

  // Return statistics. This is used for progress events and other things.
  // This can be called from any thread. It's only a snapshot of the
  // current state, since other threads might be changing the state
  // at any time.
  virtual Statistics GetStatistics() = 0;
  
  // Return the frame decode/paint related statistics.
  FrameStatistics& GetFrameStatistics() { return mFrameStats; }

  // Set the duration of the media resource in units of seconds.
  // This is called via a channel listener if it can pick up the duration
  // from a content header. Must be called from the main thread only.
  virtual void SetDuration(double aDuration) = 0;

  // Set a flag indicating whether seeking is supported
  virtual void SetSeekable(bool aSeekable) = 0;

  // Return true if seeking is supported.
  virtual bool IsSeekable() = 0;

  // Return the time ranges that can be seeked into.
  virtual nsresult GetSeekable(nsTimeRanges* aSeekable) = 0;

  // Set the end time of the media resource. When playback reaches
  // this point the media pauses. aTime is in seconds.
  virtual void SetEndTime(double aTime) = 0;

  // Invalidate the frame.
  void Invalidate()
  {
    if (mVideoFrameContainer) {
      mVideoFrameContainer->Invalidate();
    }
  }

  // Fire progress events if needed according to the time and byte
  // constraints outlined in the specification. aTimer is true
  // if the method is called as a result of the progress timer rather
  // than the result of downloaded data.
  virtual void Progress(bool aTimer);

  // Fire timeupdate events if needed according to the time constraints
  // outlined in the specification.
  virtual void FireTimeUpdate();

  // Called by MediaResource when the "cache suspended" status changes.
  // If MediaResource::IsSuspendedByCache returns true, then the decoder
  // should stop buffering or otherwise waiting for download progress and
  // start consuming data, if possible, because the cache is full.
  virtual void NotifySuspendedStatusChanged() = 0;

  // Called by MediaResource when some data has been received.
  // Call on the main thread only.
  virtual void NotifyBytesDownloaded() = 0;

  // Called by nsChannelToPipeListener or MediaResource when the
  // download has ended. Called on the main thread only. aStatus is
  // the result from OnStopRequest.
  virtual void NotifyDownloadEnded(nsresult aStatus) = 0;

  // Called as data arrives on the stream and is read into the cache.  Called
  // on the main thread only.
  virtual void NotifyDataArrived(const char* aBuffer, PRUint32 aLength, PRInt64 aOffset) = 0;

  // Cleanup internal data structures. Must be called on the main
  // thread by the owning object before that object disposes of this object.
  virtual void Shutdown();

  // Suspend any media downloads that are in progress. Called by the
  // media element when it is sent to the bfcache, or when we need
  // to throttle the download. Call on the main thread only. This can
  // be called multiple times, there's an internal "suspend count".
  virtual void Suspend() = 0;

  // Resume any media downloads that have been suspended. Called by the
  // media element when it is restored from the bfcache, or when we need
  // to stop throttling the download. Call on the main thread only.
  // The download will only actually resume once as many Resume calls
  // have been made as Suspend calls. When aForceBuffering is true,
  // we force the decoder to go into buffering state before resuming
  // playback.
  virtual void Resume(bool aForceBuffering) = 0;

  // Returns a weak reference to the media element we're decoding for,
  // if it's available.
  nsHTMLMediaElement* GetMediaElement();

  // Returns the current size of the framebuffer used in
  // MozAudioAvailable events.
  PRUint32 GetFrameBufferLength() { return mFrameBufferLength; };

  // Sets the length of the framebuffer used in MozAudioAvailable events.
  // The new size must be between 512 and 16384.
  virtual nsresult RequestFrameBufferLength(PRUint32 aLength);

  // Moves any existing channel loads into the background, so that they don't
  // block the load event. This is called when we stop delaying the load
  // event. Any new loads initiated (for example to seek) will also be in the
  // background. Implementations of this must call MoveLoadsToBackground() on
  // their MediaResource.
  virtual void MoveLoadsToBackground()=0;

  // Constructs the time ranges representing what segments of the media
  // are buffered and playable.
  virtual nsresult GetBuffered(nsTimeRanges* aBuffered) = 0;

  // Returns true if we can play the entire media through without stopping
  // to buffer, given the current download and playback rates.
  bool CanPlayThrough();

  // Returns the size, in bytes, of the heap memory used by the currently
  // queued decoded video and audio data.
  virtual PRInt64 VideoQueueMemoryInUse() = 0;
  virtual PRInt64 AudioQueueMemoryInUse() = 0;

  VideoFrameContainer* GetVideoFrameContainer() { return mVideoFrameContainer; }
  ImageContainer* GetImageContainer()
  {
    return mVideoFrameContainer ? mVideoFrameContainer->GetImageContainer() : nsnull;
  }

protected:

  // Start timer to update download progress information.
  nsresult StartProgress();

  // Stop progress information timer.
  nsresult StopProgress();

  // Ensures our media stream has been pinned.
  void PinForSeek();

  // Ensures our media stream has been unpinned.
  void UnpinForSeek();

  // Timer used for updating progress events
  nsCOMPtr<nsITimer> mProgressTimer;

  // This should only ever be accessed from the main thread.
  // It is set in Init and cleared in Shutdown when the element goes away.
  // The decoder does not add a reference the element.
  nsHTMLMediaElement* mElement;

  // Counters related to decode and presentation of frames.
  FrameStatistics mFrameStats;

  nsRefPtr<VideoFrameContainer> mVideoFrameContainer;

  // Time that the last progress event was fired. Read/Write from the
  // main thread only.
  TimeStamp mProgressTime;

  // Time that data was last read from the media resource. Used for
  // computing if the download has stalled and to rate limit progress events
  // when data is arriving slower than PROGRESS_MS. A value of null indicates
  // that a stall event has already fired and not to fire another one until
  // more data is received. Read/Write from the main thread only.
  TimeStamp mDataTime;

  // The framebuffer size to use for audioavailable events.
  PRUint32 mFrameBufferLength;

  // True when our media stream has been pinned. We pin the stream
  // while seeking.
  bool mPinnedForSeek;

  // True if the decoder is being shutdown. At this point all events that
  // are currently queued need to return immediately to prevent javascript
  // being run that operates on the element and decoder during shutdown.
  // Read/Write from the main thread only.
  bool mShuttingDown;
};

namespace mozilla {
class MediaMemoryReporter
{
  MediaMemoryReporter();
  ~MediaMemoryReporter();
  static MediaMemoryReporter* sUniqueInstance;

  static MediaMemoryReporter* UniqueInstance() {
    if (!sUniqueInstance) {
      sUniqueInstance = new MediaMemoryReporter;
    }
    return sUniqueInstance;
  }

  typedef nsTArray<nsMediaDecoder*> DecodersArray;
  static DecodersArray& Decoders() {
    return UniqueInstance()->mDecoders;
  }

  DecodersArray mDecoders;

  nsCOMPtr<nsIMemoryReporter> mMediaDecodedVideoMemory;
  nsCOMPtr<nsIMemoryReporter> mMediaDecodedAudioMemory;

public:
  static void AddMediaDecoder(nsMediaDecoder* aDecoder) {
    Decoders().AppendElement(aDecoder);
  }

  static void RemoveMediaDecoder(nsMediaDecoder* aDecoder) {
    DecodersArray& decoders = Decoders();
    decoders.RemoveElement(aDecoder);
    if (decoders.IsEmpty()) {
      delete sUniqueInstance;
      sUniqueInstance = nsnull;
    }
  }

  static PRInt64 GetDecodedVideoMemory() {
    DecodersArray& decoders = Decoders();
    PRInt64 result = 0;
    for (size_t i = 0; i < decoders.Length(); ++i) {
      result += decoders[i]->VideoQueueMemoryInUse();
    }
    return result;
  }

  static PRInt64 GetDecodedAudioMemory() {
    DecodersArray& decoders = Decoders();
    PRInt64 result = 0;
    for (size_t i = 0; i < decoders.Length(); ++i) {
      result += decoders[i]->AudioQueueMemoryInUse();
    }
    return result;
  }
};

} //namespace mozilla
#endif
