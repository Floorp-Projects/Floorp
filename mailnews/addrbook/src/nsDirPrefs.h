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
 * Portions created by the Initial Developer are Copyright (C) 1999
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

#ifndef _NSDIRPREFS_H_
#define _NSDIRPREFS_H_

class nsVoidArray;
class nsIPref;

PRInt32 INTL_ConvertToUnicode(const char* aBuffer, const PRInt32 aLength, 
							  void** uniBuffer);
PRInt32 INTL_ConvertFromUnicode(const PRUnichar* uniBuffer, 
								const PRInt32 uniLength, char** aBuffer);

#define kPreviousListVersion   2
#define kCurrentListVersion    3
#define PREF_LDAP_GLOBAL_TREE_NAME "ldap_2"
#define PREF_LDAP_VERSION_NAME     "ldap_2.version"
#define PREF_LDAP_SERVER_TREE_NAME "ldap_2.servers"

/* RDF roor for all types of address books */
/* use this to get all directories, create new directory*/
#define kAllDirectoryRoot          "moz-abdirectory://" 

#define kMDBCardRoot               "moz-abmdbcard://"
#define kMDBDirectoryRoot          "moz-abmdbdirectory://"
#define kPersonalAddressbook       "abook.mab"
#define kPersonalAddressbookUri    "moz-abmdbdirectory://abook.mab"
#define kCollectedAddressbook      "history.mab"
#define kCollectedAddressbookUri   "moz-abmdbdirectory://history.mab"


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
	cn,
	givenname,
	sn,
	mail,
	telephonenumber,   /* work */
	o,
	ou,
	l,
	street,
	auth,
	/* mscott: I've added these extra DIR_AttributeIDs because the new address book can handle these */
	carlicense,
	businesscategory,
	departmentnumber,
	description,
	employeetype,
	facsimiletelephonenumber,
	/* jpegPhoto */
	manager,
	objectclass,
	postaladdress,
	postalcode,
	secretary,
	title,
	custom1,
	custom2,
	custom3,
	custom4,
	custom5, 
	nickname, /* only valid on address book directories as LDAP does not have a nick name field */
	mobiletelephonenumber,  /* cell phone */
	pager,
	homephone
} DIR_AttributeId;

/* these enumerated types are returned by DIR_ValidateDirectoryDescription for validating a description name */
typedef enum
{
	DIR_ValidDescription = 0,
	DIR_DuplicateDescription,
	DIR_InvalidDescription
} DIR_DescriptionCode;

typedef enum
{
	idNone = 0,					/* Special value                          */ 
	idPrefName,
	idPosition, 
	idRefCount,
	idDescription,
	idServerName,
	idSearchBase,
	idFileName,
	idPort,
	idMaxHits,
	idUri,
	idLastSearchString,
	idType,	
	idCSID,
	idLocale,
	idPositionLocked,
	idDeletable,
	idStopFiltersOnHit,
	idIsOffline,
	idIsSecure,
	idVLVDisabled,
	idSaveResults,
	idEfficientWildcards,
	idEnableAuth,
	idSavePassword,
	idCustomFilters,
	idCustomAttributes,
	idAutoCompleteNever,
	idAutoCompleteEnabled,
	idAutoCompleteFilter,
	idTokenSeps,
	idColumnAttributes,
	idDnAttributes,
    idDnAttributesCount,
	idSuppressedAttributes,
	idSuppressedAttributesCount,
	idUriAttributes,
	idUriAttributesCount,
	idBasicSearchAttributes,
	idBasicSearchAttributesCount,
	idCustomDisplayUrl,
	idAuthDn,
	idPassword,
	idSearchPairList,
	idReplNever,
	idReplEnabled,
	idReplDescription,
	idReplFileName,
	idReplFilter,
	idReplLastChangeNumber,
	idReplDataVersion,
	idReplSyncURL,
	idReplExcludedAttributes,
	idReplExcludedAttributesCount
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
	char **excludedAttributes;   /* List of attributes we shouldn't replicate            */
	PRInt32 excludedAttributesCount; /* Number of attributes we shouldn't replicat           */
} DIR_ReplicationInfo;

#define DIR_Server_typedef 1     /* this quiets a redeclare warning in libaddr */

typedef struct DIR_Server
{
	/* Housekeeping fields */
	char   *prefName;			/* preference name, this server's subtree */
	PRInt32  position;			/* relative position in server list       */
	PRUint32  refCount;         /* Use count for server                   */

	/* General purpose fields */
	char   *description;		/* human readable name                    */
	char   *serverName;		    /* network host name                      */
	char   *searchBase;		    /* DN suffix to search at                 */
	char   *fileName;			/* XP path name of local DB               */
	PRInt32 port;				/* network port number                    */
	PRInt32 maxHits;			/* maximum number of hits to return       */
	char   *lastSearchString;	/* required if saving results             */
	DirectoryType dirType;	
	PRInt16   csid;				/* LDAP entries' codeset (normally UTF-8) */
	char    *locale;			/* the locale associated with the address book or directory */
    char    *uri;       // URI of the address book

	/* Flags */
	/* TBD: All the PRBool fields should eventually merge into "flags" */
	PRUint32 flags;               
	PRBool stopFiltersOnHit;
	PRBool isOffline;
	PRBool isSecure;           /* use SSL?                               */
	PRBool saveResults;    
	PRBool efficientWildcards; /* server can match substrings            */
	PRBool enableAuth;			/* AUTH: Use DN/password when binding?    */
	PRBool savePassword;		/* AUTH: Remember DN and password?        */

	/* site-configurable attributes and filters */
	nsVoidArray *customFilters;
	nsVoidArray *customAttributes;
	char *tokenSeps;
	char *autoCompleteFilter;

	/* site-configurable display column attributes */
	char *columnAttributes;     /* comma separated list of display columns */

	/* site-configurable list of attributes whose values are DNs */
	char **dnAttributes;
    PRInt32 dnAttributesCount;

	/* site-configurable list of attributes we shouldn't display in HTML */
	char **suppressedAttributes;
	PRInt32 suppressedAttributesCount;

	/* site-configurable list of attributes that contain URLs */
	char **uriAttributes;
	PRInt32 uriAttributesCount;

	/* site-configurable list of attributes for the Basic Search dialog */
	DIR_AttributeId *basicSearchAttributes;
	PRInt32 basicSearchAttributesCount;

	/* site-configurable URL to open LDAP results */
	char *customDisplayUrl;

	/* authentication fields */
	char *authDn;				/* DN to give to authenticate as			*/
	char *password;				/* Password for the DN						*/

	/* Replication fields */
	DIR_ReplicationInfo *replInfo;

	/* VLV fields */
	char *searchPairList;
} DIR_Server;

/* We are developing a new model for managing DIR_Servers. In the 4.0x world, the FEs managed each list. 
	Calls to FE_GetDirServer caused the FEs to manage and return the DIR_Server list. In our new view of the
	world, the back end does most of the list management so we are going to have the back end create and 
	manage the list. Replace calls to FE_GetDirServers() with DIR_GetDirServers(). */

nsVoidArray* DIR_GetDirectories();
nsresult DIR_GetDirServers();
nsresult DIR_ShutDown(void);  /* FEs should call this when the app is shutting down. It frees all DIR_Servers regardless of ref count values! */

nsresult DIR_AddNewAddressBook(const PRUnichar *dirName, const char *fileName, PRBool migrating, DirectoryType dirType, DIR_Server** pServer);
nsresult DIR_ContainsServer(DIR_Server* pServer, PRBool *hasDir);

nsresult DIR_DecrementServerRefCount (DIR_Server *);
nsresult DIR_IncrementServerRefCount (DIR_Server *);

/* We are trying to phase out use of FE_GetDirServers. The back end is now managing the dir server list. So you should
	be calling DIR_GetDirServers instead. */

nsVoidArray * FE_GetDirServers(void);

/* Since the strings in DIR_Server are allocated, we have bottleneck
 * routines to help with memory mgmt
 */

nsresult DIR_InitServerWithType(DIR_Server * server, DirectoryType dirType);
nsresult DIR_InitServer (DIR_Server *);
nsresult DIR_CopyServer (DIR_Server *in, DIR_Server **out);
PRBool	DIR_AreServersSame (DIR_Server *first, DIR_Server *second);
DIR_Server *DIR_LookupServer(char *serverName, PRInt32 port, char *searchBase);

nsresult DIR_DeleteServer (DIR_Server *);
nsresult DIR_DeleteServerFromList (DIR_Server *);
nsresult DIR_DeleteServerList(nsVoidArray *wholeList);

#define DIR_POS_APPEND                     0x80000000
#define DIR_POS_DELETE                     0x80000001
PRBool	DIR_SortServersByPosition(nsVoidArray *wholeList);
PRBool	DIR_SetServerPosition(nsVoidArray *wholeList, DIR_Server *server, PRInt32 position);

/* These two routines should be called to initialize and save 
 * directory preferences from the XP Java Script preferences
 */
nsresult DIR_GetServerPreferences(nsVoidArray** list);
nsresult DIR_SaveServerPreferences(nsVoidArray *wholeList);
void    DIR_GetPrefsForOneServer(DIR_Server *server, PRBool reinitialize, PRBool oldstyle);
void    DIR_SavePrefsForOneServer(DIR_Server *server);

/* This routine will clean up files for deleted directories */

/* you should never need to call this function!!! Just set the clear
	flag for the entry you want cleaned up. When all references to
	the server are released, the object will be cleaned up */
nsresult DIR_CleanUpServerPreferences(nsVoidArray *deletedList);
void    DIR_ClearPrefBranch(const char *branch);

/* Returns an allocated list of a subset of the unified list of DIR servers.
 */
nsresult DIR_GetDirServerSubset(nsVoidArray *wholeList, nsVoidArray *subList, PRUint32 flags);
PRInt32  DIR_GetDirServerSubsetCount(nsVoidArray *wholeList, PRUint32 flags);

/* We need to validate directory descriptions to make sure they are unique. They can use this API for that */
DIR_DescriptionCode DIR_ValidateDirectoryDescription(nsVoidArray * wholeList, DIR_Server * serverToValidate);

char   *DIR_CreateServerPrefName (DIR_Server *server, char *name);
void	DIR_GetServerFileName(char** filename, const char* leafName);
void	DIR_SetServerFileName(DIR_Server* pServer, const char* leafName);

/* APIs for site-configurability of LDAP attribute names and 
 * search filter behavior.
 *
 * Strings are NOT allocated, and arrays are NULL-terminated
 */
const char  *DIR_GetAttributeName (DIR_Server *server, DIR_AttributeId id);
const char **DIR_GetAttributeStrings (DIR_Server *server, DIR_AttributeId id);
const char  *DIR_GetFirstAttributeString (DIR_Server *server, DIR_AttributeId id);
const char  *DIR_GetFilterString (DIR_Server *server);
const char  *DIR_GetReplicationFilter (DIR_Server *server);
const char  *DIR_GetTokenSeparators (DIR_Server *server);
PRBool	     DIR_RepeatFilterForTokens (DIR_Server *server, const char *filter);
PRBool	     DIR_SubstStarsForSpaces (DIR_Server *server, const char *filter);
PRBool       DIR_UseCustomAttribute (DIR_Server *server, DIR_AttributeId id);

PRBool      DIR_IsDnAttribute (DIR_Server *s, const char *attr);
PRBool      DIR_IsAttributeExcludedFromHtml (DIR_Server *s, const char *attr);
PRBool      DIR_IsUriAttribute (DIR_Server *s, const char *attrib);

nsresult DIR_AttributeNameToId (DIR_Server *server, const char *attrName, DIR_AttributeId *id);

PRInt32 DIR_GetNumAttributeIDsForColumns(DIR_Server * server);
/* caller must free returned list of ids */
nsresult DIR_GetAttributeIDsForColumns(DIR_Server *server, DIR_AttributeId ** ids , PRInt32 * numIds);

/* APIs for authentication */
void		DIR_SetAuthDN (DIR_Server *s, const char *dn);
void		DIR_SetPassword (DIR_Server *s, const char *password);

/* APIs for unescaping LDAP special chars */
char   *DIR_Unescape (const char *src, PRBool makeHtml);
PRBool DIR_IsEscapedAttribute (DIR_Server *s, const char *attrib);

/* API for building a URL */
char *DIR_BuildUrl (DIR_Server *s, const char *dn, PRBool forAddToAB);

/* Walks the list enforcing the rule that only one LDAP server can be configured for autocomplete */
void DIR_SetAutoCompleteEnabled (nsVoidArray *list, DIR_Server *server, PRBool enabled);

/* Callback Notification Flags/Types/Functions */
#define DIR_NOTIFY_ADD                     0x00000001 
#define DIR_NOTIFY_DELETE                  0x00000002 
#define DIR_NOTIFY_PROPERTY_CHANGE         0x00000004 
#define DIR_NOTIFY_SCRAMBLE                0x00000008 
#define DIR_NOTIFY_ALL                     0x0000000F 

typedef PRInt32 (*DIR_NOTIFICATION_FN)(DIR_Server *server, PRUint32 flag, DIR_PrefId id, void *inst_data);

PRBool DIR_RegisterNotificationCallback(DIR_NOTIFICATION_FN fn, PRUint32 flags, void *inst_data);
PRBool DIR_DeregisterNotificationCallback(DIR_NOTIFICATION_FN fn, void *inst_data);
PRBool DIR_SendNotification(DIR_Server *server, PRUint32 flag, DIR_PrefId id);

DIR_PrefId  DIR_AtomizePrefName(const char *prefname);
char       *DIR_CopyServerStringPref(DIR_Server *server, DIR_PrefId prefid, PRInt16 csid);
PRBool     DIR_SetServerStringPref(DIR_Server *server, DIR_PrefId prefid, char *pref, PRInt16 csid);

/* Flags manipulation
 */
#define DIR_AUTO_COMPLETE_ENABLED          0x00000001  /* Directory is configured for autocomplete addressing */
#define DIR_ENABLE_AUTH                    0x00000002  /* Directory is configured for LDAP simple authentication */
#define DIR_SAVE_PASSWORD                  0x00000004
#define DIR_IS_SECURE                      0x00000008
#define DIR_SAVE_RESULTS                   0x00000010  /* not used by the FEs */
#define DIR_EFFICIENT_WILDCARDS            0x00000020  /* not used by the FEs */
#define DIR_LDAP_VERSION3                  0x00000040  /* not used by the FEs */
#define DIR_LDAP_VLV_DISABLED              0x00000080  /* not used by the FEs */
#define DIR_LDAP_VLV_SUPPORTED             0x00000100  /* not used by the FEs */
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


/* DIR_GetDirServerSubset flags
 */
#define DIR_SUBSET_HTML_ALL                0x00000001
#define DIR_SUBSET_LDAP_ALL                0x00000002
#define DIR_SUBSET_LDAP_AUTOCOMPLETE       0x00000004
#define DIR_SUBSET_LDAP_REPLICATE          0x00000008
#define DIR_SUBSET_PAB_ALL                 0x00000010


PRBool DIR_TestFlag  (DIR_Server *server, PRUint32 flag);
void    DIR_SetFlag   (DIR_Server *server, PRUint32 flag);
void    DIR_ClearFlag (DIR_Server *server, PRUint32 flag);
void    DIR_ForceFlag (DIR_Server *server, PRUint32 flag, PRBool forceOnOrOff);

char *DIR_ConvertToServerCharSet   (DIR_Server *s, char *src, PRInt16 srcCsid);
char *DIR_ConvertFromServerCharSet (DIR_Server *s, char *src, PRInt16 dstCsid);
char *DIR_ConvertString(PRInt16 srcCSID, PRInt16 dstCSID, const char *string);

#endif /* dirprefs.h */
