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
// Interface to a class that contains code common to classes that have to do drag and
// drop for items w/ urls (bookmarks, etc).
//

#pragma once

#include "cstring.h"


class CURLDragHelper
{
public:

	// call after a drop. Handles sending the data or creating files from flavors
	// registered for url-ish items
	static void DoDragSendData ( const char* inURL, char* inTitle, FlavorType inFlavor,
									ItemReference inItemRef, DragReference inDragRef) ;

	// Make sure the caption for an icon isn't too long so that it leaves turds.
	// handles middle truncation, etc if title is too long
	static cstring MakeIconTextValid ( const char* inTitle );
	
}; // CURLDragMixin
