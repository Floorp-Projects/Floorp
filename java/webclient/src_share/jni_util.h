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
 *               Ann Sunhachawee
 */


/**

 * Util methods

 */

#ifndef jni_util_h
#define jni_util_h

#include <jni.h>

//
// Local classes
// 

/**

 * This struct contains all non browser specific per-window data.  The
 * WebShellInitContext struct, in the src_moz and src_ie directories
 * contains a pointer to a ShareInitContext struct.

 */

struct ShareInitContext {
    jclass propertiesClass;
};

extern JavaVM *gVm; // defined in jni_util.cpp

#ifndef nsnull
#define nsnull 0
#endif

void    util_InitializeShareInitContext(void *initContext);
void    util_DeallocateShareInitContext(void *initContext);

void    util_ThrowExceptionToJava (JNIEnv * env, const char * message);

/**

 * simply call the java method nativeEventOccurred on
 * eventRegistrationImpl, passing webclientEventListener and eventType
 * as arguments.

 */

void    util_SendEventToJava(JNIEnv *env, jobject eventRegistrationImpl,
                             jobject webclientEventListener, 
                             jlong eventType,
                             jobject eventData);

char *util_GetCurrentThreadName(JNIEnv *env);

void util_DumpJavaStack(JNIEnv *env);

//
// Functions to wrap JNIEnv functions.
//

#include "jni_util_export.h"

jobject util_NewGlobalRef(JNIEnv *env, jobject toAddRef);

void util_DeleteGlobalRef(JNIEnv *env, jobject toAddRef);

jthrowable util_ExceptionOccurred(JNIEnv *env);

jint util_GetJavaVM(JNIEnv *env, JavaVM **vm);

jclass util_FindClass(JNIEnv *env, const char *fullyQualifiedClassName);

jfieldID util_GetStaticFieldID(JNIEnv *env, jclass clazz, 
                               const char *fieldName, 
                               const char *signature);

jlong util_GetStaticLongField(JNIEnv *env, jclass clazz, jfieldID id);

jboolean util_IsInstanceOf(JNIEnv *env, jobject obj, jclass clazz);

jint util_GetIntValueFromInstance(JNIEnv *env, jobject instance,
                                  const char *fieldName);

void util_SetIntValueForInstance(JNIEnv *env, jobject instance,
                                 const char *fieldName, jint newValue);

/**

 * A JNI wrapper to create a java.util.Properties object, or the
 * equivalent object in the BAL case.

 */

jobject util_CreatePropertiesObject(JNIEnv *env, jobject reserved_NotUsed);

/**

 * A JNI wrapper to destroy the object from CreatePropertiesObject

 */

void util_DestroyPropertiesObject(JNIEnv *env, jobject propertiesObject,
                                  jobject reserved_NotUsed);

/**

 * A JNI wrapper to clear the object from CreatePropertiesObject

 */

void util_ClearPropertiesObject(JNIEnv *env, jobject propertiesObject,
                                jobject reserved_NotUsed);

/**

 * A JNI wrapper for storing a name/value pair into the Properties
 * object created by CreatePropertiesObject

 */

void util_StoreIntoPropertiesObject(JNIEnv *env, jobject propertiesObject,
                                    jobject name, jobject value, 
                                    jobject reserved);


//
// Functions provided by the browser specific native code
//

void util_LogMessage(int level, const char *fmt);

void util_Assert(void *test);

//
// Functions from secret JDK files
//

JNIEXPORT jvalue JNICALL
JNU_CallMethodByName(JNIEnv *env, 
					 jboolean *hasException,
					 jobject obj, 
					 const char *name,
					 const char *signature,
					 ...);

JNIEXPORT jvalue JNICALL
JNU_CallMethodByNameV(JNIEnv *env, 
					  jboolean *hasException,
					  jobject obj, 
					  const char *name,
					  const char *signature, 
					  va_list args);

JNIEXPORT void * JNICALL
JNU_GetEnv(JavaVM *vm, jint version);

#endif // jni_util_h
