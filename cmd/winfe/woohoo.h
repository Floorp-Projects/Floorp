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

#ifndef _WOOHOO_H
#define _WOOHOO_H

class CMapStringToObNoCase : public CMapStringToOb
{
public:
	// Lookup
	BOOL Lookup(LPCTSTR key, CObject*& rValue) const;

// Operations
	// Lookup and add if not there
	CObject*& operator[](LPCTSTR key);
	BOOL LookupKey(LPCTSTR key, LPCTSTR& rKey) const;

	// add a new (key, value) pair
	void SetAt(LPCTSTR key, CObject* newValue);

	// removing existing (key, ?) pair
	BOOL RemoveKey(LPCTSTR key);

// Overridables: special non-virtual (see map implementation for details)
	// Routine used to user-provided hash keys
	UINT HashKey(LPCTSTR key) const;
};

#endif /* _WOOHOO_H */
