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
 *    Mark Banner <mark@standard8.demon.co.uk>
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

#ifndef _NSDIRPREFS_H_
#define _NSDIRPREFS_H_

//
// XXX nsDirPrefs is being greatly reduced if not removed altogether. Directory
// Prefs etc. should be handled via their appropriate nsAb*Directory classes.
//

class nsVoidArray;

#define kPreviousListVersion   2
#define kCurrentListVersion    3
#define PREF_LDAP_GLOBAL_TREE_NAME "ldap_2"
#define PREF_LDAP_VERSION_NAME     "ldap_2.version"
#define PREF_LDAP_SERVER_TREE_NAME "ldap_2.servers"

#define kMainLdapAddressBook "ldap.mab"   /* v3 main ldap address book file */

/* DIR_Server.dirType */
typedef enum
{
	LDAPDirectory,
	HTMLDirectory,
  PABDirectory,
  MAPIDirectory,
  FixedQueryLDAPDirectory = 777
} DirectoryType;

typedef enum
{
	idNone = 0,					/* Special value                          */ 
	idPrefName,
	idPosition, 
	idDescription,
	idServerName,
	idSearchBase,
	idFileName,
	idPort,
	idMaxHits,
	idUri,
	idType,	
	idCSID,
	idLocale,
	idPositionLocked,
	idDeletable,
	idIsOffline,
	idIsSecure,
	idVLVDisabled,
	idEnableAuth,
	idSavePassword,
	idAutoCompleteNever,
	idAutoCompleteEnabled,
	idAuthDn,
	idPassword,
	idReplNever,
	idReplEnabled,
	idReplFilter,
	idReplLastChangeNumber,
	idReplDataVersion,
	idReplSyncURL,
	idPalmCategory,
        idPalmSyncTimeStamp,
  idProtocolVersion,
  idAttributeMap
} DIR_PrefId;


typedef struct _DIR_ReplicationInfo

{
	char *filter;                /* LDAP filter string which constrains the repl search  */
	PRInt32 lastChangeNumber;      /* Last change we saw -- start replicating here         */
	char *syncURL;               /* Points to the server to use for replication          */
	char *dataVersion;           /* LDAP server's scoping of the lastChangeNumber        */
	                             /* Changes when the server's DB gets reloaded from LDIF */
} DIR_ReplicationInfo;

#define DIR_Server_typedef 1     /* this quiets a redeclare warning in libaddr */

typedef struct DIR_Server
{
	/* Housekeeping fields */
	char   *prefName;			/* preference name, this server's subtree */
	PRInt32  position;			/* relative position in server list       */

	/* General purpose fields */
	char   *description;		/* human readable name                    */
	char   *serverName;		    /* network host name                      */
	char   *searchBase;		    /* DN suffix to search at                 */
	char   *fileName;			/* XP path name of local DB               */
	PRInt32 port;				/* network port number                    */
	PRInt32 maxHits;			/* maximum number of hits to return       */
	DirectoryType dirType;	
	PRInt16   csid;				/* LDAP entries' codeset (normally UTF-8) */
	char    *locale;			/* the locale associated with the address book or directory */
    char    *uri;       // URI of the address book

	/* Flags */
	/* TBD: All the PRBool fields should eventually merge into "flags" */
	PRUint32 flags;               
	PRPackedBool isOffline;
	PRPackedBool isSecure;           /* use SSL?                               */
	PRPackedBool enableAuth;			/* AUTH: Use DN/password when binding?    */
	PRPackedBool savePassword;		/* AUTH: Remember DN and password?        */

	/* authentication fields */
	char *authDn;				/* DN to give to authenticate as			*/
	char *password;				/* Password for the DN						*/

	/* Replication fields */
	DIR_ReplicationInfo *replInfo;

	/* fields for palm Sync */
	PRInt32 PalmCategoryId;
	PRUint32 PalmSyncTimeStamp;
} DIR_Server;

/* We are developing a new model for managing DIR_Servers. In the 4.0x world, the FEs managed each list. 
	Calls to FE_GetDirServer caused the FEs to manage and return the DIR_Server list. In our new view of the
	world, the back end does most of the list management so we are going to have the back end create and 
	manage the list. Replace calls to FE_GetDirServers() with DIR_GetDirServers(). */

nsVoidArray* DIR_GetDirectories();
DIR_Server* DIR_GetServerFromList(const char* prefName);
nsresult DIR_ShutDown(void);  /* FEs should call this when the app is shutting down. It frees all DIR_Servers regardless of ref count values! */

nsresult DIR_AddNewAddressBook(const PRUnichar *dirName, const char *fileName, PRBool migrating, const char * uri, int maxHits, const char * authDn, DirectoryType dirType, DIR_Server** pServer);
nsresult DIR_ContainsServer(DIR_Server* pServer, PRBool *hasDir);

nsresult DIR_DeleteServerFromList (DIR_Server *);

void    DIR_SavePrefsForOneServer(DIR_Server *server);

void DIR_SetServerFileName(DIR_Server* pServer);

#endif /* dirprefs.h */
