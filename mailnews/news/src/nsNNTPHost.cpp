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

#include "nsMsgFolderFlags.h"

#include "nntpCore.h"
#include "nsINNTPHost.h"
#include "nsINNTPCategory.h"

#include "nsINNTPCategoryContainer.h"
#include "nsNNTPCategoryContainer.h"

#include "nsNNTPHost.h"
#include "nsNNTPArticleSet.h"

#include "nsMsgGroupRecord.h"

#include "nsINNTPNewsgroup.h"
#include "nsNNTPNewsgroup.h"

#include "nsIMsgFolder.h"
#include "nsISupportsArray.h"
#include "nsIEnumerator.h"

/* for XP_FilePerm */
#include "xp_file.h"

/* for XP_HashTable */
#include "xp_hash.h"

/* for XP_StripLine */
#include "xp_str.h"

/* for LINEBREAK, etc */
#include "fe_proto.h"

#include "prprf.h"
#include "prmem.h"
#include "plstr.h"

/* temporary hacks to test if this compiles */
typedef void MSG_GroupName;

#ifdef XP_UNIX
static const char LINEBREAK_START = '\012';
#else
static const char LINEBREAK_START = '\015';
#endif

#define PROTOCOL_DEBUG

/* externally declare this function...it doesn't exist in mailnews yet though...hmmm */

extern int msg_LineBuffer (const char *net_buffer, PRInt32 net_buffer_size,
						   char **bufferP, PRUint32 *buffer_sizeP,
						   PRUint32 *buffer_fpP,
						   PRBool convert_newlines_p,
						   PRInt32 (*per_line_fn) (char *line, PRUint32 line_length, void *closure),
						   void *closure);


class nsNNTPHost : public nsINNTPHost {
public:
#ifdef HAVE_MASTER
	nsNNTPHost(MSG_Master* master, const char* name,
				 PRInt32 port);
#else
    nsNNTPHost(const char *name, PRInt32 port);
#endif
               
	virtual ~nsNNTPHost();

    NS_DECL_ISUPPORTS
    // nsINNTPHost
    
    NS_IMPL_CLASS_GETSET(SupportsExtensions, PRBool,
                         m_supportsExtensions);
    
	NS_IMETHOD AddExtension (const char *ext);
	NS_IMETHOD QueryExtension (const char *ext, PRBool *_retval);
    
    NS_IMPL_CLASS_GETSET(PostingAllowed, PRBool, m_postingAllowed);
    
    NS_IMPL_CLASS_GETTER(GetPushAuth, PRBool, m_pushAuth);
    NS_IMETHOD SetPushAuth(PRBool value);
    
    NS_IMPL_CLASS_GETSET(LastUpdatedTime, PRInt64, m_lastGroupUpdate);

    NS_IMETHOD GetNewsgroupList(const char *groupname,
		nsINNTPNewsgroupList **_retval) { NS_ASSERTION(0, "unimplemented"); return NS_OK;};

    NS_IMETHOD GetNewsgroupAndNumberOfID(const char *message_id,
                                         nsINNTPNewsgroup **group,
                                         PRUint32 *messageNumber);

    /* get this from MSG_Master::FindNewsFolder */
    NS_IMETHOD FindNewsgroup(const char *groupname, PRBool create,
		nsINNTPNewsgroup **_retval) { NS_ASSERTION(0, "unimplemented!"); return NS_OK;}
    
	NS_IMETHOD AddPropertyForGet (const char *property, const char *value);
	NS_IMETHOD QueryPropertyForGet (const char *property, char **_retval);

    NS_IMETHOD AddSearchableGroup(const char *groupname);
    // should these go into interfaces?
	NS_IMETHOD QuerySearchableGroup (const char *group, PRBool *);
    NS_IMETHOD QuerySearchableGroupCharsets(const char *group, char **);

    // Virtual groups
    NS_IMETHOD AddVirtualGroup(const char *responseText) { return NS_OK;}
    NS_IMETHOD SetIsVirtualGroup(const char *groupname, PRBool isVirtual);
    NS_IMETHOD SetIsVirtualGroup(const char *groupname, PRBool isVirtual,
                                 nsMsgGroupRecord *inGroupRecord);
    NS_IMETHOD GetIsVirtualGroup(const char *groupname, PRBool *_retval);

    // custom/searchable headers
    NS_IMETHOD AddSearchableHeader(const char *headerName);
    NS_IMETHOD QuerySearchableHeader(const char *headerName, PRBool *_retval);
    
	// Go load the newsrc for this host.  Creates the subscribed hosts as
	// children of the given nsIMsgFolder.
	NS_IMETHOD LoadNewsrc();
    
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
    NS_IMETHOD AddGroup(const char *groupname,
                        nsINNTPNewsgroup **retval);
    
    NS_IMETHOD AddGroup(const char *groupname,
                        nsMsgGroupRecord *groupRecord,
                        nsINNTPNewsgroup **retval);
    
    NS_IMETHOD RemoveGroupByName(const char *groupName);
    NS_IMETHOD RemoveGroup(nsINNTPNewsgroup*);
    
    NS_IMETHOD AddNewNewsgroup(const char *groupName,
                               PRInt32 first,
                               PRInt32 last,
                               const char *flags,
                               PRBool xactiveFlags);

	/* Name of directory to store newsgroup
       databases in.  This needs to have
       "/groupname" appended to it, and the
       whole thing can be passed to the XP_File
       stuff with a type of xpXoverCache. */
    NS_IMETHOD GetDbDirName(char * *aDbDirName) {NS_ASSERTION(0, "unimplemented"); return NS_OK;};
    /* helper for internal accesses - part of the Pre-Mozilla API) */
    const char *GetDBDirName();

    /* Returns a list of newsgroups.  The result
       must be free'd using PR_Free(); the
       individual strings must not be free'd. */
    NS_IMETHOD GetGroupList(char **_retval) { return NS_OK;}

    NS_IMETHOD DisplaySubscribedGroup(const char *groupname,
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
	int RemoveHost();
	int DeleteFiles();

	// Returns the pretty name for the given group.  The resulting string
	// must be free'd using delete[].
	char* GetPrettyName(char* groupname);
	NS_IMETHOD SetPrettyName(const char* groupname, const char* prettyname);

	time_t GetAddTime(char* groupname);

	// Returns a unique integer associated with this newsgroup.  This is
	// mostly used by Win16 to generate a 8.3 filename.
	PRInt32 GetUniqueID(char* groupname);

	PRBool IsCategory(char* groupname);
	PRBool IsCategoryContainer(char* groupname);
	int SetIsCategoryContainer(const char* groupname, PRBool value, nsMsgGroupRecord *inGroupRecord = NULL);
	
	NS_IMETHOD SetGroupNeedsExtraInfo(const char *groupname, PRBool value);
	// Finds the container newsgroup for this category (or NULL if this isn't
	// a category).  The resulting string must be free'd using delete[].
	char* GetCategoryContainer(const char* groupname, nsMsgGroupRecord *inGroupRecord = NULL);
	nsINNTPNewsgroup *GetCategoryContainerFolderInfo(const char *groupname);


	// Get/Set whether this is a real group (as opposed to a container of
	// other groups, like "mcom".)
	PRBool IsGroup(char* groupname);
	int SetIsGroup(char* groupname, PRBool value);
	

	// Returns PR_TRUE if it's OK to post HTML in this group (either because the
	// bit is on for this group, or one of this group's ancestor's has marked
	// all of its descendents as being OK for HTML.)
	PRBool IsHTMLOk(char* groupname);

	// Get/Set if it's OK to post HTML in just this group.
	PRBool IsHTMLOKGroup(char* groupname);
	int SetIsHTMLOKGroup(char* groupname, PRBool value);

	// Get/Set if it's OK to post HTML in this group and all of its subgroups.
	PRBool IsHTMLOKTree(char* groupname);
	int SetIsHTMLOKTree(char* groupname, PRBool value);

	// Create the given group (if not already present).  Returns 0 if the
	// group is already present, 1 if we had to create it, negative on error.
	// The given group will have the "isgroup" bit set on it (in other words,
	// it is not to be just a container of other groups, like "mcom" is.)
	int NoticeNewGroup(const char* groupname, nsMsgGroupRecord **outGroupRecord = NULL);
	

	// Makes sure that we have records in memory for all known descendants
	// of the given newsgroup.
	int AssureAllDescendentsLoaded(nsMsgGroupRecord* group);


	int SaveHostInfo();

	// Suck the entire hostinfo file into memory.  If force is PR_TRUE, then throw
	// away whatever we had in memory first.
	int Inhale(PRBool force = PR_FALSE);

	// If we inhale'd, then write thing out to the file and free up the
	// memory.
	int Exhale();

	// Inhale, but make believe the file is empty.  In other words, set the
	// inhaled bit, but empty out the memory.
	int EmptyInhale();

	nsMsgGroupRecord* GetGroupTree() {return m_groupTree;}
	time_t GetFirstNewDate() {return m_firstnewdate;}
	
	NS_IMETHOD GroupNotFound(const char *groupName, PRBool opening);

	int ReorderGroup (nsINNTPNewsgroup *groupToMove, nsINNTPNewsgroup *groupToMoveBefore, PRInt32 *newIdx);

protected:
	void OpenGroupFile(const XP_FilePerm permissions = XP_FILE_UPDATE_BIN);
	PRInt32 RememberLine(char* line);
	static PRInt32 ProcessLine_s(char* line, PRUint32 line_size, void* closure);
	PRInt32 ProcessLine(char* line, PRUint32 line_size);
	static void WriteTimer(void* closure);
	int CreateFileHeader();
	int ReadInitialPart();
	nsMsgGroupRecord* FindGroupInBlock(nsMsgGroupRecord* parent,
									  char* groupname,
									  PRInt32* comp);
	nsMsgGroupRecord* LoadSingleEntry(nsMsgGroupRecord* parent,
									 char* groupname,
									 PRInt32 min, PRInt32 max);
	static PRInt32 InhaleLine(char* line, PRUint32 length, void* closure);
	nsMsgGroupRecord* FindOrCreateGroup(const char* groupname,
									   int* statusOfMakingGroup = NULL);	

	nsINNTPCategoryContainer *SwitchNewsToCategoryContainer(nsINNTPNewsgroup *newsInfo);
	nsINNTPNewsgroup *SwitchCategoryContainerToNews(nsINNTPCategoryContainer *catContainerInfo);

	char* m_hostname;
	PRInt32 m_port;

	char* m_nameAndPort;
	char* m_fullUIName;

	nsISupportsArray* m_groups;	// List of nsINNTPNewsgroup* objects.
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
	XP_HashTable m_searchableGroupCharsets;

	nsVoidArray m_propertiesForGet;
	nsVoidArray m_valuesForGet;

	PRBool m_postingAllowed;
	PRBool m_pushAuth; // PR_TRUE if we should volunteer authentication without a
						// challenge

	time_t m_lastGroupUpdate;
	time_t m_firstnewdate;


	nsMsgGroupRecord* m_groupTree; // Tree of groups we're remembering.
	PRBool m_inhaled;			// Whether we inhaled the entire list of
								// groups, or just some.
	int m_groupTreeDirty;		// Whether the group tree is dirty.  If 0, then
								// we don't need to write anything.  If 1, then
								// we can write things in place.  If >1, then
								// we need to rewrite the whole tree file.
	char* m_hostinfofilename;	// Filename of the hostinfo file.

	static nsNNTPHost* M_FileOwner; // In an effort to save file descriptors,
									  // only one newshost ever has its
									  // hostinfo file opened.  This is the
									  // one.

	XP_File m_groupFile;		// File handle to the hostinfo file.
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

nsNNTPHost * nsNNTPHost::M_FileOwner = NULL;

extern "C" {
	extern int MK_OUT_OF_MEMORY;
	extern int MK_UNABLE_TO_OPEN_NEWSRC;
	extern int MK_MIME_ERROR_WRITING_FILE;

	extern int MK_MSG_CANT_MOVE_FOLDER;
}

NS_IMPL_ISUPPORTS(nsNNTPHost, GetIID())


#ifdef HAVE_MASTER
nsNNTPHost::nsNNTPHost(MSG_Master* master, char* name,
                           PRInt32 port)
{
	m_master = master;
#else
nsNNTPHost::nsNNTPHost(const char *name, PRInt32 port)
{
#endif
	NS_INIT_REFCNT();
	m_hostname = new char [PL_strlen(name) + 1];
	PL_strcpy(m_hostname, name);

	PR_ASSERT(port);
	if (port == 0) port = NEWS_PORT;
	m_port = port;

	m_searchableGroupCharsets = XP_HashTableNew(20, XP_StringHash,
												(XP_HashCompFunction) PL_strcmp);

	m_nameAndPort = NULL;
	m_fullUIName = NULL;
	m_optionLines = NULL;
	m_filename = NULL;
	m_groups = NULL;
	m_dbfilename = NULL;
	m_dirty = 0;
	m_writetimer = NULL;
	m_postingAllowed = PR_TRUE;
	m_supportsExtensions = PR_FALSE;
	m_pushAuth = PR_FALSE;
	m_groupSucceeded = PR_FALSE;
	m_lastGroupUpdate = 0;
	m_groupTree = NULL;
}

/* we're not supposed to implement this */
 
nsNNTPHost::~nsNNTPHost()
{
}

nsresult
nsNNTPHost::CleanUp() { 
	if (m_dirty) WriteNewsrc();
	if (m_groupTreeDirty) SaveHostInfo();
	PR_FREEIF(m_optionLines);
	delete [] m_filename;
	delete [] m_hostname;
	PR_FREEIF(m_nameAndPort);
	PR_FREEIF(m_fullUIName);
	NS_IF_RELEASE(m_groups);
	delete [] m_dbfilename;
	delete m_groupTree;
	if (m_block)
		delete [] m_block;
#ifdef UNREADY_CODE
	if (m_groupFile)
		XP_FileClose (m_groupFile);
#endif
	PR_FREEIF(m_groupFilePermissions);
	if (M_FileOwner == this) M_FileOwner = NULL;
	PR_FREEIF(m_hostinfofilename);
	int i;
	for (i = 0; i < m_supportedExtensions.Count(); i++)
		PR_Free((char*) m_supportedExtensions[i]);
	for (i = 0; i < m_searchableGroups.Count(); i++)
		PR_Free((char*) m_searchableGroups[i]);
	for (i = 0; i < m_searchableHeaders.Count(); i++)
		PR_Free((char*) m_searchableHeaders[i]);
	for (i = 0; i < m_propertiesForGet.Count(); i++)
		PR_Free((char*) m_propertiesForGet[i]);
	for (i = 0; i < m_valuesForGet.Count(); i++)
		PR_Free((char*) m_valuesForGet[i]);
	if (m_searchableGroupCharsets)
	{
		// We do NOT free the individual key/value pairs,
		// because deleting m_searchableGroups above has
		// already caused this to happen.
		XP_HashTableDestroy(m_searchableGroupCharsets);
		m_searchableGroupCharsets = NULL;
	}
    return NS_OK;
}



void
nsNNTPHost::OpenGroupFile(const XP_FilePerm permissions)
{
#ifdef PROTOCOL_DEBUG


#endif
#ifdef UNREADY_CODE
	PR_ASSERT(permissions);
	if (!permissions) return;
	if (m_groupFile) {
		if (m_groupFilePermissions &&
			  PL_strcmp(m_groupFilePermissions, permissions) == 0) {
			return;
		}
		XP_FileClose(m_groupFile);
		m_groupFile = NULL;
	}
	if (M_FileOwner && M_FileOwner != this && M_FileOwner->m_groupFile) {
		XP_FileClose(M_FileOwner->m_groupFile);
		M_FileOwner->m_groupFile = NULL;
	}
	M_FileOwner = this;
	PR_FREEIF(m_groupFilePermissions);
	m_groupFilePermissions = PL_strdup(permissions);
	m_groupFile = XP_FileOpen(m_hostinfofilename, xpXoverCache, permissions);
#endif
}



void nsNNTPHost::ClearNew()
{
	m_firstnewdate = time(0) + 1;
}



void nsNNTPHost::dump()
{
	// ###tw  Write me...
}


PRInt32 nsNNTPHost::getPort()
{
	return m_port;
}

#if 0 
NS_IMETHODIMP
nsNNTPHost::GetLastUpdatedTime(PRInt64* aLastUpdatedTime)
{
  
  *aLastUpdatedTime = m_lastGroupUpdate;
  return NS_MSG_SUCCESS;
}

NS_IMETHODIMP
nsNNTPHost::setLastUpdate(PRInt64 aLastUpdatedTime)
{
	m_lastGroupUpdate = aLastUpdatedTime;
    return NS_MSG_SUCCESS;
}
#endif 
const char* nsNNTPHost::getNameAndPort()
{
	if (!m_nameAndPort) {
		if (m_port != (NEWS_PORT)) {
			m_nameAndPort = PR_smprintf("%s:%ld", m_hostname, long(m_port));
		} else {
			m_nameAndPort = PL_strdup(m_hostname);
		}
	}
	return m_nameAndPort;
}

const char* nsNNTPHost::getFullUIName()
{
	if (!m_fullUIName) {
		return getNameAndPort();
	}
	return m_fullUIName;
}

PRInt32
nsNNTPHost::RememberLine(char* line)
{
	char* new_data;
	if (m_optionLines) {
		new_data =
			(char *) PR_REALLOC(m_optionLines,
								PL_strlen(m_optionLines)
								+ PL_strlen(line) + 4);
	} else {
		new_data = (char *) PR_Malloc(PL_strlen(line) + 3);
	}
	if (!new_data) return MK_OUT_OF_MEMORY;
	PL_strcpy(new_data, line);
	PL_strcat(new_data, LINEBREAK);

	m_optionLines = new_data;

	return 0;

}


PRInt32
nsNNTPHost::ProcessLine_s(char* line, PRUint32 line_size, void* closure)
{
	return ((nsNNTPHost*) closure)->ProcessLine(line, line_size);
}


PRInt32
nsNNTPHost::ProcessLine(char* line, PRUint32 line_size)
{
	/* guard against blank line lossage */
	if (line[0] == '#' || line[0] == CR || line[0] == LF) return 0;

	line[line_size] = 0;

	if ((line[0] == 'o' || line[0] == 'O') &&
		!PL_strncasecmp (line, "options", 7)) {
		return RememberLine(line);
	}

	char *s;
	char *end = line + line_size;
	static nsNNTPArticleSet *set;
	
	for (s = line; s < end; s++)
		if (*s == ':' || *s == '!')
			break;
	
	if (*s == 0) {
		/* What is this?? Well, don't just throw it away... */
		return RememberLine(line);
	}

	set = nsNNTPArticleSet::Create(s + 1, this);
	if (!set) return MK_OUT_OF_MEMORY;

	PRBool subscribed = (*s == ':');
	*s = '\0';

	if (PL_strlen(line) == 0)
	{
		delete set;
		return 0;
	}

	nsINNTPNewsgroup* info=NULL;
    nsresult rv=NS_ERROR_NOT_INITIALIZED;
    
	if (subscribed && IsCategoryContainer(line))
	{
#ifdef HAVE_FOLDERINFO
		info = new nsINNTPCategoryContainer(line, set, subscribed,
												   this,
												   m_hostinfo->GetDepth() + 1);
#endif
		nsMsgGroupRecord* group = FindOrCreateGroup(line);
		// Go add all of our categories to the newsrc.
		AssureAllDescendentsLoaded(group);
		nsMsgGroupRecord* end = group->GetSiblingOrAncestorSibling();
		nsMsgGroupRecord* child;
		for (child = group->GetNextAlphabetic() ;
			 child != end ;
			 child = child->GetNextAlphabetic()) {
			PR_ASSERT(child);
			if (!child) break;
			char* fullname = child->GetFullName();
			if (!fullname) break;

            rv = FindGroup(fullname, &info);

			if (NS_FAILED(rv)) {
                // autosubscribe, if we haven't seen this one.
                char* groupLine = PR_smprintf("%s:", fullname);
                if (groupLine) {
                    ProcessLine(groupLine, PL_strlen(groupLine));
                    PR_Free(groupLine);
                }
            } else {
                    NS_RELEASE(info);
			}
			delete [] fullname;
		}
	}
	else {
        PRUint32 depth;
        rv = m_hostinfo->GetDepth(&depth);

#if 0                           // not defined yet
        if (NS_SUCCEEDED(rv))
            rv = NS_NewNewsgroup(&info, line, set, subscribed, this,
                                 depth+1);
#endif
    }

	if (NS_FAILED(rv) || !info) return MK_OUT_OF_MEMORY;

	// for now, you can't subscribe to category by itself.
    nsINNTPCategory *category;
    
	if (NS_SUCCEEDED(info->QueryInterface(nsINNTPCategory::GetIID(),
                                          (void **)&category))) {
        
        nsIMsgFolder *folder = getFolderFor(info);
        if (folder) {
            //            m_hostinfo->AddSubFolder(folder);
            m_hostinfo->AppendElement(folder);
            NS_RELEASE(folder);
        }
        
        NS_RELEASE(category);
	}

	m_groups->AppendElement(info);

	// prime the folder info from the folder cache while it's still around.
	// Except this might disable the update of new counts - check it out...
#ifdef HAVE_MASTER
	m_master->InitFolderFromCache (info);
#endif

	return 0;
}


nsresult nsNNTPHost::LoadNewsrc(/* nsIMsgFolder* hostinfo*/)
{
	char *ibuffer = 0;
	PRUint32 ibuffer_size = 0;
	PRUint32 ibuffer_fp = 0;

    /*	m_hostinfo = hostinfo; 
	PR_ASSERT(m_hostinfo);
	if (!m_hostinfo) return -1;
    */
#ifdef UNREADY_CODE
	int status = 0;

	PR_FREEIF(m_optionLines);

	if (!m_groups) {
		int size = 2048;
		char* buffer;
		buffer = new char[size];
		if (!buffer) return MK_OUT_OF_MEMORY;

		m_groups = NS_NewISupportsArray();
		if (!m_groups) {
			delete [] buffer;
			return MK_OUT_OF_MEMORY;
		}

        XP_File fid = XP_FileOpen(GetNewsrcFileName(),
                          xpNewsRC,
                          XP_FILE_READ_BIN);
        
		if (fid) {
			do {
				status = XP_FileRead(buffer, size, fid);
				if (status > 0) {
					msg_LineBuffer(buffer, status,
								   &ibuffer, &ibuffer_size, &ibuffer_fp,
								   PR_FALSE,
#ifdef XP_OS2
								   (PRInt32 (_Optlink*) (char*,PRUint32,void*))
#endif
								   nsNNTPHost::ProcessLine_s, this);
				}
			} while (status > 0);
			XP_FileClose(fid);
		}
		if (status == 0 && ibuffer_fp > 0) {
			status = ProcessLine(ibuffer, ibuffer_fp);
			ibuffer_fp = 0;
		}

		delete [] buffer;
		PR_FREEIF(ibuffer);

	}

	// build up the category tree for each category container so that roll-up 
	// of counts will work before category containers are opened.
	for (PRInt32 i = 0; i < m_groups->Count(); i++) {
        nsresult rv;
        
		nsINNTPNewsgroup *newsgroup = (nsINNTPNewsgroup *) (*m_groups)[i];
        
        nsINNTPCategoryContainer *catContainer;
        rv = newsgroup->QueryInterface(nsINNTPCategoryContainer::GetIID(),
                                       (void **)&catContainer);
		if (NS_SUCCEEDED(rv)) {
              
			char* groupname;
            nsresult rv = newsgroup->GetName(&groupname);
            
			nsMsgGroupRecord* group =
                m_groupTree->FindDescendant(groupname);
            
			PR_ASSERT(NS_SUCCEEDED(rv) && group);
            
			if (NS_SUCCEEDED(rv) && group) {
                nsIMsgFolder *folder = getFolderFor(newsgroup);
				folder->SetFlag(MSG_FOLDER_FLAG_ELIDED |
                                MSG_FOLDER_FLAG_DIRECTORY);
                NS_RELEASE(folder);
#ifdef HAVE_MASTER
				catContainer->BuildCategoryTree(catContainer, groupname, group,
					2, m_master);
#endif
			}
            NS_RELEASE(catContainer);
		}
	}

#endif

	return 0;
}


nsresult
nsNNTPHost::WriteNewsrc()
{
	if (!m_groups) return NS_ERROR_NOT_INITIALIZED;
#ifdef UNREADY_CODE
	PR_ASSERT(m_dirty);
	// Just to be sure.  It's safest to go ahead and write it out anyway,
	// even if we do somehow get called without the dirty bit set.

	XP_File fid=0;
    
    fid = XP_FileOpen(GetNewsrcFileName(), xpTemporaryNewsRC,
                      XP_FILE_WRITE_BIN);
    
	if (!fid) return MK_UNABLE_TO_OPEN_NEWSRC;
	
	int status = 0;

#ifdef XP_UNIX
	/* Clone the permissions of the "real" newsrc file into the temp file,
	   so that when we rename the finished temp file to the real file, the
	   preferences don't appear to change. */
	{
        XP_StatStruct st;

        if (XP_Stat (GetNewsrcFileName(), &st, xpNewsRC) == 0)
			/* Ignore errors; if it fails, bummer. */

		/* SCO doesn't define fchmod at all.  no big deal.
		   AIX3, however, defines it to take a char* as the first parameter.  The
		   man page sez it takes an int, though.  ... */
#if defined( SCO_SV ) || defined ( AIXV3 )
	  {
	    		char *really_tmp_file = WH_FileName(GetNewsrcFileName(), xpTemporaryNewsRC);

			chmod  (really_tmp_file, (st.st_mode | S_IRUSR | S_IWUSR));

			PR_Free(really_tmp_file);
	  }
#else
			fchmod (fileno(fid), (st.st_mode | S_IRUSR | S_IWUSR));
#endif
	}
#endif /* XP_UNIX */

	if (m_optionLines) {
		status = XP_FileWrite(m_optionLines, PL_strlen(m_optionLines), fid);
		if (status < int(PL_strlen(m_optionLines))) {
			status = MK_MIME_ERROR_WRITING_FILE;
		}
	}

	int n = m_groups->Count();
	for (int i=0 ; i<n && status >= 0 ; i++) {
        nsresult rv;
		nsINNTPNewsgroup* newsgroup = (nsINNTPNewsgroup*) ((*m_groups)[i]);
		// GetNewsFolderInfo will get root category for cat container.
#ifdef HAVE_FOLDERINFO
		char* str = newsgroup->GetNewsFolderInfo()->GetSet()->Output();
#else
        char *str = NULL;
#endif
		if (!str) {
			status = MK_OUT_OF_MEMORY;
			break;
		}
        char *newsgroupName=NULL;
        char *line;
        rv = newsgroup->GetName(&newsgroupName);
        
        PRBool isSubscribed=PR_FALSE;
        rv = newsgroup->GetSubscribed(&isSubscribed);
        line = PR_smprintf("%s%s %s" LINEBREAK,
                                 newsgroupName, 
								 isSubscribed ? ":" : "!", str);
		if (!line) {
			delete [] str;
			status = MK_OUT_OF_MEMORY;
			break;
		}
		int length = PL_strlen(line);
		status = XP_FileWrite(line, length, fid);
		if (status < length) {
			status = MK_MIME_ERROR_WRITING_FILE;
		}
		delete [] str;
		PR_Free(line);
	}
	  
	XP_FileClose(fid);
  
	if (status >= 0) {
        if (XP_FileRename(GetNewsrcFileName(), xpTemporaryNewsRC,
                          GetNewsrcFileName(), xpNewsRC) < 0)
            status = MK_MIME_ERROR_WRITING_FILE;
	}

	if (status < 0) {
        
		XP_FileRemove(GetNewsrcFileName(), xpTemporaryNewsRC);
		return status;
	}
	m_dirty = PR_FALSE;
	if (m_writetimer) {
#ifdef UNREADY_CODE
		FE_ClearTimeout(m_writetimer);
#endif
		m_writetimer = NULL;
	}

	if (m_groupTreeDirty) SaveHostInfo();

	return status;
#else
	return 0;
#endif
}


nsresult
nsNNTPHost::WriteIfDirty()
{
	if (m_dirty) return WriteNewsrc();
	return 0;
}


nsresult
nsNNTPHost::MarkDirty()
{
	m_dirty = PR_TRUE;
#ifdef UNREADY_CODE
	if (!m_writetimer)
		m_writetimer = FE_SetTimeout((TimeoutCallbackFunction)nsNNTPHost::WriteTimer, this,
								 5L * 60L * 1000L); // Hard-coded -- 5 minutes.
#endif
    return NS_OK;
}

void
nsNNTPHost::WriteTimer(void* closure)
{
	nsNNTPHost* host = (nsNNTPHost*) closure;
	host->m_writetimer = NULL;
	if (host->WriteNewsrc() < 0) {
		// ###tw  Pop up error message???
		host->MarkDirty();		// Cause us to try again. Or is this bad? ###tw
	}
}


nsresult
nsNNTPHost::SetNewsRCFilename(char* name)
{
	delete [] m_filename;
	m_filename = new char [PL_strlen(name) + 1];
	if (!m_filename) return MK_OUT_OF_MEMORY;
	PL_strcpy(m_filename, name);

	m_hostinfofilename = PR_smprintf("%s/hostinfo.dat", GetDBDirName());
	if (!m_hostinfofilename) return MK_OUT_OF_MEMORY;
#ifdef UNREADY_CODE
	XP_StatStruct st;
	if (XP_Stat (m_hostinfofilename, &st, xpXoverCache) == 0) {
		m_fileSize = st.st_size;
	}
	                               
	if (m_groupFile) {
		XP_FileClose(m_groupFile);
		m_groupFile = NULL;
	}

	OpenGroupFile();

	if (!m_groupFile) {
		// It does not exist.  Create an empty one.  But first, we go through
		// the whole directory and blow away anything that does not end in
		// ".snm".  This is to take care of people running B1 code, which
		// kept databases with such filenames.
		int lastcount = -1;
		int count = -1;
		do {
			XP_Dir dir = XP_OpenDir(GetDBDirName(), xpXoverCache);
			if (!dir) break;
			lastcount = count;
			count = 0;
			XP_DirEntryStruct *entry = NULL;
			for (entry = XP_ReadDir(dir); entry; entry = XP_ReadDir(dir)) {
				count++;
				const char* name = entry->d_name;
				int length = PL_strlen(name);
				if (name[0] == '.' || name[0] == '#' ||
					name[length - 1] == '~') continue;
				if (length < 4 ||
					PL_strcasecmp(name + length - 4, ".snm") != 0) {
					char* tmp = PR_smprintf("%s/%s", GetDBDirName(), name);
					if (tmp) {
						XP_FileRemove(tmp, xpXoverCache);
						PR_Free(tmp);
					}
				}
			}
			XP_CloseDir(dir);
		} while (lastcount != count); // Keep doing it over and over, until
									  // we get the same number of entries in
									  // the dir.  This is because I do not
									  // trust XP_ReadDir() to do the right
									  // thing if I delete files out from
									  // under it.

		// OK, now create the new empty hostinfo file.
		OpenGroupFile(XP_FILE_WRITE_BIN);
		PR_ASSERT(m_groupFile);
		if (!m_groupFile)
			return NS_ERROR_NOT_INITIALIZED;
		OpenGroupFile();
		PR_ASSERT(m_groupFile);
		if (!m_groupFile)
			return NS_ERROR_NOT_INITIALIZED;

		m_groupTreeDirty = 2;
	}

	m_blockSize = 10240;			// ###tw  This really ought to be the most
								// efficient file reading size for the current
								// operating system.

	m_block = NULL;
	while (!m_block && (m_blockSize >= 512))
	{
		m_block = new char[m_blockSize + 1];
		if (!m_block)
			m_blockSize /= 2;
	}
	if (!m_block)
		return MK_OUT_OF_MEMORY;

	m_groupTree = nsMsgGroupRecord::Create(NULL, NULL, 0, 0, 0);
	if (!m_groupTree)
		return MK_OUT_OF_MEMORY;

	return ReadInitialPart();
#else
	return 0;
#endif
}


int
nsNNTPHost::ReadInitialPart()
{
#ifdef UNREADY_CODE
	OpenGroupFile();
	if (!m_groupFile) return -1;

	PRInt32 length = XP_FileRead(m_block, m_blockSize, m_groupFile);
	if (length < 0) length = 0;
	m_block[length] = '\0';
	PRInt32 version = 0;

	char* ptr;
	char* endptr = NULL;
	for (ptr = m_block ; *ptr ; ptr = endptr + 1) {
		while (*ptr == CR || *ptr == LF) ptr++;
		endptr = PL_strchr(ptr, LINEBREAK_START);
		if (!endptr) break;
		*endptr = '\0';
		if (ptr[0] == '#' || ptr[0] == '\0') {
			continue;			// Skip blank lines and comments.
		}
		if (PL_strcmp(ptr, "begingroups") == 0) {
			m_fileStart = endptr - m_block;
			break;
		}
		char* ptr2 = PL_strchr(ptr, '=');
		if (!ptr2) continue;
		*ptr2++ = '\0';
		if (PL_strcmp(ptr, "lastgroupdate") == 0) {
			m_lastGroupUpdate = strtol(ptr2, NULL, 16);
		} else if (PL_strcmp(ptr, "firstnewdate") == 0) {
			m_firstnewdate = strtol(ptr2, NULL, 16);
		} else if (PL_strcmp(ptr, "uniqueid") == 0) {
			m_uniqueId = strtol(ptr2, NULL, 16);
		} else if (PL_strcmp(ptr, "pushauth") == 0) {
			m_pushAuth = strtol(ptr2, NULL, 16);
		} else if (PL_strcmp(ptr, "version") == 0) {
			version = strtol(ptr2, NULL, 16);
		}
	}
	m_block[0] = '\0';
	if (version != 1) {
		// The file got clobbered or damaged somehow.  Throw it away.
#ifdef DEBUG_bienvenu
		if (length > 0)
			PR_ASSERT(PR_FALSE);	// this really shouldn't happen, right?
#endif
		OpenGroupFile(XP_FILE_WRITE_BIN);
		PR_ASSERT(m_groupFile);
		if (!m_groupFile) return -1;
		OpenGroupFile();
		PR_ASSERT(m_groupFile);
		if (!m_groupFile) return -1;

		m_groupTreeDirty = 2;
		m_fileStart = 0;
		m_fileSize = 0;
	}
	m_groupTree->SetFileOffset(m_fileStart);
#endif
	return 0;
}

int
nsNNTPHost::CreateFileHeader()
{
	PR_snprintf(m_block, m_blockSize,
				"# Netscape newshost information file." LINEBREAK
				"# This is a generated file!  Do not edit." LINEBREAK
				"" LINEBREAK
				"version=1" LINEBREAK
				"newsrcname=%s" LINEBREAK
#ifdef OSF1
				"lastgroupdate=%08x" LINEBREAK
				"firstnewdate=%08x" LINEBREAK
				"uniqueid=%08x" LINEBREAK
#else
				"lastgroupdate=%08lx" LINEBREAK
				"firstnewdate=%08lx" LINEBREAK
				"uniqueid=%08lx" LINEBREAK
#endif
				"pushauth=%1x" LINEBREAK
				"" LINEBREAK
				"begingroups",
				m_filename, (long) m_lastGroupUpdate, (long) m_firstnewdate, (long) m_uniqueId,
				m_pushAuth);
	return PL_strlen(m_block);

}

int
nsNNTPHost::SaveHostInfo()
{                   
	int status = 0;
#ifdef UNREADY_CODE
	nsMsgGroupRecord* grec;
	XP_File in = NULL;
	XP_File out = NULL;
	char* blockcomma = NULL;
	char* ptrcomma = NULL;
	char* filename = NULL;
	int length = CreateFileHeader();
	PR_ASSERT(length < m_blockSize - 50);
	if (m_inhaled || length != m_fileStart) {
		m_groupTreeDirty = 2;
	}
	if (m_groupTreeDirty < 0) {
		// Uh, somehow we set the sign bit, probably some routine returned an
		// error status.  This will probably never happen.  But if it does,
		// we should just make sure to write things out in the most paranoid
		// fashion.
		m_groupTreeDirty = 2;
	}
	if (m_groupTreeDirty < 2) {
		// Only bits and pieces are dirty.  Write them out without writing the
		// whole file.
		OpenGroupFile();
		if (!m_groupFile) {
            status = MK_UNABLE_TO_OPEN_NEWSRC;
            goto FAIL;
		}
		XP_FileSeek(m_groupFile, 0, SEEK_SET);
		XP_FileWrite(m_block, length, m_groupFile);
		char* ptr = NULL;
		for (grec = m_groupTree->GetChildren();
			 grec;
			 grec = grec->GetNextAlphabetic()) {
			if (grec->IsDirty()) {
				if (grec->GetFileOffset() < m_fileStart) {
					m_groupTreeDirty = 2;
					break;
				}
				ptr = grec->GetSaveString();
				if (!ptr) {
					status = MK_OUT_OF_MEMORY;
					goto FAIL;
				}
				length = PL_strlen(ptr);
				if (length >= m_blockSize) {
					char* tmp = new char [length + 512];
					if (!tmp) {
						status = MK_OUT_OF_MEMORY;
						goto FAIL;
					}
					delete [] m_block;
					m_block = tmp;
					m_blockSize = length + 512;
				}
				// Read the old data, and make sure the old line was the
				// same length as the new one.  If not, set the dirty flag
				// to 2, so we end up writing the whole file, below.
				XP_FileSeek(m_groupFile, grec->GetFileOffset(), SEEK_SET);
				int l = XP_FileRead(m_block, length, m_groupFile);
				if (l != length || m_block[length - 1] != ptr[length - 1]) {
					m_groupTreeDirty = 2;
					break;
				}
				m_block[l] = '\0';
				char* p1 = PL_strchr(ptr, LINEBREAK_START);
				char* p2 = PL_strchr(m_block, LINEBREAK_START);
				PR_ASSERT(p1);
				if (!p1 || !p2 || (p1 - ptr) != (p2 - m_block)) {
					m_groupTreeDirty = 2;
					break;
				}
				PR_ASSERT(grec->GetFileOffset() > 100);	// header is at least 100 bytes long
				XP_FileSeek(m_groupFile, grec->GetFileOffset(), SEEK_SET);
				XP_FileWrite(ptr, PL_strlen(ptr), m_groupFile);
				PR_Free(ptr);
				ptr = NULL;
			}
		}
		PR_FREEIF(ptr);
	}
	if (m_groupTreeDirty >= 2) {
		// We need to rewrite the whole silly file.
		filename = PR_smprintf("%s/tempinfo", GetDBDirName());
		if (!filename) return MK_OUT_OF_MEMORY;
		PRInt32 position = 0;

		if (m_groupFile)
		{
			XP_FileClose(m_groupFile);
			m_groupFile = NULL;
		}

		out = XP_FileOpen(filename, xpXoverCache, XP_FILE_WRITE_BIN);
		if (!out) return MK_MIME_ERROR_WRITING_FILE;

		length = CreateFileHeader();	// someone probably has hosed m_block - so regen
		status = XP_FileWrite(m_block, length, out);
		if (status < 0) goto FAIL;
		position += length;
		m_fileStart = position;
		m_groupTree->SetFileOffset(m_fileStart);
		status = XP_FileWrite(LINEBREAK, LINEBREAK_LEN, out);
		if (status < 0) goto FAIL;
		position += LINEBREAK_LEN;

		blockcomma = NULL;
		if (!m_inhaled) {
			in = XP_FileOpen(m_hostinfofilename, xpXoverCache, XP_FILE_READ);
			if (!in) {
				status = MK_MIME_ERROR_WRITING_FILE;
				goto FAIL;
			}
			do {
				m_block[0] = '\0';
				XP_FileReadLine(m_block, m_blockSize, in);
			} while (m_block[0] &&
					 PL_strncmp(m_block, "begingroups", 11) != 0);
			m_block[0] = '\0';
			XP_FileReadLine(m_block, m_blockSize, in);
			blockcomma = PL_strchr(m_block, ',');
			if (blockcomma) *blockcomma = '\0';
		}
		for (grec = m_groupTree->GetChildren();
			 grec;
			 grec = grec->GetNextAlphabetic()) {
			char* ptr = grec->GetSaveString();
			if (!ptr) {
				status = MK_OUT_OF_MEMORY;
				goto FAIL;
			}
			ptrcomma = PL_strchr(ptr, ',');
			PR_ASSERT(ptrcomma);
			if (!ptrcomma) {
				status = -1;
				goto FAIL;
			}
			*ptrcomma = '\0';
			while (blockcomma &&
				   nsMsgGroupRecord::GroupNameCompare(m_block, ptr) < 0) {
				*blockcomma = ',';
				XP_StripLine(m_block);
				length = PL_strlen(m_block);
				status = XP_FileWrite(m_block, length, out);
				if (status < 0) goto FAIL;
				position += length;
				status = XP_FileWrite(LINEBREAK, LINEBREAK_LEN, out);
				if (status < 0) goto FAIL;
				position += LINEBREAK_LEN;
				m_block[0] = '\0';
				XP_FileReadLine(m_block, m_blockSize, in);
				blockcomma = PL_strchr(m_block, ',');
				if (blockcomma) *blockcomma = '\0';
			}
			if (blockcomma && PL_strcmp(m_block, ptr) == 0) {
				m_block[0] = '\0';
				XP_FileReadLine(m_block, m_blockSize, in);
				blockcomma = PL_strchr(m_block, ',');
				if (blockcomma) *blockcomma = '\0';
			}
			grec->SetFileOffset(position);
			*ptrcomma = ',';
			length = PL_strlen(ptr);
			// if the group doesn't exist on the server and has no children,
			// don't write it out.
			if (!grec->DoesNotExistOnServer()  || grec->GetNumKids() > 0)
			{
				status = XP_FileWrite(ptr, length, out);
				position += length;
			}
			else
				status = 0;
			PR_Free(ptr);
			if (status < 0) goto FAIL;
		}
		if (blockcomma) {
			*blockcomma = ',';
			do {
				XP_StripLine(m_block);
				length = PL_strlen(m_block);
				status = XP_FileWrite(m_block, length, out);
				if (status < 0) goto FAIL;
				position += length;
				status = XP_FileWrite(LINEBREAK, LINEBREAK_LEN, out);
				if (status < 0) goto FAIL;
				position += LINEBREAK_LEN;
				m_block[0] = '\0';
			} while (XP_FileReadLine(m_block, m_blockSize, in) && *m_block);
		}
		if (in) {
			XP_FileClose(in);
			in = NULL;
		}
		XP_FileClose(out);
		out = NULL;
		XP_FileRename(filename, xpXoverCache,
					  m_hostinfofilename, xpXoverCache);
		m_fileSize = position;
	}
	m_groupTreeDirty = 0;

FAIL:
	if (in) XP_FileClose(in);
	if (out) XP_FileClose(out);
	if (filename) PR_Free(filename);
	m_block[0] = '\0';
#endif
	return status;
}


struct InhaleState {
	nsMsgGroupRecord* tree;
	PRInt32 position;
	nsMsgGroupRecord* onlyIfChild;
	nsMsgGroupRecord* lastInhaled;
	char lastfullname[512];
};


PRInt32
nsNNTPHost::InhaleLine(char* line, PRUint32 length, void* closure)
{
	int status = 0;
	InhaleState* state = (InhaleState*) closure;
	PRInt32 position = state->position;
	state->position += length;
	char* lastdot;
	char* endptr = line + length;
	char* comma;
	for (comma = line ; comma < endptr ; comma++) {
		if (*comma == ',') break;
	}
	if (comma >= endptr) return 0;
	*comma = '\0';
	lastdot = PL_strrchr(line, '.');
	nsMsgGroupRecord* parent;
	nsMsgGroupRecord* child;
	if (lastdot) {
		*lastdot = '\0';
		// Try to find the parent of this line.  Very often, it will be
		// the last line we read, or some ancestor of that line.
		parent = NULL;
		if (state->lastInhaled && PL_strncmp(line, state->lastfullname,
											 lastdot - line) == 0) {
			char c = state->lastfullname[lastdot - line];
			if (c == '\0') parent = state->lastInhaled;
			else if (c == '.') {
				parent = state->lastInhaled;
				char* ptr = state->lastfullname + (lastdot - line);
				PR_ASSERT(parent);
				while (parent && ptr) {
					parent = parent->GetParent();
					PR_ASSERT(parent);
					ptr = PL_strchr(ptr + 1, '.');
				}
			}
		}


		if (!parent) parent = state->tree->FindDescendant(line);
		*lastdot = '.';
		PR_ASSERT(parent);
		if (!parent) {
			status = -1;
			goto DONE;
		}
	} else {
		parent = state->tree;
		lastdot = line - 1;
	}
	child = parent->FindDescendant(lastdot + 1);
	*comma = ',';
	if (child) {
		// It's already in memory.
		child->SetFileOffset(position);
	} else {
		child = nsMsgGroupRecord::Create(parent, line, endptr - line, position);
		if (!child) {
			status = MK_OUT_OF_MEMORY;
			goto DONE;
		}
	}
	if (state->onlyIfChild) {
		nsMsgGroupRecord* tmp;
		for (tmp = child; tmp ; tmp = tmp->GetParent()) {
			if (tmp == state->onlyIfChild) break;
		}
		if (tmp == NULL) status = -2;	// Indicates we're done.
	}
	PR_ASSERT(comma - line < sizeof(state->lastfullname));
	if ((comma - line)/sizeof(char) < sizeof(state->lastfullname)) {
		PL_strncpyz(state->lastfullname, line, comma - line + 1);
		state->lastfullname[comma - line] = '\0';
		state->lastInhaled = child;
	}
DONE:
	if (comma) *comma = ',';
	return status;
}


int
nsNNTPHost::Inhale(PRBool force)
{
#ifdef UNREADY_CODE
	if (m_groupTreeDirty) SaveHostInfo();
	if (force) {
		while (m_groupTree->GetChildren()) {
			delete m_groupTree->GetChildren();
		}
		m_inhaled = PR_FALSE;
	}
	PR_ASSERT(!m_inhaled);
	if (m_inhaled) return -1;
	int status = 0;
	OpenGroupFile();
	if (!m_groupFile) return -1;
	XP_FileSeek(m_groupFile, 0, SEEK_SET);
	status = ReadInitialPart();
	if (status < 0) return status;
	XP_FileSeek(m_groupFile, m_fileStart, SEEK_SET);
	InhaleState state;
	state.tree = m_groupTree;
	state.position = m_fileStart;
	state.onlyIfChild = NULL;
	state.lastInhaled = NULL;
	char* buffer = NULL;
	PRUint32 buffer_size = 0;
	PRUint32 buffer_fp = 0;
	do {
		status = XP_FileRead(m_block, m_blockSize, m_groupFile);
		if (status <= 0) break;
		status = msg_LineBuffer(m_block, status,
								&buffer, &buffer_size, &buffer_fp,
								PR_FALSE, 
#ifdef XP_OS2
								(PRInt32 (_Optlink*) (char*,PRUint32,void*))
#endif
								nsNNTPHost::InhaleLine, &state);
	} while (status >= 0);
	if (status >= 0 && buffer_fp > 0) {
		status = InhaleLine(buffer, buffer_fp, &state);
	}
	PR_FREEIF(buffer);
	m_block[0] = '\0';
	if (status >= 0) m_inhaled = PR_TRUE;
	return status;
#else
	return 0;
#endif
}


int
nsNNTPHost::Exhale()
{
	PR_ASSERT(m_inhaled);
	if (!m_inhaled) return -1;
	int status = SaveHostInfo();
	while (m_groupTree->GetChildren()) {
		delete m_groupTree->GetChildren();
	}
	m_inhaled = PR_FALSE;
	return status;
}


int
nsNNTPHost::EmptyInhale()
{
	PR_ASSERT(!m_inhaled);
	if (m_inhaled) return -1;
	while (m_groupTree->GetChildren()) {
		delete m_groupTree->GetChildren();
	}
	m_inhaled = PR_TRUE;
	return 0;
}


nsresult
nsNNTPHost::FindGroup(const char* name, nsINNTPNewsgroup* *retval)
{
    nsresult result = NS_ERROR_NOT_INITIALIZED;
    
	if (m_groups == NULL) return result;
	int n = m_groups->Count();
	for (int i=0 ; i<n ; i++) {
        char *newsgroupName;
        nsresult rv;
        
		nsINNTPNewsgroup* info = (nsINNTPNewsgroup*) (*m_groups)[i];
        rv = info->GetName(&newsgroupName);
        
		if (NS_SUCCEEDED(rv) &&
            PL_strcmp(newsgroupName, name) == 0) {
			*retval = info;
            result = NS_OK;
		}
	}
    return result;
}

nsresult
nsNNTPHost::AddGroup(const char *groupName,
                     nsINNTPNewsgroup **retval)
{
    return AddGroup(groupName, NULL, retval);
}


nsresult
nsNNTPHost::AddGroup(const char *groupName,
                     nsMsgGroupRecord *inGroupRecord,
                     nsINNTPNewsgroup **retval)
{
	nsINNTPNewsgroup *newsInfo = NULL;
	nsINNTPCategoryContainer *categoryContainer = NULL;
	char* containerName = NULL;
	PRBool needpaneupdate = PR_FALSE;
    PRBool isSubscribed=FALSE;
    
	nsMsgGroupRecord* group = (inGroupRecord) ? inGroupRecord : FindOrCreateGroup(groupName);
	if (!group) goto DONE;	// Out of memory.

	if (!group->IsCategoryContainer() && group->IsCategory()) {
		nsMsgGroupRecord *container = group->GetCategoryContainer();
		PR_ASSERT(container);
		if (!container) goto DONE;
		containerName = container->GetFullName();
		if (!containerName) goto DONE; // Out of memory.
        
        nsresult rv = FindGroup(containerName, &newsInfo);
        
		if (NS_SUCCEEDED(rv)) {
            rv = newsInfo->QueryInterface(nsINNTPCategoryContainer::GetIID(),
                                          (void **)&categoryContainer);
            
            // if we're not subscribed to container, do that instead.
            if (NS_SUCCEEDED(rv))
                rv = newsInfo->GetSubscribed(&isSubscribed);
            
            if (NS_FAILED(rv) || !isSubscribed)
                {
                    groupName = containerName;
                    group = FindOrCreateGroup(groupName);
                }
            NS_RELEASE(newsInfo);
            // categoryContainer released at the end of this function
            // (yuck)
        }
	}

	// no need to diddle folder pane for new categories.
	needpaneupdate = m_hostinfo && !group->IsCategory();

    nsresult rv;
    rv = FindGroup(groupName, &newsInfo);
	if (NS_SUCCEEDED(rv)) {	// seems to be already added
        PRBool subscribed;
        rv = newsInfo->GetSubscribed(&subscribed);
		if (NS_SUCCEEDED(rv) && !subscribed) {
			newsInfo->SetSubscribed(PR_TRUE);
            nsIMsgFolder *newsFolder = getFolderFor(newsInfo);
            //            m_hostinfo->AddSubfolderIfUnique(newsFolder);
            m_hostinfo->AppendElement(newsFolder);
            /*
            nsISupportsArray* infolist;
            nsresult rv = m_hostinfo->GetSubFolders(&infolist);
			// don't add it if it's already in the list!
			if (infolist->FindIndex(0, newsInfo) == -1)
				infolist->Add(newsInfo);
            */
		} else {
			goto DONE;
		}
	} else {
		char* groupLine = PR_smprintf("%s:", groupName);
		if (!groupLine) {
			goto DONE;			// Out of memory.
		}
	
		// this will add and auto-subscribe - OK, a cheap hack.
		if (ProcessLine(groupLine, PL_strlen(groupLine)) == 0) {
			// groups are added at end so look there first...
			newsInfo = (nsINNTPNewsgroup *)
				(*m_groups)[m_groups->Count() - 1];

            char *newsgroupName;
            rv = newsInfo->GetName(&newsgroupName);
			if (NS_SUCCEEDED(rv) &&
                PL_strcmp(newsgroupName, groupName)) {
				rv = FindGroup(groupName, &newsInfo);
				PR_ASSERT(NS_SUCCEEDED(rv));
			}
		}
		PR_Free(groupLine);
	}
	PR_ASSERT(newsInfo);
	if (!newsInfo) goto DONE;

	if (group->IsCategoryContainer()) {
		// Go add all of our categories to the newsrc.
		AssureAllDescendentsLoaded(group);
		nsMsgGroupRecord* end = group->GetSiblingOrAncestorSibling();
		nsMsgGroupRecord* child;
		for (child = group->GetNextAlphabetic() ;
			 child != end ;
			 child = child->GetNextAlphabetic()) {
			PR_ASSERT(child);
			if (!child) break;
			char* fullname = child->GetFullName();
			if (!fullname) break;
            
			nsINNTPNewsgroup* info;
            nsresult rv;
            
            rv = FindGroup(fullname, &info);
			if (NS_SUCCEEDED(rv)) {
                PRBool subscribed;
                rv = info->GetSubscribed(&subscribed);
				if (NS_SUCCEEDED(rv) && !subscribed) {
					info->SetSubscribed(PR_TRUE);
				}
			} else {
				char* groupLine = PR_smprintf("%s:", fullname);
				if (groupLine) {
					ProcessLine(groupLine, PL_strlen(groupLine));
					PR_Free(groupLine);
				}
			}
			delete [] fullname;
		}

        // XXX ACK. This is hairy code. Fix this.
        // solution: SwitchNewsToCategoryContainer should
        // return an nsresult
        nsINNTPCategoryContainer *catContainer;
        nsresult rv =
            newsInfo->QueryInterface(nsINNTPCategoryContainer::GetIID(),
                                     (void **)&catContainer);
		if (NS_FAILED(rv))
		{
			catContainer = SwitchNewsToCategoryContainer(newsInfo);
            if (catContainer) rv = NS_OK;
		}
		PR_ASSERT(NS_SUCCEEDED(rv));
		if (NS_SUCCEEDED(rv)) {
            // XXX should this call be on catContainer or newsInfo?
            nsIMsgFolder *folder = getFolderFor(newsInfo);
			folder->SetFlag(MSG_FOLDER_FLAG_ELIDED);
            NS_IF_RELEASE(folder);
#ifdef HAVE_MASTER
			catContainer->BuildCategoryTree(catContainer, groupName, group,
											);
#endif
            NS_RELEASE(catContainer);
		}
	}
	else if (group->IsCategory() && categoryContainer)
	{
#ifdef HAVE_MASTER
		categoryContainer->AddToCategoryTree(categoryContainer, groupName, group, m_master);
#endif
	}

#ifdef HAVE_MASTER
	if (needpaneupdate && newsInfo)
		m_master->BroadcastFolderAdded (newsInfo);
#endif
    
	MarkDirty();

DONE:

    NS_IF_RELEASE(categoryContainer);

	if (containerName) delete [] containerName;
    if (retval) *retval = newsInfo;
    return NS_OK;
}

nsINNTPCategoryContainer *
nsNNTPHost::SwitchNewsToCategoryContainer(nsINNTPNewsgroup *newsInfo)
{
    nsresult rv;
	int groupIndex = m_groups->IndexOf(newsInfo);
	if (groupIndex != -1)
	{
        // create a category container to hold this newsgroup
		nsINNTPCategoryContainer *newCatCont;
        // formerly newsInfo->CloneIntoCategoryContainer();
#if 0                           // not implemented yet
        NS_NewCategoryContainerFromNewsgroup(&newCatCont, newsInfo);
#endif
        
		// slip the category container where the newsInfo was.
		m_groups->ReplaceElementAt(newCatCont, groupIndex);

        nsIMsgFolder *newsFolder = getFolderFor(newsInfo);

        rv = newsInfo->QueryInterface(nsIMsgFolder::GetIID(),
                                      (void **)&newsFolder);
        if (newsFolder) {
            nsIMsgFolder *catContFolder = getFolderFor(newCatCont);
            if (catContFolder) {
                m_hostinfo->ReplaceElement(newsFolder, catContFolder);
                NS_RELEASE(catContFolder);
            }
            NS_RELEASE(newsFolder);
        }
        
        /*
        nsISupportsArray* infolist;
        nsresult rv = m_hostinfo->GetSubFolders(&infolist);
		// replace in folder pane server list as well.
		groupIndex = infoList->FindIndex(0, newsInfo);
		if (groupIndex != -1)
			infoList->SetAt(groupIndex, newCatCont);
        */
        return newCatCont;
	}
    // we used to just return newsInfo if groupIndex == -1
    // now we return NULL and caller has to check for that.
    return NULL;
}

nsINNTPNewsgroup *
nsNNTPHost::SwitchCategoryContainerToNews(nsINNTPCategoryContainer*
                                          catContainerInfo)
{
	nsINNTPNewsgroup *retInfo = NULL;

	int groupIndex = m_groups->IndexOf(catContainerInfo);
	if (groupIndex != -1)
	{
		nsINNTPNewsgroup *rootCategory;
        catContainerInfo->GetRootCategory(&rootCategory);
		// slip the root category container where the category container was.
		m_groups->ReplaceElementAt(rootCategory, groupIndex);

        nsIMsgFolder *catContFolder = getFolderFor(catContainerInfo);
        if (catContFolder) {
            nsIMsgFolder *rootFolder = getFolderFor(rootCategory);
            if (rootFolder) {
                m_hostinfo->ReplaceElement(catContFolder, rootFolder);
                NS_RELEASE(rootFolder);
            }
            NS_RELEASE(catContFolder);
        }
        /*
        nsISupportsArray* infoList;
        nsresult rv = m_hostinfo->GetSubFolders(&infoList);
		// replace in folder pane server list as well.
		groupIndex = infoList->FindIndex(0, catContainerInfo);
		if (groupIndex != -1)
			infoList->SetAt(groupIndex, rootCategory);
        */
		retInfo = rootCategory;
		// this effectively leaks the category container, but I don't think that's a problem
	}
	return retInfo;
}

nsresult
nsNNTPHost::RemoveGroup (nsINNTPNewsgroup *newsInfo)
{
    PRBool subscribed;
    if (!newsInfo) return NS_ERROR_NULL_POINTER;
    nsresult rv = ((nsINNTPNewsgroup *)newsInfo)->GetSubscribed(&subscribed);
	if (NS_SUCCEEDED(rv) && subscribed) 
	{
		((nsINNTPNewsgroup *)newsInfo)->SetSubscribed(PR_FALSE);
#ifdef HAVE_MASTER
		m_master->BroadcastFolderDeleted (newsInfo);
#endif
        nsIMsgFolder* newsFolder = getFolderFor((nsINNTPNewsgroup*)newsInfo);
        if (newsFolder) {
            m_hostinfo->RemoveElement(newsFolder);
            NS_RELEASE(newsFolder);
        }
        /*
        nsISupportsArray* infolist;
        nsresult rv = m_hostinfo->GetSubFolders(&infolist);
		infolist->Remove(newsInfo);
        */
	}
    return NS_OK;
}

nsresult
nsNNTPHost::RemoveGroupByName(const char* groupName)
{
	nsINNTPNewsgroup *newsInfo = NULL;

    nsresult rv = FindGroup(groupName, &newsInfo);
    if (NS_SUCCEEDED(rv))
        RemoveGroup (newsInfo);

    return NS_OK;
}


#ifdef DEBUG_terry
// #define XP_WIN16
#endif

const char*
nsNNTPHost::GetDBDirName()
{
	if (!m_filename) return NULL;
	if (!m_dbfilename) {
#if defined(XP_WIN16) || defined(XP_OS2)
		const int MAX_HOST_NAME_LEN = 8;
#elif defined(XP_MAC)
		const int MAX_HOST_NAME_LEN = 25;
#else
		const int MAX_HOST_NAME_LEN = 55;
#endif
		// Given a hostname, use either that hostname, if it fits on our
		// filesystem, or a hashified version of it, if the hostname is too
		// long to fit.
		char hashedname[MAX_HOST_NAME_LEN + 1];
		PRBool needshash = (PL_strlen(m_filename) > MAX_HOST_NAME_LEN);
#if defined(XP_WIN16)  || defined(XP_OS2)
		if (!needshash) {
			needshash = PL_strchr(m_filename, '.') != NULL ||
				PL_strchr(m_filename, ':') != NULL;
		}
#endif

		PL_strncpyz(hashedname, m_filename, MAX_HOST_NAME_LEN + 1);
		if (needshash) {
			PR_snprintf(hashedname + MAX_HOST_NAME_LEN - 8, 9, "%08lx",
						(unsigned long) XP_StringHash2(m_filename));
		}
		m_dbfilename = new char [PL_strlen(hashedname) + 15];
#if defined(XP_WIN16) || defined(XP_OS2)
		PL_strcpy(m_dbfilename, hashedname);
		PL_strcat(m_dbfilename, ".dir");
#else
		PL_strcpy(m_dbfilename, "host-");
		PL_strcat(m_dbfilename, hashedname);
		char* ptr = PL_strchr(m_dbfilename, ':');
		if (ptr) *ptr = '.'; // Windows doesn't like colons in filenames.
#endif

#ifdef UNREADY_CODE
		XP_MakeDirectory(m_dbfilename, xpXoverCache);
#endif
	}
	return m_dbfilename;
}


nsresult
nsNNTPHost::GetNumGroupsNeedingCounts(PRInt32 *value)
{
	if (!m_groups) return NS_ERROR_NOT_INITIALIZED;
	int num = m_groups->Count();
	PRInt32 result = 0;
	for (int i=0 ; i<num ; i++) {
		nsINNTPNewsgroup* info = (nsINNTPNewsgroup*) ((*m_groups)[i]);
        PRBool wantNewTotals, subscribed;
        nsresult rv;
        rv = info->GetWantNewTotals(&wantNewTotals);
        if (NS_SUCCEEDED(rv))
            rv = info->GetSubscribed(&subscribed);
		if (NS_SUCCEEDED(rv) &&
            wantNewTotals &&
            subscribed) {
			result++;
		}
	}
	*value = result;
    return NS_OK;
}

nsresult
nsNNTPHost::GetFirstGroupNeedingCounts(char **result)
{
	if (!m_groups) return NS_ERROR_NULL_POINTER;
	int num = m_groups->Count();
	for (int i=0 ; i<num ; i++) {
		nsINNTPNewsgroup* info = (nsINNTPNewsgroup*) ((*m_groups)[i]);
        
        PRBool wantNewTotals, subscribed;
        nsresult rv;
        rv = info->GetWantNewTotals(&wantNewTotals);
        if (NS_SUCCEEDED(rv))
            rv = info->GetSubscribed(&subscribed);
        
		if (NS_SUCCEEDED(rv) &&
            wantNewTotals &&
            subscribed) {
			info->SetWantNewTotals(PR_FALSE);
            char *newsgroupName;
            nsresult rv = info->GetName(&newsgroupName);
			if (NS_SUCCEEDED(rv)) {
                *result = PL_strdup(newsgroupName);
                return NS_OK;
            }
		}
	}
    *result = NULL;
	return NS_OK;
}


nsresult
nsNNTPHost::GetFirstGroupNeedingExtraInfo(char **result)
{
	nsMsgGroupRecord* grec;

    *result=NULL;
	for (grec = m_groupTree->GetChildren();	 grec; grec = grec->GetNextAlphabetic()) {
		if (grec && grec->NeedsExtraInfo()) {
			char *fullName = grec->GetFullName();
			*result = PL_strdup(fullName);
			delete [] fullName;
            return NS_OK;
		}
	}
	return NS_OK;
}



void
nsNNTPHost::SetWantNewTotals(PRBool value)
{
	if (!m_groups) return;
	int n = m_groups->Count();
	for (int i=0 ; i<n ; i++) {
		nsINNTPNewsgroup* info = (nsINNTPNewsgroup*) ((*m_groups)[i]);
		info->SetWantNewTotals(value);
	}
}



PRBool nsNNTPHost::NeedsExtension (const char * /*extension*/)
{
	//###phil need to flesh this out
	PRBool needed = m_supportsExtensions;
	return needed;
}

nsresult
nsNNTPHost::AddExtension (const char *ext)
{
    PRBool alreadyHasExtension=FALSE;
    QueryExtension(ext, &alreadyHasExtension);
	if (!alreadyHasExtension)
	{
		char *ourExt = PL_strdup (ext);
		if (ourExt)
			m_supportedExtensions.AppendElement(ourExt);
	}
    return NS_OK;
}

nsresult
nsNNTPHost::QueryExtension (const char *ext, PRBool *retval)
{
    *retval = PR_FALSE;
	for (int i = 0; i < m_supportedExtensions.Count(); i++)
		if (!PL_strcmp(ext, (char*) m_supportedExtensions[i])) {
			*retval=PR_TRUE;
            return NS_OK;
        }
	return NS_OK;
}

nsresult
nsNNTPHost::AddSearchableGroup (const char *group)
{
    PRBool searchableGroup;
    nsresult rv = QuerySearchableGroup(group, &searchableGroup);
	if (NS_SUCCEEDED(rv) && !searchableGroup)
	{
		char *ourGroup = PL_strdup (group);
		if (ourGroup)
		{
			// strip off character set spec 
			char *space = PL_strchr(ourGroup, ' ');
			if (space)
				*space = '\0';

			m_searchableGroups.AppendElement(ourGroup);

			space++; // walk over to the start of the charsets
			// Add the group -> charset association.
			XP_Puthash(m_searchableGroupCharsets, ourGroup, space);
		}
	}
    return NS_OK;
}

nsresult
nsNNTPHost::QuerySearchableGroup (const char *group, PRBool *_retval)
{
    *_retval = FALSE;
	for (int i = 0; i < m_searchableGroups.Count(); i++)
	{
		const char *searchableGroup = (const char*) m_searchableGroups[i];
		char *starInSearchableGroup = NULL;

		if (!PL_strcmp(searchableGroup, "*")) {
			*_retval = PR_TRUE; // everything is searchable
            return NS_OK;
        }
		else if (NULL != (starInSearchableGroup = PL_strchr(searchableGroup, '*')))
		{
			if (!PL_strncasecmp(group, searchableGroup, PL_strlen(searchableGroup)-2)) {
				*_retval = PR_TRUE; // this group is in a searchable hierarchy
                return NS_OK;
            }
		}
		else if (!PL_strcasecmp(group, searchableGroup)) {
            *_retval = PR_TRUE; // this group is individually searchable
            return NS_OK;
        }
	}
	return NS_OK;
}

// ### mwelch This should have been merged into one routine with QuerySearchableGroup,
//            but with two interfaces.
nsresult
nsNNTPHost::QuerySearchableGroupCharsets(const char *group, char **result)
{
	// Very similar to the above, but this time we look up charsets.
	PRBool gotGroup = PR_FALSE;
	const char *searchableGroup = NULL;
    *result = NULL;

	for (int i = 0; (i < m_searchableGroups.Count()) && (!gotGroup); i++)
	{
		searchableGroup = (const char*) m_searchableGroups[i];
		char *starInSearchableGroup = NULL;

		if (!PL_strcmp(searchableGroup, "*"))
			gotGroup = PR_TRUE; // everything is searchable
		else if (NULL != (starInSearchableGroup = PL_strchr(searchableGroup, '*')))
		{
			if (!PL_strncasecmp(group, searchableGroup, PL_strlen(searchableGroup)-2))
				gotGroup = PR_TRUE; // this group is in a searchable hierarchy
		}
		else if (!PL_strcasecmp(group, searchableGroup))
			gotGroup = PR_TRUE; // this group is individually searchable
	}

    if (gotGroup)
	{
		// Look up the searchable group for its supported charsets
		*result = (char *) XP_Gethash(m_searchableGroupCharsets, searchableGroup, NULL);
	}

	return NS_OK;
}

nsresult
nsNNTPHost::AddSearchableHeader (const char *header)
{
    PRBool searchable;
    nsresult rv = QuerySearchableHeader(header, &searchable);
	if (NS_SUCCEEDED(rv) && searchable)
	{
		char *ourHeader = PL_strdup(header);
		if (ourHeader)
			m_searchableHeaders.AppendElement(ourHeader);
	}
    return NS_OK;
}

nsresult
nsNNTPHost::QuerySearchableHeader(const char *header, PRBool *retval)
{
    *retval=PR_FALSE;
	for (int i = 0; i < m_searchableHeaders.Count(); i++)
		if (!PL_strncasecmp(header, (char*) m_searchableHeaders[i], PL_strlen(header))) {
			*retval = PR_TRUE;
            return NS_OK;
        }
	return NS_OK;
}

nsresult
nsNNTPHost::AddPropertyForGet (const char *property, const char *value)
{
	char *tmp = NULL;
	
	tmp = PL_strdup(property);
	if (tmp)
		m_propertiesForGet.AppendElement(tmp);

	tmp = PL_strdup(value);
	if (tmp)
		m_valuesForGet.AppendElement(tmp);

    // this is odd. do we need this return value?
    return NS_OK;
}

nsresult
nsNNTPHost::QueryPropertyForGet (const char *property, char **retval)
{
    *retval=NULL;
	for (int i = 0; i < m_propertiesForGet.Count(); i++)
		if (!PL_strcasecmp(property, (const char *) m_propertiesForGet[i])) {
            *retval = (char *)m_valuesForGet[i];
			return NS_OK;
        }
    
	return NS_OK;
}


nsresult
nsNNTPHost::SetPushAuth(PRBool value)
{
	if (m_pushAuth != value) 
	{
		m_pushAuth = value;
		m_groupTreeDirty |= 1;
	}
    return NS_OK;
}



const char*
nsNNTPHost::GetURLBase()
{
	if (!m_urlbase) {
		m_urlbase = PR_smprintf("%s://%s", "news",
								getNameAndPort());
	}
	return m_urlbase;
}




int nsNNTPHost::RemoveHost()
{
#ifdef UNREADY_CODE
	if (m_groupFile) {
		XP_FileClose(m_groupFile);
		m_groupFile = NULL;
	}
	m_dirty = 0;
	if (m_writetimer) {
#ifdef UNREADY_CODE
		FE_ClearTimeout(m_writetimer);
#endif
		m_writetimer = NULL;
	}

	// Tell netlib to close any connections we might have to this news host.
	NET_OnNewsHostDeleted (m_hostname);

	// Delete disk storage for summary files etc.
	// Use notifications through the master to prevent the folder pane 
	// from loading anything while we're deleting all the newsgroups.
#ifdef HAVE_MASTER
	m_master->BroadcastBeginDeletingHost (m_hostinfo);
	nsIMsgFolder *tree = m_master->GetFolderTree();
    rv = tree->PropagateDelete(&m_hostinfo, PR_TRUE /*deleteStorage*/);

	// Here's a little hack to work around the fact that the FE may be holding
	// a newsgroup open, and part of the delete might have failed. The WinFE does 
	// this when you delete a news server from the prefs
	if (NS_SUCCEEDED(rv) && m_hostinfo)
	{
		m_master->BroadcastFolderDeleted (m_hostinfo);
		tree->RemoveSubFolder (m_hostinfo);
	}
	m_master->BroadcastEndDeletingHost (m_hostinfo);

	m_master->GetHostTable()->RemoveEntry(this);
#endif
    // XXX And what are we supposed to be doing here? Do we keep
    // a reference to ourselves?
    // maybe NS_RELEASE(this); ?
#if 0
	delete this;
#endif
#endif

	return 0;
}



char*
nsNNTPHost::GetPrettyName(char* groupname)
{
	nsMsgGroupRecord* group = FindOrCreateGroup(groupname);
	if (group) {
		const char* orig = group->GetPrettyName();
		if (orig) {
			char* result = new char [PL_strlen(orig) + 1];
			if (result) {
				PL_strcpy(result, orig);
				return result;
			}
		}
	}
	return NULL;
}


nsresult
nsNNTPHost::SetPrettyName(const char* groupname, const char* prettyname)
{
	nsMsgGroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return MK_OUT_OF_MEMORY;
	nsresult rv = group->SetPrettyName(prettyname);
	if (NS_FAILED(rv))
	{
		nsINNTPNewsgroup *newsFolder;
        rv = FindGroup(groupname, &newsFolder);
		// make news folder forget prettyname so it will query again
		if (NS_SUCCEEDED(rv) && newsFolder)	
			newsFolder->SetPrettyName(NULL);
	}

    // this used to say
    // m_groupTreeDirty |= status;
    // where status came from the previous SetPrettyName
	if (NS_SUCCEEDED(rv)) m_groupTreeDirty |= 1;
	return rv;
}


time_t
nsNNTPHost::GetAddTime(char* groupname)
{
	nsMsgGroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return 0;
	return group->GetAddTime();
}


PRInt32
nsNNTPHost::GetUniqueID(char* groupname)
{
	nsMsgGroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return 0;
	return group->GetUniqueID();
}


PRBool
nsNNTPHost::IsCategory(char* groupname)
{
	nsMsgGroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return PR_FALSE;
	return group->IsCategory();
}


PRBool
nsNNTPHost::IsCategoryContainer(char* groupname)
{
	nsMsgGroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return PR_FALSE;
	return group->IsCategoryContainer();
}

int
nsNNTPHost::SetIsCategoryContainer(const char* groupname, PRBool value, nsMsgGroupRecord *inGroupRecord)
{
    nsresult rv;
	nsMsgGroupRecord* group = (inGroupRecord) ? inGroupRecord : FindOrCreateGroup(groupname);
	if (!group) return MK_OUT_OF_MEMORY;
	int status = group->SetIsCategoryContainer(value);
	m_groupTreeDirty |= status;
	if (status > 0)
	{
		nsINNTPNewsgroup *newsgroup;
        rv = FindGroup(groupname, &newsgroup);
        
        nsIMsgFolder *newsFolder = getFolderFor(newsgroup);

		// make news folder have correct category container flag
		if (NS_SUCCEEDED(rv) && newsgroup)	
		{
			if (value)
			{
                // change newsgroup into a category container
				newsFolder->SetFlag(MSG_FOLDER_FLAG_CAT_CONTAINER);
				SwitchNewsToCategoryContainer(newsgroup);
			}
			else
			{
                // change category container into a newsgroup
                nsINNTPCategoryContainer *catCont;
                rv = newsgroup->QueryInterface(nsINNTPCategoryContainer::GetIID(),
                                               (void **)&catCont);

				if (NS_SUCCEEDED(rv))
				{
                    // release the old newsgroup/newsfolder and get
                    // the converted one instead 
                    NS_IF_RELEASE(newsgroup);
					newsgroup = SwitchCategoryContainerToNews(catCont);
                    NS_RELEASE(catCont);

                    // make sure newsFolder is in sync with newsgroup
                    NS_IF_RELEASE(newsFolder);
                    newsFolder = getFolderFor(newsgroup);
                    
				}
                
				if (newsFolder)
				{
					newsFolder->ClearFlag(MSG_FOLDER_FLAG_CAT_CONTAINER);
					newsFolder->ClearFlag(MSG_FOLDER_FLAG_CATEGORY);
				}
			}
            NS_RELEASE(newsFolder);
            NS_RELEASE(newsgroup);
		}
	}
	return status;
}

nsresult
nsNNTPHost::SetGroupNeedsExtraInfo(const char *groupname, PRBool value)
{
	nsMsgGroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return MK_OUT_OF_MEMORY;
	nsresult rv = group->SetNeedsExtraInfo(value);
	if (NS_SUCCEEDED(rv)) m_groupTreeDirty |= 1;
	return rv;
}


char*
nsNNTPHost::GetCategoryContainer(const char* groupname, nsMsgGroupRecord *inGroupRecord)
{
	nsMsgGroupRecord* group = (inGroupRecord) ? inGroupRecord : FindOrCreateGroup(groupname);
	if (group) {
		group = group->GetCategoryContainer();
		if (group) return group->GetFullName();
	}
	return NULL;
}

nsINNTPNewsgroup *
nsNNTPHost::GetCategoryContainerFolderInfo(const char *groupname)
{
    nsINNTPNewsgroup *newsgroup=NULL;
    nsresult rv;
	// because GetCategoryContainer returns NULL for a category container...
	nsMsgGroupRecord *group = FindOrCreateGroup(groupname);
	if (group->IsCategoryContainer()) {
        rv = FindGroup(groupname, &newsgroup);
        if (NS_SUCCEEDED(rv)) return newsgroup;
        else return NULL;
    }

	char *categoryContainerName = GetCategoryContainer(groupname);
	if (categoryContainerName)
	{
		rv = FindGroup(categoryContainerName, &newsgroup);
		delete [] categoryContainerName;
	}
	return newsgroup;
}


nsresult
nsNNTPHost::GetIsVirtualGroup(const char* groupname, PRBool *retval)
{
	nsMsgGroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) {
        *retval = PR_FALSE;
        return NS_OK;
    }
	else
        return group->IsVirtual(retval);
}


nsresult
nsNNTPHost::SetIsVirtualGroup(const char* groupname, PRBool value)
{
    return SetIsVirtualGroup(groupname, value, nsnull);
}
nsresult
nsNNTPHost::SetIsVirtualGroup(const char* groupname, PRBool value,
                              nsMsgGroupRecord* inGroupRecord)
{
	nsMsgGroupRecord* group = (inGroupRecord) ? inGroupRecord : FindOrCreateGroup(groupname);

	if (!group) return NS_ERROR_OUT_OF_MEMORY;

	nsresult rv = group->SetIsVirtual(value);
	if (NS_SUCCEEDED(rv)) m_groupTreeDirty |= 1;
	return rv;
}



PRBool
nsNNTPHost::IsGroup(char* groupname)
{
	nsMsgGroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return PR_FALSE;
	return group->IsGroup();
}


int
nsNNTPHost::SetIsGroup(char* groupname, PRBool value)
{
	nsMsgGroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return MK_OUT_OF_MEMORY;
	int status = group->SetIsGroup(value);
	m_groupTreeDirty |= status;
	return status;
}


PRBool
nsNNTPHost::IsHTMLOk(char* groupname)
{
	nsMsgGroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return PR_FALSE;
	if (group->IsHTMLOKGroup()) return PR_TRUE;
	for ( ; group ; group = group->GetParent()) {
		if (group->IsHTMLOKTree()) return PR_TRUE;
	}
	return PR_FALSE;
}


PRBool
nsNNTPHost::IsHTMLOKGroup(char* groupname)
{
	nsMsgGroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return PR_FALSE;
	return group->IsHTMLOKGroup();
}


int
nsNNTPHost::SetIsHTMLOKGroup(char* groupname, PRBool value)
{
	nsMsgGroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return MK_OUT_OF_MEMORY;
	int status = group->SetIsHTMLOKGroup(value);
	m_groupTreeDirty |= status;
	return status;
}


PRBool
nsNNTPHost::IsHTMLOKTree(char* groupname)
{
	nsMsgGroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return PR_FALSE;
	return group->IsHTMLOKTree();
}


int
nsNNTPHost::SetIsHTMLOKTree(char* groupname, PRBool value)
{
	nsMsgGroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return MK_OUT_OF_MEMORY;
	int status = group->SetIsHTMLOKTree(value);
	m_groupTreeDirty |= status;
	return status;
}





nsMsgGroupRecord*
nsNNTPHost::FindGroupInBlock(nsMsgGroupRecord* parent,
							   char* groupname,
							   PRInt32* comp)
{
#ifdef UNREADY_CODE
	char* ptr;
	char* ptr2;
	char* tmp;
RESTART:
	ptr = m_block;
	*comp = 0;
	for (;;) {
		ptr = PL_strchr(ptr, LINEBREAK_START);
		ptr2 = ptr ? PL_strchr(ptr, ',') : 0;
		if (!ptr2) {
			if (*comp != 0) return NULL;
			// Yikes.  The whole string, and we can't even find a legitimate
			// beginning of a real line.  Grow the buffer until we can.

			if (m_fileSize - m_fileStart <= m_blockSize) {
				// Oh, well, maybe the file is just empty.  Fine.
				*comp = 0;
				return NULL;
			}

			if (PRInt32(PL_strlen(m_block)) < m_blockSize) goto RELOAD;
			goto GROWBUFFER;
		}
		while (*ptr == CR || *ptr == LF) ptr++;
		*ptr2 = '\0';
		PRInt32 c = nsMsgGroupRecord::GroupNameCompare(groupname, ptr);
		*ptr2 = ',';
		if (c < 0) {
			if (*comp > 0) {
				// This group isn't in the file, since earlier compares said
				// "look later" and this one says "look earlier".
				*comp = 0;
				return NULL;
			}
			*comp = c;
			return NULL;		// This group is somewhere before this chunk.
		}
		*comp = c;
		if (c == 0) {
			// Hey look, we found it.
			PRInt32 offset = m_blockStart + (ptr - m_block);
			ptr2 = PL_strchr(ptr2, LINEBREAK_START);
			if (!ptr2) {
				// What a pain.  We found the right place, but the rest of
				// the line is off the end of the buffer.
				if (offset > m_blockStart + LINEBREAK_LEN) {
					m_blockStart = offset - LINEBREAK_LEN;
					goto RELOAD;
				}
				// We couldn't find it, even though we're at the beginning
				// of the buffer.  This means that our buffer is too small.
				// Bleah.

				goto GROWBUFFER;
			}
			return nsMsgGroupRecord::Create(parent, ptr, -1, offset);
		}
	}
	/* NOTREACHED */
	return NULL;

GROWBUFFER:
			
	tmp = new char [m_blockSize + 512 + 1];
	if (!tmp) return NULL;
	delete [] m_block;
	m_block = tmp;
	m_blockSize += 512;
	
RELOAD:
	if (m_blockStart + m_blockSize > m_fileSize) {
		m_blockStart = m_fileSize - m_blockSize;
		if (m_blockStart < m_fileStart) m_blockStart = m_fileStart;
	}

	OpenGroupFile();
	if (!m_groupFile) return NULL;
	XP_FileSeek(m_groupFile, m_blockStart, SEEK_SET);
	int length = XP_FileRead(m_block, m_blockSize, m_groupFile);
	if (length < 0) length = 0;
	m_block[length] = '\0';
	goto RESTART;
#else
	return nsnull;
#endif
}


nsMsgGroupRecord*
nsNNTPHost::LoadSingleEntry(nsMsgGroupRecord* parent, char* groupname,
							  PRInt32 min, PRInt32 max)
{
#ifdef UNREADY_CODE
	OpenGroupFile();
	if (!m_groupFile) return NULL;

#ifdef DEBUG
	if (parent != m_groupTree) {
		char* pname = parent->GetFullName();
		if (pname) {
			PR_ASSERT(PL_strncmp(pname, groupname, PL_strlen(pname)) == 0);
			delete [] pname;
			pname = NULL;
		}
	}
#endif

	if (min < m_fileStart) min = m_fileStart;
	if (max < m_fileStart || max > m_fileSize) max = m_fileSize;

	nsMsgGroupRecord* result = NULL;
	PRInt32 comp = 1;

	// First, check if we happen to already have the line for this group in
	// memory.
	if (m_block[0]) {
		result = FindGroupInBlock(parent, groupname, &comp);
	}

	while (!result && comp != 0 && min < max) {
		PRInt32 mid = (min + max) / 2;
		m_blockStart = mid - LINEBREAK_LEN;
		XP_FileSeek(m_groupFile, m_blockStart, SEEK_SET);
		int length = XP_FileRead(m_block, m_blockSize, m_groupFile);
		if (length < 0) length = 0;
		m_block[length] = '\0';

		result = FindGroupInBlock(parent, groupname, &comp);

		if (comp > 0) {
			min = mid + 1;
		} else {
			max = mid;
		}
	}
	return result;
#else
	return 0;
#endif
}


nsMsgGroupRecord*
nsNNTPHost::FindOrCreateGroup(const char* groupname,
								int* statusOfMakingGroup)
{
	char buf[256];
	
	nsMsgGroupRecord* parent = m_groupTree;
	const char* start = groupname;
	PR_ASSERT(start && *start);
	if (!start || !*start) return NULL;

	PR_ASSERT(*start != '.');	// groupnames can't start with ".".
	if (*start == '.') return NULL;
	
	while (*start)
	{
		if (*start == '.') start++;
		const char* end = PL_strchr(start, '.');
		if (!end) end = start + PL_strlen(start);
		int length = end - start;
		PR_ASSERT(length > 0);	// groupnames can't contain ".." or end in
								// a ".".
		if (length <= 0) return NULL;
		PR_ASSERT(length < sizeof(buf));
		if ((unsigned int)length >= sizeof(buf)) return NULL;
		PL_strncpyz(buf, start, length + 1);
		buf[length] = '\0';
		
		nsMsgGroupRecord* prev = parent;
		nsMsgGroupRecord* ptr = NULL;
		int comp = 0;  // Initializing to zero.
		if (parent)
		{
			for (ptr = parent->GetChildren() ; ptr ; ptr = ptr->GetSibling()) 
			{
				comp = nsMsgGroupRecord::GroupNameCompare(ptr->GetPartName(), buf);
				if (comp >= 0) 
					break;
				prev = ptr;
			}
		}

		if (ptr == NULL || comp != 0) 
		{
			// We don't have this one in memory.  See if we can load it in.
			if (!m_inhaled && parent) 
			{
				if (ptr == NULL) 
				{
					ptr = parent->GetSiblingOrAncestorSibling();
				}
				length = end - groupname;
				char* tmp = new char[length + 1];
				if (!tmp) return NULL;
				PL_strncpyz(tmp, groupname, length + 1);
				tmp[length] = '\0';
				ptr = LoadSingleEntry(parent, tmp,
									  prev->GetFileOffset(),
									  ptr ? ptr->GetFileOffset() : m_fileSize);
				delete [] tmp;
				tmp = NULL;
			} 
			else 
			{
				ptr = NULL;
			}

			if (!ptr) 
			{
				m_groupTreeDirty = 2;
				ptr = nsMsgGroupRecord::Create(parent, buf, time(0),
											  m_uniqueId++, 0);
				if (!ptr) return NULL;
			}
		}
		parent = ptr;
		start = end;
	}
	int status = parent->SetIsGroup(PR_TRUE);
	m_groupTreeDirty |= status;
	if (statusOfMakingGroup) *statusOfMakingGroup = status;
	return parent;
}


int
nsNNTPHost::NoticeNewGroup(const char* groupname, nsMsgGroupRecord **outGroupRecord)
{
	int status = 0;
	nsMsgGroupRecord* group = FindOrCreateGroup(groupname, &status);
	if (!group) return MK_OUT_OF_MEMORY;
	if (outGroupRecord)
		*outGroupRecord = group;
	return status;
}


int
nsNNTPHost::AssureAllDescendentsLoaded(nsMsgGroupRecord* group)
{
#ifdef UNREADY_CODE
	int status = 0;
	PR_ASSERT(group);
	if (!group) return -1;
	if (group->IsDescendentsLoaded()) return 0;
	m_blockStart = group->GetFileOffset();
	PR_ASSERT(group->GetFileOffset() > 0);
	if (group->GetFileOffset() == 0) return -1;
	InhaleState state;
	state.tree = m_groupTree;
	state.position = m_blockStart;
	state.onlyIfChild = group;
	state.lastInhaled = NULL;
	OpenGroupFile();
	PR_ASSERT(m_groupFile);
	if (!m_groupFile) return -1;
	XP_FileSeek(m_groupFile, group->GetFileOffset(), SEEK_SET);
	char* buffer = NULL;
	PRUint32 buffer_size = 0;
	PRUint32 buffer_fp = 0;
	do {
		status = XP_FileRead(m_block, m_blockSize, m_groupFile);
		if (status <= 0) break;
		status = msg_LineBuffer(m_block, status,
								&buffer, &buffer_size, &buffer_fp,
								PR_FALSE, 
#ifdef XP_OS2
								(PRInt32 (_Optlink*) (char*,PRUint32,void*))
#endif
								nsNNTPHost::InhaleLine, &state);
	} while (status >= 0);
	if (status >= 0 && buffer_fp > 0) {
		status = InhaleLine(buffer, buffer_fp, &state);
	}
	if (status == -2) status = 0; // Special status meaning we ran out of
								  // lines that are children of the given
								  // group.
	
	PR_FREEIF(buffer);
	m_block[0] = '\0';

	if (status >= 0) group->SetIsDescendentsLoaded(PR_TRUE);
	return status;
#else
	return 0;
#endif
}

nsresult
nsNNTPHost::GroupNotFound(const char *groupName, PRBool opening)
{
	// if no group command has succeeded, don't blow away categories.
	// The server might be wedged...
	if (!opening && !m_groupSucceeded)
		return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;
	nsMsgGroupRecord* group = FindOrCreateGroup(groupName);
	if (group && (group->IsCategory() || opening))
	{
		nsINNTPNewsgroup *newsInfo = NULL;
        nsIMsgFolder *newsFolder = NULL;

        // get the group and corresponding folder
        rv = FindGroup(groupName, &newsInfo);
        if (NS_SUCCEEDED(rv))
            newsFolder = getFolderFor(newsInfo);
        
		group->SetDoesNotExistOnServer(PR_TRUE);
		m_groupTreeDirty |= 2;	// deleting a group has to force a rewrite anyway
		if (group->IsCategory())
		{
			nsINNTPNewsgroup *catCont = GetCategoryContainerFolderInfo(groupName);
			if (catCont)
			{
                nsIMsgFolder *catFolder = getFolderFor(catCont);
                if (catFolder) {

                    nsIMsgFolder* parentCategory;
                    rv = catFolder->FindParentOf(newsFolder,&parentCategory);
                    if (NS_SUCCEEDED(rv)) {
                        parentCategory->RemoveElement(newsFolder);
                        NS_RELEASE(parentCategory);
                    }
                    NS_RELEASE(catFolder);
                }
			}
		}
		else if (newsInfo)
#ifdef HAVE_MASTER
			m_master->BroadcastFolderDeleted (newsInfo);
#else
        ;
#endif

		if (newsInfo)
		{
            m_hostinfo->RemoveElement(newsFolder);
            /*
            nsISupportsArray* infolist;
            nsresult rv = m_hostinfo->GetSubFolders(&infolist);
			infolist->Remove(newsInfo);
            */
			m_groups->RemoveElement(newsInfo);
			m_dirty = PR_TRUE;
            NS_RELEASE(newsInfo);
            NS_IF_RELEASE(newsFolder);
		}
	}
    return NS_OK;
}


int nsNNTPHost::ReorderGroup(nsINNTPNewsgroup *groupToMove, nsINNTPNewsgroup *groupToMoveBefore, PRInt32 *newIdx)
{
	// Do the list maintenance to reorder a newsgroup. The news host has a list, and
	// so does the FolderInfo which represents the host in the hierarchy tree

	int err = MK_MSG_CANT_MOVE_FOLDER;
    nsIEnumerator *infoList=NULL;
	nsresult rv = m_hostinfo->GetSubFolders(&infoList);

	if (NS_SUCCEEDED(rv) && groupToMove && groupToMoveBefore && infoList)
	{
        nsIMsgFolder *folderToMoveBefore = getFolderFor(groupToMoveBefore);
		if (m_groups->RemoveElement(groupToMove)) 
		{
			// Not necessary to remove from infoList here because the folderPane does that (urk)

			// Unsubscribed groups are in the lists, but not in the view
			nsIMsgFolder *group = NULL;
			int idxInView, idxInData;
            int idxInHostInfo = 0;
            infoList->First();
			PRBool	foundIdxInHostInfo = PR_FALSE;

			for (idxInData = 0, idxInView = -1; idxInData < (PRInt32)m_groups->Count(); idxInData++)
			{
				group = (nsIMsgFolder*)(*m_groups)[idxInData];

                nsIMsgFolder *groupInHostInfo;
                (void)infoList->CurrentItem((nsISupports**)&groupInHostInfo);
#if 0
                nsISupports *hostInfoSupports;
                (void)infoList->CurrentItem(&hostInfoSupports);
                
				nsIMsgFolder *groupInHostInfo=NULL;
                rv = hostInfoSupports->QueryInterface(::nsISupports::GetIID(),
                                                      (void **)&groupInHostInfo);
#endif
#ifdef HAVE_PANE
				if (group->CanBeInFolderPane())
					idxInView++;
#endif
                /* XXX - pointer comparisons are bad in COM! */
				if (group == folderToMoveBefore)
					break;
				if (groupInHostInfo == folderToMoveBefore)
					foundIdxInHostInfo = PR_TRUE;
				else if (!foundIdxInHostInfo) {
					idxInHostInfo++;
                    infoList->Next();
                }
                NS_RELEASE(groupInHostInfo);
#if 0
                NS_RELEASE(hostInfoSupports);
#endif
			} 

			if (idxInView != -1) 
			{
				m_groups->InsertElementAt(groupToMove, idxInData); // the index has to be the same, right?
                nsISupports* groupSupports;
                groupToMove->QueryInterface(::nsISupports::GetIID(),
                                            (void **)&groupSupports);
//XXX				infoList->InsertElementAt(groupSupports, idxInHostInfo);
                NS_RELEASE(groupSupports);

				MarkDirty (); // Make sure the newsrc gets written out in the new order
				*newIdx = idxInView; // Tell the folder pane where the new item belongs

				err = 0;
                NS_RELEASE(groupSupports);
			}
		}
	}
	return err;
}


int nsNNTPHost::DeleteFiles ()
{
#ifdef UNREADY_CODE
	// hostinfo.dat
	PR_ASSERT(m_hostinfofilename);
	if (m_hostinfofilename)
		XP_FileRemove (m_hostinfofilename, xpXoverCache);

	// newsrc file
	const char *newsrc = GetNewsrcFileName();
	PR_ASSERT(newsrc);
	if (newsrc)
		XP_FileRemove (newsrc, xpNewsRC);

	// Delete directory
	const char *dbdirname = GetDBDirName();
	PR_ASSERT(dbdirname);
	if (dbdirname)
		return XP_RemoveDirectory (dbdirname, xpXoverCache);
#endif
	return 0;
}

/* given a message id like asl93490@ahost.com, convert
 * it to a newsgroup and a message number within that group
 * (stolen from msgglue.cpp - MSG_NewsGroupAndNumberOfID)
 */
nsresult
nsNNTPHost::GetNewsgroupAndNumberOfID(const char *message_id,
                                      nsINNTPNewsgroup **group,
                                      PRUint32 *messageNumber)
{
#ifdef HAVE_DBVIEW
    MessageDBView *view = pane->GetMsgView();
    if (!view || !view->GetDB())
        return NS_ERROR_NOT_INITIALIZED;
    nsMsgKey = view->GetDB()->GetMessageKeyForID(message_id);
    *messageNumber = (nsMsgKey == nsMsgKey_None) ? 0 : nsMsgKey;
#endif

    /* Why are we always choosing the current pane's folder? */
#ifdef HAVE_PANE
     MSG_FolderInfo *folderInfo = pane->GetFolder();
    if (folderInfo != NULL && folderInfo->IsNews())
    {
        MSG_FolderInfoNews *newsFolderInfo =
            (MSG_FolderInfoNews *) folderInfo;
        *groupName = newsFolderInfo->GetNewsgroupName();
    }
#endif
    return NS_OK;
}

/* this function originally lived in a pane
 */
nsresult
nsNNTPHost::AddNewNewsgroup(const char *groupName,
                            PRInt32 first,
                            PRInt32 last,
                            const char *flags,
                            PRBool xactiveFlags) {

    nsMsgGroupRecord     *groupRecord = NULL;
    nsresult rv;
    
    int status = NoticeNewGroup(groupName, &groupRecord);
    if (status < 0) return status;
    
    /* this used to be for the pane */
    /*    if (status > 0) m_numNewGroups++; */

    PRBool     bIsCategoryContainer = PR_FALSE;
    PRBool     bIsVirtual = PR_FALSE;

    while (flags && *flags)
    {
        char flag = toupper(*flags);
        flags++;
        switch (flag)
        {
            case 'C':
                bIsCategoryContainer = TRUE;
                break;
            case 'P':           // profile
            case 'V':
                bIsVirtual = TRUE;
                break;
            default:
                break;
        }
    }
    if (xactiveFlags)
    {
        SetIsCategoryContainer(groupName, bIsCategoryContainer, groupRecord);
        SetIsVirtualGroup(groupName, bIsVirtual, groupRecord);
    }

    if (status > 0) {
        // If this really is a new newsgroup, then if it's a category of a
        // subscribed newsgroup, then automatically subscribe to it.
        char* containerName = GetCategoryContainer(groupName, groupRecord);
        if (containerName) {
            nsINNTPNewsgroup* categoryInfo;
            rv = FindGroup(containerName, &categoryInfo);
            
            if (NS_SUCCEEDED(rv)) {
                PRBool isSubscribed;
                categoryInfo->GetSubscribed(&isSubscribed);
                if (isSubscribed) {
                    // this autosubscribes categories of subscribed newsgroups.
                    nsINNTPNewsgroup *newsgroup;
                    rv = AddGroup(groupName, groupRecord, &newsgroup);
                    if (NS_SUCCEEDED(rv)) NS_RELEASE(newsgroup);
                    
                }
                NS_RELEASE(categoryInfo);
            }
            delete [] containerName;
        }
    }

    if (status <=0) return NS_ERROR_UNEXPECTED;
    
    return NS_OK;
}

nsresult
nsNNTPHost::DisplaySubscribedGroup(const char *group,
                                   PRInt32 first_message,
                                   PRInt32 last_message,
                                   PRInt32 total_messages,
                                   PRBool visit_now)
{
    nsresult rv;
    nsINNTPNewsgroup *newsgroup=NULL;

    
    rv = FindGroup(group, &newsgroup);
    
    SetGroupSucceeded(TRUE);
    if (!newsgroup && visit_now) // let's try autosubscribe...
    {
        rv = AddGroup(group, NULL, &newsgroup);
    }

    if (!newsgroup)
        return NS_OK;
    else {
        PRBool subscribed;
        newsgroup->GetSubscribed(&subscribed);
        if (!subscribed)
            newsgroup->SetSubscribed(TRUE);
    }

    if (!newsgroup) return NS_OK;
    newsgroup->UpdateSummaryFromNNTPInfo(first_message, last_message,
                                    total_messages);
    return NS_OK;
}


#define MSG_IMPL_GETFOLDER(_type)\
nsIMsgFolder * \
nsNNTPHost::getFolderFor(_type * _class) {\
   nsIMsgFolder* folder;\
   nsresult rv = \
      _class->QueryInterface(nsIMsgFolder::GetIID(), (void **)&folder);\
   if (NS_SUCCEEDED(rv)) return folder;\
   else return nsnull;\
}\

MSG_IMPL_GETFOLDER(nsINNTPNewsgroup)
MSG_IMPL_GETFOLDER(nsINNTPCategoryContainer)

// this is just to make sure we can instantiate a news host
nsresult
NS_NewNNTPHost(nsINNTPHost **aNNTPHost, const char* name, PRUint32 port)
{
    nsresult rv;
    nsNNTPHost *aHost = new nsNNTPHost(name, port);
    if (aHost)
        return aHost->QueryInterface(nsINNTPHost::GetIID(),
                                     (void **)aNNTPHost);
    return NS_ERROR_NOT_INITIALIZED;

}
