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

#include "EventRegistration.h"

#include "nsActions.h"
#include "nsCOMPtr.h"
#include "DocumentLoaderObserverImpl.h"
#include "DOMMouseListenerImpl.h"

static NS_DEFINE_IID(kIDocumentLoaderObserverImplIID, NS_IDOCLOADEROBSERVERIMPL_IID);

void addDocumentLoadListener(JNIEnv *env, WebShellInitContext *initContext,
                             jobject listener)
{
    if (initContext == nsnull) {
        ::util_ThrowExceptionToJava(env, "Exception: null initContext passed toaddDocumentLoadListener");
        return;
    }

    if (initContext->initComplete) {


        PR_ASSERT(initContext->nativeEventThread);
        nsresult rv;
        nsCOMPtr<nsIDocumentLoaderObserver> curObserver;
        nsCOMPtr<DocumentLoaderObserverImpl> myListener = nsnull;
        
        PR_ASSERT(nsnull != initContext->docShell);

        // tricky logic to accomodate "piggybacking" a mouseListener. 

        // See if there already is a DocListener
        rv = initContext->docShell->GetDocLoaderObserver(getter_AddRefs(curObserver));
        if (NS_FAILED(rv) || !curObserver) {
            // if there is no listener, we need to create and add it now
            
            // create the c++ "peer" for the DocumentLoadListener, which is an
            // nsIDocumentLoaderObserver.
            curObserver =  new DocumentLoaderObserverImpl(env, initContext);
            if (nsnull == curObserver) {
                return;
            }
            
            wsAddDocLoaderObserverEvent *actionEvent = 
                new wsAddDocLoaderObserverEvent(initContext->docShell,
                                                curObserver);
            
            PLEvent			* event       = (PLEvent*) *actionEvent;
            
            ::util_PostSynchronousEvent(initContext, event);
        }

        if (curObserver) {
            // if we have an observer (either just created, or from mozilla),
            // install the target.

            rv = curObserver->QueryInterface(kIDocumentLoaderObserverImplIID,
                                             getter_AddRefs(myListener));
            if (NS_SUCCEEDED(rv) && myListener) {
                myListener->SetTarget(listener);
            }
        }
    }
}

void addMouseListener(JNIEnv *env, WebShellInitContext *initContext,
                      jobject listener)
{
    if (initContext == nsnull) {
        ::util_ThrowExceptionToJava(env, "Exception: null initContext passed toaddDocumentLoadListener");
        return;
    }

    if (initContext->initComplete) {
        nsresult rv;
        nsCOMPtr<nsIDocumentLoaderObserver> curObserver;
        nsCOMPtr<DocumentLoaderObserverImpl> myListener = nsnull;

        PR_ASSERT(nsnull != initContext->docShell);

        // See if there already is a DocListener
        rv = initContext->docShell->GetDocLoaderObserver(getter_AddRefs(curObserver));
        if (NS_SUCCEEDED(rv) && curObserver) {

            // if so, se if it's something we added
            rv = curObserver->QueryInterface(kIDocumentLoaderObserverImplIID,
                                         getter_AddRefs(myListener));
        }
        else {

            // if not, we need to create a listener
            myListener = new DocumentLoaderObserverImpl(env, initContext);
            // note that we don't call setTarget, since this 
            // DocumentLoaderObserver is just for getting mouse events

            // install our listener into mozilla
            wsAddDocLoaderObserverEvent *actionEvent = 
                new wsAddDocLoaderObserverEvent(initContext->docShell,
                                                myListener);
            
            PLEvent			* event       = (PLEvent*) *actionEvent;
            
            ::util_PostSynchronousEvent(initContext, event);
        }
         
        if (nsnull == myListener) {
            // either the new failed, or the currently installed listener
            // wasn't installed by us.  Either way, do nothing.
            return;
        }
        // we have a listener
        
        nsCOMPtr<nsIDOMMouseListener> mouseListener = 
            new DOMMouseListenerImpl(env, initContext, listener);

        myListener->AddMouseListener(mouseListener);
    }
}
