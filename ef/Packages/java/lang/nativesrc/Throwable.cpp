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
#include "java_lang_Throwable.h"
#include <stdio.h>

extern "C" {
/*
 * Class : java/lang/Throwable
 * Method : printStackTrace0
 * Signature : (Ljava/lang/Object;)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_Throwable_printStackTrace0(Java_java_lang_Throwable *, Java_java_lang_Object *)
{
  printf("Netscape_Java_java_lang_Throwable_printStackTrace0() not implemented");
}


/*
 * Class : java/lang/Throwable
 * Method : fillInStackTrace
 * Signature : ()Ljava/lang/Throwable;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Throwable *)
Netscape_Java_java_lang_Throwable_fillInStackTrace(Java_java_lang_Throwable *)
{
  //printf("Netscape_Java_java_lang_Throwable_fillInStackTrace() not implemented");
  return 0;
}


} /* extern "C" */

