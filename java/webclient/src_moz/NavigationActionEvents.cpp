/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s): Kirk Baker <kbaker@eb.com>
 *               Ian Wilkinson <iw@ennoble.com>
 *               Ashutosh Kulkarni <ashuk@eng.sun.com>
 *               Mark Lin <mark.lin@eng.sun.com>
 *               Mark Goddard
 *               Ed Burns <edburns@acm.org>
 *               Jason Mawdsley <jason@macadamian.com>
 *               Louis-Philippe Gagnon <louisphilippe@macadamian.com>
 *               Brian Satterfield <bsatterf@atl.lmco.com>
 *               Anthony Sizer <sizera@yahoo.com>
 *               Ron Capelli <capelli@us.ibm.com>
 */

/*
 * NavigationActionEvents.cpp
 */

#include "NavigationActionEvents.h"

#include "ns_util.h"
#include "InputStreamShim.h"
#include "nsNetUtil.h"

#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsILinkHandler.h"
#include "nsIContent.h"
#include "nsNetUtil.h" // for NS_NewPostInputStream
#include "nsIDocument.h"
#include "nsIFrame.h"

#include "NativeBrowserControl.h"
#include "EmbedWindow.h"

/*****************************

wsLoadURLEvent::wsLoadURLEvent(nsIWebNavigation* webNavigation, PRUnichar * urlString, PRInt32 urlLength) :
        nsActionEvent(),
        mWebNavigation(webNavigation),
        mURL(nsnull)
{
        mURL = new nsString(urlString, urlLength);
}


void *
wsLoadURLEvent::handleEvent ()
{
  void* threadId = PR_GetCurrentThread();
  printf ("+++++++++++++++++++++ Thread Id ---- %p\n\n", threadId);

  if (mWebNavigation && mURL) {
      nsresult rv = mWebNavigation->LoadURI(mURL->get(), 
                                            nsIWebNavigation::LOAD_FLAGS_NONE,
                                            nsnull, nsnull, nsnull);
  }
  return nsnull;
} // handleEvent()

wsLoadURLEvent::~wsLoadURLEvent ()
{
  if (mURL != nsnull)
    delete mURL;
}

***********************/

wsLoadFromStreamEvent::wsLoadFromStreamEvent(NativeBrowserControl *yourNativeBC, 
                                             void *globalStream,
                                             nsString &uriToCopy,
                                             const char *contentTypeToCopy,
                                             PRInt32 contentLength, 
                                             void *globalLoadProperties) :
    nsActionEvent(), mNativeBrowserControl(yourNativeBC), mUriString(uriToCopy),
    mContentType(PL_strdup(contentTypeToCopy)), 
    mProperties(globalLoadProperties), mShim(nsnull)
{
    mShim = new InputStreamShim((jobject) globalStream, contentLength);
    NS_IF_ADDREF(mShim);
}

wsLoadFromStreamEvent::wsLoadFromStreamEvent(NativeBrowserControl *yourNativeBC,
                                             InputStreamShim *yourShim) :
    nsActionEvent(), mNativeBrowserControl(yourNativeBC),
    mContentType(nsnull), mProperties(nsnull), mShim(yourShim)
{
}

/**

 * This funky handleEvent allows the java InputStream to be read from
 * the correct thread (the NativeEventThread), while, on a separate
 * thread, mozilla's LoadFromStream reads from our nsIInputStream.  This
 * is accomplished using a "shim" class, InputStreamShim.
 * InputStreamShim is an nsIInputStream, but it also maintains
 * information on how to read from the java InputStream.  The important
 * thing is that InputStreamShim::doReadFromJava() is called on
 * NativeEventThread() until all the data is read.  This is accomplished
 * by having this wsLoadFromStream instance copy itself and re-enqueue
 * itself, if there is more data to read.  

 */

void *
wsLoadFromStreamEvent::handleEvent ()
{
    nsresult rv = NS_ERROR_FAILURE;
    nsresult readFromJavaStatus = NS_ERROR_FAILURE;
    nsCOMPtr<nsIURI> uri;
    wsLoadFromStreamEvent *repeatEvent = nsnull;
    
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    
    // we must have both mNativeBrowserControl and mShim to do anything
    if (!mNativeBrowserControl || !mShim) {
        return (void *) rv;
    }

    // Try to read as much as possible off the java InputStream
    // into the shim's internal buffer.

    // see InputShimStream::doReadFromJava() for the meaning of the
    // return values.  They are very important.
    readFromJavaStatus = mShim->doReadFromJava();
    if (NS_ERROR_FAILURE == readFromJavaStatus) {
        NS_IF_RELEASE(mShim);
        return (void *) readFromJavaStatus;
    }
    
    // if this is the first time handleEvent has been called for this
    // InputStreamShim instance.
    
    if (mContentType) {
        rv = NS_NewURI(getter_AddRefs(uri), mUriString);
        if (!uri) {
            return (void *) rv;
        }
        
        // PENDING(edburns): turn the mProperties jobject into an
        // nsIDocShellLoadInfo instance.
        
        // Kick off a LoadStream.  This will cause
        // InputStreamShim::Read() to be called, 
        
	printf ("debug: capelli: LoadStream - mContentType: %s  mUriString: %s\n", 
	                                      mContentType,     mUriString.get());
	
        rv = mNativeBrowserControl->mWindow->LoadStream(mShim, uri, 
						nsDependentCString(mContentType),
					        NS_LITERAL_CSTRING(""),
                                                nsnull);
        // make it so we don't issue multiple LoadStream calls 
        // for this InputStreamShim instance.
        
        nsCRT::free(mContentType);
        mContentType = nsnull;
    }
    
    // if there is more data
    if (NS_OK == readFromJavaStatus){
        // and we can create a copy of ourselves
        if (repeatEvent = new wsLoadFromStreamEvent(mNativeBrowserControl, mShim)) {
            // do the loop
            ::util_PostEvent((PLEvent *) *repeatEvent);
            rv = NS_OK;
        }
        else {
            NS_IF_RELEASE(mShim);
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
    }
    if (NS_ERROR_NOT_AVAILABLE == readFromJavaStatus) {
        NS_IF_RELEASE(mShim);
        rv = NS_OK;
    }
    
    return (void *) rv;
} // handleEvent()

wsLoadFromStreamEvent::~wsLoadFromStreamEvent ()
{
    nsCRT::free(mContentType);
    mContentType = nsnull;
    if (mProperties) {
        JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
        ::util_DeleteGlobalRef(env, (jobject) mProperties);
        mProperties = nsnull;
    }
        
}


/**********************
wsPostEvent::wsPostEvent(NativeBrowserControl *yourInitContext, 
                         nsIURI              *absoluteUri,
                         const PRUnichar     *targetToCopy,
                         PRInt32              targetLength,
                         PRInt32              postDataLength,
                         const char          *postDataToCopy,  
                         PRInt32              postHeadersLength,
                         const char          *postHeadersToCopy) :
    nsActionEvent(), 
    mNativeBrowserControl(yourInitContext)
{
  mAbsoluteURI = absoluteUri;
  if (targetToCopy != nsnull){
    mTarget = new nsString(targetToCopy, targetLength);
  }
  else {
    mTarget = nsnull;
  }

  if (postDataToCopy != nsnull){
    mPostDataLength = postDataLength;
    mPostData = PL_strdup(postDataToCopy);
  }
  else {
    mPostDataLength = 0;
    mPostData = nsnull;
  }

  if (postHeadersToCopy != nsnull){
    mPostHeaderLength = postHeadersLength;
    mPostHeaders = PL_strdup(postHeadersToCopy);
  }
  else {
    mPostHeaderLength = 0;
    mPostHeaders = nsnull;
  }
}

void *
wsPostEvent::handleEvent ()
{
  nsresult rv = NS_ERROR_FAILURE;
    
  // we must have mNativeBrowserControl to do anything
  if (!mNativeBrowserControl) {
      return (void *) rv;
  }
  nsCOMPtr<nsIPresContext> presContext;
  nsCOMPtr<nsIPresShell> presShell;
  nsCOMPtr<nsISupports> container;
  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsILinkHandler> lh;
  nsCOMPtr<nsIInputStream> result;
  
  rv = mNativeBrowserControl->docShell->GetPresShell(getter_AddRefs(presShell));
  if (NS_FAILED(rv) || !presShell) {
      return (void *) rv;
  }

  rv = presShell->GetDocument(getter_AddRefs(doc));
  if (NS_FAILED(rv) || !doc) {
      return (void *) rv;
  }

  content = doc->GetRootContent();
  if (NS_FAILED(rv) || !content) {
      return (void *) rv;
  }

  rv = mNativeBrowserControl->docShell->GetPresContext(getter_AddRefs(presContext));
  if (NS_FAILED(rv) || !presContext) {
      return (void *) rv;
  }

  /* Alternate way to get link handler
  rv = presContext->GetContainer(getter_AddRefs(container));
  if (NS_FAILED(rv) || !container) {
      return (void *) rv;
  }

  lh = do_QueryInterface(container, &rv);
  if (!lh) {
      return (void *) rv;
  }

  rv = presContext->GetLinkHandler(getter_AddRefs(lh));
  if (NS_FAILED(rv) || (!lh)) {
    return (void *) rv;
  }

  // create the streams
  nsCOMPtr<nsIInputStream> postDataStream = nsnull;
  nsCOMPtr<nsIInputStream> headersDataStream = nsnull;

  if (mPostData) {
      nsCAutoString postData(mPostData);
      NS_NewPostDataStream(getter_AddRefs(postDataStream),
                           PR_FALSE,
                           postData, 0);
  }

  if (mPostHeaders) {
      NS_NewByteInputStream(getter_AddRefs(result),
                            (const char *) mPostHeaders, mPostHeaderLength);
      if (result) {
          headersDataStream = do_QueryInterface(result, &rv);
      }
  }

  rv = lh->OnLinkClick(content, 
                       eLinkVerb_Replace, 
                       mAbsoluteURI, 
                       ((mTarget != nsnull) ? mTarget->get() : nsnull),
                       postDataStream,
                       headersDataStream);  
  
  return (void *) rv;

} // handleEvent()

wsPostEvent::~wsPostEvent ()
{
  mAbsoluteURI = nsnull;
  if (mTarget != nsnull)
    delete mTarget;
  mPostData = nsnull;
  mPostHeaders = nsnull;
}


wsStopEvent::wsStopEvent(nsIWebNavigation* webNavigation) :
        nsActionEvent(),
        mWebNavigation(webNavigation)
{
}


void *
wsStopEvent::handleEvent ()
{
        if (mWebNavigation) {
                nsresult rv = mWebNavigation->Stop(nsIWebNavigation::STOP_ALL);
        }
        return nsnull;
} // handleEvent()



// Added by Mark Goddard OTMP 9/2/1999


wsRefreshEvent::wsRefreshEvent(nsIWebNavigation* webNavigation, PRInt32 reloadType) :
        nsActionEvent(),
        mWebNavigation(webNavigation),
	mReloadType(reloadType)
{

}


void *
wsRefreshEvent::handleEvent ()
{
        if (mWebNavigation) {
                nsresult rv = mWebNavigation->Reload(mReloadType);
                return (void *) rv;
        }
        return nsnull;
} // handleEvent()



wsSetPromptEvent::wsSetPromptEvent(wcIBrowserContainer* aBrowserContainer,
                                   jobject aUserPrompt) :
        nsActionEvent(),
        mBrowserContainer(aBrowserContainer),
        mUserPrompt(aUserPrompt)
{

}

void *
wsSetPromptEvent::handleEvent ()
{
        if (mBrowserContainer && mUserPrompt) {
            mBrowserContainer->SetPrompt(mUserPrompt);
        }
        return nsnull;
} // handleEvent()

**********************/
