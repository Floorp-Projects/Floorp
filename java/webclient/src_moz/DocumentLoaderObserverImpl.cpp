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
  NULL
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
    if (NULL == gVm) { // declared in jni_util.h
      env->GetJavaVM(&gVm);  // save this vm reference away for the callback!
    }

#ifdef XP_PC
    // debug: edburns:
    DWORD nativeThreadID = GetCurrentThreadId();
    printf("debug: edburns: DocumentLoaderObserverImpl ctor nativeThreadID: %d\n",
           nativeThreadID);
#endif
    
	char *currentThreadName = NULL;

	if (NULL != (currentThreadName = util_GetCurrentThreadName(env))) {
        printf("debug: edburns: DocumentLoaderObserverImpl ctor threadName: %s\n",
               currentThreadName);
        delete currentThreadName;
	}

    if (-1 == maskValues[0]) {
      InitializeMaskValues();
    }
    mRefCnt = 1; // PENDING(edburns): not sure about how right this is to do.
}

void DocumentLoaderObserverImpl::InitializeMaskValues()
{
    // if we don't have a VM, do nothing
    if (NULL == gVm) {
      return;
    }

    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION_1_2);

    if (NULL == env) {
      return;
    }

    jclass documentLoadEventClass = env->FindClass("org/mozilla/webclient/DocumentLoadEvent");

    if (NULL == documentLoadEventClass) {
      return;
    }

    int i = 0;
    jfieldID fieldID;

    while (NULL != maskNames[i]) {
      fieldID = env->GetStaticFieldID(documentLoadEventClass, maskNames[i],
				      "J");

      if (NULL == fieldID) {
	return;
      }

      maskValues[i] = env->GetStaticLongField(documentLoadEventClass, fieldID);
      i++;
    }
}
 
NS_IMETHODIMP DocumentLoaderObserverImpl::QueryInterface(REFNSIID aIID, void** aInstance)
{
  if (NULL == aInstance)
    return NS_ERROR_NULL_POINTER;

  *aInstance = NULL;

 
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
    printf("DocumentLoaderObserverImpl.cpp: OnStartDocumentLoad\n");
    util_SendEventToJava(mInitContext->env, 
                         mInitContext->nativeEventThread, mTarget, 
			 maskValues[START_DOCUMENT_LOAD_EVENT_MASK]);
    
    return NS_OK;
}

NS_IMETHODIMP DocumentLoaderObserverImpl::OnEndDocumentLoad(nsIDocumentLoader* loader, 
							    nsIChannel* channel, 
							    nsresult aStatus) 
{
    printf("!!DocumentLoaderObserverImpl.cpp: OnEndDocumentLoad\n");

    util_SendEventToJava(mInitContext->env, 
                         mInitContext->nativeEventThread, mTarget, 
                         maskValues[END_DOCUMENT_LOAD_EVENT_MASK]);
    return NS_OK;
}

NS_IMETHODIMP DocumentLoaderObserverImpl::OnStartURLLoad(nsIDocumentLoader* loader, 
							 nsIChannel* channel) 
{
    printf("!DocumentLoaderObserverImpl: OnStartURLLoad\n");
    util_SendEventToJava(mInitContext->env, mInitContext->nativeEventThread, mTarget, 
			 maskValues[START_URL_LOAD_EVENT_MASK]);
    
    return NS_OK;
}  

NS_IMETHODIMP DocumentLoaderObserverImpl::OnProgressURLLoad(nsIDocumentLoader* loader,
			       nsIChannel* channel, 
			       PRUint32 aProgress, 
                               PRUint32 aProgressMax) 
{
    printf("!DocumentLoaderObserverImpl: OnProgressURLLoad\n");
    util_SendEventToJava(mInitContext->env, mInitContext->nativeEventThread, mTarget, 
			 maskValues[PROGRESS_URL_LOAD_EVENT_MASK]);


    return NS_OK;
}

NS_IMETHODIMP DocumentLoaderObserverImpl::OnStatusURLLoad(nsIDocumentLoader* loader, 
			     nsIChannel* channel, 
			     nsString& aMsg) 
{
    printf("!DocumentLoaderObserverImpl: OnStatusURLLoad\n");
    util_SendEventToJava(mInitContext->env, mInitContext->nativeEventThread, mTarget, 
			 maskValues[STATUS_URL_LOAD_EVENT_MASK]);


    return NS_OK;
}

NS_IMETHODIMP DocumentLoaderObserverImpl::OnEndURLLoad(nsIDocumentLoader* loader, 
			  nsIChannel* channel, 
			  nsresult aStatus) 
{
    printf("!DocumentLoaderObserverImpl: OnEndURLLoad\n");
    util_SendEventToJava(mInitContext->env, mInitContext->nativeEventThread, mTarget, 
			 maskValues[END_URL_LOAD_EVENT_MASK]);

    return NS_OK;
}

NS_IMETHODIMP DocumentLoaderObserverImpl::HandleUnknownContentType(nsIDocumentLoader* loader,
				      nsIChannel* channel, 
				      const char *aContentType,
				      const char *aCommand) 
{
    printf("!DocumentLoaderObserverImpl: HandleUnknownContentType\n");
    util_SendEventToJava(mInitContext->env, mInitContext->nativeEventThread, mTarget, 
			 maskValues[UNKNOWN_CONTENT_EVENT_MASK]);
    
    return NS_OK;
}



