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
#include "DocumentLoaderObserverImpl.h"

void addDocumentLoadListener(JNIEnv *env, WebShellInitContext *initContext,
                             jobject listener)
{
    if (initContext == nsnull) {
        ::util_ThrowExceptionToJava(env, "Exception: null initContext passed toaddDocumentLoadListener");
        return;
    }

    if (initContext->initComplete) {
        // Assert (nsnull != initContext->nativeEventThread)
        
        // create the c++ "peer" for the DocumentLoadListener, which is an
        // nsIDocumentLoaderObserver.
        DocumentLoaderObserverImpl *observerImpl = 
            new DocumentLoaderObserverImpl(env, initContext, listener);
        
        wsAddDocLoaderObserverEvent *actionEvent = 
            new wsAddDocLoaderObserverEvent(initContext->docShell,
                                            observerImpl);
        
        PLEvent			* event       = (PLEvent*) *actionEvent;
        
        ::util_PostEvent(initContext, event);
    }
}
