/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

