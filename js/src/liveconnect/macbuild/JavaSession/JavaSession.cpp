/* -*- Mode: C; tab-width: 8 -*-
 * Copyright (C) 1998 Netscape Communications Corporation, All Rights Reserved.
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
