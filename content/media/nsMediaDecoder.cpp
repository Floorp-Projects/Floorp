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

#include "nsMediaDecoder.h"
#include "nsMediaStream.h"

#include "prlog.h"
#include "prmem.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include "nsThreadUtils.h"
#include "nsIDOMHTMLMediaElement.h"
#include "nsNetUtil.h"
#include "nsHTMLMediaElement.h"
#include "gfxContext.h"
#include "nsPresContext.h"
#include "nsDOMError.h"
#include "nsDisplayList.h"
#include "nsSVGEffects.h"
#include "VideoUtils.h"

using namespace mozilla;

// Number of milliseconds between progress events as defined by spec
static const PRUint32 PROGRESS_MS = 350;

// Number of milliseconds of no data before a stall event is fired as defined by spec
static const PRUint32 STALL_MS = 3000;

// Number of estimated seconds worth of data we need to have buffered 
// ahead of the current playback position before we allow the media decoder
// to report that it can play through the entire media without the decode
// catching up with the download. Having this margin make the
// nsMediaDecoder::CanPlayThrough() calculation more stable in the case of
// fluctuating bitrates.
static const PRInt64 CAN_PLAY_THROUGH_MARGIN = 10;

nsMediaDecoder::nsMediaDecoder() :
  mElement(0),
  mRGBWidth(-1),
  mRGBHeight(-1),
  mVideoUpdateLock("nsMediaDecoder.mVideoUpdateLock"),
  mFrameBufferLength(0),
  mPinnedForSeek(PR_FALSE),
  mSizeChanged(PR_FALSE),
  mImageContainerSizeChanged(PR_FALSE),
  mShuttingDown(PR_FALSE)
{
  MOZ_COUNT_CTOR(nsMediaDecoder);
  MediaMemoryReporter::AddMediaDecoder(this);
}

nsMediaDecoder::~nsMediaDecoder()
{
  MOZ_COUNT_DTOR(nsMediaDecoder);
  MediaMemoryReporter::RemoveMediaDecoder(this);
}

PRBool nsMediaDecoder::Init(nsHTMLMediaElement* aElement)
{
  mElement = aElement;
  return PR_TRUE;
}

void nsMediaDecoder::Shutdown()
{
  StopProgress();
  mElement = nsnull;
}

nsHTMLMediaElement* nsMediaDecoder::GetMediaElement()
{
  return mElement;
}

nsresult nsMediaDecoder::RequestFrameBufferLength(PRUint32 aLength)
{
  if (aLength < FRAMEBUFFER_LENGTH_MIN || aLength > FRAMEBUFFER_LENGTH_MAX) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  mFrameBufferLength = aLength;
  return NS_OK;
}

void nsMediaDecoder::Invalidate()
{
  if (!mElement)
    return;

  nsIFrame* frame = mElement->GetPrimaryFrame();
  PRBool invalidateFrame = PR_FALSE;

  {
    MutexAutoLock lock(mVideoUpdateLock);

    // Get mImageContainerSizeChanged while holding the lock.
    invalidateFrame = mImageContainerSizeChanged;
    mImageContainerSizeChanged = PR_FALSE;

    if (mSizeChanged) {
      mElement->UpdateMediaSize(nsIntSize(mRGBWidth, mRGBHeight));
      mSizeChanged = PR_FALSE;

      if (frame) {
        nsPresContext* presContext = frame->PresContext();
        nsIPresShell *presShell = presContext->PresShell();
        presShell->FrameNeedsReflow(frame,
                                    nsIPresShell::eStyleChange,
                                    NS_FRAME_IS_DIRTY);
      }
    }
  }

  if (frame) {
    nsRect contentRect = frame->GetContentRect() - frame->GetPosition();
    if (invalidateFrame) {
      frame->Invalidate(contentRect);
    } else {
      frame->InvalidateLayer(contentRect, nsDisplayItem::TYPE_VIDEO);
    }
  }

  nsSVGEffects::InvalidateDirectRenderingObservers(mElement);
}

static void ProgressCallback(nsITimer* aTimer, void* aClosure)
{
  nsMediaDecoder* decoder = static_cast<nsMediaDecoder*>(aClosure);
  decoder->Progress(PR_TRUE);
}

void nsMediaDecoder::Progress(PRBool aTimer)
{
  if (!mElement)
    return;

  TimeStamp now = TimeStamp::Now();

  if (!aTimer) {
    mDataTime = now;
  }

  // If PROGRESS_MS has passed since the last progress event fired and more
  // data has arrived since then, fire another progress event.
  if ((mProgressTime.IsNull() ||
       now - mProgressTime >= TimeDuration::FromMilliseconds(PROGRESS_MS)) &&
      !mDataTime.IsNull() &&
      now - mDataTime <= TimeDuration::FromMilliseconds(PROGRESS_MS)) {
    mElement->DispatchAsyncEvent(NS_LITERAL_STRING("progress"));
    mProgressTime = now;
  }

  if (!mDataTime.IsNull() &&
      now - mDataTime >= TimeDuration::FromMilliseconds(STALL_MS)) {
    mElement->DownloadStalled();
    // Null it out
    mDataTime = TimeStamp();
  }
}

nsresult nsMediaDecoder::StartProgress()
{
  if (mProgressTimer)
    return NS_OK;

  mProgressTimer = do_CreateInstance("@mozilla.org/timer;1");
  return mProgressTimer->InitWithFuncCallback(ProgressCallback,
                                              this,
                                              PROGRESS_MS,
                                              nsITimer::TYPE_REPEATING_SLACK);
}

nsresult nsMediaDecoder::StopProgress()
{
  if (!mProgressTimer)
    return NS_OK;

  nsresult rv = mProgressTimer->Cancel();
  mProgressTimer = nsnull;

  return rv;
}

void nsMediaDecoder::FireTimeUpdate()
{
  if (!mElement)
    return;
  mElement->FireTimeUpdate(PR_TRUE);
}

void nsMediaDecoder::SetVideoData(const gfxIntSize& aSize,
                                  Image* aImage,
                                  TimeStamp aTarget)
{
  MutexAutoLock lock(mVideoUpdateLock);

  if (mRGBWidth != aSize.width || mRGBHeight != aSize.height) {
    mRGBWidth = aSize.width;
    mRGBHeight = aSize.height;
    mSizeChanged = PR_TRUE;
  }
  if (mImageContainer && aImage) {
    gfxIntSize oldFrameSize = mImageContainer->GetCurrentSize();

    TimeStamp paintTime = mImageContainer->GetPaintTime();
    if (!paintTime.IsNull() && !mPaintTarget.IsNull()) {
      mPaintDelay = paintTime - mPaintTarget;
    }

    mImageContainer->SetCurrentImage(aImage);
    gfxIntSize newFrameSize = mImageContainer->GetCurrentSize();
    if (oldFrameSize != newFrameSize) {
      mImageContainerSizeChanged = PR_TRUE;
    }
  }

  mPaintTarget = aTarget;
}

double nsMediaDecoder::GetFrameDelay()
{
  MutexAutoLock lock(mVideoUpdateLock);
  return mPaintDelay.ToSeconds();
}

void nsMediaDecoder::PinForSeek()
{
  nsMediaStream* stream = GetCurrentStream();
  if (!stream || mPinnedForSeek) {
    return;
  }
  mPinnedForSeek = PR_TRUE;
  stream->Pin();
}

void nsMediaDecoder::UnpinForSeek()
{
  nsMediaStream* stream = GetCurrentStream();
  if (!stream || !mPinnedForSeek) {
    return;
  }
  mPinnedForSeek = PR_FALSE;
  stream->Unpin();
}

PRBool nsMediaDecoder::CanPlayThrough()
{
  Statistics stats = GetStatistics();
  if (!stats.mDownloadRateReliable || !stats.mPlaybackRateReliable) {
    return PR_FALSE;
  }
  PRInt64 bytesToDownload = stats.mTotalBytes - stats.mDownloadPosition;
  PRInt64 bytesToPlayback = stats.mTotalBytes - stats.mPlaybackPosition;
  double timeToDownload = bytesToDownload / stats.mDownloadRate;
  double timeToPlay = bytesToPlayback / stats.mPlaybackRate;

  if (timeToDownload > timeToPlay) {
    // Estimated time to download is greater than the estimated time to play.
    // We probably can't play through without having to stop to buffer.
    return PR_FALSE;
  }

  // Estimated time to download is less than the estimated time to play.
  // We can probably play through without having to buffer, but ensure that
  // we've got a reasonable amount of data buffered after the current
  // playback position, so that if the bitrate of the media fluctuates, or if
  // our download rate or decode rate estimation is otherwise inaccurate,
  // we don't suddenly discover that we need to buffer. This is particularly
  // required near the start of the media, when not much data is downloaded.
  PRInt64 readAheadMargin =
    static_cast<PRInt64>(stats.mPlaybackRate * CAN_PLAY_THROUGH_MARGIN);
  return stats.mTotalBytes == stats.mDownloadPosition ||
         stats.mDownloadPosition > stats.mPlaybackPosition + readAheadMargin;
}

namespace mozilla {

MediaMemoryReporter* MediaMemoryReporter::sUniqueInstance;

NS_MEMORY_REPORTER_IMPLEMENT(MediaDecodedVideoMemory,
                             "explicit/media/decoded-video",
                             KIND_HEAP,
                             UNITS_BYTES,
                             MediaMemoryReporter::GetDecodedVideoMemory,
                             "Memory used by decoded video frames.")

NS_MEMORY_REPORTER_IMPLEMENT(MediaDecodedAudioMemory,
                             "explicit/media/decoded-audio",
                             KIND_HEAP,
                             UNITS_BYTES,
                             MediaMemoryReporter::GetDecodedAudioMemory,
                             "Memory used by decoded audio chunks.")

MediaMemoryReporter::MediaMemoryReporter()
  : mMediaDecodedVideoMemory(new NS_MEMORY_REPORTER_NAME(MediaDecodedVideoMemory))
  , mMediaDecodedAudioMemory(new NS_MEMORY_REPORTER_NAME(MediaDecodedAudioMemory))
{
  NS_RegisterMemoryReporter(mMediaDecodedVideoMemory);
  NS_RegisterMemoryReporter(mMediaDecodedAudioMemory);
}

MediaMemoryReporter::~MediaMemoryReporter()
{
  NS_UnregisterMemoryReporter(mMediaDecodedVideoMemory);
  NS_UnregisterMemoryReporter(mMediaDecodedAudioMemory);
}

} // namespace mozilla
