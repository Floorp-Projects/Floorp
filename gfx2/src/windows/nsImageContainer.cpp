/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nsImageContainer.h"
#include "nsImageFrame.h"

#include "nsCOMPtr.h"

NS_IMPL_ISUPPORTS3(nsImageContainer, gfxIImageContainer, nsPIImageContainerWin, nsITimerCallback)

nsImageContainer::nsImageContainer()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  mCurrentFrame = 0;
  mCurrentFrameIsFinishedDecoding = PR_FALSE;
  mDoneDecoding = PR_FALSE;
}

nsImageContainer::~nsImageContainer()
{
  /* destructor code */
  mFrames.Clear();

  if (mTimer)
    mTimer->Cancel();
}



/* void init (in nscoord aWidth, in nscoord aHeight, in gfxIImageContainerObserver aObserver); */
NS_IMETHODIMP nsImageContainer::Init(nscoord aWidth, nscoord aHeight, gfxIImageContainerObserver *aObserver)
{
  if (aWidth <= 0 || aHeight <= 0) {
    printf("error - negative image size\n");
    return NS_ERROR_FAILURE;
  }

  mSize.SizeTo(aWidth, aHeight);

  mObserver = aObserver;

  return NS_OK;
}

/* readonly attribute gfx_format preferredAlphaChannelFormat; */
NS_IMETHODIMP nsImageContainer::GetPreferredAlphaChannelFormat(gfx_format *aFormat)
{
  *aFormat = gfxIFormats::RGB_A8;
  return NS_OK;
}

/* readonly attribute nscoord width; */
NS_IMETHODIMP nsImageContainer::GetWidth(nscoord *aWidth)
{
  *aWidth = mSize.width;
  return NS_OK;
}

/* readonly attribute nscoord height; */
NS_IMETHODIMP nsImageContainer::GetHeight(nscoord *aHeight)
{
  *aHeight = mSize.height;
  return NS_OK;
}


/* readonly attribute gfxIImageFrame currentFrame; */
NS_IMETHODIMP nsImageContainer::GetCurrentFrame(gfxIImageFrame * *aCurrentFrame)
{
  return this->GetFrameAt(mCurrentFrame, aCurrentFrame);
}

/* readonly attribute unsigned long numFrames; */
NS_IMETHODIMP nsImageContainer::GetNumFrames(PRUint32 *aNumFrames)
{
  return mFrames.Count(aNumFrames);
}

/* gfxIImageFrame getFrameAt (in unsigned long index); */
NS_IMETHODIMP nsImageContainer::GetFrameAt(PRUint32 index, gfxIImageFrame **_retval)
{
  nsISupports *sup = mFrames.ElementAt(index);
  if (!sup)
    return NS_ERROR_FAILURE;

  *_retval = NS_REINTERPRET_CAST(gfxIImageFrame *, sup);
  return NS_OK;
}

/* void appendFrame (in gfxIImageFrame item); */
NS_IMETHODIMP nsImageContainer::AppendFrame(gfxIImageFrame *item)
{
  PRUint32 numFrames;
  this->GetNumFrames(&numFrames);
  if (!mTimer){
    if (numFrames) {
      // Since we have more than one frame we need a timer
      mTimer = do_CreateInstance("@mozilla.org/timer;1");
      
      PRInt32 timeout;
      nsCOMPtr<gfxIImageFrame> currentFrame;
      this->GetFrameAt(mCurrentFrame, getter_AddRefs(currentFrame));
      currentFrame->GetTimeout(&timeout);
      if(timeout != -1 &&
         timeout >= 0) { // -1 means display this frame forever
        
        mTimer->Init(NS_STATIC_CAST(nsITimerCallback*, this), timeout, 
                     NS_PRIORITY_NORMAL, NS_TYPE_REPEATING_PRECISE);
      }
    }
  }

  mCurrentFrameIsFinishedDecoding = PR_FALSE;
  return mFrames.AppendElement(NS_REINTERPRET_CAST(nsISupports*, item));
}

/* void removeFrame (in gfxIImageFrame item); */
NS_IMETHODIMP nsImageContainer::RemoveFrame(gfxIImageFrame *item)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIEnumerator enumerate (); */
NS_IMETHODIMP nsImageContainer::Enumerate(nsIEnumerator **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void endFrameDecode (in gfxIImageFrame item, in unsigned long timeout); */
NS_IMETHODIMP nsImageContainer::EndFrameDecode(PRUint32 aFrameNum, PRUint32 aTimeout)
{
  // It is now okay to start the timer for the next frame in the animation
  mCurrentFrameIsFinishedDecoding = PR_TRUE;
  return NS_OK;
}

/* void decodingComplete (); */
NS_IMETHODIMP nsImageContainer::DecodingComplete(void)
{
  mDoneDecoding = PR_TRUE;
  return NS_OK;
}


/* void clear (); */
NS_IMETHODIMP nsImageContainer::Clear()
{
  return mFrames.Clear();
}

/* attribute long loopCount; */
NS_IMETHODIMP nsImageContainer::GetLoopCount(PRInt32 *aLoopCount)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsImageContainer::SetLoopCount(PRInt32 aLoopCount)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}



/** nsPIImageContainerWin methods **/

NS_IMETHODIMP nsImageContainer::DrawImage(HDC aDestDC, const nsRect * aSrcRect, const nsPoint * aDestPoint)
{
  nsresult rv;

  nsCOMPtr<gfxIImageFrame> img;
  rv = this->GetCurrentFrame(getter_AddRefs(img));

  if (NS_FAILED(rv))
    return rv;

  return NS_REINTERPRET_CAST(nsImageFrame*, img.get())->DrawImage(aDestDC, aSrcRect, aDestPoint);
}

NS_IMETHODIMP nsImageContainer::DrawScaledImage(HDC aDestDC, const nsRect * aSrcRect, const nsRect * aDestRect)
{
  nsresult rv;

  nsCOMPtr<gfxIImageFrame> img;
  rv = this->GetCurrentFrame(getter_AddRefs(img));

  if (NS_FAILED(rv))
    return rv;

  return NS_REINTERPRET_CAST(nsImageFrame*, img.get())->DrawScaledImage(aDestDC, aSrcRect, aDestRect);
}












NS_IMETHODIMP_(void) nsImageContainer::Notify(nsITimer *aTimer)
{
  ++mCurrentFrame;

  PRUint32 numFrames;
  GetNumFrames(&numFrames);
  if (mCurrentFrame > numFrames)
    mCurrentFrame = 0;

  nsCOMPtr<gfxIImageFrame> nextFrame;
  GetFrameAt(mCurrentFrame, getter_AddRefs(nextFrame));

  if (!nextFrame)
    return; // XXX thiss would suck.

  // Go to next frame in sequence
  PRInt32 timeout;
  nextFrame->GetTimeout(&timeout);

  // XXX do notification to FE to draw this frame
  nsRect* dirtyRect;
  nextFrame->GetRect(&dirtyRect);
  mObserver->FrameChanged(this, nsnull, nextFrame, dirtyRect);
/*
  mTimer->Init(NS_STATIC_CAST(nsITimerCallback*, this), timeout, 
               NS_PRIORITY_NORMAL, NS_TYPE_ONE_SHOT);
               */
}