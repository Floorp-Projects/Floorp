/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
