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
#include "java_lang_ClassLoader.h"
#include "java_io_InputStream.h"
#include "FieldOrMethod.h"
#include "JavaVM.h"
#include "JavaString.h"
#include "SysCallsRuntime.h"
#include "prio.h"
#include <stdio.h>

extern "C" {
/*
 * Class : java/lang/ClassLoader
 * Method : init
 * Signature : ()V
 */
NS_EXPORT NS_NATIVECALL(void )
Netscape_Java_java_lang_ClassLoader_init(Java_java_lang_ClassLoader *)
{
  //printf("Netscape_Java_java_lang_ClassLoader_init() not implemented");
}

NS_EXPORT NS_NATIVECALL(void )
Netscape_Java_java_lang_ClassLoader_initIDs()
{
//  printf("Netscape_Java_java_lang_ClassLoader_init() not implemented");
}


/*
 * Class : java/lang/ClassLoader
 * Method : defineClass0
 * Signature : (Ljava/lang/String;[BII)Ljava/lang/Class;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Class *)
Netscape_Java_java_lang_ClassLoader_defineClass0(Java_java_lang_ClassLoader *, Java_java_lang_String *, ArrayOf_uint8 *, int32, int32)
{
  printf("Netscape_Java_java_lang_ClassLoader_defineClass0() not implemented");
  return 0;
}


/*
 * Class : java/lang/ClassLoader
 * Method : resolveClass0
 * Signature : (Ljava/lang/Class;)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_ClassLoader_resolveClass0(Java_java_lang_ClassLoader *, Java_java_lang_Class *)
{
  printf("Netscape_Java_java_lang_ClassLoader_resolveClass0() not implemented");

}


/*
 * Class : java/lang/ClassLoader
 * Method : findSystemClass0
 * Signature : (Ljava/lang/String;)Ljava/lang/Class;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Class *)
Netscape_Java_java_lang_ClassLoader_findSystemClass0(Java_java_lang_ClassLoader *, Java_java_lang_String *str)
{
#if 0
	Java_java_lang_Class* ret;
	try {
		char *className = ((JavaString *) str)->convertUtf();
		ClassCentral &central = VM::getCentral();

		ClassFileSummary &summ = central.addClass(className);
		Class *clazz = const_cast<Class *>(static_cast<const Class *>(summ.getThisClass()));

		JavaString::freeUtf(className);

		assert(clazz);
		ret = ((Java_java_lang_Class*)&asClass(*clazz));
	} catch (VerifyError e) {
		ret = NULL;
	}
	return ret;
#endif
//	fprintf(stdout, "looking for class %s\n", ((JavaString *) str)->convertUtf());
	return NULL;
}


/*
 * Class : java/lang/ClassLoader
 * Method : getSystemResourceAsStream0
 * Signature : (Ljava/lang/String;)Ljava/io/InputStream;
 */
NS_EXPORT NS_NATIVECALL(Java_java_io_InputStream *)
Netscape_Java_java_lang_ClassLoader_getSystemResourceAsStream0(Java_java_lang_String *str)
{
	if (str == NULL)
		sysThrowNullPointerException();

	char* fileName = ((JavaString *) str)->convertUtf();
	PRFileInfo info;
	if (PR_GetFileInfo(fileName, &info) != PR_SUCCESS) {
		JavaString::freeUtf(fileName);
		return NULL;
	}
	JavaString::freeUtf(fileName);

	ClassCentral &central = VM::getCentral();

	ClassFileSummary &summ = central.addClass("java/io/FileInputStream");
	Class *clazz = const_cast<Class *>(static_cast<const Class *>(summ.getThisClass()));
	const Type* paramType[1];
	paramType[0] = (Type *) &VM::getStandardClass(cString);
	JavaObject* param[1];
	param[0] = (JavaObject *) str;

	Constructor& c = clazz->getDeclaredConstructor(paramType, 1);
	JavaObject& ret = c.newInstance(param, 1);

	return (Java_java_io_InputStream *) &ret;
}


/*
 * Class : java/lang/ClassLoader
 * Method : getSystemResourceAsName0
 * Signature : (Ljava/lang/String;)Ljava/lang/String;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_String *)
Netscape_Java_java_lang_ClassLoader_getSystemResourceAsName0(Java_java_lang_String *)
{
  printf("Netscape_Java_java_lang_ClassLoader_getSystemResourceAsName0() not implemented");
  return 0;
}

} /* extern "C" */

