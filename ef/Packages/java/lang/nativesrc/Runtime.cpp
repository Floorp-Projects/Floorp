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
#include "java_lang_Runtime.h"
#include <stdio.h>
#include "Thread.h"

extern "C" {
/*
 * Class : java/lang/Runtime
 * Method : exitInternal
 * Signature : (I)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_Runtime_exitInternal(Java_java_lang_Runtime *, int32 code)
{
	Thread::exit();
}


/*
 * Class : java/lang/Runtime
 * Method : runFinalizersOnExit0
 * Signature : (Z)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_Runtime_runFinalizersOnExit0(uint32 /* bool */)
{
  printf("Netscape_Java_java_lang_Runtime_runFinalizersOnExit0() not implemented");

}


/*
 * Class : java/lang/Runtime
 * Method : execInternal
 * Signature : ([Ljava/lang/String;[Ljava/lang/String;)Ljava/lang/Process;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Process *)
Netscape_Java_java_lang_Runtime_execInternal(Java_java_lang_Runtime *, ArrayOf_Java_java_lang_String *, ArrayOf_Java_java_lang_String *)
{
  printf("Netscape_Java_java_lang_Runtime_execInternal() not implemented");
  return 0;
}


/*
 * Class : java/lang/Runtime
 * Method : freeMemory
 * Signature : ()J
 */
NS_EXPORT NS_NATIVECALL(int64)
Netscape_Java_java_lang_Runtime_freeMemory(Java_java_lang_Runtime *)
{
  printf("Netscape_Java_java_lang_Runtime_freeMemory() not implemented");
  return 0;
}


/*
 * Class : java/lang/Runtime
 * Method : totalMemory
 * Signature : ()J
 */
NS_EXPORT NS_NATIVECALL(int64)
Netscape_Java_java_lang_Runtime_totalMemory(Java_java_lang_Runtime *)
{
  printf("Netscape_Java_java_lang_Runtime_totalMemory() not implemented");
  return 0;
}


/*
 * Class : java/lang/Runtime
 * Method : gc
 * Signature : ()V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_Runtime_gc(Java_java_lang_Runtime *)
{
  printf("Netscape_Java_java_lang_Runtime_gc() not implemented");
}


/*
 * Class : java/lang/Runtime
 * Method : runFinalization
 * Signature : ()V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_Runtime_runFinalization(Java_java_lang_Runtime *)
{
  printf("Netscape_Java_java_lang_Runtime_runFinalization() not implemented");
}


/*
 * Class : java/lang/Runtime
 * Method : traceInstructions
 * Signature : (Z)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_Runtime_traceInstructions(Java_java_lang_Runtime *, uint32 /* bool */)
{
  printf("Netscape_Java_java_lang_Runtime_traceInstructions() not implemented");
}


/*
 * Class : java/lang/Runtime
 * Method : traceMethodCalls
 * Signature : (Z)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_Runtime_traceMethodCalls(Java_java_lang_Runtime *, uint32 /* bool */)
{
  printf("Netscape_Java_java_lang_Runtime_traceMethodCalls() not implemented");
}


/*
 * Class : java/lang/Runtime
 * Method : initializeLinkerInternal
 * Signature : ()Ljava/lang/String;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_String *)
Netscape_Java_java_lang_Runtime_initializeLinkerInternal(Java_java_lang_Runtime *)
{
  printf("Netscape_Java_java_lang_Runtime_initializeLinkerInternal() not implemented");
  return 0;
}


/*
 * Class : java/lang/Runtime
 * Method : buildLibName
 * Signature : (Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_String *)
Netscape_Java_java_lang_Runtime_buildLibName(Java_java_lang_Runtime *, Java_java_lang_String *, Java_java_lang_String *)
{
  printf("Netscape_Java_java_lang_Runtime_buildLibName() not implemented");
  return 0;
}


/*
 * Class : java/lang/Runtime
 * Method : loadFileInternal
 * Signature : (Ljava/lang/String;)I
 */
NS_EXPORT NS_NATIVECALL(int32)
Netscape_Java_java_lang_Runtime_loadFileInternal(Java_java_lang_Runtime *, Java_java_lang_String *)
{
  printf("Netscape_Java_java_lang_Runtime_loadFileInternal() not implemented");
  return 0;
}


} /* extern "C" */

