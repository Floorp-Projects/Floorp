/* -*- Mode: C; tab-width: 8 -*-
 * Copyright (C) 1998 Netscape Communications Corporation, All Rights Reserved.
 */

/*
	JavaSession.h

	Uses MRJ to open a Java VM.

	by Patrick C. Beard
 */

#pragma once

#include <jni.h>
#include <JManager.h>

#include "OSStatusException.h"

class JavaSession {
public:
	JavaSession();
	~JavaSession();
	
	JNIEnv* getEnv();
	jclass getClass(const char* className);
	
	void addClassPath(const char* jarFilePath);

private:
	JMSessionRef mSession;
};
