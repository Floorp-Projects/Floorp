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

// This class should only be used by the subscribe UI and by newshost.cpp.
// And, well, a bit by the category code.  Everyone else should use the stuff
// in newshost.h.

#ifndef _grec_h_
#define _grec_h_

class msg_IMAPGroupRecord;

class msg_GroupRecord {
public:
	static msg_GroupRecord* Create(msg_GroupRecord* parent,
								   const char* partname,
								   time_t m_addtime,
								   int32 uniqueid,
								   int32 fileoffset);
	static msg_GroupRecord* Create(msg_GroupRecord* parent,
								   const char* saveline,
								   int32 savelinelength,
								   int32 fileoffset);

	virtual XP_Bool IsIMAPGroupRecord() { return FALSE; }
	virtual msg_IMAPGroupRecord *GetIMAPGroupRecord() { return 0; }
	
	// This is just like strcmp(), except it works on news group names.
	// A container groupname is always less than any contained groups.
	// So, "netscape.devs-client-technical" > "netscape.devs.directory", even
	// though strcmp says otherwise.  (YICK!)
	static int GroupNameCompare(const char* name1,
								const char* name2,
								char delimiter = '.');


	virtual ~msg_GroupRecord();

	msg_GroupRecord* FindDescendant(const char* name);

	msg_GroupRecord* GetParent() {return m_parent;}
	msg_GroupRecord* GetChildren() {return m_children;}
	msg_GroupRecord* GetSibling() {return m_sibling;}
	msg_GroupRecord* GetSiblingOrAncestorSibling();
	msg_GroupRecord* GetNextAlphabetic();

	msg_GroupRecord* GetNextAlphabeticNoCategories();

	const char* GetPartName() {return m_partname;}

	// The resulting string must be free'd using delete[].
	char* GetFullName();

	const char* GetPrettyName() {return m_prettyname;}
	int SetPrettyName(const char* prettyname);

	time_t GetAddTime() {return m_addtime;}

	virtual XP_Bool IsCategory();
	virtual XP_Bool IsCategoryContainer();
	virtual int SetIsCategoryContainer(XP_Bool value);

	msg_GroupRecord* GetCategoryContainer();

	// Get/Set whether this is a virtual newsgroup.
	XP_Bool IsProfile();
	int SetIsProfile(XP_Bool value);

	// Get/Set whether this is really a newsgroup (and not just a container
	// for newsgroups).
	virtual XP_Bool IsGroup();
	int SetIsGroup(XP_Bool value);

	XP_Bool IsDescendentsLoaded();
	int SetIsDescendentsLoaded(XP_Bool value);

	XP_Bool IsExpanded();
	int SetIsExpanded(XP_Bool value);

	XP_Bool IsHTMLOKGroup();
	int SetIsHTMLOKGroup(XP_Bool value);

	XP_Bool IsHTMLOKTree();
	int SetIsHTMLOKTree(XP_Bool value);

	XP_Bool NeedsExtraInfo();
	int SetNeedsExtraInfo(XP_Bool value);

	XP_Bool DoesNotExistOnServer();
	int SetDoesNotExistOnServer(XP_Bool value);

	int32 GetUniqueID() {return m_uniqueId;}

	int32 GetFileOffset() {return m_fileoffset;}
	int SetFileOffset(int32 value) {m_fileoffset = value; return 0;}

	// Get the number of descendents (not including ourself) that are
	// really newsgroups.
	int32 GetNumKids();

	// Gets the string that represents this group in the save file.  The
	// resulting string must be free'd with XP_FREE().
	char* GetSaveString();

	XP_Bool IsDirty();			// Whether this record has had changes made
								// to it.  Cleared by calls to GetSaveString().

	int32 GetDepth();			// Returns how deep in the heirarchy we are.
								// Basically, the number of dots in the full
								// newsgroup name, plus 1.
	virtual char GetHierarchySeparator() { return '.'; }

protected:
	msg_GroupRecord(msg_GroupRecord* parent,
								   const char* partname,
								   time_t m_addtime,
								   int32 uniqueid,
								   int32 fileoffset,
								   char delimiter = '.');
	int TweakFlag(uint32 flagbit, XP_Bool value);
	char* SuckInName(char* ptr);

	char* m_partname;
	char* m_prettyname;
	msg_GroupRecord* m_parent;
	msg_GroupRecord* m_children;
	msg_GroupRecord* m_sibling;
	uint32 m_flags;
	time_t m_addtime;
	int32 m_uniqueId;
	int32 m_fileoffset;
	char m_delimiter;
};



class msg_IMAPGroupRecord : public msg_GroupRecord {


public:
	static msg_IMAPGroupRecord* Create(msg_IMAPGroupRecord* parent,
										const char* folderName,
										char delimiter,
										XP_Bool filledInGroup);

	static msg_IMAPGroupRecord* CreateFromPartname(msg_IMAPGroupRecord* parent, const char* partname, char delimiter,
		XP_Bool filledInGroup);

	virtual XP_Bool IsCategory() { return FALSE;}
	virtual XP_Bool IsCategoryContainer() { return FALSE; }
	virtual int SetIsCategoryContainer(XP_Bool /* value */) { XP_ASSERT(0); return 0; }
	virtual XP_Bool IsIMAPGroupRecord() { return TRUE; }
	virtual msg_IMAPGroupRecord *GetIMAPGroupRecord() { return this; }

	virtual XP_Bool IsGroup() { return TRUE; }

	virtual char GetHierarchySeparator() { return m_delimiter; }
	void SetHierarchySeparator(char delimiter) { m_delimiter = delimiter; }

	XP_Bool GetAllGrandChildrenDiscovered() { return m_allGrandChildrenDiscovered; }
	void SetAllGrandChildrenDiscovered(XP_Bool discovered) { m_allGrandChildrenDiscovered = discovered; }

	XP_Bool GetIsGroupFilledIn() { return m_filledInGroup; }
	void SetIsGroupFilledIn(XP_Bool filledIn) { m_filledInGroup = filledIn; }

	void SetFlags(uint32 flags) { m_flags = flags; }
	uint32 GetFlags() { return m_flags; }

protected:
	msg_IMAPGroupRecord(msg_IMAPGroupRecord* parent, const char* partname, char delimiter, XP_Bool filledInGroup);

	XP_Bool m_allGrandChildrenDiscovered;
	XP_Bool m_filledInGroup;
	uint32 m_flags;

};


#endif /* _grec_h_ */
