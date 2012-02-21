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
 *  Chris Pearce <chris@pearce.org.nz>
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

#include <limits>
#include "nsNetUtil.h"
#include "nsAudioStream.h"
#include "nsHTMLVideoElement.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsTArray.h"
#include "VideoUtils.h"
#include "nsBuiltinDecoder.h"
#include "nsBuiltinDecoderStateMachine.h"
#include "nsTimeRanges.h"
#include "nsContentUtils.h"

using namespace mozilla;

#ifdef PR_LOGGING
PRLogModuleInfo* gBuiltinDecoderLog;
#define LOG(type, msg) PR_LOG(gBuiltinDecoderLog, type, msg)
#else
#define LOG(type, msg)
#endif

NS_IMPL_THREADSAFE_ISUPPORTS1(nsBuiltinDecoder, nsIObserver)

void nsBuiltinDecoder::Pause() 
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  if (mPlayState == PLAY_STATE_SEEKING || mPlayState == PLAY_STATE_ENDED) {
    mNextState = PLAY_STATE_PAUSED;
    return;
  }

  ChangeState(PLAY_STATE_PAUSED);
}

void nsBuiltinDecoder::SetVolume(double aVolume)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  mInitialVolume = aVolume;
  if (mDecoderStateMachine) {
    mDecoderStateMachine->SetVolume(aVolume);
  }
}

double nsBuiltinDecoder::GetDuration()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  if (mInfiniteStream) {
    return std::numeric_limits<double>::infinity();
  }
  if (mDuration >= 0) {
     return static_cast<double>(mDuration) / static_cast<double>(USECS_PER_S);
  }
  return std::numeric_limits<double>::quiet_NaN();
}

void nsBuiltinDecoder::SetInfinite(bool aInfinite)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  mInfiniteStream = aInfinite;
}

bool nsBuiltinDecoder::IsInfinite()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  return mInfiniteStream;
}

nsBuiltinDecoder::nsBuiltinDecoder() :
  mDecoderPosition(0),
  mPlaybackPosition(0),
  mCurrentTime(0.0),
  mInitialVolume(0.0),
  mRequestedSeekTime(-1.0),
  mDuration(-1),
  mSeekable(true),
  mReentrantMonitor("media.decoder"),
  mPlayState(PLAY_STATE_PAUSED),
  mNextState(PLAY_STATE_PAUSED),
  mResourceLoaded(false),
  mIgnoreProgressData(false),
  mInfiniteStream(false)
{
  MOZ_COUNT_CTOR(nsBuiltinDecoder);
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
#ifdef PR_LOGGING
  if (!gBuiltinDecoderLog) {
    gBuiltinDecoderLog = PR_NewLogModule("nsBuiltinDecoder");
  }
#endif
}

bool nsBuiltinDecoder::Init(nsHTMLMediaElement* aElement)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  if (!nsMediaDecoder::Init(aElement))
    return false;

  nsContentUtils::RegisterShutdownObserver(this);
  return true;
}

void nsBuiltinDecoder::Shutdown()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  
  if (mShuttingDown)
    return;

  mShuttingDown = true;

  // This changes the decoder state to SHUTDOWN and does other things
  // necessary to unblock the state machine thread if it's blocked, so
  // the asynchronous shutdown in nsDestroyStateMachine won't deadlock.
  if (mDecoderStateMachine) {
    mDecoderStateMachine->Shutdown();
  }

  // Force any outstanding seek and byterange requests to complete
  // to prevent shutdown from deadlocking.
  if (mResource) {
    mResource->Close();
  }

  ChangeState(PLAY_STATE_SHUTDOWN);
  nsMediaDecoder::Shutdown();

  nsContentUtils::UnregisterShutdownObserver(this);
}

nsBuiltinDecoder::~nsBuiltinDecoder()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  UnpinForSeek();
  MOZ_COUNT_DTOR(nsBuiltinDecoder);
}

nsresult nsBuiltinDecoder::Load(MediaResource* aResource,
                                nsIStreamListener** aStreamListener,
                                nsMediaDecoder* aCloneDonor)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  if (aStreamListener) {
    *aStreamListener = nsnull;
  }

  {
    // Hold the lock while we do this to set proper lock ordering
    // expectations for dynamic deadlock detectors: decoder lock(s)
    // should be grabbed before the cache lock
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    nsresult rv = aResource->Open(aStreamListener);
    if (NS_FAILED(rv)) {
      LOG(PR_LOG_DEBUG, ("%p Failed to open stream!", this));
      delete aResource;
      return rv;
    }

    mResource = aResource;
  }

  mDecoderStateMachine = CreateStateMachine();
  if (!mDecoderStateMachine) {
    LOG(PR_LOG_DEBUG, ("%p Failed to create state machine!", this));
    return NS_ERROR_FAILURE;
  }

  nsBuiltinDecoder* cloneDonor = static_cast<nsBuiltinDecoder*>(aCloneDonor);
  if (NS_FAILED(mDecoderStateMachine->Init(cloneDonor ?
                                           cloneDonor->mDecoderStateMachine : nsnull))) {
    LOG(PR_LOG_DEBUG, ("%p Failed to init state machine!", this));
    return NS_ERROR_FAILURE;
  }
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    mDecoderStateMachine->SetSeekable(mSeekable);
    mDecoderStateMachine->SetDuration(mDuration);
    mDecoderStateMachine->SetVolume(mInitialVolume);
    
    if (mFrameBufferLength > 0) {
      // The valid mFrameBufferLength value was specified earlier
      mDecoderStateMachine->SetFrameBufferLength(mFrameBufferLength);
    }
  }

  ChangeState(PLAY_STATE_LOADING);

  return ScheduleStateMachineThread();
}

nsresult nsBuiltinDecoder::RequestFrameBufferLength(PRUint32 aLength)
{
  nsresult res = nsMediaDecoder::RequestFrameBufferLength(aLength);
  NS_ENSURE_SUCCESS(res,res);

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  if (mDecoderStateMachine) {
      mDecoderStateMachine->SetFrameBufferLength(aLength);
  }
  return res;
}

nsresult nsBuiltinDecoder::ScheduleStateMachineThread()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  NS_ASSERTION(mDecoderStateMachine,
               "Must have state machine to start state machine thread");
  NS_ENSURE_STATE(mDecoderStateMachine);

  if (mShuttingDown)
    return NS_OK;
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  nsBuiltinDecoderStateMachine* m =
    static_cast<nsBuiltinDecoderStateMachine*>(mDecoderStateMachine.get());
  return m->ScheduleStateMachine();
}

nsresult nsBuiltinDecoder::Play()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  NS_ASSERTION(mDecoderStateMachine != nsnull, "Should have state machine.");
  nsresult res = ScheduleStateMachineThread();
  NS_ENSURE_SUCCESS(res,res);
  if (mPlayState == PLAY_STATE_SEEKING) {
    mNextState = PLAY_STATE_PLAYING;
    return NS_OK;
  }
  if (mPlayState == PLAY_STATE_ENDED)
    return Seek(0);

  ChangeState(PLAY_STATE_PLAYING);
  return NS_OK;
}

/**
 * Returns true if aValue is inside a range of aRanges, and put the range
 * index in aIntervalIndex if it is not null.
 * If aValue is not inside a range, false is returned, and aIntervalIndex, if
 * not null, is set to the index of the range which ends immediately before aValue
 * (and can be -1 if aValue is before aRanges.Start(0)).
 */
static bool IsInRanges(nsTimeRanges& aRanges, double aValue, PRInt32& aIntervalIndex) {
  PRUint32 length;
  aRanges.GetLength(&length);
  for (PRUint32 i = 0; i < length; i++) {
    double start, end;
    aRanges.Start(i, &start);
    if (start > aValue) {
      aIntervalIndex = i - 1;
      return false;
    }
    aRanges.End(i, &end);
    if (aValue <= end) {
      aIntervalIndex = i;
      return true;
    }
  }
  aIntervalIndex = length - 1;
  return false;
}

nsresult nsBuiltinDecoder::Seek(double aTime)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  NS_ABORT_IF_FALSE(aTime >= 0.0, "Cannot seek to a negative value.");

  nsTimeRanges seekable;
  nsresult res;
  PRUint32 length = 0;
  res = GetSeekable(&seekable);
  NS_ENSURE_SUCCESS(res, NS_OK);

  seekable.GetLength(&length);
  if (!length) {
    return NS_OK;
  }

  // If the position we want to seek to is not in a seekable range, we seek
  // to the closest position in the seekable ranges instead. If two positions
  // are equally close, we seek to the closest position from the currentTime.
  // See seeking spec, point 7 :
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-video-element.html#seeking
  PRInt32 range = 0;
  if (!IsInRanges(seekable, aTime, range)) {
    if (range != -1) {
      // |range + 1| can't be negative, because the only possible negative value
      // for |range| is -1.
      if (PRUint32(range + 1) < length) {
        double leftBound, rightBound;
        res = seekable.End(range, &leftBound);
        NS_ENSURE_SUCCESS(res, NS_OK);
        res = seekable.Start(range + 1, &rightBound);
        NS_ENSURE_SUCCESS(res, NS_OK);
        double distanceLeft = NS_ABS(leftBound - aTime);
        double distanceRight = NS_ABS(rightBound - aTime);
        if (distanceLeft == distanceRight) {
          distanceLeft = NS_ABS(leftBound - mCurrentTime);
          distanceRight = NS_ABS(rightBound - mCurrentTime);
        } 
        aTime = (distanceLeft < distanceRight) ? leftBound : rightBound;
      } else {
        // Seek target is after the end last range in seekable data.
        // Clamp the seek target to the end of the last seekable range.
        res = seekable.End(length - 1, &aTime);
        NS_ENSURE_SUCCESS(res, NS_OK);
      }
    } else {
      // aTime is before the first range in |seekable|, the closest point we can
      // seek to is the start of the first range.
      seekable.Start(0, &aTime);
    }
  }

  mRequestedSeekTime = aTime;
  mCurrentTime = aTime;

  // If we are already in the seeking state, then setting mRequestedSeekTime
  // above will result in the new seek occurring when the current seek
  // completes.
  if (mPlayState != PLAY_STATE_SEEKING) {
    bool paused = false;
    if (mElement) {
      mElement->GetPaused(&paused);
    }
    mNextState = paused ? PLAY_STATE_PAUSED : PLAY_STATE_PLAYING;
    PinForSeek();
    ChangeState(PLAY_STATE_SEEKING);
  }

  return ScheduleStateMachineThread();
}

nsresult nsBuiltinDecoder::PlaybackRateChanged()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

double nsBuiltinDecoder::GetCurrentTime()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  return mCurrentTime;
}

already_AddRefed<nsIPrincipal> nsBuiltinDecoder::GetCurrentPrincipal()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  return mResource ? mResource->GetCurrentPrincipal() : nsnull;
}

void nsBuiltinDecoder::AudioAvailable(float* aFrameBuffer,
                                      PRUint32 aFrameBufferLength,
                                      float aTime)
{
  // Auto manage the frame buffer's memory. If we return due to an error
  // here, this ensures we free the memory. Otherwise, we pass off ownership
  // to HTMLMediaElement::NotifyAudioAvailable().
  nsAutoArrayPtr<float> frameBuffer(aFrameBuffer);
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  if (mShuttingDown || !mElement) {
    return;
  }
  mElement->NotifyAudioAvailable(frameBuffer.forget(), aFrameBufferLength, aTime);
}

void nsBuiltinDecoder::MetadataLoaded(PRUint32 aChannels,
                                      PRUint32 aRate)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  if (mShuttingDown) {
    return;
  }

  // Only inform the element of MetadataLoaded if not doing a load() in order
  // to fulfill a seek, otherwise we'll get multiple metadataloaded events.
  bool notifyElement = true;
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    mDuration = mDecoderStateMachine ? mDecoderStateMachine->GetDuration() : -1;
    // Duration has changed so we should recompute playback rate
    UpdatePlaybackRate();

    notifyElement = mNextState != PLAY_STATE_SEEKING;
  }

  if (mDuration == -1) {
    SetInfinite(true);
  }

  if (mElement && notifyElement) {
    // Make sure the element and the frame (if any) are told about
    // our new size.
    Invalidate();
    mElement->MetadataLoaded(aChannels, aRate);
  }

  if (!mResourceLoaded) {
    StartProgress();
  } else if (mElement) {
    // Resource was loaded during metadata loading, when progress
    // events are being ignored. Fire the final progress event.
    mElement->DispatchAsyncEvent(NS_LITERAL_STRING("progress"));
  }

  // Only inform the element of FirstFrameLoaded if not doing a load() in order
  // to fulfill a seek, otherwise we'll get multiple loadedfirstframe events.
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  bool resourceIsLoaded = !mResourceLoaded && mResource &&
    mResource->IsDataCachedToEndOfResource(mDecoderPosition);
  if (mElement && notifyElement) {
    mElement->FirstFrameLoaded(resourceIsLoaded);
  }

  // This can run cache callbacks.
  mResource->EnsureCacheUpToDate();

  // The element can run javascript via events
  // before reaching here, so only change the
  // state if we're still set to the original
  // loading state.
  if (mPlayState == PLAY_STATE_LOADING) {
    if (mRequestedSeekTime >= 0.0) {
      ChangeState(PLAY_STATE_SEEKING);
    }
    else {
      ChangeState(mNextState);
    }
  }

  if (resourceIsLoaded) {
    ResourceLoaded();
  }

  // Run NotifySuspendedStatusChanged now to give us a chance to notice
  // that autoplay should run.
  NotifySuspendedStatusChanged();
}

void nsBuiltinDecoder::ResourceLoaded()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  // Don't handle ResourceLoaded if we are shutting down, or if
  // we need to ignore progress data due to seeking (in the case
  // that the seek results in reaching end of file, we get a bogus call
  // to ResourceLoaded).
  if (mShuttingDown)
    return;

  {
    // If we are seeking or loading then the resource loaded notification we get
    // should be ignored, since it represents the end of the seek request.
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    if (mIgnoreProgressData || mResourceLoaded || mPlayState == PLAY_STATE_LOADING)
      return;

    Progress(false);

    mResourceLoaded = true;
    StopProgress();
  }

  // Ensure the final progress event gets fired
  if (mElement) {
    mElement->ResourceLoaded();
  }
}

void nsBuiltinDecoder::NetworkError()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  if (mShuttingDown)
    return;

  if (mElement)
    mElement->NetworkError();

  Shutdown();
}

void nsBuiltinDecoder::DecodeError()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  if (mShuttingDown)
    return;

  if (mElement)
    mElement->DecodeError();

  Shutdown();
}

bool nsBuiltinDecoder::IsSeeking() const
{
  return mPlayState == PLAY_STATE_SEEKING || mNextState == PLAY_STATE_SEEKING;
}

bool nsBuiltinDecoder::IsEnded() const
{
  return mPlayState == PLAY_STATE_ENDED || mPlayState == PLAY_STATE_SHUTDOWN;
}

void nsBuiltinDecoder::PlaybackEnded()
{
  if (mShuttingDown || mPlayState == nsBuiltinDecoder::PLAY_STATE_SEEKING)
    return;

  PlaybackPositionChanged();
  ChangeState(PLAY_STATE_ENDED);

  if (mElement)  {
    UpdateReadyStateForData();
    mElement->PlaybackEnded();
  }

  // This must be called after |mElement->PlaybackEnded()| call above, in order
  // to fire the required durationchange.
  if (IsInfinite()) {
    SetInfinite(false);
  }
}

NS_IMETHODIMP nsBuiltinDecoder::Observe(nsISupports *aSubjet,
                                        const char *aTopic,
                                        const PRUnichar *someData)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    Shutdown();
  }

  return NS_OK;
}

nsMediaDecoder::Statistics
nsBuiltinDecoder::GetStatistics()
{
  NS_ASSERTION(NS_IsMainThread() || OnStateMachineThread(),
               "Should be on main or state machine thread.");
  Statistics result;

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  if (mResource) {
    result.mDownloadRate = 
      mResource->GetDownloadRate(&result.mDownloadRateReliable);
    result.mDownloadPosition =
      mResource->GetCachedDataEnd(mDecoderPosition);
    result.mTotalBytes = mResource->GetLength();
    result.mPlaybackRate = ComputePlaybackRate(&result.mPlaybackRateReliable);
    result.mDecoderPosition = mDecoderPosition;
    result.mPlaybackPosition = mPlaybackPosition;
  }
  else {
    result.mDownloadRate = 0;
    result.mDownloadRateReliable = true;
    result.mPlaybackRate = 0;
    result.mPlaybackRateReliable = true;
    result.mDecoderPosition = 0;
    result.mPlaybackPosition = 0;
    result.mDownloadPosition = 0;
    result.mTotalBytes = 0;
  }

  return result;
}

double nsBuiltinDecoder::ComputePlaybackRate(bool* aReliable)
{
  GetReentrantMonitor().AssertCurrentThreadIn();
  NS_ASSERTION(NS_IsMainThread() || OnStateMachineThread(),
               "Should be on main or state machine thread.");

  PRInt64 length = mResource ? mResource->GetLength() : -1;
  if (mDuration >= 0 && length >= 0) {
    *aReliable = true;
    return length * static_cast<double>(USECS_PER_S) / mDuration;
  }
  return mPlaybackStatistics.GetRateAtLastStop(aReliable);
}

void nsBuiltinDecoder::UpdatePlaybackRate()
{
  NS_ASSERTION(NS_IsMainThread() || OnStateMachineThread(),
               "Should be on main or state machine thread.");
  GetReentrantMonitor().AssertCurrentThreadIn();
  if (!mResource)
    return;
  bool reliable;
  PRUint32 rate = PRUint32(ComputePlaybackRate(&reliable));
  if (reliable) {
    // Avoid passing a zero rate
    rate = NS_MAX(rate, 1u);
  }
  else {
    // Set a minimum rate of 10,000 bytes per second ... sometimes we just
    // don't have good data
    rate = NS_MAX(rate, 10000u);
  }
  mResource->SetPlaybackRate(rate);
}

void nsBuiltinDecoder::NotifySuspendedStatusChanged()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  if (!mResource)
    return;
  MediaResource* activeStream;
  bool suspended = mResource->IsSuspendedByCache(&activeStream);
  
  if (suspended && mElement) {
    // if this is an autoplay element, we need to kick off its autoplaying
    // now so we consume data and hopefully free up cache space
    mElement->NotifyAutoplayDataReady();
  }
}

void nsBuiltinDecoder::NotifyBytesDownloaded()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  UpdateReadyStateForData();
  Progress(false);
}

void nsBuiltinDecoder::NotifyDownloadEnded(nsresult aStatus)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  if (aStatus == NS_BINDING_ABORTED) {
    // Download has been cancelled by user.
    if (mElement) {
      mElement->LoadAborted();
    }
    return;
  }

  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    UpdatePlaybackRate();
  }

  if (NS_SUCCEEDED(aStatus)) {
    ResourceLoaded();
  }
  else if (aStatus != NS_BASE_STREAM_CLOSED) {
    NetworkError();
  }
  UpdateReadyStateForData();
}

void nsBuiltinDecoder::NotifyBytesConsumed(PRInt64 aBytes)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  NS_ASSERTION(OnStateMachineThread() || mDecoderStateMachine->OnDecodeThread(),
               "Should be on play state machine or decode thread.");
  if (!mIgnoreProgressData) {
    mDecoderPosition += aBytes;
    mPlaybackStatistics.AddBytes(aBytes);
  }
}

void nsBuiltinDecoder::NextFrameUnavailableBuffering()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be called on main thread");
  if (!mElement || mShuttingDown || !mDecoderStateMachine)
    return;

  mElement->UpdateReadyStateForData(nsHTMLMediaElement::NEXT_FRAME_UNAVAILABLE_BUFFERING);
}

void nsBuiltinDecoder::NextFrameAvailable()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be called on main thread");
  if (!mElement || mShuttingDown || !mDecoderStateMachine)
    return;

  mElement->UpdateReadyStateForData(nsHTMLMediaElement::NEXT_FRAME_AVAILABLE);
}

void nsBuiltinDecoder::NextFrameUnavailable()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be called on main thread");
  if (!mElement || mShuttingDown || !mDecoderStateMachine)
    return;
  mElement->UpdateReadyStateForData(nsHTMLMediaElement::NEXT_FRAME_UNAVAILABLE);
}

void nsBuiltinDecoder::UpdateReadyStateForData()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be called on main thread");
  if (!mElement || mShuttingDown || !mDecoderStateMachine)
    return;
  nsHTMLMediaElement::NextFrameStatus frameStatus =
    mDecoderStateMachine->GetNextFrameStatus();
  mElement->UpdateReadyStateForData(frameStatus);
}

void nsBuiltinDecoder::SeekingStopped()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  if (mShuttingDown)
    return;

  bool seekWasAborted = false;
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    // An additional seek was requested while the current seek was
    // in operation.
    if (mRequestedSeekTime >= 0.0) {
      ChangeState(PLAY_STATE_SEEKING);
      seekWasAborted = true;
    } else {
      UnpinForSeek();
      ChangeState(mNextState);
    }
  }

  if (mElement) {
    UpdateReadyStateForData();
    if (!seekWasAborted) {
      mElement->SeekCompleted();
    }
  }
}

// This is called when seeking stopped *and* we're at the end of the
// media.
void nsBuiltinDecoder::SeekingStoppedAtEnd()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  if (mShuttingDown)
    return;

  bool fireEnded = false;
  bool seekWasAborted = false;
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    // An additional seek was requested while the current seek was
    // in operation.
    if (mRequestedSeekTime >= 0.0) {
      ChangeState(PLAY_STATE_SEEKING);
      seekWasAborted = true;
    } else {
      UnpinForSeek();
      fireEnded = true;
      ChangeState(PLAY_STATE_ENDED);
    }
  }

  if (mElement) {
    UpdateReadyStateForData();
    if (!seekWasAborted) {
      mElement->SeekCompleted();
      if (fireEnded) {
        mElement->PlaybackEnded();
      }
    }
  }
}

void nsBuiltinDecoder::SeekingStarted()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  if (mShuttingDown)
    return;

  if (mElement) {
    UpdateReadyStateForData();
    mElement->SeekStarted();
  }
}

void nsBuiltinDecoder::ChangeState(PlayState aState)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");   
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (mNextState == aState) {
    mNextState = PLAY_STATE_PAUSED;
  }

  if (mPlayState == PLAY_STATE_SHUTDOWN) {
    mReentrantMonitor.NotifyAll();
    return;
  }

  mPlayState = aState;
  if (mDecoderStateMachine) {
    switch (aState) {
    case PLAY_STATE_PLAYING:
      mDecoderStateMachine->Play();
      break;
    case PLAY_STATE_SEEKING:
      mDecoderStateMachine->Seek(mRequestedSeekTime);
      mRequestedSeekTime = -1.0;
      break;
    default:
      /* No action needed */
      break;
    }
  }
  mReentrantMonitor.NotifyAll();
}

void nsBuiltinDecoder::PlaybackPositionChanged()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  if (mShuttingDown)
    return;

  double lastTime = mCurrentTime;

  // Control the scope of the monitor so it is not
  // held while the timeupdate and the invalidate is run.
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    if (mDecoderStateMachine) {
      if (!IsSeeking()) {
        // Only update the current playback position if we're not seeking.
        // If we are seeking, the update could have been scheduled on the
        // state machine thread while we were playing but after the seek
        // algorithm set the current playback position on the main thread,
        // and we don't want to override the seek algorithm and change the
        // current time after the seek has started but before it has
        // completed.
        mCurrentTime = mDecoderStateMachine->GetCurrentTime();
      }
      mDecoderStateMachine->ClearPositionChangeFlag();
    }
  }

  // Invalidate the frame so any video data is displayed.
  // Do this before the timeupdate event so that if that
  // event runs JavaScript that queries the media size, the
  // frame has reflowed and the size updated beforehand.
  Invalidate();

  if (mElement && lastTime != mCurrentTime) {
    FireTimeUpdate();
  }
}

void nsBuiltinDecoder::DurationChanged()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  PRInt64 oldDuration = mDuration;
  mDuration = mDecoderStateMachine ? mDecoderStateMachine->GetDuration() : -1;
  // Duration has changed so we should recompute playback rate
  UpdatePlaybackRate();

  if (mElement && oldDuration != mDuration && !IsInfinite()) {
    LOG(PR_LOG_DEBUG, ("%p duration changed to %lld", this, mDuration));
    mElement->DispatchEvent(NS_LITERAL_STRING("durationchange"));
  }
}

void nsBuiltinDecoder::SetDuration(double aDuration)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  mDuration = static_cast<PRInt64>(NS_round(aDuration * static_cast<double>(USECS_PER_S)));

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  if (mDecoderStateMachine) {
    mDecoderStateMachine->SetDuration(mDuration);
  }

  // Duration has changed so we should recompute playback rate
  UpdatePlaybackRate();
}

void nsBuiltinDecoder::SetSeekable(bool aSeekable)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  mSeekable = aSeekable;
  if (mDecoderStateMachine) {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    mDecoderStateMachine->SetSeekable(aSeekable);
  }
}

bool nsBuiltinDecoder::IsSeekable()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  return mSeekable;
}

nsresult nsBuiltinDecoder::GetSeekable(nsTimeRanges* aSeekable)
{
  //TODO : change 0.0 to GetInitialTime() when available
  double initialTime = 0.0;

  if (IsSeekable()) {
    double end = IsInfinite() ? std::numeric_limits<double>::infinity()
                              : initialTime + GetDuration();
    aSeekable->Add(initialTime, end);
    return NS_OK;
  }

  return GetBuffered(aSeekable);
}

void nsBuiltinDecoder::SetEndTime(double aTime)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  if (mDecoderStateMachine) {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    mDecoderStateMachine->SetFragmentEndTime(static_cast<PRInt64>(aTime * USECS_PER_S));
  }
}

void nsBuiltinDecoder::Suspend()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  if (mResource) {
    mResource->Suspend(true);
  }
}

void nsBuiltinDecoder::Resume(bool aForceBuffering)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  if (mResource) {
    mResource->Resume();
  }
  if (aForceBuffering) {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    if (mDecoderStateMachine) {
      mDecoderStateMachine->StartBuffering();
    }
  }
}

void nsBuiltinDecoder::StopProgressUpdates()
{
  NS_ASSERTION(OnStateMachineThread() || OnDecodeThread(),
               "Should be on state machine or decode thread.");
  GetReentrantMonitor().AssertCurrentThreadIn();
  mIgnoreProgressData = true;
  if (mResource) {
    mResource->SetReadMode(nsMediaCacheStream::MODE_METADATA);
  }
}

void nsBuiltinDecoder::StartProgressUpdates()
{
  NS_ASSERTION(OnStateMachineThread() || OnDecodeThread(),
               "Should be on state machine or decode thread.");
  GetReentrantMonitor().AssertCurrentThreadIn();
  mIgnoreProgressData = false;
  if (mResource) {
    mResource->SetReadMode(nsMediaCacheStream::MODE_PLAYBACK);
    mDecoderPosition = mPlaybackPosition = mResource->Tell();
  }
}

void nsBuiltinDecoder::MoveLoadsToBackground()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  if (mResource) {
    mResource->MoveLoadsToBackground();
  }
}

void nsBuiltinDecoder::UpdatePlaybackOffset(PRInt64 aOffset)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  mPlaybackPosition = NS_MAX(aOffset, mPlaybackPosition);
}

bool nsBuiltinDecoder::OnStateMachineThread() const
{
  return IsCurrentThread(nsBuiltinDecoderStateMachine::GetStateMachineThread());
}

void nsBuiltinDecoder::NotifyAudioAvailableListener()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  if (mDecoderStateMachine) {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    mDecoderStateMachine->NotifyAudioAvailableListener();
  }
}
