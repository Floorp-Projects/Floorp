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
 *               Mark Lin <mark.lin@eng.sun.com>
 *               Mark Goddard
 *               Ed Burns <edburns@acm.org>
 *               Ann Sunhachawee
 */

#include "DocumentLoaderObserverImpl.h"

#include "nsString.h"
#include "DocumentLoaderObserverImpl.h"
#include<stdio.h>
#include "jni_util.h"
#include "nsActions.h"

#ifdef XP_PC
// PENDING(edburns): take this out
#include "winbase.h"
// end of take this out
#endif

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDocumentLoaderObserverIID, NS_IDOCUMENT_LOADER_OBSERVER_IID);

jlong DocumentLoaderObserverImpl::maskValues[] = { -1L };

char *DocumentLoaderObserverImpl::maskNames[] = {
  "START_DOCUMENT_LOAD_EVENT_MASK",
  "END_DOCUMENT_LOAD_EVENT_MASK",
  "START_URL_LOAD_EVENT_MASK",
  "END_URL_LOAD_EVENT_MASK",
  "PROGRESS_URL_LOAD_EVENT_MASK",
  "STATUS_URL_LOAD_EVENT_MASK",
  "UNKNOWN_CONTENT_EVENT_MASK",
  "FETCH_INTERRUPT_EVENT_MASK",
  nsnull
};


/*************
NS_IMETHODIMP_(nsrefcnt) DocumentLoaderObserverImpl::AddRef(void)
{
    NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt"); 
    ++mRefCnt;
    return mRefCnt;
}
**************/

NS_IMPL_ADDREF(DocumentLoaderObserverImpl);
NS_IMPL_RELEASE(DocumentLoaderObserverImpl);  

DocumentLoaderObserverImpl::DocumentLoaderObserverImpl(){
}

DocumentLoaderObserverImpl::DocumentLoaderObserverImpl(JNIEnv *env,
					 WebShellInitContext *yourInitContext,
						       jobject yourTarget) :
  mJNIEnv(env), mInitContext(yourInitContext), mTarget(yourTarget)
{
    if (nsnull == gVm) { // declared in jni_util.h
      ::util_GetJavaVM(env, &gVm);  // save this vm reference away for the callback!
    }

    if (-1 == maskValues[0]) {
        util_InitializeEventMaskValuesFromClass("org/mozilla/webclient/DocumentLoadEvent",
                                                maskNames, maskValues);
    }
    mRefCnt = 1; // PENDING(edburns): not sure about how right this is to do.
}

NS_IMETHODIMP DocumentLoaderObserverImpl::QueryInterface(REFNSIID aIID, void** aInstance)
{
  if (nsnull == aInstance)
    return NS_ERROR_NULL_POINTER;

  *aInstance = nsnull;

 
  if (aIID.Equals(kIDocumentLoaderObserverIID)) {
    *aInstance = (void*) ((nsIDocumentLoaderObserver*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  

  return NS_NOINTERFACE;
}

  /* nsIDocumentLoaderObserver methods */
NS_IMETHODIMP DocumentLoaderObserverImpl::OnStartDocumentLoad(nsIDocumentLoader* loader, 
							      nsIURI* aURL, 
							      const char* aCommand) 
{
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, 
               ("DocumentLoaderObserverImpl.cpp: OnStartDocumentLoad\n"));
    }
#endif
    util_SendEventToJava(mInitContext->env, 
                         mInitContext->nativeEventThread, mTarget, 
			 maskValues[START_DOCUMENT_LOAD_EVENT_MASK], nsnull);
    
    return NS_OK;
}

NS_IMETHODIMP DocumentLoaderObserverImpl::OnEndDocumentLoad(nsIDocumentLoader* loader, 
							    nsIChannel* channel, 
							    nsresult aStatus) 
{
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, 
               ("!!DocumentLoaderObserverImpl.cpp: OnEndDocumentLoad\n"));
    }
#endif

    util_SendEventToJava(mInitContext->env, 
                         mInitContext->nativeEventThread, mTarget, 
                         maskValues[END_DOCUMENT_LOAD_EVENT_MASK], nsnull);
    return NS_OK;
}

NS_IMETHODIMP DocumentLoaderObserverImpl::OnStartURLLoad(nsIDocumentLoader* loader, 
							 nsIChannel* channel) 
{
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, 
               ("!DocumentLoaderObserverImpl: OnStartURLLoad\n"));
    }
#endif
    util_SendEventToJava(mInitContext->env, mInitContext->nativeEventThread, mTarget, 
			 maskValues[START_URL_LOAD_EVENT_MASK], nsnull);
    
    return NS_OK;
}  

NS_IMETHODIMP DocumentLoaderObserverImpl::OnProgressURLLoad(nsIDocumentLoader* loader,
			       nsIChannel* channel, 
			       PRUint32 aProgress, 
                               PRUint32 aProgressMax) 
{
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, 
               ("!DocumentLoaderObserverImpl: OnProgressURLLoad\n"));
    }
#endif
    util_SendEventToJava(mInitContext->env, mInitContext->nativeEventThread, mTarget, 
			 maskValues[PROGRESS_URL_LOAD_EVENT_MASK], nsnull);


    return NS_OK;
}

NS_IMETHODIMP DocumentLoaderObserverImpl::OnStatusURLLoad(nsIDocumentLoader* loader, 
			     nsIChannel* channel, 
			     nsString& aMsg) 
{
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, 
               ("!DocumentLoaderObserverImpl: OnStatusURLLoad: %S\n",
                aMsg.GetUnicode()));
    }
#endif
    int length = aMsg.Length();
    jstring statusMessage = ::util_NewString(mInitContext->env,
                                             aMsg.GetUnicode(), length);
    
    util_SendEventToJava(mInitContext->env, mInitContext->nativeEventThread, mTarget, 
			 maskValues[STATUS_URL_LOAD_EVENT_MASK], (jobject) statusMessage);

#ifdef BAL_INTERFACE
    // This violates my goal of confining all #ifdef BAL_INTERFACE to
    // jni_util files, but this is the only part of the code that knows
    // that eventData is a jstring.  In java, this will get garbage
    // collected, but in non-java contexts, it will not.  Thus, we have
    // to manually de-allocate it.
    ::util_DeleteString(mInitContext->env, statusMessage);

#endif

    return NS_OK;
}

NS_IMETHODIMP DocumentLoaderObserverImpl::OnEndURLLoad(nsIDocumentLoader* loader, 
			  nsIChannel* channel, 
			  nsresult aStatus) 
{
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, 
               ("!DocumentLoaderObserverImpl: OnEndURLLoad\n"));
    }
#endif
    util_SendEventToJava(mInitContext->env, mInitContext->nativeEventThread, mTarget, 
			 maskValues[END_URL_LOAD_EVENT_MASK], nsnull);

    return NS_OK;
}

NS_IMETHODIMP DocumentLoaderObserverImpl::HandleUnknownContentType(nsIDocumentLoader* loader,
				      nsIChannel* channel, 
				      const char *aContentType,
				      const char *aCommand) 
{
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, 
               ("!DocumentLoaderObserverImpl: HandleUnknownContentType\n"));
    }
#endif
    util_SendEventToJava(mInitContext->env, mInitContext->nativeEventThread, mTarget, 
			 maskValues[UNKNOWN_CONTENT_EVENT_MASK], nsnull);
    
    return NS_OK;
}



