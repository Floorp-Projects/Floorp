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

#include "org_mozilla_xpcom_Debug.h"
#include "bcJavaGlobal.h"
/*
 * Class:     org_mozilla_xpcom_Debug
 * Method:    log
 * Signature: (Ljava/lang/String;)V
 */

JNIEXPORT void JNICALL Java_org_mozilla_xpcom_Debug_log
(JNIEnv *env, jclass, jstring jstr) {
    PRLogModuleInfo *log = bcJavaGlobal::GetLog();
    char * str = NULL;
    str = (char*)env->GetStringUTFChars(jstr,NULL);
    PR_LOG(log, PR_LOG_DEBUG, ("%s\n",str));
    env->ReleaseStringUTFChars(jstr,str);
}
