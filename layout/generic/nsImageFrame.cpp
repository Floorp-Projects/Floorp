/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsHTMLParts.h"
#include "nsCOMPtr.h"
#include "nsImageFrame.h"
#include "nsIImageLoadingContent.h"
#include "nsString.h"
#include "nsPrintfCString.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsIPresShell.h"
#include "nsIImage.h"
#include "nsIWidget.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLContent.h"
#include "nsIDocument.h"
#include "nsINodeInfo.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsImageMap.h"
#include "nsILinkHandler.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsILoadGroup.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
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
#include "nsImageMapUtils.h"
#include "nsIFrameManager.h"
#include "nsIScriptSecurityManager.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif
#include "nsIServiceManager.h"
#include "nsIDOMNode.h"
#include "nsGUIEvent.h"

#include "imgIContainer.h"
#include "imgILoader.h"

#include "nsIEventQueueService.h"
#include "plevent.h"

#include "nsContentPolicyUtils.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsCSSFrameConstructor.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "gfxIImageFrame.h"
#include "nsIDOMRange.h"

#include "nsLayoutErrors.h"

#ifdef DEBUG
#undef NOISY_IMAGE_LOADING
#undef NOISY_ICON_LOADING
#else
#undef NOISY_IMAGE_LOADING
#undef NOISY_ICON_LOADING
#endif

// sizes (pixels) for image icon, padding and border frame
#define ICON_SIZE        (16)
#define ICON_PADDING     (3)
#define ALT_BORDER_WIDTH (1)


//we must add hooks soon
#define IMAGE_EDITOR_CHECK 1

// Default alignment value (so we can tell an unset value from a set value)
#define ALIGN_UNSET PRUint8(-1)

// static icon information: initialized in Init and de-initialized in Destroy
// - created if null, deleted if refcount goes to 0
nsImageFrame::IconLoad* nsImageFrame::mIconLoad = nsnull;

// test if the width and height are fixed, looking at the style data
static PRBool HaveFixedSize(const nsStylePosition* aStylePosition)
{
  // check the width and height values in the reflow state's style struct
  // - if width and height are specified as either coord or percentage, then
  //   the size of the image frame is constrained
  nsStyleUnit widthUnit = aStylePosition->mWidth.GetUnit();
  nsStyleUnit heightUnit = aStylePosition->mHeight.GetUnit();

  return ((widthUnit  == eStyleUnit_Coord ||
           widthUnit  == eStyleUnit_Percent) &&
          (heightUnit == eStyleUnit_Coord ||
           heightUnit == eStyleUnit_Percent));
}
// use the data in the reflow state to decide if the image has a constrained size
// (i.e. width and height that are based on the containing block size and not the image size) 
// so we can avoid animated GIF related reflows
inline PRBool HaveFixedSize(const nsHTMLReflowState& aReflowState)
{ 
  NS_ASSERTION(aReflowState.mStylePosition, "crappy reflowState - null stylePosition");
  // when an image has percent css style height or width, but mComputedHeight 
  // or mComputedWidth of reflow state is  NS_UNCONSTRAINEDSIZE  
  // it needs to return PR_FALSE to cause an incremental reflow later
  // if an image is inside table like bug 156731 simple testcase III, 
  // during pass 1 reflow, mComputedWidth is NS_UNCONSTRAINEDSIZE
  // in pass 2 reflow, mComputedWidth is 0, it also needs to return PR_FALSE
  // see bug 156731
  nsStyleUnit heightUnit = (*(aReflowState.mStylePosition)).mHeight.GetUnit();
  nsStyleUnit widthUnit = (*(aReflowState.mStylePosition)).mWidth.GetUnit();
  return ((eStyleUnit_Percent == heightUnit && NS_UNCONSTRAINEDSIZE == aReflowState.mComputedHeight) ||
          (eStyleUnit_Percent == widthUnit && (NS_UNCONSTRAINEDSIZE == aReflowState.mComputedWidth ||
           0 == aReflowState.mComputedWidth)))
          ? PR_FALSE
          : HaveFixedSize(aReflowState.mStylePosition); 
}

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
  mComputedSize(0, 0),
  mIntrinsicSize(0, 0),
  mPresContext(nsnull)
{
  // We assume our size is not constrained and we haven't gotten an
  // initial reflow yet, so don't touch those flags.
}

nsImageFrame::~nsImageFrame()
{
  NS_PRECONDITION(!mPresContext, "Never got destroyed properly!");
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
  }

  return NS_NOINTERFACE;
}

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsImageFrame::GetAccessible(nsIAccessible** aAccessible)
{
  nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");

  if (accService) {
    return accService->CreateHTMLImageAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
  }

  return NS_ERROR_FAILURE;
}
#endif

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
  if (mImageMap) {
    mImageMap->Destroy();
    NS_RELEASE(mImageMap);
  }

  // set the frame to null so we don't send messages to a dead object.
  if (mListener) {
    nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
    if (imageLoader) {
      imageLoader->RemoveObserver(mListener);
    }
    
    NS_REINTERPRET_CAST(nsImageListener*, mListener.get())->SetFrame(nsnull);
  }
  
  mListener = nsnull;

  // check / cleanup the IconLoads singleton
  if (mIconLoad) {
#ifdef NOISY_ICON_LOADING
    printf( "Releasing IconLoad (%p)\n", this);
#endif
    if (mIconLoad->Release()) {
#ifdef NOISY_ICON_LOADING
      printf( "Deleting IconLoad (%p)\n", this);
#endif
      delete mIconLoad;
      mIconLoad = nsnull;
    }
  }
  
  mPresContext = nsnull;

  return nsSplittableFrame::Destroy(aPresContext);
}



NS_IMETHODIMP
nsImageFrame::Init(nsIPresContext*  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsStyleContext*  aContext,
                   nsIFrame*        aPrevInFlow)
{
  mPresContext = aPresContext;
  
  nsresult  rv = nsSplittableFrame::Init(aPresContext, aContent, aParent,
                                         aContext, aPrevInFlow);
  NS_ENSURE_SUCCESS(rv, rv);
  
  mPresContext = aPresContext;

  mListener = new nsImageListener(this);
  if (!mListener) return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(aContent);
  NS_ENSURE_TRUE(imageLoader, NS_ERROR_UNEXPECTED);
  imageLoader->AddObserver(mListener);

  LoadIcons(aPresContext);

  nsCOMPtr<imgIRequest> currentRequest;
  imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                          getter_AddRefs(currentRequest));
  PRUint32 currentLoadStatus = imgIRequest::STATUS_ERROR;
  if (currentRequest) {
    currentRequest->GetImageStatus(&currentLoadStatus);
  }

  if (currentLoadStatus & imgIRequest::STATUS_ERROR) {
    nsCOMPtr<nsIPresShell> presShell;
    mPresContext->GetShell(getter_AddRefs(presShell));
    NS_ASSERTION(presShell, "Must have a pres shell!");

    PRBool loadBlocked = PR_FALSE;
    imageLoader->GetImageBlocked(&loadBlocked);
    rv = HandleLoadError(loadBlocked ? NS_ERROR_IMAGE_BLOCKED : NS_ERROR_FAILURE,
                         presShell);
  }
  // If we already have an image container, OnStartContainer won't be called
  // Set the animation mode here
  nsCOMPtr<imgIContainer> image;
  if (currentRequest)
    currentRequest->GetImage(getter_AddRefs(image));
  if (image) {
    PRUint16 animateMode = imgIContainer::kNormalAnimMode; //default value
    nsresult rv = mPresContext->GetImageAnimationMode(&animateMode);
    if (NS_SUCCEEDED(rv))
      image->SetAnimationMode(animateMode);
  }
  
  return rv;
}

PRBool
nsImageFrame::RecalculateTransform(imgIContainer* aImage)
{
  PRBool intrinsicSizeChanged = PR_FALSE;
  
  if (aImage) {
    float p2t;
    mPresContext->GetPixelsToTwips(&p2t);

    nsSize imageSizeInPx;
    aImage->GetWidth(&imageSizeInPx.width);
    aImage->GetHeight(&imageSizeInPx.height);
    nsSize newSize(NSIntPixelsToTwips(imageSizeInPx.width, p2t),
                   NSIntPixelsToTwips(imageSizeInPx.height, p2t));
    if (mIntrinsicSize != newSize) {
      intrinsicSizeChanged = PR_TRUE;
      mIntrinsicSize = newSize;
    }
  }

  // In any case, we need to translate this over appropriately.  Set
  // translation _before_ setting scaling so that it does not get
  // scaled!

  // XXXbz does this introduce rounding errors because of the cast to
  // float?  Should we just manually add that stuff in every time
  // instead?
  mTransform.SetToTranslate(float(mBorderPadding.left),
                            float(mBorderPadding.top - GetContinuationOffset()));
  
  // Set the scale factors
  if (mIntrinsicSize.width != 0 && mIntrinsicSize.height != 0 &&
      mIntrinsicSize != mComputedSize) {
    mTransform.AddScale(float(mComputedSize.width)  / float(mIntrinsicSize.width),
                        float(mComputedSize.height) / float(mIntrinsicSize.height));
  }

  return intrinsicSizeChanged;
}

/*
 * These two functions basically do the same check.  The first one
 * checks that the given request is the current request for our
 * mContent.  The second checks that the given image container the
 * same as the image container on the current request for our
 * mContent.
 */
PRBool
nsImageFrame::IsPendingLoad(imgIRequest* aRequest) const
{
  // Default to pending load in case of errors
  nsCOMPtr<nsIImageLoadingContent> imageLoader(do_QueryInterface(mContent));
  NS_ASSERTION(imageLoader, "No image loading content?");

  PRInt32 requestType = nsIImageLoadingContent::UNKNOWN_REQUEST;
  imageLoader->GetRequestType(aRequest, &requestType);

  return requestType != nsIImageLoadingContent::CURRENT_REQUEST;
}

PRBool
nsImageFrame::IsPendingLoad(imgIContainer* aContainer) const
{
  //  default to pending load in case of errors
  if (!aContainer) {
    NS_ERROR("No image container!");
    return PR_TRUE;
  }

  nsCOMPtr<nsIImageLoadingContent> imageLoader(do_QueryInterface(mContent));
  NS_ASSERTION(imageLoader, "No image loading content?");
  
  nsCOMPtr<imgIRequest> currentRequest;
  imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                          getter_AddRefs(currentRequest));
  if (!currentRequest) {
    NS_ERROR("No current request");
    return PR_TRUE;
  }

  nsCOMPtr<imgIContainer> currentContainer;
  currentRequest->GetImage(getter_AddRefs(currentContainer));

  return currentContainer != aContainer;
  
}

nsRect
nsImageFrame::ConvertPxRectToTwips(const nsRect& aRect) const
{
  float p2t;
  mPresContext->GetPixelsToTwips(&p2t);
  return nsRect(NSIntPixelsToTwips(aRect.x, p2t), // x
                NSIntPixelsToTwips(aRect.y, p2t), // y
                NSIntPixelsToTwips(aRect.width, p2t), // width
                NSIntPixelsToTwips(aRect.height, p2t)); // height
}

nsresult
nsImageFrame::HandleLoadError(nsresult aStatus, nsIPresShell* aPresShell)
{
  if (aStatus == NS_ERROR_IMAGE_BLOCKED &&
      !(mIconLoad && mIconLoad->mPrefAllImagesBlocked)) {
    // don't display any alt feedback in this case; we're blocking images
    // from that site and don't care to see anything from them
    return NS_OK;
  }
  
  // If we have an image map, don't do anything here
  // XXXbz Why?  This is what the code used to do, but there's no good
  // reason for it....

  nsAutoString usemap;
  mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::usemap, usemap);    
  if (!usemap.IsEmpty()) {
    return NS_OK;
  }
  
  // Check if we want to use a placeholder box with an icon or just
  // let the the presShell make us into inline text.  Decide as follows:
  //
  //  - if our special "force icons" style is set, show an icon
  //  - else if our "do not show placeholders" pref is set, skip the icon
  //  - else:
  //  - if QuirksMode, and there is no alt attribute, and this is not an
  //    <object> (which could not possibly have such an attribute), show an
  //    icon.
  //  - if QuirksMode, and the IMG has a size, and the image is
  //    broken, not blocked, show an icon.
  //  - otherwise, skip the icon

  PRBool useSizedBox;
  
  const nsStyleUIReset* uiResetData = GetStyleUIReset();
  if (uiResetData->mForceBrokenImageIcon) {
    useSizedBox = PR_TRUE;
  }
  else if (mIconLoad && mIconLoad->mPrefForceInlineAltText) {
    useSizedBox = PR_FALSE;
  }
  else {
    nsCompatibility mode;
    mPresContext->GetCompatibilityMode(&mode);
    if (mode != eCompatibility_NavQuirks) {
      useSizedBox = PR_FALSE;
    }
    else {
      // We are in quirks mode, so we can just check the tag name; no need to
      // check the namespace.
      nsCOMPtr<nsINodeInfo> nodeInfo;
      mContent->GetNodeInfo(getter_AddRefs(nodeInfo));
      
      if (!mContent->HasAttr(kNameSpaceID_None, nsHTMLAtoms::alt) &&
          nodeInfo &&
          !nodeInfo->Equals(nsHTMLAtoms::object)) {
        useSizedBox = PR_TRUE;
      }
      else if (aStatus == NS_ERROR_IMAGE_BLOCKED) {
        useSizedBox = PR_FALSE;
      }
      else {
        // check whether we have fixed size
        useSizedBox = HaveFixedSize(GetStylePosition());
      }
    }
  }
  
  if (!useSizedBox) {
    // let the presShell handle converting this into the inline alt
    // text frame
    // We have to try to get the primary frame for mContent, since for
    // <object> the frame CantRenderReplacedElement wants is the
    // ObjectFrame, not us (we're an anonymous frame then)....
    nsIFrame* primaryFrame = nsnull;
    aPresShell->GetPrimaryFrameFor(mContent, &primaryFrame);
    aPresShell->CantRenderReplacedElement(primaryFrame ? primaryFrame : this);
    return NS_ERROR_FRAME_REPLACED;
  }

  // we are handling it
  // invalidate the icon area (it may change states)   
  InvalidateIcon(mPresContext);
  return NS_OK;
}

nsresult
nsImageFrame::OnStartContainer(imgIRequest *aRequest, imgIContainer *aImage)
{
  if (!aImage) return NS_ERROR_INVALID_ARG;
  NS_ENSURE_TRUE(mPresContext, NS_ERROR_UNEXPECTED);  // why are we bothering?

  // handle iconLoads first...
  if (HandleIconLoads(aRequest, PR_FALSE)) {
    return NS_OK;
  }

  if (aImage)
  {
    /* Get requested animation policy from the pres context:
     *   normal = 0
     *   one frame = 1
     *   one loop = 2
     */
    PRUint16 animateMode = imgIContainer::kNormalAnimMode; //default value
    nsresult rv = mPresContext->GetImageAnimationMode(&animateMode);
    if (NS_SUCCEEDED(rv))
      aImage->SetAnimationMode(animateMode);
  }

  if (IsPendingLoad(aRequest)) {
    // We don't care
    return NS_OK;
  }
  
  RecalculateTransform(aImage);

  // Now we need to reflow if we have an unconstrained size and have
  // already gotten the initial reflow
  if (!(mState & IMAGE_SIZECONSTRAINED) && (mState & IMAGE_GOTINITIALREFLOW)) { 
    nsCOMPtr<nsIPresShell> presShell;
    mPresContext->GetShell(getter_AddRefs(presShell));
    NS_ASSERTION(mParent, "No parent to pass the reflow request up to.");
    NS_ASSERTION(presShell, "No PresShell.");
    if (mParent && presShell) { 
      mState |= NS_FRAME_IS_DIRTY;
      mParent->ReflowDirtyChild(presShell, NS_STATIC_CAST(nsIFrame*, this));
    }
  }

  return NS_OK;
}

nsresult
nsImageFrame::OnDataAvailable(imgIRequest *aRequest,
                              gfxIImageFrame *aFrame,
                              const nsRect *aRect)
{
  // XXX do we need to make sure that the reflow from the
  // OnStartContainer has been processed before we start calling
  // invalidate?

  NS_ENSURE_ARG_POINTER(aRect);
  NS_ENSURE_TRUE(mPresContext, NS_ERROR_UNEXPECTED);  // why are we bothering?


  if (!(mState & IMAGE_GOTINITIALREFLOW)) {
    // Don't bother to do anything; we have a reflow coming up!
    return NS_OK;
  }
  
  // handle iconLoads first...
  if (HandleIconLoads(aRequest, PR_FALSE)) {
    if (!aRect->IsEmpty()) {
      Invalidate(mPresContext, *aRect, PR_FALSE);
    }
    return NS_OK;
  }

  if (IsPendingLoad(aRequest)) {
    // We don't care
    return NS_OK;
  }

  // Don't invalidate if the current visible frame isn't the one the data is
  // from
  nsCOMPtr<imgIContainer> container;
  aRequest->GetImage(getter_AddRefs(container));
  if (container) {
    nsCOMPtr<gfxIImageFrame> currentFrame;
    container->GetCurrentFrame(getter_AddRefs(currentFrame));
    if (aFrame != currentFrame) {
      // just bail
      return NS_OK;
    }
  }

  nsRect r = ConvertPxRectToTwips(*aRect);
  mTransform.TransformCoord(&r.x, &r.y, &r.width, &r.height);

  if (!r.IsEmpty()) {
    Invalidate(mPresContext, r, PR_FALSE);
  }
  
  return NS_OK;
}

nsresult
nsImageFrame::OnStopDecode(imgIRequest *aRequest,
                           nsresult aStatus,
                           const PRUnichar *aStatusArg)
{
  NS_ENSURE_TRUE(mPresContext, NS_ERROR_UNEXPECTED);  // why are we bothering?

  nsCOMPtr<nsIPresShell> presShell;
  mPresContext->GetShell(getter_AddRefs(presShell));
  NS_ASSERTION(presShell, "No PresShell.");

  // handle iconLoads first...
  if (HandleIconLoads(aRequest, NS_SUCCEEDED(aStatus))) {
    return NS_OK;
  }

  // Check what request type we're dealing with
  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
  NS_ASSERTION(imageLoader, "Who's notifying us??");
  PRInt32 loadType = nsIImageLoadingContent::UNKNOWN_REQUEST;
  imageLoader->GetRequestType(aRequest, &loadType);
  if (loadType != nsIImageLoadingContent::CURRENT_REQUEST &&
      loadType != nsIImageLoadingContent::PENDING_REQUEST) {
    return NS_ERROR_FAILURE;
  }

  if (loadType == nsIImageLoadingContent::PENDING_REQUEST) {
    // May have to switch sizes here!
    PRBool intrinsicSizeChanged = PR_TRUE;
    if (NS_SUCCEEDED(aStatus)) {
      nsCOMPtr<imgIContainer> imageContainer;
      aRequest->GetImage(getter_AddRefs(imageContainer));
      NS_ASSERTION(imageContainer, "Successful load with no container?");
      intrinsicSizeChanged = RecalculateTransform(imageContainer);
    }
    else {
      // Have to size to 0,0 so that GetDesiredSize recalculates the size
      mIntrinsicSize.SizeTo(0, 0);
    }

    if (mState & IMAGE_GOTINITIALREFLOW) { // do nothing if we havn't gotten the inital reflow yet
      if (!(mState & IMAGE_SIZECONSTRAINED) && intrinsicSizeChanged) {
        NS_ASSERTION(mParent, "No parent to pass the reflow request up to.");
        if (mParent && presShell) { 
          mState |= NS_FRAME_IS_DIRTY;
          mParent->ReflowDirtyChild(presShell, NS_STATIC_CAST(nsIFrame*, this));
        }
      } else {
        nsSize s = GetSize();
        nsRect r(0, 0, s.width, s.height);
        if (!r.IsEmpty()) {
          Invalidate(mPresContext, r, PR_FALSE);
        }
      }
    }
  }

  // if src failed to load, determine how to handle it: 
  //  - either render the ALT text in this frame, or let the presShell
  //  handle it
  if (NS_FAILED(aStatus) && aStatus != NS_ERROR_IMAGE_SRC_CHANGED &&
      presShell) {
    HandleLoadError(aStatus, presShell);
  }

  return NS_OK;
}

nsresult
nsImageFrame::FrameChanged(imgIContainer *aContainer,
                           gfxIImageFrame *aNewFrame,
                           nsRect *aDirtyRect)
{
  if (!GetStyleVisibility()->IsVisible()) {
    return NS_OK;
  }

  if (IsPendingLoad(aContainer)) {
    // We don't care about it
    return NS_OK;
  }
  
  nsRect r = ConvertPxRectToTwips(*aDirtyRect);
  mTransform.TransformCoord(&r.x, &r.y, &r.width, &r.height);

  if (!r.IsEmpty()) {
    Invalidate(mPresContext, r, PR_FALSE);
  }
  return NS_OK;
}


#define MINMAX(_value,_min,_max) \
    ((_value) < (_min)           \
     ? (_min)                    \
     : ((_value) > (_max)        \
        ? (_max)                 \
        : (_value)))

void
nsImageFrame::GetDesiredSize(nsIPresContext* aPresContext,
                             const nsHTMLReflowState& aReflowState,
                             nsHTMLReflowMetrics& aDesiredSize)
{
  // if mIntrinsicSize.width and height are 0, then we should
  // check to see if the size is already known by the image container.
  if (mIntrinsicSize.width == 0 && mIntrinsicSize.height == 0) {
    nsCOMPtr<imgIRequest> currentRequest;
    nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
    if (imageLoader) {
      imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                              getter_AddRefs(currentRequest));
    }
    nsCOMPtr<imgIContainer> currentContainer;
    if (currentRequest) {
      currentRequest->GetImage(getter_AddRefs(currentContainer));
    }
      
    float p2t;
    aPresContext->GetPixelsToTwips(&p2t);

    if (currentContainer) {
      RecalculateTransform(currentContainer);
    } else {
      // image request is null or image size not known, probably an
      // invalid image specified
      // - make the image big enough for the icon (it may not be
      // used if inline alt expansion is used instead)
      // XXX: we need this in composer, but it is also good for
      // XXX: general quirks mode to always have room for the icon
      nsCompatibility mode;
      aPresContext->GetCompatibilityMode(&mode);
      if (mode == eCompatibility_NavQuirks) {
        mIntrinsicSize.SizeTo(NSIntPixelsToTwips(ICON_SIZE+(2*(ICON_PADDING+ALT_BORDER_WIDTH)), p2t),
                              NSIntPixelsToTwips(ICON_SIZE+(2*(ICON_PADDING+ALT_BORDER_WIDTH)), p2t));
      }
      RecalculateTransform(nsnull);
    }
  }

  float t2p, sp2t;
  aPresContext->GetTwipsToPixels(&t2p);
  aPresContext->GetScaledPixelsToTwips(&sp2t);

  // convert from normal twips to scaled twips (printing...)
  float t2st = t2p * sp2t; // twips to scaled twips
  nscoord intrinsicWidth =
      NSToCoordRound(float(mIntrinsicSize.width) * t2st);
  nscoord intrinsicHeight =
      NSToCoordRound(float(mIntrinsicSize.height) * t2st);

  // Determine whether the image has fixed content width
  nscoord widthConstraint = aReflowState.mComputedWidth;
  nscoord minWidth = aReflowState.mComputedMinWidth;
  nscoord maxWidth = aReflowState.mComputedMaxWidth;

  // Determine whether the image has fixed content height
  nscoord heightConstraint = aReflowState.mComputedHeight;
  nscoord minHeight = aReflowState.mComputedMinHeight;
  nscoord maxHeight = aReflowState.mComputedMaxHeight;

  PRBool isAutoWidth = widthConstraint == NS_INTRINSICSIZE;
  if (isAutoWidth) {
    widthConstraint = intrinsicWidth;
  }
  PRBool isAutoHeight = heightConstraint == NS_UNCONSTRAINEDSIZE;
  if (isAutoHeight) {
    heightConstraint = intrinsicHeight;
  }

  nscoord newWidth = MINMAX(widthConstraint, minWidth, maxWidth);
  nscoord newHeight = MINMAX(heightConstraint, minHeight, maxHeight);

  if (isAutoWidth && newWidth != intrinsicWidth) {
    isAutoWidth = PR_FALSE;
  }
  if (isAutoHeight && newHeight != intrinsicHeight) {
    isAutoHeight = PR_FALSE;
  }

  if (isAutoWidth) {
    if (!isAutoHeight && intrinsicHeight != 0) {
       newWidth = (intrinsicWidth * newHeight) / intrinsicHeight;
    }
  } else {
    if (isAutoHeight && intrinsicWidth != 0) {
       newHeight = (intrinsicHeight * newWidth) / intrinsicWidth;
    }
  }

  if (mComputedSize.width != newWidth || mComputedSize.height != newHeight) {
    mComputedSize.SizeTo(newWidth, newHeight);
    RecalculateTransform(nsnull);
  }

  aDesiredSize.width = mComputedSize.width;
  aDesiredSize.height = mComputedSize.height;
}

void
nsImageFrame::GetInnerArea(nsIPresContext* aPresContext,
                           nsRect& aInnerArea) const
{
  aInnerArea.x = mBorderPadding.left;
  aInnerArea.y = mPrevInFlow ? 0 : mBorderPadding.top;
  aInnerArea.width = mRect.width - mBorderPadding.left - mBorderPadding.right;
  aInnerArea.height = mRect.height -
    (mPrevInFlow ? 0 : mBorderPadding.top) -
    (mNextInFlow ? 0 : mBorderPadding.bottom);
}

NS_IMETHODIMP
nsImageFrame::ContentChanged(nsIPresContext* aPresContext,
                             nsIContent*     aChild,
                             nsISupports*    aSubContent)
{
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  mState |= NS_FRAME_IS_DIRTY;
  return mParent->ReflowDirtyChild(shell, this);
}

// get the offset into the content area of the image where aImg starts if it is a continuation.
nscoord 
nsImageFrame::GetContinuationOffset(nscoord* aWidth) const
{
  nscoord offset = 0;
  if (aWidth) {
    *aWidth = 0;
  }

  if (mPrevInFlow) {
    for (nsIFrame* prevInFlow = mPrevInFlow ; prevInFlow; prevInFlow->GetPrevInFlow(&prevInFlow)) {
      nsRect rect = prevInFlow->GetRect();
      if (aWidth) {
        *aWidth = rect.width;
      }
      offset += rect.height;
    }
    offset -= mBorderPadding.top;
    offset = PR_MAX(0, offset);
  }
  return offset;
}

NS_IMETHODIMP
nsImageFrame::Reflow(nsIPresContext*          aPresContext,
                     nsHTMLReflowMetrics&     aMetrics,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsImageFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aMetrics, aStatus);
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                  ("enter nsImageFrame::Reflow: availSize=%d,%d",
                  aReflowState.availableWidth, aReflowState.availableHeight));

  NS_PRECONDITION(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  aStatus = NS_FRAME_COMPLETE;

  // see if we have a frozen size (i.e. a fixed width and height)
  if (HaveFixedSize(aReflowState)) {
    mState |= IMAGE_SIZECONSTRAINED;
  } else {
    mState &= ~IMAGE_SIZECONSTRAINED;
  }

  if (aReflowState.reason == eReflowReason_Initial) {
    mState |= IMAGE_GOTINITIALREFLOW;
  }

  // Set our borderpadding so that if GetDesiredSize has to recalc the
  // transform it can.
  mBorderPadding   = aReflowState.mComputedBorderPadding;

  // get the desired size of the complete image
  GetDesiredSize(aPresContext, aReflowState, aMetrics);

  // add borders and padding
  aMetrics.width  += mBorderPadding.left + mBorderPadding.right;
  aMetrics.height += mBorderPadding.top + mBorderPadding.bottom;
  
  if (mPrevInFlow) {
    nscoord y = GetContinuationOffset(&aMetrics.width);
    aMetrics.height -= y + mBorderPadding.top;
    aMetrics.height = PR_MAX(0, aMetrics.height);
  }


  // we have to split images if we are:
  //  in Paginated mode, we need to have a constrained height, and have a height larger than our available height
  PRBool isPaginated;
  aPresContext->IsPaginated(&isPaginated);
  PRUint32 loadStatus = imgIRequest::STATUS_NONE;
  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
  NS_ASSERTION(imageLoader, "No content node??");
  if (imageLoader) {
    nsCOMPtr<imgIRequest> currentRequest;
    imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                            getter_AddRefs(currentRequest));
    if (currentRequest) {
      currentRequest->GetImageStatus(&loadStatus);
    }
  }
  if (isPaginated &&
      ((loadStatus & imgIRequest::STATUS_SIZE_AVAILABLE) || (mState & IMAGE_SIZECONSTRAINED)) &&
      NS_UNCONSTRAINEDSIZE != aReflowState.availableHeight && 
      aMetrics.height > aReflowState.availableHeight) { 
    nsCOMPtr<nsIAtom> fType;
    GetFrameType(getter_AddRefs(fType));
    // split an image frame but not an image control frame
    if (nsLayoutAtoms::imageFrame == fType) {
      float p2t;
      aPresContext->GetScaledPixelsToTwips(&p2t);
      // our desired height was greater than 0, so to avoid infinite splitting, use 1 pixel as the min
      aMetrics.height = PR_MAX(NSToCoordRound(p2t), aReflowState.availableHeight);
      aStatus = NS_FRAME_NOT_COMPLETE;
    }
  }
  aMetrics.ascent  = aMetrics.height;
  aMetrics.descent = 0;

  if (aMetrics.mComputeMEW) {
    // If we have a percentage based width, then our MEW is 0
    if (eStyleUnit_Percent == aReflowState.mStylePosition->mWidth.GetUnit()) {
      aMetrics.mMaxElementWidth = 0;
    } else {
      aMetrics.mMaxElementWidth = aMetrics.width;
    }
  }
  
  if (aMetrics.mFlags & NS_REFLOW_CALC_MAX_WIDTH) {
    aMetrics.mMaximumWidth = aMetrics.width;
  }

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                  ("exit nsImageFrame::Reflow: size=%d,%d",
                  aMetrics.width, aMetrics.height));
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aMetrics);
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
  // Set font and color
  aRenderingContext.SetColor(GetStyleColor()->mColor);
  SetFontFromStyle(&aRenderingContext, mStyleContext);

  // Format the text to display within the formatting rect
  nsIFontMetrics* fm;
  aRenderingContext.GetFontMetrics(fm);

  nscoord maxAscent, maxDescent, height;
  fm->GetMaxAscent(maxAscent);
  fm->GetMaxDescent(maxDescent);
  fm->GetHeight(height);

  // XXX It would be nice if there was a way to have the font metrics tell
  // use where to break the text given a maximum width. At a minimum we need
  // to be able to get the break character...
  const PRUnichar* str = aAltText.get();
  PRInt32          strLen = aAltText.Length();
  nscoord          y = aRect.y;
  while ((strLen > 0) && ((y + maxDescent) < aRect.YMost())) {
    // Determine how much of the text to display on this line
    PRUint32  maxFit;  // number of characters that fit
    MeasureString(str, strLen, aRect.width, maxFit, aRenderingContext);
    
    // Display the text
    aRenderingContext.DrawString(str, maxFit, aRect.x, y + maxAscent);

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
  borderEdgeWidth = NSIntPixelsToTwips(ALT_BORDER_WIDTH, p2t);

  // if inner area is empty, then make it big enough for at least the icon
  if (inner.IsEmpty()){
    inner.SizeTo(2*(NSIntPixelsToTwips(ICON_SIZE+ICON_PADDING+ALT_BORDER_WIDTH,p2t)),
                 2*(NSIntPixelsToTwips(ICON_SIZE+ICON_PADDING+ALT_BORDER_WIDTH,p2t)));
  }

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
  inner.Deflate(NSIntPixelsToTwips(ICON_PADDING+ALT_BORDER_WIDTH, p2t), 
                NSIntPixelsToTwips(ICON_PADDING+ALT_BORDER_WIDTH, p2t));
  if (inner.IsEmpty()) {
    return;
  }

  // check if the size of the (deflated) rect is big enough to show the icon
  // - if not, then just bail, leaving the border rect by itself
  // NOTE: setting the clip (below) should be enough, but there is a bug in the Linux
  //       rendering context that images are not clipped when a clip rect is set (bugzilla bug 78497)
  //       and also this will be slightly more efficient since we will not try to render any icons
  //       or ALT text into the rect.
  if (inner.width < NSIntPixelsToTwips(ICON_SIZE, p2t) || 
      inner.height < NSIntPixelsToTwips(ICON_SIZE, p2t)) {
    return;
  }

  // Clip so we don't render outside the inner rect
  PRBool clipState;
  aRenderingContext.PushState();
  aRenderingContext.SetClipRect(inner, nsClipCombine_kIntersect, clipState);

  PRBool dispIcon = mIconLoad ? mIconLoad->mPrefShowPlaceholders : PR_TRUE;

  // Check if we should display image placeholders
  if (dispIcon) {
    PRInt32 size = NSIntPixelsToTwips(ICON_SIZE, p2t);

    PRBool iconUsed = PR_FALSE;

    // see if the icon images are present...
    if (mIconLoad && mIconLoad->mIconsLoaded) {
      // pick the correct image
      NS_ASSERTION( aIconId == NS_ICON_LOADING_IMAGE ||
                    aIconId == NS_ICON_BROKEN_IMAGE, "Invalid Icon ID in DisplayAltFeedback");

      nsCOMPtr<imgIContainer> imgCon;
      if (mIconLoad->mIconLoads[aIconId].mRequest) {
        mIconLoad->mIconLoads[aIconId].mRequest->GetImage(getter_AddRefs(imgCon));
      }
      if (imgCon) {
        // draw it
        nsPoint p(inner.x, inner.y);
        nsRect r(0,0,size,size);
        aRenderingContext.DrawImage(imgCon, &r, &p);
        iconUsed = PR_TRUE;
      }
    }

    // if we could not draw the image, then just draw some grafitti
    if (!iconUsed) {
      nscolor oldColor;
      aRenderingContext.DrawRect(0,0,size,size);
      aRenderingContext.GetColor(oldColor);
      aRenderingContext.SetColor( aIconId == NS_ICON_BROKEN_IMAGE ? NS_RGB(0xFF,0,0) : NS_RGB(0,0xFF,0));
      aRenderingContext.FillEllipse(NS_STATIC_CAST(int,size/2),NS_STATIC_CAST(int,size/2),
                                    NS_STATIC_CAST(int,(size/2)-(2*p2t)),NS_STATIC_CAST(int,(size/2)-(2*p2t)));
      aRenderingContext.SetColor(oldColor);
    }  

    // Reduce the inner rect by the width of the icon, and leave an
    // additional ICON_PADDING pixels for padding
    PRInt32 iconWidth = NSIntPixelsToTwips(ICON_SIZE + ICON_PADDING, p2t);
    inner.x += iconWidth;
    inner.width -= iconWidth;
  }

  // If there's still room, display the alt-text
  if (!inner.IsEmpty()) {
    nsIContent* content = GetContent();
    if (content) {
      nsCOMPtr<nsIAtom> tag;
      content->GetTag(getter_AddRefs(tag));
      if (tag) {
        nsAutoString altText;
        nsCSSFrameConstructor::GetAlternateTextFor(content, tag, altText);
        DisplayAltText(aPresContext, aRenderingContext, altText, inner);
      }
    }
  }

  aRenderingContext.PopState(clipState);
}

NS_METHOD
nsImageFrame::Paint(nsIPresContext*      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect&        aDirtyRect,
                    nsFramePaintLayer    aWhichLayer,
                    PRUint32             aFlags)
{
  PRBool isVisible;
  if (NS_SUCCEEDED(IsVisibleForPainting(aPresContext, aRenderingContext, PR_TRUE, &isVisible)) && 
      isVisible && mRect.width && mRect.height) {
    // If painting is suppressed, we need to stop image painting.  We
    // have to cover <img> here because of input image controls.
    PRBool paintingSuppressed = PR_FALSE;
    nsCOMPtr<nsIPresShell> shell;
    aPresContext->GetShell(getter_AddRefs(shell));
    shell->IsPaintingSuppressed(&paintingSuppressed);
    if (paintingSuppressed) {
      return NS_OK;
    }

    // First paint background and borders, which should be in the
    // FOREGROUND or BACKGROUND paint layer if the element is
    // inline-level or block-level, respectively (bug 36710).  (See
    // CSS2 9.5, which is the rationale for paint layers.)
    const nsStyleDisplay* display = GetStyleDisplay();
    nsFramePaintLayer backgroundLayer = display->IsBlockLevel()
                                            ? NS_FRAME_PAINT_LAYER_BACKGROUND
                                            : NS_FRAME_PAINT_LAYER_FOREGROUND;
    if (aWhichLayer == backgroundLayer) {
      PaintSelf(aPresContext, aRenderingContext, aDirtyRect);
    }

    nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
    NS_ASSERTION(mContent, "Not an image loading content?");

    nsCOMPtr<imgIRequest> currentRequest;
    if (imageLoader) {
      imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                              getter_AddRefs(currentRequest));
    }
      
    nsCOMPtr<imgIContainer> imgCon;

    PRUint32 loadStatus = imgIRequest::STATUS_ERROR;

    if (currentRequest) {
      currentRequest->GetImage(getter_AddRefs(imgCon));
      currentRequest->GetImageStatus(&loadStatus);
    }

    if (loadStatus & imgIRequest::STATUS_ERROR || !imgCon) {
      // No image yet, or image load failed. Draw the alt-text and an icon
      // indicating the status (unless image is blocked, in which case we show nothing)

      PRBool imageBlocked = PR_FALSE;
      if (imageLoader) {
        imageLoader->GetImageBlocked(&imageBlocked);
      }
      
      if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer &&
          (!imageBlocked || mIconLoad->mPrefAllImagesBlocked)) {
        DisplayAltFeedback(aPresContext, aRenderingContext, 
                           (loadStatus & imgIRequest::STATUS_ERROR)
                           ? NS_ICON_BROKEN_IMAGE
                           : NS_ICON_LOADING_IMAGE);
      }
    }
    else {
      PRBool paintOutline   = PR_FALSE;
      if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer && imgCon) {
        // Render the image into our content area (the area inside
        // the borders and padding)
        nsRect inner;
        GetInnerArea(aPresContext, inner);
        nsRect paintArea(inner);

        nscoord offsetY = 0; 

        // if the image is split account for y-offset
        if (mPrevInFlow) {
          offsetY = GetContinuationOffset();
        }

        if (mIntrinsicSize == mComputedSize) {
          // Find the actual rect to be painted to in the rendering context
          paintArea.IntersectRect(paintArea, aDirtyRect);

          // Point to paint to
          nsPoint p(paintArea.x, paintArea.y);
          
          // Rect in the image to paint
          nsRect r(paintArea.x - inner.x,
                   paintArea.y - inner.y + offsetY,
                   paintArea.width,
                   paintArea.height);
          
          aRenderingContext.DrawImage(imgCon, &r, &p);
        } else {
          // The computed size is the total size of all the continuations,
          // including ourselves.  Note that we're basically inverting
          // mTransform here (would it too much to ask for
          // nsTransform2D::Invert?), since we need to convert from
          // rendering context coords to image coords...
          nsTransform2D trans;
          trans.SetToScale((float(mIntrinsicSize.width) / float(mComputedSize.width)),
                           (float(mIntrinsicSize.height) / float(mComputedSize.height)));
          
          // XXXbz it looks like we should take
          // IntersectRect(paintArea, aDirtyRect) here too, but things
          // get very weird if I do that ....
          //   paintArea.IntersectRect(paintArea, aDirtyRect);
          
          // dirty rect in image our coord size...
          nsRect r(paintArea.x - inner.x,
                   paintArea.y - inner.y + offsetY,
                   paintArea.width,
                   paintArea.height);

          // Transform that to image coords
          trans.TransformCoord(&r.x, &r.y, &r.width, &r.height);
          
          // dest rect in our coords
          nsRect d(paintArea.x, paintArea.y,
                   paintArea.width, paintArea.height);
          
          aRenderingContext.DrawScaledImage(imgCon, &r, &d);
        }
        paintOutline = PR_TRUE;
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
        paintOutline = PR_TRUE;
      }

      // paint the outline in the overlay layer (or if there is an image map) until the 
      // general problem of painting it outside the border box is solved.
      if (paintOutline) {
        const nsStyleBorder* myBorder = GetStyleBorder();
        const nsStyleOutline* myOutline = GetStyleOutline();
        nsRect rect(0, 0, mRect.width, mRect.height);
        nsCSSRendering::PaintOutline(aPresContext, aRenderingContext, this,
                                     aDirtyRect, rect, *myBorder,
                                     *myOutline, mStyleContext, 0);
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

  }
  PRInt16 displaySelection = 0;

  nsresult result; 
  nsCOMPtr<nsIPresShell> shell;
  result = aPresContext->GetShell(getter_AddRefs(shell));
  if (NS_FAILED(result))
    return result;
  result = shell->GetSelectionFlags(&displaySelection);
  if (NS_FAILED(result))
    return result;
  if (!(displaySelection & nsISelectionDisplay::DISPLAY_IMAGES))
    return NS_OK;//no need to check the blue border, we cannot be drawn selected
//insert hook here for image selection drawing
#if IMAGE_EDITOR_CHECK
  //check to see if this frame is in an editor context
  //isEditor check. this needs to be changed to have better way to check
  if (displaySelection == nsISelectionDisplay::DISPLAY_ALL) 
  {
    nsCOMPtr<nsISelectionController> selCon;
    result = GetSelectionController(aPresContext, getter_AddRefs(selCon));
    if (NS_SUCCEEDED(result) && selCon)
    {
      nsCOMPtr<nsISelection> selection;
      result = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));
      if (NS_SUCCEEDED(result) && selection)
      {
        PRInt32 rangeCount;
        selection->GetRangeCount(&rangeCount);
        if (rangeCount == 1) //if not one then let code drop to nsFrame::Paint
        {
          nsCOMPtr<nsIContent> parentContent = mContent->GetParent();
          if (parentContent)
          {
            PRInt32 thisOffset;
            parentContent->IndexOf(mContent, thisOffset);
            nsCOMPtr<nsIDOMNode> parentNode = do_QueryInterface(parentContent);
            nsCOMPtr<nsIDOMNode> rangeNode;
            PRInt32 rangeOffset;
            nsCOMPtr<nsIDOMRange> range;
            selection->GetRangeAt(0,getter_AddRefs(range));
            if (range)
            {
              range->GetStartContainer(getter_AddRefs(rangeNode));
              range->GetStartOffset(&rangeOffset);

              if (parentNode && rangeNode && (rangeNode == parentNode) && rangeOffset == thisOffset)
              {
                range->GetEndContainer(getter_AddRefs(rangeNode));
                range->GetEndOffset(&rangeOffset);
                if ((rangeNode == parentNode) && (rangeOffset == (thisOffset +1))) //+1 since that would mean this whole content is selected only
                  return NS_OK; //do not allow nsFrame do draw any further selection
              }
            }
          }
        }
      }
    }
  }
#endif
  
  return nsFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer, nsISelectionDisplay::DISPLAY_IMAGES);
}

NS_IMETHODIMP
nsImageFrame::GetImageMap(nsIPresContext *aPresContext, nsIImageMap **aImageMap)
{
  nsImageMap *map = GetImageMap(aPresContext);
  return CallQueryInterface(map, aImageMap);
}

nsImageMap*
nsImageFrame::GetImageMap(nsIPresContext* aPresContext)
{
  if (!mImageMap) {
    nsIDocument* doc = mContent->GetDocument();
    if (!doc) {
      return nsnull;
    }

    nsAutoString usemap;
    mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::usemap, usemap);

    nsCOMPtr<nsIDOMHTMLMapElement> map;
    if (NS_SUCCEEDED(nsImageMapUtils::FindImageMap(doc,usemap,getter_AddRefs(map))) && map) {
      nsCOMPtr<nsIPresShell> presShell;
      aPresContext->GetShell(getter_AddRefs(presShell));

      mImageMap = new nsImageMap();
      if (mImageMap) {
        NS_ADDREF(mImageMap);
        mImageMap->Init(presShell, this, map);
      }
    }
  }

  return mImageMap;
}

void
nsImageFrame::TriggerLink(nsIPresContext* aPresContext,
                          nsIURI* aURI,
                          const nsString& aTargetSpec,
                          PRBool aClick)
{
  // We get here with server side image map
  nsCOMPtr<nsILinkHandler> handler;
  aPresContext->GetLinkHandler(getter_AddRefs(handler));
  if (nsnull != handler) {
    if (aClick) {
      nsresult proceed = NS_OK;
      // Check that this page is allowed to load this URI.
      // Almost a copy of the similarly named method in nsGenericElement
      nsresult rv;
      nsCOMPtr<nsIScriptSecurityManager> securityManager = 
               do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);

      nsCOMPtr<nsIPresShell> ps;
      if (NS_SUCCEEDED(rv)) 
        rv = aPresContext->GetShell(getter_AddRefs(ps));
      nsCOMPtr<nsIDocument> doc;
      if (NS_SUCCEEDED(rv) && ps) 
        rv = ps->GetDocument(getter_AddRefs(doc));
      
      nsCOMPtr<nsIURI> baseURI;
      if (NS_SUCCEEDED(rv) && doc) 
        doc->GetDocumentURL(getter_AddRefs(baseURI));
      
      if (NS_SUCCEEDED(rv)) 
        proceed = securityManager->CheckLoadURI(baseURI, aURI, nsIScriptSecurityManager::STANDARD);

      // Only pass off the click event if the script security manager
      // says it's ok.
      if (NS_SUCCEEDED(proceed))
        handler->OnLinkClick(mContent, eLinkVerb_Replace, aURI, aTargetSpec.get());
    }
    else {
      handler->OnOverLink(mContent, aURI, aTargetSpec.get());
    }
  }
}

PRBool
nsImageFrame::IsServerImageMap()
{
  nsAutoString ismap;
  return NS_CONTENT_ATTR_HAS_VALUE ==
    mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::ismap, ismap);
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
  if (!HasView()) {
    nsPoint offset;
    nsIView *view;
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
  for (nsIContent* content = mContent->GetParent();
       content; content = content->GetParent()) {
    nsCOMPtr<nsIDOMHTMLAnchorElement> anchor(do_QueryInterface(content));
    if (anchor) {
      anchor->GetHref(aHref);
      if (!aHref.IsEmpty()) {
        status = PR_TRUE;
      }
      anchor->GetTarget(aTarget);
      break;
    }
  }
  return status;
}

NS_IMETHODIMP
nsImageFrame::CanContinueTextRun(PRBool& aContinueTextRun) const
{
  // images really CAN continue text runs, but the textFrame needs to be 
  // educated before we can indicate that it can. For now, we handle the fixing up 
  // of max element widths in nsLineLayout::VerticalAlignFrames, but hopefully
  // this can be eliminated and the textFrame can be convinced to handle inlines
  // that take up space in text runs.

  aContinueTextRun = PR_FALSE;
  return NS_OK;
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
    PRBool inside = PR_FALSE;
    nsCOMPtr<nsIContent> area;
    inside = map->IsInside(p.x, p.y, getter_AddRefs(area),
                           absURL, target, altText);
    if (inside && area) {
      *aContent = area;
      NS_ADDREF(*aContent);
      return NS_OK;
    }
  }

  *aContent = GetContent();
  NS_IF_ADDREF(*aContent);
  return NS_OK;
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
        PRBool inside = PR_FALSE;
        // Even though client-side image map triggering happens
        // through content, we need to make sure we're not inside
        // (in case we deal with a case of both client-side and
        // sever-side on the same image - it happens!)
        if (nsnull != map) {
          nsCOMPtr<nsIContent> area;
          inside = map->IsInside(p.x, p.y, getter_AddRefs(area),
                                 absURL, target, altText);
        }

        if (!inside && isServerMap) {
          nsCOMPtr<nsIURI> baseURL;
          mContent->GetBaseURL(getter_AddRefs(baseURL));
          
          if (baseURL) {
            // Server side image maps use the href in a containing anchor
            // element to provide the basis for the destination url.
            nsAutoString src;
            if (GetAnchorHREFAndTarget(src, target)) {
              nsCOMPtr<nsINodeInfo> nodeInfo;
              mContent->GetNodeInfo(getter_AddRefs(nodeInfo));
              NS_ASSERTION(nodeInfo, "Image content without a nodeinfo?");
              nsIDocument* doc = nodeInfo->GetDocument();
              nsCAutoString charset;
              if (doc) {
                doc->GetDocumentCharacterSet(charset);
              } 
              nsCOMPtr<nsIURI> uri;
              nsresult rv = NS_NewURI(getter_AddRefs(uri), src,
                                      charset.get(),
                                      baseURL);
              NS_ENSURE_SUCCESS(rv, rv);
            
              // XXX if the mouse is over/clicked in the border/padding area
              // we should probably just pretend nothing happened. Nav4
              // keeps the x,y coordinates positive as we do; IE doesn't
              // bother. Both of them send the click through even when the
              // mouse is over the border.
              if (p.x < 0) p.x = 0;
              if (p.y < 0) p.y = 0;
              nsCAutoString spec;
              uri->GetSpec(spec);
              spec += nsPrintfCString("?%d,%d", p.x, p.y);
              uri->SetSpec(spec);                
              
              PRBool clicked = PR_FALSE;
              if (aEvent->message == NS_MOUSE_LEFT_BUTTON_UP) {
                *aEventStatus = nsEventStatus_eConsumeDoDefault; 
                clicked = PR_TRUE;
              }
              TriggerLink(aPresContext, uri, target, clicked);
            }
          } 
          else 
          {
            NS_WARNING("baseURL is null for imageFrame - ignoring mouse event");
          }
        }
      }
      break;
    }
    default:
      break;
  }

  return nsSplittableFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

//XXX This will need to be rewritten once we have content for areas
//XXXbz We have content for areas now.... 
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
      aCursor = GetStyleUserInterface()->mCursor;
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
                               PRInt32 aModType)
{
  nsresult rv = nsSplittableFrame::AttributeChanged(aPresContext, aChild,
                                                    aNameSpaceID, aAttribute,
                                                    aModType);
  if (NS_OK != rv) {
    return rv;
  }
  // XXXldb Shouldn't width and height be handled by attribute mapping?
  if (nsHTMLAtoms::width == aAttribute || nsHTMLAtoms::height == aAttribute  || nsHTMLAtoms::alt == aAttribute)
  { // XXX: could check for new width == old width, and make that a no-op
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));
    mState |= NS_FRAME_IS_DIRTY;
    mParent->ReflowDirtyChild(presShell, (nsIFrame*) this);
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

#ifdef DEBUG
NS_IMETHODIMP
nsImageFrame::List(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent) const
{
  IndentBy(out, aIndent);
  ListTag(out);
#ifdef DEBUG_waterson
  fprintf(out, " [parent=%p]", mParent);
#endif
  if (HasView()) {
    fprintf(out, " [view=%p]", GetView());
  }
  fprintf(out, " {%d,%d,%d,%d}", mRect.x, mRect.y, mRect.width, 
mRect.height);
  if (0 != mState) {
    fprintf(out, " [state=%08x]", mState);
  }
  fprintf(out, " [content=%p]", mContent);

  // output the img src url
  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
  if (imageLoader) {
    nsCOMPtr<imgIRequest> currentRequest;
    imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                            getter_AddRefs(currentRequest));
    if (currentRequest) {
      nsCOMPtr<nsIURI> uri;
      currentRequest->GetURI(getter_AddRefs(uri));
      nsCAutoString uristr;
      uri->GetAsciiSpec(uristr);
      fprintf(out, " [src=%s]", uristr.get());
    }
  }
  fputs("\n", out);
  return NS_OK;
}
#endif

NS_IMETHODIMP 
nsImageFrame::GetIntrinsicImageSize(nsSize& aSize)
{
  aSize = mIntrinsicSize;
  return NS_OK;
}

nsresult
nsImageFrame::LoadIcon(const nsAString& aSpec,
                       nsIPresContext *aPresContext,
                       imgIRequest** aRequest)
{
  nsresult rv = NS_OK;
  NS_PRECONDITION(!aSpec.IsEmpty(), "What happened??");

  nsCOMPtr<nsIURI> realURI;
  nsCOMPtr<nsIIOService> ioService;
  aPresContext->GetIOService(getter_AddRefs(ioService));
  SpecToURI(aSpec, ioService, getter_AddRefs(realURI));
 
  nsCOMPtr<imgILoader> il(do_GetService("@mozilla.org/image/loader;1", &rv));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsILoadGroup> loadGroup;
  GetLoadGroup(aPresContext, getter_AddRefs(loadGroup));

  // For icon loads, we don't need to merge with the loadgroup flags
  nsLoadFlags loadFlags = nsIRequest::LOAD_NORMAL;

  return il->LoadImage(realURI,     /* icon URI */
                       nsnull,      /* initial document URI; this is only
                                       relevant for cookies, so does not
                                       apply to icons. */
                       nsnull,      /* referrer (not relevant for icons) */
                       loadGroup,
                       mListener,
                       nsnull,      /* Not associated with any particular document */
                       loadFlags,
                       nsnull,
                       nsnull,
                       aRequest);
}

void
nsImageFrame::GetDocumentCharacterSet(nsACString& aCharset) const
{
  if (mContent) {
    NS_ASSERTION(mContent->GetDocument(),
                 "Frame still alive after content removed from document!");
    mContent->GetDocument()->GetDocumentCharacterSet(aCharset);
  }
}

void
nsImageFrame::SpecToURI(const nsAString& aSpec, nsIIOService *aIOService,
                         nsIURI **aURI)
{
  nsCOMPtr<nsIURI> baseURI;
  if (mContent) {
    mContent->GetBaseURL(getter_AddRefs(baseURI));
  }
  nsCAutoString charset;
  GetDocumentCharacterSet(charset);
  NS_NewURI(aURI, aSpec, 
            charset.IsEmpty() ? nsnull : charset.get(), 
            baseURI, aIOService);
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

nsresult nsImageFrame::LoadIcons(nsIPresContext *aPresContext)
{
  NS_NAMED_LITERAL_STRING(loadingSrc,"resource:/res/loading-image.gif"); 
  NS_NAMED_LITERAL_STRING(brokenSrc,"resource:/res/broken-image.gif");

  PRBool doLoad = PR_FALSE;  // only load icons once...

  // see if the first instance and we need to create the icon loader
  if (!mIconLoad) {
#ifdef NOISY_ICON_LOADING
      printf( "Allocating IconLoad (%p)\n", this);
#endif
    mIconLoad = new IconLoad(aPresContext, mListener);
    if (!mIconLoad) 
      return NS_ERROR_OUT_OF_MEMORY;
    doLoad = PR_TRUE;
  }
  // always addref
  mIconLoad->AddRef();

#ifdef NOISY_ICON_LOADING
  printf( "IconLoad AddRef'd(%p)\n", this);
#endif

  if (!doLoad) {
    // icons already in the pipe
    return NS_OK;
  }

  nsresult rv;
  
  // create a loader and load the images
  rv = LoadIcon(loadingSrc,
                aPresContext,
                getter_AddRefs(mIconLoad->mIconLoads[NS_ICON_LOADING_IMAGE].mRequest));
#ifdef NOISY_ICON_LOADING
  printf("Loading request %p, rv=%u\n",
         mIconLoad->mIconLoads[NS_ICON_LOADING_IMAGE].mRequest.get(), rv);
#endif

  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = LoadIcon(brokenSrc,
                aPresContext,
                getter_AddRefs(mIconLoad->mIconLoads[NS_ICON_BROKEN_IMAGE].mRequest));
#ifdef NOISY_ICON_LOADING
  printf("Loading request %p, rv=%u\n",
         mIconLoad->mIconLoads[NS_ICON_BROKEN_IMAGE].mRequest.get(), rv);
#endif

  // ImageLoader will callback into OnStartContainer, which will
  // handle the mIconsLoaded flag

  return rv;
}

PRBool nsImageFrame::HandleIconLoads(imgIRequest* aRequest, PRBool aLoaded)
{
  PRBool result = PR_FALSE;

  if (mIconLoad) {
    // check which image it is
    if (aRequest == mIconLoad->mIconLoads[NS_ICON_LOADING_IMAGE].mRequest ||
        aRequest == mIconLoad->mIconLoads[NS_ICON_BROKEN_IMAGE].mRequest) {
      result = PR_TRUE;
      mIconLoad->mIconsLoaded = aLoaded;
    }

#ifdef NOISY_ICON_LOADING
    if (mIconLoad->mIconsLoaded && result) {
      printf( "Icons Loaded: request for %s\n",
              aRequest == mIconLoad->mIconLoads[NS_ICON_LOADING_IMAGE].mRequest
              ? "NS_ICON_LOADING_IMAGE" : "NS_ICON_BROKEN_IMAGE" );
    }
#endif
  }
  
#ifdef NOISY_ICON_LOADING
  printf( "HandleIconLoads returned %s (%p)\n", result ? "TRUE" : "FALSE", this);
#endif

  return result;
}

void nsImageFrame::InvalidateIcon(nsIPresContext *aPresContext)
{
  // invalidate the inner area, where the icon lives

  NS_ASSERTION(aPresContext, "null presContext in InvalidateIcon");
  float   p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  nsRect inner;
  GetInnerArea(aPresContext, inner);

  nsRect rect(inner.x,
              inner.y,
              NSIntPixelsToTwips(ICON_SIZE+ICON_PADDING, p2t),
              NSIntPixelsToTwips(ICON_SIZE+ICON_PADDING, p2t));
  NS_ASSERTION(!rect.IsEmpty(), "icon rect cannot be empty!");
  Invalidate(aPresContext, rect, PR_FALSE);
}

void nsImageFrame::IconLoad::GetPrefs(nsIPresContext *aPresContext)
{
  NS_ASSERTION(aPresContext, "null presContext is not allowed in GetAltModePref");
  // NOTE: the presContext could be used to fetch a cached pref if needed, but is not for now

  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefBranch) {
    PRBool boolPref;
    PRInt32 intPref;
    if (NS_SUCCEEDED(prefBranch->GetBoolPref("browser.display.force_inline_alttext", &boolPref))) {
      mPrefForceInlineAltText = boolPref;
    } else {
      mPrefForceInlineAltText = PR_FALSE;
    }
    if (NS_SUCCEEDED(prefBranch->GetIntPref("network.image.imageBehavior", &intPref)) && intPref == 2) {
      mPrefAllImagesBlocked = PR_TRUE;
    } else {
      mPrefAllImagesBlocked = PR_FALSE;
    }
    if (NS_SUCCEEDED(prefBranch->GetBoolPref("browser.display.show_image_placeholders", &boolPref))) {
      mPrefShowPlaceholders = boolPref;
    } else {
      mPrefShowPlaceholders = PR_TRUE;
    }
  }
}

NS_IMPL_ISUPPORTS2(nsImageListener, imgIDecoderObserver, imgIContainerObserver)

nsImageListener::nsImageListener(nsImageFrame *aFrame) :
  mFrame(aFrame)
{
}

nsImageListener::~nsImageListener()
{
}

NS_IMETHODIMP nsImageListener::OnStartDecode(imgIRequest *aRequest)
{
  // Not useful to us yet.
  return NS_OK;
}

NS_IMETHODIMP nsImageListener::OnStartContainer(imgIRequest *aRequest,
                                                imgIContainer *aImage)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  return mFrame->OnStartContainer(aRequest, aImage);
}

NS_IMETHODIMP nsImageListener::OnStartFrame(imgIRequest *aRequest,
                                            gfxIImageFrame *aFrame)
{
  // Not useful to us yet.
  return NS_OK;
}

NS_IMETHODIMP nsImageListener::OnDataAvailable(imgIRequest *aRequest,
                                               gfxIImageFrame *aFrame,
                                               const nsRect *aRect)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  return mFrame->OnDataAvailable(aRequest, aFrame, aRect);
}

NS_IMETHODIMP nsImageListener::OnStopFrame(imgIRequest *aRequest,
                                           gfxIImageFrame *aFrame)
{
  // Not useful to us yet.
  return NS_OK;
}

NS_IMETHODIMP nsImageListener::OnStopContainer(imgIRequest *aRequest,
                                               imgIContainer *aImage)
{
  // Not useful to us yet.
  return NS_OK;
}

NS_IMETHODIMP nsImageListener::OnStopDecode(imgIRequest *aRequest,
                                            nsresult status,
                                            const PRUnichar *statusArg)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  return mFrame->OnStopDecode(aRequest, status, statusArg);
}

NS_IMETHODIMP nsImageListener::FrameChanged(imgIContainer *aContainer,
                                            gfxIImageFrame *newframe,
                                            nsRect * dirtyRect)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  return mFrame->FrameChanged(aContainer, newframe, dirtyRect);
}

