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
#include "java_lang_SecurityManager.h"
#include <stdio.h>

extern "C" {
/*
 * Class : java/lang/SecurityManager
 * Method : getClassContext
 * Signature : ()[Ljava/lang/Class;
 */
NS_EXPORT NS_NATIVECALL(ArrayOf_Java_java_lang_Class *)
Netscape_Java_java_lang_SecurityManager_getClassContext(Java_java_lang_SecurityManager *)
{
  printf("Netscape_Java_java_lang_SecurityManager_getClassContext() not implemented");
  return 0;
}


/*
 * Class : java/lang/SecurityManager
 * Method : currentClassLoader
 * Signature : ()Ljava/lang/ClassLoader;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_ClassLoader *)
Netscape_Java_java_lang_SecurityManager_currentClassLoader(Java_java_lang_SecurityManager *)
{
  printf("Netscape_Java_java_lang_SecurityManager_currentClassLoader() not implemented");
  return 0;
}


/*
 * Class : java/lang/SecurityManager
 * Method : classDepth
 * Signature : (Ljava/lang/String;)I
 */
NS_EXPORT NS_NATIVECALL(int32)
Netscape_Java_java_lang_SecurityManager_classDepth(Java_java_lang_SecurityManager *, Java_java_lang_String *)
{
  printf("Netscape_Java_java_lang_SecurityManager_classDepth() not implemented");
  return 0;
}


/*
 * Class : java/lang/SecurityManager
 * Method : classLoaderDepth
 * Signature : ()I
 */
NS_EXPORT NS_NATIVECALL(int32)
Netscape_Java_java_lang_SecurityManager_classLoaderDepth(Java_java_lang_SecurityManager *)
{
  printf("Netscape_Java_java_lang_SecurityManager_classLoaderDepth() not implemented");
  return 0;
}


/*
 * Class : java/lang/SecurityManager
 * Method : currentLoadedClass0
 * Signature : ()Ljava/lang/Class;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Class *)
Netscape_Java_java_lang_SecurityManager_currentLoadedClass0(Java_java_lang_SecurityManager *)
{
  printf("Netscape_Java_java_lang_SecurityManager_currentLoadedClass0() not implemented");
  return 0;
}


} /* extern "C" */

