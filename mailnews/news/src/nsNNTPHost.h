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

#ifndef __msg_NewsHost__
#define __msg_NewsHost__ 1

#include "nsINNTPHost.h"

#include "msgCore.h"    // precompiled header...

#include "nsMsgFolderFlags.h"

#include "nntpCore.h"
#include "nsINNTPHost.h"
#include "nsINNTPCategory.h"

#include "nsINNTPCategoryContainer.h"
#include "nsNNTPCategoryContainer.h"

#include "nsNNTPHost.h"
#include "nsMsgKeySet.h"

#include "nsMsgGroupRecord.h"

#include "nsINNTPNewsgroup.h"
#include "nsNNTPNewsgroup.h"

#include "nsINNTPNewsgroupList.h"
#include "nsNNTPNewsgroupList.h"

#include "nsIMsgFolder.h"
#include "nsISupportsArray.h"
#include "nsIEnumerator.h"

#include "nsFileStream.h"

#include "nsNewsUtils.h"

#include "prprf.h"
#include "prmem.h"
#include "plstr.h"
#include "plhash.h"
#include "prio.h"

#include "nsCOMPtr.h"

/* temporary hacks to test if this compiles */
typedef void MSG_GroupName;

class nsNNTPHost : public nsINNTPHost {
public:
    nsNNTPHost();
	virtual ~nsNNTPHost();

    NS_DECL_ISUPPORTS
    // nsINNTPHost
    NS_IMETHOD Initialize(const char *username, const char *hostname, PRInt32 port);
    
    NS_IMPL_CLASS_GETSET(SupportsExtensions, PRBool,
                         m_supportsExtensions);
    
	NS_IMETHOD AddExtension (const char *ext);
	NS_IMETHOD QueryExtension (const char *ext, PRBool *_retval);
    
    NS_IMPL_CLASS_GETSET(PostingAllowed, PRBool, m_postingAllowed);
    
    NS_IMPL_CLASS_GETTER(GetPushAuth, PRBool, m_pushAuth);
    NS_IMETHOD SetPushAuth(PRBool value);
    
    NS_IMPL_CLASS_GETSET(LastUpdatedTime, PRUint32, m_lastGroupUpdate);

    NS_IMETHOD GetNewsgroupList(const char *name,nsINNTPNewsgroupList **_retval);
	
    NS_IMETHOD GetNewsgroupAndNumberOfID(const char *message_id,
                                         nsINNTPNewsgroup **group,
                                         PRUint32 *messageNumber);

    /* get this from MSG_Master::FindNewsFolder */
    NS_IMETHOD FindNewsgroup(const char *name, PRBool create,
		nsINNTPNewsgroup **_retval) { NS_ASSERTION(0, "unimplemented!"); return NS_OK;}
    
	NS_IMETHOD AddPropertyForGet (const char *property, const char *value);
	NS_IMETHOD QueryPropertyForGet (const char *property, char **_retval);

    NS_IMETHOD AddSearchableGroup(const char *name);
    // should these go into interfaces?
	NS_IMETHOD QuerySearchableGroup (const char *group, PRBool *);
    NS_IMETHOD QuerySearchableGroupCharsets(const char *group, char **);

    // Virtual groups
    NS_IMETHOD AddVirtualGroup(const char *responseText) { return NS_OK;}
    NS_IMETHOD SetIsVirtualGroup(const char *name, PRBool isVirtual);
    NS_IMETHOD SetIsVirtualGroup(const char *name, PRBool isVirtual,
                                 nsMsgGroupRecord *inGroupRecord);
    NS_IMETHOD GetIsVirtualGroup(const char *name, PRBool *_retval);

    // custom/searchable headers
    NS_IMETHOD AddSearchableHeader(const char *headerName);
    NS_IMETHOD QuerySearchableHeader(const char *headerName, PRBool *_retval);
    
	// Write out the newsrc for this host right now.  In general, either
	// MarkDirty() or WriteIfDirty() should be called instead.
	NS_IMETHOD WriteNewsrc();

	// Write out the newsrc for this host right now, if anything has changed
	// in it.
	NS_IMETHOD WriteIfDirty();

	// Note that something has changed, and we need to rewrite the newsrc file
	// for this host at some point.
	NS_IMETHOD MarkDirty();

    /* the Setter implementation is a little more complex */
    NS_IMPL_CLASS_GETTER(GetNewsRCFilename, char *, m_filename);
    NS_IMETHOD SetNewsRCFilename(char *);
    

    // helper for accessing the above accessors from within this class
    // (this is what the pre-mozilla API looked like)
    char *GetNewsrcFileName() { return m_filename; };
    
    NS_IMETHOD FindGroup(const char* name, nsINNTPNewsgroup* *_retval);
    NS_IMETHOD AddGroup(const char *name,
                        nsINNTPNewsgroup **_retval);
    
    NS_IMETHOD AddGroup(const char *name,
                        nsMsgGroupRecord *groupRecord,
                        nsINNTPNewsgroup **_retval);
    
    NS_IMETHOD RemoveGroupByName(const char *name);
    NS_IMETHOD RemoveGroup(nsINNTPNewsgroup*);
    
    NS_IMETHOD AddNewNewsgroup(const char *name,
                               PRInt32 first,
                               PRInt32 last,
                               const char *flags,
                               PRBool xactiveFlags);

	/* Name of directory to store newsgroup
       databases in.  This needs to have
       "/name" appended to it, and the
       whole thing can be passed to the XP_File
       stuff with a type of xpXoverCache. */
    NS_IMETHOD GetDbDirName(char * *aDbDirName) {NS_ASSERTION(0, "unimplemented"); return NS_OK;};
    /* helper for internal accesses - part of the Pre-Mozilla API) */
    const char *GetDBDirName();

    /* Returns a list of newsgroups.  The result
       must be free'd using PR_Free(); the
       individual strings must not be free'd. */
    NS_IMETHOD GetGroupList(char **_retval) { return NS_OK;}

    NS_IMETHOD DisplaySubscribedGroup(nsINNTPNewsgroup *newsgroup,
                                      PRInt32 first_message,
                                      PRInt32 last_message,
                                      PRInt32 total_messages,
                                      PRBool visit_now);
    // end of nsINNTPHost
    
private:
    // simplify the QueryInterface calls
    static nsIMsgFolder *getFolderFor(nsINNTPNewsgroup *group);
    static nsIMsgFolder *getFolderFor(nsINNTPCategoryContainer *catContainer);
    
    NS_METHOD CleanUp();
    virtual PRBool IsNews () { return PR_TRUE; }
	virtual nsINNTPHost *GetNewsHost() { return this; }

    // 
    void addNew(MSG_GroupName* group);
	void ClearNew();

	virtual void dump();

	virtual PRInt32 getPort();

  
	// Return the name of this newshost.
	const char* getStr();

	// Returns the name of this newshost, possibly followed by ":<port>" if
	// the port number for this newshost is not the default.
	const char* getNameAndPort();

    // Returns a fully descriptive name for this newshost, including the
    // above ":<port>" and also possibly a trailing (and localized) property
	virtual const char* getFullUIName();


	// Get the nsIMsgFolder which represents this host; the children
	// of this nsIMsgFolder are the groups in this host.
	nsIMsgFolder* GetHostInfo() {return m_hostinfo;}


#ifdef HAVE_MASTER
	MSG_Master* GetMaster() {
		return m_master;
	}
#endif



	// GetNumGroupsNeedingCounts() returns how many newsgroups we have
	// that we don't have an accurate count of unread/total articles.
	NS_IMETHOD GetNumGroupsNeedingCounts(PRInt32 *);

	// GetFirstGroupNeedingCounts() returns the name of the first newsgroup
	// that does not have an accurate count of unread/total articles.  The
	// string must be free'd by the caller using PR_Free().
    NS_IMETHOD GetFirstGroupNeedingCounts(char **);
	

	// GetFirstGroupNeedingExtraInfo() returns the name of the first newsgroup
	// that does not have extra info (xactive flags and prettyname).  The
	// string must be free'd by the caller using PR_Free().
	NS_IMETHOD GetFirstGroupNeedingExtraInfo(char **);
	

	void SetWantNewTotals(PRBool value); // Sets this bit on every newsgroup
										  // in this host.

	PRBool NeedsExtension (const char *ext);




	// Returns the base part of the URL for this newshost, in the
	// form "news://hostname:port" or "snews://hostname:port".  
	// Note that no trailing slash is appended, and that the
	// ":port" part will be omitted if this host uses the default
	// port.
	const char* GetURLBase();

	PRBool GetEverExpanded() {return m_everexpanded;}
	void SetEverExpanded(PRBool value) {m_everexpanded = value;}
	PRBool GetCheckedForNew() {return m_checkedForNew;}
	void SetCheckedForNew(PRBool value) {m_checkedForNew = value;}
	void SetGroupSucceeded(PRBool value) {m_groupSucceeded = value;}
	// Completely obliterate this news host.  Remove all traces of it from
	// disk and memory.
	PRInt32 RemoveHost();
	PRInt32 DeleteFiles();

	// Returns the pretty name for the given group.  The resulting string
	// must be free'd using delete[].
	char* GetPrettyName(char* name);
	NS_IMETHOD SetPrettyName(const char* name, const char* prettyname);

	PRTime GetAddTime(char* name);

	// Returns a unique integer associated with this newsgroup.  This is
	// mostly used by Win16 to generate a 8.3 filename.
	PRInt32 GetUniqueID(char* name);

	PRBool IsCategory(char* name);
	PRBool IsCategoryContainer(char* name);
	PRInt32 SetIsCategoryContainer(const char* name, PRBool value, nsMsgGroupRecord *inGroupRecord = nsnull);
	
	NS_IMETHOD SetGroupNeedsExtraInfo(const char *name, PRBool value);
	// Finds the container newsgroup for this category (or nsnull if this isn't
	// a category).  The resulting string must be free'd using delete[].
	char* GetCategoryContainer(const char* name, nsMsgGroupRecord *inGroupRecord = nsnull);
    nsINNTPNewsgroup * GetCategoryContainerFolderInfo(const char *name);

	// Get/Set whether this is a real group (as opposed to a container of
	// other groups, like "mcom".)
	PRBool IsGroup(char* name);
	PRInt32 SetIsGroup(char* name, PRBool value);
	

	// Returns PR_TRUE if it's OK to post HTML in this group (either because the
	// bit is on for this group, or one of this group's ancestor's has marked
	// all of its descendents as being OK for HTML.)
	PRBool IsHTMLOk(char* name);

	// Get/Set if it's OK to post HTML in just this group.
	PRBool IsHTMLOKGroup(char* name);
	PRInt32 SetIsHTMLOKGroup(char* name, PRBool value);

	// Get/Set if it's OK to post HTML in this group and all of its subgroups.
	PRBool IsHTMLOKTree(char* name);
	PRInt32 SetIsHTMLOKTree(char* name, PRBool value);

	// Create the given group (if not already present).  Returns 0 if the
	// group is already present, 1 if we had to create it, negative on error.
	// The given group will have the "isgroup" bit set on it (in other words,
	// it is not to be just a container of other groups, like "mcom" is.)
	PRInt32 NoticeNewGroup(const char* name, nsMsgGroupRecord **outGroupRecord = nsnull);
	

	// Makes sure that we have records in memory for all known descendants
	// of the given newsgroup.
	PRInt32 AssureAllDescendentsLoaded(nsMsgGroupRecord* group);


	PRInt32 SaveHostInfo();

	// Suck the entire hostinfo file into memory.  If force is PR_TRUE, then throw
	// away whatever we had in memory first.
	PRInt32 Inhale(PRBool force = PR_FALSE);

	// If we inhale'd, then write thing out to the file and free up the
	// memory.
	PRInt32 Exhale();

	// Inhale, but make believe the file is empty.  In other words, set the
	// inhaled bit, but empty out the memory.
	PRInt32 EmptyInhale();

	nsMsgGroupRecord* GetGroupTree() {return m_groupTree;}
	PRTime GetFirstNewDate() {return m_firstnewdate;}
	
	NS_IMETHOD GroupNotFound(const char *name, PRBool opening);

	PRInt32 ReorderGroup (nsINNTPNewsgroup *groupToMove, nsINNTPNewsgroup *groupToMoveBefore, PRInt32 *newIdx);

protected:
	void OpenGroupFile(const PRIntn = PR_WRONLY);
  
	static void WriteTimer(void* closure);
	PRInt32 CreateFileHeader();
	PRInt32 ReadInitialPart();
	nsMsgGroupRecord* FindGroupInBlock(nsMsgGroupRecord* parent,
									  char* name,
									  PRInt32* comp);
	nsMsgGroupRecord* LoadSingleEntry(nsMsgGroupRecord* parent,
									 char* name,
									 PRInt32 min, PRInt32 max);
	static PRInt32 InhaleLine(char* line, PRUint32 length, void* closure);
	nsMsgGroupRecord* FindOrCreateGroup(const char* name,
									   PRInt32 * statusOfMakingGroup = nsnull);	

	nsINNTPCategoryContainer *SwitchNewsToCategoryContainer(nsINNTPNewsgroup *newsInfo);
  
    nsINNTPNewsgroup * SwitchCategoryContainerToNews(nsINNTPCategoryContainer *catContainerInfo);

	char* m_hostname;
    char* m_username;
	PRInt32 m_port;

	char* m_nameAndPort;
	char* m_fullUIName;

	nsISupportsArray* m_groups;	// List of nsINNTPNewsgroup* objects.
	nsISupportsArray* m_newsgrouplists;  // List of nsINNTPNewsgroupList* objects.

#ifdef HAVE_MASTER
	MSG_Master* m_master;
#endif

	nsIMsgFolder* m_hostinfo;	// Object representing entire newshost in
								// tree.
	char* m_optionLines;


 	char* m_filename;			/* The name of the newsrc file associated with
                                   this host.  This will be of the form:

                                   ""		  meaning .newsrc or .snewsrc
                                   HOST       meaning .newsrc-HOST
                                   HOST:PORT  meaning .newsrc-HOST:PORT

								   Whether it begins with .newsrc or .snewsrc
								   depends on a special property slot
                                   (we pass one of
								   the types xpNewsRC or xpSNewsRC to xp_file.)

                                   The reason this is not simply derived from
                                   the host_name and port slots is that it's
                                   not a 1:1 mapping; if the user has a file
                                   called "newsrc", we will use that for the
                                   default host (the "" name.)  Likewise,
								   ".newsrc-H" and ".newsrc-H:119" are
								   ambiguous.
                                 */

	char* m_dbfilename;
	PRBool m_dirty;
	PRBool m_supportsExtensions;
	void* m_writetimer;
	char* m_urlbase;
	PRBool m_everexpanded;		// Whether the user ever opened up this
								// newshost this session.
	PRBool m_checkedForNew;	// Whether we've checked for new newgroups
								// in this newshost this session.
	PRBool m_groupSucceeded;	// Whether a group command has succeeded this 
								// session, protect against server bustage
								// where it says no group exists.

	nsVoidArray m_supportedExtensions;
	nsVoidArray m_searchableGroups;
	nsVoidArray m_searchableHeaders;
	// ### mwelch Added to determine what charsets can be used
	//            for each table.
	PLHashTable * m_searchableGroupCharsets;

	nsVoidArray m_propertiesForGet;
	nsVoidArray m_valuesForGet;

	PRBool m_postingAllowed;
	PRBool m_pushAuth; // PR_TRUE if we should volunteer authentication without a
						// challenge

	PRUint32 m_lastGroupUpdate;
	PRTime m_firstnewdate;


	nsMsgGroupRecord* m_groupTree; // Tree of groups we're remembering.
	PRBool m_inhaled;			// Whether we inhaled the entire list of
								// groups, or just some.
	PRInt32 m_groupTreeDirty;		// Whether the group tree is dirty.  If 0, then
								// we don't need to write anything.  If 1, then
								// we can write things in place.  If >1, then
								// we need to rewrite the whole tree file.
	char* m_hostinfofilename;	// Filename of the hostinfo file.

	static nsNNTPHost* M_FileOwner; // In an effort to save file descriptors,
									  // only one newshost ever has its
									  // hostinfo file opened.  This is the
									  // one.

	PRFileDesc * m_groupFile;		// File handle to the hostinfo file.
	char* m_groupFilePermissions; // Permissions used to create the above
								  // file handle.

	char* m_block;				// A block of text read from the hostinfo file.
	PRInt32 m_blockSize;			// How many bytes allocated in m_block.
	PRInt32 m_blockStart;			// Where in the file we read the data that is
								// currently sitting in m_block.
	PRInt32 m_fileStart;			// Where in the file the actual newsgroup data
								// starts.
	PRInt32 m_fileSize;			// Total number of bytes in the hostinfo file.
	PRInt32 m_uniqueId;			// Unique number assigned to each newsgroup.
	
};

#endif /* __msg_NewsHost__ */
