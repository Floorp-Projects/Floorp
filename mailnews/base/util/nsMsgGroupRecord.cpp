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

#include "msgCore.h"    // precompiled header...

#include "nsMsgGroupRecord.h"

#include "plstr.h"
#include "prmem.h"
#include "nsEscape.h"
#include "nsCRT.h"

/* for the XP_TO_UPPER stuff */
#include "net.h"

// mscott: this is lame...I know....
#define MK_OUT_OF_MEMORY  1

const PRUint32 F_ISGROUP			 = 0x00000001;
const PRUint32 F_EXPANDED			 = 0x00000002;
const PRUint32 F_CATCONT			 = 0x00000004;
const PRUint32 F_VIRTUAL			 = 0x00000008;
const PRUint32 F_DIRTY			 = 0x00000010;
const PRUint32 F_DESCENDENTSLOADED = 0x00000020;
const PRUint32 F_HTMLOKGROUP		 = 0x00000040;
const PRUint32 F_HTMLOKTREE		 = 0x00000080;
const PRUint32 F_NEEDEXTRAINFO	 = 0x00000100;
const PRUint32 F_DOESNOTEXIST		 = 0x00000200;
const PRUint32 RUNTIMEFLAGS =		// Flags to be sure *not* to write to disk.
	F_DIRTY | F_DESCENDENTSLOADED | F_EXPANDED;



int
nsMsgGroupRecord::GroupNameCompare(const char* name1, const char* name2,
								  char delimiter, PRBool caseInsensitive)
{
	if (caseInsensitive)
	{
		while (*name1 && (nsCRT::ToUpper(*name1) == nsCRT::ToUpper(*name2))) {
			name1++;
			name2++;
		}		
	}
	else
	{
		while (*name1 && *name1 == *name2) {
			name1++;
			name2++;
		}
	}

	if (*name1 && *name2) {
		if (*name1 == delimiter) return -1;
		if (*name2 == delimiter) return 1;
	}

	if (caseInsensitive)
		return int(nsCRT::ToUpper(*name1)) - int(nsCRT::ToUpper(*name2));
	else
		return int(*name1) - int(*name2);
}


nsMsgGroupRecord*
nsMsgGroupRecord::Create(nsMsgGroupRecord* parent, const char* partname,
						PRInt64 time, PRInt32 uniqueid, PRInt32 fileoffset)
{
	nsMsgGroupRecord* result = new nsMsgGroupRecord(parent, partname,
												  time, uniqueid, fileoffset);
	if (result && partname && !result->m_partname) {
		// We ran out of memory.
		delete result;
		result = NULL;
	}
	result->InitializeSibling();
	return result;
}


nsMsgGroupRecord::nsMsgGroupRecord(nsMsgGroupRecord* parent, const char* partname,
								 PRInt64 time, PRInt32 uniqueid, PRInt32 fileoffset,
								 char delimiter /* = '.' */)
{
	int length;
	m_prettyname = NULL;
	m_parent = parent;
	m_children = NULL;
	m_sibling = NULL;
	m_flags = 0;
	m_partname = NULL;
	m_addtime = time;
	m_uniqueId = uniqueid;
	m_fileoffset = fileoffset;
	m_delimiter = delimiter;
	if (partname) {
		length = PL_strlen(partname);
		// PR_ASSERT(parent != NULL);
		m_partname = new char [length + 1];
		if (!m_partname) {
			m_parent = NULL;
			return;
		}
		PL_strcpy(m_partname, partname);
	}
}


nsMsgGroupRecord::~nsMsgGroupRecord()
{
	delete [] m_partname;
	m_partname = NULL;
	delete [] m_prettyname;
	m_prettyname = NULL;
	while (m_children) {
		delete m_children;
	}
	m_children = NULL;
	if (m_parent) {
		nsMsgGroupRecord** ptr;
		for (ptr = &(m_parent->m_children);
			 *ptr;
			 ptr = &((*ptr)->m_sibling)) {
			if (*ptr == this) {
				*ptr = m_sibling;
				break;
			}
		}
	}
}

void nsMsgGroupRecord::InitializeSibling()
{
	if (m_parent) {
		PR_ASSERT(m_partname != NULL);
		nsMsgGroupRecord** ptr;
		for (ptr = &(m_parent->m_children) ; *ptr ; ptr = &((*ptr)->m_sibling)) {
			int comp = GroupNameCompare((*ptr)->m_partname, m_partname, m_delimiter, IsIMAPGroupRecord());
			PR_ASSERT(comp != 0);
			if (comp >= 0) break;
		}
		m_sibling = *ptr;
		*ptr = this;
	}
}


nsMsgGroupRecord*
nsMsgGroupRecord::FindDescendant(const char* name)
{
	if (!name || !*name) return this;
	char* ptr = PL_strchr(name, m_delimiter);
	if (ptr) *ptr = '\0';
	nsMsgGroupRecord* child;
	for (child = m_children ; child ; child = child->m_sibling) {
		if (PL_strcmp(child->m_partname, name) == 0) {
			break;
		}
	}
	if (ptr) {
		*ptr++ = m_delimiter;
		if (child) {
			return child->FindDescendant(ptr);
		}
	}
	return child;
}


nsMsgGroupRecord*
nsMsgGroupRecord::GetSiblingOrAncestorSibling()
{
	if (m_sibling) return m_sibling;
	if (m_parent) return m_parent->GetSiblingOrAncestorSibling();
	return NULL;
}

nsMsgGroupRecord*
nsMsgGroupRecord::GetNextAlphabetic()
{
	nsMsgGroupRecord* result;
	if (m_children) result = m_children;
	else result = GetSiblingOrAncestorSibling();
#ifdef DEBUG_slowAndParanoid
	if (result) {
		char* ptr1 = GetFullName();
		char* ptr2 = result->GetFullName();
		PR_ASSERT(GroupNameCompare(ptr1, ptr2) < 0);
		delete [] ptr1;
		delete [] ptr2;
	}
#endif
	return result;
}

nsMsgGroupRecord*
nsMsgGroupRecord::GetNextAlphabeticNoCategories()
{
	if (IsCategoryContainer()) {
		return GetSiblingOrAncestorSibling();
	} else {
		return GetNextAlphabetic();
	}
}


char*
nsMsgGroupRecord::GetFullName()
{
	int length = 0;
	nsMsgGroupRecord* ptr;
	for (ptr = this ; ptr ; ptr = ptr->m_parent) {
		if (ptr->m_partname) length += PL_strlen(ptr->m_partname) + 1;
	}
	PR_ASSERT(length > 0);
	if (length <= 0) return NULL;
	char* result = new char [length];
	if (result) {
		SuckInName(result);
		PR_ASSERT(int(PL_strlen(result)) + 1 == length);
	}
	return result;
}


char* 
nsMsgGroupRecord::SuckInName(char* ptr)
{
	if (m_parent && m_parent->m_partname) {
		ptr = m_parent->SuckInName(ptr);
		*ptr++ = m_delimiter;
	}
	PL_strcpy(ptr, m_partname);
	return ptr + PL_strlen(ptr);
}



int
nsMsgGroupRecord::SetPrettyName(const char* name)
{
	if (name == NULL && m_prettyname == NULL) return 0;
	m_flags |= F_DIRTY;
	delete [] m_prettyname;
	m_prettyname = NULL;
	if (!name || !*name) {
		return 0;
	}
	int length = PL_strlen(name);
	m_prettyname = new char [length + 1];
	if (!m_prettyname) {
		return MK_OUT_OF_MEMORY;
	}
	PL_strcpy(m_prettyname, name);
	return 1;
}


PRBool
nsMsgGroupRecord::IsCategory()
{
	return GetCategoryContainer() != NULL;
}

PRBool
nsMsgGroupRecord::IsCategoryContainer()
{
	return (m_flags & F_CATCONT) != 0;
}

PRBool
nsMsgGroupRecord::NeedsExtraInfo()
{
	return (m_flags & F_NEEDEXTRAINFO) != 0;
}

int
nsMsgGroupRecord::SetNeedsExtraInfo(PRBool value)
{
	return TweakFlag(F_NEEDEXTRAINFO, value);
}


int
nsMsgGroupRecord::SetIsCategoryContainer(PRBool value)
{
	// refuse to set a group to be a category container if it has a parent
	// that's a category container.
	if (! (value && GetCategoryContainer()))
		return TweakFlag(F_CATCONT, value);
	else
		return 0;
}


nsMsgGroupRecord*
nsMsgGroupRecord::GetCategoryContainer()
{
	if (IsCategoryContainer()) return NULL;
	for (nsMsgGroupRecord* ptr = m_parent ; ptr ; ptr = ptr->m_parent) {
		if (ptr->IsCategoryContainer()) return ptr;
	}
	return NULL;
}


nsresult
nsMsgGroupRecord::IsVirtual(PRBool *retval)
{
	*retval =( (m_flags & F_VIRTUAL) != 0);
    return NS_OK;
}

nsresult
nsMsgGroupRecord::SetIsVirtual(PRBool value)
{
	TweakFlag(F_VIRTUAL, value);
    return NS_OK;
}



PRBool
nsMsgGroupRecord::IsExpanded()
{
	return (m_flags & F_EXPANDED) != 0;
}

int
nsMsgGroupRecord::SetIsExpanded(PRBool value)
{
	return TweakFlag(F_EXPANDED, value);
}


PRBool
nsMsgGroupRecord::IsHTMLOKGroup()
{
	return (m_flags & F_HTMLOKGROUP) != 0;
}

int
nsMsgGroupRecord::SetIsHTMLOKGroup(PRBool value)
{
	return TweakFlag(F_HTMLOKGROUP, value);
}



PRBool
nsMsgGroupRecord::IsHTMLOKTree()
{
	return (m_flags & F_HTMLOKTREE) != 0;
}

int
nsMsgGroupRecord::SetIsHTMLOKTree(PRBool value)
{
	return TweakFlag(F_HTMLOKTREE, value);
}



PRBool
nsMsgGroupRecord::IsGroup()
{
	return (m_flags & F_ISGROUP) != 0;
}

int
nsMsgGroupRecord::SetIsGroup(PRBool value)
{
	return TweakFlag(F_ISGROUP, value);
}


PRBool
nsMsgGroupRecord::IsDescendentsLoaded()
{
	return (m_flags & F_DESCENDENTSLOADED) != 0;
}


int
nsMsgGroupRecord::SetIsDescendentsLoaded(PRBool value)
{
	PR_ASSERT(value);			// No reason we'd ever unset this.
	TweakFlag(F_DESCENDENTSLOADED, PR_TRUE);
	nsMsgGroupRecord* child;
	for (child = m_children ; child ; child = child->m_sibling) {
		child->SetIsDescendentsLoaded(value);
	}
	return 0;
}

PRBool nsMsgGroupRecord::DoesNotExistOnServer()
{
	return (m_flags & F_DOESNOTEXIST) != 0;
}

int nsMsgGroupRecord::SetDoesNotExistOnServer(PRBool value)
{
	if (value)		// turn off group flag if doesn't exist on server.
		TweakFlag(F_ISGROUP, PR_FALSE);
	return TweakFlag(F_DOESNOTEXIST, value);
}

int
nsMsgGroupRecord::TweakFlag(PRUint32 flagbit, PRBool value)
{
	if (value) {
		if (!(m_flags & flagbit)) {
			m_flags |= flagbit;
			if (flagbit & ~RUNTIMEFLAGS)
				m_flags |= F_DIRTY;
			return 1;
		}
	} else {
		if (m_flags & flagbit) {
			m_flags &= ~flagbit;
			if (flagbit & ~RUNTIMEFLAGS)
				m_flags |= F_DIRTY;
			return 1;
		}
	}
	return 0;
}


PRInt32
nsMsgGroupRecord::GetNumKids()
{
	PRInt32 result = 0;
	nsMsgGroupRecord* child;
	for (child = m_children ; child ; child = child->m_sibling) {
		if (IsIMAPGroupRecord())
			result++;
		else
			if (child->m_flags & F_ISGROUP) result++;

		if (!IsIMAPGroupRecord())
			result += child->GetNumKids();
	}
	return result;
}




char*
nsMsgGroupRecord::GetSaveString()
{
	char* pretty = NULL;
	char* result = nsnull;
	
	if (m_prettyname) {
		pretty = nsEscape(m_prettyname, url_XAlphas);
		if (!pretty) return NULL;
	}
	char* fullname = GetFullName();
	if (!fullname) return NULL; {
		long nAddTime;
		LL_L2I(nAddTime, m_addtime);
        result = PR_smprintf("%s,%s,%lx,%lx,%lx" LINEBREAK,
							   fullname, pretty ? pretty : "",
							   (long) (m_flags & ~RUNTIMEFLAGS),
							   nAddTime,
							   (long) m_uniqueId);
	}
	delete [] fullname;
	if (pretty) PR_Free(pretty);
	m_flags &= ~F_DIRTY;
	return result;
}


PRBool
nsMsgGroupRecord::IsDirty()
{
	return (m_flags & F_DIRTY) != 0;
}


PRInt32
nsMsgGroupRecord::GetDepth()
{
	PRInt32 result = 0;
	nsMsgGroupRecord* tmp = m_parent;
	while (tmp) {
		tmp = tmp->m_parent;
		result++;
	}
	return result;
}




nsMsgGroupRecord*
nsMsgGroupRecord::Create(nsMsgGroupRecord* parent, const char* saveline,
						PRInt32 savelinelength, PRInt32 fileoffset)
{
	char* tmp;
	char* ptr;
	char* endptr;
	char* partname;
	char* prettyname;
	PRInt32 flags;
	PRInt32 addtime;
	PRInt32 uniqueid;
	nsMsgGroupRecord* result = NULL;

	if (savelinelength < 0) savelinelength = PL_strlen(saveline);
	tmp = (char*) PR_Malloc(savelinelength + 1);
	if (!tmp) return NULL;
	PL_strncpy(tmp, saveline, savelinelength);
	tmp[savelinelength] = '\0';
	ptr = PL_strchr(tmp, ',');
	PR_ASSERT(ptr);
	if (!ptr) goto FAIL;
	*ptr++ = '\0';
	partname = PL_strrchr(tmp, '.');
	if (!partname) partname = tmp;
	else partname++;

#ifdef DEBUG_slowAndParanoid
	if (parent->m_partname) {
		char* parentname = parent->GetFullName();
		PR_ASSERT(partname > tmp && partname[-1] == '.');
		partname[-1] = '\0';
		PR_ASSERT(PL_strcmp(parentname, tmp) == 0);
		partname[-1] = '.';
		delete [] parentname;
		parentname = NULL;
	} else {
		PR_ASSERT(partname == tmp);
	}
#endif

	endptr = PL_strchr(ptr, ',');
	PR_ASSERT(endptr);
	if (!endptr) goto FAIL;
	*endptr++ = '\0';
	prettyname = nsUnescape(ptr);

	ptr = endptr;
	endptr = PL_strchr(ptr, ',');
	PR_ASSERT(endptr);
	if (!endptr) goto FAIL;
	*endptr++ = '\0';
	flags = strtol(ptr, NULL, 16);

	ptr = endptr;
	endptr = PL_strchr(ptr, ',');
	PR_ASSERT(endptr);
	if (!endptr) goto FAIL;
	*endptr++ = '\0';
	addtime = strtol(ptr, NULL, 16);

	ptr = endptr;
	uniqueid = strtol(ptr, NULL, 16);

	PRInt64 llAddtime;
	LL_I2L(llAddtime, addtime);
	result = Create(parent, partname, llAddtime, uniqueid, fileoffset);
	if (result) {
		PRBool maybeCategoryContainer = flags & F_CATCONT;
		flags &= ~F_CATCONT;
		result->m_flags = flags;
		if (maybeCategoryContainer)
			result->SetIsCategoryContainer(PR_TRUE);
		if (prettyname && *prettyname) result->SetPrettyName(prettyname);
	}

FAIL:
	PR_Free(tmp);
	return result;
	
}



