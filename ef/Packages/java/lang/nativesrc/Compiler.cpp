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
#include "java_lang_Compiler.h"
#include <stdio.h>

extern "C" {
/*
 * Class : java/lang/Compiler
 * Method : initialize
 * Signature : ()V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_Compiler_initialize()
{
  printf("Netscape_Java_java_lang_Compiler_initialize() not implemented");
}


/*
 * Class : java/lang/Compiler
 * Method : compileClass
 * Signature : (Ljava/lang/Class;)Z
 */
NS_EXPORT NS_NATIVECALL(uint32 /* bool */)
Netscape_Java_java_lang_Compiler_compileClass(Java_java_lang_Class *)
{
  printf("Netscape_Java_java_lang_Compiler_compileClass() not implemented");
  return 0;
}


/*
 * Class : java/lang/Compiler
 * Method : compileClasses
 * Signature : (Ljava/lang/String;)Z
 */
NS_EXPORT NS_NATIVECALL(uint32 /* bool */)
Netscape_Java_java_lang_Compiler_compileClasses(Java_java_lang_String *)
{
  printf("Netscape_Java_java_lang_Compiler_compileClasses() not implemented");
  return 0;
}


/*
 * Class : java/lang/Compiler
 * Method : command
 * Signature : (Ljava/lang/Object;)Ljava/lang/Object;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Object *)
Netscape_Java_java_lang_Compiler_command(Java_java_lang_Object *)
{
  printf("Netscape_Java_java_lang_Compiler_command() not implemented");
  return 0;
}


/*
 * Class : java/lang/Compiler
 * Method : enable
 * Signature : ()V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_Compiler_enable()
{
  printf("Netscape_Java_java_lang_Compiler_enable() not implemented");
}


/*
 * Class : java/lang/Compiler
 * Method : disable
 * Signature : ()V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_Compiler_disable()
{
  printf("Netscape_Java_java_lang_Compiler_disable() not implemented");
}

} /* extern "C" */

