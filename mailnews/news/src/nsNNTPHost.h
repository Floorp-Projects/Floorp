/* Insert copyright and license here 1996 */

#ifndef __msg_NewsHost__
#define __msg_NewsHost__ 1

#include "msghost.h"
#include "xp_hash.h"
#include "ptrarray.h"

class MSG_FolderInfo;
class MSG_FolderInfoNews;
class MSG_Master;
class msg_GroupRecord;
class MSG_FolderInfoCategoryContainer;

class MSG_NewsHost : public nsINNTPHost {
public:
	MSG_NewsHost(MSG_Master* master, const char* name, 
				 PRInt32 port);
	virtual ~MSG_NewsHost();

    virtual PRBool IsNews () { return PR_TRUE; }
	virtual MSG_NewsHost *GetNewsHost() { return this; }

	void addNew(MSG_GroupName* group);
	void ClearNew();

	virtual void dump();

	virtual PRInt32 getPort();

	time_t getLastUpdate();
	void setLastUpdate(time_t date);

	// Return the name of this newshost.
	const char* getStr();

	// Returns the name of this newshost, possibly followed by ":<port>" if
	// the port number for this newshost is not the default.
	const char* getNameAndPort();

    // Returns a fully descriptive name for this newshost, including the
    // above ":<port>" and also possibly a trailing (and localized) property
	virtual const char* getFullUIName();

	// Go load the newsrc for this host.  Creates the subscribed hosts as
	// children of the given MSG_FolderInfo.
	int LoadNewsrc(MSG_FolderInfo* hostinfo);

	// Get the MSG_FolderInfo which represents this host; the children
	// of this MSG_FolderInfo are the groups in this host.
	MSG_FolderInfo* GetHostInfo() {return m_hostinfo;}

	// Write out the newsrc for this host right now.  In general, either
	// MarkDirty() or WriteIfDirty() should be called instead.
	int WriteNewsrc();

	// Write out the newsrc for this host right now, if anything has changed
	// in it.
	int WriteIfDirty();

	// Note that something has changed, and we need to rewrite the newsrc file
	// for this host at some point.
	void MarkDirty();

	const char* GetNewsrcFileName();
	int SetNewsrcFileName(const char* name);

	MSG_Master* GetMaster() {
		return m_master;
	}

	MSG_FolderInfoNews* FindGroup(const char* name);

	MSG_FolderInfoNews *AddGroup(const char *groupName, msg_GroupRecord *inGroupRecord = NULL);
	void RemoveGroup(const char* groupName);
	void RemoveGroup(MSG_FolderInfoNews*);

	const char* GetDBDirName();	// Name of directory to store newsgroup
								// databases in.  This needs to have
								// "/groupname" appended to it, and the
								// whole thing can be passed to the XP_File
								// stuff with a type of xpXoverCache.

	// GetNumGroupsNeedingCounts() returns how many newsgroups we have
	// that we don't have an accurate count of unread/total articles.
	PRInt32 GetNumGroupsNeedingCounts();

	// GetFirstGroupNeedingCounts() returns the name of the first newsgroup
	// that does not have an accurate count of unread/total articles.  The
	// string must be free'd by the caller using PR_Free().
	char* GetFirstGroupNeedingCounts();
	

	// GetFirstGroupNeedingExtraInfo() returns the name of the first newsgroup
	// that does not have extra info (xactive flags and prettyname).  The
	// string must be free'd by the caller using PR_Free().
	char* GetFirstGroupNeedingExtraInfo();
	
	char** GetGroupList();		// Returns a list of newsgroups.  The result
								// must be free'd using PR_Free(); the
								// individual strings must not be free'd.

	void SetWantNewTotals(PRBool value); // Sets this bit on every newsgroup
										  // in this host.

	PRBool NeedsExtension (const char *ext);

	void AddExtension (const char *ext);
	PRBool QueryExtension (const char *ext);

	void AddSearchableGroup (const char *group);
	PRBool QuerySearchableGroup (const char *group);
	const char *QuerySearchableGroupCharsets(const char *group);

	void AddSearchableHeader (const char *header);
	PRBool QuerySearchableHeader (const char *header);

	void AddPropertyForGet (const char *property, const char *value);
	const char *QueryPropertyForGet (const char *property);


	void SetPostingAllowed (PRBool allowed) { m_postingAllowed = allowed; }
	PRBool GetPostingAllowed() { return m_postingAllowed; }

	void SetSupportsExtensions (PRBool isSupported)  
	{ m_supportsExtensions = isSupported; }
	PRBool GetSupportsExtensions () { return m_supportsExtensions; }

	PRBool GetPushAuth() { return m_pushAuth; }
	void SetPushAuth(PRBool pushAuth);

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
	char* GetPrettyName(const char* groupname);
	int SetPrettyName(const char* groupname, const char* prettyname);


	time_t GetAddTime(const char* groupname);

	// Returns a unique integer associated with this newsgroup.  This is
	// mostly used by Win16 to generate a 8.3 filename.
	PRInt32 GetUniqueID(const char* groupname);

	PRBool IsCategory(const char* groupname);
	PRBool IsCategoryContainer(const char* groupname);
	int SetIsCategoryContainer(const char* groupname, PRBool value, msg_GroupRecord *inGroupRecord = NULL);
	
	int SetGroupNeedsExtraInfo(const char *groupname, PRBool value);
	// Finds the container newsgroup for this category (or NULL if this isn't
	// a category).  The resulting string must be free'd using delete[].
	char* GetCategoryContainer(const char* groupname, msg_GroupRecord *inGroupRecord = NULL);
	MSG_FolderInfoNews *GetCategoryContainerFolderInfo(const char *groupname);

	// Get/Set whether this is a virtual newsgroup.
	PRBool IsProfile(const char* groupname);
	int SetIsProfile(const char* groupname, PRBool value, msg_GroupRecord *inGroupRecord = NULL);

	// Get/Set whether this is a real group (as opposed to a container of
	// other groups, like "mcom".)
	PRBool IsGroup(const char* groupname);
	int SetIsGroup(const char* groupname, PRBool value);
	

	// Returns PR_TRUE if it's OK to post HTML in this group (either because the
	// bit is on for this group, or one of this group's ancestor's has marked
	// all of its descendents as being OK for HTML.)
	PRBool IsHTMLOk(const char* groupname);

	// Get/Set if it's OK to post HTML in just this group.
	PRBool IsHTMLOKGroup(const char* groupname);
	int SetIsHTMLOKGroup(const char* groupname, PRBool value);

	// Get/Set if it's OK to post HTML in this group and all of its subgroups.
	PRBool IsHTMLOKTree(const char* groupname);
	int SetIsHTMLOKTree(const char* groupname, PRBool value);

	// Create the given group (if not already present).  Returns 0 if the
	// group is already present, 1 if we had to create it, negative on error.
	// The given group will have the "isgroup" bit set on it (in other words,
	// it is not to be just a container of other groups, like "mcom" is.)
	int NoticeNewGroup(const char* groupname, msg_GroupRecord **outGroupRecord = NULL);
	

	// Makes sure that we have records in memory for all known descendants
	// of the given newsgroup.
	int AssureAllDescendentsLoaded(msg_GroupRecord* group);


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

	msg_GroupRecord* GetGroupTree() {return m_groupTree;}
	time_t GetFirstNewDate() {return m_firstnewdate;}
	
	void	GroupNotFound(const char *groupName, PRBool opening);

	int ReorderGroup (MSG_FolderInfoNews *groupToMove, MSG_FolderInfoNews *groupToMoveBefore, PRInt32 *newIdx);

protected:
	void OpenGroupFile(const XP_FilePerm permissions = XP_FILE_UPDATE_BIN);
	PRInt32 RememberLine(char* line);
	static PRInt32 ProcessLine_s(char* line, PRUint32 line_size, void* closure);
	PRInt32 ProcessLine(char* line, PRUint32 line_size);
	static void WriteTimer(void* closure);
	int CreateFileHeader();
	int ReadInitialPart();
	msg_GroupRecord* FindGroupInBlock(msg_GroupRecord* parent,
									  const char* groupname,
									  PRInt32* comp);
	msg_GroupRecord* LoadSingleEntry(msg_GroupRecord* parent,
									 const char* groupname,
									 PRInt32 min, PRInt32 max);
	static PRInt32 InhaleLine(char* line, PRUint32 length, void* closure);
	msg_GroupRecord* FindOrCreateGroup(const char* groupname,
									   int* statusOfMakingGroup = NULL);	

	MSG_FolderInfoNews *SwitchNewsToCategoryContainer(MSG_FolderInfoNews *newsInfo);
	MSG_FolderInfoNews *SwitchCategoryContainerToNews(MSG_FolderInfoCategoryContainer *catContainerInfo);

	char* m_hostname;
	PRInt32 m_port;

	char* m_nameAndPort;
	char* m_fullUIName;

	MSG_FolderArray* m_groups;	// List of MSG_FolderInfoNews* objects.
	MSG_Master* m_master;

	MSG_FolderInfo* m_hostinfo;	// Object representing entire newshost in
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

	XPPtrArray m_supportedExtensions;
	XPPtrArray m_searchableGroups;
	XPPtrArray m_searchableHeaders;
	// ### mwelch Added to determine what charsets can be used
	//            for each table.
	XP_HashTable m_searchableGroupCharsets;

	XPPtrArray m_propertiesForGet;
	XPPtrArray m_valuesForGet;

	PRBool m_postingAllowed;
	PRBool m_pushAuth; // PR_TRUE if we should volunteer authentication without a
						// challenge

	time_t m_lastgroupdate;
	time_t m_firstnewdate;


	msg_GroupRecord* m_groupTree; // Tree of groups we're remembering.
	PRBool m_inhaled;			// Whether we inhaled the entire list of
								// groups, or just some.
	int m_groupTreeDirty;		// Whether the group tree is dirty.  If 0, then
								// we don't need to write anything.  If 1, then
								// we can write things in place.  If >1, then
								// we need to rewrite the whole tree file.
	char* m_hostinfofilename;	// Filename of the hostinfo file.

	static MSG_NewsHost* M_FileOwner; // In an effort to save file descriptors,
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



#endif /* __msg_NewsHost__ */
