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

// ===========================================================================
// ulaunch.h
// External application launching routines
// ===========================================================================

#pragma once

class LFileBufferStream;

// Opens the file in its creator application					
void LaunchFile(LFileBufferStream * inFile);

// Launches an application with a given doc
OSErr StartDocInApp(FSSpec theDocument,  FSSpec theApplication);

// Builds an ODOC event for appliation specified by applSig,
// with file in fileSpec
OSErr BuildODOCEvent(OSType applSig, 
					FSSpec fileSpec, 
					AppleEvent& theEvent);

OSErr
CreateFinderAppleEvent(		AEEventID		eventID,
							SInt16 			returnID, 
							SInt32 			transactionID,
							AppleEvent	&	theEvent);

