/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsImageBoxFrame.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsCOMPtr.h"
#include "nsIPresContext.h"
#include "nsButtonFrameRenderer.h"
#include "nsBoxLayoutState.h"

#include "nsHTMLParts.h"
#include "nsString.h"
#include "nsLeafFrame.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsIPresShell.h"
#include "nsIImage.h"
#include "nsIWidget.h"
#include "nsHTMLAtoms.h"
#include "nsLayoutAtoms.h"
#include "nsIDocument.h"
#include "nsIHTMLDocument.h"
#include "nsStyleConsts.h"
#include "nsImageMap.h"
#include "nsILinkHandler.h"
#include "nsIURL.h"
#include "nsILoadGroup.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsHTMLContainerFrame.h"
#include "prprf.h"
#include "nsIFontMetrics.h"
#include "nsCSSRendering.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDeviceContext.h"
#include "nsINameSpaceManager.h"
#include "nsTextFragment.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsBoxLayoutState.h"
#include "nsIDOMDocument.h"
#include "nsIEventQueueService.h"
#include "nsTransform2D.h"
#include "nsITheme.h"

#include "nsIServiceManager.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsGUIEvent.h"

#include "nsFormControlHelper.h"

#define ONLOAD_CALLED_TOO_EARLY 1

static void PR_CALLBACK
HandleImagePLEvent(nsIContent *aContent, PRUint32 aMessage, PRUint32 aFlags)
{
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(aContent));

  if (!node) {
    NS_ERROR("null or non-DOM node passed to HandleImagePLEvent!");

    return;
  }

  nsCOMPtr<nsIDOMDocument> dom_doc;
  node->GetOwnerDocument(getter_AddRefs(dom_doc));

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(dom_doc));

  if (!doc) {
    return;
  }

  nsIPresShell *pres_shell = doc->GetShellAt(0);
  if (!pres_shell) {
    return;
  }

  nsCOMPtr<nsIPresContext> pres_context;
  pres_shell->GetPresContext(getter_AddRefs(pres_context));

  if (!pres_context) {
    return;
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  nsEvent event(aMessage);

  aContent->HandleDOMEvent(pres_context, &event, nsnull, aFlags, &status);
}

static void PR_CALLBACK
HandleImageOnloadPLEvent(PLEvent *aEvent)
{
  nsIContent *content = (nsIContent *)PL_GetEventOwner(aEvent);

  HandleImagePLEvent(content, NS_IMAGE_LOAD,
                     NS_EVENT_FLAG_INIT | NS_EVENT_FLAG_CANT_BUBBLE);

  NS_RELEASE(content);
}

static void PR_CALLBACK
HandleImageOnerrorPLEvent(PLEvent *aEvent)
{
  nsIContent *content = (nsIContent *)PL_GetEventOwner(aEvent);

  HandleImagePLEvent(content, NS_IMAGE_ERROR, NS_EVENT_FLAG_INIT);

  NS_RELEASE(content);
}

static void PR_CALLBACK
DestroyImagePLEvent(PLEvent* aEvent)
{
  delete aEvent;
}

// Fire off a PLEvent that'll asynchronously call the image elements
// onload handler once handled. This is needed since the image library
// can't decide if it wants to call it's observer methods
// synchronously or asynchronously. If an image is loaded from the
// cache the notifications come back synchronously, but if the image
// is loaded from the netswork the notifications come back
// asynchronously.

void
FireDOMEvent(nsIContent* aContent, PRUint32 aMessage)
{
  static NS_DEFINE_CID(kEventQueueServiceCID,   NS_EVENTQUEUESERVICE_CID);

  nsCOMPtr<nsIEventQueueService> event_service =
    do_GetService(kEventQueueServiceCID);

  if (!event_service) {
    NS_WARNING("Failed to get event queue service");

    return;
  }

  nsCOMPtr<nsIEventQueue> event_queue;

  event_service->GetThreadEventQueue(NS_CURRENT_THREAD,
                                     getter_AddRefs(event_queue));

  if (!event_queue) {
    NS_WARNING("Failed to get event queue from service");

    return;
  }

  PLEvent *event = new PLEvent;

  if (!event) {
    // Out of memory, but none of our callers care, so just warn and
    // don't fire the event

    NS_WARNING("Out of memory?");

    return;
  }

  PLHandleEventProc f;

  switch (aMessage) {
  case NS_IMAGE_LOAD :
    f = (PLHandleEventProc)::HandleImageOnloadPLEvent;

    break;
  case NS_IMAGE_ERROR :
    f = (PLHandleEventProc)::HandleImageOnerrorPLEvent;

    break;
  default:
    NS_WARNING("Huh, I don't know how to fire this type of event?!");

    return;
  }

  PL_InitEvent(event, aContent, f, (PLDestroyEventProc)::DestroyImagePLEvent);

  // The event owns the content pointer now.
  NS_ADDREF(aContent);

  event_queue->PostEvent(event);
}

//
// NS_NewImageBoxFrame
//
// Creates a new image frame and returns it in |aNewFrame|
//
nsresult
NS_NewImageBoxFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsImageBoxFrame* it = new (aPresShell) nsImageBoxFrame (aPresShell);
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewTitledButtonFrame

NS_IMETHODIMP
nsImageBoxFrame::AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent* aChild,
                               PRInt32 aNameSpaceID,
                               nsIAtom* aAttribute,
                               PRInt32 aModType)
{
  nsresult rv = nsLeafBoxFrame::AttributeChanged(aPresContext, aChild, aNameSpaceID, aAttribute, aModType);

  PRBool aResize = UpdateAttributes(aAttribute);

  if (aResize) {
    nsBoxLayoutState state(aPresContext);
    MarkDirty(state);
  }

  return rv;
}

nsImageBoxFrame::nsImageBoxFrame(nsIPresShell* aShell) :
  nsLeafBoxFrame(aShell),
  mUseSrcAttr(PR_FALSE),
  mSizeFrozen(PR_FALSE),
  mHasImage(PR_FALSE),
  mSuppressStyleCheck(PR_FALSE),
  mIntrinsicSize(0,0),
  mLoadFlags(nsIRequest::LOAD_NORMAL)
{
  NeedsRecalc();
}

nsImageBoxFrame::~nsImageBoxFrame()
{
}


NS_IMETHODIMP
nsImageBoxFrame::NeedsRecalc()
{
  SizeNeedsRecalc(mImageSize);
  return NS_OK;
}

NS_METHOD
nsImageBoxFrame::Destroy(nsIPresContext* aPresContext)
{
  // Release image loader first so that it's refcnt can go to zero
  if (mImageRequest)
    mImageRequest->Cancel(NS_ERROR_FAILURE);

  if (mListener)
    NS_REINTERPRET_CAST(nsImageBoxListener*, mListener.get())->SetFrame(nsnull); // set the frame to null so we don't send messages to a dead object.

  return nsLeafBoxFrame::Destroy(aPresContext);
}


NS_IMETHODIMP
nsImageBoxFrame::Init(nsIPresContext*  aPresContext,
                          nsIContent*      aContent,
                          nsIFrame*        aParent,
                          nsStyleContext*  aContext,
                          nsIFrame*        aPrevInFlow)
{
  if (!mListener) {
    nsImageBoxListener *listener;
    NS_NEWXPCOM(listener, nsImageBoxListener);
    NS_ADDREF(listener);
    listener->SetFrame(this);
    listener->QueryInterface(NS_GET_IID(imgIDecoderObserver), getter_AddRefs(mListener));
    NS_RELEASE(listener);
  }

  mSuppressStyleCheck = PR_TRUE;
  nsresult  rv = nsLeafBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  mSuppressStyleCheck = PR_FALSE;

  GetImageSource();
  UpdateLoadFlags();

  UpdateImage();

  return rv;
}

void
nsImageBoxFrame::GetImageSource()
{
  // get the new image src
  nsAutoString src;
  mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::src, src);
  mUseSrcAttr = !src.IsEmpty();
  if (mUseSrcAttr) {
    nsCOMPtr<nsIURI> baseURI;
    if (mContent) {
      baseURI = mContent->GetBaseURI();
    }
    // XXX origin charset needed
    NS_NewURI(getter_AddRefs(mURI), src, nsnull, baseURI);
  } else {
    // Only get the list-style-image if we aren't being drawn
    // by a native theme.
    const nsStyleDisplay* disp = GetStyleDisplay();
    if (disp->mAppearance && nsBox::gTheme && 
        nsBox::gTheme->ThemeSupportsWidget(nsnull, this, disp->mAppearance))
      return;

    // get the list-style-image
    mURI = GetStyleList()->mListStyleImage;
  }
}

PRBool
nsImageBoxFrame::UpdateAttributes(nsIAtom* aAttribute)
{
  if (aAttribute == nsnull || aAttribute == nsHTMLAtoms::src) {
    GetImageSource();
    return UpdateImage();
  }
  else if (aAttribute == nsXULAtoms::validate)
    UpdateLoadFlags();

  return PR_FALSE;
}

void
nsImageBoxFrame::UpdateLoadFlags()
{
  nsAutoString loadPolicy;
  mContent->GetAttr(kNameSpaceID_None, nsXULAtoms::validate, loadPolicy);
  if (loadPolicy.EqualsLiteral("always"))
    mLoadFlags = nsIRequest::VALIDATE_ALWAYS;
  else if (loadPolicy.EqualsLiteral("never"))
    mLoadFlags = nsIRequest::VALIDATE_NEVER|nsIRequest::LOAD_FROM_CACHE; 
  else
    mLoadFlags = nsIRequest::LOAD_NORMAL;
}

PRBool
nsImageBoxFrame::UpdateImage()
{
  // get the new image src
  if (!mURI) {
    mSizeFrozen = PR_TRUE;
    mHasImage = PR_FALSE;

    if (mImageRequest) {
      mImageRequest->Cancel(NS_ERROR_FAILURE);
      mImageRequest = nsnull;
    }

    return PR_TRUE;
  }

  nsresult rv;
  if (mImageRequest) {
    nsCOMPtr<nsIURI> requestURI;
    rv = mImageRequest->GetURI(getter_AddRefs(requestURI));
    NS_ASSERTION(NS_SUCCEEDED(rv) && requestURI,"no request URI");
    if (NS_FAILED(rv) || !requestURI) return PR_FALSE;

    PRBool eq;
    // if the source uri and the current one are the same, return
    if (NS_SUCCEEDED(requestURI->Equals(mURI, &eq)) && eq)
      return PR_FALSE;
  }

  mSizeFrozen = PR_FALSE;
  mHasImage = PR_TRUE;

  // otherwise, we need to load the new uri
  if (mImageRequest) {
    mImageRequest->Cancel(NS_ERROR_FAILURE);
    mImageRequest = nsnull;
  }

  nsCOMPtr<imgILoader> il(do_GetService("@mozilla.org/image/loader;1", &rv));
  if (NS_FAILED(rv)) return PR_FALSE;

  nsCOMPtr<nsILoadGroup> loadGroup = GetLoadGroup();

  // Get the document URI for the referrer...
  nsIURI *documentURI = nsnull;
  nsCOMPtr<nsIDocument> doc;
  if (mContent) {
    doc = mContent->GetDocument();
    if (doc) {
      documentURI = doc->GetDocumentURI();
    }
  }

  // XXX: initialDocumentURI is NULL!
  il->LoadImage(mURI, nsnull, documentURI, loadGroup, mListener, doc,
                mLoadFlags, nsnull, nsnull, getter_AddRefs(mImageRequest));

  return PR_TRUE;
}

NS_IMETHODIMP
nsImageBoxFrame::Paint(nsIPresContext*      aPresContext,
                       nsIRenderingContext& aRenderingContext,
                       const nsRect&        aDirtyRect,
                       nsFramePaintLayer    aWhichLayer,
                       PRUint32             aFlags)
{	
  if (!GetStyleVisibility()->IsVisible())
    return NS_OK;

  nsresult rv = nsLeafBoxFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

  PaintImage(aRenderingContext, aDirtyRect, aWhichLayer);

  return rv;
}


void
nsImageBoxFrame::PaintImage(nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect,
                            nsFramePaintLayer aWhichLayer)
{
  if ((0 == mRect.width) || (0 == mRect.height)) {
    // Do not render when given a zero area. This avoids some useless
    // scaling work while we wait for our image dimensions to arrive
    // asynchronously.
    return;
  }

  nsRect rect;
  GetClientRect(rect);

  if (NS_FRAME_PAINT_LAYER_FOREGROUND != aWhichLayer)
    return;

  if (!mImageRequest)
    return;

  // don't draw if the image is not dirty
  if (!mHasImage || !aDirtyRect.Intersects(rect))
    return;

  nsCOMPtr<imgIContainer> imgCon;
  mImageRequest->GetImage(getter_AddRefs(imgCon));

  if (imgCon) {
    PRBool hasSubRect = !mUseSrcAttr && (mSubRect.width > 0 || mSubRect.height > 0);
    PRBool sizeMatch = hasSubRect ? 
                       mSubRect.width == rect.width && mSubRect.height == rect.height :
                       mImageSize.width == rect.width && mImageSize.height == rect.height;

    if (sizeMatch) {
      nsRect dest(rect);
        
      if (hasSubRect)
        rect = mSubRect;
      else {
        rect.x = 0;
        rect.y = 0;
      }

      // XXXdwh do dirty rect intersection like the HTML image frame does,
      // so that we don't always repaint the entire image!
      aRenderingContext.DrawImage(imgCon, rect, dest);
    }
    else {
      nsRect src(0, 0, mImageSize.width, mImageSize.height);
      if (hasSubRect)
        src = mSubRect;
      aRenderingContext.DrawImage(imgCon, src, rect);
    }
  }
}


//
// DidSetStyleContext
//
// When the style context changes, make sure that all of our image is up to date.
//
NS_IMETHODIMP
nsImageBoxFrame::DidSetStyleContext( nsIPresContext* aPresContext )
{
  // Fetch our subrect.
  const nsStyleList* myList = GetStyleList();
  mSubRect = myList->mImageRegion; // before |mSuppressStyleCheck| test!

  if (mUseSrcAttr || mSuppressStyleCheck)
    return NS_OK; // No more work required, since the image isn't specified by style.

  // If we're using a native theme implementation, we shouldn't draw anything.
  const nsStyleDisplay* disp = GetStyleDisplay();
  if (disp->mAppearance && nsBox::gTheme && 
      nsBox::gTheme->ThemeSupportsWidget(nsnull, this, disp->mAppearance))
    return NS_OK;

  // If list-style-image changes, we have a new image.
  nsIURI *newURI = myList->mListStyleImage;
  PRBool equal;
  if (newURI == mURI ||   // handles null==null
      (newURI && mURI && NS_SUCCEEDED(newURI->Equals(mURI, &equal)) && equal))
    return NS_OK;

  mURI = newURI;

  UpdateImage();
  return NS_OK;
} // DidSetStyleContext

void
nsImageBoxFrame::GetImageSize()
{
  nsHTMLReflowMetrics desiredSize(PR_TRUE);
  const PRInt32 kDefaultSize = 0;
  float p2t;
  GetPresContext()->GetScaledPixelsToTwips(&p2t);
  // XXX constant zero?
  const PRInt32 kDefaultSizeInTwips = NSIntPixelsToTwips(kDefaultSize, p2t);

// not calculated? Get the intrinsic size
	if (mHasImage) {
	  // get the size of the image and set the desired size
	  if (mSizeFrozen) {
			mImageSize.width = kDefaultSizeInTwips;
			mImageSize.height = kDefaultSizeInTwips;
      return;
	  } else {
      // Ask the image loader for the *intrinsic* image size
      if (mIntrinsicSize.width > 0 && mIntrinsicSize.height > 0) {
        mImageSize.width = mIntrinsicSize.width;
        mImageSize.height = mIntrinsicSize.height;
        return;
      } else {
        mImageSize.width = kDefaultSizeInTwips;
        mImageSize.height = kDefaultSizeInTwips;
        return;
      }
	  }
	}

  // XXX constant zero?
  mImageSize.width = desiredSize.width;
  mImageSize.height = desiredSize.height;
}


/**
 * Ok return our dimensions
 */
NS_IMETHODIMP
nsImageBoxFrame::GetPrefSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  if (DoesNeedRecalc(mImageSize)) {
     GetImageSize();
  }

  if (!mUseSrcAttr && (mSubRect.width > 0 || mSubRect.height > 0))
    aSize = nsSize(mSubRect.width, mSubRect.height);
  else
    aSize = mImageSize;
  AddBorderAndPadding(aSize);
  AddInset(aSize);
  nsIBox::AddCSSPrefSize(aState, this, aSize);

  nsSize minSize(0,0);
  nsSize maxSize(0,0);
  GetMinSize(aState, minSize);
  GetMaxSize(aState, maxSize);

  BoundsCheck(minSize, aSize, maxSize);

  return NS_OK;
}

NS_IMETHODIMP
nsImageBoxFrame::GetMinSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  // An image can always scale down to (0,0).
  aSize.width = aSize.height = 0;
  AddBorderAndPadding(aSize);
  AddInset(aSize);
  nsIBox::AddCSSMinSize(aState, this, aSize);
  return NS_OK;
}

NS_IMETHODIMP
nsImageBoxFrame::GetAscent(nsBoxLayoutState& aState, nscoord& aCoord)
{
  nsSize size(0,0);
  GetPrefSize(aState, size);
  aCoord = size.height;
  return NS_OK;
}

nsIAtom*
nsImageBoxFrame::GetType() const
{
  return nsLayoutAtoms::imageBoxFrame;
}

#ifdef DEBUG
NS_IMETHODIMP
nsImageBoxFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("ImageBox"), aResult);
}
#endif


already_AddRefed<nsILoadGroup>
nsImageBoxFrame::GetLoadGroup()
{
  nsIPresShell *shell = GetPresContext()->GetPresShell();
  if (!shell)
    return nsnull;

  nsCOMPtr<nsIDocument> doc;
  shell->GetDocument(getter_AddRefs(doc));
  if (!doc)
    return nsnull;

  return doc->GetDocumentLoadGroup(); // already_AddRefed
}


NS_IMETHODIMP nsImageBoxFrame::OnStartContainer(imgIRequest *request,
                                                imgIContainer *image)
{
  NS_ENSURE_ARG_POINTER(image);

  // Ensure the animation (if any) is started
  image->StartAnimation();

  mHasImage = PR_TRUE;
  mSizeFrozen = PR_FALSE;

  nscoord w, h;
  image->GetWidth(&w);
  image->GetHeight(&h);

  nsIPresContext* presContext = GetPresContext();
  float p2t = presContext->PixelsToTwips();

  mIntrinsicSize.SizeTo(NSIntPixelsToTwips(w, p2t), NSIntPixelsToTwips(h, p2t));

  nsBoxLayoutState state(presContext);
  this->MarkDirty(state);

  return NS_OK;
}

NS_IMETHODIMP nsImageBoxFrame::OnStopContainer(imgIRequest *request,
                                               imgIContainer *image)
{
  nsBoxLayoutState state(GetPresContext());
  this->Redraw(state);

  return NS_OK;
}

NS_IMETHODIMP nsImageBoxFrame::OnStopDecode(imgIRequest *request,
                                            nsresult aStatus,
                                            const PRUnichar *statusArg)
{
  if (NS_SUCCEEDED(aStatus))
    // Fire an onerror DOM event.
    FireDOMEvent(mContent, NS_IMAGE_LOAD);
  else // Fire an onload DOM event.
    FireDOMEvent(mContent, NS_IMAGE_ERROR);

  return NS_OK;
}

NS_IMETHODIMP nsImageBoxFrame::FrameChanged(imgIContainer *container,
                                            gfxIImageFrame *newframe,
                                            nsRect * dirtyRect)
{
  nsBoxLayoutState state(GetPresContext());
  this->Redraw(state);

  return NS_OK;
}

NS_IMPL_ISUPPORTS2(nsImageBoxListener, imgIDecoderObserver, imgIContainerObserver)

nsImageBoxListener::nsImageBoxListener()
{
}

nsImageBoxListener::~nsImageBoxListener()
{
}

NS_IMETHODIMP nsImageBoxListener::OnStartDecode(imgIRequest *request)
{
  return NS_OK;
}

NS_IMETHODIMP nsImageBoxListener::OnStartContainer(imgIRequest *request,
                                                   imgIContainer *image)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  return mFrame->OnStartContainer(request, image);
}

NS_IMETHODIMP nsImageBoxListener::OnStartFrame(imgIRequest *request,
                                               gfxIImageFrame *frame)
{
  return NS_OK;
}

NS_IMETHODIMP nsImageBoxListener::OnDataAvailable(imgIRequest *request,
                                                  gfxIImageFrame *frame,
                                                  const nsRect * rect)
{
  return NS_OK;
}

NS_IMETHODIMP nsImageBoxListener::OnStopFrame(imgIRequest *request,
                                              gfxIImageFrame *frame)
{
  return NS_OK;
}

NS_IMETHODIMP nsImageBoxListener::OnStopContainer(imgIRequest *request,
                                                  imgIContainer *image)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  return mFrame->OnStopContainer(request, image);
}

NS_IMETHODIMP nsImageBoxListener::OnStopDecode(imgIRequest *request,
                                               nsresult status,
                                               const PRUnichar *statusArg)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  return mFrame->OnStopDecode(request, status, statusArg);
}

NS_IMETHODIMP nsImageBoxListener::FrameChanged(imgIContainer *container,
                                               gfxIImageFrame *newframe,
                                               nsRect * dirtyRect)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  return mFrame->FrameChanged(container, newframe, dirtyRect);
}

