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

/* dirprefs.c -- directory server preferences */
#include "rosetta.h"
#include "xp.h"

#include "xp_mcom.h"
#include "xpgetstr.h"
#include "dirprefs.h"
#include "prefapi.h"
#include "libi18n.h"
#ifdef MOZ_LDAP
	#include "ldap.h"
#else
	#define LDAP_PORT 389
	#define LDAPS_PORT 636
#endif

#ifndef MOZ_MAIL_NEWS

int DIR_GetLdapServers(XP_List *wholeList, XP_List *subList)
{
  return -1;
}

const char **DIR_GetAttributeStrings (DIR_Server *server, DIR_AttributeId id)
{
  return 0 ;
}

const char *DIR_GetFirstAttributeString (DIR_Server *server, DIR_AttributeId id)
{
  return 0 ;
}

#else

#ifndef MOZADDRSTANDALONE

extern int MK_OUT_OF_MEMORY;
extern int MK_ADDR_PAB;

extern int MK_LDAP_COMMON_NAME;
extern int MK_LDAP_GIVEN_NAME;
extern int MK_LDAP_SURNAME;
extern int MK_LDAP_EMAIL_ADDRESS; 
extern int MK_LDAP_PHONE_NUMBER;
extern int MK_LDAP_ORGANIZATION; 
extern int MK_LDAP_ORG_UNIT; 
extern int MK_LDAP_LOCALITY;
extern int MK_LDAP_STREET;
extern int MK_LDAP_CUSTOM1; 
extern int MK_LDAP_CUSTOM2;
extern int MK_LDAP_CUSTOM3; 
extern int MK_LDAP_CUSTOM4;
extern int MK_LDAP_CUSTOM5;
extern int MK_LDAP_DESCRIPTION;   
extern int MK_LDAP_EMPLOYEE_TYPE; 
extern int MK_LDAP_FAX_NUMBER;    
extern int MK_LDAP_MANAGER;       
extern int MK_LDAP_OBJECT_CLASS;       
extern int MK_LDAP_POSTAL_ADDRESS;
extern int MK_LDAP_POSTAL_CODE;   
extern int MK_LDAP_SECRETARY;
extern int MK_LDAP_TITLE; 
extern int MK_LDAP_CAR_LICENSE;
extern int MK_LDAP_BUSINESS_CAT;
extern int MK_LDAP_DEPT_NUMBER;
extern int MK_LDAP_REPL_CANT_SYNC_REPLICA;

#else

#define MK_OUT_OF_MEMORY -1;

#endif /* !MOZADDRSTANDALONE */

/*****************************************************************************
 * Private structs and stuff
 */

/* DIR_Server.flags */
#define DIR_AUTO_COMPLETE_ENABLED 0x00000001
#define DIR_ENABLE_AUTH           0x00000002
#define DIR_SAVE_PASSWORD         0x00000004
#define DIR_DEFEAT_UTF8           0x00000008
#define DIR_IS_SECURE             0x00000010
#define DIR_SAVE_RESULTS          0x00000020
#define DIR_EFFICIENT_WILDCARDS   0x00000040

/* DIR_Server.customAttributes is a list of DIR_Attribute structures */
typedef struct DIR_Attribute
{
	DIR_AttributeId id;
	char *prettyName;
	char **attrNames;
} DIR_Attribute;

/* Our internal view of a default attribute is a resourceId for the pretty
 * name and the real attribute name. These are catenated to create a string
 * of the form "Pretty Name:attrname"
 */
typedef struct DIR_DefaultAttribute
{
	DIR_AttributeId id;
	int resourceId;
	char *name;
} DIR_DefaultAttribute;

/* DIR_Filter.flags */
#define DIR_F_SUBST_STARS_FOR_SPACES   0x00000001
#define DIR_F_REPEAT_FILTER_FOR_TOKENS 0x00000002

/* DIR_Server.filters is a list of DIR_Filter structures */
typedef struct DIR_Filter
{
	char *string;
	uint32 flags;
} DIR_Filter;

/* Default settings for site-configurable prefs */
#define kDefaultTokenSeps " ,."
#define kDefaultSubstStarsForSpaces TRUE
#define kDefaultRepeatFilterForTokens TRUE
#define kDefaultEfficientWildcards TRUE
#define kDefaultFilter "(cn=*%s*)"
#define kDefaultEfficientFilter "(|(givenname=%s)(sn=%s))"
#define kDefaultStopOnHit TRUE
#define kDefaultMaxHits 100
#define kDefaultIsOffline TRUE
#define kDefaultEnableAuth FALSE
#define kDefaultSavePassword FALSE
#define kDefaultUtf8Disabled FALSE
#define kDefaultLdapPublicDirectory FALSE

#define kDefaultAutoCompleteEnabled FALSE
#define kDefaultAutoCompleteStyle acsGivenAndSurname

#define kDefaultReplicaEnabled FALSE
#define kDefaultReplicaFileName NULL
#define kDefaultReplicaDataVersion NULL
#define kDefaultReplicaDescription NULL
#define kDefaultReplicaChangeNumber -1
#define kDefaultReplicaFilter "(objectclass=*)"
#define kDefaultReplicaExcludedAttributes NULL

#define kLdapServersPrefName "network.hosts.ldap_servers"

const char *DIR_GetAttributeName (DIR_Server *server, DIR_AttributeId id);
static DIR_DefaultAttribute *DIR_GetDefaultAttribute (DIR_AttributeId id);
void DIR_SetFileName(char** filename, const char* leafName);


/*****************************************************************************
 * Functions for creating the new back end managed DIR_Server list.
 */

static XP_List * dir_ServerList = NULL;

XP_List * DIR_GetDirServers()
{
	const char * msg = "abook.nab";
	if (!dir_ServerList)
	{
		/* we need to build the DIR_Server list */
		dir_ServerList = XP_ListNew();

		PREF_SetDefaultCharPref("browser.addressbook_location",msg);

		DIR_GetServerPreferences (&dir_ServerList, msg);
/*		char * ldapPref = PR_smprintf("ldap_%d.end_of_directories", kCurrentListVersion);
		if (ldapPref)
			PREF_RegisterCallback(ldapPref, DirServerListChanged, NULL);
		XP_FREEIF (ldapPref); */
	}
	return dir_ServerList;
}

int DIR_ShutDown()  /* FEs should call this when the app is shutting down. It frees all DIR_Servers regardless of ref count values! */
{
	int i = 1;
	if (dir_ServerList)
	{
		for (i = 1; i <= XP_ListCount(dir_ServerList); i++)
			DIR_DeleteServer(XP_ListGetObjectNum (dir_ServerList, i));
		XP_ListDestroy (dir_ServerList);
	}

	return 0;
}

int DIR_DecrementServerRefCount (DIR_Server *server)
{
	XP_ASSERT(server);
	if (server && --server->refCount <= 0)
		return DIR_DeleteServer(server);
	return 0;
}

int DIR_IncrementServerRefCount (DIR_Server *server)
{
	XP_ASSERT(server);
	if (server)
		server->refCount++;
	return 0;
}

/*****************************************************************************
 * Functions for creating DIR_Servers
 */

int DIR_InitServer (DIR_Server *server)
{
	XP_ASSERT(server);
	if (server)
	{
		XP_BZERO(server, sizeof(DIR_Server));
		server->saveResults = TRUE;
		server->efficientWildcards = kDefaultEfficientWildcards;
		server->port = LDAP_PORT;
		server->maxHits = kDefaultMaxHits;
		server->isOffline = kDefaultIsOffline;
		server->refCount = 1;
	}
	return 0;
}


/*****************************************************************************
 * Functions for cloning DIR_Servers
 */

static DIR_Attribute *DIR_CopyAttribute (DIR_Attribute *inAttribute)
{
	DIR_Attribute *outAttribute = (DIR_Attribute*) XP_ALLOC(sizeof(DIR_Attribute));
	if (outAttribute)
	{
		int count = 0;
		outAttribute->id = inAttribute->id;
		outAttribute->prettyName = XP_STRDUP(inAttribute->prettyName);
		while (inAttribute->attrNames[count])
			count++;
		outAttribute->attrNames = (char**) XP_ALLOC((count + 1) * sizeof(char*));
		if (outAttribute->attrNames)
		{
			int i;
			for (i = 0; i < count; i++)
				outAttribute->attrNames[i] = XP_STRDUP(inAttribute->attrNames[i]);
			outAttribute->attrNames[i] = NULL;
		}
	}
	return outAttribute;
}


static DIR_Filter *DIR_CopyFilter (DIR_Filter *inFilter)
{
	DIR_Filter *outFilter = (DIR_Filter*) XP_ALLOC(sizeof(DIR_Filter));
	if (outFilter)
	{
		outFilter->flags = inFilter->flags;
		outFilter->string = XP_STRDUP(inFilter->string);
	}
	return outFilter;
}


static int dir_CopyTokenList (char **inList, int inCount, char ***outList, int *outCount)
{
	int status = 0;
	if (0 != inCount && NULL != inList)
	{
		*outList = (char**) XP_ALLOC(inCount * sizeof(char*));
		if (*outList)
		{
			int i;
			for (i = 0; i < inCount; i++)
				(*outList)[i] = XP_STRDUP (inList[i]);
			*outCount = inCount;
		}
		else
			status = MK_OUT_OF_MEMORY;
	}
	return status;
}


static DIR_ReplicationInfo *dir_CopyReplicationInfo (DIR_ReplicationInfo *inInfo)
{
	DIR_ReplicationInfo *outInfo = (DIR_ReplicationInfo*) XP_CALLOC (1, sizeof(DIR_ReplicationInfo));
	if (outInfo)
	{
		outInfo->lastChangeNumber = inInfo->lastChangeNumber;
		if (inInfo->description)
			outInfo->description = XP_STRDUP (inInfo->description);
		if (inInfo->fileName)
			outInfo->fileName = XP_STRDUP (inInfo->fileName);
		if (inInfo->dataVersion)
			outInfo->dataVersion = XP_STRDUP (inInfo->dataVersion);
		if (inInfo->filter)
			outInfo->filter = XP_STRDUP (inInfo->filter);
		dir_CopyTokenList (inInfo->excludedAttributes, inInfo->excludedAttributesCount,
			&outInfo->excludedAttributes, &outInfo->excludedAttributesCount);
	}
	return outInfo;
}

int DIR_CopyServer (DIR_Server *in, DIR_Server **out)
{
	int err = 0;
	if (in) {
		*out = XP_ALLOC(sizeof(DIR_Server));
		if (*out)
		{
			XP_BZERO (*out, sizeof(DIR_Server));

			if (in->description)
			{
				(*out)->description = XP_STRDUP(in->description);
				if (!(*out)->description)
					err = MK_OUT_OF_MEMORY;
			}

			if (in->serverName)
			{
				(*out)->serverName = XP_STRDUP(in->serverName);
				if (!(*out)->serverName)
					err = MK_OUT_OF_MEMORY;
			}

			if (in->searchBase)
			{
				(*out)->searchBase = XP_STRDUP(in->searchBase);
				if (!(*out)->searchBase)
					err = MK_OUT_OF_MEMORY;
			}

			if (in->fileName)
			{
				(*out)->fileName = XP_STRDUP(in->fileName);
				if (!(*out)->fileName)
					err = MK_OUT_OF_MEMORY;
 			}

			if (in->prefId)
			{
				(*out)->prefId = XP_STRDUP(in->prefId);
				if (!(*out)->prefId)
					err = MK_OUT_OF_MEMORY;
			}

			(*out)->port = in->port;
			(*out)->maxHits = in->maxHits;
			(*out)->isSecure = in->isSecure;
			(*out)->saveResults = in->saveResults;
			(*out)->isOffline = in->isOffline;
			(*out)->efficientWildcards = in->efficientWildcards;
			(*out)->dirType = in->dirType;

			(*out)->flags = in->flags;
			
			(*out)->enableAuth = in->enableAuth;
			(*out)->savePassword = in->savePassword;
			if (in->authDn)
			{
				(*out)->authDn = XP_STRDUP (in->authDn);
				if (!(*out)->authDn)
					err = MK_OUT_OF_MEMORY;
			}
			if (in->password)
			{
				(*out)->password = XP_STRDUP (in->password);
				if (!(*out)->password)
					err = MK_OUT_OF_MEMORY;
			}

			if (in->customAttributes)
			{
				(*out)->customAttributes = XP_ListNew();
				if ((*out)->customAttributes)
				{
					XP_List *list = in->customAttributes;
					DIR_Attribute *attribute = NULL;

					while ((attribute = XP_ListNextObject (list)) != NULL)
					{
						DIR_Attribute *outAttr = DIR_CopyAttribute (attribute);
						if (outAttr)
							XP_ListAddObject ((*out)->customAttributes, outAttr);
						else
							err = MK_OUT_OF_MEMORY;
 					}
				}
				else
					err = MK_OUT_OF_MEMORY;
			}

			if (in->customFilters)
			{
				(*out)->customFilters = XP_ListNew();
				if ((*out)->customFilters)
				{
					XP_List *list = in->customFilters;
					DIR_Filter *filter = NULL;

					while ((filter = XP_ListNextObject (list)) != NULL)
					{
						DIR_Filter *outFilter = DIR_CopyFilter (filter);
						if (outFilter)
							XP_ListAddObject ((*out)->customFilters, outFilter);
						else
							err = MK_OUT_OF_MEMORY;
					}
				}
				else
					err = MK_OUT_OF_MEMORY;
			}

			if (in->replInfo)
				(*out)->replInfo = dir_CopyReplicationInfo (in->replInfo);

			if (in->basicSearchAttributesCount > 0)
			{
				int bsaLength = in->basicSearchAttributesCount * sizeof(DIR_AttributeId);
				(*out)->basicSearchAttributes = (DIR_AttributeId*) XP_ALLOC(bsaLength);
				if ((*out)->basicSearchAttributes)
				{
					XP_MEMCPY ((*out)->basicSearchAttributes, in->basicSearchAttributes, bsaLength);
					(*out)->basicSearchAttributesCount = in->basicSearchAttributesCount;
				}
			}

			dir_CopyTokenList (in->dnAttributes, in->dnAttributesCount,
				&(*out)->dnAttributes, &(*out)->dnAttributesCount);

			dir_CopyTokenList (in->suppressedAttributes, in->suppressedAttributesCount,
				&(*out)->suppressedAttributes, &(*out)->suppressedAttributesCount);

			if (in->customDisplayUrl)
				(*out)->customDisplayUrl = XP_STRDUP (in->customDisplayUrl);
			if (in->searchPairList)
				(*out)->searchPairList = XP_STRDUP (in->searchPairList);

			(*out)->autoCompleteStyle = in->autoCompleteStyle;

			(*out)->refCount = 1;
		}
		else {
			err = MK_OUT_OF_MEMORY;
			(*out) = NULL;
		}
	}
	else {
		XP_ASSERT (0);
		err = -1;
		(*out) = NULL;
	}

	return err;
}

/*****************************************************************************
 * Function for comparing DIR_Servers 
 */

XP_Bool	DIR_AreServersSame (DIR_Server *first, DIR_Server *second)
{
	/* This function used to be written to assume that we only had one PAB so it
	   only checked the server type for PABs. If both were PABDirectories, then 
	   it returned TRUE. Now that we support multiple address books, we need to
	   check type & file name for address books to test if they are the same */

	XP_Bool result = FALSE;

	if (first && second) 
	{
		/* assume for right now one personal address book type where offline is false */
		if ((first->dirType == PABDirectory) && (second->dirType == PABDirectory)) {
			if ((first->isOffline == FALSE) && (second->isOffline == FALSE))  /* are they both really address books? */
			{
				XP_ASSERT(first->fileName && second->fileName);
				if (first->fileName && second->fileName)
					if (XP_STRCASECMP(first->fileName, second->fileName) == 0)
						return TRUE;

				return FALSE;
			}
			else {
				XP_ASSERT (first->serverName && second->serverName);
				if (first->serverName && second->serverName)
				{
					if (XP_STRCASECMP (first->serverName, second->serverName) == 0) 
					{
						if (first->port == second->port) 
						{
							/* allow for a null search base */
							if (first->searchBase == NULL && second->searchBase == NULL)
								return TRUE;
							/* otherwise check the strings */
							if (first->searchBase && second->searchBase)
							{
								if (XP_STRCASECMP (first->searchBase, second->searchBase) == 0) 
								{
										return TRUE;
								}
								else
									return FALSE;
							}
							else
								return FALSE;
						}
					}
					else
						return FALSE;
				}
			}
		}

		if (first->dirType != second->dirType)
			return FALSE;

		/* otherwise check for ldap servers */
		XP_ASSERT (first->serverName && second->serverName);
		if (first->serverName && second->serverName)
		{
			if (!XP_STRCASECMP (first->serverName, second->serverName)) 
			{
				if (first->port == second->port) 
				{
					/* allow for a null search base */
					if (first->searchBase == NULL && second->searchBase == NULL)
						result = TRUE;
					/* otherwise check the strings */
					if (first->searchBase && second->searchBase)
					{
						if (!XP_STRCASECMP (first->searchBase, second->searchBase)) 
						{
								result = TRUE;
						}
					}
				}
			}
		}
	}
	return result;
}


/*****************************************************************************
 * Functions for destroying DIR_Servers 
 */

static void dir_DeleteTokenList (char **tokenList, int tokenListCount)
{
	int tokenIdx;
	for (tokenIdx = 0; tokenIdx < tokenListCount; tokenIdx++)
		XP_FREE(tokenList[tokenIdx]);
	XP_FREE(tokenList);
}


static int DIR_DeleteFilter (DIR_Filter *filter)
{
	if (filter->string)
		XP_FREE(filter->string);
	XP_FREE(filter);
	return 0;
}


static int DIR_DeleteAttribute (DIR_Attribute *attribute)
{
	int i = 0;
	if (attribute->prettyName)
		XP_FREE(attribute->prettyName);
	if (attribute->attrNames)
	{
		while (attribute->attrNames[i])
			XP_FREE((char**)attribute->attrNames[i++]);
		XP_FREE(attribute->attrNames);
	}
	XP_FREE(attribute);
	return 0;
}


static void dir_DeleteReplicationInfo (DIR_Server *server)
{
	DIR_ReplicationInfo *info = NULL;
	if (server && (info = server->replInfo) != NULL)
	{
		dir_DeleteTokenList (info->excludedAttributes, info->excludedAttributesCount);
		
		XP_FREEIF(info->description);
		XP_FREEIF(info->fileName);
		XP_FREEIF(info->dataVersion);
		XP_FREEIF(info->filter);
		XP_FREE(info);
	}
}

/* When the back end manages the server list, deleting a server just decrements
 * its ref count, in the old world, we actually delete the server
 */
int DIR_DeleteServer (DIR_Server *server)
{
	if (server)
	{
		int i;

		XP_FREEIF (server->description);
		XP_FREEIF (server->serverName);
		XP_FREEIF (server->searchBase);
		XP_FREEIF (server->fileName);
		XP_FREEIF (server->lastSearchString);
		XP_FREEIF (server->tokenSeps);
		XP_FREEIF (server->authDn);
		XP_FREEIF (server->password);
		XP_FREEIF (server->prefId);

		if (server->customFilters)
		{
			for (i = 1; i <= XP_ListCount(server->customFilters); i++)
				DIR_DeleteFilter ((DIR_Filter*) XP_ListGetObjectNum (server->customFilters, i));
			XP_ListDestroy (server->customFilters);
		}

		if (server->customAttributes)
		{
			XP_List *list = server->customAttributes;
			DIR_Attribute *walkAttrStruct = NULL;
			while ((walkAttrStruct = XP_ListNextObject(list)) != NULL)
				DIR_DeleteAttribute (walkAttrStruct);
			XP_ListDestroy (server->customAttributes);
		}

		if (server->suppressedAttributes)
			dir_DeleteTokenList (server->suppressedAttributes, server->suppressedAttributesCount);
		if (server->dnAttributes)
			dir_DeleteTokenList (server->dnAttributes, server->dnAttributesCount);
		XP_FREEIF (server->basicSearchAttributes);
		if (server->replInfo)
			dir_DeleteReplicationInfo (server);

		XP_FREEIF (server->customDisplayUrl);
		XP_FREEIF (server->searchPairList);

		XP_FREE (server);
	}
	return 0;
}

int DIR_DeleteServerList(XP_List *wholeList)
{
	int i;
	if (wholeList)
	{
		for (i = 1; i <= XP_ListCount(wholeList); i++)
			DIR_DeleteServer(XP_ListGetObjectNum (wholeList, i));
		XP_ListDestroy (wholeList);
	}
	return 0;
}


int DIR_CleanUpServerPreferences(XP_List *deletedList)
{
	/* In the new world order of DIR_Servers it has been decreed that to clean
	 * up a server you should set its DIR_CLEAR_SERVER flag.  Then, release
	 * your ref count on the list or servers (if you have one)
	 */
	XP_ASSERT(FALSE);
	return 0;
}


/*****************************************************************************
 * Functions for retrieving subsets of the DIR_Server list 
 */

static int DIR_GetHtmlServers(XP_List *wholeList, XP_List *subList)
{
	int i;
	if (wholeList && subList)
	{
		for (i = 1; i <= XP_ListCount(wholeList); i++)
		{
			DIR_Server *s = (DIR_Server*) XP_ListGetObjectNum (wholeList, i);
			if (HTMLDirectory == s->dirType)
				XP_ListAddObjectToEnd (subList, s);
		}
		return 0;
	}
	return -1;
}

int DIR_GetPersonalAddressBooks (XP_List *wholeList, XP_List * subList)
{
	int i;
	if (wholeList && subList)
	{
		for (i = 1; i <= XP_ListCount(wholeList); i++)
		{
			DIR_Server *s = (DIR_Server*) XP_ListGetObjectNum (wholeList, i);
			if (PABDirectory == s->dirType)
				XP_ListAddObjectToEnd (subList, s);
		}
		return 0;
	}
	return -1;
}

int DIR_GetLdapServers(XP_List *wholeList, XP_List *subList)
{
	int i;
	if (wholeList && subList)
	{
		for (i = 1; i <= XP_ListCount(wholeList); i++)
		{
			DIR_Server *s = (DIR_Server*) XP_ListGetObjectNum (wholeList, i);
			if (LDAPDirectory == s->dirType)
				XP_ListAddObjectToEnd (subList, s);
		}
		return 0;
	}
	return -1;
}

int	DIR_ReorderLdapServers(XP_List *wholeList)
{
	int status = 0;
	int length = 0;
	DIR_Server *s = NULL;
	char *prefValue = NULL;

	XP_List *walkList = wholeList;
	while (s = (DIR_Server*) XP_ListNextObject (walkList))
	{
		length += XP_STRLEN (s->prefId) + 2; /* +2 for ", " */
	}

	prefValue = (char*) XP_ALLOC (length + 1); /* +1 for null term */
	if (prefValue)
	{
		*prefValue = '\0';
		walkList = wholeList;
		while (s = (DIR_Server*) XP_ListNextObject (walkList))
		{
			XP_STRCAT (prefValue, s->prefId);
			XP_STRCAT (prefValue, ", ");
		}
		if (PREF_SetCharPref (kLdapServersPrefName, prefValue) < 0)
			status = -1; /* not sure how to make a pref error into an mk error */
		XP_FREE(prefValue);
	}
	else
		status = MK_OUT_OF_MEMORY;

	return status;
}

int DIR_GetPersonalAddressBook(XP_List *wholeList, DIR_Server **pab)
{
	int i;
	if (wholeList && pab)
	{
		*pab = NULL;
		for (i = 1; i <= XP_ListCount(wholeList); i++)
		{
			DIR_Server *s = (DIR_Server*) XP_ListGetObjectNum (wholeList, i);
			if ((PABDirectory == s->dirType) && (FALSE == s->isOffline))
			{
				if (s->serverName) {
					if (XP_STRLEN (s->serverName) == 0) {
						*pab = s;
						return 0;
					}
				}
				if (s->serverName == NULL) {
						*pab = s;
						return 0;
				}
			}
		}
	}
	return -1;
}

int DIR_GetComposeNameCompletionAddressBook(XP_List *wholeList, DIR_Server **cab)
{
	int i;
	if (wholeList && cab)
	{
		*cab = NULL;
		for (i = 1; i <= XP_ListCount(wholeList); i++)
		{
			DIR_Server *s = (DIR_Server*) XP_ListGetObjectNum (wholeList, i);
			if (PABDirectory == s->dirType)
			{
				*cab = s;
				return 0;
			}
		}
	}
	return -1;
}


/*****************************************************************************
 * Functions for managing JavaScript prefs for the DIR_Servers 
 */

#if !defined(MOZADDRSTANDALONE)

static char *DIR_GetStringPref(const char *prefRoot, const char *prefLeaf, char *scratch, const char *defaultValue)
{
	int valueLength = 0;
	char *value = NULL;
	XP_STRCPY(scratch, prefRoot);
	XP_STRCAT (scratch, prefLeaf);

	if (PREF_NOERROR == PREF_CopyCharPref(scratch, &value))
	{
		/* unfortunately, there may be some prefs out there which look like this */
		if (!XP_STRCMP(value, "(null)")) 
			value = defaultValue ? XP_STRDUP(defaultValue) : NULL;
	}
	else
		value = defaultValue ? XP_STRDUP(defaultValue) : NULL;
	return value;
}


static int32 DIR_GetIntPref(const char *prefRoot, const char *prefLeaf, char *scratch, int32 defaultValue)
{
	int32 value;
	XP_STRCPY(scratch, prefRoot);
	if (PREF_NOERROR != PREF_GetIntPref(XP_STRCAT (scratch, prefLeaf), &value))
		value = defaultValue;
	return value;
}


static XP_Bool DIR_GetBoolPref(const char *prefRoot, const char *prefLeaf, char *scratch, XP_Bool defaultValue)
{
	XP_Bool value;
	XP_STRCPY(scratch, prefRoot);
	if (PREF_NOERROR != PREF_GetBoolPref(XP_STRCAT (scratch, prefLeaf), &value))
		value = defaultValue;
	return value;
}


int DIR_AttributeNameToId(const char *attrName, DIR_AttributeId *id)
{
	int status = 0;

	switch (attrName[0])
	{
	case 'a':
		if (!XP_STRCASECMP(attrName, "auth"))
			*id = auth;
		else
			status = -1;
		break;

	case 'c' :
		if (!XP_STRCASECMP(attrName, "cn"))
			*id = cn;
		else if (!XP_STRNCASECMP(attrName, "custom", 6))
		{
			switch (attrName[6])
			{
			case '1': *id = custom1; break;
			case '2': *id = custom2; break;
			case '3': *id = custom3; break;
			case '4': *id = custom4; break;
			case '5': *id = custom5; break;
			default: status = -1; 
			}
		}
		else
			status = -1;
		break;
	case 'g':
		if (!XP_STRCASECMP(attrName, "givenname"))
			*id = givenname; 
		else
			status = -1;
		break;
	case 'l':
		if (!XP_STRCASECMP(attrName, "l"))
			*id = l;
		else
			status = -1;
		break;
	case 'm':
		if (!XP_STRCASECMP(attrName, "mail"))
			*id = mail;
		else
			status = -1;
		break;
	case 'o':
		if (!XP_STRCASECMP(attrName, "o"))
			*id = o;
		else if (!XP_STRCASECMP(attrName, "ou"))
			*id = ou;
		else
			status = -1;
		break;
	case 's': 
		if (!XP_STRCASECMP(attrName, "street"))
			*id = street;
		else if (!XP_STRCASECMP(attrName, "sn"))
			*id = sn;
		else
			status = -1;
		break;
	case 't':
		if (!XP_STRCASECMP(attrName, "telephonenumber"))
			*id = telephonenumber;
		else
			status = -1;
		break;
	default:
		status = -1;
	}

	return status;
}


static int DIR_AddCustomAttribute(DIR_Server *server, const char *attrName, char *jsAttr)
{
	int status = 0;
	char *jsCompleteAttr = NULL;
	char *jsAttrForTokenizing = jsAttr;

	DIR_AttributeId id;
	status = DIR_AttributeNameToId (attrName, &id);

	/* If the string they gave us doesn't have a ':' in it, assume it's one or more
	 * attributes without a pretty name. So find the default pretty name, and generate
	 * a "complete" string to use for tokenizing.
	 */
	if (status == 0 && !XP_STRCHR(jsAttr, ':'))
	{
		const char *defaultPrettyName = DIR_GetAttributeName (server, id);
		if (defaultPrettyName)
		{
			jsCompleteAttr = PR_smprintf ("%s:%s", defaultPrettyName, jsAttr);
			if (jsCompleteAttr)
				jsAttrForTokenizing = jsCompleteAttr;
			else
				status = MK_OUT_OF_MEMORY;
		}
	}

	if (status == 0)
	{
		char *scratchAttr = XP_STRDUP(jsAttrForTokenizing);
		DIR_Attribute *attrStruct = (DIR_Attribute*) XP_ALLOC(sizeof(DIR_Attribute));
		if (!server->customAttributes)
			server->customAttributes = XP_ListNew();

		if (attrStruct && server->customAttributes && scratchAttr)
		{
			char *attrToken = NULL;
			int attrCount = 0;

			XP_BZERO(attrStruct, sizeof(DIR_Attribute));

			/* Try to pull out the pretty name into the struct */
			attrStruct->id = id;
			attrStruct->prettyName = XP_STRDUP(XP_STRTOK(scratchAttr, ":")); 

			/* Count up the attribute names */
			while ((attrToken = XP_STRTOK(NULL, ", ")) != NULL)
				attrCount++;

			/* Pull the attribute names into the struct */
			XP_STRCPY(scratchAttr, jsAttrForTokenizing);
			XP_STRTOK(scratchAttr, ":"); 
			attrStruct->attrNames = (char**) XP_ALLOC((attrCount + 1) * sizeof(char*));
			if (attrStruct->attrNames)
			{
				int i = 0;
				while ((attrToken = XP_STRTOK(NULL, ", ")) != NULL)
					attrStruct->attrNames[i++] = XP_STRDUP(attrToken);
				attrStruct->attrNames[i] = NULL; /* null-terminate the array */
			}

			if (status == 0)
				XP_ListAddObject (server->customAttributes, attrStruct);
			else
				DIR_DeleteAttribute (attrStruct);

			XP_FREE(scratchAttr);
		}
		else 
			status = MK_OUT_OF_MEMORY;
	}

	if (jsCompleteAttr)
		XP_FREE(jsCompleteAttr);

	return status;
}

static int dir_CreateTokenListFromWholePref(const char *pref, char ***outList, int *outCount)
{
	int result = 0;
	char *commaSeparatedList = NULL;

	if (PREF_NOERROR == PREF_CopyCharPref(pref, &commaSeparatedList) && commaSeparatedList)
	{
		char *tmpList = commaSeparatedList;
		*outCount = 1;
		while (*tmpList)
			if (*tmpList++ == ',')
				(*outCount)++;

		*outList = (char**) XP_ALLOC(*outCount * sizeof(char*));
		if (*outList)
		{
			int i;
			char *token = XP_STRTOK(commaSeparatedList, ", ");
			for (i = 0; i < *outCount; i++)
			{
				(*outList)[i] = XP_STRDUP(token);
				token = XP_STRTOK(NULL, ", ");
			}
		}
		else
			result = MK_OUT_OF_MEMORY;

		XP_FREE (commaSeparatedList);
	}
	else
		result = -1;
	return result;
}


static int dir_CreateTokenListFromPref(const char *prefBase, const char *prefLeaf, char *scratch, char ***outList, int *outCount)
{
	XP_STRCPY (scratch, prefBase);
	XP_STRCAT (scratch, prefLeaf);

	return dir_CreateTokenListFromWholePref(scratch, outList, outCount);
}


static int dir_ConvertTokenListToIdList(char **tokenList, int tokenCount, DIR_AttributeId **outList)
{
	*outList = (DIR_AttributeId*) XP_ALLOC(sizeof(DIR_AttributeId) * tokenCount);
	if (*outList)
	{
		int i;
		for (i = 0; i < tokenCount; i++)
			DIR_AttributeNameToId (tokenList[i], &(*outList)[i]);
	}
	else
		return MK_OUT_OF_MEMORY;
	return 0;
}


static void dir_GetReplicationInfo(const char *prefName, DIR_Server *server, char *scratch)
{
	char *replPrefName;
	XP_Bool arePrefsValid = FALSE;

	XP_ASSERT (server->replInfo == NULL);

	replPrefName = (char *) XP_ALLOC(128);
	server->replInfo = (DIR_ReplicationInfo *) XP_CALLOC (1, sizeof (DIR_ReplicationInfo));
	if (server->replInfo && replPrefName)
	{
		XP_STRCPY(replPrefName, prefName);
		XP_STRCAT(replPrefName, "replication.");

		if (DIR_GetBoolPref (replPrefName, "enabled", scratch, kDefaultReplicaEnabled))
			DIR_SetFlag (server, DIR_REPLICATION_ENABLED);

		server->replInfo->fileName = DIR_GetStringPref (replPrefName, "fileName", scratch, kDefaultReplicaFileName);
		server->replInfo->dataVersion = DIR_GetStringPref (replPrefName, "dataVersion", scratch, kDefaultReplicaDataVersion);

		/* The file name and data version must be set or we ignore all of the
		 * replication prefs.
		 */
		if (server->replInfo->fileName && server->replInfo->dataVersion)
		{
			dir_CreateTokenListFromPref (replPrefName, "excludedAttributes", scratch, &server->replInfo->excludedAttributes, 
			                             &server->replInfo->excludedAttributesCount);

			server->replInfo->description = DIR_GetStringPref (replPrefName, "description", scratch, kDefaultReplicaDescription);
			server->replInfo->filter = DIR_GetStringPref (replPrefName, "filter", scratch, kDefaultReplicaFilter);
			server->replInfo->lastChangeNumber = DIR_GetIntPref (replPrefName, "lastChangeNumber", scratch, kDefaultReplicaChangeNumber);

			arePrefsValid = TRUE;
		}
	}

	if (!arePrefsValid)
	{
		if (server->replInfo)
		{
			XP_FREEIF(server->replInfo->fileName);
			XP_FREEIF(server->replInfo->dataVersion);
		}
		XP_FREEIF(server->replInfo);
		XP_FREEIF(replPrefName);
	}
}


/* Called at startup-time to read whatever overrides the LDAP site administrator has
 * done to the attribute names
 */
static int DIR_GetCustomAttributePrefs(const char *prefName, DIR_Server *server, char *scratch)
{
	char **tokenList = NULL;
	char *childList = NULL;
	XP_STRCPY(scratch, prefName);

	if (PREF_NOERROR == PREF_CreateChildList(XP_STRCAT(scratch, "attributes"), &childList))
	{
		if (childList && childList[0])
		{
			char *child = NULL;
			int index = 0;
			while ((child = PREF_NextChild (childList, &index)) != NULL)
			{
				char *jsValue = NULL;
				if (PREF_NOERROR == PREF_CopyCharPref (child, &jsValue))
				{
					if (jsValue && jsValue[0])
					{
						char *attrName = child + XP_STRLEN(scratch) + 1;
						DIR_AddCustomAttribute (server, attrName, jsValue);
					}
					XP_FREEIF(jsValue);
				}
			}
		}

		XP_FREEIF(childList);
	}

	/* The replicated attributes and basic search attributes can only be
	 * attributes which are in our predefined set (DIR_AttributeId) so
	 * store those in an array of IDs for more convenient access
	 */
	dir_GetReplicationInfo (prefName, server, scratch);

	if (0 == dir_CreateTokenListFromPref (prefName, "basicSearchAttributes", scratch, 
		&tokenList, &server->basicSearchAttributesCount))
	{
		dir_ConvertTokenListToIdList (tokenList, server->basicSearchAttributesCount, 
			&server->basicSearchAttributes);
		dir_DeleteTokenList (tokenList, server->basicSearchAttributesCount);
	}

	/* The DN attributes and suppressed attributes can be attributes that
	 * we've never heard of, so they're stored by name, so we can match 'em
	 * as we get 'em from the server
	 */
	dir_CreateTokenListFromPref (prefName, "html.dnAttributes", scratch, 
		&server->dnAttributes, &server->dnAttributesCount);
	dir_CreateTokenListFromPref (prefName, "html.excludedAttributes", scratch, 
		&server->suppressedAttributes, &server->suppressedAttributesCount);

	return 0;
}


/* Called at startup-time to read whatever overrides the LDAP site administrator has
 * done to the filtering logic
 */
static int DIR_GetCustomFilterPrefs(const char *prefName, DIR_Server *server, char *scratch)
{
	int status = 0;
	XP_Bool keepGoing = TRUE;
	int filterNum = 1;
	char *localScratch = XP_ALLOC(128);
	if (!localScratch)
		return MK_OUT_OF_MEMORY;

	while (keepGoing && !status)
	{
		char *childList = NULL;

		PR_snprintf (scratch, 128, "%sfilter%d", prefName, filterNum);
		if (PREF_NOERROR == PREF_CreateChildList(scratch, &childList))
		{
			if ('\0' != childList[0])
			{
				DIR_Filter *filter = (DIR_Filter*) XP_ALLOC (sizeof(DIR_Filter));
				if (filter)
				{
					XP_Bool tempBool;
					XP_BZERO(filter, sizeof(DIR_Filter));

					/* Pull per-directory filter preferences out of JS values */
					if (1 == filterNum)
					{
						server->tokenSeps = DIR_GetStringPref (prefName, "wordSeparators", localScratch, kDefaultTokenSeps);
						XP_STRCAT(scratch, ".");
					}

					/* Pull per-filter preferences out of JS values */
					filter->string = DIR_GetStringPref (scratch, "string", localScratch, 
						server->efficientWildcards ? kDefaultFilter : kDefaultEfficientFilter);
					tempBool = DIR_GetBoolPref (scratch, "repeatFilterForWords", localScratch, kDefaultRepeatFilterForTokens);
					if (tempBool)
						filter->flags |= DIR_F_REPEAT_FILTER_FOR_TOKENS;
					tempBool = DIR_GetBoolPref (scratch, "substituteStarsForSpaces", localScratch, kDefaultSubstStarsForSpaces);
					if (tempBool)
						filter->flags |= DIR_F_SUBST_STARS_FOR_SPACES;

					/* Add resulting DIR_Filter to the list */
					if (!server->customFilters)
						server->customFilters = XP_ListNew();
					if (server->customFilters)
						XP_ListAddObject (server->customFilters, filter);
					else
						status = MK_OUT_OF_MEMORY;
				}
				else
					status = MK_OUT_OF_MEMORY;
				filterNum++;
			}
			else
				keepGoing = FALSE;
			XP_FREE(childList);
		}
		else
			keepGoing = FALSE;
	}

	XP_FREE(localScratch);
	return status;
}

/* This will convert from the old preference that was a path and filename */
/* to a just a filename */
static void DIR_ConvertServerFileName(DIR_Server* pServer)
{
	char* leafName = pServer->fileName;
	char* newLeafName = NULL;
#if defined(XP_WIN) || defined(XP_OS2)
	/* This is to allow users to share the same address book.
	 * It only works if the user specify a full path filename.
	 */
	if (! XP_FileIsFullPath(leafName))
		newLeafName = XP_STRRCHR (leafName, '\\');
#else
	newLeafName = XP_STRRCHR (leafName, '/');
#endif
	pServer->fileName = newLeafName ? XP_STRDUP(newLeafName + 1) : XP_STRDUP(leafName);
	if (leafName) XP_FREE(leafName);
}

/* This will generate a correct filename and then remove the path */
void DIR_SetFileName(char** fileName, const char* leafName)
{
	char* tempName = WH_TempName(xpAddrBook, leafName);
	char* nativeName = WH_FileName(tempName, xpAddrBook);
	char* urlName = XP_PlatformFileToURL(nativeName);
#if defined(XP_WIN) || defined(XP_UNIX) || defined(XP_MAC) || defined(XP_OS2)
	char* newLeafName = XP_STRRCHR (urlName + XP_STRLEN("file://"), '/');
	(*fileName) = newLeafName ? XP_STRDUP(newLeafName + 1) : XP_STRDUP(urlName + XP_STRLEN("file://"));
#else
	(*fileName) = XP_STRDUP(urlName + XP_STRLEN("file://"));
#endif
	if (urlName) XP_FREE(urlName);
	if (nativeName) XP_FREE(nativeName);
	if (tempName) XP_FREE(tempName);
}

void DIR_SetServerFileName(DIR_Server *server, const char* leafName)
{
	DIR_SetFileName(&(server->fileName), leafName);
}

/* This will reconstruct a correct filename including the path */
void DIR_GetServerFileName(char** filename, const char* leafName)
{
#ifdef XP_MAC
	char* realLeafName;
	char* nativeName;
	char* urlName;
	if (XP_STRCHR(leafName, ':') != NULL)
		realLeafName = XP_STRRCHR(leafName, ':') + 1;	/* makes sure that leafName is not a fullpath */
	else
		realLeafName = leafName;

	nativeName = WH_FileName(realLeafName, xpAddrBookNew);
	urlName = XP_PlatformFileToURL(nativeName);
	(*filename) = XP_STRDUP(urlName + XP_STRLEN("file://"));
	if (urlName) XP_FREE(urlName);
#elif defined(XP_WIN)
	/* jefft -- Bug 73349. To allow users share same address book.
	 * This only works if user sepcified a full path name in his
	 * prefs.js
	 */ 
	char *nativeName = WH_FilePlatformName (leafName);
	char *fullnativeName;
	if (XP_FileIsFullPath(nativeName)) {
		fullnativeName = nativeName;
		nativeName = NULL;
	}
	else {
		fullnativeName = WH_FileName(nativeName, xpAddrBookNew);
	}
	(*filename) = fullnativeName;
#else
	char* nativeName = WH_FilePlatformName (leafName);
	char* fullnativeName = WH_FileName(nativeName, xpAddrBookNew);
	(*filename) = fullnativeName;
#endif
	if (nativeName) XP_FREE(nativeName);
}

static int DIR_GetPrefsFromBranch(XP_List **list, const char *pabFile, const char *branch)
{
	int32 numDirectories = 0;
	int i = 0;
	char *prefstring = NULL;
	char *tempString = NULL;
	int result = 0;
	XP_Bool hasPAB = FALSE;
	DIR_Server *pNewServer = NULL;
	char *newfilename = NULL;

	if (*list)
		DIR_DeleteServerList(*list);

	prefstring = (char *) XP_ALLOC(128);
	tempString = (char *) XP_ALLOC(256);

	(*list) = XP_ListNew();
	/* get the preference for how many directories */
	if (prefstring && tempString && (*list) && branch && *branch)
	{
		char *numberOfDirs = PR_smprintf ("%s.number_of_directories", branch);
		if (numberOfDirs)
			PREF_GetIntPref(numberOfDirs, &numDirectories);	

		for (i = 1; i <= numDirectories; i++)
		{
			pNewServer = (DIR_Server *) XP_ALLOC(sizeof(DIR_Server));
			if (pNewServer)
			{
				XP_Bool prefBool;
				int prefInt;

				DIR_InitServer(pNewServer);
				XP_SPRINTF(prefstring, "%s.directory%i.", branch, i);

				pNewServer->isSecure = DIR_GetBoolPref (prefstring, "isSecure", tempString, FALSE);
				pNewServer->saveResults = DIR_GetBoolPref (prefstring, "saveResults", tempString, TRUE);				
				pNewServer->efficientWildcards = DIR_GetBoolPref (prefstring, "efficientWildcards", tempString, TRUE);				
				pNewServer->port = DIR_GetIntPref (prefstring, "port", tempString, pNewServer->isSecure ? LDAPS_PORT : LDAP_PORT);
				if (pNewServer->port == 0)
					pNewServer->port = pNewServer->isSecure ? LDAPS_PORT : LDAP_PORT;
				pNewServer->maxHits = DIR_GetIntPref (prefstring, "maxHits", tempString, kDefaultMaxHits);
				pNewServer->description = DIR_GetStringPref (prefstring, "description", tempString, "");
				pNewServer->serverName = DIR_GetStringPref (prefstring, "serverName", tempString, "");
				pNewServer->searchBase = DIR_GetStringPref (prefstring, "searchBase", tempString, "");
				if (!XP_STRCASECMP(pNewServer->serverName, "ldap.infospace.com") && !*pNewServer->searchBase)
				{
					/* 4.0 unfortunately shipped with the wrong searchbase for Infospace, so
					 * if we find Infospace, and their searchBase hasn't been set, make it right
					 * It's not legal to do this in all.js unless we rename the LDAP prefs tree (ugh)
					 */
					pNewServer->searchBase = XP_STRDUP ("c=US");
					PREF_SetCharPref (tempString, "c=US");
				}
				
				pNewServer->dirType = (DirectoryType)DIR_GetIntPref (prefstring, "dirType", tempString, (int) LDAPDirectory);
				if (pNewServer->dirType == PABDirectory)
				{
					hasPAB = TRUE;
					/* make sure there is a true PAB */
					if (XP_STRLEN (pNewServer->serverName) == 0)
						pNewServer->isOffline = FALSE;
					pNewServer->saveResults = TRUE; /* never let someone delete their PAB this way */
				}

				pNewServer->fileName = DIR_GetStringPref (prefstring, "filename", tempString, "");
				if (!pNewServer->fileName || !*(pNewServer->fileName)) 
				{
					if (pNewServer->dirType != PABDirectory)
						DIR_SetServerFileName (pNewServer, pNewServer->serverName);
					else
					{
						XP_FREEIF(pNewServer->fileName);
						pNewServer->fileName = XP_STRDUP (pabFile);
					}
				}
#if defined(XP_WIN) || defined(XP_UNIX) || defined(XP_MAC) || defined(XP_OS2)
				DIR_ConvertServerFileName(pNewServer);
#endif
				pNewServer->lastSearchString = DIR_GetStringPref (prefstring, "searchString", tempString, "");
				pNewServer->isOffline = DIR_GetBoolPref (prefstring, "isOffline", tempString, kDefaultIsOffline);

				/* This is where site-configurable attributes and filters are read from JavaScript */
				DIR_GetCustomAttributePrefs (prefstring, pNewServer, tempString);
				DIR_GetCustomFilterPrefs (prefstring, pNewServer, tempString);

				/* Get authentication prefs */
				pNewServer->enableAuth = DIR_GetBoolPref (prefstring, "enableAuth", tempString, kDefaultEnableAuth);
				pNewServer->authDn = DIR_GetStringPref (prefstring, "authDn", tempString, NULL);
				pNewServer->savePassword = DIR_GetBoolPref (prefstring, "savePassword", tempString, kDefaultSavePassword);
				if (pNewServer->savePassword)
					pNewServer->password = DIR_GetStringPref (prefstring, "password", tempString, "");

				prefBool = DIR_GetBoolPref (prefstring, "autoComplete.enabled", tempString, kDefaultAutoCompleteEnabled);
				DIR_ForceFlag (pNewServer, DIR_AUTO_COMPLETE_ENABLED, prefBool);
				prefInt = DIR_GetIntPref (prefstring, "autoComplete.style", tempString, kDefaultAutoCompleteStyle);
				pNewServer->autoCompleteStyle = (DIR_AutoCompleteStyle) prefInt;

				prefBool = DIR_GetBoolPref (prefstring, "utf8Disabled", tempString, kDefaultUtf8Disabled);
				DIR_ForceFlag (pNewServer, DIR_UTF8_DISABLED, prefBool);

				prefBool = DIR_GetBoolPref (prefstring, "ldapPublicDirectory", tempString, kDefaultLdapPublicDirectory);
				DIR_ForceFlag (pNewServer, DIR_LDAP_PUBLIC_DIRECTORY, prefBool);
				DIR_ForceFlag (pNewServer, DIR_LDAP_ROOTDSE_PARSED, prefBool);

				pNewServer->customDisplayUrl = DIR_GetStringPref (prefstring, "customDisplayUrl", tempString, "");

				XP_ListAddObjectToEnd((*list), pNewServer);
			}
		}

		XP_FREEIF(numberOfDirs);

		/* all.js should have filled this stuff in */
		XP_ASSERT(hasPAB);
		XP_ASSERT(numDirectories != 0);
	}
	else
		result = -1;

	XP_FREEIF (prefstring);
	XP_FREEIF (tempString);

	return result;
}


int DIR_GetServerPreferences(XP_List **list, const char* pabFile)
{
	int err = 0;
	XP_List *oldList = NULL;
	XP_List *newList = NULL;
	char *oldChildren = NULL;

	int32 listVersion = -1;
	XP_Bool userHasOldPrefs = FALSE;

	if (PREF_NOERROR == PREF_GetIntPref ("ldapList.version", &listVersion))
		userHasOldPrefs = (kCurrentListVersion > listVersion);
	else
		userHasOldPrefs = TRUE;
	
	/* Look to see if there's an old-style "directories" tree in prefs */
	if (PREF_NOERROR == PREF_CreateChildList ("directories", &oldChildren))
	{
		if (oldChildren)
		{
			if (XP_STRLEN(oldChildren))
			{
				if (userHasOldPrefs)
					err = DIR_GetPrefsFromBranch (&oldList, pabFile, "directories");
				PREF_DeleteBranch ("directories");
			}
			XP_FREEIF(oldChildren);
		}
	}
	/* Look to see if there's an old-style "ldap" tree in prefs */
	else if (PREF_NOERROR == PREF_CreateChildList ("ldap", &oldChildren))
	{
		if (oldChildren)
		{
			if (XP_STRLEN(oldChildren))
			{
				if (userHasOldPrefs)
					err = DIR_GetPrefsFromBranch (&oldList, pabFile, "ldap");
				PREF_DeleteBranch ("ldap");
			}
			XP_FREEIF(oldChildren);
		}
	}

	/* Find the new-style "ldap_1" tree in prefs */
	DIR_GetPrefsFromBranch(&newList, pabFile, "ldap_1");

	if (oldList && newList)
	{
		/* Merge the new tree onto the old tree, new on top, unique old at bottom */
		DIR_Server *oldServer;
		XP_List *walkOldList = oldList;

		while (NULL != (oldServer = XP_ListNextObject(walkOldList)))
		{
			XP_Bool addOldServer = TRUE;
			DIR_Server *newServer;
			XP_List *walkNewList = newList;

			while (NULL != (newServer = XP_ListNextObject(walkNewList)) && addOldServer)
			{
				if (DIR_AreServersSame(oldServer, newServer))
					addOldServer = FALSE; /* don't add servers which are in the new list */
				else if (PABDirectory == oldServer->dirType)
					addOldServer = FALSE; /* don't need the old PAB; there's already one in ALL.JS */
				else if (!XP_STRCMP(oldServer->serverName, "ldap-trace.fedex.com"))
					addOldServer = FALSE;
			}

			if (addOldServer)
			{
				DIR_Server *copyOfOldServer;
				DIR_CopyServer(oldServer, &copyOfOldServer);
				XP_ListAddObjectToEnd(newList, copyOfOldServer);
			}
		}

		/* Delete the list of old-style prefs */
		DIR_DeleteServerList (oldList);

		/* Write the new/merged list so we get it next time we ask */
		DIR_SaveServerPreferences (newList);
	}

	PREF_SetIntPref ("ldapList.version", kCurrentListVersion);

	*list = newList;
	return err;
}


static void DIR_ClearPrefBranch(const char *branch)
{
	/* This little function provides a way to delete a prefs object but still
	 * allow reassignment of that object later. 
	 */
	char *recreateBranch = NULL;

	PREF_DeleteBranch (branch);
	recreateBranch = PR_smprintf("pref_inittree(\"%s\")", branch);
	if (recreateBranch)
	{
		PREF_QuietEvaluateJSBuffer (recreateBranch, XP_STRLEN(recreateBranch));
		XP_FREE(recreateBranch);
	}
}


static void DIR_ClearIntPref (const char *pref)
{
	int32 oldDefault;
	int prefErr = PREF_GetDefaultIntPref (pref, &oldDefault);
	DIR_ClearPrefBranch (pref);
	if (prefErr >= 0)
		PREF_SetDefaultIntPref (pref, oldDefault);
}


static void DIR_ClearStringPref (const char *pref)
{
	char *oldDefault = NULL;
	int prefErr = PREF_CopyDefaultCharPref (pref, &oldDefault);
	DIR_ClearPrefBranch (pref);
	if (prefErr >= 0)
		PREF_SetDefaultCharPref (pref, oldDefault);
	XP_FREEIF(oldDefault);
}


static void DIR_ClearBoolPref (const char *pref)
{
	XP_Bool oldDefault;
	int prefErr = PREF_GetDefaultBoolPref (pref, &oldDefault);
	DIR_ClearPrefBranch (pref);
	if (prefErr >= 0)
		PREF_SetDefaultBoolPref (pref, oldDefault);
}


static void DIR_SetStringPref (const char *prefRoot, const char *prefLeaf, char *scratch, const char *value, const char *defaultValue)
{
	char *defaultPref = NULL;
	int prefErr = PREF_NOERROR;

	XP_STRCPY(scratch, prefRoot);
	XP_STRCAT (scratch, prefLeaf);

	if (PREF_NOERROR == PREF_CopyDefaultCharPref (scratch, &defaultPref))
	{
		/* If there's a default pref, just set ours in and let libpref worry 
		 * about potential defaults in all.js
		 */
		 if (value) /* added this check to make sure we have a value before we try to set it..*/
		 	prefErr = PREF_SetCharPref (scratch, value);
		 else
			 DIR_ClearStringPref(scratch);

		XP_FREE(defaultPref);
	}
	else
	{
		/* If there's no default pref, look for a user pref, and only set our value in
		 * if the user pref is different than one of them.
		 */
		char *userPref = NULL;
		if (PREF_NOERROR == PREF_CopyCharPref (scratch, &userPref))
		{
			if (value && (defaultValue ? XP_STRCASECMP(value, defaultValue) : value != defaultValue))
				prefErr = PREF_SetCharPref (scratch, value);
			else
				DIR_ClearStringPref (scratch); 
		}
		else
		{
			if (value && (defaultValue ? XP_STRCASECMP(value, defaultValue) : value != defaultValue))
				prefErr = PREF_SetCharPref (scratch, value); 
		}
	}

	XP_ASSERT(prefErr >= 0);
}


static void DIR_SetIntPref (const char *prefRoot, const char *prefLeaf, char *scratch, int32 value, int32 defaultValue)
{
	int32 defaultPref;
	int prefErr = 0;

	XP_STRCPY(scratch, prefRoot);
	XP_STRCAT (scratch, prefLeaf);

	if (PREF_NOERROR == PREF_GetDefaultIntPref (scratch, &defaultPref))
	{
		/* solve the problem where reordering user prefs must override default prefs */
		PREF_SetIntPref (scratch, value);
	}
	else
	{
		int32 userPref;
		if (PREF_NOERROR == PREF_GetIntPref (scratch, &userPref))
		{
			if (value != defaultValue)
				prefErr = PREF_SetIntPref(scratch, value);
			else
				DIR_ClearIntPref (scratch);
		}
		else
		{
			if (value != defaultValue)
				prefErr = PREF_SetIntPref (scratch, value); 
		}
	}

	XP_ASSERT(prefErr >= 0);
}


static void DIR_SetBoolPref (const char *prefRoot, const char *prefLeaf, char *scratch, XP_Bool value, XP_Bool defaultValue)
{
	XP_Bool defaultPref;
	int prefErr = PREF_NOERROR;

	XP_STRCPY(scratch, prefRoot);
	XP_STRCAT (scratch, prefLeaf);

	if (PREF_NOERROR == PREF_GetDefaultBoolPref (scratch, &defaultPref))
	{
		/* solve the problem where reordering user prefs must override default prefs */
		prefErr = PREF_SetBoolPref (scratch, value);
	}
	else
	{
		XP_Bool userPref;
		if (PREF_NOERROR == PREF_GetBoolPref (scratch, &userPref))
		{
			if (value != defaultValue)
				prefErr = PREF_SetBoolPref(scratch, value);
			else
				DIR_ClearBoolPref (scratch);
		}
		else
		{
			if (value != defaultValue)
				prefErr = PREF_SetBoolPref (scratch, value);
		}

	}

	XP_ASSERT(prefErr >= 0);
}


static int DIR_ConvertAttributeToPrefsString (DIR_Attribute *attrib, char **ppPrefsString)
{
	int err = 0;

	/* Compute size in bytes req'd for prefs string */
	int length = XP_STRLEN(attrib->prettyName);
	int i = 0;
	while (attrib->attrNames[i])
	{
		length += XP_STRLEN(attrib->attrNames[i]) + 1; /* +1 for comma separator */
		i++;
	}
	length += 1; /* +1 for colon */

	/* Allocate prefs string */
	*ppPrefsString = (char*) XP_ALLOC(length + 1); /* +1 for null term */

	/* Unravel attrib struct back out into prefs */
	if (*ppPrefsString)
	{
		int j = 0;
		XP_STRCPY (*ppPrefsString, attrib->prettyName);
		XP_STRCAT (*ppPrefsString, ":");
		while (attrib->attrNames[j])
		{
			XP_STRCAT (*ppPrefsString, attrib->attrNames[j]);
			if (j + 1 < i)
				XP_STRCAT (*ppPrefsString, ",");
			j++;
		}
	}
	else
		err = MK_OUT_OF_MEMORY;

	return err;
}


static int DIR_SaveOneCustomAttribute (const char *prefRoot, char *scratch, DIR_Server *server, DIR_AttributeId id)
{
	int err;

	const char *name = DIR_GetDefaultAttribute (id)->name;
	err = 0;

	if (server->customAttributes)
	{
		DIR_Attribute *attrib = NULL;
		XP_List *walkList = server->customAttributes;
		while ((attrib = XP_ListNextObject(walkList)) != NULL)
		{
			if (attrib->id == id)
			{
				char *jsString = NULL;
				if (0 == DIR_ConvertAttributeToPrefsString(attrib, &jsString))
				{
					DIR_SetStringPref (prefRoot, name, scratch, jsString, "");
					XP_FREE(jsString);
					return err;
				}
			}
		}

	}

	/* This server doesn't have a custom attribute for the requested ID
	 * so set it to the null string just in case there's an ALL.JS setting
	 * or had a previous user value
	 */
	DIR_SetStringPref (prefRoot, name, scratch, "", "");

	return err;
}


static int DIR_SaveCustomAttributes (const char *prefRoot, char *scratch, DIR_Server *server)
{
	int err;
	char *localScratch = (char*) XP_ALLOC(256);
	err = 0;

	XP_STRCPY (scratch, prefRoot);
	XP_STRCAT (scratch, "attributes.");
	if (localScratch)
	{
		DIR_SaveOneCustomAttribute (scratch, localScratch, server, cn);
		DIR_SaveOneCustomAttribute (scratch, localScratch, server, givenname);
		DIR_SaveOneCustomAttribute (scratch, localScratch, server, sn);
		DIR_SaveOneCustomAttribute (scratch, localScratch, server, mail);
		DIR_SaveOneCustomAttribute (scratch, localScratch, server, telephonenumber);
		DIR_SaveOneCustomAttribute (scratch, localScratch, server, o);
		DIR_SaveOneCustomAttribute (scratch, localScratch, server, ou);
		DIR_SaveOneCustomAttribute (scratch, localScratch, server, l);
		DIR_SaveOneCustomAttribute (scratch, localScratch, server, street);
		DIR_SaveOneCustomAttribute (scratch, localScratch, server, custom1);
		DIR_SaveOneCustomAttribute (scratch, localScratch, server, custom2);
		DIR_SaveOneCustomAttribute (scratch, localScratch, server, custom3);
		DIR_SaveOneCustomAttribute (scratch, localScratch, server, custom4);
		DIR_SaveOneCustomAttribute (scratch, localScratch, server, custom5);
		DIR_SaveOneCustomAttribute (scratch, localScratch, server, auth);
		XP_FREE(localScratch);
	}
	else
		err = MK_OUT_OF_MEMORY;

	return err;
}


static int DIR_SaveCustomFilters (const char *prefRoot, char *scratch, DIR_Server *server)
{
	int err;
	char *localScratch = (char*) XP_ALLOC(256);
	err = 0;

	XP_STRCPY (scratch, prefRoot);
	XP_STRCAT (scratch, "filter1.");
	if (server->customFilters)
	{
		/* Save the custom filters into the JS prefs */
		DIR_Filter *filter = NULL;
		XP_List *walkList = server->customFilters;
		if (localScratch)
		{
			while ((filter = XP_ListNextObject(walkList)) != NULL)
			{
				DIR_SetBoolPref (scratch, "repeatFilterForWords", localScratch, 
					(filter->flags & DIR_F_REPEAT_FILTER_FOR_TOKENS) != 0, kDefaultRepeatFilterForTokens);
				DIR_SetStringPref (scratch, "string", localScratch, filter->string, kDefaultFilter);
			}
			XP_FREE(localScratch);
			localScratch = NULL;
		}
		else
			err = MK_OUT_OF_MEMORY;
	}
	else
	{
		/* The DIR_Server object doesn't think it has any custom filters,
		 * so make sure the prefs settings are empty too
		 */
		DIR_SetBoolPref (scratch, "repeatFilterForWords", localScratch, 
			kDefaultRepeatFilterForTokens, kDefaultRepeatFilterForTokens);
		DIR_SetStringPref (scratch, "string", localScratch, kDefaultFilter, kDefaultFilter);
	}

	if (localScratch)  /* memory leak! I'm adding this to patch up the leak. */
		XP_FREE(localScratch);

	return err;
}


static int dir_SaveReplicationInfo (const char *prefRoot, char *scratch, DIR_Server *server)
{
	int err = 0;
	char *localScratch = (char*) XP_ALLOC(256);
	if (!localScratch)
		return MK_OUT_OF_MEMORY;

	XP_STRCPY (scratch, prefRoot);
	XP_STRCAT (scratch, "replication.");

	DIR_SetBoolPref (scratch, "enabled", localScratch, DIR_TestFlag (server, DIR_REPLICATION_ENABLED), kDefaultReplicaEnabled);

	if (server->replInfo)
	{
		char *excludedList = NULL;
		int i;
		int excludedLength = 0;
		for (i = 0; i < server->replInfo->excludedAttributesCount; i++)
			excludedLength += XP_STRLEN (server->replInfo->excludedAttributes[i]) + 2; /* +2 for ", " */
		if (excludedLength)
		{
			excludedList = (char*) XP_ALLOC (excludedLength + 1);
			if (excludedList)
			{
				excludedList[0] = '\0';
				for (i = 0; i < server->replInfo->excludedAttributesCount; i++)
				{
					XP_STRCAT (excludedList, server->replInfo->excludedAttributes[i]);
					XP_STRCAT (excludedList, ", ");
				}
			}
			else
				err = MK_OUT_OF_MEMORY;
		}

		DIR_SetStringPref (scratch, "excludedAttributes", localScratch, excludedList, kDefaultReplicaExcludedAttributes);

		DIR_SetStringPref (scratch, "description", localScratch, server->replInfo->description, kDefaultReplicaDescription);
		DIR_SetStringPref (scratch, "fileName", localScratch, server->replInfo->fileName, kDefaultReplicaFileName);
		DIR_SetStringPref (scratch, "filter", localScratch, server->replInfo->filter, kDefaultReplicaFilter);
		DIR_SetIntPref (scratch, "lastChangeNumber", localScratch, server->replInfo->lastChangeNumber, kDefaultReplicaChangeNumber);
		DIR_SetStringPref (scratch, "dataVersion", localScratch, server->replInfo->dataVersion, kDefaultReplicaDataVersion);
	}

	XP_FREE(localScratch);
	return err;
}


int DIR_SaveServerPreferences (XP_List *wholeList)
{
	int i;
	DIR_Server *s;
	char * prefstring = NULL;
	char * tempString = NULL;

	if (wholeList)
	{
		prefstring = (char *) XP_ALLOC(128);
		tempString = (char *) XP_ALLOC(256);
		if (prefstring && tempString) 
		{
			PREF_SetIntPref("ldap_1.number_of_directories", XP_ListCount (wholeList));	

			for (i = 1; i <= XP_ListCount(wholeList); i++)
			{
				s = (DIR_Server*) XP_ListGetObjectNum (wholeList, i);
				if (s)
				{
					XP_SPRINTF(prefstring, "ldap_1.directory%i.", i);

					DIR_SetStringPref (prefstring, "description", tempString, s->description, "");
					DIR_SetStringPref (prefstring, "serverName", tempString, s->serverName, "");
					DIR_SetStringPref (prefstring, "searchBase", tempString, s->searchBase, "");
					DIR_SetStringPref (prefstring, "filename", tempString, s->fileName, "");
					if (s->port == 0)
						s->port = s->isSecure ? LDAPS_PORT : LDAP_PORT;
					DIR_SetIntPref (prefstring, "port", tempString, s->port, s->isSecure ? LDAPS_PORT : LDAP_PORT);
					DIR_SetIntPref (prefstring, "maxHits", tempString, s->maxHits, kDefaultMaxHits);
					DIR_SetBoolPref (prefstring, "isSecure", tempString, s->isSecure, FALSE);
					DIR_SetBoolPref (prefstring, "saveResults", tempString, s->saveResults, TRUE);
					DIR_SetBoolPref (prefstring, "efficientWildcards", tempString, s->efficientWildcards, TRUE);
					DIR_SetStringPref (prefstring, "searchString", tempString, s->lastSearchString, "");
					DIR_SetIntPref (prefstring, "dirType", tempString, s->dirType, (int) LDAPDirectory);
					DIR_SetBoolPref (prefstring, "isOffline", tempString, s->isOffline, kDefaultIsOffline);

					DIR_SetBoolPref (prefstring, "autoComplete.enabled", tempString, DIR_TestFlag(s, DIR_AUTO_COMPLETE_ENABLED), kDefaultAutoCompleteEnabled);
					DIR_SetIntPref (prefstring, "autoComplete.style", tempString, (int32) s->autoCompleteStyle, kDefaultAutoCompleteStyle);

					DIR_SetBoolPref (prefstring, "utf8Disabled", tempString, DIR_TestFlag(s, DIR_UTF8_DISABLED), kDefaultUtf8Disabled);

					DIR_SetBoolPref (prefstring, "enableAuth", tempString, s->enableAuth, kDefaultEnableAuth);
					DIR_SetStringPref (prefstring, "authDn", tempString, s->authDn, NULL);
					DIR_SetBoolPref (prefstring, "savePassword", tempString, s->savePassword, kDefaultSavePassword);
					DIR_SetStringPref (prefstring, "password", tempString, s->savePassword ? s->password : "", "");

					DIR_SetBoolPref (prefstring, "publicDirectory", tempString, DIR_TestFlag(s, DIR_LDAP_PUBLIC_DIRECTORY), kDefaultLdapPublicDirectory);

					DIR_SaveCustomAttributes (prefstring, tempString, s);
					DIR_SaveCustomFilters (prefstring, tempString, s);
					dir_SaveReplicationInfo (prefstring, tempString, s);

					DIR_SetStringPref (prefstring, "customDisplayUrl", tempString, s->customDisplayUrl, "");
				}
			}
			XP_SPRINTF(tempString, "%i", tempString);
			PREF_SetCharPref("ldap_1.end_of_directories", tempString);
		} 

		XP_FREEIF (prefstring);
		XP_FREEIF (tempString);
	}

	return 0;
}

/*****************************************************************************
 * Functions for getting site-configurable preferences, from JavaScript if
 * the site admin has provided them, else out of thin air.
 */

static DIR_DefaultAttribute *DIR_GetDefaultAttribute (DIR_AttributeId id)
{
	int i = 0;

	static DIR_DefaultAttribute defaults[16];

	if (defaults[0].name == NULL)
	{
		defaults[0].id = cn;
		defaults[0].resourceId = MK_LDAP_COMMON_NAME;
		defaults[0].name = "cn";
	
		defaults[1].id = givenname;
		defaults[1].resourceId = MK_LDAP_GIVEN_NAME;
		defaults[1].name = "givenName";
	
		defaults[2].id = sn;
		defaults[2].resourceId = MK_LDAP_SURNAME;
		defaults[2].name = "sn";
	
		defaults[3].id = mail;
		defaults[3].resourceId = MK_LDAP_EMAIL_ADDRESS;
		defaults[3].name = "mail";
	
		defaults[4].id = telephonenumber;
		defaults[4].resourceId = MK_LDAP_PHONE_NUMBER;
		defaults[4].name = "telephoneNumber";
	
		defaults[5].id = o;
		defaults[5].resourceId = MK_LDAP_ORGANIZATION;
		defaults[5].name = "o";
	
		defaults[6].id = ou;
		defaults[6].resourceId = MK_LDAP_ORG_UNIT;
		defaults[6].name = "ou";
	
		defaults[7].id = l;
		defaults[7].resourceId = MK_LDAP_LOCALITY;
		defaults[7].name = "l";
	
		defaults[8].id = street;
		defaults[8].resourceId = MK_LDAP_STREET;
		defaults[8].name = "street";
	
		defaults[9].id = custom1;
		defaults[9].resourceId = MK_LDAP_CUSTOM1;
		defaults[9].name = "custom1";
	
		defaults[10].id = custom2;
		defaults[10].resourceId = MK_LDAP_CUSTOM2;
		defaults[10].name = "custom2";
	
		defaults[11].id = custom3;
		defaults[11].resourceId = MK_LDAP_CUSTOM3;
		defaults[11].name = "custom3";
	
		defaults[12].id = custom4;
		defaults[12].resourceId = MK_LDAP_CUSTOM4;
		defaults[12].name = "custom4";
	
		defaults[13].id = custom5;
		defaults[13].resourceId = MK_LDAP_CUSTOM5;
		defaults[13].name = "custom5";

		defaults[14].id = auth;
		defaults[14].resourceId = MK_LDAP_EMAIL_ADDRESS;
		defaults[14].name = "mail";

		defaults[15].id = cn;
		defaults[15].resourceId = 0;
		defaults[15].name = NULL;
	}

	while (defaults[i].name)
	{
		if (defaults[i].id == id)
			return &defaults[i];
		i++;
	}

	return NULL;
}

const char *DIR_GetAttributeName (DIR_Server *server, DIR_AttributeId id)
{
	char *result = NULL;

	/* First look in the custom attributes in case the attribute is overridden */
	XP_List *list = server->customAttributes;
	DIR_Attribute *walkList = NULL;

	while ((walkList = XP_ListNextObject(list)) != NULL)
	{
		if (walkList->id == id)
			result = walkList->prettyName;
	}

	/* If we didn't find it, look in our own static list of attributes */
	if (!result)
	{
		DIR_DefaultAttribute *def;
		if ((def = DIR_GetDefaultAttribute(id)) != NULL)
			result = XP_GetString(def->resourceId);
	}

	return result;
}


const char **DIR_GetAttributeStrings (DIR_Server *server, DIR_AttributeId id)
{
	const char **result = NULL;

	if (server && server->customAttributes)
	{
		/* First look in the custom attributes in case the attribute is overridden */
		XP_List *list = server->customAttributes;
		DIR_Attribute *walkList = NULL;

		while ((walkList = XP_ListNextObject(list)) != NULL)
		{
			if (walkList->id == id)
				result = (const char**)walkList->attrNames;
		}
	}

	/* If we didn't find it, look in our own static list of attributes */
	if (!result)
	{
		static const char *array[2];
		array[0] = DIR_GetDefaultAttribute(id)->name;
		array[1] = NULL;
		result = (const char**)array;
	}
	return result;
}


const char *DIR_GetFirstAttributeString (DIR_Server *server, DIR_AttributeId id)
{
	const char **array = DIR_GetAttributeStrings (server, id);
	return array[0];
}

const char *DIR_GetReplicationFilter (DIR_Server *server)
{
	if (server && server->replInfo)
		return server->replInfo->filter;
	else
		return NULL;
}

const char *DIR_GetFilterString (DIR_Server *server)
{
	DIR_Filter *filter = XP_ListTopObject (server->customFilters);
	if (filter)
		return filter->string;
	return NULL;
}


static DIR_Filter *DIR_LookupFilter (DIR_Server *server, const char *filter)
{
	XP_List *list = server->customFilters;
	DIR_Filter *walkFilter = NULL;

	while ((walkFilter = XP_ListNextObject(list)) != NULL)
		if (!XP_STRCASECMP(filter, walkFilter->string))
			return walkFilter;

	return NULL;
}


XP_Bool DIR_RepeatFilterForTokens (DIR_Server *server, const char *filter)
{
	const DIR_Filter *filterStruct = DIR_LookupFilter (server, filter);
	if (filterStruct)
		return (filterStruct->flags & DIR_F_REPEAT_FILTER_FOR_TOKENS) != 0;

	return kDefaultRepeatFilterForTokens;
}


XP_Bool DIR_SubstStarsForSpaces (DIR_Server *server, const char *filter)
{
	const DIR_Filter *filterStruct = DIR_LookupFilter (server, filter);
	if (filterStruct)
		return (filterStruct->flags & DIR_F_SUBST_STARS_FOR_SPACES) != 0;

	return kDefaultSubstStarsForSpaces;
}


const char *DIR_GetTokenSeparators (DIR_Server *server)
{
	return server->tokenSeps ? server->tokenSeps : kDefaultTokenSeps;
}

XP_Bool DIR_UseCustomAttribute (DIR_Server *server, DIR_AttributeId id)
{
	XP_List *list = server->customAttributes;
	DIR_Attribute *walkList = NULL;

	while ((walkList = XP_ListNextObject(list)) != NULL)
	{
		if (walkList->id == id)
			return TRUE;
	}
	return FALSE;
}


XP_Bool DIR_IsDnAttribute (DIR_Server *s, const char *attrib)
{
	if (s && s->dnAttributes)
	{
		/* Look in the server object to see if there are prefs to tell
		 * us which attributes contain DNs
		 */
		int i;
		for (i = 0; i < s->dnAttributesCount; i++)
		{
			if (!XP_STRCASECMP(attrib, s->dnAttributes[i]))
				return TRUE;
		}
	}
	else
	{
		/* We have some default guesses about what attributes 
		 * are likely to contain DNs 
		 */
		switch (XP_TO_LOWER(attrib[0]))
		{
		case 'm':
			if (!XP_STRCASECMP(attrib, "manager") || 
				!XP_STRCASECMP(attrib, "member"))
				return TRUE;
			break;
		case o:
			if (!XP_STRCASECMP(attrib, "owner"))
				return TRUE;
			break;
		case 'u':
			if (!XP_STRCASECMP(attrib, "uniquemember"))
				return TRUE;
			break;
		}
	}
	return FALSE;

}


XP_Bool DIR_IsAttributeExcludedFromHtml (DIR_Server *s, const char *attrib)
{
	if (s && s->suppressedAttributes)
	{
		/* Look in the server object to see if there are prefs to tell
		 * us which attributes shouldn't be shown in HTML
		 */
		int i;
		for (i = 0; i < s->suppressedAttributesCount; i++)
		{
			if (!XP_STRCASECMP(attrib, s->suppressedAttributes[i]))
				return TRUE;
		}
	}
	/* else don't exclude it. By default we show everything */

	return FALSE;
}


static void dir_PushStringToPrefs (DIR_Server *s, char **curVal, const char *newVal, const char *name)
{
	XP_List *servers = NULL;
	XP_List *walkList = NULL;
	DIR_Server *walkServer = NULL;
	XP_Bool found = FALSE;
	int idx = 0;

	/* Don't do anything if we already have a value and it's the same as the new one */
	if (curVal && *curVal)
	{
		if (!XP_STRCMP(*curVal, newVal))
			return;
	}

	/* Find the server in the list so we can get its index.
	 * Can't use XP_GetNumFromObject because this DIR_Server isn't guaranteed
	 * to be in the FE's list -- it may be from a copy of the FE's list
	 */
	walkList = FE_GetDirServers ();
	while (!found)
	{
		walkServer = (DIR_Server*) XP_ListNextObject (walkList);
		if (!walkServer)
			break;
		found = DIR_AreServersSame (walkServer, s);
		idx++;
	}

	if (found)
	{
		/* Build the name of the prefs string */
		char buf[512];
		char *prefRoot = PR_smprintf ("ldap_1.directory%d.", idx);

		/* Set into the prefs */
		if (prefRoot)
		{
			*curVal = XP_STRDUP(newVal);
			DIR_SetStringPref (prefRoot, name, buf, *curVal, "");

			XP_FREE(prefRoot);
		}
	}

}


void DIR_SetAuthDN (DIR_Server *s, const char *dn)
{
	dir_PushStringToPrefs (s, &(s->authDn), dn, "authDn");
}


void DIR_SetPassword (DIR_Server *s, const char *password)
{
	dir_PushStringToPrefs (s, &(s->password), password, "password");
}


XP_Bool DIR_IsEscapedAttribute (DIR_Server *s, const char *attrib)
{
	/* We're not exposing this setting in JS prefs right now, but in case we
	 * might want to in the future, leave the DIR_Server* in the prototype.
	 */

	switch (XP_TO_LOWER(attrib[0]))
	{
	case 'p':
		if (!XP_STRCASECMP(attrib, "postaladdress"))
			return TRUE;
		break;
	case 'f': 
		if (!XP_STRCASECMP(attrib, "facsimiletelephonenumber"))
			return TRUE;
		break;
	case 'o':
		if (!XP_STRCASECMP(attrib, "othermail"))
			return TRUE;
		break;
	}
	return FALSE;
}


char *DIR_Unescape (const char *src, XP_Bool makeHtml)
{
/* Borrowed from libnet\mkparse.c */
#define UNHEX(C) \
	((C >= '0' && C <= '9') ? C - '0' : \
	((C >= 'A' && C <= 'F') ? C - 'A' + 10 : \
	((C >= 'a' && C <= 'f') ? C - 'a' + 10 : 0)))

	char *dest = NULL;
	int destLength = 0;

	int dollarCount = 0;
	int convertedLengthOfDollar = makeHtml ? 4 : 1;

	const char *tmpSrc = src;

	while (*tmpSrc)
		if (*tmpSrc++ == '$')
			dollarCount++;

	destLength = XP_STRLEN(src) + (dollarCount * convertedLengthOfDollar);
	dest = (char*) XP_ALLOC (destLength + 1);
	if (dest)
	{
		char *tmpDst = dest;
		*dest = '\0';
		tmpSrc = src;

		while (*tmpSrc)
		{
			switch (*tmpSrc)
			{
			case '$':
				/* A dollar sign is a linebreak. This is easy for HTML, but if we're converting
				 * for the Address Book or something without multiple lines, just put in a space
				 */
				if (makeHtml)
				{
					*tmpDst++ = '<';
					*tmpDst++ = 'B';
					*tmpDst++ = 'R';
					*tmpDst++ = '>';
				}
				else
					*tmpDst++ = ' ';
				break;
			case '\\': {
				/* A backslash indicates that two hex digits follow, which we're supposed to
				 * convert. The spec sez that '$', '#' and '\'' (single quote) must be encoded
				 * this way. 
				 */
				XP_Bool didEscape = FALSE;
				char c1 = *(tmpSrc + 1);
				if (c1 && (XP_IS_DIGIT(c1) || XP_IS_ALPHA(c1)))
				{
					char c2 = *(tmpSrc + 2);
					if (c2 && (XP_IS_DIGIT(c2) || XP_IS_ALPHA(c2)))
					{
						*tmpDst++ = (UNHEX(c1) << 4) | UNHEX(c2);
						tmpSrc +=2;
						didEscape = TRUE;
					}
				}
				if (!didEscape)
					*tmpDst++ = *tmpSrc;
			}
			break;
			default:
				/* Just a plain old char -- copy it over */
				*tmpDst++ = *tmpSrc;
			}
			tmpSrc++;
		}
		*tmpDst = '\0';
	}

	return dest;
}


int DIR_ValidateRootDSE (DIR_Server *server, char *version, int32 first, int32 last)
{
	/* Here we validate the replication info that the server has against the
	 * state of the local replica as stored in JS prefs. 
	 */

	if (!server || !version)
		return -1;

	/* If the replication info file name in the server is NULL, that means the
	 * server has not been replicated.  Reinitialize the change number and the
	 * data version and generate a file name for the replica database.
	 */
	if (!server->replInfo)
		server->replInfo = (DIR_ReplicationInfo *) XP_CALLOC (1, sizeof (DIR_ReplicationInfo));
	if (!server->replInfo)
		return MK_OUT_OF_MEMORY;

	if (!server->replInfo->fileName)
	{
		server->replInfo->lastChangeNumber = kDefaultReplicaChangeNumber;
		XP_FREEIF(server->replInfo->filter);
		server->replInfo->filter = XP_STRDUP(kDefaultReplicaFilter);
		XP_FREEIF(server->replInfo->dataVersion);
		server->replInfo->dataVersion = XP_STRDUP(version);
		DIR_SetFileName (&(server->replInfo->fileName), server->serverName);
		return 0;
	}

	/* There are three cases in which we should reinitialize the replica:
	 *  1) The data version of the server's DB is different than when we last
	 *     saw it, which means that the first and last change number we know
	 *     are totally meaningless.
	 *  2) Some changes have come and gone on the server since we last 
	 *     replicated. Since we have no way to know what those changes were,
	 *     we have no way to get sync'd up with the current server state
	 *  3) We have already replicated changes that the server hasn't made yet.
	 *     Not likely.
	 */
	if (   !server->replInfo->dataVersion || XP_STRCASECMP(version, server->replInfo->dataVersion)
	    || first > server->replInfo->lastChangeNumber + 1
	    || last < server->replInfo->lastChangeNumber)
	{
		server->replInfo->lastChangeNumber = kDefaultReplicaChangeNumber;
		XP_FREEIF(server->replInfo->dataVersion);
		server->replInfo->dataVersion = XP_STRDUP(version);
	}

	return 0;
}

#ifdef MOZ_LDAP
int DIR_ParseRootDSE (DIR_Server *server, LDAP *ld, LDAPMessage *message)
{
	char **values = NULL;

	server->flags |= DIR_LDAP_ROOTDSE_PARSED;
	server->flags &= ~(DIR_LDAP_VERSION3 | DIR_LDAP_VIRTUALLISTVIEW);

	values = ldap_get_values (ld, message, "supportedLDAPVersion");
	if (values && values[0])
	{
		int i;

		for (i = 0; values[i]; i++)
		{
			if (XP_ATOI (values[i]) == LDAP_VERSION3)
			{
				server->flags |= DIR_LDAP_VERSION3;
				break;
			}
		}
		ldap_value_free (values);
	}

	values = ldap_get_values (ld, message, "supportedControl");
	if (values)
	{
		int i;

		for (i = 0; values[i]; i++)
		{
			if (XP_STRCMP (values[i], LDAP_CONTROL_VLVREQUEST) == 0)
			{
				server->flags |= DIR_LDAP_VIRTUALLISTVIEW;
				break;
			}
		}
		ldap_value_free (values);
	}
	return 0;
}
#endif


void DIR_SetAutoCompleteEnabled (XP_List *list, DIR_Server *server, XP_Bool enabled)
{
	XP_ASSERT(server); /*list can be null*/
	if (server)
	{
		DIR_Server *tmp;

		if (enabled)
		{
			while (NULL != (tmp = XP_ListNextObject(list)))
				DIR_ClearFlag (tmp, DIR_AUTO_COMPLETE_ENABLED);
			DIR_SetFlag (server, DIR_AUTO_COMPLETE_ENABLED);
		}
		else
			DIR_ClearFlag (server, DIR_AUTO_COMPLETE_ENABLED);
	}
}

XP_Bool DIR_TestFlag (DIR_Server *server, uint32 flag)
{
	if (server)
		return 0 != (server->flags & flag);
	return FALSE;
}

void DIR_SetFlag (DIR_Server *server, uint32 flag)
{
	XP_ASSERT(server);
	if (server)
		server->flags |= flag;
}

void DIR_ClearFlag (DIR_Server *server, uint32 flag)
{
	XP_ASSERT(server);
	if (server)
		server->flags &= ~flag;
}


void DIR_ForceFlag (DIR_Server *server, uint32 flag, XP_Bool setIt)
{
	XP_ASSERT(server);
	if (server)
	{
		if (setIt)
			server->flags |= flag;
		else
			server->flags &= ~flag;
	}
}


/* Centralize this charset conversion so everyone can do the UTF8 conversion
 * in the same way. Also, if someone ever makes us do T.61 or some other silly
 * thing, we can use these bottlenecks
 */

char *DIR_ConvertToServerCharSet (DIR_Server *server, char *src, int16 srcCsid)
{
	if (server && (server->flags & DIR_UTF8_DISABLED))
		return XP_STRDUP (src);
	else
		return (char*) INTL_ConvertLineWithoutAutoDetect (srcCsid, CS_UTF8, (unsigned char*) src, XP_STRLEN(src));
}

char *DIR_ConvertFromServerCharSet (DIR_Server *server, char *src, int16 destCsid)
{
	if (server && (server->flags & DIR_UTF8_DISABLED))
		return XP_STRDUP (src);
	else
		return (char*) INTL_ConvertLineWithoutAutoDetect (CS_UTF8, destCsid, (unsigned char*) src, XP_STRLEN(src));
}


char *DIR_BuildUrl (DIR_Server *server, const char *dn, XP_Bool forAddToAB)
{
	char *url = NULL;
	char *escapedDn = NET_Escape (dn, URL_XALPHAS);
	if (escapedDn)
	{
		if (!forAddToAB && server->customDisplayUrl && server->customDisplayUrl[0])
		{
			/* Allow users to customize the URL we run when we open an LDAP entry. This
			 * is intended to appease the people who want extensive customization of the
			 * HTML we generate for LDAP. We're sidestepping the issue by allowing them
			 * to easily plug in DSGW, the LDAP-to-HTTP gateway, which already has
			 * very extensive template support.
			 */
			url = PR_smprintf (server->customDisplayUrl, escapedDn);
		}
		else
		{
			/* The default case, where we run an LDAP URL in a browser window 
			 */
			char *urlTemplate = "%s//%s/%s";
			char *urlPortTemplate = "%s//%s:%d/%s";
			char *urlScheme = NULL;
			int port = server->port;
			int standardPort = server->isSecure ? LDAPS_PORT : LDAP_PORT;

			if (server->isSecure)
				urlScheme = forAddToAB ? "addbook-ldaps:" : "ldaps:";
			else
				urlScheme = forAddToAB ? "addbook-ldap:" : "ldap:";

			if (port == standardPort)
				url = PR_smprintf (urlTemplate, urlScheme, server->serverName, escapedDn);
			else
				url = PR_smprintf (urlPortTemplate, urlScheme, server->serverName, port, escapedDn);

		}
		XP_FREE (escapedDn);
	}
	return url;
}
#endif /* !MOZADDRSTANDALONE */

#endif /* !MOZ_MAIL_NEWS */



