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

#ifndef EventRegistration_h
#define EventRegistration_h

#include "jni_util.h"

/**

 * This file contains methods to add and remove listeners.

 */

/**

 * This function creates an instance of DocumentLoaderObserverImpl,
 * which is the shim between the mozilla nsIDocumentLoaderObserver class
 * and the Java DocumentLoadListener interface.  See
 * DocumentLoaderObserverImpl.h

 * PENDING(): implement removeDocumentLoadListener

 * PENDING(): implement the ability to have multiple listener instances
 * per listener types, all of which get notified.

 */

void addDocumentLoadListener(JNIEnv *env, WebShellInitContext *initContext,
                             jobject listener);

/**

 * This function creates an instance of DOMMouseListenerImpl,
 * which is the shim between the mozilla nsIDOMMouseListener class
 * and the Java MouseListener interface.  See
 * DocumentLoaderObserverImpl.h

 * PENDING(): implement removeMouseListener

 * PENDING(): implement the ability to have multiple listener instances
 * per listener types, all of which get notified.

 */

void addMouseListener(JNIEnv *env, WebShellInitContext *initContext,
                      jobject listener);


#endif
