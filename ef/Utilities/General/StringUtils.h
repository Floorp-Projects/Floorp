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

// StringUtils.h
//
// Small replacements for the standard string class which is
// probably too big and complicated for our needs.

#include "prprf.h"
#include "plstr.h"
#include "LogModule.h"

//	TemporaryStringCopy
//
//	Make a copy of a string, for temporary use.  Intended to be used
//	statically.
class TemporaryStringCopy
{
	char*	mCopy;

public:
  	// Make a copy of a string 
	TemporaryStringCopy(const char*	inStringToCopy) 
    { 	
		mCopy = new char[PL_strlen(inStringToCopy)+1];
		PL_strcpy(mCopy, inStringToCopy);
    }
    
	// Make a copy of the first len characters of the string */
	TemporaryStringCopy(const char *inStringToCopy, Int32 len) 
    {
		mCopy = new char[len+1];
		mCopy = PL_strncpy(mCopy, inStringToCopy, len);
		mCopy[len] = 0;
    }

	~TemporaryStringCopy() { delete [] mCopy; }
	
	char& operator[](int inIndex) 	{ return mCopy[inIndex]; }
	operator char*(void) 			{ return mCopy; }
  
#ifdef DEBUG
private:
	TemporaryStringCopy(const TemporaryStringCopy &);		// Copying forbidden
	void operator=(const TemporaryStringCopy &);			// Copying forbidden
#endif
};


// TemporaryBuffer
//
// Buffer is thrown away in destructor
class TemporaryBuffer 
{
	char *mData;

public:
	TemporaryBuffer(Uint32 len) 	{ mData = new char[len]; }
	~TemporaryBuffer() 			{ delete [] mData; }

	operator char*(void) 			{ return mData; }
};

#ifdef DEBUG_LOG
#define DEBUG_TRACER(inLogModule, inString)	Tracer(inLogModule, inString)	
#else
#define DEBUG_TRACER(inLogModule, inString)
#endif

class Tracer
{
	TemporaryStringCopy		mString;
	LogModuleObject&		mLogModule;
	
public:
	Tracer(LogModuleObject& inLogModule, const char* inString) :
		mString(inString),
		mLogModule(inLogModule) { UT_OBJECTLOG(mLogModule, PR_LOG_ALWAYS, ("--> %s\n", (char*) mString)); }

	~Tracer() { UT_OBJECTLOG(mLogModule, PR_LOG_ALWAYS, ("<-- %s\n", (char*) mString)); }
};
