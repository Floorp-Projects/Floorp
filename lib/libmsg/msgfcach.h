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


#ifndef _MSGFCACH_H
#define _MSGFCACH_H

#include "ptrarray.h"

class MSG_FolderCacheElement;

class MSG_FolderCache : public XPSortedPtrArray
{

friend MSG_FolderCacheElement;

public:

	MSG_FolderCache ();
	virtual ~MSG_FolderCache ();

	// Read the disk file into 'this', returning zero if successful
	int ReadFromDisk ();

	// Write the disk file based on the root folder info
	int WriteToDisk (MSG_FolderInfo *root) const;

	// Look up the folder in 'this' and populate the fields known in the cache
	// Returns TRUE if the folder was found in the cache
	XP_Bool InitializeFolder (MSG_FolderInfo *folder) const;

	// Call this to free memory allocated by the cache since we
	// don't need it (or trust it) at runtime.
	void Depopulate ();

protected:

	MSG_FolderCacheElement *GetAt (int i) const;

};

#endif // _MSGFCACH_H
