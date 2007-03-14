/* 
 The contents of this file are subject to the Mozilla Public
 License Version 1.1 (the "License"); you may not use this file
 except in compliance with the License. You may obtain a copy of
 the License at http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS
 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 implied. See the License for the specific language governing
 rights and limitations under the License.

 The Original Code is mozilla.org code.

 The Initial Developer of the Original Code is Sun Microsystems,
 Inc. Portions created by Sun are
 Copyright (C) 1999 Sun Microsystems, Inc. All
 Rights Reserved.

 Contributor(s): 
*/

#include<stdio.h>
#include"prlog.h"
#include"nativeDOMProxyListener.h"
#include"nsIDOMNode.h"
#include"nsIDOMEventTarget.h"
#include"nsIDOMEventListener.h"
#include"javaDOMEventsGlobals.h"

#include "nsCOMPtr.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDOMEventListener, NS_IDOMEVENTLISTENER_IID);

NativeDOMProxyListener::NativeDOMProxyListener(JNIEnv *env, jobject jlistener) 
{  
    mRefCnt = 0;  
    listener = env->NewGlobalRef(jlistener);

    if (env->GetJavaVM(&vm) != 0) 
        PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
            ("NativeDOMProxyListener: Can't objant jvm\n"));
}  


//should be called oly from NativeDOMProxyListener::Release
NativeDOMProxyListener::~NativeDOMProxyListener() 
{
    JNIEnv *env;

    if (vm->AttachCurrentThread(&env, NULL) != 0) 
        PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
            ("NativeDOMProxyListener: Can't attach current thread to JVM\n"));

    env->DeleteGlobalRef(listener);
    if (env->ExceptionOccurred()) {
        PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
            ("NativeDOMProxyListener::Destroy: failed to delete EventListener global ref %x\n", 
             listener));
        return;
    }
}

NS_IMETHODIMP NativeDOMProxyListener::HandleEvent(nsIDOMEvent* aEventIn) 
{
    jobject jevent;
    JNIEnv *env;
    nsCOMPtr<nsIDOMEvent> aEvent = aEventIn;

    if (vm->AttachCurrentThread(&env, NULL) != 0) {
        PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
            ("NativeDOMProxyListener:HandleEvent Can't attach current thread to JVM\n"));
        return NS_ERROR_FAILURE;
    }

    jevent = JavaDOMEventsGlobals::CreateEventSubtype(env, aEvent);

    if (!jevent) {
        PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING,
            ("NativeDOMProxyListener::HandleEvent Can't create java event object"));
        return NS_ERROR_FAILURE;
    }

    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING,
	   ("NativeDOMProxyListener::HandleEvent About to call into java.\n listener: %p\n eventListenerHandleEventMID: %p\n jevent: %p\n", listener, JavaDOMEventsGlobals::eventListenerHandleEventMID, jevent));
    
    env->CallVoidMethod(listener,
                        JavaDOMEventsGlobals::eventListenerHandleEventMID,
                        jevent);    

    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING,
	   ("NativeDOMProxyListener::HandleEvent returned from java"));


    if (env->ExceptionOccurred()) {
        PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
            ("NativeDOMProxyListener::HandleEvent: failed to call EventListener %x\n", 
             listener));
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

NS_IMETHODIMP NativeDOMProxyListener::QueryInterface(const nsIID &aIID, void **aResult)
{
    if (aResult == NULL) {  
        return NS_ERROR_NULL_POINTER;  
    }  

    // Always NULL result, in case of failure  
    *aResult = NULL;  

    if (aIID.Equals(kISupportsIID)) {  
        *aResult = (void *) this;  
    } else if (aIID.Equals(kIDOMEventListener)) {  
        *aResult = (void *) this;  
    }  

    if (aResult != NULL) {  
        return NS_ERROR_NO_INTERFACE;  
    }  

    AddRef();  
    return NS_OK;  
}

NS_IMETHODIMP_(nsrefcnt) NativeDOMProxyListener::AddRef()
{
    return mRefCnt++;
}

NS_IMETHODIMP_(nsrefcnt) NativeDOMProxyListener::Release()
{
    if (--mRefCnt == 0) {  
        delete this;  
        return 0; // Don't access mRefCnt after deleting! 
    }      
    return mRefCnt;
}
