/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsIHTMLContent.h"
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

#include "nsIDOMDocumentEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMEvent.h"

nsImageLoadingContent::nsImageLoadingContent()
  : mObserverList(nsnull),
    mLoadingEnabled(PR_TRUE),
    mImageIsBlocked(PR_FALSE)
{
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

// Macro to call some func on each observer.  This handles observers
// removing themselves.
#define LOOP_OVER_OBSERVERS(func_)                                       \
  PR_BEGIN_MACRO                                                         \
    for (ImageObserver* observer = &mObserverList; observer;             \
         observer = observer->mNext) {                                   \
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
 * nsIImageLoadingElement impl
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
nsImageLoadingContent::GetRequest(const PRInt32 aRequestType,
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

  if (aNewURI.IsEmpty()) {
    // Do not take down the already loaded image... (for compat with
    // the old code that loaded images from frames)    
    // XXXbz is this what we really want?
    return NS_OK;
  }

  nsresult rv;   // XXXbz Should failures in this method fire onerror?

  // First, get a document (needed for security checks, base URI and the like)
  nsCOMPtr<nsIDocument> doc;
  rv = GetOurDocument(getter_AddRefs(doc));
  if (!doc) {
    // No reason to bother, I think...
    return rv;
  }

  nsCOMPtr<nsIURI> imageURI;
  rv = StringToURI(aNewURI, doc, getter_AddRefs(imageURI));
  NS_ENSURE_SUCCESS(rv, rv);

  
  // Now get the image loader...
  nsCOMPtr<imgILoader> loader = do_GetService("@mozilla.org/image/loader;1", &rv);
  NS_ENSURE_TRUE(loader, rv);

  // If we'll be loading a new image, we want to cancel our existing
  // requests; the question is what reason to pass in.  If everything
  // is going smoothly, that reason should be
  // NS_ERROR_IMAGE_SRC_CHANGED so that our frame (if any) will know
  // not to show the broken image icon.  If the load is blocked by the
  // content policy or security manager, we will want to cancel with
  // the error code from those.
  nsresult cancelResult = CanLoadImage(imageURI, doc);
  if (NS_SUCCEEDED(cancelResult)) {
    cancelResult = NS_ERROR_IMAGE_SRC_CHANGED;
  }

  mImageIsBlocked = (cancelResult == NS_ERROR_IMAGE_BLOCKED);
  
  CancelImageRequests(cancelResult);

  if (cancelResult != NS_ERROR_IMAGE_SRC_CHANGED) {
    // Don't actually load anything!  This was blocked by CanLoadImage.
    return NS_OK;
  }

  nsCOMPtr<nsILoadGroup> loadGroup;
  doc->GetDocumentLoadGroup(getter_AddRefs(loadGroup));
  NS_WARN_IF_FALSE(loadGroup, "Could not get loadgroup; onload may fire too early");

  nsCOMPtr<nsIURI> documentURI;
  doc->GetDocumentURL(getter_AddRefs(documentURI));

  nsCOMPtr<imgIRequest> & req = mCurrentRequest ? mPendingRequest : mCurrentRequest;

  // XXXbz using "documentURI" for the initialDocumentURI is not quite
  // right, but the best we can do here...
  return loader->LoadImage(imageURI,                /* uri to load */
                           documentURI,             /* initialDocumentURI */
                           documentURI,             /* referrer */
                           loadGroup,               /* loadgroup */
                           this,                    /* imgIDecoderObserver */
                           doc,                     /* uniquification key */
                           nsIRequest::LOAD_NORMAL, /* load flags */
                           nsnull,                  /* cache key */
                           nsnull,                  /* existing request*/
                           getter_AddRefs(req));
}

void
nsImageLoadingContent::CancelImageRequests(nsresult aReason)
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
    
    if (!(loadStatus & imgIRequest::STATUS_SIZE_AVAILABLE)) {
      mCurrentRequest->Cancel(aReason);
      mCurrentRequest = nsnull;
    }
  }
}

nsresult
nsImageLoadingContent::CanLoadImage(nsIURI* aURI, nsIDocument* aDocument)
{
  NS_PRECONDITION(aURI, "Null URI");
  NS_PRECONDITION(aDocument, "Null document!");
  
  // Check with the content-policy things to make sure this load is permitted.

  nsCOMPtr<nsIScriptGlobalObject> globalScript;
  nsresult rv = aDocument->GetScriptGlobalObject(getter_AddRefs(globalScript));
  if (NS_FAILED(rv)) {
    // just let it load.  Documents loaded as data should take care to
    // prevent image loading themselves.
    return NS_OK;
  }

  nsCOMPtr<nsIDOMWindow> domWin(do_QueryInterface(globalScript));

  PRBool shouldLoad = PR_TRUE;
  rv = NS_CheckContentLoadPolicy(nsIContentPolicy::IMAGE, aURI, this,
                                 domWin, &shouldLoad);
  if (NS_SUCCEEDED(rv) && !shouldLoad) {
    return NS_ERROR_IMAGE_BLOCKED;
  }

  return NS_OK;
}


nsresult
nsImageLoadingContent::GetOurDocument(nsIDocument** aDocument)
{
  NS_PRECONDITION(aDocument, "Null out param");

  nsresult rv;

  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(this, &rv);
  NS_ENSURE_TRUE(thisContent, rv);

  rv = thisContent->GetDocument(*aDocument);
  if (!*aDocument) {  // nodeinfo time
    nsCOMPtr<nsINodeInfo> nodeInfo;
    rv = thisContent->GetNodeInfo(*getter_AddRefs(nodeInfo));
    if (nodeInfo) {
      rv = nodeInfo->GetDocument(*aDocument);
    }
  }

  return rv;
}

nsresult
nsImageLoadingContent::StringToURI(const nsACString& aSpec,
                                   nsIDocument* aDocument,
                                   nsIURI** aURI)
{
  NS_PRECONDITION(aDocument, "Must have a document");
  NS_PRECONDITION(aURI, "Null out param");

  nsresult rv;
  
  // (1) Get the base URI
  nsCOMPtr<nsIURI> baseURL;
  nsCOMPtr<nsIHTMLContent> thisContent = do_QueryInterface(this);
  if (thisContent) {
    rv = thisContent->GetBaseURL(*getter_AddRefs(baseURL));
  } else {
    rv = aDocument->GetBaseURL(*getter_AddRefs(baseURL));
    if (!baseURL) {
      rv = aDocument->GetDocumentURL(getter_AddRefs(baseURL));
    }
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // (2) Get the charset
  nsAutoString charset;
  aDocument->GetDocumentCharacterSet(charset);

  // (3) Construct the silly thing
  return NS_NewURI(aURI,
                   aSpec,
                   charset.IsEmpty() ? nsnull : NS_ConvertUCS2toUTF8(charset).get(),
                   baseURL);
}


/**
 * Struct used to dispatch events
 */
struct ImageEvent : PLEvent {
  ImageEvent(nsIDOMEventTarget* aTarget, nsIDOMEvent* aEvent)
    : mTarget(aTarget),
      mEvent(aEvent)
  {}
      
  nsCOMPtr<nsIDOMEventTarget> mTarget;
  nsCOMPtr<nsIDOMEvent> mEvent;
};

PR_STATIC_CALLBACK(void*)
HandleImagePLEvent(PLEvent* aEvent)
{
  ImageEvent* evt = NS_STATIC_CAST(ImageEvent*, aEvent);
  PRBool preventDefaultCalled;  // we don't care...
  evt->mTarget->DispatchEvent(evt->mEvent, &preventDefaultCalled);
  return nsnull;
}

PR_STATIC_CALLBACK(void)
DestroyImagePLEvent(PLEvent* aEvent)
{
  ImageEvent* evt = NS_STATIC_CAST(ImageEvent*, aEvent);
  delete evt;
}

nsresult
nsImageLoadingContent::FireEvent(const nsAString& aEventType)
{
  // We have to fire the event asynchronously so that we won't go into infinite
  // loops in cases when onLoad handlers reset the src and the new src is in
  // cache.
  
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

  nsCOMPtr<nsIDocument> document;
  rv = GetOurDocument(getter_AddRefs(document));

  nsCOMPtr<nsIDOMEventTarget> eventTarget = do_QueryInterface(this);
  NS_ENSURE_TRUE(eventTarget, NS_ERROR_UNEXPECTED);
  
  nsCOMPtr<nsIDOMDocumentEvent> doc = do_QueryInterface(document);
  NS_ENSURE_TRUE(doc, rv);

  nsCOMPtr<nsIDOMEvent> domEvent;
  rv = doc->CreateEvent(NS_LITERAL_STRING("HTMLEvents"), getter_AddRefs(domEvent));
  NS_ENSURE_TRUE(domEvent, rv);
  
  domEvent->InitEvent(aEventType, PR_FALSE, PR_FALSE);
  
  ImageEvent* evt = new ImageEvent(eventTarget, domEvent);
  
  NS_ENSURE_TRUE(evt, NS_ERROR_OUT_OF_MEMORY);

  PL_InitEvent(evt, this, ::HandleImagePLEvent, ::DestroyImagePLEvent);

  return eventQ->PostEvent(evt);
}
