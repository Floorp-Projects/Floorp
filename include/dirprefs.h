/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _DIRPREFS_H_
#define _DIRPREFS_H_

#define kCurrentListVersion 1 

/* DIR_Server.dirType */
typedef enum
{
	LDAPDirectory,
	HTMLDirectory,
	PABDirectory
} DirectoryType;

typedef enum
{
	cn,
	givenname,
	sn,
	mail,
	telephonenumber,
	o,
	ou,
	l,
	street,
	auth,
	custom1,
	custom2,
	custom3,
	custom4,
	custom5
} DIR_AttributeId;


typedef struct _DIR_ReplicationInfo DIR_ReplicationInfo;


typedef struct DIR_Server
{
	char *description;		    /* human readable name                    */
	char *serverName;		    /* network host name                      */
	char *searchBase;		    /* DN suffix to search at                 */
	char *fileName;			    /* XP path name of local DB               */
	char *prefId;               /* name of this server's tree in JS prefs */
	int port;				    /* network port number                    */
	int maxHits;			    /* maximum number of hits to return       */
	XP_Bool isSecure;           /* use SSL?                               */
	XP_Bool saveResults;    
	XP_Bool efficientWildcards; /* server can match substrings            */
	char *lastSearchString;	    /* required if saving results             */
	DirectoryType dirType;	
	uint32 flags;               

	/* site-configurable attributes and filters */
	XP_List *customFilters;
	XP_List *customAttributes;
	char *tokenSeps;
	XP_Bool stopFiltersOnHit;
	XP_Bool isOffline;

	/* site-configurable list of attributes whose values are DNs */
	char **dnAttributes;
    int dnAttributesCount;

	/* site-configurable list of attributes we shouldn't display in HTML */
	char **suppressedAttributes;
	int suppressedAttributesCount;

	/* site-configurable list of attributes for the Basic Search dialog */
	DIR_AttributeId *basicSearchAttributes;
	int basicSearchAttributesCount;

	/* site-configurable URL to open LDAP results */
	char *customDisplayUrl;

	/* authentication fields */
	XP_Bool enableAuth;		/* Use DN and password when binding?		*/
	XP_Bool savePassword;	/* Remember the DN and password we gave?	*/
	char *authDn;			/* DN to give to authenticate as			*/
	char *password;			/* Password for the DN						*/

	/* replication fields */
	DIR_ReplicationInfo *replInfo;

} DIR_Server;


XP_BEGIN_PROTOS

/* Return the list of directory servers
 * each front end hangs on to the list
 */
XP_List * FE_GetDirServers(void);

/* Since the strings in DIR_Server are allocated, we have bottleneck
 * routines to help with memory mgmt
 */
int		DIR_CopyServer (DIR_Server *in, DIR_Server **out);
int		DIR_DeleteServer (DIR_Server *);
int		DIR_InitServer (DIR_Server *);

int		DIR_ValidateServer (DIR_Server *);
XP_Bool	DIR_AreServersSame (DIR_Server *first, DIR_Server *second);
int		DIR_DeleteServerList(XP_List *wholeList);

int		DIR_GetLdapServers (XP_List *wholeList, XP_List *subList);
int		DIR_ReorderLdapServers (XP_List *wholeList);

/* These two routines should be called to initialize and save 
 * directory preferences from the XP Java Script preferences
 */
int		DIR_GetServerPreferences (XP_List **list, const char* pabFile);
int		DIR_SaveServerPreferences (XP_List *wholeList);

/* This routine will clean up files for deleted directories */
int		DIR_CleanUpServerPreferences(XP_List *deletedList);


#if 1 

/* Returns a pointer into the list (not allocated, so don't free) */
int		DIR_GetPersonalAddressBook (XP_List *wholeList, DIR_Server **pab);
int		DIR_GetComposeNameCompletionAddressBook (XP_List *wholeList, DIR_Server **cab);

#else

/* Returns an allocated list of all personal address books, excluding
 * LDAP directories, replicated directories, and CABs
 */
XP_List *DIR_GetPersonalAddressBooks (XP_List *wholeList);
XP_List *DIR_GetAddressBooksForCompletion (XP_List *wholeList);

#endif

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
const char  *DIR_GetTokenSeparators (DIR_Server *server);
XP_Bool	     DIR_RepeatFilterForTokens (DIR_Server *server, const char *filter);
XP_Bool	     DIR_SubstStarsForSpaces (DIR_Server *server, const char *filter);
XP_Bool      DIR_UseCustomAttribute (DIR_Server *server, DIR_AttributeId id);

XP_Bool      DIR_IsDnAttribute (DIR_Server *s, const char *attr);
XP_Bool      DIR_IsAttributeExcludedFromHtml (DIR_Server *s, const char *attr);

int DIR_AttributeNameToId (const char *attrName, DIR_AttributeId *id);

/* APIs for authentication */
void		DIR_SetAuthDN (DIR_Server *s, const char *dn);
void		DIR_SetPassword (DIR_Server *s, const char *password);

/* APIs for unescaping LDAP special chars */
char   *DIR_Unescape (const char *src, XP_Bool makeHtml);
XP_Bool DIR_IsEscapedAttribute (DIR_Server *s, const char *attrib);

/* APIs for replication */
int DIR_ValidateRootDSE (DIR_Server *s, int32 gen, int32 first, int32 last);

/* API for building a URL */
char *DIR_BuildUrl (DIR_Server *s, const char *dn, XP_Bool forAddToAB);

/* Flags manipulation */

#define DIR_AUTO_COMPLETE_ENABLED          0x00000001
#define DIR_ENABLE_AUTH                    0x00000002
#define DIR_SAVE_PASSWORD                  0x00000004
#define DIR_UTF8_DISABLED                  0x00000008  /* not used by the FEs */
#define DIR_IS_SECURE                      0x00000010
#define DIR_SAVE_RESULTS                   0x00000020  /* not used by the FEs */
#define DIR_EFFICIENT_WILDCARDS            0x00000040  /* not used by the FEs */
#define DIR_LDAPV3_SUPPORTED               0x00000080  /* not used by the FEs */
#define DIR_LDAPV3_NOT_SUPPORTED           0x00000100  /* not used by the FEs */
#define DIR_VIRTUAL_LISTBOX_SUPPORTED      0x00000200  /* not used by the FEs */
#define DIR_VIRTUAL_LISTBOX_NOT_SUPPORTED  0x00000400  /* not used by the FEs */

void DIR_SetAutoCompleteEnabled (XP_List *list, DIR_Server *server, XP_Bool onOrOff);

XP_Bool DIR_TestFlag  (DIR_Server *server, uint32 flag);
void    DIR_SetFlag   (DIR_Server *server, uint32 flag);
void    DIR_ClearFlag (DIR_Server *server, uint32 flag);
void    DIR_ForceFlag (DIR_Server *server, uint32 flag, XP_Bool forceOnOrOff);

char *DIR_ConvertToServerCharSet   (DIR_Server *s, char *src, int16 srcCsid);
char *DIR_ConvertFromServerCharSet (DIR_Server *s, char *src, int16 dstCsid);

#ifdef MOZ_LDAP

/* Does the LDAP client lib work for SSL */
#include "ldap.h"
int DIR_SetupSecureConnection (LDAP *ld);

#endif /* MOZ_LDAP */

XP_END_PROTOS


#endif /* dirprefs.h */
