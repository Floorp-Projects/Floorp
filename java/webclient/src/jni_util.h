/*
 * @(#)jni_util.h	1.24 98/09/02
 *
 * Copyright 1997, 1998 by Sun Microsystems, Inc.,
 * 901 San Antonio Road, Palo Alto, California, 94303, U.S.A.
 * All rights reserved.
 *
 * This software is the confidential and proprietary information
 * of Sun Microsystems, Inc. ("Confidential Information").  You
 * shall not disclose such Confidential Information and shall use
 * it only in accordance with the terms of the license agreement
 * you entered into with Sun.
 */

#ifndef JNI_UTIL_H
#define JNI_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif


/* Invoke an instance method by name. 
 */
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

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* JNI_UTIL_H */
