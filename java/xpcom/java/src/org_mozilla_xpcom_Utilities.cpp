/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Igor Kushnirskiy <idk@eng.sun.com>
 */
#include "nsISupports.h"
#include "org_mozilla_xpcom_Utilities.h"
#include "bcIORB.h"
#include "bcICall.h"
#include "bcDefs.h"


/*
 * Class:     org_mozilla_xpcom_Utilities
 * Method:    callMethodByIndex
 * Signature: (JILjava/lang/String;J[Ljava/lang/Object;)Ljava/lang/Object;
 */

JNIEXPORT jobject JNICALL Java_org_mozilla_xpcom_Utilities_callMethodByIndex
    (JNIEnv *env, jclass clazz, jlong _oid, jint mid, jstring jiid, jlong _orb, jobjectArray) {
    bcIORB * orb = (bcIORB*) _orb;
    bcOID oid = (bcOID)_oid;
    nsIID iid;
    printf("--[c++] jni %d\n",(int)mid);
    const char * str = NULL;
    str = env->GetStringUTFChars(jiid,NULL);
    iid.Parse(str);
    env->ReleaseStringUTFChars(jiid,str);
    bcICall *call = orb->CreateCall(&iid, &oid, mid);
    orb->SendReceive(call);
    return NULL;
}

