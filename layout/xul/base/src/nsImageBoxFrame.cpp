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
#include "nsPresContext.h"
#include "nsButtonFrameRenderer.h"
#include "nsBoxLayoutState.h"

#include "nsHTMLParts.h"
#include "nsString.h"
#include "nsLeafFrame.h"
#include "nsPresContext.h"
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
#include "nsContentUtils.h"

#define ONLOAD_CALLED_TOO_EARLY 1

static void PR_CALLBACK
HandleImagePLEvent(nsIContent *aContent, PRUint32 aMessage, PRUint32 aFlags)
{
  if (!aContent) {
    NS_ERROR("null node passed to HandleImagePLEvent!");

    return;
  }

  nsIDocument* doc = aContent->GetOwnerDoc();

  if (!doc) {
    return;
  }

  nsIPresShell *pres_shell = doc->GetShellAt(0);
  if (!pres_shell) {
    return;
  }

  nsCOMPtr<nsPresContext> pres_context = pres_shell->GetPresContext();
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
nsImageBoxFrame::AttributeChanged(nsPresContext* aPresContext,
                               nsIContent* aChild,
                               PRInt32 aNameSpaceID,
                               nsIAtom* aAttribute,
                               PRInt32 aModType)
{
  nsresult rv = nsLeafBoxFrame::AttributeChanged(aPresContext, aChild, aNameSpaceID, aAttribute, aModType);

  if (aAttribute == nsHTMLAtoms::src) {
    UpdateImage();
    nsBoxLayoutState state(aPresContext);
    MarkDirty(state);
  }
  else if (aAttribute == nsXULAtoms::validate)
    UpdateLoadFlags();

  return rv;
}

nsImageBoxFrame::nsImageBoxFrame(nsIPresShell* aShell) :
  nsLeafBoxFrame(aShell),
  mUseSrcAttr(PR_FALSE),
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
nsImageBoxFrame::Destroy(nsPresContext* aPresContext)
{
  // Release image loader first so that it's refcnt can go to zero
  if (mImageRequest)
    mImageRequest->Cancel(NS_ERROR_FAILURE);

  if (mListener)
    NS_REINTERPRET_CAST(nsImageBoxListener*, mListener.get())->SetFrame(nsnull); // set the frame to null so we don't send messages to a dead object.

  return nsLeafBoxFrame::Destroy(aPresContext);
}


NS_IMETHODIMP
nsImageBoxFrame::Init(nsPresContext*  aPresContext,
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

  UpdateLoadFlags();
  UpdateImage();

  return rv;
}

void
nsImageBoxFrame::UpdateImage()
{
  if (mImageRequest) {
    mImageRequest->Cancel(NS_ERROR_FAILURE);
    mImageRequest = nsnull;
    mIntrinsicSize.SizeTo(0, 0);
  }

  // get the new image src
  nsAutoString src;
  mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::src, src);
  mUseSrcAttr = !src.IsEmpty();
  if (mUseSrcAttr) {
    nsIDocument* doc = mContent->GetDocument();
    if (!doc) {
      // No need to do anything here...
      return;
    }
    nsCOMPtr<nsIURI> baseURI = mContent->GetBaseURI();
    nsCOMPtr<nsIURI> uri;
    nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(uri),
                                              src,
                                              doc,
                                              baseURI);

    if (uri && nsContentUtils::CanLoadImage(uri, mContent, doc)) {
      nsContentUtils::LoadImage(uri, doc, doc->GetDocumentURI(),
                                mListener, mLoadFlags,
                                getter_AddRefs(mImageRequest));
    }
  } else {
    // Only get the list-style-image if we aren't being drawn
    // by a native theme.
    PRUint8 appearance = GetStyleDisplay()->mAppearance;
    if (!(appearance && nsBox::gTheme && 
          nsBox::gTheme->ThemeSupportsWidget(nsnull, this, appearance))) {
      // get the list-style-image
      imgIRequest *styleRequest = GetStyleList()->mListStyleImage;
      if (styleRequest) {
        styleRequest->Clone(mListener, getter_AddRefs(mImageRequest));
      }
    }
  }
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

NS_IMETHODIMP
nsImageBoxFrame::Paint(nsPresContext*      aPresContext,
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
  if (!aDirtyRect.Intersects(rect))
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
nsImageBoxFrame::DidSetStyleContext( nsPresContext* aPresContext )
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
  nsCOMPtr<nsIURI> oldURI, newURI;
  if (mImageRequest)
    mImageRequest->GetURI(getter_AddRefs(oldURI));
  if (myList->mListStyleImage)
    myList->mListStyleImage->GetURI(getter_AddRefs(newURI));
  PRBool equal;
  if (newURI == oldURI ||   // handles null==null
      (newURI && oldURI &&
       NS_SUCCEEDED(newURI->Equals(oldURI, &equal)) && equal))
    return NS_OK;

  UpdateImage();
  return NS_OK;
} // DidSetStyleContext

void
nsImageBoxFrame::GetImageSize()
{
  if (mIntrinsicSize.width > 0 && mIntrinsicSize.height > 0) {
    mImageSize.width = mIntrinsicSize.width;
    mImageSize.height = mIntrinsicSize.height;
  } else {
    mImageSize.width = 0;
    mImageSize.height = 0;
  }
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

  nsIDocument *doc = shell->GetDocument();
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

  nscoord w, h;
  image->GetWidth(&w);
  image->GetHeight(&h);

  nsPresContext* presContext = GetPresContext();
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

