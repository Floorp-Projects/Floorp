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

#include "msg.h"

#include "grec.h"

extern "C" {
	extern int MK_OUT_OF_MEMORY;
}

const uint32 F_ISGROUP			 = 0x00000001;
const uint32 F_EXPANDED			 = 0x00000002;
const uint32 F_CATCONT			 = 0x00000004;
const uint32 F_PROFILE			 = 0x00000008;
const uint32 F_DIRTY			 = 0x00000010;
const uint32 F_DESCENDENTSLOADED = 0x00000020;
const uint32 F_HTMLOKGROUP		 = 0x00000040;
const uint32 F_HTMLOKTREE		 = 0x00000080;
const uint32 F_NEEDEXTRAINFO	 = 0x00000100;
const uint32 F_DOESNOTEXIST		 = 0x00000200;
const uint32 RUNTIMEFLAGS =		// Flags to be sure *not* to write to disk.
	F_DIRTY | F_DESCENDENTSLOADED | F_EXPANDED;



int
msg_GroupRecord::GroupNameCompare(const char* name1, const char* name2, char delimiter)
{
	while (*name1 && *name1 == *name2) {
		name1++;
		name2++;
	}
	if (*name1 && *name2) {
		if (*name1 == delimiter) return -1;
		if (*name2 == delimiter) return 1;
	}
	return int(*name1) - int(*name2);
}


msg_GroupRecord*
msg_GroupRecord::Create(msg_GroupRecord* parent, const char* partname,
						time_t time, int32 uniqueid, int32 fileoffset)
{
	msg_GroupRecord* result = new msg_GroupRecord(parent, partname,
												  time, uniqueid, fileoffset);
	if (result && partname && !result->m_partname) {
		// We ran out of memory.
		delete result;
		result = NULL;
	}
	return result;
}


msg_GroupRecord::msg_GroupRecord(msg_GroupRecord* parent, const char* partname,
								 time_t time, int32 uniqueid, int32 fileoffset,
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
		length = XP_STRLEN(partname);
		// XP_ASSERT(parent != NULL);
		m_partname = new char [length + 1];
		if (!m_partname) {
			m_parent = NULL;
			return;
		}
		XP_STRCPY(m_partname, partname);
	}

	if (parent) {
		XP_ASSERT(partname != NULL);
		msg_GroupRecord** ptr;
		for (ptr = &(parent->m_children) ; *ptr ; ptr = &((*ptr)->m_sibling)) {
			int comp = GroupNameCompare((*ptr)->m_partname, partname, m_delimiter);
			XP_ASSERT(comp != 0);
			if (comp >= 0) break;
		}
		m_sibling = *ptr;
		*ptr = this;
	}
	
}


msg_GroupRecord::~msg_GroupRecord()
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
		msg_GroupRecord** ptr;
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



msg_GroupRecord*
msg_GroupRecord::FindDescendant(const char* name)
{
	if (!name || !*name) return this;
	char* ptr = XP_STRCHR(name, m_delimiter);
	if (ptr) *ptr = '\0';
	msg_GroupRecord* child;
	for (child = m_children ; child ; child = child->m_sibling) {
		if (XP_STRCMP(child->m_partname, name) == 0) {
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


msg_GroupRecord*
msg_GroupRecord::GetSiblingOrAncestorSibling()
{
	if (m_sibling) return m_sibling;
	if (m_parent) return m_parent->GetSiblingOrAncestorSibling();
	return NULL;
}

msg_GroupRecord*
msg_GroupRecord::GetNextAlphabetic()
{
	msg_GroupRecord* result;
	if (m_children) result = m_children;
	else result = GetSiblingOrAncestorSibling();
#ifdef DEBUG_slowAndParanoid
	if (result) {
		char* ptr1 = GetFullName();
		char* ptr2 = result->GetFullName();
		XP_ASSERT(GroupNameCompare(ptr1, ptr2) < 0);
		delete [] ptr1;
		delete [] ptr2;
	}
#endif
	return result;
}

msg_GroupRecord*
msg_GroupRecord::GetNextAlphabeticNoCategories()
{
	if (IsCategoryContainer()) {
		return GetSiblingOrAncestorSibling();
	} else {
		return GetNextAlphabetic();
	}
}


char*
msg_GroupRecord::GetFullName()
{
	int length = 0;
	msg_GroupRecord* ptr;
	for (ptr = this ; ptr ; ptr = ptr->m_parent) {
		if (ptr->m_partname) length += XP_STRLEN(ptr->m_partname) + 1;
	}
	XP_ASSERT(length > 0);
	if (length <= 0) return NULL;
	char* result = new char [length];
	if (result) {
		SuckInName(result);
		XP_ASSERT(int(XP_STRLEN(result)) + 1 == length);
	}
	return result;
}


char* 
msg_GroupRecord::SuckInName(char* ptr)
{
	if (m_parent && m_parent->m_partname) {
		ptr = m_parent->SuckInName(ptr);
		*ptr++ = m_delimiter;
	}
	XP_STRCPY(ptr, m_partname);
	return ptr + XP_STRLEN(ptr);
}



int
msg_GroupRecord::SetPrettyName(const char* name)
{
	if (name == NULL && m_prettyname == NULL) return 0;
	m_flags |= F_DIRTY;
	delete [] m_prettyname;
	m_prettyname = NULL;
	if (!name || !*name) {
		return 0;
	}
	int length = XP_STRLEN(name);
	m_prettyname = new char [length + 1];
	if (!m_prettyname) {
		return MK_OUT_OF_MEMORY;
	}
	XP_STRCPY(m_prettyname, name);
	return 1;
}


XP_Bool
msg_GroupRecord::IsCategory()
{
	return GetCategoryContainer() != NULL;
}

XP_Bool
msg_GroupRecord::IsCategoryContainer()
{
	return (m_flags & F_CATCONT) != 0;
}

XP_Bool
msg_GroupRecord::NeedsExtraInfo()
{
	return (m_flags & F_NEEDEXTRAINFO) != 0;
}

int
msg_GroupRecord::SetNeedsExtraInfo(XP_Bool value)
{
	return TweakFlag(F_NEEDEXTRAINFO, value);
}


int
msg_GroupRecord::SetIsCategoryContainer(XP_Bool value)
{
	// refuse to set a group to be a category container if it has a parent
	// that's a category container.
	if (! (value && GetCategoryContainer()))
		return TweakFlag(F_CATCONT, value);
	else
		return 0;
}


msg_GroupRecord*
msg_GroupRecord::GetCategoryContainer()
{
	if (IsCategoryContainer()) return NULL;
	for (msg_GroupRecord* ptr = m_parent ; ptr ; ptr = ptr->m_parent) {
		if (ptr->IsCategoryContainer()) return ptr;
	}
	return NULL;
}


XP_Bool
msg_GroupRecord::IsProfile()
{
	return (m_flags & F_PROFILE) != 0;
}

int
msg_GroupRecord::SetIsProfile(XP_Bool value)
{
	return TweakFlag(F_PROFILE, value);
}



XP_Bool
msg_GroupRecord::IsExpanded()
{
	return (m_flags & F_EXPANDED) != 0;
}

int
msg_GroupRecord::SetIsExpanded(XP_Bool value)
{
	return TweakFlag(F_EXPANDED, value);
}


XP_Bool
msg_GroupRecord::IsHTMLOKGroup()
{
	return (m_flags & F_HTMLOKGROUP) != 0;
}

int
msg_GroupRecord::SetIsHTMLOKGroup(XP_Bool value)
{
	return TweakFlag(F_HTMLOKGROUP, value);
}



XP_Bool
msg_GroupRecord::IsHTMLOKTree()
{
	return (m_flags & F_HTMLOKTREE) != 0;
}

int
msg_GroupRecord::SetIsHTMLOKTree(XP_Bool value)
{
	return TweakFlag(F_HTMLOKTREE, value);
}



XP_Bool
msg_GroupRecord::IsGroup()
{
	return (m_flags & F_ISGROUP) != 0;
}

int
msg_GroupRecord::SetIsGroup(XP_Bool value)
{
	return TweakFlag(F_ISGROUP, value);
}


XP_Bool
msg_GroupRecord::IsDescendentsLoaded()
{
	return (m_flags & F_DESCENDENTSLOADED) != 0;
}


int
msg_GroupRecord::SetIsDescendentsLoaded(XP_Bool value)
{
	XP_ASSERT(value);			// No reason we'd ever unset this.
	TweakFlag(F_DESCENDENTSLOADED, TRUE);
	msg_GroupRecord* child;
	for (child = m_children ; child ; child = child->m_sibling) {
		child->SetIsDescendentsLoaded(value);
	}
	return 0;
}

XP_Bool msg_GroupRecord::DoesNotExistOnServer()
{
	return (m_flags & F_DOESNOTEXIST) != 0;
}

int msg_GroupRecord::SetDoesNotExistOnServer(XP_Bool value)
{
	if (value)		// turn off group flag if doesn't exist on server.
		TweakFlag(F_ISGROUP, FALSE);
	return TweakFlag(F_DOESNOTEXIST, value);
}

int
msg_GroupRecord::TweakFlag(uint32 flagbit, XP_Bool value)
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


int32
msg_GroupRecord::GetNumKids()
{
	int32 result = 0;
	msg_GroupRecord* child;
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
msg_GroupRecord::GetSaveString()
{
	char* pretty = NULL;
	if (m_prettyname) {
		pretty = NET_Escape(m_prettyname, URL_XALPHAS);
		if (!pretty) return NULL;
	}
	char* fullname = GetFullName();
	if (!fullname) return NULL;
        char* result = PR_smprintf("%s,%s,%lx,%lx,%lx" LINEBREAK,
							   fullname, pretty ? pretty : "",
							   (long) (m_flags & ~RUNTIMEFLAGS),
							   (long) m_addtime,
							   (long) m_uniqueId);
	delete [] fullname;
	if (pretty) XP_FREE(pretty);
	m_flags &= ~F_DIRTY;
	return result;
}


XP_Bool
msg_GroupRecord::IsDirty()
{
	return (m_flags & F_DIRTY) != 0;
}


int32
msg_GroupRecord::GetDepth()
{
	int32 result = 0;
	msg_GroupRecord* tmp = m_parent;
	while (tmp) {
		tmp = tmp->m_parent;
		result++;
	}
	return result;
}




msg_GroupRecord*
msg_GroupRecord::Create(msg_GroupRecord* parent, const char* saveline,
						int32 savelinelength, int32 fileoffset)
{
	char* tmp;
	char* ptr;
	char* endptr;
	char* partname;
	char* prettyname;
	int32 flags;
	int32 addtime;
	int32 uniqueid;
	msg_GroupRecord* result = NULL;

	if (savelinelength < 0) savelinelength = XP_STRLEN(saveline);
	tmp = (char*) XP_ALLOC(savelinelength + 1);
	if (!tmp) return NULL;
	XP_STRNCPY_SAFE(tmp, saveline, savelinelength);
	tmp[savelinelength] = '\0';
	ptr = XP_STRCHR(tmp, ',');
	XP_ASSERT(ptr);
	if (!ptr) goto FAIL;
	*ptr++ = '\0';
	partname = XP_STRRCHR(tmp, '.');
	if (!partname) partname = tmp;
	else partname++;

#ifdef DEBUG_slowAndParanoid
	if (parent->m_partname) {
		char* parentname = parent->GetFullName();
		XP_ASSERT(partname > tmp && partname[-1] == '.');
		partname[-1] = '\0';
		XP_ASSERT(XP_STRCMP(parentname, tmp) == 0);
		partname[-1] = '.';
		delete [] parentname;
		parentname = NULL;
	} else {
		XP_ASSERT(partname == tmp);
	}
#endif

	endptr = XP_STRCHR(ptr, ',');
	XP_ASSERT(endptr);
	if (!endptr) goto FAIL;
	*endptr++ = '\0';
	prettyname = NET_UnEscape(ptr);

	ptr = endptr;
	endptr = XP_STRCHR(ptr, ',');
	XP_ASSERT(endptr);
	if (!endptr) goto FAIL;
	*endptr++ = '\0';
	flags = strtol(ptr, NULL, 16);

	ptr = endptr;
	endptr = XP_STRCHR(ptr, ',');
	XP_ASSERT(endptr);
	if (!endptr) goto FAIL;
	*endptr++ = '\0';
	addtime = strtol(ptr, NULL, 16);

	ptr = endptr;
	uniqueid = strtol(ptr, NULL, 16);

	result = Create(parent, partname, addtime, uniqueid, fileoffset);
	if (result) {
		XP_Bool maybeCategoryContainer = flags & F_CATCONT;
		flags &= ~F_CATCONT;
		result->m_flags = flags;
		if (maybeCategoryContainer)
			result->SetIsCategoryContainer(TRUE);
		if (prettyname && *prettyname) result->SetPrettyName(prettyname);
	}

FAIL:
	XP_FREE(tmp);
	return result;
	
}



msg_IMAPGroupRecord*
msg_IMAPGroupRecord::Create(msg_IMAPGroupRecord* parent, const char* folderName,
							char delimiter, XP_Bool filledInGroup)
{
	char* tmp = NULL;
	char* partname = NULL;
	msg_IMAPGroupRecord* result = NULL;

	if (folderName)
	{
		tmp = XP_STRDUP(folderName);
		partname = XP_STRRCHR(tmp, delimiter);
		if (!partname) partname = tmp;
		else partname++;
	}

#ifdef DEBUG_slowAndParanoid
	if (parent->m_partname) {
		char* parentname = parent->GetFullName();
		XP_ASSERT(partname > tmp && partname[-1] == m_delimiter);
		partname[-1] = '\0';
		XP_ASSERT(XP_STRCMP(parentname, tmp) == 0);
		partname[-1] = m_delimiter;
		delete [] parentname;
		parentname = NULL;
	} else {
		XP_ASSERT(partname == tmp);
	}
#endif

	result = msg_IMAPGroupRecord::CreateFromPartname(parent, partname, delimiter, filledInGroup);
	if (result) {
		result->m_flags = 0;
	}

	XP_FREE(tmp);
	return result;
	
}


msg_IMAPGroupRecord*
msg_IMAPGroupRecord::CreateFromPartname(msg_IMAPGroupRecord* parent, const char* partname, char delimiter, XP_Bool filledInGroup)
{
	msg_IMAPGroupRecord* result = new msg_IMAPGroupRecord(parent, partname, delimiter, filledInGroup);
	if (result && partname && !result->m_partname) {
		// We ran out of memory.
		delete result;
		result = NULL;
	}
	return result;
}


msg_IMAPGroupRecord::msg_IMAPGroupRecord(msg_IMAPGroupRecord* parent,
										 const char* partname, char delimiter, XP_Bool filledInGroup) : msg_GroupRecord(parent, partname, 0, 0, 0, delimiter)
{
	m_flags = 0;
	m_allGrandChildrenDiscovered = FALSE;
	m_filledInGroup = filledInGroup;
}

