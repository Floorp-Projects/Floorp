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
#include "nsString.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsIPresShell.h"
#include "nsHTMLIIDs.h"
#include "nsIImage.h"
#include "nsIWidget.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLContent.h"
#include "nsIDocument.h"
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
#include "nsIPref.h"

#ifdef DEBUG
#undef NOISY_IMAGE_LOADING
#undef NOISY_ICON_LOADING
#else
#undef NOISY_IMAGE_LOADING
#undef NOISY_ICON_LOADING
#endif


// Value's for mSuppress
#define SUPPRESS_UNSET   0
#define DONT_SUPPRESS    1
#define SUPPRESS         2
#define DEFAULT_SUPPRESS 3

// values for image loading icons
#define NS_ICON_LOADING_IMAGE (0)
#define NS_ICON_BROKEN_IMAGE  (1)

// Default alignment value (so we can tell an unset value from a set value)
#define ALIGN_UNSET PRUint8(-1)

// static icon information: initialized in Init and de-initialized in Destroy
// - created if null, deleted if refcount goes to 0
nsImageFrame::IconLoad* nsImageFrame::mIconLoad = nsnull;

// test if the width and height are fixed, looking at the style data
static PRBool HaveFixedSize(const struct nsStylePosition& aStylePosition)
{
  // check the width and height values in the reflow state's style struct
  // - if width and height are specified as either coord or percentage, then
  //   the size of the image frame is constrained
  nsStyleUnit widthUnit = aStylePosition.mWidth.GetUnit();
  nsStyleUnit heightUnit = aStylePosition.mHeight.GetUnit();

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
  return HaveFixedSize(*(aReflowState.mStylePosition)); 
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


int
nsImageFrame::GetImageLoad(imgIRequest *aRequest)
{
  if (aRequest == mLoads[0].mRequest)
    return 0;
  else if (aRequest == mLoads[1].mRequest)
    return 1;

  NS_ASSERTION((aRequest == mLoads[0].mRequest &&
               aRequest == mLoads[1].mRequest), "Failure to figure out which imgIRequest this is!");

  return -1;
}

nsImageFrame::nsImageFrame() :
  mIntrinsicSize(0, 0),
  mGotInitialReflow(PR_FALSE),
  mFailureReplace(PR_TRUE)
{
  // Size is constrained if we have a width and height. 
  // - Set in reflow in case the attributes are changed
  mSizeConstrained = PR_FALSE;
}

nsImageFrame::~nsImageFrame()
{
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
  if(mImageMap) {
    mImageMap->Destroy();
    NS_RELEASE(mImageMap);
  }

  for (int i=0; i != 2; ++i) {
    if (mLoads[i].mRequest) {
      mLoads[i].mRequest->Cancel(NS_ERROR_FAILURE);
      mLoads[i].mRequest = nsnull;
    }
  }

  // set the frame to null so we don't send messages to a dead object.
  if (mListener)
    NS_REINTERPRET_CAST(nsImageListener*, mListener.get())->SetFrame(nsnull);

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
 

  return nsSplittableFrame::Destroy(aPresContext);
}



NS_IMETHODIMP
nsImageFrame::Init(nsIPresContext*  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsSplittableFrame::Init(aPresContext, aContent, aParent,
                                         aContext, aPrevInFlow);
  // See if we have a SRC attribute
  nsAutoString src;
  nsresult ca;
  ca = mContent->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::src, src);
  if ((NS_CONTENT_ATTR_HAS_VALUE != ca) || (src.Length() == 0))
  {
    // Let's see if this is an object tag and we have a DATA attribute
    nsIAtom*  tag;
    mContent->GetTag(tag);

    if(tag == nsHTMLAtoms::object)
      mContent->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::data, src);
  
    NS_IF_RELEASE(tag);
  }

  mInitialLoadCompleted = PR_FALSE;
  mCanSendLoadEvent = PR_TRUE;

  LoadIcons(aPresContext);

  mLoads[0].mRequest = do_CreateInstance("@mozilla.org/image/request;1");
  rv = LoadImage(src, aPresContext, mLoads[0].mRequest); // if the image was found in the cache,
                                                         // it is possible that LoadImage will result in
                                                         // a call to OnStartContainer()

  return rv;
}


NS_IMETHODIMP nsImageFrame::OnStartDecode(imgIRequest *aRequest, nsIPresContext *aPresContext)
{
  return NS_OK;
}

NS_IMETHODIMP nsImageFrame::OnStartContainer(imgIRequest *aRequest, nsIPresContext *aPresContext, imgIContainer *aImage)
{
  if (!aImage) return NS_ERROR_INVALID_ARG;

  mInitialLoadCompleted = PR_TRUE;

  // handle iconLoads first...
  if (HandleIconLoads(aRequest, PR_FALSE)) {
    return NS_OK;
  }

  int whichLoad = GetImageLoad(aRequest);
  if (whichLoad == -1) return NS_ERROR_FAILURE;
  struct ImageLoad *load= &mLoads[whichLoad];

  if (aImage)
  {
    /* Get requested animation policy from the pres context:
     *   normal = 0
     *   one frame = 1
     *   one loop = 2
     */
    nsImageAnimation animateMode = eImageAnimation_Normal; //default value
    nsresult rv = aPresContext->GetImageAnimationMode(&animateMode);
    if (NS_SUCCEEDED(rv))
      aImage->SetAnimationMode(animateMode);
  }

  nscoord w, h;
  aImage->GetWidth(&w);
  aImage->GetHeight(&h);

  float p2t;
  aPresContext->GetPixelsToTwips(&p2t);

  nsSize newsize(NSIntPixelsToTwips(w, p2t), NSIntPixelsToTwips(h, p2t));

  if (load->mIntrinsicSize != newsize) {
    load->mIntrinsicSize = newsize;

    if (load->mIntrinsicSize.width != 0 && load->mIntrinsicSize.height != 0)
      load->mTransform.SetToScale((float(mComputedSize.width) / float(load->mIntrinsicSize.width)),
                                  (float(mComputedSize.height) / float(load->mIntrinsicSize.height)));

    if (!mSizeConstrained) {
      nsCOMPtr<nsIPresShell> presShell;
      aPresContext->GetShell(getter_AddRefs(presShell));
      NS_ASSERTION(mParent, "No parent to pass the reflow request up to.");
      NS_ASSERTION(presShell, "No PresShell.");
      if (mParent && presShell && mGotInitialReflow && whichLoad == 0) { // don't reflow if we havn't gotten the inital reflow yet
        mState |= NS_FRAME_IS_DIRTY;
        mParent->ReflowDirtyChild(presShell, NS_STATIC_CAST(nsIFrame*, this));
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsImageFrame::OnStartFrame(imgIRequest *aRequest, nsIPresContext *aPresContext, gfxIImageFrame *aFrame)
{
  return NS_OK;
}

NS_IMETHODIMP nsImageFrame::OnDataAvailable(imgIRequest *aRequest, nsIPresContext *aPresContext, gfxIImageFrame *aFrame, const nsRect *aRect)
{
  // XXX do we need to make sure that the reflow from the OnStartContainer has been
  // processed before we start calling invalidate

  if (!aRect)
    return NS_ERROR_NULL_POINTER;

  // handle iconLoads first...
  if (HandleIconLoads(aRequest, PR_FALSE)) {
    if (!aRect->IsEmpty()) {
      Invalidate(aPresContext, *aRect, PR_FALSE);
    }
    return NS_OK;
  }

  int whichLoad = GetImageLoad(aRequest);
  if (whichLoad == -1) return NS_ERROR_FAILURE;
  if (whichLoad != 0) return NS_OK;
  struct ImageLoad *load= &mLoads[whichLoad];


  nsRect r(aRect->x, aRect->y, aRect->width, aRect->height);

  float p2t;
  aPresContext->GetPixelsToTwips(&p2t);
  r.x = NSIntPixelsToTwips(r.x, p2t);
  r.y = NSIntPixelsToTwips(r.y, p2t);
  r.width = NSIntPixelsToTwips(r.width, p2t);
  r.height = NSIntPixelsToTwips(r.height, p2t);

  load->mTransform.TransformCoord(&r.x, &r.y, &r.width, &r.height);

  r.x += mBorderPadding.left;
  r.y += mBorderPadding.top;

  if (whichLoad == 0 && !r.IsEmpty())
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

  nsCOMPtr<nsIPresShell> pres_shell;
  doc->GetShellAt(0, getter_AddRefs(pres_shell));

  if (!pres_shell) {
    return;
  }

  nsCOMPtr<nsIPresContext> pres_context;
  pres_shell->GetPresContext(getter_AddRefs(pres_context));

  if (!pres_context) {
    return;
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  nsEvent event;
  event.eventStructType = NS_EVENT;
  event.message = aMessage;

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
nsImageFrame::FireDOMEvent(PRUint32 aMessage)
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

  PL_InitEvent(event, mContent, f, (PLDestroyEventProc)::DestroyImagePLEvent);

  // The event owns the content pointer now.
  NS_ADDREF(mContent);

  event_queue->PostEvent(event);
}


NS_IMETHODIMP nsImageFrame::OnStopDecode(imgIRequest *aRequest, nsIPresContext *aPresContext, nsresult aStatus, const PRUnichar *aStatusArg)
{
  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));

  // check to see if an image error occurred
  PRBool imageFailedToLoad = PR_FALSE;

  // handle iconLoads first...
  if (HandleIconLoads(aRequest, NS_SUCCEEDED(aStatus))) {
    return NS_OK;
  }

  int whichLoad = GetImageLoad(aRequest);

  if (whichLoad == -1) return NS_ERROR_FAILURE;

  if (whichLoad == 1) {
    if (NS_SUCCEEDED(aStatus)) {
      if (mLoads[0].mRequest) {
        mLoads[0].mRequest->Cancel(NS_ERROR_FAILURE);
      }

      mLoads[0].mRequest = mLoads[1].mRequest;
      mLoads[0].mIntrinsicSize = mLoads[1].mIntrinsicSize;
      // XXX i don't think we always want to set this.
      mLoads[0].mTransform = mLoads[1].mTransform;
      struct ImageLoad *load= &mLoads[0];

      mLoads[1].mRequest = nsnull;

      if (!mSizeConstrained && (mLoads[0].mIntrinsicSize != mIntrinsicSize)) {
        nsCOMPtr<nsIPresShell> presShell;
        aPresContext->GetShell(getter_AddRefs(presShell));
        NS_ASSERTION(mParent, "No parent to pass the reflow request up to.");
        NS_ASSERTION(presShell, "No PresShell.");
        if (mParent && presShell && mGotInitialReflow) { // don't reflow if we havn't gotten the inital reflow yet
          mState |= NS_FRAME_IS_DIRTY;
          mParent->ReflowDirtyChild(presShell, NS_STATIC_CAST(nsIFrame*, this));
        }
      } else {
        nsSize s;
        GetSize(s);
        nsRect r(0,0, s.width, s.height);
        if (!r.IsEmpty()) {
          Invalidate(aPresContext, r, PR_FALSE);
        }
      }

    } else {
      mLoads[1].mRequest = nsnull;
    }

  } else if (NS_FAILED(aStatus)) {

    if (whichLoad == 0 || !mLoads[0].mRequest) {
      imageFailedToLoad = PR_TRUE;
    }

  }

  // if src failed to load, determine how to handle it: 
  //  - either render the ALT text in this frame, or let the presShell handle it
  if (imageFailedToLoad && presShell) {
    if (mFailureReplace) {
      // first check for image map
      nsAutoString usemap;
      mContent->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::usemap, usemap);    
      if (usemap.IsEmpty()) {
        // check if we want to honor the ALT text in the IMG frame, or let the preShell make it into inline text
        //  - if QuirksMode, and the IMG has a size, then render the ALT text in the ING frame
        //    UNLESS there is a pref set to force inline alt text
        PRBool useSizedBox = PR_FALSE;
        PRBool prefForceInlineAltText = mIconLoad ? mIconLoad->mPrefForceInlineAltText : PR_FALSE;

        // check if we have fixed size
        const nsStylePosition* stylePosition =
          (const nsStylePosition*)mStyleContext->GetStyleData(eStyleStruct_Position);
        NS_ASSERTION(stylePosition, "null style position: frame is corrupted");
        
        // check for quirks mode
        nsCompatibility mode;
        aPresContext->GetCompatibilityMode(&mode);
        
        // wrap it all up
        useSizedBox = !prefForceInlineAltText &&
                      HaveFixedSize(*stylePosition) && 
                      mode == eCompatibility_NavQuirks;

        if (!useSizedBox) {
          // let the presShell handle converting this into the inline alt text frame
          presShell->CantRenderReplacedElement(aPresContext, this);
        } else {
          // we are handling it
          // invalidate the icon area (it may change states)   
          InvalidateIcon(aPresContext);
        }
      }
    }
    mFailureReplace = PR_TRUE;
  }

  // After these DOM events are fired its possible that this frame may be deleted.  As a result
  // the code should not attempt to access any of the frames internal data after this point.
  if (presShell) {
    if (imageFailedToLoad) {
      // Send error event
      FireDOMEvent(NS_IMAGE_ERROR);
    } else if (mCanSendLoadEvent) {
      // Send load event
      mCanSendLoadEvent = PR_FALSE;

      FireDOMEvent(NS_IMAGE_LOAD);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsImageFrame::FrameChanged(imgIContainer *aContainer, nsIPresContext *aPresContext, gfxIImageFrame *aNewFrame, nsRect *aDirtyRect)
{
  if (!mLoads[0].mRequest)
    return NS_OK; // if mLoads[0].mRequest is null, this isn't for the first one, so we don't care about it.

  nsCOMPtr<imgIContainer> con;
  mLoads[0].mRequest->GetImage(getter_AddRefs(con));
  if (aContainer == con.get()) {
    nsRect r(*aDirtyRect);

    float p2t;
    aPresContext->GetPixelsToTwips(&p2t);
    r.x = NSIntPixelsToTwips(r.x, p2t);
    r.y = NSIntPixelsToTwips(r.y, p2t);
    r.width = NSIntPixelsToTwips(r.width, p2t);
    r.height = NSIntPixelsToTwips(r.height, p2t);

    mLoads[0].mTransform.TransformCoord(&r.x, &r.y, &r.width, &r.height);
    if (!r.IsEmpty()) {
      Invalidate(aPresContext, r, PR_FALSE);
    }
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

  // if mLoads[0].mIntrinsicSize.width and height are 0, then we should check to see
  // if the size is already known by the image container.
  if (mLoads[0].mIntrinsicSize.width == 0 && mLoads[0].mIntrinsicSize.height == 0) {
    if (mLoads[0].mRequest) {
      nsCOMPtr<imgIContainer> con;
      mLoads[0].mRequest->GetImage(getter_AddRefs(con));
      if (con) {
        con->GetWidth(&mLoads[0].mIntrinsicSize.width);
        con->GetHeight(&mLoads[0].mIntrinsicSize.height);
      }
    }
  }
  mIntrinsicSize = mLoads[0].mIntrinsicSize;

  PRBool haveComputedSize = PR_FALSE;
  PRBool needIntrinsicImageSize = PR_FALSE;

  float t2p, sp2t;
  aPresContext->GetTwipsToPixels(&t2p);
  aPresContext->GetScaledPixelsToTwips(&sp2t);

  // convert from normal twips to scaled twips (printing...)
  float t2st = t2p*sp2t; // twips to scaled twips
  nscoord intrinsicScaledWidth = NSToCoordRound(float(mIntrinsicSize.width) * t2st);
  nscoord intrinsicScaledHeight = NSToCoordRound(float(mIntrinsicSize.height) * t2st);

  nscoord newWidth=0, newHeight=0;
  if (fixedContentWidth) {
    newWidth = MINMAX(widthConstraint, minWidth, maxWidth);
    if (fixedContentHeight) {
      newHeight = MINMAX(heightConstraint, minHeight, maxHeight);
      haveComputedSize = PR_TRUE;
    } else {
      // We have a width, and an auto height. Compute height from
      // width once we have the intrinsic image size.
      if (intrinsicScaledWidth != 0) {
        newHeight = (intrinsicScaledHeight * newWidth) / intrinsicScaledWidth;
        haveComputedSize = PR_TRUE;
      } else {
        newHeight = 0;
        needIntrinsicImageSize = PR_TRUE;
      }
    }
  } else if (fixedContentHeight) {
    // We have a height, and an auto width. Compute width from height
    // once we have the intrinsic image size.
    newHeight = MINMAX(heightConstraint, minHeight, maxHeight);
    if (intrinsicScaledHeight != 0) {
      newWidth = (intrinsicScaledWidth * newHeight) / intrinsicScaledHeight;
      haveComputedSize = PR_TRUE;
    } else {
      newWidth = 0;
      needIntrinsicImageSize = PR_TRUE;
    }
  } else {
    // auto size the image
    if (mIntrinsicSize.width == 0 && mIntrinsicSize.height == 0)
      needIntrinsicImageSize = PR_TRUE;
    else
      haveComputedSize = PR_TRUE;

    newWidth = intrinsicScaledWidth;
    newHeight = intrinsicScaledHeight;
  }

  mComputedSize.width = newWidth;
  mComputedSize.height = newHeight;

  nsTransform2D *transform = &mLoads[0].mTransform;

  if (mComputedSize == mIntrinsicSize) {
    transform->SetToIdentity();
  } else {
    if (mIntrinsicSize.width != 0 && mIntrinsicSize.height != 0) {
      transform->SetToScale(float(mComputedSize.width) / float(mIntrinsicSize.width),
                            float(mComputedSize.height) / float(mIntrinsicSize.height));
    }
  }

  aDesiredSize.width = mComputedSize.width;
  aDesiredSize.height = mComputedSize.height;
}

void
nsImageFrame::GetInnerArea(nsIPresContext* aPresContext,
                           nsRect& aInnerArea) const
{
  aInnerArea.x = mBorderPadding.left;
  aInnerArea.y = mBorderPadding.top;
  aInnerArea.width = mRect.width -
    (mBorderPadding.left + mBorderPadding.right);
  aInnerArea.height = (!mPrevInFlow && !mNextInFlow) 
    ? mRect.height - mBorderPadding.top - mBorderPadding.bottom : mComputedSize.height;
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
      nsRect rect;
      prevInFlow->GetRect(rect);
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
  mSizeConstrained = HaveFixedSize(aReflowState);

  if (aReflowState.reason == eReflowReason_Initial)
    mGotInitialReflow = PR_TRUE;

  // get the desired size of the complete image
  GetDesiredSize(aPresContext, aReflowState, aMetrics);

  // add borders and padding
  mBorderPadding   = aReflowState.mComputedBorderPadding;
  aMetrics.width  += mBorderPadding.left + mBorderPadding.right;
  aMetrics.height += mBorderPadding.top + mBorderPadding.bottom;
  
  if (mPrevInFlow) {
    nscoord y = GetContinuationOffset(&aMetrics.width);
    aMetrics.height -= y + mBorderPadding.top;
    aMetrics.height = PR_MAX(0, aMetrics.height);
  }

  if ((NS_UNCONSTRAINEDSIZE != aReflowState.availableHeight) && 
      (aMetrics.height > aReflowState.availableHeight)) { 
    nsCOMPtr<nsIAtom> fType;
    GetFrameType(getter_AddRefs(fType));
    // split an image frame but not an image control frame
    if (nsLayoutAtoms::imageFrame == fType.get()) {
      aMetrics.height = aReflowState.availableHeight;
      aStatus = NS_FRAME_NOT_COMPLETE;
    }
  }
  aMetrics.ascent  = aMetrics.height;
  aMetrics.descent = 0;

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

#define ICON_SIZE        (16)
#define ICON_PADDING     (6)
#define ALT_BORDER_WIDTH (1)

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

  // Clip so we don't render outside the inner rect
  PRBool clipState;
  aRenderingContext.PushState();
  aRenderingContext.SetClipRect(inner, nsClipCombine_kIntersect, clipState);

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
      r.x -= mBorderPadding.left;
      r.y -= mBorderPadding.top;
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

  // If there's still room, display the alt-text
  if (!inner.IsEmpty()) {
    nsCOMPtr<nsIContent> content;
    nsCOMPtr<nsIAtom>    tag;
    nsAutoString         altText;

    GetContent(getter_AddRefs(content));
    if (content) {
      content->GetTag(*getter_AddRefs(tag));
      if (tag) {
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
    if (paintingSuppressed)
      return NS_OK;


    // First paint background and borders
    if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
      const nsStyleVisibility* vis = 
        (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
      if (vis->IsVisibleOrCollapsed()) {
        const nsStyleBackground* myColor = (const nsStyleBackground*)
          mStyleContext->GetStyleData(eStyleStruct_Background);
        const nsStyleBorder* myBorder = (const nsStyleBorder*)
          mStyleContext->GetStyleData(eStyleStruct_Border);
        const nsStyleOutline* myOutline = (const nsStyleOutline*)
          mStyleContext->GetStyleData(eStyleStruct_Outline);
        nsRect rect(0, 0, mRect.width, mRect.height);
        nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                        aDirtyRect, rect, *myColor, *myBorder, 0, 0);

        nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect, *myBorder, mStyleContext, 0);

        nsCSSRendering::PaintOutline(aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect, *myBorder, *myOutline, mStyleContext, 0);
      }
    }

    nsCOMPtr<imgIContainer> imgCon;

    PRUint32 loadStatus = imgIRequest::STATUS_ERROR;

    if (mLoads[0].mRequest) {
      mLoads[0].mRequest->GetImage(getter_AddRefs(imgCon));
      mLoads[0].mRequest->GetImageStatus(&loadStatus);
    }

    if (loadStatus & imgIRequest::STATUS_ERROR || !imgCon) {
      // No image yet, or image load failed. Draw the alt-text and an icon
      // indicating the status
#ifndef SUPPRESS_LOADING_ICON
      if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
#else
      if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer &&
          mInitialLoadCompleted) {
#endif
        DisplayAltFeedback(aPresContext, aRenderingContext, 
                           (loadStatus & imgIRequest::STATUS_ERROR)
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

        if (loadStatus & imgIRequest::STATUS_ERROR) {
          if (imgCon) {
            inner.SizeTo(mComputedSize);
          }
        }

        if (imgCon) {
          nscoord offsetY = 0; 
          nsSize  size(inner.width, inner.height);
          // if the image is split account for y-offset, border, padding
          if (mPrevInFlow || mNextInFlow) {
            if (mPrevInFlow) {
              offsetY = GetContinuationOffset();
            }
            size.height = mRect.height;
            if (!mPrevInFlow) {
              size.height -= mBorderPadding.top;
            }
            if (!mNextInFlow) {
              size.height -= mBorderPadding.bottom;
            }
          }

          if (mLoads[0].mIntrinsicSize == mComputedSize) {
            nsPoint p(inner.x, inner.y);
            inner.IntersectRect(inner, aDirtyRect);
            nsRect r(inner.x, inner.y, inner.width, inner.height);
            r.x -= mBorderPadding.left;
            r.y -= mBorderPadding.top;
            if (mPrevInFlow) {
              r.y = offsetY;
              p.y = 0;
            }
            aRenderingContext.DrawImage(imgCon, &r, &p);
          } else {
            nsTransform2D trans;
            trans.SetToScale((float(mLoads[0].mIntrinsicSize.width) / float(inner.width)),
                             (float(mLoads[0].mIntrinsicSize.height) / float(inner.height)));

            nsRect r(0, offsetY, size.width, size.height);

            trans.TransformCoord(&r.x, &r.y, &r.width, &r.height);

            nsRect d(inner.x, inner.y, mComputedSize.width, mComputedSize.height);
            // if the image is split account for y-offset, border, padding
            if (mPrevInFlow || mNextInFlow) {
              if (mPrevInFlow) {
                d.y -= mBorderPadding.top;
              }
              d.height = size.height;
            }

            aRenderingContext.DrawScaledImage(imgCon, &r, &d);
          }
        }
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

  }
  
  return nsFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
}

nsImageMap*
nsImageFrame::GetImageMap(nsIPresContext* aPresContext)
{
  if (!mImageMap) {
    nsCOMPtr<nsIDocument> doc;
    mContent->GetDocument(*getter_AddRefs(doc));
    if (!doc) {
      return nsnull;
    }

    nsAutoString usemap;
    mContent->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::usemap, usemap);

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
      nsCOMPtr<nsIURI> absURI;
      if (NS_SUCCEEDED(rv)) 
        rv = NS_NewURI(getter_AddRefs(absURI), aURLSpec, baseURI);
      
      if (NS_SUCCEEDED(rv)) 
        proceed = securityManager->CheckLoadURI(baseURI, absURI, nsIScriptSecurityManager::STANDARD);

      // Only pass off the click event if the script security manager
      // says it's ok.
      if (NS_SUCCEEDED(proceed))
        handler->OnLinkClick(mContent, eLinkVerb_Replace, aURLSpec.get(), aTargetSpec.get());
    }
    else {
      handler->OnOverLink(mContent, aURLSpec.get(), aTargetSpec.get());
    }
    NS_RELEASE(handler);
  }
}

PRBool
nsImageFrame::IsServerImageMap()
{
  nsAutoString ismap;
  return NS_CONTENT_ATTR_HAS_VALUE ==
    mContent->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::ismap, ismap);
}

PRIntn
nsImageFrame::GetSuppress()
{
  nsAutoString s;
  if (NS_CONTENT_ATTR_HAS_VALUE ==
      mContent->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::suppress, s)) {
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
          nsCOMPtr<nsIURI> baseURL;
          GetBaseURI(getter_AddRefs(baseURL));
          
          if (baseURL) {
            // Server side image maps use the href in a containing anchor
            // element to provide the basis for the destination url.
            nsAutoString src;
            if (GetAnchorHREFAndTarget(src, target)) {
              NS_MakeAbsoluteURI(absURL, src, baseURL);
            
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
      const nsStyleUserInterface* styleUserInterface;
      GetStyleData(eStyleStruct_UserInterface, (const nsStyleStruct*&)styleUserInterface);
      aCursor = styleUserInterface->mCursor;
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
                               PRInt32 aModType, 
                               PRInt32 aHint)
{
  nsresult rv = nsSplittableFrame::AttributeChanged(aPresContext, aChild,
                                                    aNameSpaceID, aAttribute, aModType, aHint);
  if (NS_OK != rv) {
    return rv;
  }
  if (nsHTMLAtoms::src == aAttribute) {
    nsAutoString newSRC;
    aChild->GetAttr(kNameSpaceID_None, nsHTMLAtoms::src, newSRC);

    PRUint32 loadStatus = imgIRequest::STATUS_ERROR;

    if (mLoads[0].mRequest)
      mLoads[0].mRequest->GetImageStatus(&loadStatus);

    if (!(loadStatus & imgIRequest::STATUS_SIZE_AVAILABLE)) {
      if (mLoads[0].mRequest) {
        mFailureReplace = PR_FALSE; // don't cause a CantRenderReplacedElement call
        mLoads[0].mRequest->Cancel(NS_ERROR_FAILURE);
        mLoads[0].mRequest = nsnull;
      }

      mCanSendLoadEvent = PR_TRUE;
    }

    if (mLoads[1].mRequest) {
      mLoads[1].mRequest->Cancel(NS_ERROR_FAILURE);
      mLoads[1].mRequest = nsnull;
    }
    
    nsCOMPtr<imgIRequest> req(do_CreateInstance("@mozilla.org/image/request;1"));
    if (!mLoads[0].mRequest) {
      mLoads[0].mRequest = req;
    } else {
      mLoads[1].mRequest = req;
    }

    LoadImage(newSRC, aPresContext, req);
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

#ifdef DEBUG
NS_IMETHODIMP
nsImageFrame::List(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent) const
{
  IndentBy(out, aIndent);
  ListTag(out);
#ifdef DEBUG_waterson
  fprintf(out, " [parent=%p]", mParent);
#endif
  nsIView*  view;
  GetView(aPresContext, &view);
  if (view) {
    fprintf(out, " [view=%p]", view);
  }
  fprintf(out, " {%d,%d,%d,%d}", mRect.x, mRect.y, mRect.width, 
mRect.height);
  if (0 != mState) {
    fprintf(out, " [state=%08x]", mState);
  }
  fprintf(out, " [content=%p]", mContent);

  // output the img src url
  nsCOMPtr<nsIURI> uri;
  mLoads[0].mRequest->GetURI(getter_AddRefs(uri));
  nsXPIDLCString uristr;
  uri->GetSpec(getter_Copies(uristr));
  fprintf(out, " [src=%s]", uristr.get());

  fputs("\n", out);
  return NS_OK;
}
#endif

NS_IMETHODIMP 
nsImageFrame::GetIntrinsicImageSize(nsSize& aSize)
{
  aSize = mLoads[0].mIntrinsicSize;
  return NS_OK;
}

NS_IMETHODIMP
nsImageFrame::GetNaturalImageSize(PRUint32 *naturalWidth, 
                                  PRUint32 *naturalHeight)
{ 
  *naturalWidth = 0;
  *naturalHeight = 0;

  if (mLoads[0].mRequest) {
    nsCOMPtr<imgIContainer> container;
    mLoads[0].mRequest->GetImage(getter_AddRefs(container));
    if (container) {
      PRInt32 w, h;
      container->GetWidth(&w);
      container->GetHeight(&h);

      *naturalWidth = NS_STATIC_CAST(PRUint32, w);
      *naturalHeight = NS_STATIC_CAST(PRUint32, h);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsImageFrame::GetImageRequest(imgIRequest **aRequest)
{
  *aRequest = mLoads[0].mRequest;
  NS_IF_ADDREF(*aRequest);
  return NS_OK;
}

NS_IMETHODIMP 
nsImageFrame::IsImageComplete(PRBool* aComplete)
{
  NS_ENSURE_ARG_POINTER(aComplete);
  if (!mLoads[0].mRequest) {
    *aComplete = PR_FALSE;
    return NS_OK;
  }

  PRUint32 status;
  mLoads[0].mRequest->GetImageStatus(&status);
  *aComplete = ((status & imgIRequest::STATUS_LOAD_COMPLETE) != 0);

  return NS_OK;
}

nsresult
nsImageFrame::LoadImage(const nsAReadableString& aSpec, nsIPresContext *aPresContext, imgIRequest *aRequest)
{
  nsresult rv = RealLoadImage(aSpec, aPresContext, aRequest);

  if (NS_FAILED(rv)) {
    int whichLoad = GetImageLoad(aRequest);
    if (whichLoad == -1) return NS_ERROR_FAILURE;
    mLoads[whichLoad].mRequest = nsnull;
  }

  return rv;
}

nsresult
nsImageFrame::RealLoadImage(const nsAReadableString& aSpec, nsIPresContext *aPresContext, imgIRequest *aRequest)
{
  nsresult rv = NS_OK;

  /* set this to TRUE here in case we return early */
  mInitialLoadCompleted = PR_TRUE;

  /* don't load the image if aSpec is empty */
  if (aSpec.IsEmpty()) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIURI> realURI;

  /* don't load the image if some security check fails... */
  GetRealURI(aSpec, getter_AddRefs(realURI));
  if (!CanLoadImage(realURI)) return NS_ERROR_FAILURE;

  if (!mListener) {
    nsImageListener *listener = new nsImageListener(this);
    if (!listener) return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(listener);
    listener->QueryInterface(NS_GET_IID(imgIDecoderObserver), getter_AddRefs(mListener));
    NS_ASSERTION(mListener, "queryinterface for the listener failed");
    NS_RELEASE(listener);
  }

  nsCOMPtr<imgILoader> il(do_GetService("@mozilla.org/image/loader;1", &rv));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsILoadGroup> loadGroup;
  GetLoadGroup(aPresContext, getter_AddRefs(loadGroup));

  nsLoadFlags loadFlags = nsIRequest::LOAD_NORMAL;
  if (aPresContext) {
    aPresContext->GetImageLoadFlags(loadFlags);
  }

  /* get the URI, convert internal-gopher-stuff if needed */
  nsCOMPtr<nsIURI> uri;
  GetURI(aSpec, getter_AddRefs(uri));
  if (!uri)
    uri = realURI;

  /* set this back to FALSE before we do the real load */
  mInitialLoadCompleted = PR_FALSE;

  nsCOMPtr<imgIRequest> tempRequest;
  return il->LoadImage(uri, loadGroup, mListener, aPresContext, loadFlags, nsnull, aRequest, getter_AddRefs(tempRequest));
}

#define INTERNAL_GOPHER_LENGTH 16 /* "internal-gopher-" length */

void
nsImageFrame::GetURI(const nsAReadableString& aSpec, nsIURI **aURI)
{
  *aURI = nsnull;

  /* Note: navigator 4.* and earlier releases ignored the base tags
     effect on the builtin images. So we do too. Use aSpec instead
     of the absolute url...
   */

  /* The prefix for special "internal" images that are well known.
     Look and see if this is an internal-gopher- url.
   */
  if (NS_LITERAL_STRING("internal-gopher-").Equals(Substring(aSpec, 0, INTERNAL_GOPHER_LENGTH))) {
    nsAutoString newURI;
    newURI.Assign(NS_LITERAL_STRING("resource:/res/html/gopher-") +
                  Substring(aSpec, INTERNAL_GOPHER_LENGTH, aSpec.Length() - INTERNAL_GOPHER_LENGTH) +
                  NS_LITERAL_STRING(".gif"));
    GetRealURI(newURI, aURI);
  }
}

void
nsImageFrame::GetRealURI(const nsAReadableString& aSpec, nsIURI **aURI)
{
  nsCOMPtr<nsIURI> baseURI;
  GetBaseURI(getter_AddRefs(baseURI));
  NS_NewURI(aURI, aSpec, baseURI);
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
    if (mContent) {
      rv = mContent->GetDocument(*getter_AddRefs(doc));
      if (doc) {
        doc->GetBaseURL(*getter_AddRefs(baseURI));
      }
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


PRBool
nsImageFrame::CanLoadImage(nsIURI *aURI)
{

  PRBool shouldLoad = PR_TRUE; // default permit

#if 0
  nsCOMPtr<nsIScriptSecurityManager> securityManager(do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID));

  if (securityManager) {
    nsCOMPtr<nsIURI> baseURI;
    GetBaseURI(getter_AddRefs(baseURI));

    nsresult proceed = securityManager->CheckLoadURI(baseURI, aURI, nsIScriptSecurityManager::STANDARD);
    if (NS_FAILED(proceed))
      return PR_FALSE;
  }
#endif

  // Check with the content-policy things to make sure this load is permitted.
  nsresult rv;
  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mContent));

  if (!element) // this would seem bad(tm)
    return shouldLoad;

  nsCOMPtr<nsIDocument> document;
  if (mContent) {
    rv = mContent->GetDocument(*getter_AddRefs(document));
    if (! document) {
      NS_ASSERTION(0, "expecting a document");
      return shouldLoad;
    }

    nsCOMPtr<nsIScriptGlobalObject> globalScript;
    rv = document->GetScriptGlobalObject(getter_AddRefs(globalScript));
    if (NS_FAILED(rv)) return shouldLoad;

    nsCOMPtr<nsIDOMWindow> domWin(do_QueryInterface(globalScript));

    rv = NS_CheckContentLoadPolicy(nsIContentPolicy::IMAGE,
                                 aURI, element, domWin, &shouldLoad);
    if (NS_SUCCEEDED(rv) && !shouldLoad)
        return shouldLoad;
  }
   
  /* ... additional checks ? */

  return shouldLoad;
}

nsresult nsImageFrame::LoadIcons(nsIPresContext *aPresContext)
{
  nsAutoString loadingSrc(NS_LITERAL_STRING("resource:/res/html/loading-image.gif")); 
  nsAutoString brokenSrc(NS_LITERAL_STRING("resource:/res/html/broken-image.gif")); 

  nsresult rv = NS_OK;
  PRBool doLoad = PR_FALSE;  // only load icons once...

  // see if the first instance and we need to create the icon loader
  if (!mIconLoad) {
#ifdef NOISY_ICON_LOADING
      printf( "Allocating IconLoad (%p)\n", this);
#endif
    mIconLoad = new IconLoad(aPresContext);
    if (!mIconLoad) 
      return NS_ERROR_OUT_OF_MEMORY;
    doLoad = PR_TRUE;
  }
  // always addref
  mIconLoad->AddRef();

#ifdef NOISY_ICON_LOADING
   printf( "IconLoad AddRef'd(%p)\n", this);
#endif

  // if already loaded, we are done
  if (mIconLoad->mIconsLoaded) return rv;

  if (doLoad) {
    // create a loader and load the images
    mIconLoad->mIconLoads[NS_ICON_LOADING_IMAGE].mRequest = do_CreateInstance("@mozilla.org/image/request;1");
    if (mIconLoad->mIconLoads[NS_ICON_LOADING_IMAGE].mRequest) {
#ifdef NOISY_ICON_LOADING
      printf( "Loading request %p\n", mIconLoad->mIconLoads[NS_ICON_LOADING_IMAGE].mRequest.get() );
#endif
      rv = LoadImage(loadingSrc, aPresContext, mIconLoad->mIconLoads[NS_ICON_LOADING_IMAGE].mRequest);
      if (NS_SUCCEEDED(rv)) {
        mIconLoad->mIconLoads[NS_ICON_BROKEN_IMAGE].mRequest = do_CreateInstance("@mozilla.org/image/request;1");
        if (mIconLoad->mIconLoads[NS_ICON_BROKEN_IMAGE].mRequest) {
#ifdef NOISY_ICON_LOADING
          printf( "Loading request %p\n", mIconLoad->mIconLoads[NS_ICON_BROKEN_IMAGE].mRequest.get() );
#endif
          rv = LoadImage(brokenSrc, aPresContext, mIconLoad->mIconLoads[NS_ICON_BROKEN_IMAGE].mRequest);
          // ImageLoader will callback into OnStartContainer, which will handle the mIconsLoaded flag
        }
      }
    }
  }
  return rv;
}

PRBool nsImageFrame::HandleIconLoads(imgIRequest* aRequest, PRBool aLoaded)
{
  PRBool result = PR_FALSE;

  if( mIconLoad ) {
    // check which image it is
    if (aRequest == mIconLoad->mIconLoads[NS_ICON_LOADING_IMAGE].mRequest.get()) {
      result = PR_TRUE;
    } else if (aRequest == mIconLoad->mIconLoads[NS_ICON_BROKEN_IMAGE].mRequest.get()) {
      result = PR_TRUE;
    }
    if (result) {
      mIconLoad->mIconsLoaded = aLoaded;
    }

#ifdef NOISY_ICON_LOADING
    if (mIconLoad->mIconsLoaded && result) {
      printf( "Icons Loaded: request for %s\n", aRequest == mIconLoad->mIconLoads[NS_ICON_LOADING_IMAGE].mRequest ?
                                                "NS_ICON_LOADING_IMAGE" : "NS_ICON_BROKEN_IMAGE" );
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
  NS_ASSERTION(aPresContext, "null presContext in InvalidateIcon");
  float   p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  nsRect inner;
  GetInnerArea(aPresContext, inner);

  nsRect rect(inner.x,
              inner.y,
              NSIntPixelsToTwips(ICON_SIZE+ICON_PADDING, p2t),
              NSIntPixelsToTwips(ICON_SIZE+ICON_PADDING, p2t));
  Invalidate(aPresContext, rect, PR_FALSE);
}

void nsImageFrame::IconLoad::GetAltModePref(nsIPresContext *aPresContext)
{
  NS_ASSERTION(aPresContext, "null presContext is not allowed in GetAltModePref");
  // NOTE: the presContext could be used to fetch a cached pref if needed, but is not for now

  nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID);
  if (prefs) {
    PRBool boolPref;
    if (NS_SUCCEEDED(prefs->GetBoolPref("browser.display.force_inline_alttext", &boolPref))) {
      mPrefForceInlineAltText = boolPref;
    } else {
      mPrefForceInlineAltText = PR_FALSE;
    }
  }
}

#ifdef DEBUG
NS_IMETHODIMP
nsImageFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
#if 0
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
#endif
  *aResult = 0;

  return NS_OK;
}
#endif


NS_IMPL_ISUPPORTS2(nsImageListener, imgIDecoderObserver, imgIContainerObserver)

nsImageListener::nsImageListener(nsImageFrame *aFrame) :
  mFrame(aFrame)
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

