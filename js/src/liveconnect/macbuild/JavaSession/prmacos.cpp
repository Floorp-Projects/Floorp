/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/*
    prmacos.cpp

	Mac OS Support functions for LiveConnect. Provides standard JNI JavaVM creation
	functions, and strdup.
	
	by Patrick C. Beard.
 */

#include <string.h>
#include <stdlib.h>
#include <Errors.h>

#include "jstypes.h"

#if defined(LIVECONNECT)

#include "JavaSession.h"

// Fake VM initialization

jint JNICALL JNI_GetDefaultJavaVMInitArgs(void* args)
{
	if (args != NULL) {
		JDK1_1InitArgs* initArgs = (JDK1_1InitArgs*)args;
		memset(initArgs, 0, sizeof(JDK1_1InitArgs));
		initArgs->version = 0x00010001;
		initArgs->classpath = ".";
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

static void shutdownJava()
{
	// Toss the Java session.
	if (theSession != NULL) {
		delete theSession;
		theSession = NULL;
	}
}

jint JNICALL JNI_CreateJavaVM(JavaVM** outVM, JNIEnv ** outEnv, void* args)
{
	int result = -1;
	*outVM = NULL;
	*outEnv = NULL;

	try {
		if (theSession == NULL)
			theSession = new JavaSession();
		if (theSession == NULL)
			throw(OSStatusException(memFullErr));
		
		// set up the classpath if any.
		JDK1_1InitArgs* initArgs = (JDK1_1InitArgs*)args;
		char* classpath = initArgs->classpath;
		char* colon = ::strchr(classpath, ':');
		while (colon != NULL) {
			char* element = classpath;
			*colon = '\0';
			theSession->addClassPath(element);
			*colon = ':';
			classpath = colon + 1;
			colon = ::strchr(classpath, ':');
		}
		
		// Make sure that the JavaSession is torn down at the end of execution.
		// If the user chooses "Quit" from the file menu, this guarantees
		// the shutdown will happen.
		atexit(&shutdownJava);
		
		JNIEnv* env = theSession->getEnv();
		JavaVM* javaVM = NULL;
		result = env->GetJavaVM(&javaVM);
		if (result == 0) {
			*outEnv = env;
			*outVM = javaVM;

			// Patch the JavaVM so it won't actually destroy itself.
			// If we don't do this, MRJ crashest at exit.
			// The functions are restored when DestoryJavaVM is called.
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
