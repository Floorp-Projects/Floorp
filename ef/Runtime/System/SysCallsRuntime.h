/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _SYSCALLS_RUNTIME_H_
#define _SYSCALLS_RUNTIME_H_

#include "Fundamentals.h"

#define SYSCALL_FUNC(inReturnType) NS_NATIVECALL(inReturnType)

/* Implementation of sysCalls needed by the runtime. */

extern "C" {

NS_EXTERN SYSCALL_FUNC(void) sysThrow(const JavaObject &inObject);

NS_EXTERN SYSCALL_FUNC(void) sysCheckArrayStore(const JavaArray *arr, const JavaObject *obj);

NS_EXTERN SYSCALL_FUNC(void) sysMonitorExit(JavaObject *obj);
NS_EXTERN SYSCALL_FUNC(void) sysMonitorEnter(JavaObject *obj);

NS_EXTERN SYSCALL_FUNC(void*) sysNew(const Class &inClass);

NS_EXTERN SYSCALL_FUNC(void*) sysNewNDArray(const Type *type, JavaArray* lengths);
NS_EXTERN SYSCALL_FUNC(void*) sysNew3DArray(const Type *type, Int32 length1, Int32 length2, Int32 length);
NS_EXTERN SYSCALL_FUNC(void*) sysNew2DArray(const Type *type, Int32 length1, Int32 length2);

NS_EXTERN SYSCALL_FUNC(void*) sysNewByteArray(Int32 length);
NS_EXTERN SYSCALL_FUNC(void*) sysNewCharArray(Int32 length);
NS_EXTERN SYSCALL_FUNC(void*) sysNewIntArray(Int32 length);
NS_EXTERN SYSCALL_FUNC(void*) sysNewLongArray(Int32 length);
NS_EXTERN SYSCALL_FUNC(void*) sysNewShortArray(Int32 length);
NS_EXTERN SYSCALL_FUNC(void*) sysNewBooleanArray(Int32 length);
NS_EXTERN SYSCALL_FUNC(void*) sysNewDoubleArray(Int32 length);
NS_EXTERN SYSCALL_FUNC(void*) sysNewFloatArray(Int32 length);
NS_EXTERN SYSCALL_FUNC(void*) sysNewObjectArray(const Type *type, Int32 length);

// The following are not called directly from primitives 
NS_EXTERN SYSCALL_FUNC(void *) sysNewPrimitiveArray(TypeKind tk, Int32 length);

NS_EXTERN SYSCALL_FUNC(void) sysThrowNullPointerException();
NS_EXTERN SYSCALL_FUNC(void) sysThrowArrayIndexOutOfBoundsException();
NS_EXTERN SYSCALL_FUNC(void) sysThrowClassCastException();
NS_EXTERN SYSCALL_FUNC(void) sysThrowArrayStoreException();

NS_EXTERN SYSCALL_FUNC(void) sysThrowNamedException(const char *name);

}

#endif /* _SYSCALLS_RUNTIME_H_ */
