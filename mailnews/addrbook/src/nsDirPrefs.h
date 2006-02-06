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

class nsVoidArray;

#define kPreviousListVersion   2
#define kCurrentListVersion    3
#define PREF_LDAP_GLOBAL_TREE_NAME "ldap_2"
#define PREF_LDAP_VERSION_NAME     "ldap_2.version"
#define PREF_LDAP_SERVER_TREE_NAME "ldap_2.servers"

#define kABFileName_PreviousSuffix ".na2" /* final v2 address book format */
#define kABFileName_PreviousSuffixLen 4
#define kABFileName_CurrentSuffix ".mab"  /* v3 address book extension */
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
	idReplDescription,
	idReplFileName,
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
	char *description;           /* Human readable description of replica                */
	char *fileName;              /* File name of replication database                    */
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
nsresult DIR_GetDirServers();
nsresult DIR_ShutDown(void);  /* FEs should call this when the app is shutting down. It frees all DIR_Servers regardless of ref count values! */

nsresult DIR_AddNewAddressBook(const PRUnichar *dirName, const char *fileName, PRBool migrating, const char * uri, int maxHits, const char * authDn, DirectoryType dirType, DIR_Server** pServer);
nsresult DIR_ContainsServer(DIR_Server* pServer, PRBool *hasDir);

/* Since the strings in DIR_Server are allocated, we have bottleneck
 * routines to help with memory mgmt
 */

nsresult DIR_InitServerWithType(DIR_Server * server, DirectoryType dirType);
nsresult DIR_InitServer (DIR_Server *);
nsresult DIR_CopyServer (DIR_Server *in, DIR_Server **out);

void DIR_DeleteServer (DIR_Server *);
nsresult DIR_DeleteServerFromList (DIR_Server *);

#define DIR_POS_APPEND                     0x80000000
#define DIR_POS_DELETE                     0x80000001
PRBool	DIR_SetServerPosition(nsVoidArray *wholeList, DIR_Server *server, PRInt32 position);

/* These two routines should be called to initialize and save 
 * directory preferences from the XP Java Script preferences
 */
nsresult DIR_GetServerPreferences(nsVoidArray** list);
nsresult DIR_SaveServerPreferences(nsVoidArray *wholeList);
void    DIR_GetPrefsForOneServer(DIR_Server *server, PRBool reinitialize, PRBool oldstyle);
void    DIR_SavePrefsForOneServer(DIR_Server *server);

void DIR_SetServerFileName(DIR_Server* pServer);

DIR_PrefId  DIR_AtomizePrefName(const char *prefname);

/* Flags manipulation
 */
#define DIR_AUTO_COMPLETE_ENABLED          0x00000001  /* Directory is configured for autocomplete addressing */
#define DIR_LDAP_VERSION3                  0x00000040
#define DIR_LDAP_VLV_DISABLED              0x00000080  /* not used by the FEs */
#define DIR_LDAP_ROOTDSE_PARSED            0x00000200  /* not used by the FEs */
#define DIR_AUTO_COMPLETE_NEVER            0x00000400  /* Directory is never to be used for autocompletion */
#define DIR_REPLICATION_ENABLED            0x00000800  /* Directory is configured for offline use */
#define DIR_REPLICATE_NEVER                0x00001000  /* Directory is never to be replicated */
#define DIR_UNDELETABLE                    0x00002000
#define DIR_POSITION_LOCKED                0x00004000

/* The following flags are not used by the FEs.  The are operational flags
 * that get set in occasionally to keep track of special states.
 */
/* Set when a DIR_Server is being saved.  Keeps the pref callback code from
 * reinitializing the DIR_Server structure, which in this case would always
 * be a waste of time.
 */
#define DIR_SAVING_SERVER                  0x40000000
/* Set by back end when all traces of the DIR_Server need to be removed (i.e.
 * destroying the file) when the last reference to to the DIR_Server is
 * released.  This is used primarily when the user decides to delete the
 * DIR_Server but it is referenced by other objects.  When no one is using the
 * dir server anymore, we destroy the file and clear the server
 */
#define DIR_CLEAR_SERVER				   0x80000000  

PRBool DIR_TestFlag  (DIR_Server *server, PRUint32 flag);
void    DIR_SetFlag   (DIR_Server *server, PRUint32 flag);
void    DIR_ClearFlag (DIR_Server *server, PRUint32 flag);
void    DIR_ForceFlag (DIR_Server *server, PRUint32 flag, PRBool forceOnOrOff);

#endif /* dirprefs.h */
