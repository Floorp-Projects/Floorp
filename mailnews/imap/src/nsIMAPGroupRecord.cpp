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
