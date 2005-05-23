/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): edburns <edburns@acm.org>
 */


/*
 * CocoaBrowserControlCanvas.cpp
 */

#include "org_mozilla_webclient_impl_wrapper_0005fnative_CocoaBrowserControlCanvas.h"

#include "jni_util.h" //for throwing Exceptions to Java

#include "CocoaBrowserControlCanvas.h"

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_CocoaBrowserControlCanvas
 * Method:    getHandleToPeer
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CocoaBrowserControlCanvas_getHandleToPeer
  (JNIEnv *env, jobject canvas) {

    jint result = -1;

    result = CocoaBrowserControlCanvas::cocoaGetHandleToPeer(env, canvas);
    return result;
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CocoaBrowserControlCanvas_paintMe(JNIEnv *env, jobject canvas, jobject graphics) {
    CocoaBrowserControlCanvas::paintMe(env, canvas, graphics);
}
