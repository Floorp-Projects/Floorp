/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

// This class should only be used by the subscribe UI and by newshost.cpp.
// And, well, a bit by the category code.  Everyone else should use the stuff
// in newshost.h.

#ifndef _nsIMAPGroupRecord_h__
#define _nsIMAPGroupRecord_h__

#include "nsMsgGroupRecord.h"
#include "prlog.h"

class nsIMAPGroupRecord : public nsMsgGroupRecord {


public:
	static nsIMAPGroupRecord* Create(nsIMAPGroupRecord* parent,
										const char* folderName,
										char delimiter,
										PRBool filledInGroup,
										PRBool folderIsNamespace);

	static nsIMAPGroupRecord* CreateFromPartname(nsIMAPGroupRecord* parent, const char* partname, char delimiter,
		PRBool filledInGroup);

	virtual PRBool IsCategory() { return PR_FALSE;}
	virtual PRBool IsCategoryContainer() { return PR_FALSE; }
	virtual int SetIsCategoryContainer(PRBool /* value */) { PR_ASSERT(0); return 0; }
	virtual PRBool IsIMAPGroupRecord() { return PR_TRUE; }
	virtual nsIMAPGroupRecord *GetIMAPGroupRecord() { return this; }

	virtual PRBool IsGroup() { return PR_TRUE; }

	virtual char GetHierarchySeparator() { return m_onlineDelimiter; }
	void SetHierarchySeparator(char delimiter) { m_onlineDelimiter = delimiter; }

	PRBool GetAllGrandChildrenDiscovered() { return m_allGrandChildrenDiscovered; }
	void SetAllGrandChildrenDiscovered(PRBool discovered) { m_allGrandChildrenDiscovered = discovered; }

	PRBool GetIsGroupFilledIn() { return m_filledInGroup; }
	void SetIsGroupFilledIn(PRBool filledIn) { m_filledInGroup = filledIn; }

	void SetFlags(PRUint32 flags) { m_flags = flags; }
	PRUint32 GetFlags() { return m_flags; }

	void		SetFolderIsNamespace(PRBool isNamespace) { m_folderIsNamespace = isNamespace; }
	PRBool		GetFolderIsNamespace() { return m_folderIsNamespace; }

	PRBool		GetCanToggleSubscribe();

protected:
	nsIMAPGroupRecord(nsIMAPGroupRecord* parent, const char* partname, char delimiter, PRBool filledInGroup);

	PRBool m_allGrandChildrenDiscovered;
	PRBool m_filledInGroup;
	PRBool m_folderIsNamespace;
	char	m_onlineDelimiter;
	PRUint32 m_flags;

};


#endif

