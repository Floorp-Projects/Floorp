/* 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are Copyright (C) 1999 Sun Microsystems,
 * Inc. All Rights Reserved. 
 */
#include "org_mozilla_util_Debug.h"
/*
 * Class:     org_mozilla_util_Debug
 * Method:    print
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_util_Debug_print
    (JNIEnv *env, jclass, jstring jval) {
    jboolean iscopy = JNI_FALSE;
    const char* cvalue = env->GetStringUTFChars(jval, &iscopy);
    printf("%s",cvalue);
    if (iscopy == JNI_TRUE) {
	env->ReleaseStringUTFChars(jval, cvalue);
    }
}


