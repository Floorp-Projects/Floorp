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
#include "bcJavaGlobal.h"
#include "prenv.h"


JavaVM *bcJavaGlobal::jvm = NULL;

#ifdef XP_PC
#define PATH_SEPARATOR ';'
#else
#define PATH_SEPARATOR ':'
#endif

JNIEnv * bcJavaGlobal::GetJNIEnv(void) {
    JNIEnv * res;
    if (!jvm) {
	StartJVM();
    }
    if (jvm) {
	jvm->AttachCurrentThread(&res,NULL);
    }
    printf("--bcJavaGlobal::GetJNIEnv \n");
    return res;
}

void bcJavaGlobal::StartJVM() {
    JNIEnv *env = NULL;
    jint res;
    jsize jvmCount;
    JNI_GetCreatedJavaVMs(&jvm, 1, &jvmCount);
    if (jvmCount) {
        return;
    }
    JDK1_1InitArgs vm_args;
    char classpath[1024];
    JNI_GetDefaultJavaVMInitArgs(&vm_args);
    vm_args.version = 0x00010001;
    /* Append USER_CLASSPATH to the default system class path */
    sprintf(classpath, "%s%c%s",
            vm_args.classpath, PATH_SEPARATOR, PR_GetEnv("CLASSPATH"));
    printf("--[c++] classpath %s\n",classpath);
    char **props = new char*[2];
    props[0]="java.compiler=NONE";
    props[1]=0;
    vm_args.properties = props;
    vm_args.classpath = classpath;
    /* Create the Java VM */
    res = JNI_CreateJavaVM(&jvm, &env, &vm_args);
    printf("--bcJavaGlobal::StartJVM jvm started\n");
}




