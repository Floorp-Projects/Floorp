/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/*
    prmacos.cpp

	Mac OS Support functions for LiveConnect. Provides standard JNI JavaVM creation
	functions, and strdup.
	
	by Patrick C. Beard.
 */

#include <string.h>
#include <stdlib.h>

#include "prtypes.h"
#include "prmacos.h"

char* strdup(const char *str)
{
    char* dup = (char*) malloc(1 + strlen(str));
    if (dup != NULL)
	    strcpy(dup, str);
    return dup;
}

#if defined(LIVECONNECT)

#include "JavaSession.h"

// Fake VM initialization

jint JNICALL JNI_GetDefaultJavaVMInitArgs(void* args)
{
	if (args != NULL) {
		JDK1_1InitArgs* initArgs = (JDK1_1InitArgs*)args;
		memset(initArgs, 0, sizeof(JDK1_1InitArgs));
		return 0;
	}
	return -1;
}

static JavaSession* theSession = NULL;
static const JNIInvokeInterface_* theActualFunctions;
static JNIInvokeInterface_ thePatchedFunctions;

static jint MRJ_DestroyJavaVM(JavaVM* javaVM)
{
	// Restore the original functions.
	javaVM->functions = theActualFunctions;

	// Toss the Java session instead.
	if (theSession != NULL) {
		delete theSession;
		theSession = NULL;
		return 0;
	}
	return -1;
}

jint JNICALL JNI_CreateJavaVM(JavaVM** outVM, JNIEnv ** outEnv, void* args)
{
	int result = -1;
	*outVM = NULL;
	*outEnv = NULL;

	try {
		if (theSession == NULL)
			theSession = new JavaSession();
		JNIEnv* env = theSession->getEnv();
		JavaVM* javaVM = NULL;
		result = env->GetJavaVM(&javaVM);
		if (result == 0) {
			*outEnv = env;
			*outVM = javaVM;

			// patch the JavaVM so it won't actually destroy itself. This crashes MRJ right now.
			theActualFunctions = javaVM->functions;
			thePatchedFunctions = *theActualFunctions;
			thePatchedFunctions.DestroyJavaVM = MRJ_DestroyJavaVM;
			javaVM->functions = &thePatchedFunctions;
		}
	} catch (OSStatusException status) {
		*outVM = NULL;
		*outEnv = NULL;
	}
	return result;
}

#endif
