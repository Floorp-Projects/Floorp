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
#include "jni_util.h"
#include "dom_util.h"
#include "nsActions.h"

#include "nsIDOMDocument.h"
#include "nsIDOMEventTarget.h"

#include "prlog.h" // for PR_ASSERT

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

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDocumentLoaderObserverIID, NS_IDOCUMENT_LOADER_OBSERVER_IID);
static NS_DEFINE_IID(kIDocumentLoaderObserverImplIID, NS_IDOCLOADEROBSERVERIMPL_IID);

NS_IMPL_ADDREF(DocumentLoaderObserverImpl);
NS_IMPL_RELEASE(DocumentLoaderObserverImpl);  

DocumentLoaderObserverImpl::DocumentLoaderObserverImpl() : mRefCnt(1),
mTarget(nsnull), mMouseListener(nsnull) {
}

DocumentLoaderObserverImpl::DocumentLoaderObserverImpl(JNIEnv *env,
					 WebShellInitContext *yourInitContext) :
    mJNIEnv(env), mInitContext(yourInitContext), mTarget(nsnull), 
    mMouseListener(nsnull)
{
    if (nsnull == gVm) { // declared in jni_util.h
      ::util_GetJavaVM(env, &gVm);  // save this vm reference away for the callback!
    }
#ifndef BAL_INTERFACE
    PR_ASSERT(gVm);
#endif

    if (-1 == maskValues[0]) {
        util_InitializeEventMaskValuesFromClass("org/mozilla/webclient/DocumentLoadEvent",
                                                maskNames, maskValues);
    }
    mRefCnt = 1; // PENDING(edburns): not sure about how right this is to do.
}

NS_IMETHODIMP DocumentLoaderObserverImpl::QueryInterface(REFNSIID aIID, 
                                                         void** aInstance)
{
    if (nsnull == aInstance)
        return NS_ERROR_NULL_POINTER;
    
    *aInstance = nsnull;
    
    
    if (aIID.Equals(kIDocumentLoaderObserverIID)) {
        *aInstance = (void*) ((nsIDocumentLoaderObserver*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    else if (aIID.Equals(kIDocumentLoaderObserverImplIID)) {
            *aInstance = (void*) ((DocumentLoaderObserverImpl*)this);
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
    // If we don't have a target, don't take any action
    if (nsnull == mTarget) {
        return NS_OK;
    }

    char *urlStr = nsnull;
    jobject urlJStr = nsnull;
    if (nsnull != aURL) {

        // IMPORTANT: do not use initContext->env here since it comes
        // from another thread.  Use JNU_GetEnv instead.

        JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION_1_2);
        
        aURL->GetSpec(&urlStr);
        if (nsnull != urlStr) {
            urlJStr = (jobject) ::util_NewStringUTF(env, urlStr);
            ::Recycle(urlStr);
        }
    }            
    util_SendEventToJava(mInitContext->env, 
                         mInitContext->nativeEventThread, mTarget, 
                         maskValues[START_DOCUMENT_LOAD_EVENT_MASK], urlJStr);

    if (urlJStr) {
        ::util_DeleteStringUTF(mInitContext->env, (jstring) urlJStr);
    }
    
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
    // if we have a mouse listener
    if (mMouseListener) {
        // install the mouse listener into mozilla
        
        nsCOMPtr<nsIDOMDocument> doc;
        PR_ASSERT(mMouseListener);
        
        if (nsnull == loader) {
            // NOT really ok, but we can't do anything.
            return NS_OK;
        }
        
        if (!(doc = dom_getDocumentFromLoader(loader))) {
            // NOT really ok, but we can't do anything.
            return NS_OK;
        }
        nsCOMPtr<nsIDOMEventTarget> domEventTarget;
        nsresult rv;
        
        rv = doc->QueryInterface(NS_GET_IID(nsIDOMEventTarget),
                                 getter_AddRefs(domEventTarget));
        if (NS_FAILED(rv) || !domEventTarget) {
            return NS_OK;
        }
        nsAutoString eType("mouseover");
        domEventTarget->AddEventListener(eType, mMouseListener, PR_FALSE);
        
    } // end of "install mouse listener"
    
    // If we don't have a target, don't take any action
    if (nsnull == mTarget) {
        return NS_OK;
    }

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
    // If we don't have a target, don't take any action
    if (nsnull == mTarget) {
        return NS_OK;
    }

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
    // If we don't have a target, don't take any action
    if (nsnull == mTarget) {
        return NS_OK;
    }

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
    // If we don't have a target, don't take any action
    if (nsnull == mTarget) {
        return NS_OK;
    }

    int length = aMsg.Length();

    // IMPORTANT: do not use initContext->env here since it comes
    // from another thread.  Use JNU_GetEnv instead.
    
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION_1_2);
    jstring statusMessage = ::util_NewString(env, (const jchar *) 
                                             aMsg.GetUnicode(), length);
    
    util_SendEventToJava(mInitContext->env, mInitContext->nativeEventThread, mTarget, 
			 maskValues[STATUS_URL_LOAD_EVENT_MASK], (jobject) statusMessage);

    if (statusMessage) {
        ::util_DeleteString(mInitContext->env, statusMessage);
    }
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
    // If we don't have a target, don't take any action
    if (nsnull == mTarget) {
        return NS_OK;
    }

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
    // If we don't have a target, don't take any action
    if (nsnull == mTarget) {
        return NS_OK;
    }

    util_SendEventToJava(mInitContext->env, mInitContext->nativeEventThread, mTarget, 
			 maskValues[UNKNOWN_CONTENT_EVENT_MASK], nsnull);
    
    return NS_OK;
}

//
// Methods from DocumentLoaderObserverImpl
//

NS_IMETHODIMP DocumentLoaderObserverImpl::AddMouseListener(nsCOMPtr<nsIDOMMouseListener> toAdd)
{
    nsresult rv = NS_ERROR_FAILURE;

    if (nsnull == toAdd) {
        return rv;
    }

    mMouseListener = toAdd;

    return NS_OK;
}

NS_IMETHODIMP DocumentLoaderObserverImpl::RemoveMouseListener(nsCOMPtr<nsIDOMMouseListener> toRemove)
{
    nsresult rv = NS_ERROR_FAILURE;

    return rv;
}

NS_IMETHODIMP DocumentLoaderObserverImpl::SetTarget(jobject yourTarget)
{
    nsresult rv = NS_OK;

    mTarget = yourTarget;

    return rv;
}

NS_IMETHODIMP DocumentLoaderObserverImpl::ClearTarget(void)
{
    nsresult rv = NS_OK;

    mTarget = nsnull;

    return rv;
}
