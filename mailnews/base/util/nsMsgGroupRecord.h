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

#ifndef _nsMsgGroupRecord_h_
#define _nsMsgGroupRecord_h__

#include "msgCore.h"
#include "prtypes.h"
#include "nsISupports.h"

class nsIMAPGroupRecord;

class NS_MSG_BASE nsMsgGroupRecord {
public:
	static nsMsgGroupRecord* Create(nsMsgGroupRecord* parent,
								   const char* partname,
                                   PRInt64 m_addtime,
								   PRInt32 uniqueid,
								   PRInt32 fileoffset);
	static nsMsgGroupRecord* Create(nsMsgGroupRecord* parent,
								   const char* saveline,
								   PRInt32 savelinelength,
								   PRInt32 fileoffset);


	virtual void	InitializeSibling();
	virtual PRBool IsIMAPGroupRecord() { return PR_FALSE; }
	virtual nsIMAPGroupRecord *GetIMAPGroupRecord() { return 0; }
	
	// This is just like PL_strcmp(), except it works on news group names.
	// A container groupname is always less than any contained groups.
	// So, "netscape.devs-client-technical" > "netscape.devs.directory", even
	// though PL_strcmp says otherwise.  (YICK!)
	static int GroupNameCompare(const char* name1,
								const char* name2,
								char delimiter = '.',
								PRBool caseInsensitive = PR_FALSE);


	virtual ~nsMsgGroupRecord();

	nsMsgGroupRecord* FindDescendant(const char* name);

	nsMsgGroupRecord* GetParent() {return m_parent;}
	nsMsgGroupRecord* GetChildren() {return m_children;}
	nsMsgGroupRecord* GetSibling() {return m_sibling;}
	nsMsgGroupRecord* GetSiblingOrAncestorSibling();
	nsMsgGroupRecord* GetNextAlphabetic();

	nsMsgGroupRecord* GetNextAlphabeticNoCategories();

	const char* GetPartName() {return m_partname;}

	// The resulting string must be free'd using delete[].
	char* GetFullName();

	const char* GetPrettyName() {return m_prettyname;}
	int SetPrettyName(const char* prettyname);

	PRInt64 GetAddTime() {return m_addtime;}

	virtual PRBool IsCategory();
	virtual PRBool IsCategoryContainer();
	virtual int SetIsCategoryContainer(PRBool value);

	nsMsgGroupRecord* GetCategoryContainer();

	// Get/Set whether this is a virtual newsgroup.
	nsresult IsVirtual(PRBool *retval);
    nsresult SetIsVirtual(PRBool value);

	// Get/Set whether this is really a newsgroup (and not just a container
	// for newsgroups).
	virtual PRBool IsGroup();
	int SetIsGroup(PRBool value);

	PRBool IsDescendentsLoaded();
	int SetIsDescendentsLoaded(PRBool value);

	PRBool IsExpanded();
	int SetIsExpanded(PRBool value);

	PRBool IsHTMLOKGroup();
	int SetIsHTMLOKGroup(PRBool value);

	PRBool IsHTMLOKTree();
	int SetIsHTMLOKTree(PRBool value);

	PRBool NeedsExtraInfo();
	int SetNeedsExtraInfo(PRBool value);

	PRBool DoesNotExistOnServer();
	int SetDoesNotExistOnServer(PRBool value);

	PRInt32 GetUniqueID() {return m_uniqueId;}

	PRInt32 GetFileOffset() {return m_fileoffset;}
	int SetFileOffset(PRInt32 value) {m_fileoffset = value; return 0;}

	// Get the number of descendents (not including ourself) that are
	// really newsgroups.
	PRInt32 GetNumKids();

	// Gets the string that represents this group in the save file.  The
	// resulting string must be free'd with PR_Free().
	char* GetSaveString();

	PRBool IsDirty();			// Whether this record has had changes made
								// to it.  Cleared by calls to GetSaveString().

	PRInt32 GetDepth();			// Returns how deep in the heirarchy we are.
								// Basically, the number of dots in the full
								// newsgroup name, plus 1.
	virtual char GetHierarchySeparator() { return '.'; }

protected:
	nsMsgGroupRecord(nsMsgGroupRecord* parent,
								   const char* partname,
								   PRInt64 m_addtime,
								   PRInt32 uniqueid,
								   PRInt32 fileoffset,
								   char delimiter = '.');
	int TweakFlag(PRUint32 flagbit, PRBool value);
	char* SuckInName(char* ptr);

	char* m_partname;
	char* m_prettyname;
	nsMsgGroupRecord* m_parent;
	nsMsgGroupRecord* m_children;
	nsMsgGroupRecord* m_sibling;
	PRUint32 m_flags;
	PRInt64 m_addtime;
	PRInt32 m_uniqueId;
	PRInt32 m_fileoffset;
	char m_delimiter;
};




#endif /* _grec_h_ */
