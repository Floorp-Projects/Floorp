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
// qaoutput.cpp
// Test prototype for API testing
//

#include <stdlib.h>
#include <stdio.h>
#include <iostream.h>
#include <string.h>
#include "stdafx.h"
#include "qaoutput.h"

// declarations
enum outcome GetMessage();

// defines
#define space "    "

// global variables
enum outcome { pass = 0, fail, none };
char *TestcaseOutputFilename;
FILE *stream2;

// currently can handle only a single testcase. Doing a second
// testcase requires changing the FileJustOpened variable.
int QAOpenOutputFile()
{
	static int FileJustOpened = 0;

	if (FileJustOpened == 0)
	{
		FileJustOpened = 1;
		if (!TestcaseOutputFilename)
		{
			AfxMessageBox("Output Filename is not defined");
			return 1;
		}

		/* Open for write */
		if( (stream2 = fopen(TestcaseOutputFilename, "w+")) == NULL )
		{
			// cout << "The file qaoutput was not opened\n";
			return 1;
		}
		else
		{
			// cout << "The file qaoutput was opened\n";
			fputs( "Whitebox Testing Trace Log\n", stream2);
		}
	}
	else
	{	
		// Open for append
		if( (stream2 = fopen(TestcaseOutputFilename, "a+")) == NULL )
		{
		// cout << "The file qaoutput was not opened\n";
		return 1;
		}
	}

	return 0;
}

int QACloseOutputFile()
{
	/* Close stream */
	if( fclose( stream2 ) )
	{
		// cout << "The file 'data' was not closed\n";
		return 1;
	}

	return 0;
}

int QAAddToOutputFile(char* s)
{
	if (!s)
		return 1;
	else
	{
		fputs(s, stream2);
		fputs("\n",stream2);
	}

	return 0;
}

int AssignFilename(const char* newfilename)
{
	int length = strlen(newfilename);
	TestcaseOutputFilename = new char[length+1];
	if (!TestcaseOutputFilename)
		return 1;

	strcpy(TestcaseOutputFilename,newfilename);
	return 0;
}

int DeleteFilename()
{
	if (TestcaseOutputFilename)
	{
		delete[] TestcaseOutputFilename;
		return 0;
	}
	else
		return 1;
}



