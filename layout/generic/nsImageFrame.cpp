/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsHTMLParts.h"
#include "nsCOMPtr.h"
#include "nsImageFrame.h"
#include "nsString.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsIFrameImageLoader.h"
#include "nsIPresShell.h"
#include "nsHTMLIIDs.h"
#include "nsIImage.h"
#include "nsIWidget.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLContent.h"
#include "nsIDocument.h"
#include "nsIHTMLDocument.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsImageMap.h"
#include "nsILinkHandler.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsILoadGroup.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsHTMLContainerFrame.h"
#include "prprf.h"
#include "nsIFontMetrics.h"
#include "nsCSSRendering.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDeviceContext.h"
#include "nsINameSpaceManager.h"
#include "nsTextFragment.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsIStyleSet.h"
#include "nsLayoutAtoms.h"
#include "nsISizeOfHandler.h"

#include "nsIFrameManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsIMutableAccessible.h"
#include "nsIAccessibilityService.h"
#include "nsIServiceManager.h"
#include "nsIDOMNode.h"



#ifdef DEBUG
#undef NOISY_IMAGE_LOADING
#else
#undef NOISY_IMAGE_LOADING
#endif


// Value's for mSuppress
#define SUPPRESS_UNSET   0
#define DONT_SUPPRESS    1
#define SUPPRESS         2
#define DEFAULT_SUPPRESS 3

// Default alignment value (so we can tell an unset value from a set value)
#define ALIGN_UNSET PRUint8(-1)

// use the data in the reflow state to decide if the image has a constrained size
// (i.e. width and height that are based on the containing block size and not the image size) 
// so we can avoid animated GIF related reflows
static void HaveFixedSize(const nsHTMLReflowState& aReflowState, PRPackedBool& aConstrainedSize);

nsresult
NS_NewImageFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsImageFrame* it = new (aPresShell) nsImageFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsImageFrame::nsImageFrame() :
  mLowSrcImageLoader(nsnull)
#ifdef USE_IMG2
  , mIntrinsicSize(0, 0),
  mGotInitialReflow(PR_FALSE)
#endif
{
  // Size is constrained if we have a width and height. 
  // - Set in reflow in case the attributes are changed
  mSizeConstrained = PR_FALSE;
}

nsImageFrame::~nsImageFrame()
{
  if (mLowSrcImageLoader != nsnull) {
    delete mLowSrcImageLoader;
  }
}

NS_IMETHODIMP
nsImageFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_ENSURE_ARG_POINTER(aInstancePtr);
  *aInstancePtr = nsnull;

#ifdef DEBUG
  if (aIID.Equals(NS_GET_IID(nsIFrameDebug))) {
    *aInstancePtr = NS_STATIC_CAST(nsIFrameDebug*,this);
    return NS_OK;
  }
#endif

  if (aIID.Equals(NS_GET_IID(nsIImageFrame))) {
    *aInstancePtr = NS_STATIC_CAST(nsIImageFrame*,this);
    return NS_OK;
  } else if (aIID.Equals(NS_GET_IID(nsIFrame))) {
    *aInstancePtr = NS_STATIC_CAST(nsIFrame*,this);
    return NS_OK;
  } else if (aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = NS_STATIC_CAST(nsIImageFrame*,this);
    return NS_OK;
  } else if (aIID.Equals(NS_GET_IID(nsIAccessible))) {
    nsresult rv = NS_OK;
    NS_WITH_SERVICE(nsIAccessibilityService, accService, "@mozilla.org/accessibilityService;1", &rv);
    if (accService) {
     nsCOMPtr<nsIDOMNode> node = do_QueryInterface(mContent);
     nsIMutableAccessible* acc = nsnull;
     accService->CreateMutableAccessible(node,&acc);
     acc->SetName(NS_LITERAL_STRING("Image").get()); 
     acc->SetRole(NS_LITERAL_STRING("graphic").get());
     *aInstancePtr = acc;
     return NS_OK;
    }
    return NS_ERROR_FAILURE;
  } 

  return NS_NOINTERFACE;
}

NS_IMETHODIMP_(nsrefcnt) nsImageFrame::AddRef(void)
{
  NS_WARNING("not supported for frames");
  return 1;
}

NS_IMETHODIMP_(nsrefcnt) nsImageFrame::Release(void)
{
  NS_WARNING("not supported for frames");
  return 1;
}

NS_IMETHODIMP
nsImageFrame::Destroy(nsIPresContext* aPresContext)
{
  // Tell our image map, if there is one, to clean up
  // This causes the nsImageMap to unregister itself as
  // a DOM listener.
    if(mImageMap) {
      mImageMap->Destroy();
      NS_RELEASE(mImageMap);
    }

#ifdef USE_IMG2
    if (mImageRequest)
      mImageRequest->Cancel(NS_ERROR_FAILURE); // NS_BINDING_ABORT ?
    if (mLowImageRequest)
      mLowImageRequest->Cancel(NS_ERROR_FAILURE); // NS_BINDING_ABORT ?

    if (mListener)
      NS_REINTERPRET_CAST(nsImageListener*, mListener.get())->SetFrame(nsnull); // set the frame to null so we don't send messages to a dead object.
#endif

#ifndef USE_IMG2
  // Release image loader first so that it's refcnt can go to zero
  mImageLoader.StopAllLoadImages(aPresContext);
  if (mLowSrcImageLoader != nsnull) {
    mLowSrcImageLoader->StopAllLoadImages(aPresContext);
  }
#endif

  return nsLeafFrame::Destroy(aPresContext);
}

#ifdef USE_IMG2
#include "imgIContainer.h"
#include "imgILoader.h"

#endif

NS_IMETHODIMP
nsImageFrame::Init(nsIPresContext*  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsLeafFrame::Init(aPresContext, aContent, aParent,
                                   aContext, aPrevInFlow);

  // See if we have a SRC attribute
  nsAutoString src;
  nsresult ca;
  ca = mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::src, src);
  if ((NS_CONTENT_ATTR_HAS_VALUE != ca) || (src.Length() == 0))
  {
    // Let's see if this is an object tag and we have a DATA attribute
    nsIAtom*  tag;
    mContent->GetTag(tag);

    if(tag == nsHTMLAtoms::object)
      mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::data, src);
  
    NS_IF_RELEASE(tag);
  }

  nsAutoString lowSrc;
  nsresult lowSrcResult;
  lowSrcResult = mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::lowsrc, lowSrc);

  // Set the image loader's source URL and base URL
  nsCOMPtr<nsIURI> baseURL;
  GetBaseURI(getter_AddRefs(baseURL));

#ifdef USE_IMG2
  nsImageListener *listener;
  NS_NEWXPCOM(listener, nsImageListener);
  NS_ADDREF(listener);
  listener->SetFrame(this);
  listener->QueryInterface(NS_GET_IID(imgIDecoderObserver), getter_AddRefs(mListener));
  NS_ASSERTION(mListener, "queryinterface for the listener failed");
  NS_RELEASE(listener);


  nsCOMPtr<imgILoader> il(do_GetService("@mozilla.org/image/loader;1", &rv));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsILoadGroup> loadGroup;
  GetLoadGroup(aPresContext, getter_AddRefs(loadGroup));
#endif

  if (NS_CONTENT_ATTR_HAS_VALUE == lowSrcResult && lowSrc.Length() > 0) {
#ifdef USE_IMG2
    nsCOMPtr<nsIURI> lowURI;
    NS_NewURI(getter_AddRefs(lowURI), src, baseURL);

    il->LoadImage(lowURI, loadGroup, mListener, aPresContext, getter_AddRefs(mLowImageRequest));
#else
    mLowSrcImageLoader = new nsHTMLImageLoader;
    if (mLowSrcImageLoader) {
      mLowSrcImageLoader->Init(this, UpdateImageFrame, (void*)mLowSrcImageLoader, baseURL, lowSrc);
    }
#endif
  }


#ifdef USE_IMG2
  mInitialLoadCompleted = PR_FALSE;
  mCanSendLoadEvent = PR_TRUE;

  nsCOMPtr<nsIURI> srcURI;
  NS_NewURI(getter_AddRefs(srcURI), src, baseURL);
  il->LoadImage(srcURI, loadGroup, mListener, aPresContext, getter_AddRefs(mImageRequest));
  // if the image was found in the cache, it is possible that LoadImage will result in a call to OnStartContainer()
#else
  mImageLoader.Init(this, UpdateImageFrame, (void*)&mImageLoader, baseURL, src);

  mInitialLoadCompleted = PR_FALSE;
  mCanSendLoadEvent = PR_TRUE;
#endif

  return rv;
}

#ifndef USE_IMG2

nsresult
nsImageFrame::UpdateImageFrame(nsIPresContext* aPresContext,
                               nsHTMLImageLoader* aLoader,
                               nsIFrame* aFrame,
                               void* aClosure,
                               PRUint32 aStatus)
{
  nsImageFrame* frame = (nsImageFrame*) aFrame;
  return frame->UpdateImage(aPresContext, aStatus, aClosure);
}
#endif


#ifdef USE_IMG2

NS_IMETHODIMP nsImageFrame::OnStartDecode(imgIRequest *aRequest, nsIPresContext *aPresContext)
{
  return NS_OK;
}

NS_IMETHODIMP nsImageFrame::OnStartContainer(imgIRequest *aRequest, nsIPresContext *aPresContext, imgIContainer *aImage)
{
  nsCOMPtr<nsIPresShell> presShell;
  nsresult rv = aPresContext->GetShell(getter_AddRefs(presShell));
  if (NS_FAILED(rv)) return rv;

  mInitialLoadCompleted = PR_TRUE;

  nscoord w, h;
#ifdef DEBUG_pavlov
  NS_ENSURE_ARG_POINTER(aImage);
#else
  if (!aImage) return NS_ERROR_INVALID_ARG;
#endif
  aImage->GetWidth(&w);
  aImage->GetHeight(&h);

  float p2t;
  aPresContext->GetPixelsToTwips(&p2t);

  nsSize newsize(NSIntPixelsToTwips(w, p2t), NSIntPixelsToTwips(h, p2t));

  if (mIntrinsicSize != newsize) {
    mIntrinsicSize = newsize;

    if (mComputedSize.width != 0 && mComputedSize.height != 0)
      mTransform.SetToScale((float(mIntrinsicSize.width) / float(mComputedSize.width)), (float(mIntrinsicSize.height) / float(mComputedSize.height)));

    if (mParent) {
      if (mGotInitialReflow) { // don't reflow if we havn't gotten the inital reflow yet
        mState |= NS_FRAME_IS_DIRTY;
	      mParent->ReflowDirtyChild(presShell, (nsIFrame*) this);
      }
    }
    else {
      NS_ASSERTION(0, "No parent to pass the reflow request up to.");
    }
  }

  if (mCanSendLoadEvent && presShell) {
    // Send load event
    mCanSendLoadEvent = PR_FALSE;

    nsEventStatus status = nsEventStatus_eIgnore;
    nsEvent event;
    event.eventStructType = NS_EVENT;
    event.message = NS_IMAGE_LOAD;
    presShell->HandleEventWithTarget(&event,this,mContent,NS_EVENT_FLAG_INIT | NS_EVENT_FLAG_CANT_BUBBLE,&status);
  }

  return NS_OK;
}

NS_IMETHODIMP nsImageFrame::OnStartFrame(imgIRequest *aRequest, nsIPresContext *aPresContext, gfxIImageFrame *aFrame)
{
  return NS_OK;
}

NS_IMETHODIMP nsImageFrame::OnDataAvailable(imgIRequest *aRequest, nsIPresContext *aPresContext, gfxIImageFrame *aFrame, const nsRect *aRect)
{
  nsCOMPtr<nsIPresShell> presShell;
  nsresult rv = aPresContext->GetShell(getter_AddRefs(presShell));
  if (NS_FAILED(rv)) return rv;

  // XXX we need to make sure that the reflow from the OnContainerStart has been
  // processed before we start calling invalidate

  if (!aRect)
    return NS_ERROR_NULL_POINTER;


  nsRect r(aRect->x, aRect->y, aRect->width, aRect->height);

  // The y coordinate of aRect is passed as a scanline where the first scanline is given
  // a value of 1. We need to convert this to the nsFrames coordinate space by subtracting
  // 1.
  r.y -= 1;

  float p2t;
  aPresContext->GetPixelsToTwips(&p2t);
  r.x = NSIntPixelsToTwips(r.x, p2t);
  r.y = NSIntPixelsToTwips(r.y, p2t);
  r.width = NSIntPixelsToTwips(r.width, p2t);
  r.height = NSIntPixelsToTwips(r.height, p2t);

  mTransform.TransformCoord(&r.x, &r.y, &r.width, &r.height);

  Invalidate(aPresContext, r, PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP nsImageFrame::OnStopFrame(imgIRequest *aRequest, nsIPresContext *aPresContext, gfxIImageFrame *aFrame)
{
  return NS_OK;
}

NS_IMETHODIMP nsImageFrame::OnStopContainer(imgIRequest *aRequest, nsIPresContext *aPresContext, imgIContainer *aImage)
{
  return NS_OK;
}

NS_IMETHODIMP nsImageFrame::OnStopDecode(imgIRequest *aRequest, nsIPresContext *aPresContext, nsresult aStatus, const PRUnichar *aStatusArg)
{
  return NS_OK;
}

NS_IMETHODIMP nsImageFrame::FrameChanged(imgIContainer *aContainer, nsIPresContext *aPresContext, gfxIImageFrame *aNewFrame, nsRect *aDirtyRect)
{
  float p2t;
  aPresContext->GetPixelsToTwips(&p2t);
  nsRect r(*aDirtyRect);
  r *= p2t; // convert to twips

  mTransform.TransformCoord(&r.x, &r.y, &r.width, &r.height);

  Invalidate(aPresContext, r, PR_FALSE);

  return NS_OK;
}
#endif

#ifndef USE_IMG2

nsresult
nsImageFrame::UpdateImage(nsIPresContext* aPresContext, PRUint32 aStatus, void* aClosure)
{
#ifdef NOISY_IMAGE_LOADING
  ListTag(stdout);
  printf(": UpdateImage: status=%x\n", aStatus);
#endif

  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));

  // check to see if an image error occurred
  PRBool imageFailedToLoad = PR_FALSE;
  if (NS_IMAGE_LOAD_STATUS_ERROR & aStatus) { // We failed to load the image. Notify the pres shell

    // One of the two images didn't load, which one?
    // see if we are loading the lowsrc image
    if (mLowSrcImageLoader != nsnull) {
      // We ARE loading the lowsrc image
      // check to see if the closure data is the lowsrc
      if (aClosure == mLowSrcImageLoader) {
        // The lowsrc failed to load, but only set the bool to true
        // true if the src image failed.
        imageFailedToLoad = mImageLoader.GetImage() == nsnull || (mImageLoader.GetLoadStatus() & NS_IMAGE_LOAD_STATUS_ERROR);
      } else { 
        // src failed to load, 
        // see if the lowsrc is here so we can paint it instead
        imageFailedToLoad = (mLowSrcImageLoader->GetLoadStatus() & NS_IMAGE_LOAD_STATUS_ERROR) != 0;
      }
    } else {
      imageFailedToLoad = PR_TRUE;
    }
  }

  // if src failed and there is no lowsrc
  // or both failed to load, then notify the PresShell
  if (imageFailedToLoad) {    
    if (presShell) {
      nsAutoString usemap;
      mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::usemap, usemap);    
      // We failed to load the image. Notify the pres shell if we aren't an image map
      if (usemap.Length() == 0) {
        presShell->CantRenderReplacedElement(aPresContext, this);      
      }
    }
  } else if (NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE & aStatus) {
    // see if the image is ready in this notification as well, and if the size is not needed
    // check if we have a constrained size: if so, no need to reflow since we cannot change our size
    if(!mSizeConstrained) {
      if (mParent) {
        mState |= NS_FRAME_IS_DIRTY;
	      mParent->ReflowDirtyChild(presShell, (nsIFrame*) this);
      }
      else {
        NS_ASSERTION(0, "No parent to pass the reflow request up to.");
      }
    } else {
#ifdef NOISY_IMAGE_LOADING
      printf("Image has frozen size: skipping reflow for NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE status\n");
#endif
    }
  }

  //After these DOM events are fired its possible that this frame may be deleted.  As a result
  //the code should not attempt to access any of the frames internal data after this point.
  if (presShell) {
    if (imageFailedToLoad) {
      // Send error event
      nsEventStatus status = nsEventStatus_eIgnore;
      nsEvent event;
      event.eventStructType = NS_EVENT;
      event.message = NS_IMAGE_ERROR;
      presShell->HandleEventWithTarget(&event,this,mContent,NS_EVENT_FLAG_INIT,&status);
    }
    else if (mCanSendLoadEvent && NS_IMAGE_LOAD_STATUS_IMAGE_READY & aStatus) {
      // Send load event
      mCanSendLoadEvent = PR_FALSE;

      nsEventStatus status = nsEventStatus_eIgnore;
      nsEvent event;
      event.eventStructType = NS_EVENT;
      event.message = NS_IMAGE_LOAD;
      presShell->HandleEventWithTarget(&event,this,mContent,NS_EVENT_FLAG_INIT | NS_EVENT_FLAG_CANT_BUBBLE,&status);
    }
  }

#if 0 // ifdef'ing out the deleting of the lowsrc image
      // if the "src" image was change via script to an iage that
      // didn't exist, the the author would expect the lowsrc to be displayed.
  // now check to see if we have the entire low
  if (mLowSrcImageLoader != nsnull) {
    nsIImage * image = mImageLoader.GetImage();
    if (image) {
      PRInt32 imgSrcLinesLoaded = image != nsnull?image->GetDecodedY2():-1;
      if (image->GetDecodedY2() == image->GetHeight()) {
        delete mLowSrcImageLoader;
        mLowSrcImageLoader = nsnull;
      }
      NS_RELEASE(image);
    }
  }
#endif

  return NS_OK;
}

#endif


#ifdef USE_IMG2

#define MINMAX(_value,_min,_max) \
    ((_value) < (_min)           \
     ? (_min)                    \
     : ((_value) > (_max)        \
        ? (_max)                 \
        : (_value)))

#endif


void
nsImageFrame::GetDesiredSize(nsIPresContext* aPresContext,
                             const nsHTMLReflowState& aReflowState,
                             nsHTMLReflowMetrics& aDesiredSize)
{

#ifdef USE_IMG2
  nscoord widthConstraint = NS_INTRINSICSIZE;
  nscoord heightConstraint = NS_INTRINSICSIZE;
  PRBool fixedContentWidth = PR_FALSE;
  PRBool fixedContentHeight = PR_FALSE;

  nscoord minWidth, maxWidth, minHeight, maxHeight;

  // Determine whether the image has fixed content width
  widthConstraint = aReflowState.mComputedWidth;
  minWidth = aReflowState.mComputedMinWidth;
  maxWidth = aReflowState.mComputedMaxWidth;
  if (widthConstraint != NS_INTRINSICSIZE) {
    fixedContentWidth = PR_TRUE;
  }

  // Determine whether the image has fixed content height
  heightConstraint = aReflowState.mComputedHeight;
  minHeight = aReflowState.mComputedMinHeight;
  maxHeight = aReflowState.mComputedMaxHeight;
  if (heightConstraint != NS_UNCONSTRAINEDSIZE) {
    fixedContentHeight = PR_TRUE;
  }

  float p2t;
  aPresContext->GetPixelsToTwips(&p2t);

  PRBool haveComputedSize = PR_FALSE;
  PRBool needIntrinsicImageSize = PR_FALSE;

  nscoord newWidth=0, newHeight=0;
  if (fixedContentWidth) {
    newWidth = MINMAX(widthConstraint, minWidth, maxWidth);
    if (fixedContentHeight) {
      newHeight = MINMAX(heightConstraint, minHeight, maxHeight);
      haveComputedSize = PR_TRUE;
    } else {
      // We have a width, and an auto height. Compute height from
      // width once we have the intrinsic image size.
      if (mIntrinsicSize.height != 0) {
        newHeight = (mIntrinsicSize.height * newWidth) / mIntrinsicSize.width;
        haveComputedSize = PR_TRUE;
      } else {
        newHeight = NSIntPixelsToTwips(1, p2t); // XXX?
        needIntrinsicImageSize = PR_TRUE;
      }
    }
  } else if (fixedContentHeight) {
    // We have a height, and an auto width. Compute width from height
    // once we have the intrinsic image size.
    newHeight = MINMAX(heightConstraint, minHeight, maxHeight);
    if (mIntrinsicSize.width != 0) {
      newWidth = (mIntrinsicSize.width * newHeight) / mIntrinsicSize.height;
      haveComputedSize = PR_TRUE;
    } else {
      newWidth = NSIntPixelsToTwips(1, p2t);
      needIntrinsicImageSize = PR_TRUE;
    }
  } else {
    // auto size the image
    if (mIntrinsicSize.width == 0 && mIntrinsicSize.height == 0) {
      newWidth = NSIntPixelsToTwips(1, p2t);
      newHeight = NSIntPixelsToTwips(1, p2t);
      needIntrinsicImageSize = PR_TRUE;
    } else {
      newWidth = mIntrinsicSize.width;
      newHeight = mIntrinsicSize.height;
      haveComputedSize = PR_TRUE;
    }

  }

  mComputedSize.width = newWidth;
  mComputedSize.height = newHeight;

  if (mComputedSize == mIntrinsicSize)
    mTransform.SetToIdentity();
  else
    mTransform.SetToScale((float(mIntrinsicSize.width) / float(mComputedSize.width)), (float(mIntrinsicSize.height) / float(mComputedSize.height)));

  aDesiredSize.width = mComputedSize.width;
  aDesiredSize.height = mComputedSize.height;

#else
  PRBool cancelledReflow = PR_FALSE;

  if (mLowSrcImageLoader && !(mImageLoader.GetLoadStatus() & NS_IMAGE_LOAD_STATUS_IMAGE_READY)) {
#ifdef DEBUG_RODS
    PRBool gotDesiredSize =
#endif
    mLowSrcImageLoader->GetDesiredSize(aPresContext, &aReflowState, aDesiredSize);
#ifdef DEBUG_RODS
    if (gotDesiredSize) {
      // We have our "final" desired size. Cancel any pending
      // incremental reflows aimed at this frame.
      nsIPresShell* shell;
      aPresContext->GetShell(&shell);
      if (shell) {
        shell->CancelReflowCommand(this, nsnull);
        NS_RELEASE(shell);
      }
    }
#endif
  }

  // Must ask for desiredSize to start loading of image
  // that seems like a bogus API
  if (mImageLoader.GetDesiredSize(aPresContext, &aReflowState, aDesiredSize)) {
    if (!cancelledReflow) {
      // We have our "final" desired size. Cancel any pending
      // incremental reflows aimed at this frame.
      nsIPresShell* shell;
      aPresContext->GetShell(&shell);
      if (shell) {
        shell->CancelReflowCommand(this, nsnull);
        NS_RELEASE(shell);
      }
    }
  }
#endif
}

void
nsImageFrame::GetInnerArea(nsIPresContext* aPresContext,
                           nsRect& aInnerArea) const
{
  aInnerArea.x = mBorderPadding.left;
  aInnerArea.y = mBorderPadding.top;
  aInnerArea.width = mRect.width -
    (mBorderPadding.left + mBorderPadding.right);
  aInnerArea.height = mRect.height -
    (mBorderPadding.top + mBorderPadding.bottom);
}

NS_IMETHODIMP
nsImageFrame::Reflow(nsIPresContext*          aPresContext,
                     nsHTMLReflowMetrics&     aMetrics,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsImageFrame", aReflowState.reason);
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                  ("enter nsImageFrame::Reflow: availSize=%d,%d",
                  aReflowState.availableWidth, aReflowState.availableHeight));

  NS_PRECONDITION(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  // see if we have a frozen size (i.e. a fixed width and height)
  HaveFixedSize(aReflowState, mSizeConstrained);

#ifdef USE_IMG2
  if (aReflowState.reason == eReflowReason_Initial)
    mGotInitialReflow = PR_TRUE;
#endif

  GetDesiredSize(aPresContext, aReflowState, aMetrics);
  AddBordersAndPadding(aPresContext, aReflowState, aMetrics, mBorderPadding);
  if (nsnull != aMetrics.maxElementSize) {
    // If we have a percentage based width, then our MES width is 0
    if (eStyleUnit_Percent == aReflowState.mStylePosition->mWidth.GetUnit()) {
      aMetrics.maxElementSize->width = 0;
    } else {
      aMetrics.maxElementSize->width = aMetrics.width;
    }
    aMetrics.maxElementSize->height = aMetrics.height;
  }
  if (aMetrics.mFlags & NS_REFLOW_CALC_MAX_WIDTH) {
    aMetrics.mMaximumWidth = aMetrics.width;
  }
  aStatus = NS_FRAME_COMPLETE;

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                  ("exit nsImageFrame::Reflow: size=%d,%d",
                  aMetrics.width, aMetrics.height));
  return NS_OK;
}

// Computes the width of the specified string. aMaxWidth specifies the maximum
// width available. Once this limit is reached no more characters are measured.
// The number of characters that fit within the maximum width are returned in
// aMaxFit. NOTE: it is assumed that the fontmetrics have already been selected
// into the rendering context before this is called (for performance). MMP
void
nsImageFrame::MeasureString(const PRUnichar*     aString,
                            PRInt32              aLength,
                            nscoord              aMaxWidth,
                            PRUint32&            aMaxFit,
                            nsIRenderingContext& aContext)
{
  nscoord totalWidth = 0;
  nscoord spaceWidth;
  aContext.GetWidth(' ', spaceWidth);

  aMaxFit = 0;
  while (aLength > 0) {
    // Find the next place we can line break
    PRUint32  len = aLength;
    PRBool    trailingSpace = PR_FALSE;
    for (PRInt32 i = 0; i < aLength; i++) {
      if (XP_IS_SPACE(aString[i]) && (i > 0)) {
        len = i;  // don't include the space when measuring
        trailingSpace = PR_TRUE;
        break;
      }
    }
  
    // Measure this chunk of text, and see if it fits
    nscoord width;
    aContext.GetWidth(aString, len, width);
    PRBool  fits = (totalWidth + width) <= aMaxWidth;

    // If it fits on the line, or it's the first word we've processed then
    // include it
    if (fits || (0 == totalWidth)) {
      // New piece fits
      totalWidth += width;

      // If there's a trailing space then see if it fits as well
      if (trailingSpace) {
        if ((totalWidth + spaceWidth) <= aMaxWidth) {
          totalWidth += spaceWidth;
        } else {
          // Space won't fit. Leave it at the end but don't include it in
          // the width
          fits = PR_FALSE;
        }

        len++;
      }

      aMaxFit += len;
      aString += len;
      aLength -= len;
    }

    if (!fits) {
      break;
    }
  }
}

// Formats the alt-text to fit within the specified rectangle. Breaks lines
// between words if a word would extend past the edge of the rectangle
void
nsImageFrame::DisplayAltText(nsIPresContext*      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsString&      aAltText,
                             const nsRect&        aRect)
{
  const nsStyleColor* color =
    (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
  const nsStyleFont* font =
    (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);

  // Set font and color
  aRenderingContext.SetColor(color->mColor);
  aRenderingContext.SetFont(font->mFont);

  // Format the text to display within the formatting rect
  nsIFontMetrics* fm;
  aRenderingContext.GetFontMetrics(fm);

  nscoord maxDescent, height;
  fm->GetMaxDescent(maxDescent);
  fm->GetHeight(height);

  // XXX It would be nice if there was a way to have the font metrics tell
  // use where to break the text given a maximum width. At a minimum we need
  // to be able to get the break character...
  const PRUnichar* str = aAltText.GetUnicode();
  PRInt32          strLen = aAltText.Length();
  nscoord          y = aRect.y;
  while ((strLen > 0) && ((y + maxDescent) < aRect.YMost())) {
    // Determine how much of the text to display on this line
    PRUint32  maxFit;  // number of characters that fit
    MeasureString(str, strLen, aRect.width, maxFit, aRenderingContext);
    
    // Display the text
    aRenderingContext.DrawString(str, maxFit, aRect.x, y);

    // Move to the next line
    str += maxFit;
    strLen -= maxFit;
    y += height;
  }

  NS_RELEASE(fm);
}

struct nsRecessedBorder : public nsStyleBorder {
  nsRecessedBorder(nscoord aBorderWidth)
    : nsStyleBorder()
  {
    nsStyleCoord  styleCoord(aBorderWidth);

    mBorder.SetLeft(styleCoord);
    mBorder.SetTop(styleCoord);
    mBorder.SetRight(styleCoord);
    mBorder.SetBottom(styleCoord);

    mBorderStyle[0] = NS_STYLE_BORDER_STYLE_INSET;  
    mBorderStyle[1] = NS_STYLE_BORDER_STYLE_INSET;  
    mBorderStyle[2] = NS_STYLE_BORDER_STYLE_INSET;  
    mBorderStyle[3] = NS_STYLE_BORDER_STYLE_INSET;  

    mBorderColor[0] = 0;  
    mBorderColor[1] = 0;  
    mBorderColor[2] = 0;  
    mBorderColor[3] = 0;  

    mHasCachedBorder = PR_FALSE;
  }
};

void
nsImageFrame::DisplayAltFeedback(nsIPresContext*      aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 PRInt32              aIconId)
{
  // Calculate the inner area
  nsRect  inner;
  GetInnerArea(aPresContext, inner);

  // Display a recessed one pixel border
  float   p2t;
  nscoord borderEdgeWidth;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  borderEdgeWidth = NSIntPixelsToTwips(1, p2t);

  // Make sure we have enough room to actually render the border within
  // our frame bounds
  if ((inner.width < 2 * borderEdgeWidth) || (inner.height < 2 * borderEdgeWidth)) {
    return;
  }

  // Paint the border
  nsRecessedBorder recessedBorder(borderEdgeWidth);
  nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this, inner,
                              inner, recessedBorder, mStyleContext, 0);

  // Adjust the inner rect to account for the one pixel recessed border,
  // and a six pixel padding on each edge
  inner.Deflate(NSIntPixelsToTwips(7, p2t), NSIntPixelsToTwips(7, p2t));
  if (inner.IsEmpty()) {
    return;
  }

  // Clip so we don't render outside the inner rect
  PRBool clipState;
  aRenderingContext.PushState();
  aRenderingContext.SetClipRect(inner, nsClipCombine_kIntersect, clipState);

  // Display the icon
#ifdef USE_IMG2
  // XXX
#else
  nsIDeviceContext* dc;
  aRenderingContext.GetDeviceContext(dc);
  nsIImage*         icon;

  if (NS_SUCCEEDED(dc->LoadIconImage(aIconId, icon)) && icon) {
    aRenderingContext.DrawImage(icon, inner.x, inner.y);

    // Reduce the inner rect by the width of the icon, and leave an
    // additional six pixels padding
    PRInt32 iconWidth = NSIntPixelsToTwips(icon->GetWidth() + 6, p2t);
    inner.x += iconWidth;
    inner.width -= iconWidth;

    NS_RELEASE(icon);
  }

  NS_RELEASE(dc);
#endif

  // If there's still room, display the alt-text
  if (!inner.IsEmpty()) {
    nsAutoString altText;
    if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::alt, altText)) {
      DisplayAltText(aPresContext, aRenderingContext, altText, inner);
    }
  }

  aRenderingContext.PopState(clipState);
}

NS_METHOD
nsImageFrame::Paint(nsIPresContext* aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect,
                    nsFramePaintLayer aWhichLayer)
{
  PRBool isVisible;
  if (NS_SUCCEEDED(IsVisibleForPainting(aPresContext, aRenderingContext, PR_TRUE, &isVisible)) && 
      isVisible && mRect.width && mRect.height) {
    // First paint background and borders
    nsLeafFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                       aWhichLayer);

    // first get to see if lowsrc image is here
    PRInt32 lowSrcLinesLoaded = -1;
    PRInt32 imgSrcLinesLoaded = -1;
#ifdef USE_IMG2

    NS_ASSERTION(mImageRequest, "no image request!  this is bad");

    nsCOMPtr<imgIContainer> imgCon;
    nsCOMPtr<imgIContainer> lowImgCon;

    mImageRequest->GetImage(getter_AddRefs(imgCon));
#else
    nsIImage * lowImage = nsnull;
    nsIImage * image    = nsnull;
#endif

#ifdef USE_IMG2
    if (mLowImageRequest) {
      mLowImageRequest->GetImage(getter_AddRefs(lowImgCon));
    }
#else
    // if lowsrc is here 
    if (mLowSrcImageLoader) {
      lowImage = mLowSrcImageLoader->GetImage();
      lowSrcLinesLoaded = lowImage != nsnull?lowImage->GetDecodedY2():-1;
    }
#endif

#ifdef USE_IMG2
    PRUint32 loadStatus;
    mImageRequest->GetImageStatus(&loadStatus);
    if (!(loadStatus & imgIRequest::STATUS_SIZE_AVAILABLE) || (!imgCon && !lowImgCon)) {
#else
    image = mImageLoader.GetImage();
    imgSrcLinesLoaded = image != nsnull?image->GetDecodedY2():-1;

    if (!image && !lowImage) {
#endif
      // No image yet, or image load failed. Draw the alt-text and an icon
      // indicating the status
      if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer &&
          !mInitialLoadCompleted) {
        DisplayAltFeedback(aPresContext, aRenderingContext,
#ifdef USE_IMG2
                           (loadStatus & imgIRequest::STATUS_ERROR)
#else
                           mImageLoader.GetLoadImageFailed()
#endif
                           ? NS_ICON_BROKEN_IMAGE
                           : NS_ICON_LOADING_IMAGE);
      }
    }
    else {
      mInitialLoadCompleted = PR_TRUE;
      if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
        // Now render the image into our content area (the area inside the
        // borders and padding)
        nsRect inner;
        GetInnerArea(aPresContext, inner);

#ifdef USE_IMG2
        if (loadStatus & imgIRequest::STATUS_ERROR) {
          if (imgCon) {
            inner.SizeTo(mComputedSize);
          } else if (lowImgCon) {
          }
        }

        if (imgCon) {
          nsPoint p(inner.x, inner.y);
          if (mIntrinsicSize == mComputedSize) {
            inner.IntersectRect(inner, aDirtyRect);
            nsRect r(inner.x, inner.y, inner.width, inner.height);
            r.x -= mBorderPadding.left;
            r.y -= mBorderPadding.top;
            aRenderingContext.DrawImage(imgCon, &r, &p);
          } else {
            nsTransform2D trans;
            trans.SetToScale((float(mIntrinsicSize.width) / float(inner.width)),
                             (float(mIntrinsicSize.height) / float(inner.height)));

            nsRect r(inner.x, inner.y, inner.width, inner.height);

            trans.TransformCoord(&r.x, &r.y, &r.width, &r.height);

            nsRect d(inner.x, inner.y, mComputedSize.width, mComputedSize.height);
            aRenderingContext.DrawScaledImage(imgCon, &r, &d);
          }
        }
#else
        /*
         * aDirtyRect is the dirty rect, which we want to use to minimize
         * painting, but it's in terms of the frame and not the image, so
         * we need to use the intersection of the dirty rect and the inner
         * image rect. 
         */
        if (inner.IntersectRect(inner, aDirtyRect)) {
          if (mImageLoader.GetLoadImageFailed()) {
            float p2t;
            aPresContext->GetScaledPixelsToTwips(&p2t);
            if (image != nsnull) {
              inner.width  = NSIntPixelsToTwips(image->GetWidth(), p2t);
              inner.height = NSIntPixelsToTwips(image->GetHeight(), p2t);
            } else if (lowImage != nsnull) {
              inner.width  = NSIntPixelsToTwips(lowImage->GetWidth(), p2t);
              inner.height = NSIntPixelsToTwips(lowImage->GetHeight(), p2t);
            }
          }
          /*
           * We need to adjust the image source rect to match
           * the image's coordinates: we slide to the right and down.
           */
          nsRect innerSource(inner);
          innerSource.x -= mBorderPadding.left;
          innerSource.y -= mBorderPadding.top;

          if (image != nsnull && imgSrcLinesLoaded > 0) {
            aRenderingContext.DrawImage(image, innerSource, inner);
          } else if (lowImage != nsnull && lowSrcLinesLoaded > 0) {
            aRenderingContext.DrawImage(lowImage, innerSource, inner);
          }
        }
#endif
      }

      nsImageMap* map = GetImageMap(aPresContext);
      if (nsnull != map) {
        nsRect inner;
        GetInnerArea(aPresContext, inner);
        PRBool clipState;
        aRenderingContext.SetColor(NS_RGB(0, 0, 0));
        aRenderingContext.SetLineStyle(nsLineStyle_kDotted);
        aRenderingContext.PushState();
        aRenderingContext.Translate(inner.x, inner.y);
        map->Draw(aPresContext, aRenderingContext);
        aRenderingContext.PopState(clipState);
      }

#ifdef DEBUG
      if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) &&
          GetShowFrameBorders()) {
        nsImageMap* map = GetImageMap(aPresContext);
        if (nsnull != map) {
          nsRect inner;
          GetInnerArea(aPresContext, inner);
          PRBool clipState;
          aRenderingContext.SetColor(NS_RGB(0, 0, 0));
          aRenderingContext.PushState();
          aRenderingContext.Translate(inner.x, inner.y);
          map->Draw(aPresContext, aRenderingContext);
          aRenderingContext.PopState(clipState);
        }
      }
#endif
    }

#ifndef USE_IMG2
    NS_IF_RELEASE(lowImage);
    NS_IF_RELEASE(image);
#endif
  }
  
  return nsFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
}

nsImageMap*
nsImageFrame::GetImageMap(nsIPresContext* aPresContext)
{
  if (nsnull == mImageMap) {
    nsAutoString usemap;
    mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::usemap, usemap);
    if (0 == usemap.Length()) {
      return nsnull;
    }

    // Strip out whitespace in the name for navigator compatability
    // XXX NAV QUIRK
    usemap.StripWhitespace();

    nsIDocument* doc = nsnull;
    mContent->GetDocument(doc);
    if (nsnull == doc) {
      return nsnull;
    }

    if (usemap.First() == '#') {
      usemap.Cut(0, 1);
    }
    nsIHTMLDocument* hdoc;
    nsresult rv = doc->QueryInterface(NS_GET_IID(nsIHTMLDocument), (void**)&hdoc);
    NS_RELEASE(doc);
    if (NS_SUCCEEDED(rv)) {
      nsIDOMHTMLMapElement* map;
      rv = hdoc->GetImageMap(usemap, &map);
      NS_RELEASE(hdoc);
      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIPresShell> presShell;
        aPresContext->GetShell(getter_AddRefs(presShell));

        mImageMap = new nsImageMap();
        if (nsnull != mImageMap) {
          NS_ADDREF(mImageMap);
          mImageMap->Init(presShell, this, map);
        }
        NS_IF_RELEASE(map);
      }
    }
  }

  return mImageMap;
}

void
nsImageFrame::TriggerLink(nsIPresContext* aPresContext,
                          const nsString& aURLSpec,
                          const nsString& aTargetSpec,
                          PRBool aClick)
{
  // We get here with server side image map
  nsILinkHandler* handler = nsnull;
  aPresContext->GetLinkHandler(&handler);
  if (nsnull != handler) {
    if (aClick) {
      nsresult proceed = NS_OK;
      // Check that this page is allowed to load this URI.
      // Almost a copy of the similarly named method in nsGenericElement
      nsresult rv;
      NS_WITH_SERVICE(nsIScriptSecurityManager, securityManager, 
                      NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);

      nsCOMPtr<nsIPresShell> ps;
      if (NS_SUCCEEDED(rv)) 
        rv = aPresContext->GetShell(getter_AddRefs(ps));
      nsCOMPtr<nsIDocument> doc;
      if (NS_SUCCEEDED(rv) && ps) 
        rv = ps->GetDocument(getter_AddRefs(doc));
      
      nsCOMPtr<nsIURI> baseURI;
      if (NS_SUCCEEDED(rv) && doc) 
        baseURI = dont_AddRef(doc->GetDocumentURL());
      nsCOMPtr<nsIURI> absURI;
      if (NS_SUCCEEDED(rv)) 
        rv = NS_NewURI(getter_AddRefs(absURI), aURLSpec, baseURI);
      
      if (NS_SUCCEEDED(rv)) 
        proceed = securityManager->CheckLoadURI(baseURI, absURI, nsIScriptSecurityManager::STANDARD);

      // Only pass off the click event if the script security manager
      // says it's ok.
      if (NS_SUCCEEDED(proceed))
        handler->OnLinkClick(mContent, eLinkVerb_Replace, aURLSpec.GetUnicode(), aTargetSpec.GetUnicode());
    }
    else {
      handler->OnOverLink(mContent, aURLSpec.GetUnicode(), aTargetSpec.GetUnicode());
    }
    NS_RELEASE(handler);
  }
}

PRBool
nsImageFrame::IsServerImageMap()
{
  nsAutoString ismap;
  return NS_CONTENT_ATTR_HAS_VALUE ==
    mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::ismap, ismap);
}

PRIntn
nsImageFrame::GetSuppress()
{
  nsAutoString s;
  if (NS_CONTENT_ATTR_HAS_VALUE ==
      mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::suppress, s)) {
    if (s.EqualsIgnoreCase("true")) {
      return SUPPRESS;
    } else if (s.EqualsIgnoreCase("false")) {
      return DONT_SUPPRESS;
    }
  }
  return DEFAULT_SUPPRESS;
}

//XXX the event come's in in view relative coords, but really should
//be in frame relative coords by the time it hits our frame.

// Translate an point that is relative to our view (or a containing
// view) into a localized pixel coordinate that is relative to the
// content area of this frame (inside the border+padding).
void
nsImageFrame::TranslateEventCoords(nsIPresContext* aPresContext,
                                   const nsPoint& aPoint,
                                   nsPoint& aResult)
{
  nscoord x = aPoint.x;
  nscoord y = aPoint.y;

  // If we have a view then the event coordinates are already relative
  // to this frame; otherwise we have to adjust the coordinates
  // appropriately.
  nsIView* view;
  GetView(aPresContext, &view);
  if (nsnull == view) {
    nsPoint offset;
    GetOffsetFromView(aPresContext, offset, &view);
    if (nsnull != view) {
      x -= offset.x;
      y -= offset.y;
    }
  }

  // Subtract out border and padding here so that the coordinates are
  // now relative to the content area of this frame.
  nsRect inner;
  GetInnerArea(aPresContext, inner);
  x -= inner.x;
  y -= inner.y;

  // Translate the coordinates from twips to pixels
  float t2p;
  aPresContext->GetTwipsToPixels(&t2p);
  aResult.x = NSTwipsToIntPixels(x, t2p);
  aResult.y = NSTwipsToIntPixels(y, t2p);
}

PRBool
nsImageFrame::GetAnchorHREFAndTarget(nsString& aHref, nsString& aTarget)
{
  PRBool status = PR_FALSE;
  aHref.Truncate();
  aTarget.Truncate();

  // Walk up the content tree, looking for an nsIDOMAnchorElement
  nsCOMPtr<nsIContent> content;
  mContent->GetParent(*getter_AddRefs(content));
  while (content) {
    nsCOMPtr<nsIDOMHTMLAnchorElement> anchor(do_QueryInterface(content));
    if (anchor) {
      anchor->GetHref(aHref);
      if (aHref.Length() > 0) {
        status = PR_TRUE;
      }
      anchor->GetTarget(aTarget);
      break;
    }
    nsCOMPtr<nsIContent> parent;
    content->GetParent(*getter_AddRefs(parent));
    content = parent;
  }
  return status;
}

NS_IMETHODIMP  
nsImageFrame::GetContentForEvent(nsIPresContext* aPresContext,
                                 nsEvent* aEvent,
                                 nsIContent** aContent)
{
  NS_ENSURE_ARG_POINTER(aContent);
  nsImageMap* map;
  map = GetImageMap(aPresContext);

  if (nsnull != map) {
    nsPoint p;
    TranslateEventCoords(aPresContext, aEvent->point, p);
    nsAutoString absURL, target, altText;
    PRBool suppress;
    PRBool inside = PR_FALSE;
    nsCOMPtr<nsIContent> area;
    inside = map->IsInside(p.x, p.y, getter_AddRefs(area),
                           absURL, target, altText,
                           &suppress);
    if (inside && area) {
      *aContent = area;
      NS_ADDREF(*aContent);
      return NS_OK;
    }
  }

  return GetContent(aContent);
}

// XXX what should clicks on transparent pixels do?
NS_METHOD
nsImageFrame::HandleEvent(nsIPresContext* aPresContext,
                          nsGUIEvent* aEvent,
                          nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  nsImageMap* map;

  switch (aEvent->message) {
  case NS_MOUSE_LEFT_BUTTON_UP:
  case NS_MOUSE_MOVE:
    {
      map = GetImageMap(aPresContext);
      PRBool isServerMap = IsServerImageMap();
      if ((nsnull != map) || isServerMap) {
        nsPoint p;
        TranslateEventCoords(aPresContext, aEvent->point, p);
        nsAutoString absURL, target, altText;
        PRBool suppress;
        PRBool inside = PR_FALSE;
        // Even though client-side image map triggering happens
        // through content, we need to make sure we're not inside
        // (in case we deal with a case of both client-side and
        // sever-side on the same image - it happens!)
        if (nsnull != map) {
          nsCOMPtr<nsIContent> area;
          inside = map->IsInside(p.x, p.y, getter_AddRefs(area),
                                 absURL, target, altText,
                                 &suppress);
        }
        
        if (!inside && isServerMap) {
          suppress = GetSuppress();
          nsIURI* baseURL = nsnull;
          GetBaseURI(&baseURL);
          
          // Server side image maps use the href in a containing anchor
          // element to provide the basis for the destination url.
          nsAutoString src;
          if (GetAnchorHREFAndTarget(src, target)) {
            NS_MakeAbsoluteURI(absURL, src, baseURL);
            NS_IF_RELEASE(baseURL);
            
            // XXX if the mouse is over/clicked in the border/padding area
            // we should probably just pretend nothing happened. Nav4
            // keeps the x,y coordinates positive as we do; IE doesn't
            // bother. Both of them send the click through even when the
            // mouse is over the border.
            if (p.x < 0) p.x = 0;
            if (p.y < 0) p.y = 0;
            char cbuf[50];
            PR_snprintf(cbuf, sizeof(cbuf), "?%d,%d", p.x, p.y);
            absURL.AppendWithConversion(cbuf);
            PRBool clicked = PR_FALSE;
            if (aEvent->message == NS_MOUSE_LEFT_BUTTON_UP) {
              *aEventStatus = nsEventStatus_eConsumeDoDefault; 
              clicked = PR_TRUE;
            }
            TriggerLink(aPresContext, absURL, target, clicked);
          }
        }
      }
      break;
    }
    default:
      break;
  }

  return nsLeafFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

//XXX This will need to be rewritten once we have content for areas
NS_METHOD
nsImageFrame::GetCursor(nsIPresContext* aPresContext,
                        nsPoint& aPoint,
                        PRInt32& aCursor)
{
  nsImageMap* map = GetImageMap(aPresContext);
  if (nsnull != map) {
    nsPoint p;
    TranslateEventCoords(aPresContext, aPoint, p);
    aCursor = NS_STYLE_CURSOR_DEFAULT;
    if (map->IsInside(p.x, p.y)) {
      // Use style defined cursor if one is provided, otherwise when
      // the cursor style is "auto" we use the pointer cursor.
      const nsStyleColor* styleColor;
      GetStyleData(eStyleStruct_Color, (const nsStyleStruct*&)styleColor);
      aCursor = styleColor->mCursor;
      if (NS_STYLE_CURSOR_AUTO == aCursor) {
        aCursor = NS_STYLE_CURSOR_POINTER;
      }
    }
    return NS_OK;
  }
  return nsFrame::GetCursor(aPresContext, aPoint, aCursor);
}

NS_IMETHODIMP
nsImageFrame::AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent* aChild,
                               PRInt32 aNameSpaceID,
                               nsIAtom* aAttribute,
                               PRInt32 aHint)
{
  nsresult rv = nsLeafFrame::AttributeChanged(aPresContext, aChild,
                                              aNameSpaceID, aAttribute, aHint);
  if (NS_OK != rv) {
    return rv;
  }
  if (nsHTMLAtoms::src == aAttribute) {
    nsAutoString oldSRC, newSRC;
    mImageLoader.GetURLSpec(oldSRC);
    aChild->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::src, newSRC);
    if (!oldSRC.Equals(newSRC)) {
#ifdef NOISY_IMAGE_LOADING
      ListTag(stdout);
      printf(": new image source; old='");
      fputs(oldSRC, stdout);
      printf("' new='");
      fputs(newSRC, stdout);
      printf("'\n");
#endif



      PRUint32 loadStatus;
#ifdef USE_IMG2
      nsCOMPtr<nsIURI> baseURI;
      GetBaseURI(getter_AddRefs(baseURI));

      nsCOMPtr<nsILoadGroup> loadGroup;
      GetLoadGroup(aPresContext, getter_AddRefs(loadGroup));

      mImageRequest->GetImageStatus(&loadStatus);
      if (loadStatus & imgIRequest::STATUS_SIZE_AVAILABLE) {
        nsCOMPtr<nsIURI> uri;
        NS_NewURI(getter_AddRefs(uri), newSRC, baseURI);
        nsCOMPtr<imgILoader> il(do_GetService("@mozilla.org/image/loader;1"));
        NS_ASSERTION(il, "no image loader!");
        il->LoadImage(uri, loadGroup, mListener, aPresContext, getter_AddRefs(mImageRequest));

        mImageRequest->GetImageStatus(&loadStatus);
        if (loadStatus & imgIRequest::STATUS_SIZE_AVAILABLE) {

#else
      if (mImageLoader.IsImageSizeKnown()) {
        mImageLoader.UpdateURLSpec(aPresContext, newSRC);
        loadStatus = mImageLoader.GetLoadStatus();
        if (loadStatus & NS_IMAGE_LOAD_STATUS_IMAGE_READY) {
#endif
          // Trigger a paint now because image-loader won't if the
          // image is already loaded and ready to go.
          Invalidate(aPresContext, nsRect(0, 0, mRect.width, mRect.height), PR_FALSE);
        }
      }
      else {        
        // Stop the earlier image load
#ifdef USE_IMG2
        mImageRequest->Cancel(NS_ERROR_FAILURE); // NS_BINDING_ABORT ?

        mCanSendLoadEvent = PR_TRUE;

        nsCOMPtr<nsIURI> uri;
        NS_NewURI(getter_AddRefs(uri), newSRC, baseURI);
        nsCOMPtr<imgILoader> il(do_GetService("@mozilla.org/image/loader;1"));
        NS_ASSERTION(il, "no image loader!");
        il->LoadImage(uri, loadGroup, mListener, aPresContext, getter_AddRefs(mImageRequest));
#else
        mImageLoader.StopLoadImage(aPresContext);

        mCanSendLoadEvent = PR_TRUE;

        // Update the URL and start the new image load
        mImageLoader.UpdateURLSpec(aPresContext, newSRC);
#endif
      }
    }
  }
  else if (nsHTMLAtoms::width == aAttribute || nsHTMLAtoms::height == aAttribute)
  { // XXX: could check for new width == old width, and make that a no-op
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));
    mState |= NS_FRAME_IS_DIRTY;
	  mParent->ReflowDirtyChild(presShell, (nsIFrame*) this);
    // NOTE: mSizeFixed will be updated in Reflow...
  }

  return NS_OK;
}

NS_IMETHODIMP
nsImageFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::imageFrame;
  NS_ADDREF(*aType);
  return NS_OK;
}

NS_IMETHODIMP 
nsImageFrame::GetIntrinsicImageSize(nsSize& aSize)
{
#ifdef USE_IMG2
  aSize = mIntrinsicSize;
#else
  mImageLoader.GetIntrinsicSize(aSize);
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsImageFrame::GetNaturalImageSize(PRUint32* naturalWidth, 
                                  PRUint32 *naturalHeight)
{ 
#ifdef USE_IMG2
  *naturalWidth = 0;
  *naturalHeight = 0;

  if (mImageRequest) {
    nsCOMPtr<imgIContainer> container;
    mImageRequest->GetImage(getter_AddRefs(container));
    if (container) {
      PRInt32 w, h;
      container->GetWidth(&w);
      container->GetHeight(&h);

      *naturalWidth = NS_STATIC_CAST(PRUint32, w);
      *naturalHeight = NS_STATIC_CAST(PRUint32, h);
    }
  }

#else
  mImageLoader.GetNaturalImageSize(naturalWidth, naturalHeight);
#endif
  return NS_OK;
}

NS_IMETHODIMP 
nsImageFrame::IsImageComplete(PRBool* aComplete)
{
  NS_ENSURE_ARG_POINTER(aComplete);
#ifdef USE_IMG2
  PRUint32 status;
  mImageRequest->GetImageStatus(&status);
  *aComplete = ((status & imgIRequest::STATUS_LOAD_COMPLETE) != 0);
#else
  *aComplete = ((mImageLoader.GetLoadStatus() & NS_IMAGE_LOAD_STATUS_IMAGE_READY) != 0);
#endif
  return NS_OK;
}

void HaveFixedSize(const nsHTMLReflowState& aReflowState, PRPackedBool& aConstrainedSize)
{
  // check the width and height values in the reflow state's style struct
  // - if width and height are specified as either coord or percentage, then
  //   the size of the image frame is constrained
  nsStyleUnit widthUnit = aReflowState.mStylePosition->mWidth.GetUnit();
  nsStyleUnit heightUnit = aReflowState.mStylePosition->mHeight.GetUnit();

  aConstrainedSize = 
    ((widthUnit  == eStyleUnit_Coord ||
      widthUnit  == eStyleUnit_Percent) &&
     (heightUnit == eStyleUnit_Coord ||
      heightUnit == eStyleUnit_Percent));
}

void
nsImageFrame::GetBaseURI(nsIURI **aURI)
{
  NS_PRECONDITION(nsnull != aURI, "null OUT parameter pointer");

  nsresult rv;
  nsCOMPtr<nsIURI> baseURI;
  nsCOMPtr<nsIHTMLContent> htmlContent(do_QueryInterface(mContent, &rv));
  if (NS_SUCCEEDED(rv)) {
    htmlContent->GetBaseURL(*getter_AddRefs(baseURI));
  }
  else {
    nsCOMPtr<nsIDocument> doc;
    rv = mContent->GetDocument(*getter_AddRefs(doc));
    if (NS_SUCCEEDED(rv)) {
      doc->GetBaseURL(*getter_AddRefs(baseURI));
    }
  }
  *aURI = baseURI;
  NS_IF_ADDREF(*aURI);
}

void
nsImageFrame::GetLoadGroup(nsIPresContext *aPresContext, nsILoadGroup **aLoadGroup)
{
  if (!aPresContext)
    return;

  NS_PRECONDITION(nsnull != aLoadGroup, "null OUT parameter pointer");

  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));

  if (!shell)
    return;

  nsCOMPtr<nsIDocument> doc;
  shell->GetDocument(getter_AddRefs(doc));
  if (!doc)
    return;

  doc->GetDocumentLoadGroup(aLoadGroup);
}


#ifdef DEBUG
NS_IMETHODIMP
nsImageFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  PRUint32 sum;
  mImageLoader.SizeOf(aHandler, &sum);
  sum += sizeof(*this) - sizeof(mImageLoader);
  if (mImageMap) {
    PRBool recorded;
    aHandler->RecordObject((void*) mImageMap, &recorded);
    if (!recorded) {
      PRUint32 mapSize;
      mImageMap->SizeOf(aHandler, &mapSize);
      aHandler->AddSize(nsLayoutAtoms::imageMap, mapSize);
    }
  }
  *aResult = sum;
  return NS_OK;
}
#endif


#ifdef USE_IMG2
NS_IMPL_ISUPPORTS2(nsImageListener, imgIDecoderObserver, imgIContainerObserver)

nsImageListener::nsImageListener()
{
  NS_INIT_ISUPPORTS();
}

nsImageListener::~nsImageListener()
{
}

NS_IMETHODIMP nsImageListener::OnStartDecode(imgIRequest *aRequest, nsISupports *aContext)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresContext> pc(do_QueryInterface(aContext));

  NS_ASSERTION(pc, "not a pres context!");

  return mFrame->OnStartDecode(aRequest, pc);
}

NS_IMETHODIMP nsImageListener::OnStartContainer(imgIRequest *aRequest, nsISupports *aContext, imgIContainer *aImage)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresContext> pc(do_QueryInterface(aContext));

  NS_ASSERTION(pc, "not a pres context!");

  return mFrame->OnStartContainer(aRequest, pc, aImage);
}

NS_IMETHODIMP nsImageListener::OnStartFrame(imgIRequest *aRequest, nsISupports *aContext, gfxIImageFrame *aFrame)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresContext> pc(do_QueryInterface(aContext));

  NS_ASSERTION(pc, "not a pres context!");

  return mFrame->OnStartFrame(aRequest, pc, aFrame);
}

NS_IMETHODIMP nsImageListener::OnDataAvailable(imgIRequest *aRequest, nsISupports *aContext, gfxIImageFrame *aFrame, const nsRect *aRect)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresContext> pc(do_QueryInterface(aContext));

  NS_ASSERTION(pc, "not a pres context!");

  return mFrame->OnDataAvailable(aRequest, pc, aFrame, aRect);
}

NS_IMETHODIMP nsImageListener::OnStopFrame(imgIRequest *aRequest, nsISupports *aContext, gfxIImageFrame *aFrame)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresContext> pc(do_QueryInterface(aContext));

  NS_ASSERTION(pc, "not a pres context!");

  return mFrame->OnStopFrame(aRequest, pc, aFrame);
}

NS_IMETHODIMP nsImageListener::OnStopContainer(imgIRequest *aRequest, nsISupports *aContext, imgIContainer *aImage)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresContext> pc(do_QueryInterface(aContext));

  NS_ASSERTION(pc, "not a pres context!");

  return mFrame->OnStopContainer(aRequest, pc, aImage);
}

NS_IMETHODIMP nsImageListener::OnStopDecode(imgIRequest *aRequest, nsISupports *aContext, nsresult status, const PRUnichar *statusArg)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresContext> pc(do_QueryInterface(aContext));

  NS_ASSERTION(pc, "not a pres context!");

  return mFrame->OnStopDecode(aRequest, pc, status, statusArg);
}

NS_IMETHODIMP nsImageListener::FrameChanged(imgIContainer *aContainer, nsISupports *aContext, gfxIImageFrame *newframe, nsRect * dirtyRect)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresContext> pc(do_QueryInterface(aContext));

  NS_ASSERTION(pc, "not a pres context!");

  return mFrame->FrameChanged(aContainer, pc, newframe, dirtyRect);
}

#endif

