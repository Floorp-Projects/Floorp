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

#ifndef OWNED_H
#define OWNED_H

enum OL_HandleMethod { OL_OLD_NETSCAPE, OL_CURRENT_NETSCAPE, OL_OTHER_APP };

class COwnedLostItem
{
public:
	CString m_csPrettyName;
	CString m_csMimeType;
	OL_HandleMethod m_nHandleMethod;
	BOOL m_bIgnored;
	BOOL m_bBroken;

	COwnedLostItem(CString mimeType);
	COwnedLostItem(CString mimeType, CString ignored);

	void SetHandledViaNetscape();
	void FetchPrettyName();
	void SetIgnored(BOOL b) { m_bIgnored = b; };

	BOOL IsInternetShortcut();
	CString GetInternetShortcutFileClass();
	CString MakeInternetShortcutName();

	void SetHandleMethodViaFileClass(const CString& fileClass, const CString& netscapeName,
									   const CString& directoryName);
	void GiveControlToNetscape();
	
	CString GetRegistryValue() 
	{ 
		if (m_bIgnored) 
			return "Ignored";
		if (m_bBroken)
			return "Broken";
		return "Not ignored."; 
	};
};

class COwnedAndLostList
{
private:
	CPtrArray m_OwnedList;
	CPtrArray m_LostList;
	
public:
	~COwnedAndLostList();

	void ConstructLists();
	void WriteLists();

	CPtrArray* GetLostList() { return &m_LostList; };
	CPtrArray* GetOwnedList() { return &m_OwnedList; };

	BOOL IsInOwnedList(const CString& mimeType);
	BOOL IsInLostList(const CString& mimeType);

	COwnedLostItem* RemoveFromLostList(const CString& mimeType);

	BOOL NonemptyLostIgnoredIntersection();
};

#endif // OWNED_H
