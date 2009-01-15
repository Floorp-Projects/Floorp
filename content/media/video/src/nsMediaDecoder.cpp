/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: ML 1.1/GPL 2.0/LGPL 2.1
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
#include "prlog.h"
#include "prmem.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include "nsThreadUtils.h"
#include "nsIDOMHTMLMediaElement.h"
#include "nsNetUtil.h"
#include "nsHTMLMediaElement.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsAutoLock.h"
#include "nsIRenderingContext.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "nsPresContext.h"
#include "nsMediaDecoder.h"

// Number of milliseconds between progress events as defined by spec
#define PROGRESS_MS 350

// Number of milliseconds of no data before a stall event is fired as defined by spec
#define STALL_MS 3000

#ifdef PR_LOGGING
// Logging object for decoder
PRLogModuleInfo* gVideoDecoderLog = nsnull;
#endif

nsMediaDecoder::nsMediaDecoder() :
  mElement(0),
  mRGBWidth(-1),
  mRGBHeight(-1),
  mProgressTime(0),
  mDataTime(0),
  mSizeChanged(PR_FALSE),
  mVideoUpdateLock(nsnull),
  mFramerate(0.0),
  mShuttingDown(PR_FALSE),
  mStopping(PR_FALSE)
{
  MOZ_COUNT_CTOR(nsMediaDecoder);
}

nsMediaDecoder::~nsMediaDecoder()
{
  if (mVideoUpdateLock) {
    PR_DestroyLock(mVideoUpdateLock);
    mVideoUpdateLock = nsnull;
  }
  MOZ_COUNT_DTOR(nsMediaDecoder);
}

PRBool nsMediaDecoder::Init()
{
  mVideoUpdateLock = PR_NewLock();

  return mVideoUpdateLock != nsnull;
}

void nsMediaDecoder::Shutdown()
{
  StopProgress();
  ElementUnavailable();
}


nsresult nsMediaDecoder::InitLogger() 
{
#ifdef PR_LOGGING
  gVideoDecoderLog = PR_NewLogModule("nsMediaDecoder");
#endif
  return NS_OK;
}

void nsMediaDecoder::Invalidate()
{
  if (!mElement)
    return;

  nsIFrame* frame = mElement->GetPrimaryFrame();
  if (!frame)
    return;
  
  {
    nsAutoLock lock(mVideoUpdateLock);
    if (mSizeChanged) {
      mElement->UpdateMediaSize(nsIntSize(mRGBWidth, mRGBHeight));
      mSizeChanged = PR_FALSE;
      nsPresContext* presContext = frame->PresContext();      
      nsIPresShell *presShell = presContext->PresShell();
      presShell->FrameNeedsReflow(frame, 
                                  nsIPresShell::eStyleChange,
                                  NS_FRAME_IS_DIRTY);
    }
  }
  nsRect r(nsPoint(0,0), frame->GetSize());
  frame->Invalidate(r);
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

  PRIntervalTime now = PR_IntervalNow();
  if (mProgressTime == 0 ||
      PR_IntervalToMilliseconds(PR_IntervalNow() - mProgressTime) >= PROGRESS_MS) {
    mElement->DispatchAsyncProgressEvent(NS_LITERAL_STRING("progress"));
    mProgressTime = now;
  }

  // The test for aTimer is to ensure that we dispatch 'stalled'
  // only when we are not receiving data.
  if (aTimer &&
      mDataTime != 0 &&
      PR_IntervalToMilliseconds(now - mDataTime) >= STALL_MS) {
    mElement->DispatchAsyncProgressEvent(NS_LITERAL_STRING("stalled"));
    mDataTime = 0;
  }

  if (!aTimer) {
    mDataTime = now;
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

void nsMediaDecoder::SetRGBData(PRInt32 aWidth, PRInt32 aHeight, float aFramerate, unsigned char* aRGBBuffer)
{
  if (mRGBWidth != aWidth || mRGBHeight != aHeight) {
    mRGBWidth = aWidth;
    mRGBHeight = aHeight;
    mSizeChanged = PR_TRUE;
    // Delete buffer so we'll reallocate it
    mRGB = nsnull;
  }
  mFramerate = aFramerate;

  if (!mRGB) 
    mRGB = new unsigned char[aWidth * aHeight * 4];
  if (mRGB && aRGBBuffer) {
    memcpy(mRGB.get(), aRGBBuffer, aWidth*aHeight*4);
  }
}

void nsMediaDecoder::Paint(gfxContext* aContext, const gfxRect& aRect)
{
  nsAutoLock lock(mVideoUpdateLock);

  if (!mRGB)
    return;

  /* Create a surface backed by the RGB */
  nsRefPtr<gfxASurface> surface = 
    new gfxImageSurface(mRGB,
                        gfxIntSize(mRGBWidth, mRGBHeight), 
                        mRGBWidth * 4,
                        gfxASurface::ImageFormatARGB32);    

  if (!surface)
    return;

  nsRefPtr<gfxPattern> pat = new gfxPattern(surface);
  if (!pat)
    return;

  // Make the source image fill the rectangle completely
  pat->SetMatrix(gfxMatrix().Scale(mRGBWidth/aRect.Width(), mRGBHeight/aRect.Height()));
  // Set PAD mode so that when the video is being scaled, we do not sample
  // outside the bounds of the video image.
  pat->SetExtend(gfxPattern::EXTEND_PAD);

  /* Draw RGB surface onto frame */
  aContext->NewPath();
  aContext->PixelSnappedRectangleAndSetPattern(aRect, pat);
  aContext->Fill();

#ifdef DEBUG_FRAME_RATE
  {
    // Output frame rate
    static float last = double(PR_IntervalToMilliseconds(PR_IntervalNow()))/1000.0;
    float now = double(PR_IntervalToMilliseconds(PR_IntervalNow()))/1000.0;
    static int count = 0;
    count++;
    if (now-last > 10.0) {
      LOG(PR_LOG_DEBUG, ("Paint Frame Rate = %f (should be %f)\n", (float)count / (float)(now-last), mFramerate));
      count = 0;
      last = double(PR_IntervalToMilliseconds(PR_IntervalNow()))/1000.0;
    }
  }   
#endif
}

void nsMediaDecoder::ElementAvailable(nsHTMLMediaElement* anElement)
{
  mElement = anElement;
}

void nsMediaDecoder::ElementUnavailable()
{
  mElement = nsnull;
}

