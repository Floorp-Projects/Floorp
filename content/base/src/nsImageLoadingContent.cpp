/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim: ft=cpp tw=78 sw=2 et ts=2
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Boris Zbarsky <bzbarsky@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
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

#include "nsImageLoadingContent.h"
#include "nsContentErrors.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsINodeInfo.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"
#include "nsIServiceManagerUtils.h"
#include "nsContentPolicyUtils.h"
#include "nsIURI.h"
#include "nsILoadGroup.h"
#include "imgIContainer.h"
#include "gfxIImageFrame.h"
#include "imgILoader.h"
#include "plevent.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"
#include "nsNetUtil.h"

#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsGUIEvent.h"
#include "nsDummyLayoutRequest.h"

#include "nsIChannel.h"
#include "nsIStreamListener.h"

#include "nsLayoutAtoms.h"
#include "nsIFrame.h"
#include "nsIDOMNode.h"

#include "nsContentUtils.h"

#ifdef DEBUG_chb
static void PrintReqURL(imgIRequest* req) {
  if (!req) {
    printf("(null req)\n");
    return;
  }

  nsCOMPtr<nsIURI> uri;
  req->GetURI(getter_AddRefs(uri));
  if (!uri) {
    printf("(null uri)\n");
    return;
  }

  nsCAutoString spec;
  uri->GetSpec(spec);
  printf("spec='%s'\n", spec.get());
}
#endif /* DEBUG_chb */

// Statics
imgILoader* nsImageLoadingContent::sImgLoader = nsnull;
nsIIOService* nsImageLoadingContent::sIOService = nsnull;


nsImageLoadingContent::nsImageLoadingContent()
  : mObserverList(nsnull),
    mLoadingEnabled(PR_TRUE),
    mImageIsBlocked(PR_FALSE),
    mHaveHadObserver(PR_FALSE)
{
  if (!sImgLoader)
    mLoadingEnabled = PR_FALSE;
}

nsImageLoadingContent::~nsImageLoadingContent()
{
  // Cancel our requests so they won't hold stale refs to us
  if (mCurrentRequest) {
    mCurrentRequest->Cancel(NS_ERROR_FAILURE);
  }
  if (mPendingRequest) {
    mPendingRequest->Cancel(NS_ERROR_FAILURE);
  }
  NS_ASSERTION(!mObserverList.mObserver && !mObserverList.mNext,
               "Observers still registered?");
}

void
nsImageLoadingContent::Initialize()
{
  // If this fails, NS_NewURI will try to get the service itself
  CallGetService("@mozilla.org/network/io-service;1", &sIOService);

  // Ignore failure and just don't load images
  CallGetService("@mozilla.org/image/loader;1", &sImgLoader);
}

void
nsImageLoadingContent::Shutdown()
{
  NS_IF_RELEASE(sImgLoader);
  NS_IF_RELEASE(sIOService);
}

// Macro to call some func on each observer.  This handles observers
// removing themselves.
#define LOOP_OVER_OBSERVERS(func_)                                       \
  PR_BEGIN_MACRO                                                         \
    for (ImageObserver* observer = &mObserverList, *next; observer;      \
         observer = next) {                                              \
      next = observer->mNext;                                            \
      if (observer->mObserver) {                                         \
        observer->mObserver->func_;                                      \
      }                                                                  \
    }                                                                    \
  PR_END_MACRO


/*
 * imgIContainerObserver impl
 */
NS_IMETHODIMP
nsImageLoadingContent::FrameChanged(imgIContainer* aContainer,
                                    gfxIImageFrame* aFrame,
                                    nsRect* aDirtyRect)
{
  LOOP_OVER_OBSERVERS(FrameChanged(aContainer, aFrame, aDirtyRect));
  return NS_OK;
}
            
/*
 * imgIDecoderObserver impl
 */
NS_IMETHODIMP
nsImageLoadingContent::OnStartDecode(imgIRequest* aRequest)
{
  LOOP_OVER_OBSERVERS(OnStartDecode(aRequest));
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::OnStartContainer(imgIRequest* aRequest,
                                        imgIContainer* aContainer)
{
  LOOP_OVER_OBSERVERS(OnStartContainer(aRequest, aContainer));
  return NS_OK;    
}

NS_IMETHODIMP
nsImageLoadingContent::OnStartFrame(imgIRequest* aRequest,
                                    gfxIImageFrame* aFrame)
{
  LOOP_OVER_OBSERVERS(OnStartFrame(aRequest, aFrame));
  return NS_OK;    
}

NS_IMETHODIMP
nsImageLoadingContent::OnDataAvailable(imgIRequest* aRequest,
                                       gfxIImageFrame* aFrame,
                                       const nsRect* aRect)
{
  LOOP_OVER_OBSERVERS(OnDataAvailable(aRequest, aFrame, aRect));
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::OnStopFrame(imgIRequest* aRequest,
                                   gfxIImageFrame* aFrame)
{
  LOOP_OVER_OBSERVERS(OnStopFrame(aRequest, aFrame));
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::OnStopContainer(imgIRequest* aRequest,
                                       imgIContainer* aContainer)
{
  LOOP_OVER_OBSERVERS(OnStopContainer(aRequest, aContainer));
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::OnStopDecode(imgIRequest* aRequest,
                                    nsresult aStatus,
                                    const PRUnichar* aStatusArg)
{
  NS_PRECONDITION(aRequest == mCurrentRequest || aRequest == mPendingRequest,
                  "Unknown request");
  LOOP_OVER_OBSERVERS(OnStopDecode(aRequest, aStatus, aStatusArg));

  if (aRequest == mPendingRequest) {
    mCurrentRequest->Cancel(NS_ERROR_IMAGE_SRC_CHANGED);
    mCurrentRequest = mPendingRequest;
    mPendingRequest = nsnull;
  }

  if (NS_SUCCEEDED(aStatus)) {
    FireEvent(NS_LITERAL_STRING("load"));
  } else {
    FireEvent(NS_LITERAL_STRING("error"));
  }

  return NS_OK;
}

/*
 * nsIImageLoadingContent impl
 */

NS_IMETHODIMP
nsImageLoadingContent::GetLoadingEnabled(PRBool *aLoadingEnabled)
{
  *aLoadingEnabled = mLoadingEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::SetLoadingEnabled(PRBool aLoadingEnabled)
{
  if (sImgLoader)
    mLoadingEnabled = aLoadingEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::GetImageBlocked(PRBool* aBlocked)
{
  NS_PRECONDITION(aBlocked, "Null out param");
  *aBlocked = mImageIsBlocked;
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::AddObserver(imgIDecoderObserver* aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);

  mHaveHadObserver = PR_TRUE;
  
  if (!mObserverList.mObserver) {
    mObserverList.mObserver = aObserver;
    // Don't touch the linking of the list!
    return NS_OK;
  }

  // otherwise we have to create a new entry

  ImageObserver* observer = &mObserverList;
  while (observer->mNext) {
    observer = observer->mNext;
  }

  observer->mNext = new ImageObserver(aObserver);
  if (! observer->mNext) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::RemoveObserver(imgIDecoderObserver* aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);

  if (mObserverList.mObserver == aObserver) {
    mObserverList.mObserver = nsnull;
    // Don't touch the linking of the list!
    return NS_OK;
  }

  // otherwise have to find it and splice it out
  ImageObserver* observer = &mObserverList;
  while (observer->mNext && observer->mNext->mObserver != aObserver) {
    observer = observer->mNext;
  }

  // At this point, we are pointing to the list element whose mNext is
  // the right observer (assuming of course that mNext is not null)
  if (observer->mNext) {
    // splice it out
    ImageObserver* oldObserver = observer->mNext;
    observer->mNext = oldObserver->mNext;
    oldObserver->mNext = nsnull;  // so we don't destroy them all
    delete oldObserver;
  }
#ifdef DEBUG
  else {
    NS_WARNING("Asked to remove non-existent observer");
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::GetRequest(PRInt32 aRequestType,
                                  imgIRequest** aRequest)
{
  switch(aRequestType) {
  case CURRENT_REQUEST:
    *aRequest = mCurrentRequest;
    break;
  case PENDING_REQUEST:
    *aRequest = mPendingRequest;
    break;
  default:
    NS_ERROR("Unknown request type");
    *aRequest = nsnull;
    return NS_ERROR_UNEXPECTED;
  }
  
  NS_IF_ADDREF(*aRequest);
  return NS_OK;
}


NS_IMETHODIMP
nsImageLoadingContent::GetRequestType(imgIRequest* aRequest,
                                      PRInt32* aRequestType)
{
  NS_PRECONDITION(aRequestType, "Null out param");
  
  if (aRequest == mCurrentRequest) {
    *aRequestType = CURRENT_REQUEST;
    return NS_OK;
  }

  if (aRequest == mPendingRequest) {
    *aRequestType = PENDING_REQUEST;
    return NS_OK;
  }

  *aRequestType = UNKNOWN_REQUEST;
  NS_ERROR("Unknown request");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsImageLoadingContent::GetCurrentURI(nsIURI** aURI)
{
  if (mCurrentRequest)
    return mCurrentRequest->GetURI(aURI);

  NS_IF_ADDREF(*aURI = mCurrentURI);
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::LoadImageWithChannel(nsIChannel* aChannel,
                                            nsIStreamListener** aListener)
{
  NS_PRECONDITION(aListener, "null out param");
  
  NS_ENSURE_ARG_POINTER(aChannel);

  if (!sImgLoader)
    return NS_ERROR_NULL_POINTER;

  // XXX what should we do with content policies here, if anything?
  // Shouldn't that be done before the start of the load?
  
  nsCOMPtr<nsIDocument> doc = GetOurDocument();
  if (!doc) {
    // Don't bother
    return NS_OK;
  }
  
  CancelImageRequests(NS_ERROR_IMAGE_SRC_CHANGED, PR_FALSE);

  nsCOMPtr<imgIRequest> & req = mCurrentRequest ? mPendingRequest : mCurrentRequest;

  return sImgLoader->LoadImageWithChannel(aChannel, this, doc, aListener, getter_AddRefs(req));
}

// XXX This should be a protected method, not an interface method!!!
NS_IMETHODIMP
nsImageLoadingContent::ImageURIChanged(const nsAString& aNewURI) {
  return ImageURIChanged(NS_ConvertUCS2toUTF8(aNewURI));
}

/*
 * Non-interface methods
 */
nsresult
nsImageLoadingContent::ImageURIChanged(const nsACString& aNewURI)
{
  if (!mLoadingEnabled) {
    return NS_OK;
  }

  // First, get a document (needed for security checks and the like)
  nsCOMPtr<nsIDocument> doc = GetOurDocument();
  if (!doc) {
    // No reason to bother, I think...
    return NS_OK;
  }

  nsresult rv;   // XXXbz Should failures in this method fire onerror?

  nsCOMPtr<nsIURI> imageURI;
  rv = StringToURI(aNewURI, doc, getter_AddRefs(imageURI));
  NS_ENSURE_SUCCESS(rv, rv);

  // Remember the URL of this request, in case someone asks us for it later
  // But this only matters if we are affecting the current request
  if (!mCurrentRequest)
    mCurrentURI = imageURI;
  
  // If we'll be loading a new image, we want to cancel our existing
  // requests; the question is what reason to pass in.  If everything
  // is going smoothly, that reason should be
  // NS_ERROR_IMAGE_SRC_CHANGED so that our frame (if any) will know
  // not to show the broken image icon.  If the load is blocked by the
  // content policy or security manager, we will want to cancel with
  // the error code from those.

  // TODOtw: figure out whether we should show alternate text
  //     (CanLoadImage can tell us this, since Content Policy tells it)
  PRBool loadImage = nsContentUtils::CanLoadImage(imageURI, this, doc);

  nsresult cancelResult = loadImage ? NS_ERROR_IMAGE_SRC_CHANGED
                                    : NS_ERROR_IMAGE_BLOCKED;

  mImageIsBlocked = !loadImage;

  CancelImageRequests(cancelResult, PR_FALSE);

  if (mImageIsBlocked) {
    // Don't actually load anything!  This was blocked by CanLoadImage.
    return NS_OK;
  }

  nsCOMPtr<imgIRequest> & req = mCurrentRequest ? mPendingRequest : mCurrentRequest;

  // It may be that one of our frames has replaced itself with alt text... This
  // would only have happened if our mCurrentRequest had issues, and we would
  // have set it to null by now in that case.  Have to save that information
  // here, since LoadImage may clobber the value of mCurrentRequest.  On the
  // other hand, if we've never had an observer, we know there aren't any frames
  // that have changed to alt text on us yet.
  PRBool mayNeedReframe = mHaveHadObserver && !mCurrentRequest;
  
  rv = nsContentUtils::LoadImage(imageURI, doc, doc->GetDocumentURI(),
                                 this, nsIRequest::LOAD_NORMAL,
                                 getter_AddRefs(req));
  // If we now have a current request, we don't need to store the URI, since
  // we can get it off the request. Release it.
  if (mCurrentRequest) {
    mCurrentURI = nsnull;
  }

  if (!mayNeedReframe) {
    // We're all set
    return NS_OK;
  }

  // Only continue if we have a parent and a document -- that would mean we're
  // a useful chunk of the content model and _may_ have a frame.  This should
  // eliminate things like SetAttr calls during the parsing process, as well as
  // things like setting src on |new Image()|-type things.
  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(this, &rv);
  NS_ENSURE_TRUE(thisContent, rv);

  if (!thisContent->GetDocument() || !thisContent->GetParent()) {
    return NS_OK;
  }

  // OK, now for each PresShell, see whether we have a frame -- this tends to
  // be expensive, which is why it's the last check....  If we have a frame
  // and it's not of the right type, reframe it.
  PRInt32 numShells = doc->GetNumberOfShells();
  for (PRInt32 i = 0; i < numShells; ++i) {
    nsIPresShell *shell = doc->GetShellAt(i);
    if (shell) {
      nsIFrame* frame = nsnull;
      shell->GetPrimaryFrameFor(thisContent, &frame);
      if (frame) {
        // XXXbz I don't like this one bit... we really need a better way of
        // doing the CantRenderReplacedElement stuff.. In particular, it needs
        // to be easily detectable.  For example, I suspect that this code will
        // fail for <object> in the current CantRenderReplacedElement
        // implementation...
        nsIAtom* frameType = frame->GetType();
        if (frameType != nsLayoutAtoms::imageFrame &&
            frameType != nsLayoutAtoms::imageControlFrame &&
            frameType != nsLayoutAtoms::objectFrame) {
          shell->RecreateFramesFor(thisContent);
        }
      }
    }
  }

  return NS_OK;
}

void
nsImageLoadingContent::CancelImageRequests(nsresult aReason,
                                           PRBool   aEvenIfSizeAvailable)
{
  // Cancel the pending request, if any
  if (mPendingRequest) {
    mPendingRequest->Cancel(aReason);
    mPendingRequest = nsnull;
  }

  // Cancel the current request if it has not progressed enough to
  // have a size yet
  if (mCurrentRequest) {
    PRUint32 loadStatus = imgIRequest::STATUS_ERROR;
    mCurrentRequest->GetImageStatus(&loadStatus);
    
    if (aEvenIfSizeAvailable ||
        !(loadStatus & imgIRequest::STATUS_SIZE_AVAILABLE)) {
      mCurrentRequest->Cancel(aReason);
      mCurrentRequest = nsnull;
    }
  }
}

nsIDocument*
nsImageLoadingContent::GetOurDocument()
{
  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(this);
  NS_ENSURE_TRUE(thisContent, nsnull);

  return thisContent->GetOwnerDoc();
}

nsresult
nsImageLoadingContent::StringToURI(const nsACString& aSpec,
                                   nsIDocument* aDocument,
                                   nsIURI** aURI)
{
  NS_PRECONDITION(aDocument, "Must have a document");
  NS_PRECONDITION(aURI, "Null out param");

  // (1) Get the base URI
  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(this);
  NS_ASSERTION(thisContent, "An image loading content must be an nsIContent");
  nsCOMPtr<nsIURI> baseURL = thisContent->GetBaseURI();

  // (2) Get the charset
  const nsACString &charset = aDocument->GetDocumentCharacterSet();

  // (3) Construct the silly thing
  return NS_NewURI(aURI,
                   aSpec,
                   charset.IsEmpty() ? nsnull : PromiseFlatCString(charset).get(),
                   baseURL,
                   sIOService);
}


/**
 * Struct used to dispatch events
 */
MOZ_DECL_CTOR_COUNTER(ImageEvent)

class ImageEvent : public PLEvent,
                   public nsDummyLayoutRequest
{
public:
  ImageEvent(nsPresContext* aPresContext, nsIContent* aContent,
             const nsAString& aMessage, nsILoadGroup *aLoadGroup)
    : nsDummyLayoutRequest(nsnull),
      mPresContext(aPresContext),
      mContent(aContent),
      mMessage(aMessage),
      mLoadGroup(aLoadGroup)
  {
    MOZ_COUNT_CTOR(ImageEvent);
  }
  ~ImageEvent()
  {
    MOZ_COUNT_DTOR(ImageEvent);
  }
  
  nsCOMPtr<nsPresContext> mPresContext;
  nsCOMPtr<nsIContent> mContent;
  nsString mMessage;
  nsCOMPtr<nsILoadGroup> mLoadGroup;
};

PR_STATIC_CALLBACK(void*)
HandleImagePLEvent(PLEvent* aEvent)
{
  ImageEvent* evt = NS_STATIC_CAST(ImageEvent*, aEvent);
  nsEventStatus estatus = nsEventStatus_eIgnore;
  PRUint32 eventMsg;

  if (evt->mMessage.EqualsLiteral("load")) {
    eventMsg = NS_IMAGE_LOAD;
  } else {
    eventMsg = NS_IMAGE_ERROR;
  }

  nsEvent event(eventMsg);
  evt->mContent->HandleDOMEvent(evt->mPresContext, &event, nsnull,
                                NS_EVENT_FLAG_INIT, &estatus);

  evt->mLoadGroup->RemoveRequest(evt, nsnull, NS_OK);

  return nsnull;
}

PR_STATIC_CALLBACK(void)
DestroyImagePLEvent(PLEvent* aEvent)
{
  ImageEvent* evt = NS_STATIC_CAST(ImageEvent*, aEvent);

  // We're reference counted, and we hold a strong reference to
  // ourselves while we're a 'live' PLEvent. Now that the PLEvent is
  // destroyed, release ourselves.

  NS_RELEASE(evt);
}

nsresult
nsImageLoadingContent::FireEvent(const nsAString& aEventType)
{
  // We have to fire the event asynchronously so that we won't go into infinite
  // loops in cases when onLoad handlers reset the src and the new src is in
  // cache.

  nsCOMPtr<nsIDocument> document = GetOurDocument();
  if (!document) {
    // no use to fire events if there is no document....
    return NS_OK;
  }                                                                             
  nsresult rv;
  nsCOMPtr<nsIEventQueueService> eventQService =
    do_GetService("@mozilla.org/event-queue-service;1", &rv);
  NS_ENSURE_TRUE(eventQService, rv);

  nsCOMPtr<nsIEventQueue> eventQ;
  // Use the UI thread event queue (though we should not be getting called from
  // off the UI thread in any case....)
  rv = eventQService->GetSpecialEventQueue(nsIEventQueueService::UI_THREAD_EVENT_QUEUE,
                                           getter_AddRefs(eventQ));
  NS_ENSURE_TRUE(eventQ, rv);

  nsCOMPtr<nsILoadGroup> loadGroup = document->GetDocumentLoadGroup();

  nsIPresShell *shell = document->GetShellAt(0);
  NS_ENSURE_TRUE(shell, NS_ERROR_FAILURE);

  nsPresContext *presContext = shell->GetPresContext();
  NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

  nsCOMPtr<nsIContent> ourContent = do_QueryInterface(this);
  
  ImageEvent* evt = new ImageEvent(presContext, ourContent, aEventType,
                                   loadGroup);

  NS_ENSURE_TRUE(evt, NS_ERROR_OUT_OF_MEMORY);

  PL_InitEvent(evt, this, ::HandleImagePLEvent, ::DestroyImagePLEvent);

  // The event will own itself while it's in the event queue, once
  // removed, it will release itself, and if there are no other
  // references, it will be deleted.
  NS_ADDREF(evt);

  rv = eventQ->PostEvent(evt);

  if (NS_SUCCEEDED(rv)) {
    // Add the dummy request (the ImageEvent) to the load group only
    // after all the early returns here!
    loadGroup->AddRequest(evt, nsnull);
  } else {
    PL_DestroyEvent(evt);
  }

  return rv;
}
