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
 * Contributor(s):  Ed Burns <edburns@acm.org>
 */

#include "org_mozilla_webclient_impl_wrapper_0005fnative_ISupportsPeer.h"

#include "nsCOMPtr.h"
#include "nsISupports.h"

// 
// JNI methods
//

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_ISupportsPeer_nativeAddRef
(JNIEnv *env, jobject obj, jint nativeISupportsImpl)
{
    nsCOMPtr<nsISupports> supports = (nsISupports *) nativeISupportsImpl;
    ((nsISupports *)supports.get())->AddRef();
}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_ISupportsPeer_nativeRelease
(JNIEnv *env, jobject obj, jint nativeISupportsImpl)
{
    nsCOMPtr<nsISupports> supports = (nsISupports *) nativeISupportsImpl;
    ((nsISupports *)supports.get())->Release();
}


//
// Local functions
//

