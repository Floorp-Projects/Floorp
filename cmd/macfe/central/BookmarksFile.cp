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

#include "BookmarksFile.h"
#include "resgui.h"
#include "fe_proto.h"
#include <LStream.h>

#include <algorithm>


//
// ReadBookmarksFile
//
// Given a file containing a single URL (probably dropped on the Finder), open it and
// read the URL so we can load it.
//
OSErr 
ReadBookmarksFile ( vector<char> & oURL, FSSpec & inSpec )
{
	FInfo info;
	OSErr err = ::FSpGetFInfo (&inSpec, &info);
	if (err != noErr)
		return err;
	
	if (info.fdType != emBookmarkFile)
		return fnfErr;

	try {
		LFileStream stream(inSpec);
		stream.OpenDataFork(fsRdPerm);
		Int32 howMuch;
		
		// Read in the URL, which is in the form URL\rTITLE
		howMuch = stream.ReadData(oURL.begin(), oURL.size());
		char* where = find(oURL.begin(), oURL.end(), '\r');
		ThrowIfNil_(where);
		*where = 0;
	}
	catch ( Uint32 inErr )
	{
		return inErr;
	}
	
	return noErr;
}


//
// WriteBookmarksFile
//
// Given an URL (probably dropped on the Finder), create a file and store the URL
//
OSErr WriteBookmarksFile(char * url, FSSpec & spec)
{
	if (!url)
		return noErr;
	LFileStream stream(spec);
	try
	{
		stream.CreateNewDataFile(emSignature, emBookmarkFile, 0);
	}
	catch (OSErr inErr)
	{
		if (inErr != dupFNErr)
			return inErr;
	}
	catch (...)
	{
		return memFullErr;
	}

	try
	{
		// Set up the file
		stream.OpenDataFork(fsRdWrPerm);
		stream.SetMarker(0, streamFrom_Start);

		char lineEnding = CR;
		// Write the URL
		stream.WriteData((void*)url, XP_STRLEN(url) );
		stream.WriteData(&lineEnding, 1);
	}
	catch (OSErr err)
	{
		return err;
	}
	catch (...)
	{
		return memFullErr;
	}
	return noErr;
}
