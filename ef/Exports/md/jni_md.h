/*
 * @(#)jni_md.h	1.6 97/08/07
 * 
 * Copyright 1993-1997 Sun Microsystems, Inc. 901 San Antonio Road, 
 * Palo Alto, California, 94303, U.S.A.  All Rights Reserved.
 * 
 * This software is the confidential and proprietary information of Sun
 * Microsystems, Inc. ("Confidential Information").  You shall not
 * disclose such Confidential Information and shall use it only in
 * accordance with the terms of the license agreement you entered into
 * with Sun.
 * 
 * CopyrightVersion 1.2
 * 
 */

#ifndef JNI_MD_H
#define JNI_MD_H

#if defined(_WIN32)
#define JNIEXPORT __declspec(dllexport)
#define JNIIMPORT __declspec(dllimport)
#define JNICALL(returnType) returnType __stdcall
#define JNIFUNCPTR(returnType, name) returnType (__stdcall *name)

typedef long jint;
typedef __int64 jlong;
typedef signed char jbyte;

#elif defined(LINUX) || defined(FREEBSD)
#define JNIEXPORT
#define JNIIMPORT
#define JNICALL(returnType) __attribute__((stdcall)) returnType
#define JNIFUNCPTR(returnType, name) __attribute__((stdcall)) returnType (*name)

typedef long jint;
typedef long long jlong;
typedef signed char jbyte;
#else
#define JNIEXPORT
#define JNIIMPORT
#define JNICALL(X) X
#define JNIFUNCPTR(returnType, name) returnType (*name)

typedef long jint;
typedef long long jlong;
typedef signed char jbyte;
#endif

#endif /* JNI_MD_H */
