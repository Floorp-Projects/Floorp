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

 /* This is a stub event sink for a NNTP Host introduced by mscott to test
   the NNTP protocol */

#include "nsINNTPHost.h"
#include "nscore.h"
#include "plstr.h"
#include "prmem.h"
#include "nsVoidArray.h"
#include <stdio.h>

#include "nsINNTPNewsgroup.h"

#include "nsMsgGroupRecord.h"
#include "nsIMsgFolder.h"
#include "nsNNTPNewsgroupList.h"
#include "nsNNTPNewsgroup.h"

static NS_DEFINE_IID(kINNTPHostIID, NS_INNTPHOST_IID);
static NS_DEFINE_IID(kINNTPNewsgroupIID, NS_INNTPNEWSGROUP_IID);

class nsNNTPHostStub : public nsINNTPHost 
{
public:
    nsNNTPHostStub(const char *name, PRInt32 port);
               
	virtual ~nsNNTPHostStub();

	// nsISupports
    NS_DECL_ISUPPORTS
    
	// nsINNTPHost

    NS_IMETHOD	GetSupportsExtensions(PRBool *aSupportsExtensions);
	NS_IMETHOD  SetSupportsExtensions(PRBool aSupportsExtensions);
    
	NS_IMETHOD AddExtension (const char *ext);
	NS_IMETHOD QueryExtension (const char *ext, PRBool *_retval);
    
	NS_IMETHOD GetPostingAllowed(PRBool * aPostingAllowed);
	NS_IMETHOD SetPostingAllowed(PRBool aPostingAllowed);

	NS_IMETHOD GetPushAuth(PRBool *aPushAuth);
	NS_IMETHOD SetPushAuth(PRBool aPushAuth);
    
    NS_IMPL_CLASS_GETSET(LastUpdatedTime, PRInt64, m_lastGroupUpdate);

    NS_IMETHOD GetNewsgroupList(const char *groupname, nsINNTPNewsgroupList **_retval);

    NS_IMETHOD GetNewsgroupAndNumberOfID(const char *message_id,
                                         nsINNTPNewsgroup **group,
                                         PRUint32 *messageNumber);

    /* get this from MSG_Master::FindNewsFolder */
    NS_IMETHOD FindNewsgroup(const char *groupname, PRBool create,
		nsINNTPNewsgroup **_retval) { return FindGroup(groupname, _retval);}
    
	NS_IMETHOD AddPropertyForGet (const char *property, const char *value);
	NS_IMETHOD QueryPropertyForGet (const char *property, char **_retval);

    NS_IMETHOD AddSearchableGroup(const char *groupname);
    // should these go into interfaces?
	NS_IMETHOD QuerySearchableGroup (const char *group, PRBool *);
    //NS_IMETHOD QuerySearchableGroupCharsets(const char *group, char **);

    // Virtual groups
    NS_IMETHOD AddVirtualGroup(const char *responseText) { return NS_OK;}
    NS_IMETHOD SetIsVirtualGroup(const char *groupname, PRBool isVirtual);
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
    
   
    NS_IMETHOD FindGroup(const char* name, nsINNTPNewsgroup* *_retval);
    NS_IMETHOD AddGroup(const char *groupname,
                        nsINNTPNewsgroup **retval);
    
    //NS_IMETHOD AddGroup(const char *groupname,
//                        nsMsgGroupRecord *groupRecord,
//                        nsINNTPNewsgroup **retval);
    
    NS_IMETHOD RemoveGroupByName(const char *groupName);
    NS_IMETHOD RemoveGroup(const nsINNTPNewsgroup*);
    
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
    NS_IMETHOD GetDbDirName(char * *aDbDirName);

    /* Returns a list of newsgroups.  The result
       must be free'd using PR_Free(); the
       individual strings must not be free'd. */
    NS_IMETHOD GetGroupList(char **_retval);

    NS_IMETHOD DisplaySubscribedGroup(const char *groupname,
                                      PRInt32 first_message,
                                      PRInt32 last_message,
                                      PRInt32 total_messages,
                                      PRBool visit_now);
    // end of nsINNTPHost
    
private:
    virtual PRBool IsNews () { return PR_TRUE; }
	virtual nsINNTPHost *GetNewsHost() { return this; }

 	// Get the nsIMsgFolder which represents this host; the children
	// of this nsIMsgFolder are the groups in this host.
	nsIMsgFolder* GetHostInfo() {return m_hostinfo;}

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
	
	// Returns the pretty name for the given group.  The resulting string
	// must be free'd using delete[].
	char* GetPrettyName(const char* groupname) { return m_prettyName;}
	NS_IMETHOD SetPrettyName(const char* groupname, const char* prettyname);

	NS_IMETHOD SetGroupNeedsExtraInfo(const char *groupname, PRBool value);
	// Finds the container newsgroup for this category (or NULL if this isn't
	// a category).  The resulting string must be free'd using delete[].
	char* GetCategoryContainer(const char* groupname, nsMsgGroupRecord *inGroupRecord = NULL);
	nsINNTPNewsgroup *GetCategoryContainerFolderInfo(const char *groupname);


	nsMsgGroupRecord* GetGroupTree() {return m_groupTree;}
	
	NS_IMETHOD GroupNotFound(const char *groupName, PRBool opening);

protected:

	char* m_hostname;
	char * m_prettyName;
	PRInt32 m_port;

	nsISupportsArray *m_groups;	// List of nsINNTPNewsgroup* objects.

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
	nsVoidArray m_propertiesForGet;
	nsVoidArray m_valuesForGet;

	PRBool m_postingAllowed;
	PRBool m_pushAuth; // PR_TRUE if we should volunteer authentication without a
						// challenge

	nsMsgGroupRecord* m_groupTree; // Tree of groups we're remembering.
	PRInt64 m_lastGroupUpdate;
	PRInt32 m_numCallsToGetCounts;
	
};

NS_IMPL_ISUPPORTS(nsNNTPHostStub, kINNTPHostIID);

nsNNTPHostStub::nsNNTPHostStub(const char * name, PRInt32 port)
{
	NS_INIT_REFCNT();
	m_supportsExtensions = PR_FALSE;
	m_postingAllowed = PR_FALSE;
	m_lastGroupUpdate = 0;
	m_numCallsToGetCounts = 0;
	NS_NewISupportsArray(&m_groups);
}

nsNNTPHostStub::~nsNNTPHostStub()
{
}

nsresult nsNNTPHostStub::GetSupportsExtensions(PRBool *aSupportsExtensions)
{
	printf("Supports Extensions: %s. \n", m_supportsExtensions ? "TRUE" : "FALSE");
	if (aSupportsExtensions)
		*aSupportsExtensions = m_supportsExtensions;
	return NS_OK;
}

nsresult nsNNTPHostStub::SetSupportsExtensions(PRBool aSupportsExtensions)
{
	m_supportsExtensions = aSupportsExtensions;
	printf("Setting supports extensions to: %s. \n", m_supportsExtensions ? "TRUE" : "FALSE");
	return NS_OK;
}


nsresult nsNNTPHostStub::AddExtension(const char * extension)
{
	printf("Adding extension: %s. \n", extension ? extension : "invalid extension");
	return NS_OK;
}

nsresult nsNNTPHostStub::QueryExtension(const char * extension, PRBool * aRetValue)
{
	printf("Querying extension %s. \n", extension ? extension : "invalid extension");
	if (PL_strstr(extension, "SEARCH"))
		*aRetValue = PR_TRUE;
	else
		*aRetValue = PR_FALSE;
	return NS_OK;
}

nsresult nsNNTPHostStub::GetPostingAllowed(PRBool * aPostingAllowed)
{
	printf("Posting Allowed: %s. \n", m_postingAllowed ? "TRUE" : "FALSE");
	if (aPostingAllowed)
		*aPostingAllowed = m_postingAllowed;
	return NS_OK;
}

nsresult nsNNTPHostStub::SetPostingAllowed(PRBool aPostingAllowed)
{
	m_postingAllowed = aPostingAllowed;
	printf("Setting posting allowed to: %s. \n", m_postingAllowed ? "TRUE" : "FALSE");
	return NS_OK;
}

nsresult nsNNTPHostStub::GetPushAuth(PRBool * aPushAuth)
{
	printf("Push Authentication: %s. \n", m_pushAuth ? "TRUE" : "FALSE");
	if (aPushAuth)
		*aPushAuth = m_pushAuth;
	return NS_OK;
}

nsresult nsNNTPHostStub::SetPushAuth(PRBool aPushAuth)
{
	m_pushAuth = aPushAuth;
	printf("Setting push auth to: %s. \n", m_pushAuth ? "TRUE" : "FALSE");
	return NS_OK;
}


nsresult nsNNTPHostStub::AddPropertyForGet (const char *property, const char *value)
{
	char *tmp = NULL;
	
	tmp = PL_strdup(property);
	if (tmp)
		m_propertiesForGet.AppendElement(tmp);

	tmp = PL_strdup(value);
	if (tmp)
		m_valuesForGet.AppendElement(tmp);
	
	printf("Adding property %s for value %s. \n", property, value);

    // this is odd. do we need this return value?
    return NS_OK;
}

nsresult nsNNTPHostStub::QueryPropertyForGet (const char *property, char **retval)
{
    *retval=NULL;
	for (int i = 0; i < m_propertiesForGet.Count(); i++)
		if (!PL_strcasecmp(property, (const char *) m_propertiesForGet[i])) {
            *retval = (char *)m_valuesForGet[i];
			printf("Retrieving property %s for get. \n", *retval);
			return NS_OK;
        }
    
	return NS_OK;
}

nsresult nsNNTPHostStub::AddSearchableGroup (const char *group)
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
			printf("Adding %s to list of searchable groups. \n", ourGroup);

			space++; // walk over to the start of the charsets
			// Add the group -> charset association.
//			XP_Puthash(m_searchableGroupCharsets, ourGroup, space);
		}
	}
    return NS_OK;
}

nsresult nsNNTPHostStub::QuerySearchableGroup (const char *group, PRBool *_retval)
{
    *_retval = PR_FALSE;
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

nsresult nsNNTPHostStub::SetIsVirtualGroup(const char * groupname, PRBool isVirtual)
{
	return NS_OK;
}

nsresult nsNNTPHostStub::GetIsVirtualGroup(const char * groupname, PRBool  * isVirtual)
{
	*isVirtual = PR_FALSE;
	return NS_OK;
}

nsresult nsNNTPHostStub::AddSearchableHeader (const char *header)
{
    PRBool searchable;
    nsresult rv = QuerySearchableHeader(header, &searchable);
	if (NS_SUCCEEDED(rv) && searchable)
	{
		char *ourHeader = PL_strdup(header);
		if (ourHeader)
			m_searchableHeaders.AppendElement(ourHeader);

		printf("Added %s as a searchable header. \n", ourHeader);
	}
    return NS_OK;
}

nsresult nsNNTPHostStub::QuerySearchableHeader(const char *header, PRBool *retval)
{
    *retval=PR_FALSE;
	for (int i = 0; i < m_searchableHeaders.Count(); i++)
		if (!PL_strncasecmp(header, (char*) m_searchableHeaders[i], PL_strlen(header))) {
			*retval = PR_TRUE;
            return NS_OK;
        }
	return NS_OK;
}

nsresult nsNNTPHostStub::GroupNotFound(const char *groupName, PRBool opening)
{
	printf("Group %s not found. \n", groupName);
	return NS_OK;
}

nsresult nsNNTPHostStub::AddNewNewsgroup(const char *groupName,
                            PRInt32 first,
                            PRInt32 last,
                            const char *flags,
                            PRBool xactiveFlags)
{
	printf ("Adding new newsgroup: %s. \n", groupName);
	nsINNTPNewsgroup * group = nsnull;
	AddGroup(groupName, &group);
	NS_IF_RELEASE(group); // we don't care about it...
	return NS_OK;
}

nsresult nsNNTPHostStub::GetNumGroupsNeedingCounts(PRInt32 * aNumGroups)
{
	printf("Getting number of groups needing counts. \n");
	// for now we'll pretend that all groups need counts since we don't actually have a newsrc file to read in from...
	PRInt32 numGroupsToCount = m_groups->Count() - m_numCallsToGetCounts;
	*aNumGroups = numGroupsToCount > 0 ? numGroupsToCount : 0 /* if negative, no more groups to count */;
	return NS_OK;
}

nsresult nsNNTPHostStub::GetFirstGroupNeedingCounts(char ** aFirstGroup)
{
	nsISupports * groupToBe = m_groups->ElementAt(m_numCallsToGetCounts);
	if (groupToBe)
	{
		nsINNTPNewsgroup * group = nsnull;
		groupToBe->QueryInterface(kINNTPNewsgroupIID, (void **) &group);
		if (group)
		{
			group->GetName(aFirstGroup);
			printf("%s needs count information.\n", *aFirstGroup);
		}
		else
			*aFirstGroup = nsnull;

		NS_IF_RELEASE(group);
	}
	else
		*aFirstGroup = nsnull;

	m_numCallsToGetCounts++; // increment # calls into this function....

	return NS_OK;
}

nsresult nsNNTPHostStub::DisplaySubscribedGroup(const char * groupname, PRInt32 first_message, PRInt32 last_message, PRInt32 total_messages,
												PRBool visit_now)
{
	printf("Displaying subscribed group %s which has %d total messages. \n", groupname, total_messages);

    nsresult rv;
    nsINNTPNewsgroup *newsgroup=NULL;

    
    rv = FindGroup(groupname, &newsgroup);
    
//    SetGroupSucceeded(TRUE);
    if (!newsgroup && visit_now) // let's try autosubscribe...
    {
        rv = AddGroup(groupname, &newsgroup);
    }

    if (!newsgroup)
        return NS_OK;
    else 
	{
        PRBool subscribed;
        newsgroup->GetSubscribed(&subscribed);
        if (!subscribed)
            newsgroup->SetSubscribed(PR_TRUE);
    }

    if (!newsgroup) return NS_OK;
    newsgroup->UpdateSummaryFromNNTPInfo(first_message, last_message,
                                    total_messages);
    return NS_OK;
}

nsresult nsNNTPHostStub::GetFirstGroupNeedingExtraInfo(char ** retval)
{
	if (retval)
		*retval = nsnull;
	return NS_OK;
}

nsresult nsNNTPHostStub::SetGroupNeedsExtraInfo(const char * groupName, PRBool needsExtraInfo)
{
	printf("Setting group %s to need extra info.\n", groupName);
	return NS_OK;
}

nsresult nsNNTPHostStub::GetNewsgroupAndNumberOfID(const char *messageID, nsINNTPNewsgroup ** group, PRUint32 * messageNumber)
{
	return NS_OK;
}

nsresult nsNNTPHostStub::SetPrettyName(const char * groupname, const char * prettyname)
{
	printf("Setting group %s to have pretty name %s. \n", groupname, prettyname);
	return NS_OK;
}

nsresult nsNNTPHostStub::LoadNewsrc()
{
	printf("Loading newsrc....\n");
	return NS_OK;
}

nsresult nsNNTPHostStub::WriteNewsrc()
{
	printf("Writing out newsrc ...\n");
	return NS_OK;
}

nsresult nsNNTPHostStub::WriteIfDirty()
{
	return NS_OK;
}

nsresult nsNNTPHostStub::MarkDirty()
{
	return NS_OK;
}

#if 0
nsresult nsNNTPHostStub::GetNewsRCFilename(char ** aNewsRCFileName)
{
	if (aNewsRCFileName)
		*aNewsRCFileName = "temp.rc";
	return NS_OK;
}
#endif

nsresult nsNNTPHostStub::SetNewsRCFilename(char * fileName)
{
	printf ("Setting newsrc file name to %s\n.", fileName);
	return NS_OK;
}

nsresult nsNNTPHostStub::GetNewsgroupList(const char *groupname, nsINNTPNewsgroupList **_retval)
{
	nsresult rv = NS_OK;
	// find group with the group name...
	nsINNTPNewsgroup * group = nsnull;
	rv = FindGroup(groupname, &group);
	if (group)
	{
		rv = group->GetNewsgroupList(_retval);
		NS_RELEASE(group);
	}

	return rv;
}

nsresult nsNNTPHostStub::FindGroup(const char * name, nsINNTPNewsgroup ** retVal)
{
	PRBool found = PR_FALSE;
	*retVal = nsnull;
	for (PRInt32 count = 0; count < m_groups->Count() && !found; count++)
	{
		nsISupports * elem = m_groups->ElementAt(count);
		if (elem)
		{	
			nsINNTPNewsgroup * group = nsnull;
			elem->QueryInterface(kINNTPNewsgroupIID, (void **) &group);
			if (group)
			{
				char * groupName = nsnull;
				group->GetName(&groupName);
				if (groupName && PL_strcmp(name, groupName) == 0)
				{
					found = PR_TRUE;
					*retVal = group;
					NS_ADDREF(group);
				}

				PR_FREEIF(groupName);
				NS_RELEASE(group);
			} // if group
			NS_RELEASE(elem);
		} // if elem
	}

	return NS_OK;
}

nsresult nsNNTPHostStub::AddGroup(const char *groupname, nsINNTPNewsgroup **retval)
{
	nsresult rv = NS_OK;
	printf ("Adding group %s to host.\n", groupname);
	// turn the group name into a new news group and create a newsgroup list for it as well...
	nsINNTPNewsgroup * group = nsnull;
	rv = NS_NewNewsgroup(&group, nsnull, /* nsNNTPArticleSet * */ nsnull, PR_TRUE, this, 0);
	if (group) // set the name for our group..
		group->SetName((char *) groupname);
	// generate a news group list for the new group...
	nsINNTPNewsgroupList * list = nsnull;
	rv = NS_NewNewsgroupList(&list,this, group);  
	// now bind the group list to the group....
	if (list)
	{
		group->SetNewsgroupList(list);
		NS_RELEASE(list); 
	}

	// now add this group to our global list...
	m_groups->AppendElement(group);

	if (retval)
	{
		*retval = group;
		NS_IF_ADDREF(group);
	}
	return NS_OK;
}

nsresult nsNNTPHostStub::RemoveGroup(const nsINNTPNewsgroup * group)
{
	char * name = nsnull;
	((nsINNTPNewsgroup *) group)->GetName(&name);

	printf ("Removing group %s.\n", name ? name : "unspecified");
	PR_FREEIF(name);
	return NS_OK;
}

nsresult nsNNTPHostStub::RemoveGroupByName(const char *groupName)
{
	printf ("Removing group %s. \n", groupName);
	return NS_OK;
}

nsresult nsNNTPHostStub::GetDbDirName(char ** aDirName)
{
	if (aDirName)
		*aDirName = "temp.db";
	return NS_OK;
}

nsresult nsNNTPHostStub::GetGroupList(char ** retVal)
{
	if (retVal)
		*retVal = nsnull;
	return NS_OK;
}

NS_BEGIN_EXTERN_C

nsresult NS_NewNNTPHost(nsINNTPHost ** aInstancePtr, const char * name, PRUint32 port)
{
	nsresult rv = NS_OK;
	if (aInstancePtr)
	{
		nsNNTPHostStub * host = new nsNNTPHostStub(name, port);
		if (host)
			rv =host->QueryInterface(kINNTPHostIID, (void **) aInstancePtr);		
	}

	return rv;
}

NS_END_EXTERN_C
