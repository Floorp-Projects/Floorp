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

//
// File: UStClasses.h
//
// This file contains a couple of stack-based save/restore classes
//

class LPane;


class StExcludeVisibleRgn {

public:

						StExcludeVisibleRgn(LPane *inPane);
						~StExcludeVisibleRgn(void);

private:

	GrafPtr				mGrafPtr;
	RgnHandle			mSaveVisRgn;
	Point 				mSavePortOrigin;
};



#pragma mark -

class StSetResAttrs {

public:

	enum { eAddAttrs = true, eDontAddAttrs = false };
	
						StSetResAttrs(Handle inResourceH, short inResAttrs, 
									  Boolean inAddAttrs = eDontAddAttrs);
						~StSetResAttrs(void);

protected:

	Handle				mResourceH;
	short				mSavedAttrs;
};
