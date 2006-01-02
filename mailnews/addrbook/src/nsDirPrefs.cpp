/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Dan Mosedale <dmose@mozilla.org>
 *   Mark Banner <mark@standard8.demon.co.uk>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* directory server preferences (used to be dirprefs.c in 4.x) */

#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranch2.h"
#include "nsIPrefLocalizedString.h"
#include "nsIObserver.h"
#include "nsVoidArray.h"
#include "nsIServiceManager.h"
#include "nsDirPrefs.h"
#include "nsIAddrDatabase.h"
#include "nsCOMPtr.h"
#include "nsAbBaseCID.h"
#include "nsIAddrBookSession.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsILocalFile.h"
#include "nsWeakReference.h"

#include "prlog.h"
#include "plstr.h"
#include "prmem.h"
#include "prprf.h"

#define LDAP_PORT 389
#define LDAPS_PORT 636

#if defined(MOZADDRSTANDALONE)

#define NS_ERROR_OUT_OF_MEMORY -1;

#endif /* #if !defined(MOZADDRSTANDALONE) */

/*****************************************************************************
 * Private definitions
 */

/* Codeset type */
#define SINGLEBYTE   0x0000 /* 0000 0000 0000 0000 =    0 */
#define MULTIBYTE    0x0100 /* 0000 0001 0000 0000 =  256 */

/* Code Set IDs */
/* CS_DEFAULT: used if no charset param in header */
/* CS_UNKNOWN: used for unrecognized charset */

                    /* type                  id   */
#define CS_DEFAULT    (SINGLEBYTE         |   0) /*    0 */
#define CS_UTF8       (MULTIBYTE          |  34) /*  290 */
#define CS_UNKNOWN    (SINGLEBYTE         | 255) /* 255 */

/* Default settings for site-configurable prefs */
#define kDefaultMaxHits 100
#define kDefaultIsOffline PR_TRUE
#define kDefaultEnableAuth PR_FALSE
#define kDefaultSavePassword PR_FALSE
#define kDefaultLDAPCSID CS_UTF8 
#define kDefaultPABCSID  CS_DEFAULT
#define kDefaultVLVDisabled PR_FALSE
#define kDefaultPosition 1

#define kDefaultAutoCompleteEnabled PR_FALSE
#define kDefaultAutoCompleteNever   PR_FALSE

#define kDefaultReplicateNever PR_FALSE
#define kDefaultReplicaEnabled PR_FALSE
#define kDefaultReplicaFileName nsnull
#define kDefaultReplicaDataVersion nsnull
#define kDefaultReplicaDescription nsnull
#define kDefaultReplicaChangeNumber -1
#define kDefaultReplicaFilter "(objectclass=*)"

static PRBool dir_IsServerDeleted(DIR_Server * server);
static char *DIR_GetStringPref(const char *prefRoot, const char *prefLeaf, const char *defaultValue);
static char *DIR_GetLocalizedStringPref(const char *prefRoot, const char *prefLeaf, const char *defaultValue);
static PRInt32 DIR_GetIntPref(const char *prefRoot, const char *prefLeaf, PRInt32 defaultValue);
static PRBool DIR_GetBoolPref(const char *prefRoot, const char *prefLeaf, PRBool defaultValue);
static char * dir_ConvertDescriptionToPrefName(DIR_Server * server);
void DIR_SetFileName(char** filename, const char* leafName);
static void DIR_SetIntPref(const char *prefRoot, const char *prefLeaf, PRInt32 value, PRInt32 defaultValue);
static DIR_Server *dir_MatchServerPrefToServer(nsVoidArray *wholeList, const char *pref);
static PRBool dir_ValidateAndAddNewServer(nsVoidArray *wholeList, const char *fullprefname);

static PRInt32      dir_UserId = 0;
nsVoidArray  *dir_ServerList = nsnull;

/*****************************************************************************
 * Functions for creating the new back end managed DIR_Server list.
 */
class DirPrefObserver : public nsSupportsWeakReference,
                        public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS2(DirPrefObserver, nsISupportsWeakReference, nsIObserver)

NS_IMETHODIMP DirPrefObserver::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData)
{
  nsCOMPtr<nsIPrefBranch> prefBranch(do_QueryInterface(aSubject));

  const char *prefname = NS_ConvertUTF16toUTF8(aData).get();

  DIR_PrefId id = DIR_AtomizePrefName(prefname);

  /* Check to see if the server is in the unified server list.
   */
  DIR_Server *server = dir_MatchServerPrefToServer(dir_ServerList, prefname);
  if (server)
  {
    /* If the server is in the process of being saved, just ignore this
     * change.  The DIR_Server structure is not really changing.
     */
    if (DIR_TestFlag(server, DIR_SAVING_SERVER))
      return NS_OK;

    /* Reparse the root DSE if one of the following attributes changed.
     */
    if (id == idServerName || id == idSearchBase ||
        id == idEnableAuth || id == idAuthDn || id == idPassword)
      DIR_ClearFlag(server, DIR_LDAP_ROOTDSE_PARSED);

    /* If the pref that changed is the position, read it in.  If the new
     * position is zero, remove the server from the list.
     */
    if (id == idPosition)
    {
      PRInt32 position;

      /* We must not do anything if the new position is the same as the
       * position in the DIR_Server.  This avoids recursion in cases
       * where we are deleting the server.
       */
      prefBranch->GetIntPref(prefname, &position);
      if (position != server->position)
      {
        server->position = position;
        if (dir_IsServerDeleted(server))
          DIR_SetServerPosition(dir_ServerList, server, DIR_POS_DELETE);
      }
    }
  }
  /* If the server is not in the unified list, we may need to add it.  Servers
   * are only added when the position, serverName and description are valid.
   */
  else if (id == idPosition || id == idType || id == idServerName || id == idDescription)
  {
    dir_ValidateAndAddNewServer(dir_ServerList, prefname);
  }

  return NS_OK;
}

nsVoidArray* DIR_GetDirectories()
{
    if (!dir_ServerList)
        DIR_GetDirServers();
	return dir_ServerList;
}

// A pointer to the pref observer
static DirPrefObserver *prefObserver = nsnull;

nsresult DIR_GetDirServers()
{
  nsresult rv = NS_OK;

  if (!dir_ServerList)
  {
    /* we need to build the DIR_Server list */ 
    rv = DIR_GetServerPreferences(&dir_ServerList);

    /* Register the preference call back if necessary. */
    if (NS_SUCCEEDED(rv) && !prefObserver)
    {
      nsCOMPtr<nsIPrefBranch2> pbi(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
      if (NS_FAILED(rv))
        return rv;
      prefObserver = new DirPrefObserver();

      if (!prefObserver)
        return NS_ERROR_OUT_OF_MEMORY;

      NS_ADDREF(prefObserver);

      pbi->AddObserver(PREF_LDAP_SERVER_TREE_NAME, prefObserver, PR_TRUE);
    }
  }
  return rv;
}

static nsresult dir_ConvertToMabFileName()
{
  if (dir_ServerList)
  {
    PRInt32 count = dir_ServerList->Count();
    PRInt32 i;
    for (i = 0; i < count; i++)
    {
      DIR_Server *server = (DIR_Server *)dir_ServerList->ElementAt(i);
      
      // convert for main personal addressbook only
      // do other address book when convert from 4.5 to mork is done
      if (server && server->position == 1 && server->fileName)
      {
        // determine if server->fileName ends with ".na2"
        PRUint32 fileNameLen = strlen(server->fileName);
        if ((fileNameLen > kABFileName_PreviousSuffixLen) && 
          strcmp(server->fileName + fileNameLen - kABFileName_PreviousSuffixLen, kABFileName_PreviousSuffix) == 0)
        {
          //Move old abook.na2 to end of the list and change the description
          DIR_Server * newServer = nsnull;
          DIR_CopyServer(server, &newServer);
          newServer->position = count + 1;
          char *newDescription = PR_smprintf("%s 4.x", newServer->description);
          PR_FREEIF(newServer->description);
          newServer->description = newDescription;
          char *newPrefName = PR_smprintf("%s4x", newServer->prefName);
          PR_FREEIF(newServer->prefName);
          newServer->prefName = newPrefName;
          dir_ServerList->AppendElement(newServer);
          DIR_SavePrefsForOneServer(newServer);
          
          PR_FREEIF (server->fileName);
          server->fileName = nsCRT::strdup(kPersonalAddressbook);
          DIR_SavePrefsForOneServer(server);
        }
      }
    }
  }
  return NS_OK;
}

static nsresult SavePrefsFile()
{
  nsresult rv;
  nsCOMPtr<nsIPrefService> pPref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  return pPref->SavePrefFile(nsnull);
}

nsresult DIR_ShutDown()  /* FEs should call this when the app is shutting down. It frees all DIR_Servers regardless of ref count values! */
{
  nsresult rv = SavePrefsFile();
  NS_ENSURE_SUCCESS(rv, rv);

  if (dir_ServerList)
  {
    PRInt32 count = dir_ServerList->Count();
    PRInt32 i;
    for (i = 0; i < count; i++)
    {
      DIR_DeleteServer((DIR_Server *)(dir_ServerList->ElementAt(i)));
    }
    delete dir_ServerList;
    dir_ServerList = nsnull;
  }
  
  /* unregister the preference call back, if necessary.
  * we need to do this as DIR_Shutdown() is called when switching profiles
  * when using turbo.  (see nsAbDirectoryDataSource::Observe())
  * When switching profiles, prefs get unloaded and then re-loaded
  * we don't want our callback to get called for all that.
  * We'll reset our callback the first time DIR_GetDirServers() is called
  * after we've switched profiles.
  */
  NS_IF_RELEASE(prefObserver);
  
  return NS_OK;
}

nsresult DIR_ContainsServer(DIR_Server* pServer, PRBool *hasDir)
{
  if (dir_ServerList)
  {
    PRInt32 count = dir_ServerList->Count();
    PRInt32 i;
    for (i = 0; i < count; i++)
    {
      DIR_Server* server = (DIR_Server *)(dir_ServerList->ElementAt(i));
      if (server == pServer)
      {
        *hasDir = PR_TRUE;
        return NS_OK;
      }
    }
  }
  *hasDir = PR_FALSE;
  return NS_OK;
}

nsresult DIR_AddNewAddressBook(const PRUnichar *dirName, const char *fileName, PRBool migrating, const char * uri, int maxHits, const char * authDn, DirectoryType dirType, DIR_Server** pServer)
{
  DIR_Server * server = (DIR_Server *) PR_Malloc(sizeof(DIR_Server));
  if (!server)
    return NS_ERROR_OUT_OF_MEMORY;

  DIR_InitServerWithType (server, dirType);
  if (!dir_ServerList)
    DIR_GetDirServers();
  if (dir_ServerList)
  {
    NS_ConvertUCS2toUTF8 utf8str(dirName);
    server->description = ToNewCString(utf8str);
    server->position = kDefaultPosition; // don't set position so alphabetic sort will happen.
    
    if (fileName)
      server->fileName = nsCRT::strdup(fileName);
    else
      DIR_SetFileName(&server->fileName, kPersonalAddressbook);
    if (dirType == LDAPDirectory) {
      // We don't actually allow the password to be saved in the preferences;
      // this preference is (effectively) ignored by the current code base.  
      // It's here because versions of Mozilla 1.0 and earlier (maybe 1.1alpha
      // too?) would blow away the .auth.dn preference if .auth.savePassword
      // is not set.  To avoid trashing things for users who switch between
      // versions, we'll set it.  Once the versions in question become 
      // obsolete enough, this workaround can be gotten rid of.
      server->savePassword = PR_TRUE;
      if (uri)
        server->uri = nsCRT::strdup(uri);
      if (authDn)
        server->authDn = nsCRT::strdup(authDn);
      // force new LDAP directories to be treated as v3 this includes the case when 
      // we are migrating directories.
      DIR_ForceFlag(server, DIR_LDAP_VERSION3, PR_TRUE);
    }
    if (maxHits)
      server->maxHits = maxHits;
    
    dir_ServerList->AppendElement(server);
    if (!migrating) {
      DIR_SavePrefsForOneServer(server); 
    }
    else if (!server->prefName)
    {
      // Need to return pref names here so the caller will be able to get the
      // right directory properties. For migration, pref names were already
      // created so no need to get unique ones via DIR_CreateServerPrefName().
      if (!strcmp(server->fileName, kPersonalAddressbook))
        server->prefName = nsCRT::strdup("ldap_2.servers.pab");
      else if (!strcmp(server->fileName, kCollectedAddressbook))
        server->prefName = nsCRT::strdup("ldap_2.servers.history");
      else
      {
        char * leafName = dir_ConvertDescriptionToPrefName (server);
        if (leafName)
          server->prefName = PR_smprintf(PREF_LDAP_SERVER_TREE_NAME".%s", leafName);
      }
    }
#ifdef DEBUG_sspitzer
    else {
      printf("don't set the prefs, they are already set since this ab was migrated\n");
    }
#endif
    *pServer = server;
    
    // save new address book into pref file 
    return SavePrefsFile();
  }
  return NS_ERROR_FAILURE;
}

nsresult DIR_DecrementServerRefCount (DIR_Server *server)
{
  NS_ASSERTION((server != nsnull), "server is null");
 	if (server && --server->refCount <= 0)
          return DIR_DeleteServer(server);
        else
          return 1;
}

nsresult DIR_IncrementServerRefCount (DIR_Server *server)
{
  NS_ASSERTION((server != nsnull), "server is null");
  if (server)
    server->refCount++;
  return NS_OK;
}

/*****************************************************************************
 * Functions for creating DIR_Servers
 */

/* use this when you want to create a server of a particular type */
nsresult DIR_InitServerWithType(DIR_Server * server, DirectoryType dirType)
{
  NS_ENSURE_ARG_POINTER(server);
  nsresult rv = DIR_InitServer(server);

  server->dirType = dirType;
  server->csid = CS_UTF8;
  server->locale = nsnull;
  server->isOffline = (dirType == LDAPDirectory);

  return rv;
}

nsresult DIR_InitServer (DIR_Server *server)
{
  if (server)
  {
    memset(server, 0, sizeof(DIR_Server));
    server->port = LDAP_PORT;
    server->maxHits = kDefaultMaxHits;
    server->isOffline = kDefaultIsOffline;
    server->refCount = 1;
    server->position = kDefaultPosition;
    server->csid = CS_UTF8;
    server->locale = nsnull;
    server->uri = nsnull;
    // initialize the palm category
    server->PalmCategoryId = -1;
    return NS_OK;
  }
  return NS_ERROR_NULL_POINTER;
}

/*****************************************************************************
 * Functions for cloning DIR_Servers
 */

static DIR_ReplicationInfo *dir_CopyReplicationInfo (DIR_ReplicationInfo *inInfo)
{
	DIR_ReplicationInfo *outInfo = (DIR_ReplicationInfo*) PR_Calloc (1, sizeof(DIR_ReplicationInfo));
	if (outInfo)
	{
		outInfo->lastChangeNumber = inInfo->lastChangeNumber;
		if (inInfo->description)
            outInfo->description = nsCRT::strdup (inInfo->description);
		if (inInfo->fileName)
            outInfo->fileName = nsCRT::strdup (inInfo->fileName);
		if (inInfo->dataVersion)
            outInfo->dataVersion = nsCRT::strdup (inInfo->dataVersion);
		if (inInfo->syncURL)
            outInfo->syncURL = nsCRT::strdup (inInfo->syncURL);
		if (inInfo->filter)
            outInfo->filter = nsCRT::strdup (inInfo->filter);
	}
	return outInfo;
}

nsresult DIR_CopyServer (DIR_Server *in, DIR_Server **out)
{
	nsresult err = NS_OK;
	if (in) {
		*out = (DIR_Server*)PR_Malloc(sizeof(DIR_Server));
		if (*out)
		{
			memset(*out, 0, sizeof(DIR_Server));

			if (in->prefName)
			{
                (*out)->prefName = nsCRT::strdup(in->prefName);
				if (!(*out)->prefName)
					err = NS_ERROR_OUT_OF_MEMORY;
			}

			if (in->description)
			{
                (*out)->description = nsCRT::strdup(in->description);
				if (!(*out)->description)
					err = NS_ERROR_OUT_OF_MEMORY;
			}

			if (in->serverName)
			{
                (*out)->serverName = nsCRT::strdup(in->serverName);
				if (!(*out)->serverName)
					err = NS_ERROR_OUT_OF_MEMORY;
			}

			if (in->searchBase)
			{
                (*out)->searchBase = nsCRT::strdup(in->searchBase);
				if (!(*out)->searchBase)
					err = NS_ERROR_OUT_OF_MEMORY;
			}

			if (in->fileName)
			{
                (*out)->fileName = nsCRT::strdup(in->fileName);
				if (!(*out)->fileName)
					err = NS_ERROR_OUT_OF_MEMORY;
 			}

			if (in->locale)
			{
                (*out)->locale = nsCRT::strdup(in->locale);
				if (!(*out)->locale)
					err = NS_ERROR_OUT_OF_MEMORY;
			}

			(*out)->position = in->position;
			(*out)->port = in->port;
			(*out)->maxHits = in->maxHits;
			(*out)->isSecure = in->isSecure;
			(*out)->isOffline = in->isOffline;
			(*out)->dirType = in->dirType;
			(*out)->csid = in->csid;

			(*out)->flags = in->flags;
			
			(*out)->enableAuth = in->enableAuth;
			(*out)->savePassword = in->savePassword;
			if (in->authDn)
			{
                (*out)->authDn = nsCRT::strdup (in->authDn);
				if (!(*out)->authDn)
					err = NS_ERROR_OUT_OF_MEMORY;
			}
			if (in->password)
			{
                (*out)->password = nsCRT::strdup (in->password);
				if (!(*out)->password)
					err = NS_ERROR_OUT_OF_MEMORY;
			}

			if (in->replInfo)
				(*out)->replInfo = dir_CopyReplicationInfo (in->replInfo);

			(*out)->refCount = 1;
		}
		else {
			err = NS_ERROR_OUT_OF_MEMORY;
			(*out) = nsnull;
		}
	}
	else {
		PR_ASSERT (0);
		err = NS_ERROR_FAILURE;
		(*out) = nsnull;
	}

	return err;
}


/* Function for setting the position of a server.  Can be used to append,
 * delete, or move a server in a server list.
 *
 * The third parameter specifies the new position the server is to occupy.
 * The resulting position may differ depending on the lock state of the
 * given server and other servers in the list.  The following special values
 * are supported:
 *   DIR_POS_APPEND - Appends the server to the end of the list.  If the server
 *                    is already in the list, does nothing.
 *   DIR_POS_DELETE - Deletes the given server from the list.  Note that this
 *                    does not cause the server structure to be freed.
 *
 * Returns PR_TRUE if the server list was re-sorted.
 */
 PRBool DIR_SetServerPosition(nsVoidArray *wholeList, DIR_Server *server, PRInt32 position)
 {
   NS_ENSURE_ARG_POINTER(wholeList);

   PRInt32    i, count, num;
   PRBool     resort = PR_FALSE;
   DIR_Server *s=nsnull;
   
   switch (position) {
   case DIR_POS_APPEND:
   /* Do nothing if the request is to append a server that is already
		 * in the list.
     */
     count = wholeList->Count();
     for (i= 0; i < count; i++)
     {
       if  ((s = (DIR_Server *)wholeList->ElementAt(i)) != nsnull)
         if (s == server)
           return PR_FALSE;
     }
     /* In general, if there are any servers already in the list, set the
     * position to the position of the last server plus one.  If there
     * are none, set it to position 1.
     */
     if (count > 0)
     {
     /* Exception to the rule: if the last server is a locked server,
     * find the position of last unlocked server.  If there are no
     * unlocked servers, set the position to 1; otherwise, set it to
     * the position of the last unlocked server plus one.  In either
     * case the list must be resorted (to find the correct position).
       */
       s = (DIR_Server *)wholeList->ElementAt(count - 1);
       if (DIR_TestFlag(s, DIR_POSITION_LOCKED))
       {
         DIR_Server *sLast = nsnull;
         
         for (i= 0; i < count; i++)
         {
           if  ((s = (DIR_Server *)wholeList->ElementAt(i)) != nsnull)
             if (!DIR_TestFlag(s, DIR_POSITION_LOCKED))
               sLast = s;
         }
         
         if (sLast)
           server->position = sLast->position + 1;
         else
           server->position = 1;
         
         resort = PR_TRUE;
       }
       else
         server->position = s->position + 1;
     }
     else
       server->position = 1;
     
     wholeList->AppendElement(server);
     break;
     
   case DIR_POS_DELETE:
   /* Undeletable servers cannot be deleted.
     */
     if (DIR_TestFlag(server, DIR_UNDELETABLE))
       return PR_FALSE;
     
       /* Remove the prefs corresponding to the given server.  If the prefName
       * value is nsnull, the server has never been saved and there are no
       * prefs to remove.
     */
     if (server->prefName)
     {
       nsresult rv;
       nsCOMPtr<nsIPrefBranch> pPref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
       if (NS_FAILED(rv))
         return PR_FALSE;

       pPref->ClearUserPref(server->prefName);
       // mark the server as deleted by setting its position to 0
       DIR_SetIntPref(server->prefName, "position", 0, -1);
     }
     
     /* If the server is in the server list, remove it.
     */
     num = wholeList->IndexOf(server);
     if (num >= 0)
     {
     /* The list does not need to be re-sorted if the server is the
     * last one in the list.
       */
       count = wholeList->Count();
       if (num == count - 1)
       {
         wholeList->RemoveElementAt(num);
       }
       else
       {
         resort = PR_TRUE;
         wholeList->RemoveElement(server);
       }
     }
     break;
     
   default:
   /* See if the server is already in the list.
     */
     count = wholeList->Count();
     for (i= 0; i < count; i++)
     {
       if  ((s = (DIR_Server *)wholeList->ElementAt(i)) != nsnull)
         if (s == server)
           break;
     }
     
     /* If the server is not in the list, add it to the beginning and re-sort.
     */
     if (s == nsnull)
     {
       server->position = position;
       wholeList->AppendElement(server);
       resort = PR_TRUE;
     }
     
     /* Servers with locked position values cannot be moved.
     */
     else if (DIR_TestFlag(server, DIR_POSITION_LOCKED))
       return PR_FALSE;
     
       /* Don't re-sort if the server is already in the requested position.
     */
     else if (server->position != position)
     {
       server->position = position;
       wholeList->RemoveElement(server);
       wholeList->AppendElement(server);
       resort = PR_TRUE;
     }
     break;
        }
        
        /* Make sure our position changes get saved back to prefs
        */
        DIR_SaveServerPreferences(wholeList);
        
        return resort;
}


/*****************************************************************************
 * DIR_Server Callback Notification Functions
 */

/* dir_matchServerPrefToServer
 *
 * This function finds the DIR_Server in the unified DIR_Server list to which
 * the given preference string belongs.
 */
static DIR_Server *dir_MatchServerPrefToServer(nsVoidArray *wholeList, const char *pref)
{
	DIR_Server *server;

	PRInt32 count = wholeList->Count();
	PRInt32 i;
	for (i = 0; i < count; i++)
	{
		if ((server = (DIR_Server *)wholeList->ElementAt(i)) != nsnull)
		{
			if (server->prefName && PL_strstr(pref, server->prefName) == pref)
			{
				char c = pref[PL_strlen(server->prefName)];
				if (c == 0 || c == '.')
					return server;
			}
		}
	}
	return nsnull;
}

/* dir_ValidateAndAddNewServer
 *
 * This function verifies that the position, serverName and description values
 * are set for the given prefName.  If they are then it adds the server to the
 * unified server list.
 */
static PRBool dir_ValidateAndAddNewServer(nsVoidArray *wholeList, const char *fullprefname)
{
  PRBool rc = PR_FALSE;

  const char *endname = PL_strchr(&fullprefname[PL_strlen(PREF_LDAP_SERVER_TREE_NAME) + 1], '.');
  if (endname)
  {
    char *prefname = (char *)PR_Malloc(endname - fullprefname + 1);
    if (prefname)
    {
      PRInt32 dirType;
      char *t1 = nsnull, *t2 = nsnull;

      PL_strncpyz(prefname, fullprefname, endname - fullprefname + 1);

      dirType = DIR_GetIntPref(prefname, "dirType", -1);
      if (dirType != -1 &&
          DIR_GetIntPref(prefname, "position", 0) != 0 &&
          (t1 = DIR_GetStringPref(prefname, "description", nsnull)) != nsnull)
      {
        if (dirType == PABDirectory ||
           (t2 = DIR_GetStringPref(prefname, "serverName",  nsnull)) != nsnull)
        {
          DIR_Server *server = (DIR_Server *)PR_Malloc(sizeof(DIR_Server));
					if (server)
					{
            DIR_InitServerWithType(server, (DirectoryType)dirType);
            server->prefName = prefname;
            DIR_GetPrefsForOneServer(server, PR_FALSE, PR_FALSE);
            DIR_SetServerPosition(wholeList, server, server->position);
            rc = PR_TRUE;
          }
          PR_FREEIF(t2);
        }
        PR_Free(t1);
      }
      else
        PR_Free(prefname);
    }
  }

  return rc;
}

DIR_PrefId DIR_AtomizePrefName(const char *prefname)
{
  if (!prefname)
    return idNone;

	DIR_PrefId rc = idNone;

	/* Skip the "ldap_2.servers.<server-name>." portion of the string.
	 */
	if (PL_strstr(prefname, PREF_LDAP_SERVER_TREE_NAME) == prefname)
	{
		prefname = PL_strchr(&prefname[PL_strlen(PREF_LDAP_SERVER_TREE_NAME) + 1], '.');
		if (!prefname)
			return idNone;
		else
			prefname = prefname + 1;
	}


	switch (prefname[0]) {
	case 'a':
		if (PL_strstr(prefname, "autoComplete.") == prefname)
		{
			switch (prefname[13]) {
			case 'e': /* autoComplete.enabled */
				rc = idAutoCompleteEnabled;
				break;
			case 'n':
				rc = idAutoCompleteNever;
				break;
			}
		}
		else if (PL_strstr(prefname, "auth.") == prefname)
		{
			switch (prefname[5]) {
			case 'd': /* auth.dn */
				rc = idAuthDn;
				break;
			case 'e': /* auth.enabled */
				rc = idEnableAuth;
				break;
			case 'p': /* auth.password */
				rc = idPassword;
				break;
			case 's': /* auth.savePassword */
				rc = idSavePassword;
				break;
			}
		}
    else if (PL_strstr(prefname, "attrmap.") == prefname)
    {
      rc = idAttributeMap;
    }
		break;

	case 'c':
		switch (prefname[1]) {
		case 'h': /* charset */
			rc = idCSID;
			break;
		case 's': /* the new csid pref that replaced char set */
			rc = idCSID;
			break;
		}
		break;

	case 'd':
		switch (prefname[1]) {
		case 'e': /* description */
			rc = idDescription;
			break;
		case 'i': /* dirType */
			rc = idType;
			break;
		}
		break;

  case 'f':
    rc = idFileName;
    break;

	case 'i':
		switch (prefname[2]) {
		case 'O': /* filename */
			rc = idIsOffline;
			break;
		case 'S': /* filename */
			rc = idIsSecure;
			break;
		}
		break;
	case 'l':
		rc = idLocale;
		break;

	case 'm':
		rc = idMaxHits;
		break;

	case 'p':
		switch (prefname[1]) {
		case 'o':
			switch (prefname[2]) {
			case 'r': /* port */
				rc = idPort;
				break;
			case 's': /* position */
				rc = idPosition;
				break;
			}
			break;
                case  'r': /* protocolVersion */
                    rc = idProtocolVersion;
		}
		break;

	case 'r':
		if (PL_strstr(prefname, "replication.") == prefname)
		{
			switch (prefname[12]) {
			case 'd':
				switch (prefname[13]) {
					case 'a': /* replication.dataVersion */
						rc = idReplDataVersion;
						break;
					case 'e': /* replication.description */
						rc = idReplDescription;
						break;
				}
				break;
			case 'e':
        if (prefname[13] == 'n') {
					rc = idReplEnabled;
				}
				break;
			case 'f':
				switch (prefname[15]) {
				case 'e': /* replication.fileName */
					rc = idReplFileName;
					break;
				case 't': /* replication.filter */
					rc = idReplFilter;
					break;
				}
				break;
			case 'l': /* replication.lastChangeNumber */
				rc = idReplLastChangeNumber;
				break;
			case 'n': /* replication.never */
				rc = idReplNever;
				break;
			case 's': /* replication.syncURL */
				rc = idReplSyncURL;
				break;
			}
		}
		break;

	case 's':
    if (prefname[1] == 'e') {
			switch (prefname[2]) {
			case 'a':
        if (prefname[6] == 'B') { /* searchBase */
					rc = idSearchBase;
				}
				break;
			case 'r': /* serverName */
				rc = idServerName;
				break;
			}
			break;
		}
		break;

	case 'u': /* uri */
		rc = idUri;
		break;

	case 'v': /* vlvDisabled */
		rc = idVLVDisabled;
		break;
	case 'P':
		switch (prefname[4]) {
			case 'C': /* PalmCategoryId */
				rc = idPalmCategory;
				break;
			case 'S': /* PalmSyncTimeStamp */
				rc = idPalmSyncTimeStamp;
				break;
		}
		break;
	}

	PR_ASSERT(rc != idNone);
	return rc;
}


/*****************************************************************************
 * Function for comparing DIR_Servers 
 */

static PRBool dir_AreLDAPServersSame (DIR_Server *first, DIR_Server *second, PRBool strict)
{
	PR_ASSERT (first->serverName && second->serverName);

	if (first->serverName && second->serverName)
	{
        if (nsCRT::strcasecmp (first->serverName, second->serverName) == 0) 
		{
			if (first->port == second->port) 
			{
				/* allow for a null search base */
				if (!strict || (first->searchBase == nsnull && second->searchBase == nsnull))
					return PR_TRUE;
				/* otherwise check the strings */
				else if (   first->searchBase
				         && second->searchBase
                         && nsCRT::strcasecmp (first->searchBase, second->searchBase) == 0)
					return PR_TRUE;
			}
		}
	}

	return PR_FALSE;
}

static PRBool dir_AreServersSame (DIR_Server *first, DIR_Server *second, PRBool strict)
{
	/* This function used to be written to assume that we only had one PAB so it
	   only checked the server type for PABs. If both were PABDirectories, then 
	   it returned PR_TRUE. Now that we support multiple address books, we need to
	   check type & file name for address books to test if they are the same */

	if (first && second) 
	{
		/* assume for right now one personal address book type where offline is PR_FALSE */
		if ((first->dirType == PABDirectory) && (second->dirType == PABDirectory))
		{
      /* are they both really address books? */
      if (!first->isOffline && !second->isOffline)
			{
				PR_ASSERT(first->fileName && second->fileName);
				if (first->fileName && second->fileName)
                    if (nsCRT::strcasecmp(first->fileName, second->fileName) == 0)
						return PR_TRUE;

				return PR_FALSE;
			}
			else
				return dir_AreLDAPServersSame(first, second, strict);
		}

		if (first->dirType == second->dirType)
			return dir_AreLDAPServersSame(first, second, strict);
	}
	return PR_FALSE;
}

/*****************************************************************************
 * Functions for destroying DIR_Servers 
 */

/* this function determines if the passed in server is no longer part of the of
   the global server list. */
static PRBool dir_IsServerDeleted(DIR_Server * server)
{
	if (server && server->position == 0)
		return PR_TRUE;
	else
		return PR_FALSE;
}

static void dir_DeleteReplicationInfo (DIR_Server *server)
{
	DIR_ReplicationInfo *info = nsnull;
	if (server && (info = server->replInfo) != nsnull)
	{
		PR_FREEIF(info->description);
		PR_FREEIF(info->fileName);
		PR_FREEIF(info->dataVersion);
		PR_FREEIF(info->syncURL);
		PR_FREEIF(info->filter);
		PR_Free(info);
	}
}

/* when the back end manages the server list, deleting a server just decrements its ref count,
   in the old world, we actually delete the server */
static nsresult dir_DeleteServerContents (DIR_Server *server)
{
	if (server)
	{
		/* when destroying the server check its clear flag to see if things need cleared */
#ifdef XP_FileRemove
		if (DIR_TestFlag(server, DIR_CLEAR_SERVER))
		{
			if (server->fileName)
				XP_FileRemove (server->fileName, xpAddrBookNew);
			if (server->replInfo && server->replInfo->fileName)
				XP_FileRemove (server->replInfo->fileName, xpAddrBookNew);
		}
#endif /* XP_FileRemove */

		PR_FREEIF (server->prefName);
		PR_FREEIF (server->description);
		PR_FREEIF (server->serverName);
		PR_FREEIF (server->searchBase);
		PR_FREEIF (server->fileName);
		PR_FREEIF (server->authDn);
		PR_FREEIF (server->password);
		PR_FREEIF (server->locale);
        PR_FREEIF (server->uri);

		if (server->replInfo)
			dir_DeleteReplicationInfo (server);
	}
	return NS_OK;
}

nsresult DIR_DeleteServer(DIR_Server *server)
{
	if (server)
	{
		dir_DeleteServerContents(server);
		PR_Free(server);
	}

	return NS_OK;
}

nsresult DIR_DeleteServerFromList(DIR_Server *server)
{
	if (!server)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;
	nsCOMPtr<nsILocalFile> dbPath;

	nsCOMPtr<nsIAddrBookSession> abSession = 
	         do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv); 
	if (NS_SUCCEEDED(rv))
	  rv = abSession->GetUserProfileDirectory(getter_AddRefs(dbPath));
	
	if (NS_SUCCEEDED(rv))
	{
    // close the database, as long as it isn't the special ones 
    // (personal addressbook and collected addressbook)
    // which can never be deleted.  There was a bug where we would slap in
    // "abook.mab" as the file name for LDAP directories, which would cause a crash
    // on delete of LDAP directories.  this is just extra protection.
    if (strcmp(server->fileName, kPersonalAddressbook) && 
        strcmp(server->fileName, kCollectedAddressbook))
    {
      nsCOMPtr<nsIAddrDatabase> database;

      rv = dbPath->AppendNative(nsDependentCString(server->fileName));
      NS_ENSURE_SUCCESS(rv, rv);

      // close file before delete it
      nsCOMPtr<nsIAddrDatabase> addrDBFactory = 
               do_GetService(NS_ADDRDATABASE_CONTRACTID, &rv);

      if (NS_SUCCEEDED(rv) && addrDBFactory)
        rv = addrDBFactory->Open(dbPath, PR_FALSE, PR_TRUE, getter_AddRefs(database));
      if (database)  /* database exists */
      {
        database->ForceClosed();
        rv = dbPath->Remove(PR_FALSE);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }

		nsVoidArray *dirList = DIR_GetDirectories();
		DIR_SetServerPosition(dirList, server, DIR_POS_DELETE);
		DIR_DeleteServer(server);

    return SavePrefsFile();
  }

	return NS_ERROR_NULL_POINTER;
}

nsresult DIR_DeleteServerList(nsVoidArray *wholeList)
{
  if (wholeList)
  {
    DIR_Server *server = nsnull;
	
    /* TBD: Send notifications? */
    PRInt32 count = wholeList->Count();
    PRInt32 i;
    for (i = count - 1; i >=0; i--)
    {
      server = (DIR_Server *)wholeList->ElementAt(i);
      if (server != nsnull)
        DIR_DeleteServer(server);
    }
    delete wholeList;
  }
	return NS_OK;
}

/*****************************************************************************
 * Functions for retrieving subsets of the DIR_Server list 
 */
nsresult DIR_GetPersonalAddressBook(nsVoidArray *wholeList, DIR_Server **pab)
{
	if (wholeList && pab)
	{
		PRInt32 count = wholeList->Count();
		PRInt32 i;

		*pab = nsnull;
		for (i = 0; i < count; i++)
		{
			DIR_Server *server = (DIR_Server *)wholeList->ElementAt(i);
			if ((PABDirectory == server->dirType) && (PR_FALSE == server->isOffline))
			{
				if (server->serverName == nsnull || server->serverName[0] == '\0')
				{
					*pab = server;
					return NS_OK;
				}
			}
		}
	}
	return NS_ERROR_FAILURE;
}


#ifndef MOZADDRSTANDALONE

/*****************************************************************************
 * Functions for managing JavaScript prefs for the DIR_Servers 
 */

#include "nsQuickSort.h"

PR_STATIC_CALLBACK(int)
comparePrefArrayMembers(const void* aElement1, const void* aElement2, void* aData)
{
    const char* element1 = *NS_STATIC_CAST(const char* const *, aElement1);
    const char* element2 = *NS_STATIC_CAST(const char* const *, aElement2);
    const PRUint32 offset = *((const PRUint32*)aData);

    // begin the comparison at |offset| chars into the string -
    // this avoids comparing the "ldap_2.servers." portion of every element,
    // which will always remain the same.
    return strcmp(element1 + offset, element2 + offset);
}

static nsresult dir_GetChildList(const nsAFlatCString &aBranch,
                                 PRUint32 *aCount, char ***aChildList)
{
    PRUint32 branchLen = aBranch.Length();

    nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (!prefBranch) {
        return NS_ERROR_FAILURE;
    }

    nsresult rv = prefBranch->GetChildList(aBranch.get(), aCount, aChildList);
    if (NS_FAILED(rv)) {
        return rv;
    }

    // traverse the list, and truncate all the descendant strings to just
    // one branch level below the root branch.
    for (PRUint32 i = *aCount; i--; ) {
        // The prefname we passed to GetChildList was of the form
        // "ldap_2.servers." and we are returned the descendants
        // in the form of "ldap_2.servers.servername.foo"
        // But we want the prefbranch of the servername, so
        // write a NUL character in to terminate the string early.
        char *endToken = strchr((*aChildList)[i] + branchLen, '.');
        if (endToken)
            *endToken = '\0';
    }

    if (*aCount > 1) {
        // sort the list, in preparation for duplicate entry removal
        NS_QuickSort(*aChildList, *aCount, sizeof(char*), comparePrefArrayMembers, &branchLen);

        // traverse the list and remove duplicate entries.
        // we use two positions in the list; the current entry and the next
        // entry; and perform a bunch of in-place ptr moves. so |cur| points
        // to the last unique entry, and |next| points to some (possibly much
        // later) entry to test, at any given point. we know we have >= 2
        // elements in the list here, so we just init the two counters sensibly
        // to begin with.
        PRUint32 cur = 0;
        for (PRUint32 next = 1; next < *aCount; ++next) {
            // check if the elements are equal or unique
            if (!comparePrefArrayMembers(&((*aChildList)[cur]), &((*aChildList)[next]), &branchLen)) {
                // equal - just free & increment the next element ptr

                nsMemory::Free((*aChildList)[next]);
            } else {
                // cur & next are unique, so we need to shift the element.
                // ++cur will point to the next free location in the
                // reduced array (it's okay if that's == next)
                (*aChildList)[++cur] = (*aChildList)[next];
            }
        }

        // update the unique element count
        *aCount = cur + 1;
    }

    return NS_OK;
}

static char *DIR_GetStringPref(const char *prefRoot, const char *prefLeaf, const char *defaultValue)
{
    nsresult rv;
    nsCOMPtr<nsIPrefBranch> pPref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
    if (NS_FAILED(rv))
        return nsnull;

    nsXPIDLCString value;
    nsCAutoString prefLocation(prefRoot);

    prefLocation.Append('.');
    prefLocation.Append(prefLeaf);
 
    if (NS_SUCCEEDED(pPref->GetCharPref(prefLocation.get(), getter_Copies(value))))
    {
        /* unfortunately, there may be some prefs out there which look like this */
        if (value.EqualsLiteral("(null)")) 
        {
            if (defaultValue)
                value = defaultValue;
            else
                value.Truncate();
        }

        if (value.IsEmpty())
        {
          rv = pPref->GetCharPref(prefLocation.get(), getter_Copies(value));
        }
    }
    else
    {
        value = defaultValue ? nsCRT::strdup(defaultValue) : nsnull;
    }

    return ToNewCString(value);
}

/*
	Get localized unicode string pref from properties file, convert into an UTF8 string 
	since address book prefs store as UTF8 strings.  So far there are 2 default 
	prefs stored in addressbook.properties.
	"ldap_2.servers.pab.description"
	"ldap_2.servers.history.description"
*/
static char *DIR_GetLocalizedStringPref
(const char *prefRoot, const char *prefLeaf, const char *defaultValue)
{
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> pPref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));

  if (NS_FAILED(rv))
    return nsnull;

  nsCAutoString prefLocation(prefRoot);
  prefLocation.Append('.');
  prefLocation.Append(prefLeaf);

  nsXPIDLString wvalue;
  nsCOMPtr<nsIPrefLocalizedString> locStr;

  rv = pPref->GetComplexValue(prefLocation.get(), NS_GET_IID(nsIPrefLocalizedString), getter_AddRefs(locStr));
  if (NS_SUCCEEDED(rv))
    rv = locStr->ToString(getter_Copies(wvalue));

  char *value = nsnull;
  if ((const PRUnichar*)wvalue)
  {
    NS_ConvertUCS2toUTF8 utf8str(wvalue.get());
    value = ToNewCString(utf8str);
  }
  else
    value = defaultValue ? nsCRT::strdup(defaultValue) : nsnull;

  return value;
}

static PRInt32 DIR_GetIntPref(const char *prefRoot, const char *prefLeaf, PRInt32 defaultValue)
{
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> pPref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));

  if (NS_FAILED(rv))
    return defaultValue;

  PRInt32 value;
  nsCAutoString prefLocation(prefRoot);

  prefLocation.Append('.');
  prefLocation.Append(prefLeaf);

  if (NS_FAILED(pPref->GetIntPref(prefLocation.get(), &value)))
		value = defaultValue;

	return value;
}


static PRBool DIR_GetBoolPref(const char *prefRoot, const char *prefLeaf, PRBool defaultValue)
{
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> pPref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return defaultValue;

	PRBool value;
  nsCAutoString prefLocation(prefRoot);
  prefLocation.Append('.');
  prefLocation.Append(prefLeaf);

  if (NS_FAILED(pPref->GetBoolPref(prefLocation.get(), &value)))
		value = defaultValue;
	return value;
}

static void dir_GetReplicationInfo(const char *prefstring, DIR_Server *server)
{
  PR_ASSERT(server->replInfo == nsnull);

  server->replInfo = (DIR_ReplicationInfo *)PR_Calloc(1, sizeof (DIR_ReplicationInfo));
  if (server->replInfo)
  {
    PRBool prefBool;
    nsCAutoString replPrefName(prefstring);

    replPrefName.AppendLiteral(".replication");

    prefBool = DIR_GetBoolPref(replPrefName.get(), "never", kDefaultReplicateNever);
    DIR_ForceFlag(server, DIR_REPLICATE_NEVER, prefBool);

    prefBool = DIR_GetBoolPref(replPrefName.get(), "enabled", kDefaultReplicaEnabled);
    DIR_ForceFlag(server, DIR_REPLICATION_ENABLED, prefBool);

    server->replInfo->description = DIR_GetStringPref(replPrefName.get(), "description", kDefaultReplicaDescription);
    server->replInfo->syncURL = DIR_GetStringPref(replPrefName.get(), "syncURL", nsnull);
    server->replInfo->filter = DIR_GetStringPref(replPrefName.get(), "filter", kDefaultReplicaFilter);

    /* The file name and data version must be set or we ignore the
     * remaining replication prefs.
     */
    server->replInfo->fileName = DIR_GetStringPref(replPrefName.get(), "fileName", kDefaultReplicaFileName);
    server->replInfo->dataVersion = DIR_GetStringPref(replPrefName.get(), "dataVersion", kDefaultReplicaDataVersion);
    if (server->replInfo->fileName && server->replInfo->dataVersion)
    {
      server->replInfo->lastChangeNumber = DIR_GetIntPref(replPrefName.get(), "lastChangeNumber", kDefaultReplicaChangeNumber);
    }
  }
}


/* This will convert from the old preference that was a path and filename */
/* to a just a filename */
static void DIR_ConvertServerFileName(DIR_Server* pServer)
{
	char* leafName = pServer->fileName;
	char* newLeafName = nsnull;
#if defined(XP_WIN) || defined(XP_OS2)
	/* jefft -- bug 73349 This is to allow users share same address book.
	 * It only works if the user specify a full path filename.
	 */
#ifdef XP_FileIsFullPath
	if (! XP_FileIsFullPath(leafName))
		newLeafName = XP_STRRCHR (leafName, '\\');
#endif /* XP_FileIsFullPath */
#else
	newLeafName = strrchr(leafName, '/');
#endif
    pServer->fileName = newLeafName ? nsCRT::strdup(newLeafName + 1) : nsCRT::strdup(leafName);
	if (leafName) PR_Free(leafName);
}

/* This will generate a correct filename and then remove the path.
 * Note: we are assuming that the default name is in the native
 * filesystem charset. The filename will be returned as a UTF8
 * string.
 */
void DIR_SetFileName(char** fileName, const char* defaultName)
{
  if (!fileName)
    return;

	nsresult rv = NS_OK;
	nsCOMPtr<nsILocalFile> dbPath;

	*fileName = nsnull;

	nsCOMPtr<nsIAddrBookSession> abSession = 
	         do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv); 
	if (NS_SUCCEEDED(rv))
		rv = abSession->GetUserProfileDirectory(getter_AddRefs(dbPath));
	if (NS_SUCCEEDED(rv))
	{
		rv = dbPath->AppendNative(nsDependentCString(defaultName));
		if (NS_SUCCEEDED(rv))
		{
		  rv = dbPath->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0664);

		  nsAutoString realFileName;
		  rv = dbPath->GetLeafName(realFileName);

		  if (NS_SUCCEEDED(rv))
		    *fileName = ToNewUTF8String(realFileName);
		}
	}
}

/****************************************************************
Helper function used to generate a file name from the description
of a directory. Caller must free returned string. 
An extension is not applied 
*****************************************************************/

static char * dir_ConvertDescriptionToPrefName(DIR_Server * server)
{
#define MAX_PREF_NAME_SIZE 25
	char * fileName = nsnull;
	char fileNameBuf[MAX_PREF_NAME_SIZE];
	PRInt32 srcIndex = 0;
	PRInt32 destIndex = 0;
	PRInt32 numSrcBytes = 0;
	const char * descr = nsnull;
	if (server && server->description)
	{
		descr = server->description;
		numSrcBytes = PL_strlen(descr);
		while (srcIndex < numSrcBytes && destIndex < MAX_PREF_NAME_SIZE-1)
		{
			if (nsCRT::IsAsciiDigit(descr[srcIndex]) || nsCRT::IsAsciiAlpha(descr[srcIndex]) )
			{
				fileNameBuf[destIndex] = descr[srcIndex];
				destIndex++;
			}

			srcIndex++;
		}

		fileNameBuf[destIndex] = '\0'; /* zero out the last character */
	}

	if (destIndex) /* have at least one character in the file name? */
        fileName = nsCRT::strdup(fileNameBuf);

	return fileName;
}


void DIR_SetServerFileName(DIR_Server *server, const char* leafName)
{
	char * tempName = nsnull; 
	const char * prefName = nsnull;
	PRUint32 numHeaderBytes = 0; 

	if (server && (!server->fileName || !(*server->fileName)) )
	{
          PR_FREEIF(server->fileName); // might be one byte empty string.
		/* make sure we have a pref name...*/
		if (!server->prefName || !*server->prefName)
			server->prefName = DIR_CreateServerPrefName (server, nsnull);

		/* set default personal address book file name*/
		if ((server->position == 1) && (server->dirType == PABDirectory))
            server->fileName = nsCRT::strdup(kPersonalAddressbook);
		else
		{
			/* now use the pref name as the file name since we know the pref name
			   will be unique */
			prefName = server->prefName;
			if (prefName && *prefName)
			{
				/* extract just the pref name part and not the ldap tree name portion from the string */
				numHeaderBytes = PL_strlen(PREF_LDAP_SERVER_TREE_NAME) + 1; /* + 1 for the '.' b4 the name */
				if (PL_strlen(prefName) > numHeaderBytes) 
                    tempName = nsCRT::strdup(prefName + numHeaderBytes);

				if (tempName)
				{
					server->fileName = PR_smprintf("%s%s", tempName, kABFileName_CurrentSuffix);
					PR_Free(tempName);
				}
			}
		}

		if (!server->fileName || !*server->fileName) /* when all else has failed, generate a default name */
		{
			if (server->dirType == LDAPDirectory)
				DIR_SetFileName(&(server->fileName), kMainLdapAddressBook); /* generates file name with an ldap prefix */
			else
				DIR_SetFileName(&(server->fileName), kPersonalAddressbook);
		}
	}
}

char *DIR_CreateServerPrefName (DIR_Server *server, char *name)
{
	/* we are going to try to be smart in how we generate our server
	   pref name. We'll try to convert the description into a pref name
	   and then verify that it is unique. If it is unique then use it... */
	char * leafName = nsnull;
	char * prefName = nsnull;
	PRBool isUnique = PR_FALSE;

	if (name)
        leafName = nsCRT::strdup(name);
	else
		leafName = dir_ConvertDescriptionToPrefName (server);
	if (leafName)
	{
		PRInt32 uniqueIDCnt = 0;
        char **children = nsnull;
		/* we need to verify that this pref string name is unique */
		prefName = PR_smprintf(PREF_LDAP_SERVER_TREE_NAME".%s", leafName);
		isUnique = PR_FALSE;
        PRUint32 prefCount;
        nsresult rv = dir_GetChildList(NS_LITERAL_CSTRING(PREF_LDAP_SERVER_TREE_NAME "."),
                                       &prefCount, &children);
        if (NS_SUCCEEDED(rv))
        {
            while (!isUnique && prefName)
            {
                isUnique = PR_TRUE; /* now flip the logic and assume we are unique until we find a match */
                for (PRUint32 i = 0; i < prefCount && isUnique; ++i)
				{
                    if (!nsCRT::strcasecmp(children[i], prefName)) /* are they the same branch? */
                        isUnique = PR_FALSE;
				}
				if (!isUnique) /* then try generating a new pref name and try again */
				{
					PR_smprintf_free(prefName);
					prefName = PR_smprintf(PREF_LDAP_SERVER_TREE_NAME".%s_%d", leafName, ++uniqueIDCnt);
				}
			} /* if we have a list of pref Names */

            NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(prefCount, children);
		} /* while we don't have a unique name */

		PR_Free(leafName);

	} /* if leafName */

	if (!prefName) /* last resort if we still don't have a pref name is to use user_directory string */
		return PR_smprintf(PREF_LDAP_SERVER_TREE_NAME".user_directory_%d", ++dir_UserId);
	else
		return prefName;
}

void DIR_GetPrefsForOneServer (DIR_Server *server, PRBool reinitialize, PRBool oldstyle /* 4.0 Branch */)
{
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> pPref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return;
  
  PRBool  prefBool;
  char    *prefstring = server->prefName;
  char    *csidString  = nsnull;
  PRBool forcePrefSave = PR_FALSE;  /* if when reading in the prefs we did something that forces us to save the branch...*/

  if (reinitialize)
  {
    /* If we're reinitializing, we need to save off the runtime volatile
    * data which isn't stored in persistent JS prefs and restore it
    */
    PRUint32 oldRefCount = server->refCount;
    server->prefName = nsnull;
    dir_DeleteServerContents(server);
    DIR_InitServer(server);
    server->prefName = prefstring;
    server->refCount = oldRefCount; 
  }
  
  // this call fills in tempstring with the position pref, and
  // we then check to see if it's locked.
  server->position = DIR_GetIntPref (prefstring, "position", kDefaultPosition);
  PRBool bIsLocked;
  nsCAutoString tempString(prefstring);
  tempString.AppendLiteral(".position");
  pPref->PrefIsLocked(tempString.get(), &bIsLocked);
  DIR_ForceFlag(server, DIR_UNDELETABLE | DIR_POSITION_LOCKED, bIsLocked);

  server->isSecure = DIR_GetBoolPref (prefstring, "isSecure", PR_FALSE);
  server->port = DIR_GetIntPref (prefstring, "port", server->isSecure ? LDAPS_PORT : LDAP_PORT);
  if (server->port == 0)
    server->port = server->isSecure ? LDAPS_PORT : LDAP_PORT;
  server->maxHits = DIR_GetIntPref (prefstring, "maxHits", kDefaultMaxHits);
  
  if (0 == PL_strcmp(prefstring, "ldap_2.servers.pab") || 
    0 == PL_strcmp(prefstring, "ldap_2.servers.history")) 
  {
    // get default address book name from addressbook.properties 
    server->description = DIR_GetLocalizedStringPref(prefstring, "description", "");
  }
  else
    server->description = DIR_GetStringPref (prefstring, "description", "");
  
  server->serverName = DIR_GetStringPref (prefstring, "serverName", "");
  server->searchBase = DIR_GetStringPref (prefstring, "searchBase", "");
  server->isOffline = DIR_GetBoolPref (prefstring, "isOffline", kDefaultIsOffline);
  server->dirType = (DirectoryType)DIR_GetIntPref (prefstring, "dirType", LDAPDirectory);
  if (server->dirType == PABDirectory)
  {
    /* make sure there is a PR_TRUE PAB */
    if (!server->serverName || !*server->serverName)
      server->isOffline = PR_FALSE;
  }

  server->fileName = DIR_GetStringPref (prefstring, "filename", "");
  if ( (!server->fileName || !*(server->fileName)) && !oldstyle) /* if we don't have a file name and this is the new branch get a file name */
    DIR_SetServerFileName (server, server->serverName);
  if (server->fileName && *server->fileName)
    DIR_ConvertServerFileName(server);

  // the string "s" is the default uri ( <scheme> + "://" + <filename> )
  nsCString s((server->dirType == PABDirectory || server->dirType == MAPIDirectory) ? kMDBDirectoryRoot : kLDAPDirectoryRoot);
  s.Append (server->fileName);
  server->uri = DIR_GetStringPref (prefstring, "uri", s.get ());

  dir_GetReplicationInfo (prefstring, server);

  server->PalmCategoryId = DIR_GetIntPref (prefstring, "PalmCategoryId", -1);
  server->PalmSyncTimeStamp = DIR_GetIntPref (prefstring, "PalmSyncTimeStamp", 0);

  /* Get authentication prefs */
  server->enableAuth = DIR_GetBoolPref (prefstring, "auth.enabled", kDefaultEnableAuth);
  server->authDn = DIR_GetStringPref (prefstring, "auth.dn", nsnull);
  server->savePassword = DIR_GetBoolPref (prefstring, "auth.savePassword", kDefaultSavePassword);
  if (server->savePassword)
    server->password = DIR_GetStringPref (prefstring, "auth.password", "");
  
  char *versionString = DIR_GetStringPref(prefstring, "protocolVersion", "3");
  DIR_ForceFlag(server, DIR_LDAP_VERSION3, !strcmp(versionString, "3"));
  nsCRT::free(versionString);

  prefBool = DIR_GetBoolPref (prefstring, "autoComplete.enabled", kDefaultAutoCompleteEnabled);
  DIR_ForceFlag (server, DIR_AUTO_COMPLETE_ENABLED, prefBool);
  prefBool = DIR_GetBoolPref (prefstring, "autoComplete.never", kDefaultAutoCompleteNever);
  DIR_ForceFlag (server, DIR_AUTO_COMPLETE_NEVER, prefBool);
  
  /* read in the I18N preferences for the directory --> locale and csid */
  
  /* okay we used to write out the csid as a integer pref called "charset" then we switched to a string pref called "csid" 
  for I18n folks. So we want to read in the old integer pref and if it is not kDefaultPABCSID (which is a bogus -1), 
  then use it as the csid and when we save the server preferences later on we'll clear the old "charset" pref so we don't
  have to do this again. Otherwise, we already have a string pref so use that one */
  
  csidString = DIR_GetStringPref (prefstring, "csid", nsnull);
  if (csidString) /* do we have a csid string ? */
  {
    server->csid = CS_UTF8;
    //		server->csid = INTL_CharSetNameToID (csidString);
    PR_Free(csidString);
  }
  else 
  { 
    /* try to read it in from the old integer style char set preference */
    if (server->dirType == PABDirectory || server->dirType == MAPIDirectory)
      server->csid = (PRInt16) DIR_GetIntPref (prefstring, "charset", kDefaultPABCSID);
    else
      server->csid = (PRInt16) DIR_GetIntPref (prefstring, "charset", kDefaultLDAPCSID);	
    
    forcePrefSave = PR_TRUE; /* since we read from the old pref we want to force the new pref to be written out */
  }
  
  if (server->csid == CS_DEFAULT || server->csid == CS_UNKNOWN)
    server->csid = CS_UTF8;
  //		server->csid = INTL_GetCharSetID(INTL_DefaultTextWidgetCsidSel);
  
  /* now that the csid is taken care of, read in the locale preference */
  server->locale = DIR_GetStringPref (prefstring, "locale", nsnull);

  prefBool = DIR_GetBoolPref (prefstring, "vlvDisabled", kDefaultVLVDisabled);
  DIR_ForceFlag (server, DIR_LDAP_VLV_DISABLED | DIR_LDAP_ROOTDSE_PARSED, prefBool);
  
  if (!oldstyle /* we don't care about saving old directories */ && forcePrefSave && !dir_IsServerDeleted(server) )
    DIR_SavePrefsForOneServer(server); 
}

/* return total number of directories */
static PRInt32 dir_GetPrefsFrom40Branch(nsVoidArray **list)
{
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> pPref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return -1;
  
  PRInt32 result = -1;
  (*list) = new nsVoidArray();
  if (!(*list))
    return result;
  
  /* get the preference for how many directories */
  if (*list)
  {
    PRInt32 i = 0;
    PRInt32 numDirectories = 0;
    
    pPref->GetIntPref("ldap_1.number_of_directories", &numDirectories);	
    /* ldap_1.directory start from 1 */
    for (i = 1; i <= numDirectories; i++)
    {
      DIR_Server *server;
      
      server = (DIR_Server *)PR_Calloc(1, sizeof(DIR_Server));
      if (server)
      {
        char *prefName = PR_smprintf("ldap_1.directory%i", i);
        if (prefName)
        {
          DIR_InitServer(server);
          server->prefName = prefName;
          DIR_GetPrefsForOneServer(server, PR_FALSE, PR_TRUE);				
          PR_smprintf_free(server->prefName);
          server->prefName = DIR_CreateServerPrefName (server, nsnull);
          /* Leave room for Netcenter */
          server->position = (server->dirType == PABDirectory ? i : i + 1);
          (*list)->AppendElement(server);
        }
      }
    }
    
    /* all.js should have filled this stuff in */
    PR_ASSERT(numDirectories != 0);
    
    result = numDirectories;
  }
  
  return result;
}

static nsresult dir_GetPrefsFrom45Branch(nsVoidArray **list, nsVoidArray **obsoleteList)
{
    nsresult rv;
    nsCOMPtr<nsIPrefBranch> pPref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
    if (NS_FAILED(rv))
        return rv;

    (*list) = new nsVoidArray();
    if (!(*list))
        return NS_ERROR_OUT_OF_MEMORY;

    if (obsoleteList)
    {
        (*obsoleteList) = new nsVoidArray();
        if (!(*obsoleteList))
        {
            delete (*list);
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    char **children;
    PRUint32 prefCount;

    rv = dir_GetChildList(NS_LITERAL_CSTRING(PREF_LDAP_SERVER_TREE_NAME "."),
                          &prefCount, &children);
    if (NS_FAILED(rv))
        return rv;

    /* TBD: Temporary code to read broken "ldap" preferences tree.
     *      Remove line with if statement after M10.
     */
    if (dir_UserId == 0)
        pPref->GetIntPref(PREF_LDAP_GLOBAL_TREE_NAME".user_id", &dir_UserId);

    for (PRUint32 i = 0; i < prefCount; ++i)
    {
        DIR_Server *server;

        server = (DIR_Server *)PR_Calloc(1, sizeof(DIR_Server));
        if (server)
        {
            DIR_InitServer(server);
            server->prefName = nsCRT::strdup(children[i]);
            DIR_GetPrefsForOneServer(server, PR_FALSE, PR_FALSE);
            if (server->description && server->description[0] && 
                ((server->dirType == PABDirectory ||
                  server->dirType == MAPIDirectory ||
                  server->dirType == FixedQueryLDAPDirectory ||  // this one might go away
                  server->dirType == LDAPDirectory) ||
                 (server->serverName && server->serverName[0])))
            {
                if (!dir_IsServerDeleted(server))
                {
                    (*list)->AppendElement(server);
                }
                else if (obsoleteList)
                    (*obsoleteList)->AppendElement(server);
                else
                    DIR_DeleteServer(server);
            }
            else
            {
                DIR_DeleteServer(server);
            }
        }
    }

    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(prefCount, children);

    return NS_OK;
}

// I don't think we care about locked positions, etc.
void DIR_SortServersByPosition(nsVoidArray *serverList)
{
  int i, j;
  DIR_Server *server;
  
  int count = serverList->Count();
  for (i = 0; i < count - 1; i++)
  {
    for (j = i + 1; j < count; j++)
    {
      if (((DIR_Server *) serverList->ElementAt(j))->position < ((DIR_Server *) serverList->ElementAt(i))->position)
      {
        server        = (DIR_Server *) serverList->ElementAt(i);
        serverList->ReplaceElementAt(serverList->ElementAt(j), i);
        serverList->ReplaceElementAt(server, j);
      }
    }
  }
}


nsresult DIR_GetServerPreferences(nsVoidArray** list)
{
  nsresult err;
  nsCOMPtr<nsIPrefBranch> pPref(do_GetService(NS_PREFSERVICE_CONTRACTID, &err));
  if (NS_FAILED(err))
    return err;

  PRInt32 position = 1;
  PRInt32 version = -1;
  char **oldChildren = nsnull;
  PRBool savePrefs = PR_FALSE;
  PRBool migrating = PR_FALSE;
  nsVoidArray *oldList = nsnull;
  nsVoidArray *obsoleteList = nsnull;
  nsVoidArray *newList = nsnull;
  PRInt32 i, j, count;
  
  
  /* Update the ldap list version and see if there are old prefs to migrate. */
  if (NS_SUCCEEDED(pPref->GetIntPref(PREF_LDAP_VERSION_NAME, &version)))
  {
    if (version < kPreviousListVersion)
    {
      pPref->SetIntPref(PREF_LDAP_VERSION_NAME, kCurrentListVersion);
      
      /* Look to see if there's an old-style "ldap_1" tree in prefs */
      PRUint32 prefCount;
      err = dir_GetChildList(NS_LITERAL_CSTRING("ldap_1."),
        &prefCount, &oldChildren);
      if (NS_SUCCEEDED(err))
      {
        if (prefCount > 0)
        {
          migrating = PR_TRUE;
          position = dir_GetPrefsFrom40Branch(&oldList);
        }
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(prefCount, oldChildren);
      }
    }
  }

  /* Find the new-style "ldap_2.servers" tree in prefs */
  err = dir_GetPrefsFrom45Branch(&newList, migrating ? &obsoleteList : nsnull);

  /* Merge the new tree onto the old tree, old on top, new at bottom */
  if (NS_SUCCEEDED(err) && oldList && newList)
  {
    DIR_Server *newServer;
    
    /* Walk through the new list looking for servers that are duplicates of
    * ones in the old list.  Mark any duplicates for non-inclusion in the
    * final list.
    */
    PRInt32 newCount = newList->Count();
    for (i = 0; i < newCount; i++)
    {
      newServer = (DIR_Server *)newList->ElementAt(i);
      if (nsnull != newServer)
      {
        DIR_Server *oldServer;
        
        PRInt32 oldCount = oldList->Count();
        for (j = 0; j < oldCount; j++)
        {
          oldServer = (DIR_Server *)oldList->ElementAt(j);
          if (nsnull != oldServer)
          {
          /* Don't add servers which are in the old list and don't add a
          * second personal address book.
            */
            if (dir_AreServersSame(newServer, oldServer, PR_FALSE) ||
                (oldServer->dirType == PABDirectory && !oldServer->isOffline &&
                 newServer->dirType == PABDirectory && !newServer->isOffline))
            {
            /* Copy any new prefs out of the new server.
              */
              PR_FREEIF(oldServer->prefName);
              oldServer->prefName  = nsCRT::strdup(newServer->prefName);
              /* since the pref name has now been set, we can generate a proper
              file name in case we don't have one already */
              if (!oldServer->fileName || !*oldServer->fileName)
                DIR_SetServerFileName(oldServer, nsnull); 
              
              oldServer->flags     = newServer->flags;
              
              /* Mark the new version of the server as one not to be moved
              * to the final list.
              */
              newServer->position = 0;
              break;
            }
          }
        }
      }
    }

    /* Walk throught the new list again.  This time delete duplicates and
    * move the rest to the old (final) list.
    */
    count = newList->Count();
    for (i = count - 1; i >= 0; i--)
    {
      newServer = (DIR_Server *)newList->ElementAt(i);
      if (!dir_IsServerDeleted(newServer))
      {
      /* Make sure new servers are placed after old servers, but
      * keep them in relative order.
        */
        if (!DIR_TestFlag(newServer, DIR_POSITION_LOCKED))
        {
        /* The server at position 2 (i.e. Netcenter) must be left
        * at position 2.
          */
          if (newServer->position > 2) 
            newServer->position += position;
        }
        oldList->AppendElement(newServer);
      }
      else
      {
        DIR_DecrementServerRefCount(newServer);
      }
    }
    newList->Clear();
    DIR_DeleteServerList(newList);
    
    *list = oldList;
    savePrefs = PR_TRUE;
  }
  else
    *list = newList;

    /* Remove any obsolete servers from the list.
    * Note that we only remove obsolete servers when we are migrating.  We
    * don't do it otherwise because that would keep users from manually
    * re-adding these servers (which they should be allowed to do).
    */
  if (NS_SUCCEEDED(err) && obsoleteList)
  {
    DIR_Server *obsoleteServer;
    nsVoidArray *walkObsoleteList = obsoleteList;
    
    count = walkObsoleteList->Count();
    for (i = 0; i < count;i++)
    {
      if (nsnull != (obsoleteServer = (DIR_Server *)walkObsoleteList->ElementAt(i)))
      {
        DIR_Server *existingServer;
        nsVoidArray *walkExistingList = *list;
        
        PRInt32 existCount = walkExistingList->Count();
        for (j = 0; j < existCount;j++)
        {
          existingServer = (DIR_Server *)walkExistingList->ElementAt(j);
          if (nsnull != existingServer)
          {
            if (dir_AreServersSame(existingServer, obsoleteServer, PR_FALSE))
            {
              savePrefs = PR_TRUE;
              DIR_DecrementServerRefCount(existingServer);
              (*list)->RemoveElement(existingServer);
              break;
            }
          }
        }
      }
    }
  }
  if (obsoleteList)
    DIR_DeleteServerList(obsoleteList);

  if (version < kCurrentListVersion)
  {
    pPref->SetIntPref(PREF_LDAP_VERSION_NAME, kCurrentListVersion);
    // see if we have the ab upgrader.  if so, skip this, since we
    // will be migrating.
    // we can't migrate 4.x therefore, move the 4.x pab aside
    dir_ConvertToMabFileName();
  }
  /* Write the merged list so we get it next time we ask */
  if (savePrefs)
    DIR_SaveServerPreferences(*list);
 
  DIR_SortServersByPosition(*list);
  return err;
}

static void DIR_SetStringPref(const char *prefRoot, const char *prefLeaf, const char *value, const char *defaultValue)
{
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> pPref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv)); 
  if (NS_FAILED(rv)) 
    return;

  nsXPIDLCString defaultPref;
  nsCAutoString prefLocation(prefRoot);

  prefLocation.Append('.');
  prefLocation.Append(prefLeaf);

  if (NS_SUCCEEDED(pPref->GetCharPref(prefLocation.get(), getter_Copies(defaultPref))))
  {
		/* If there's a default pref, just set ours in and let libpref worry 
		 * about potential defaults in all.js
		 */
    if (value) /* added this check to make sure we have a value before we try to set it..*/
      rv = pPref->SetCharPref (prefLocation.get(), value);
    else
      rv = pPref->ClearUserPref(prefLocation.get());
	}
	else
	{
		/* If there's no default pref, look for a user pref, and only set our value in
		 * if the user pref is different than one of them.
		 */
    nsXPIDLCString userPref;
    if (NS_SUCCEEDED(pPref->GetCharPref (prefLocation.get(), getter_Copies(userPref))))
		{
      if (value && (defaultValue ? nsCRT::strcasecmp(value, defaultValue) : value != defaultValue))
        rv = pPref->SetCharPref (prefLocation.get(), value);
      else
        rv = pPref->ClearUserPref(prefLocation.get());
		}
		else
		{
      if (value && (defaultValue ? nsCRT::strcasecmp(value, defaultValue) : value != defaultValue))
        rv = pPref->SetCharPref (prefLocation.get(), value); 
		}
	}

  NS_ASSERTION(NS_SUCCEEDED(rv), "Could not set pref in DIR_SetStringPref");
}


static void DIR_SetIntPref(const char *prefRoot, const char *prefLeaf, PRInt32 value, PRInt32 defaultValue)
{
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> pPref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv)); 
  if (NS_FAILED(rv)) 
    return;

  PRInt32 defaultPref;
  nsCAutoString prefLocation(prefRoot);

  prefLocation.Append('.');
  prefLocation.Append(prefLeaf);

  if (NS_SUCCEEDED(pPref->GetIntPref(prefLocation.get(), &defaultPref)))
  {
    /* solve the problem where reordering user prefs must override default prefs */
    rv = pPref->SetIntPref(prefLocation.get(), value);
  }
  else
  {
    PRInt32 userPref;
    if (NS_SUCCEEDED(pPref->GetIntPref(prefLocation.get(), &userPref)))
    {
      if (value != defaultValue)
        rv = pPref->SetIntPref(prefLocation.get(), value);
      else
        rv = pPref->ClearUserPref(prefLocation.get());
    }
    else
    {
      if (value != defaultValue)
        rv = pPref->SetIntPref(prefLocation.get(), value); 
    }
  }

  NS_ASSERTION(NS_SUCCEEDED(rv), "Could not set pref in DIR_SetIntPref");
}


static void DIR_SetBoolPref(const char *prefRoot, const char *prefLeaf, PRBool value, PRBool defaultValue)
{
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> pPref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv)); 
  if (NS_FAILED(rv)) 
    return;

  PRBool defaultPref;
  nsCAutoString prefLocation(prefRoot);

  prefLocation.Append('.');
  prefLocation.Append(prefLeaf);

  if (NS_SUCCEEDED(pPref->GetBoolPref(prefLocation.get(), &defaultPref)))
  {
    /* solve the problem where reordering user prefs must override default prefs */
    rv = pPref->SetBoolPref(prefLocation.get(), value);
  }
  else
  {
    PRBool userPref;
    if (NS_SUCCEEDED(pPref->GetBoolPref(prefLocation.get(), &userPref)))
    {
      if (value != defaultValue)
        rv = pPref->SetBoolPref(prefLocation.get(), value);
      else
        rv = pPref->ClearUserPref(prefLocation.get());
    }
    else
    {
      if (value != defaultValue)
        rv = pPref->SetBoolPref(prefLocation.get(), value);
    }
  }

  NS_ASSERTION(NS_SUCCEEDED(rv), "Could not set pref in DIR_SetBoolPref");
}


static nsresult dir_SaveReplicationInfo(const char *prefRoot, DIR_Server *server)
{
  nsresult err = NS_OK;
  nsCAutoString prefLocation(prefRoot);
  prefLocation.AppendLiteral(".replication");

  DIR_SetBoolPref(prefLocation.get(), "never", DIR_TestFlag (server, DIR_REPLICATE_NEVER), kDefaultReplicateNever);
  DIR_SetBoolPref(prefLocation.get(), "enabled", DIR_TestFlag (server, DIR_REPLICATION_ENABLED), kDefaultReplicaEnabled);

	if (server->replInfo)
	{
    DIR_SetStringPref(prefLocation.get(), "description", server->replInfo->description, kDefaultReplicaDescription);
    DIR_SetStringPref(prefLocation.get(), "fileName", server->replInfo->fileName, kDefaultReplicaFileName);
    DIR_SetStringPref(prefLocation.get(), "filter", server->replInfo->filter, kDefaultReplicaFilter);
    DIR_SetIntPref(prefLocation.get(), "lastChangeNumber", server->replInfo->lastChangeNumber, kDefaultReplicaChangeNumber);
    DIR_SetStringPref(prefLocation.get(), "syncURL", server->replInfo->syncURL, nsnull);
    DIR_SetStringPref(prefLocation.get(), "dataVersion", server->replInfo->dataVersion, kDefaultReplicaDataVersion);
	}
  else if (DIR_TestFlag(server, DIR_REPLICATION_ENABLED))
    server->replInfo = (DIR_ReplicationInfo *) PR_Calloc (1, sizeof(DIR_ReplicationInfo));

  return err;
}


void DIR_SavePrefsForOneServer(DIR_Server *server)
{
  if (!server)
    return;

  char *prefstring;
  char * csidAsString = nsnull;

  if (server->prefName == nsnull)
    server->prefName = DIR_CreateServerPrefName (server, nsnull);
  prefstring = server->prefName;

  DIR_SetFlag(server, DIR_SAVING_SERVER);

  DIR_SetIntPref (prefstring, "position", server->position, kDefaultPosition);

  // Only save the non-default address book name
  if (0 != PL_strcmp(prefstring, "ldap_2.servers.pab") &&
      0 != PL_strcmp(prefstring, "ldap_2.servers.history")) 
    DIR_SetStringPref(prefstring, "description", server->description, "");

  DIR_SetStringPref(prefstring, "serverName", server->serverName, "");
  DIR_SetStringPref(prefstring, "searchBase", server->searchBase, "");
  DIR_SetStringPref(prefstring, "filename", server->fileName, "");
  if (server->port == 0)
    server->port = server->isSecure ? LDAPS_PORT : LDAP_PORT;
  DIR_SetIntPref(prefstring, "port", server->port, server->isSecure ? LDAPS_PORT : LDAP_PORT);
  DIR_SetIntPref(prefstring, "maxHits", server->maxHits, kDefaultMaxHits);
  DIR_SetBoolPref(prefstring, "isSecure", server->isSecure, PR_FALSE);
  DIR_SetIntPref(prefstring, "dirType", server->dirType, LDAPDirectory);
  DIR_SetBoolPref(prefstring, "isOffline", server->isOffline, kDefaultIsOffline);

  if (server->dirType == LDAPDirectory)
    DIR_SetStringPref(prefstring, "uri", server->uri, "");

  DIR_SetBoolPref(prefstring, "autoComplete.enabled", DIR_TestFlag(server, DIR_AUTO_COMPLETE_ENABLED), kDefaultAutoCompleteEnabled);
  DIR_SetBoolPref(prefstring, "autoComplete.never", DIR_TestFlag(server, DIR_AUTO_COMPLETE_NEVER), kDefaultAutoCompleteNever);

  /* save the I18N information for the directory */
	
  /* I18N folks want us to save out the csid as a string.....*/
/* csidAsString = (char *) INTL_CsidToCharsetNamePt(server->csid);*/ /* this string is actually static we should not free it!!! */
  csidAsString = NULL; /* this string is actually static we should not free it!!! */
  if (csidAsString)
    DIR_SetStringPref(prefstring, "csid", csidAsString, nsnull);
	
  /* since we are no longer writing out the csid as an integer, make sure that preference is removed.
     kDefaultPABCSID is a bogus csid value that when we read back in we can recognize as an outdated pref */

  /* this is dirty but it works...this is how we assemble the pref name in all of the DIR_SetString/bool/intPref functions */
  nsCAutoString tempPref(prefstring);
  tempPref.AppendLiteral(".charset");

  /* now clear the pref */
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> pPref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return;

  pPref->ClearUserPref(tempPref.get());

  /* now save the locale string */
  DIR_SetStringPref(prefstring, "locale", server->locale, nsnull);

  /* Save authentication prefs */
  DIR_SetBoolPref (prefstring, "auth.enabled", server->enableAuth, kDefaultEnableAuth);
  DIR_SetBoolPref (prefstring, "auth.savePassword", server->savePassword, kDefaultSavePassword);
  DIR_SetStringPref (prefstring, "auth.dn", server->authDn, "");
  if (server->savePassword && server->authDn && server->password)
  {
    DIR_SetStringPref (prefstring, "auth.password", server->password, "");
  }
  else
  {
    DIR_SetStringPref (prefstring, "auth.password", "", "");
    PR_FREEIF (server->password);
  }

  DIR_SetBoolPref (prefstring, "vlvDisabled", DIR_TestFlag(server, DIR_LDAP_VLV_DISABLED), kDefaultVLVDisabled);

  DIR_SetStringPref(prefstring, "protocolVersion",
                    DIR_TestFlag(server, DIR_LDAP_VERSION3) ? "3" : "2",
                    "3");

  dir_SaveReplicationInfo (prefstring, server);
	
  DIR_SetIntPref (prefstring, "PalmCategoryId", server->PalmCategoryId, -1);
  DIR_SetIntPref (prefstring, "PalmSyncTimeStamp", server->PalmSyncTimeStamp, 0);

  DIR_ClearFlag(server, DIR_SAVING_SERVER);
}

nsresult DIR_SaveServerPreferences (nsVoidArray *wholeList)
{
	if (wholeList)
	{
    nsresult rv;
    nsCOMPtr<nsIPrefBranch> pPref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv)); 
    if (NS_FAILED(rv))
      return rv;

		PRInt32  i;
		PRInt32  count = wholeList->Count();
		DIR_Server *server;

		for (i = 0; i < count; i++)
		{
			server = (DIR_Server *) wholeList->ElementAt(i);
			if (server)
				DIR_SavePrefsForOneServer(server);
		}
		pPref->SetIntPref(PREF_LDAP_GLOBAL_TREE_NAME".user_id", dir_UserId);
	}

	return NS_OK;
}

#endif /* #if !defined(MOZADDRSTANDALONE) */

PRBool DIR_TestFlag (DIR_Server *server, PRUint32 flag)
{
	if (server)
		return NS_OK != (server->flags & flag);
	return PR_FALSE;
}

void DIR_SetFlag (DIR_Server *server, PRUint32 flag)
{
	PR_ASSERT(server);
	if (server)
		server->flags |= flag;
}

void DIR_ClearFlag (DIR_Server *server, PRUint32 flag)
{
	PR_ASSERT(server);
	if (server)
		server->flags &= ~flag;
}


void DIR_ForceFlag (DIR_Server *server, PRUint32 flag, PRBool setIt)
{
	PR_ASSERT(server);
	if (server)
	{
		if (setIt)
			server->flags |= flag;
		else
			server->flags &= ~flag;
	}
}
