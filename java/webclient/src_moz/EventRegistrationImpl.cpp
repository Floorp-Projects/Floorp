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
 *               Ashutosh Kulkarni <ashuk@eng.sun.com>
 */

#include "org_mozilla_webclient_impl_wrapper_0005fnative_EventRegistrationImpl.h"

#include "ns_util.h"
#include "NativeBrowserControl.h"
#include "EmbedProgress.h"

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_EventRegistrationImpl_nativeSetCapturePageInfo
(JNIEnv *env, jobject obj, jint nativeBCPtr, jboolean newState)
{

    NativeBrowserControl* nativeBrowserControl = (NativeBrowserControl *) nativeBCPtr;
    nativeBrowserControl->mProgress->SetCapturePageInfo(newState);

}

