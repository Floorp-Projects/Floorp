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
#include "java_util_ResourceBundle.h"
#include "prprf.h"
#include "prio.h"
#include "JavaVM.h"
#include "SysCallsRuntime.h"

extern "C" {

NS_EXPORT NS_NATIVECALL(ArrayOf_Java_java_lang_Class *)
Netscape_Java_java_util_ResourceBundle_getClassContext()
{
#ifdef DEBUG_LOG
//  PR_fprintf(PR_STDERR, "Warning: ResourceBundle::getClassContext() not implemented\n");
#endif
	
	ArrayOf_Java_java_lang_Class *arr = (ArrayOf_Java_java_lang_Class *) sysNewObjectArray(&VM::getStandardClass(cClass), 3);
	return arr;
}

}
