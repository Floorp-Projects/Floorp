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
	JavaSession.cpp
	
	Uses MRJ to open a Java VM.
	
	by Patrick C. Beard.
 */

#include "JavaSession.h"

#include <Errors.h>
#include <string.h>
#include <stdio.h>

#include <Resources.h>

extern "C" {

static void java_stdout(JMSessionRef session, const void *message, UInt32 messageLengthInBytes);
static void java_stderr(JMSessionRef session, const void *message, UInt32 messageLengthInBytes);
static SInt32 java_stdin(JMSessionRef session, void *buffer, SInt32 maxBufferLength);
static Boolean java_exit(JMSessionRef session, int value);
static Boolean java_authenticate(JMSessionRef session, const char *url, const char *realm,
									char userName[255], char password[255]);
static void java_lowmem(JMSessionRef session);

}

static JMSessionCallbacks callbacks = {
	kJMVersion,					/* should be set to kJMVersion */
	&java_stdout,				/* JM will route "stdout" to this function. */
	&java_stderr,				/* JM will route "stderr" to this function. */
	&java_stdin,				/* read from console - can be nil for default behavior (no console IO) */
	&java_exit,					/* handle System.exit(int) requests */
	&java_authenticate,			/* present basic authentication dialog */
	&java_lowmem				/* Low Memory notification Proc */
};

JavaSession::JavaSession() : mSession(NULL)
{
	OSStatus status = ::JMOpenSession(&mSession, /* eDisableJITC */ eJManager2Defaults, eCheckRemoteCode,
								&callbacks, kTextEncodingMacRoman, NULL);
	checkStatus(status);
}

JavaSession::~JavaSession()
{
	if (mSession != NULL) {
		OSStatus status = ::JMCloseSession(mSession);
		checkStatus(status);
	}
}

JNIEnv* JavaSession::getEnv()
{
	return ::JMGetCurrentEnv(mSession);
}

static StringPtr c2p(const char* str, StringPtr pstr)
{
	unsigned char len = strlen(str);
	memcpy(pstr + 1, str, len);
	*pstr = len;
	return pstr;
}

jclass JavaSession::getClass(const char* className)
{
	JNIEnv* env = getEnv();
	jclass result = env->FindClass(className);
	if (result == NULL) throw OSStatusException(fnfErr);
/*	if (result == NULL) {
		Str255 pClassName;
		c2p(className, pClassName);
		Handle classHandle = ::GetNamedResource(ClassResType, pClassName);
		if (classHandle != NULL) {
			::HLock(classHandle);
			result = env->DefineClass(className, NULL, (signed char*)*classHandle, ::GetHandleSize(classHandle));
			::ReleaseResource(classHandle);
		}
	} */
	return result;
}

/**
 * Adds a Mac-style path name to the MRJ class path.
 */
void JavaSession::addClassPath(const char* jarFilePath)
{
	Str255 pJarFilePath;
	FSSpec jarFileSpec;
	OSStatus status = FSMakeFSSpec(	0, 0,	// use "current working directory"
								 	c2p(jarFilePath, pJarFilePath),
								 	&jarFileSpec);
	checkStatus(status);
	status = JMAddToClassPath(mSession, &jarFileSpec);
	checkStatus(status);
}

// OBLIGATORY JMSession callbacks.

static void java_stdout(JMSessionRef session, const void *message, UInt32 messageLengthInBytes)
{
	char* msg = (char*)message;
	while (messageLengthInBytes--) {
		char c = *msg++;
		if (c == '\r')
			c = '\n';
		fputc(c, stdout);
	}
}

static void java_stderr(JMSessionRef session, const void *message, UInt32 messageLengthInBytes)
{
	char* msg = (char*)message;
	while (messageLengthInBytes--) {
		char c = *msg++;
		if (c == '\r')
			c = '\n';
		fputc(c, stderr);
	}
}

static SInt32 java_stdin(JMSessionRef session, void *buffer, SInt32 maxBufferLength)
{
	return -1;
}

static Boolean java_exit(JMSessionRef session, int value) { return false; }

static Boolean java_authenticate(JMSessionRef session, const char *url, const char *realm,
									char userName[255], char password[255])
{
	return true;
}

static void java_lowmem(JMSessionRef session)
{
}
