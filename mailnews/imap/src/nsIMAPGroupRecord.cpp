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

#include "nsIMAPGroupRecord.h"
#include "plstr.h"
#include "prmem.h"

extern int MSG_GROUPNAME_FLAG_IMAP_NOSELECT;

nsIMAPGroupRecord*
nsIMAPGroupRecord::Create(nsIMAPGroupRecord* parent, const char* folderName,
							char delimiter, PRBool filledInGroup, PRBool folderIsNamespace)
{
	char* tmp = NULL;
	char* partname = NULL;
	nsIMAPGroupRecord* result = NULL;

	if (folderName)
	{
		tmp = PL_strdup(folderName);
		partname = PL_strrchr(tmp, '/');
		if (!partname || !delimiter || folderIsNamespace) // if delimiter == NULL, partname is the full folderName
			partname = tmp;
		else
			partname++;
	}

#ifdef DEBUG_slowAndParanoid
	if (parent->m_partname) {
		char* parentname = parent->GetFullName();
		PR_ASSERT(partname > tmp && partname[-1] == m_delimiter);
		partname[-1] = '\0';
		PR_ASSERT(PL_strcmp(parentname, tmp) == 0);
		partname[-1] = m_delimiter;
		delete [] parentname;
		parentname = NULL;
	} else {
		PR_ASSERT(partname == tmp);
	}
#endif

	result = nsIMAPGroupRecord::CreateFromPartname(parent, partname, delimiter, filledInGroup);
	if (result) {
		result->m_flags = 0;
		result->SetFolderIsNamespace(folderIsNamespace);
	}

	PR_Free(tmp);
	return result;
	
}


nsIMAPGroupRecord*
nsIMAPGroupRecord::CreateFromPartname(nsIMAPGroupRecord* parent, const char* partname, char delimiter, PRBool filledInGroup)
{
	nsIMAPGroupRecord* result = new nsIMAPGroupRecord(parent, partname, delimiter, filledInGroup);
	if (result && partname && !result->m_partname) {
		// We ran out of memory.
		delete result;
		result = NULL;
	}
	result->InitializeSibling();
	return result;
}


nsIMAPGroupRecord::nsIMAPGroupRecord(nsIMAPGroupRecord* parent,
										 const char* partname, char delimiter, PRBool filledInGroup) : nsMsgGroupRecord(parent, partname, 0, 0, 0, '/')
{
	m_flags = 0;
	m_allGrandChildrenDiscovered = PR_FALSE;
	m_filledInGroup = filledInGroup;
	m_onlineDelimiter = delimiter;
}


PRBool nsIMAPGroupRecord::GetCanToggleSubscribe()
{
	return !GetFolderIsNamespace() && !(GetFlags() & MSG_GROUPNAME_FLAG_IMAP_NOSELECT);
}
