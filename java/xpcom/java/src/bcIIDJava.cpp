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
#include "bcIIDJava.h"
#include "bcJavaGlobal.h"

jclass bcIIDJava::iidClass = NULL;
jmethodID bcIIDJava::iidInitMID = NULL;
jmethodID bcIIDJava::getStringMID = NULL;


void bcIIDJava::Init(void) {
    JNIEnv * env = bcJavaGlobal::GetJNIEnv();
    if (env) {
	if (!(iidClass = env->FindClass("org/mozilla/xpcom/IID"))
	    || !(iidClass = (jclass) env->NewGlobalRef(iidClass))) {
	    env->ExceptionDescribe();
	    Destroy();
	    return;  
	}
	if (!(iidInitMID = env->GetMethodID(iidClass,"<init>","(Ljava/lang/String;)V"))) {
	    env->ExceptionDescribe();
	    Destroy();
	    return;
	}
	if (!(getStringMID = env->GetMethodID(iidClass,"getString","()Ljava/lang/String;"))) {
	    env->ExceptionDescribe();
	    Destroy();
	    return;
	}
    }
}
void bcIIDJava::Destroy() {
    JNIEnv * env = bcJavaGlobal::GetJNIEnv();
    if (env) {
	if (iidClass) {
	    env->DeleteGlobalRef(iidClass);
	    iidClass = NULL;
	}
    }
}

jobject  bcIIDJava::GetObject(nsIID *iid) {
    JNIEnv * env = bcJavaGlobal::GetJNIEnv();
    if (!iid 
	|| !env ) {
	return NULL;
    }
    if (!iidClass) {
	Init();
    }
    char *str = iid->ToString(); //nb free ?
    jstring jstr = NULL;
    if (str) {
	char *siid = str+1; //we do need to have it. The format is {_xxx-xxx-xxx_}
	siid[strlen(siid)-1] = 0;
	jstr = env->NewStringUTF((const char *)siid);
    }
    return env->NewObject(iidClass,iidInitMID,jstr);
}

jclass bcIIDJava::GetClass() {
    return iidClass;
}

nsIID bcIIDJava::GetIID(jobject obj) {
    nsIID iid;
    JNIEnv * env = bcJavaGlobal::GetJNIEnv();
    if (env) {
	if (!iidClass) {
	    Init();
	}
	jstring jstr = (jstring)env->CallObjectMethod(obj, getStringMID);  
	const char * str = NULL;
    str = env->GetStringUTFChars(jstr,NULL);
	iid.Parse(str);
	env->ReleaseStringUTFChars(jstr,str);

    }
    return iid;
}
