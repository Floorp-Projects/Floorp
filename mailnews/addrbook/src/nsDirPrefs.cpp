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

/* directory server preferences (used to be dirprefs.c in 4.x) */

#include "nsIPref.h"
#include "nsVoidArray.h"
#include "nsIServiceManager.h"
#include "nsDirPrefs.h"
#include "nsIAddrDatabase.h"
#include "nsCOMPtr.h"
#include "nsAbBaseCID.h"
#include "nsIAddrBookSession.h"
#include "nsICharsetConverterManager.h"
#include "nsIAbUpgrader.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"

#include "plstr.h"
#include "prmem.h"
#include "xp_str.h"
#include "xp_file.h"
#include "prprf.h"

static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

#define LDAP_PORT 389
#define LDAPS_PORT 636
#define PREF_NOERROR 0

/* This format suffix is being defined here because it is needed by the FEs in their 
   file operation routines */
#define ABFileName_kPreviousSuffix ".na2" /* final v2 address book format */

const char *kMainLdapAddressBook = "ldap.mab"; /* v3 main ldap address book file */

#define ABFileName_kCurrentSuffix ".mab" /* v3 address book extension */
#define ABPabFileName_kCurrent "abook" /* v3 address book name */

#if !defined(MOZADDRSTANDALONE)

typedef enum
{
	MK_ADDR_PAB,
	MK_LDAP_COMMON_NAME,
	MK_LDAP_GIVEN_NAME,
	MK_LDAP_SURNAME,
	MK_LDAP_EMAIL_ADDRESS,
	MK_LDAP_PHONE_NUMBER,
	MK_LDAP_ORGANIZATION, 
	MK_LDAP_ORG_UNIT, 
	MK_LDAP_LOCALITY,
	MK_LDAP_STREET,
	MK_LDAP_CUSTOM1, 
	MK_LDAP_CUSTOM2,
	MK_LDAP_CUSTOM3, 
	MK_LDAP_CUSTOM4,
	MK_LDAP_CUSTOM5,
	MK_LDAP_DESCRIPTION,   
	MK_LDAP_EMPLOYEE_TYPE, 
	MK_LDAP_FAX_NUMBER,    
	MK_LDAP_MANAGER,       
	MK_LDAP_OBJECT_CLASS,       
	MK_LDAP_POSTAL_ADDRESS,
	MK_LDAP_POSTAL_CODE,   
	MK_LDAP_SECRETARY,
	MK_LDAP_TITLE,
	MK_LDAP_CAR_LICENSE,
	MK_LDAP_BUSINESS_CAT,
	MK_LDAP_DEPT_NUMBER,
	MK_LDAP_REPL_QUERY_RESYNC,
	MK_LDAP_NICK_NAME,
	MK_LDAP_HOMEPHONE,
	MK_LDAP_MOBILEPHONE,
	MK_LDAP_PAGER
} DIR_ResourceID;

#else

#define NS_ERROR_OUT_OF_MEMORY -1;

#endif /* #if !defined(MOZADDRSTANDALONE) */

XP_FILE_URL_PATH	XP_PlatformFileToURL (const XP_FILE_NATIVE_PATH ) {return NULL;}

/*************************************************
   The following functions are used to implement
   a thread safe strtok
 *************************************************/
/*
 * Get next token from string *stringp, where tokens are (possibly empty)
 * strings separated by characters from delim.  Tokens are separated
 * by exactly one delimiter iff the skip parameter is false; otherwise
 * they are separated by runs of characters from delim, because we
 * skip over any initial `delim' characters.
 *
 * Writes NULs into the string at *stringp to end tokens.
 * delim will usually, but need not, remain CONSTant from call to call.
 * On return, *stringp points past the last NUL written (if there might
 * be further tokens), or is NULL (if there are definitely no more tokens).
 *
 * If *stringp is NULL, strtoken returns NULL.
 */
static 
char *strtoken_r(char ** stringp, const char *delim, PRInt32 skip)
{
	char *s;
	const char *spanp;
	PRInt32 c, sc;
	char *tok;

	if ((s = *stringp) == NULL)
		return (NULL);

	if (skip) {
		/*
		 * Skip (span) leading delimiters (s += strspn(s, delim)).
		 */
	cont:
		c = *s;
		for (spanp = delim; (sc = *spanp++) != 0;) {
			if (c == sc) {
				s++;
				goto cont;
			}
		}
		if (c == 0) {		/* no token found */
			*stringp = NULL;
			return (NULL);
		}
	}

	/*
	 * Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
	 * Note that delim must have one NUL; we stop if we see that, too.
	 */
	for (tok = s;;) {
		c = *s++;
		spanp = delim;
		do {
			if ((sc = *spanp++) == c) {
				if (c == 0)
					s = NULL;
				else
					s[-1] = 0;
				*stringp = s;
				return( (char *) tok );
			}
		} while (sc != 0);
	}
	/* NOTREACHED */
	return (NULL);
}



char *AB_pstrtok_r(char *s1, const char *s2, char **lasts)
{
	if (s1)
		*lasts = s1;
	return (strtoken_r(lasts, s2, 1));
}

/*****************************************************************************
 * Private structs and stuff
 */

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
	PRInt32 resourceId;
	char *name;
} DIR_DefaultAttribute;

/* DIR_Filter.flags */
#define DIR_F_SUBST_STARS_FOR_SPACES   0x00000001
#define DIR_F_REPEAT_FILTER_FOR_TOKENS 0x00000002

/* DIR_Server.filters is a list of DIR_Filter structures */
typedef struct DIR_Filter
{
	char *string;
	PRUint32 flags;
} DIR_Filter;

/* Callback list structure */
typedef struct DIR_Callback
{
	DIR_NOTIFICATION_FN  fn;
	PRUint32               flags;
	void                *data;
	struct DIR_Callback *next;
} DIR_Callback;

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
#define kDefaultTokenSeps " ,."
#define kDefaultSubstStarsForSpaces PR_TRUE
#define kDefaultRepeatFilterForTokens PR_TRUE
#define kDefaultEfficientWildcards PR_TRUE
#define kDefaultFilter "(cn=*%s*)"
#define kDefaultEfficientFilter "(|(givenname=%s)(sn=%s))"
#define kDefaultStopOnHit PR_TRUE
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
#define kDefaultReplicaExcludedAttributes nsnull

#define kDefaultPABColumnHeaders "cn,mail,o,nickname,telephonenumber,l"  /* default column headers for the address book window */
#define kDefaultLDAPColumnHeaders "cn,mail,o,telephonenumber,l,nickname"

static PRBool dir_IsServerDeleted(DIR_Server * server);
static DIR_DefaultAttribute *DIR_GetDefaultAttribute (DIR_AttributeId id);
static char *DIR_GetStringPref(const char *prefRoot, const char *prefLeaf, char *scratch, const char *defaultValue);
static char *DIR_GetLocalizedStringPref(const char *prefRoot, const char *prefLeaf, char *scratch, const char *defaultValue);
static PRInt32 DIR_GetIntPref(const char *prefRoot, const char *prefLeaf, char *scratch, PRInt32 defaultValue);
static PRBool DIR_GetBoolPref(const char *prefRoot, const char *prefLeaf, char *scratch, PRBool defaultValue);
static char * dir_ConvertDescriptionToPrefName(DIR_Server * server);
void DIR_SetFileName(char** filename, const char* leafName);

static PRInt32 PR_CALLBACK dir_ServerPrefCallback(const char *pref, void *inst_data);


// This can now use nsTextFormatter. 
// e.g.
// #include "nsTextFormatter.h"
// nsString aString(""); 
// nsAutoString fmt("%s"); 
// PRUnichar *uniBuffer = nsTextFormatter::smprintf(fmt.get(), aBuffer); // this converts UTF-8 to UCS-2 
// Do not use void* that was inherited from old libmime when it could not include C++, use PRUnichar* instead.
PRInt32 INTL_ConvertToUnicode(const char* aBuffer, const PRInt32 aLength,
                                      void** uniBuffer)
{
	if (nsnull == aBuffer) 
	{
		return -1;
	}

  NS_ConvertUTF8toUCS2 temp(aBuffer, aLength);
  *uniBuffer = nsCRT::strdup(temp.get());
  return (*uniBuffer) ? 0 : -1;
}

// This can now use ToNewUTF8String (or this function itself can be substitued by that).
// e.g.
// nsAutoString aStr(uniBuffer);
// *aBuffer = ToNewUTF8String(aStr);
// Do not use void* that was inherited from old libmime when it could not include C++, use PRUnichar* instead.
PRInt32 INTL_ConvertFromUnicode(const PRUnichar* uniBuffer, const PRInt32 uniLength, char** aBuffer)
{
	if (nsnull == uniBuffer) 
	{
		return -1;
	}

  NS_ConvertUCS2toUTF8 temp(uniBuffer, uniLength);
  *aBuffer = ToNewCString(temp);
  return (*aBuffer) ? 0 : -1;
}

/*****************************************************************************
 * Functions for creating the new back end managed DIR_Server list.
 */

static PRBool       dir_ServerPrefCallbackRegistered = PR_FALSE;
static PRInt32      dir_UserId = 0;
static DIR_Callback *dir_CallbackList = nsnull;

nsVoidArray  *dir_ServerList = nsnull;

nsVoidArray* DIR_GetDirectories()
{
    if (!dir_ServerList)
        DIR_GetDirServers();
	return dir_ServerList;
}

nsresult DIR_GetDirServers()
{
    nsresult rv = NS_OK;

	if (!dir_ServerList)
	{
        nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
        if (NS_FAILED(rv) || !pPref) 
    		return NS_ERROR_FAILURE;

		/* we need to build the DIR_Server list */ 
		rv = DIR_GetServerPreferences(&dir_ServerList);

		/* Register the preference call back if necessary. */
		if (NS_SUCCEEDED(rv) && (!dir_ServerPrefCallbackRegistered))
		{
			dir_ServerPrefCallbackRegistered = PR_TRUE;
        	pPref->RegisterCallback(PREF_LDAP_SERVER_TREE_NAME, dir_ServerPrefCallback, nsnull);
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
				nsString name; name.AssignWithConversion(server->fileName);
				PRInt32 pos = name.Find(ABFileName_kPreviousSuffix);
				if (pos > 0)
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

#ifdef CONVERT_TO_MORK_DONE
			if (server && server->fileName)
			{
				nsString name(server->fileName);
				PRInt32 pos = name.Find(ABFileName_kPreviousSuffix);
				if (pos)
				{
					name.Cut(pos, PL_strlen(ABFileName_kPreviousSuffix));
					name.Append(ABFileName_kCurrentSuffix);
					PR_FREEIF (server->fileName);
					server->fileName = ToNewCString(name);
				}
				DIR_SavePrefsForOneServer(server);
			}
#endif /* CONVERT_TO_MORK_DONE */
		}
	}
	return NS_OK;

}

nsresult DIR_ShutDown()  /* FEs should call this when the app is shutting down. It frees all DIR_Servers regardless of ref count values! */
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
	if (NS_FAILED(rv) || !pPref) 
		return NS_ERROR_FAILURE;
	pPref->SavePrefFile(nsnull);

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

	return NS_OK;
}

static NS_DEFINE_CID(kAddressBookDBCID, NS_ADDRDATABASE_CID);

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

nsresult DIR_AddNewAddressBook(const PRUnichar *dirName, const char *fileName, PRBool migrating, DirectoryType dirType, DIR_Server** pServer)
{
	DIR_Server * server = (DIR_Server *) PR_Malloc(sizeof(DIR_Server));
	DIR_InitServerWithType (server, dirType);
	if (!dir_ServerList)
		DIR_GetDirServers();
	if (dir_ServerList)
	{
		PRInt32 count = dir_ServerList->Count();
		nsString descString(dirName);
		PRInt32 unicharLength = descString.Length();

		INTL_ConvertFromUnicode(dirName, unicharLength, &server->description);
		server->position = count + 1;

		if (fileName)
            server->fileName = nsCRT::strdup(fileName);
		else
			DIR_SetFileName(&server->fileName, kPersonalAddressbook);

		dir_ServerList->AppendElement(server);
		if (!migrating) {
			DIR_SavePrefsForOneServer(server); 
		}
#ifdef DEBUG_sspitzer
		else {
			printf("don't set the prefs, they are already set since this ab was migrated\n");
		}
#endif
		*pServer = server;

		// save new address book into pref file 
		nsresult rv = NS_OK;
		nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
		if (NS_FAILED(rv) || !pPref) 
			return NS_ERROR_FAILURE;
		pPref->SavePrefFile(nsnull);

		return NS_OK;
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
	DIR_InitServer(server);
	if (dirType == LDAPDirectory)
	{
        server->columnAttributes = nsCRT::strdup(kDefaultLDAPColumnHeaders);
		server->dirType = LDAPDirectory;
		server->isOffline = PR_TRUE;
		server->csid = CS_UTF8;
		server->locale = nsnull;
	}
	else if (dirType == PABDirectory)
	{
        server->columnAttributes = nsCRT::strdup(kDefaultPABColumnHeaders);
		server->dirType = PABDirectory;
		server->isOffline = PR_FALSE;
		server->csid = CS_UTF8;
//		server->csid = INTL_GetCharSetID(INTL_DefaultTextWidgetCsidSel);
		server->locale = nsnull; /* get the locale we are going to use with this AB */
/*		server->locale = INTL_GetCollationKeyLocale(nsnull); */
	}
	return NS_OK;
}

nsresult DIR_InitServer (DIR_Server *server)
{
	NS_ASSERTION((server != nsnull), "server is null");
	if (server)
	{
		XP_BZERO(server, sizeof(DIR_Server));
		server->saveResults = PR_TRUE;
		server->efficientWildcards = kDefaultEfficientWildcards;
		server->port = LDAP_PORT;
		server->maxHits = kDefaultMaxHits;
		server->isOffline = kDefaultIsOffline;
		server->refCount = 1;
		server->position = kDefaultPosition;
		server->csid = CS_UTF8;
		server->locale = nsnull;
        server->uri = nsnull;
	}
	return NS_OK;
}

DIR_DescriptionCode DIR_ValidateDirectoryDescription(nsVoidArray * wholeList, DIR_Server * serverToValidate)
{
	/* right now the only invalid description is a duplicate...so check for duplicates */
	if (wholeList && serverToValidate && serverToValidate->description)
	{
		PRInt32 numItems = wholeList->Count();
		PRInt32 i;
		for (i = 0; i < numItems; i++)
		{
			DIR_Server *s = (DIR_Server *)(dir_ServerList->ElementAt(i));
			/* don't check the description if it is the same directory as the one we are comparing against */
            if (s != serverToValidate && s->description && !nsCRT::strcasecmp(s->description, serverToValidate->description))
				return DIR_DuplicateDescription;
		}

	}

	return DIR_ValidDescription;
}


/*****************************************************************************
 * Functions for cloning DIR_Servers
 */

static DIR_Attribute *DIR_CopyAttribute (DIR_Attribute *inAttribute)
{
	DIR_Attribute *outAttribute = (DIR_Attribute*) PR_Malloc(sizeof(DIR_Attribute));
	if (outAttribute)
	{
		PRInt32 count = 0;
		outAttribute->id = inAttribute->id;
        outAttribute->prettyName = nsCRT::strdup(inAttribute->prettyName);
		while (inAttribute->attrNames[count])
			count++;
		outAttribute->attrNames = (char**) PR_Malloc((count + 1) * sizeof(char*));
		if (outAttribute->attrNames)
		{
			PRInt32 i;
			for (i = 0; i < count; i++)
                outAttribute->attrNames[i] = nsCRT::strdup(inAttribute->attrNames[i]);
			outAttribute->attrNames[i] = nsnull;
		}
	}
	return outAttribute;
}


static DIR_Filter *DIR_CopyFilter (DIR_Filter *inFilter)
{
	DIR_Filter *outFilter = (DIR_Filter*) PR_Malloc(sizeof(DIR_Filter));
	if (outFilter)
	{
		outFilter->flags = inFilter->flags;
        outFilter->string = nsCRT::strdup(inFilter->string);
	}
	return outFilter;
}


static nsresult dir_CopyTokenList (char **inList, PRInt32 inCount, char ***outList, PRInt32 *outCount)
{
	nsresult status = NS_OK;
	if (0 != inCount && nsnull != inList)
	{
		*outList = (char**) PR_Malloc(inCount * sizeof(char*));
		if (*outList)
		{
			PRInt32 i;
			for (i = 0; i < inCount; i++)
                (*outList)[i] = nsCRT::strdup (inList[i]);
			*outCount = inCount;
		}
		else
			status = NS_ERROR_OUT_OF_MEMORY;
	}
	return status;
}


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
		dir_CopyTokenList (inInfo->excludedAttributes, inInfo->excludedAttributesCount,
			&outInfo->excludedAttributes, &outInfo->excludedAttributesCount);
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
			XP_BZERO (*out, sizeof(DIR_Server));

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

			if (in->columnAttributes)
			{
                (*out)->columnAttributes = nsCRT::strdup(in->columnAttributes);
				if (!(*out)->columnAttributes)
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
			(*out)->saveResults = in->saveResults;
			(*out)->isOffline = in->isOffline;
			(*out)->efficientWildcards = in->efficientWildcards;
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

			if (in->customAttributes)
			{
				(*out)->customAttributes = new nsVoidArray();
				if ((*out)->customAttributes)
				{
					nsVoidArray *list = in->customAttributes;
					DIR_Attribute *attribute = nsnull;
					PRInt32 count = list->Count();
					PRInt32 i;
					for (i = 0; i < count; i++)
					{
						attribute = (DIR_Attribute *)list->ElementAt(i);
						if (attribute)
						{
							DIR_Attribute *outAttr = DIR_CopyAttribute (attribute);
							if (outAttr)
								((*out)->customAttributes)->AppendElement(outAttr);
							else
								err = NS_ERROR_OUT_OF_MEMORY;
 						}
					}
				}
				else
					err = NS_ERROR_OUT_OF_MEMORY;
			}

			if (in->customFilters)
			{
				(*out)->customFilters = new nsVoidArray();
				if ((*out)->customFilters)
				{
					nsVoidArray *list = in->customFilters;
					DIR_Filter *filter = nsnull;

					PRInt32 count = list->Count();
					PRInt32 i;
					for (i = 0; i < count; i++)
					{
						filter = (DIR_Filter *)list->ElementAt(i);
						if (filter)
						{
							DIR_Filter *outFilter = DIR_CopyFilter (filter);
							if (outFilter)
								((*out)->customFilters)->AppendElement(outFilter);
							else
								err = NS_ERROR_OUT_OF_MEMORY;
						}
					}
				}
				else
					err = NS_ERROR_OUT_OF_MEMORY;
			}

			if (in->autoCompleteFilter)
			{
                (*out)->autoCompleteFilter = nsCRT::strdup(in->autoCompleteFilter);
				if (!(*out)->autoCompleteFilter)
					err = NS_ERROR_OUT_OF_MEMORY;
			}

			if (in->replInfo)
				(*out)->replInfo = dir_CopyReplicationInfo (in->replInfo);

			if (in->basicSearchAttributesCount > 0)
			{
				PRInt32 bsaLength = in->basicSearchAttributesCount * sizeof(DIR_AttributeId);
				(*out)->basicSearchAttributes = (DIR_AttributeId*) PR_Malloc(bsaLength);
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
			dir_CopyTokenList (in->uriAttributes, in->uriAttributesCount,
				&(*out)->uriAttributes, &(*out)->uriAttributesCount);

			if (in->customDisplayUrl)
                (*out)->customDisplayUrl = nsCRT::strdup (in->customDisplayUrl);
			if (in->searchPairList)
                (*out)->searchPairList = nsCRT::strdup (in->searchPairList);

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


/*****************************************************************************
 * Functions for manipulating DIR_Servers' positions
 */

/* Worker function to sort the servers in the given list by position value.
 * Uses a simple bubble sort to perform the operation (the server list
 * shouldn't be too long, right?)
 */
static void dir_SortServersByPosition(DIR_Server **serverList, PRInt32 count)
{
	PRInt32 i, j;
	DIR_Server *server;

	for (i = 0; i < count - 1; i++)
	{
		for (j = i + 1; j < count; j++)
		{
			if (serverList[j]->position < serverList[i]->position)
			{
				server        = serverList[i];
				serverList[i] = serverList[j];
				serverList[j] = server;
			}
		}
	}
}

/* Sorts the servers in the given list by position value and lock state
 * (in eight easy steps...).
 *
 * Here's how we do it:
 *  1) Create a list of the servers in array form from the list
 *  2) Separate the array into two parts, locked and unlocked servers
 *  3) Sort the locked portion of the array
 *  4) Sort the unlocked portion of the array
 *  5) Move unlocked servers to fill holes in the locked portion of the array
 *  6) Adjust any remaining unlocked servers positions to match the new order
 *  7) Delete the old server list
 *  8) Create a new server list based on the sorted order
 *
 * Returns PR_TRUE if the list was re-sorted.
 */
PRBool DIR_SortServersByPosition(nsVoidArray *wholeList)
{
	PRInt32		i, count, pos;
	PRInt32		in_order;
	nsVoidArray  *walkList;
	DIR_Server  *server;
	DIR_Server **serverList;

	/* Allocate space for the array of servers.
	 */
	count = wholeList->Count();
	serverList = (DIR_Server **)PR_Malloc(sizeof(DIR_Server *) * count);
	if (!serverList)
		return PR_FALSE;

	/* Copy the servers in the list into the array in the same order.
	 * While doing so check to see if the servers are aready in order.
	 */
	pos = 1;
	in_order = 2;
	walkList = wholeList;
	count = walkList->Count();
	for (i = 0; i < count;)
	{
		if ((server = (DIR_Server *)walkList->ElementAt(i)) != nsnull)
		{
			if (in_order > 0)
			{
				/* If the position values are out of order, then the list is
				 * out of order.
				 */
				if (i > 0 && serverList[i - 1]->position > server->position)
					in_order = 0;
				else
				{
					/* The list is considered to be in order if the position
					 * values are non-contiguous and all of the servers have
					 * locked positions.
					 */
					if (in_order == 2 && server->position != pos)
						in_order = 1;
					if (in_order == 1 && !DIR_TestFlag(server, DIR_POSITION_LOCKED))
						in_order = 0;
				}
			}

			serverList[i++] = server;
			pos++;
		}
	}

	/* Don't do anything if the servers are already in order.
	 */
	if (in_order == 0)
	{
		PRInt32 first_unlocked, j;

		/* Separate the array into two sections:  locked and unlocked.
		 */
		for (i = 0, j = count - 1; i < j; )
		{
			if (!DIR_TestFlag(serverList[i], DIR_POSITION_LOCKED))
			{
				while (i < j && !DIR_TestFlag(serverList[j], DIR_POSITION_LOCKED))
					j--;

				if (i < j)
				{
					server        = serverList[j];
					serverList[j] = serverList[i];
					serverList[i] = server;
					i++, j--;
				}
			}
			else
				i++;
		}
	
		/* If there are no servers with locked positions, then the first
		 * unlocked server is the first server.
		 */
		if (i == 0 && !DIR_TestFlag(serverList[0], DIR_POSITION_LOCKED))
			first_unlocked = 0;

		/* Otherwise, the first unlocked server is at the index of the
		 * last locked server plus one.  Sort the subset of locked
		 * servers.
		 */
		else
		{
			first_unlocked = i;
			dir_SortServersByPosition(serverList, first_unlocked);
		}

		/* Sort the subset of servers with unlocked positions.
		 */
		dir_SortServersByPosition(&serverList[first_unlocked], count - first_unlocked);
	
		/* Merge the two sub-lists by moving servers with unlocked positions
		 * into the holes in the servers with locked positions.
		 */
		for (pos = 1, i = 0; i < first_unlocked; pos++, i++)
		{
			if (serverList[i]->position != pos && first_unlocked < count)
			{
				server                     = serverList[i];
				serverList[i]              = serverList[first_unlocked];
				serverList[first_unlocked] = server;

				serverList[i]->position = pos;
				first_unlocked++;
			}
		}

		/* Adjust the position values of the remaining unlocked servers (those
		 * not moved to fill holes in the locked servers).
		 */
		for (i = first_unlocked; i < count; i++)
			serverList[i]->position = pos++;

		/* Delete the old list.  We can't just call delete because
		 * we must use the same list as passed in.
		 */
		for (i = count - 1; i >= 0; i--)
			wholeList->RemoveElementAt(i);

		/* Create a new list based on the array
		 */
		for (i = 0; i < count; i++)
			wholeList->InsertElementAt(serverList[i], i);

		/* Any time the unified server list is scrambled we need to notify
		 * whoever is interested.
		 */
		if (wholeList == dir_ServerList)
			DIR_SendNotification(nsnull, DIR_NOTIFY_SCRAMBLE, idNone);
	}

	PR_Free(serverList);

	return in_order == 0;
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
    nsresult rv = NS_OK;
    nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
    if (NS_FAILED(rv) || !pPref) 
		return PR_FALSE;

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

		if (wholeList == dir_ServerList)
			DIR_SendNotification(server, DIR_NOTIFY_ADD, idNone);
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
			char *name = PR_smprintf("%s.position", server->prefName);

			if (name != nsnull)
			{
				/* We can't just delete default servers, we can only mark them
				 * as "deleted" by setting their position to 0.  Since there
				 * doesn't seem to be a way to figure out if a server is a
				 * default server we must set the position of all deleted
				 * servers to 0.
				 */
       			DIR_ClearPrefBranch(server->prefName);
				pPref->SetIntPref(name, 0);
				PR_smprintf_free(name);
			}
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

			if (wholeList == dir_ServerList)
				DIR_SendNotification(server, DIR_NOTIFY_DELETE, idNone);
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

			if (wholeList == dir_ServerList)
				DIR_SendNotification(server, DIR_NOTIFY_ADD, idNone);
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

	/* If necessary, re-sort the server list.  This function tries to keep from
	 * calling the sort function when it can, but complicated cases may be
	 * more appropriate to leave up to the sort function to see if the list
	 * needs to be re-sorted.
	 */
	if (resort)
		resort = DIR_SortServersByPosition(wholeList);

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
			char tempstring[256];

			PL_strncpyz(prefname, fullprefname, endname - fullprefname + 1);

			dirType = DIR_GetIntPref(prefname, "dirType", tempstring, -1);
			if (   dirType != -1
			    && DIR_GetIntPref(prefname, "position", tempstring, 0)    != 0
			    && (t1 = DIR_GetStringPref(prefname, "description", tempstring, nsnull)) != nsnull)
			{
				if (   dirType == PABDirectory
					|| (t2 = DIR_GetStringPref(prefname, "serverName",  tempstring, nsnull)) != nsnull)
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

static PRInt32 PR_CALLBACK dir_ServerPrefCallback(const char *prefname, void *inst_data)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
    if (NS_FAILED(rv) || !pPref) 
		return NS_ERROR_FAILURE;

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
		if (   id == idServerName || id == idSearchBase
			|| id == idEnableAuth || id == idAuthDn || id == idPassword)
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
			pPref->GetIntPref(prefname, &position);
			if (position != server->position)
			{
				server->position = position;
				if (dir_IsServerDeleted(server))
					DIR_SetServerPosition(dir_ServerList, server, DIR_POS_DELETE);
				else
					DIR_SendNotification(server, DIR_NOTIFY_PROPERTY_CHANGE, idPosition);
			}
		}

		/* Some pref other position changed, reload the server and send a property
		 * changed notification.
		 */
		else if (dir_CallbackList != nsnull)
		{
			DIR_GetPrefsForOneServer(server, PR_TRUE, PR_FALSE);
			DIR_SendNotification(server, DIR_NOTIFY_PROPERTY_CHANGE, id);
		}
	}

	/* If the server is not in the unified list, we may need to add it.  Servers
	 * are only added when the position, serverName and description are valid.
	 */
	else
	{
		if (id == idPosition || id == idType || id == idServerName || id == idDescription)
			dir_ValidateAndAddNewServer(dir_ServerList, prefname);
	}

    return NS_OK;
}

PRBool DIR_RegisterNotificationCallback(DIR_NOTIFICATION_FN fn, PRUint32 flags, void *inst_data)
{
	DIR_Callback *cb;

	for (cb = dir_CallbackList; cb; cb = cb->next)
	{
		if (cb->fn == fn)
		{
			cb->flags = flags;
			return PR_TRUE;
		}
	}

	cb = (DIR_Callback *)PR_Malloc(sizeof(DIR_Callback));
	if (!cb)
		return PR_FALSE;

	cb->fn    = fn;
	cb->flags = flags;
	cb->data  = inst_data;
	cb->next  = dir_CallbackList;
	dir_CallbackList = cb;

	return PR_TRUE;
}

PRBool DIR_DeregisterNotificationCallback(DIR_NOTIFICATION_FN fn, void *inst_data)
{
	DIR_Callback *cb, *cbPrev=nsnull;

	for (cb = dir_CallbackList; cb && cb->fn != fn && cb->data != inst_data; cb = cb->next)
		cbPrev = cb;

	if (cb == nsnull)
		return PR_FALSE;

	if (cb == dir_CallbackList)
		dir_CallbackList = cb->next;
	else
		cbPrev->next = cb->next;

	PR_Free(cb);
	return PR_TRUE;
}

PRBool DIR_SendNotification(DIR_Server *server, PRUint32 flag, DIR_PrefId id)
{
	PRBool sent = PR_FALSE;
	DIR_Callback *cb, *cbNext;

	for (cb = dir_CallbackList; cb; cb = cbNext)
	{
		cbNext = cb->next;

		if (cb->flags & flag)
		{
			sent = PR_TRUE;
			cb->fn(server, flag, id, cb->data);
		}
	}

	return sent;
}


char *DIR_CopyServerStringPref(DIR_Server *server, DIR_PrefId prefid, int16 csid)
{
	char *pref;

	if (!server)
		return nsnull;

	switch (prefid) {
	case idAuthDn:
		pref = server->authDn;
		break;
	case idPassword:
		pref = server->password;
		break;
	case idSearchBase:
		pref = server->searchBase;
		break;
	default:
		PR_ASSERT(0);
		pref = nsnull;
		break;
	}

	if (pref)
		pref = DIR_ConvertFromServerCharSet(server, pref, csid);

	return pref;
}

PRBool DIR_SetServerStringPref(DIR_Server *server, DIR_PrefId prefid, char *pref, PRInt16 csid)
{
	PRBool rc = PR_TRUE;

	if (!server || !pref)
		return PR_FALSE;

	pref = DIR_ConvertToServerCharSet(server, pref, csid);

	switch (prefid) {
	case idAuthDn:
		PR_FREEIF(server->authDn);
		server->authDn = pref;
		break;
	case idPassword:
		PR_FREEIF(server->password);
		server->password = pref;
		break;
	case idSearchBase:
		PR_FREEIF(server->searchBase);
		server->searchBase = pref;
		break;
	default:
		PR_ASSERT(0);
		rc = PR_FALSE;
		break;
	}

	return PR_FALSE;
}


DIR_PrefId DIR_AtomizePrefName(const char *prefname)
{
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
			case 'f':
				rc = idAutoCompleteFilter;
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
		else if (PL_strstr(prefname, "attributes.") == prefname)
		{
			rc = idCustomAttributes;
		}
		break;

	case 'b':
		rc = idBasicSearchAttributes;
		break;

	case 'c':
		switch (prefname[1]) {
		case 'h': /* charset */
			rc = idCSID;
			break;
		case 's': /* the new csid pref that replaced char set */
			rc = idCSID;
			break;
		case 'o': /* columns */
			rc = idColumnAttributes;
			break;
		case 'u': /* customDisplayUrl */
			rc = idCustomDisplayUrl;
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

	case 'e':
		switch (prefname[1]) {
		case 'e': /* efficientWildcards */
			rc = idEfficientWildcards;
			break;
		}
		break;

	case 'f':
		if (PL_strstr(prefname, "filter") == prefname)
		{
			rc = idCustomFilters;
		}
		else
		{
			rc = idFileName;
		}
		break;

	case 'h':
		if (PL_strstr(prefname, "html.") == prefname)
		{
			switch (prefname[5]) {
			case 'd':
				rc = idDnAttributes;
				break;
			case 's':
				rc = idSuppressedAttributes;
				break;
			case 'u':
				rc = idUriAttributes;
				break;
			}
		}
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
				switch (prefname[13]) {
				case 'n': /* replication.enabled */
					rc = idReplEnabled;
					break;
				case 'x': /* replication.excludedAttributes */
					rc = idReplExcludedAttributes;
					break;
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
		switch (prefname[1]) {
		case 'a': /* saveResults */
			rc = idSaveResults;
			break;
		case 'e':
			switch (prefname[2]) {
			case 'a':
				switch (prefname[6]) {
				case 'B': /* searchBase */
					rc = idSearchBase;
					break;
				case 'S': /* searchString */
					rc = idLastSearchString;
					break;
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
			if ((first->isOffline == PR_FALSE) && (second->isOffline == PR_FALSE))  /* are they both really address books? */
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

PRBool	DIR_AreServersSame (DIR_Server *first, DIR_Server *second)
{
	return dir_AreServersSame(first, second, PR_TRUE);
}

DIR_Server *DIR_LookupServer(char *serverName, PRInt32 port, char *searchBase)
{
	PRInt32 i;
	DIR_Server *server;

	if (!serverName || !searchBase || !dir_ServerList)
		return nsnull;

	for (i = dir_ServerList->Count() - 1; i >= 0; i--)
	{
		server = (DIR_Server *)dir_ServerList->ElementAt(i);
		if (   server->port == port
            && server->serverName && nsCRT::strcasecmp(server->serverName, serverName) == 0
            && server->searchBase && nsCRT::strcasecmp(server->searchBase, searchBase) == 0)
		{
			return server;
		}
	}

	return nsnull;
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

static void dir_DeleteTokenList (char **tokenList, PRInt32 tokenListCount)
{
	PRInt32 tokenIdx;
	for (tokenIdx = 0; tokenIdx < tokenListCount; tokenIdx++)
		PR_Free(tokenList[tokenIdx]);
	PR_Free(tokenList);
}


static nsresult DIR_DeleteFilter (DIR_Filter *filter)
{
	if (filter->string)
		PR_Free(filter->string);
	PR_Free(filter);
	return NS_OK;
}


static nsresult DIR_DeleteAttribute (DIR_Attribute *attribute)
{
	PRInt32 i = 0;
	if (attribute->prettyName)
		PR_Free(attribute->prettyName);
	if (attribute->attrNames)
	{
		while (attribute->attrNames[i])
			PR_Free((char**)attribute->attrNames[i++]);
		PR_Free(attribute->attrNames);
	}
	PR_Free(attribute);
	return NS_OK;
}


static void dir_DeleteReplicationInfo (DIR_Server *server)
{
	DIR_ReplicationInfo *info = nsnull;
	if (server && (info = server->replInfo) != nsnull)
	{
		dir_DeleteTokenList (info->excludedAttributes, info->excludedAttributesCount);
		
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
		PRInt32 i;

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
		PR_FREEIF (server->lastSearchString);
		PR_FREEIF (server->tokenSeps);
		PR_FREEIF (server->authDn);
		PR_FREEIF (server->password);
		PR_FREEIF (server->columnAttributes);
		PR_FREEIF (server->locale);
        PR_FREEIF (server->uri);

		if (server->customFilters)
		{
			PRInt32 count = server->customFilters->Count();
			for (i = 0; i < count; i++)
				DIR_DeleteFilter ((DIR_Filter*) server->customFilters->ElementAt(i));
			delete server->customFilters;
		}

		PR_FREEIF (server->autoCompleteFilter);

		if (server->customAttributes)
		{
			nsVoidArray *list = server->customAttributes;
			DIR_Attribute *walkAttrStruct = nsnull;
			PRInt32 count = list->Count();
			for (i = 0; i < count; i++)
			{
				walkAttrStruct = (DIR_Attribute *)list->ElementAt(i);
				if (walkAttrStruct != nsnull)
					DIR_DeleteAttribute (walkAttrStruct);
			}
			delete server->customAttributes;
		}

		if (server->uriAttributes)
			dir_DeleteTokenList (server->uriAttributes, server->uriAttributesCount);
		if (server->suppressedAttributes)
			dir_DeleteTokenList (server->suppressedAttributes, server->suppressedAttributesCount);
		if (server->dnAttributes)
			dir_DeleteTokenList (server->dnAttributes, server->dnAttributesCount);
		PR_FREEIF (server->basicSearchAttributes);
		if (server->replInfo)
			dir_DeleteReplicationInfo (server);

		PR_FREEIF (server->customDisplayUrl);
		PR_FREEIF (server->searchPairList);
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
	nsFileSpec* dbPath = nsnull;

	nsCOMPtr<nsIAddrBookSession> abSession = 
	         do_GetService(kAddrBookSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		abSession->GetUserProfileDirectory(&dbPath);
	
	if (dbPath)
	{
		nsCOMPtr<nsIAddrDatabase> database;

		(*dbPath) += server->fileName;

		// close file before delete it
		nsCOMPtr<nsIAddrDatabase> addrDBFactory = 
		         do_GetService(kAddressBookDBCID, &rv);

		if (NS_SUCCEEDED(rv) && addrDBFactory)
			rv = addrDBFactory->Open(dbPath, PR_FALSE, getter_AddRefs(database), PR_TRUE);
		if (database)  /* database exists */
		{
			database->ForceClosed();
			dbPath->Delete(PR_FALSE);
		}

    delete dbPath;

		nsVoidArray *dirList = DIR_GetDirectories();
		DIR_SetServerPosition(dirList, server, DIR_POS_DELETE);
		DIR_DeleteServer(server);

		rv = NS_OK;
		nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
		if (NS_FAILED(rv) || !pPref) 
			return NS_ERROR_FAILURE;
		pPref->SavePrefFile(nsnull);

		return NS_OK;
	}
	return NS_ERROR_NULL_POINTER;
}

nsresult DIR_DeleteServerList(nsVoidArray *wholeList)
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
	return NS_OK;
}


nsresult DIR_CleanUpServerPreferences(nsVoidArray *deletedList)
{
	/* OBSOLETE  OBSOLETE  OBSOLETE  OBSOLETE  OBSOLETE  OBSOLETE  OBSOLETE */
	PR_ASSERT(PR_FALSE);
	return NS_OK;
	/* OBSOLETE  OBSOLETE  OBSOLETE  OBSOLETE  OBSOLETE  OBSOLETE  OBSOLETE */
}


/*****************************************************************************
 * Functions for retrieving subsets of the DIR_Server list 
 */

#define DIR_SUBSET_MATCH(_SERVER, _FLAGS)                                                 \
	(   (   (_FLAGS & DIR_SUBSET_PAB_ALL          ) && PABDirectory  == _SERVER->dirType) \
	 || (   (_FLAGS & DIR_SUBSET_HTML_ALL         ) && HTMLDirectory == _SERVER->dirType) \
	 || (   (_FLAGS & DIR_SUBSET_LDAP_ALL         ) && LDAPDirectory == _SERVER->dirType) \
	 || (   (_FLAGS & DIR_SUBSET_LDAP_AUTOCOMPLETE) && LDAPDirectory == _SERVER->dirType  \
		 && !DIR_TestFlag(s, DIR_AUTO_COMPLETE_NEVER))                                    \
	 || (   (_FLAGS & DIR_SUBSET_LDAP_REPLICATE   ) && LDAPDirectory == _SERVER->dirType  \
		 && !DIR_TestFlag(s, DIR_REPLICATE_NEVER))                                        \
    )

nsresult DIR_GetDirServerSubset(nsVoidArray *wholeList, nsVoidArray *subList, PRUint32 flags)
{
	if (wholeList && subList && flags)
	{
		PRInt32 i;
		PRInt32 numItems = wholeList->Count();

		for (i = 0; i < numItems; i++)
		{
			DIR_Server *s = (DIR_Server*) wholeList->ElementAt(i);
			if (DIR_SUBSET_MATCH(s, flags))
			{
				subList->AppendElement(s);
			}
		}
		return NS_OK;
	}
	return NS_ERROR_FAILURE;
}

PRInt32 DIR_GetDirServerSubsetCount(nsVoidArray * wholeList, PRUint32 flags)
{
	PRInt32 count = 0;

	if (wholeList && flags)
	{
		PRInt32 i;
		PRInt32 numItems = wholeList->Count();

		for (i = 0; i < numItems; i++)
		{
			DIR_Server *s = (DIR_Server*) wholeList->ElementAt(i);
			if (DIR_SUBSET_MATCH(s, flags))
			{
				count++;
			}
		}
	}

	return count;
}

nsresult DIR_GetComposeNameCompletionAddressBook (nsVoidArray *wholeList, DIR_Server **cab)
{
	/* OBSOLETE  OBSOLETE  OBSOLETE  OBSOLETE  OBSOLETE  OBSOLETE  OBSOLETE */
	PR_ASSERT(PR_FALSE);
    return NS_ERROR_FAILURE;
	/* OBSOLETE  OBSOLETE  OBSOLETE  OBSOLETE  OBSOLETE  OBSOLETE  OBSOLETE */
}

nsresult DIR_GetLdapServers (nsVoidArray *wholeList, nsVoidArray *subList)
{
	/* OBSOLETE  OBSOLETE  OBSOLETE  OBSOLETE  OBSOLETE  OBSOLETE  OBSOLETE */
	return DIR_GetDirServerSubset(wholeList, subList, DIR_SUBSET_LDAP_ALL);
	/* OBSOLETE  OBSOLETE  OBSOLETE  OBSOLETE  OBSOLETE  OBSOLETE  OBSOLETE */
}

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


/*****************************************************************************
 * Functions for managing JavaScript prefs for the DIR_Servers 
 */

#if !defined(MOZADDRSTANDALONE)

static char *DIR_GetStringPref(const char *prefRoot, const char *prefLeaf, char *scratch, const char *defaultValue)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
    if (NS_FAILED(rv) || !pPref) 
		return nsnull;

	char *value = nsnull;
	PL_strcpy(scratch, prefRoot);
	PL_strcat(scratch, ".");
	PL_strcat(scratch, prefLeaf);
 
	if (PREF_NOERROR == pPref->CopyCharPref(scratch, &value))
	{
		/* unfortunately, there may be some prefs out there which look like this */
		if (!PL_strcmp(value, "(null)")) 
		{
			PR_FREEIF(value); /* free old value because we are going to give it a new value.... */
            value = defaultValue ? nsCRT::strdup(defaultValue) : nsnull;
		}
		if (PL_strlen(value) == 0)
		{
			PR_FREEIF(value);
			pPref->CopyDefaultCharPref(scratch, &value);
		}
	}
	else
	{
		PR_FREEIF(value); /* the pref may have generated an error but we still might have something in value...... */
        value = defaultValue ? nsCRT::strdup(defaultValue) : nsnull;
	}
	return value;
}

/*
	Get localized unicode string pref from properties file, convert into an UTF8 string 
	since address book prefs store as UTF8 strings.  So far there are 2 default 
	prefs stored in addressbook.properties.
	"ldap_2.servers.pab.description"
	"ldap_2.servers.history.description"
*/
static char *DIR_GetLocalizedStringPref
(const char *prefRoot, const char *prefLeaf, char *scratch, const char *defaultValue)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
    if (NS_FAILED(rv) || !pPref) 
		return nsnull;

	PL_strcpy(scratch, prefRoot);
	PL_strcat(scratch, ".");
	PL_strcat(scratch, prefLeaf);

	nsXPIDLString wvalue;
	rv = pPref->GetLocalizedUnicharPref(scratch, getter_Copies(wvalue));
	char *value = nsnull;
	if ((const PRUnichar*)wvalue)
	{
		nsString descString(wvalue);
		PRInt32 unicharLength = descString.Length();
		// convert to UTF8 string
		INTL_ConvertFromUnicode(wvalue, unicharLength, &value);
	}
	else
        value = defaultValue ? nsCRT::strdup(defaultValue) : nsnull;

	return value;
}

static PRInt32 DIR_GetIntPref(const char *prefRoot, const char *prefLeaf, char *scratch, PRInt32 defaultValue)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
    if (NS_FAILED(rv) || !pPref) 
		return defaultValue;

	PRInt32 value;
	PL_strcpy(scratch, prefRoot);
	PL_strcat(scratch, ".");
	PL_strcat(scratch, prefLeaf);

	if (PREF_NOERROR != pPref->GetIntPref(scratch, &value))
		value = defaultValue;

	return value;
}


static PRBool DIR_GetBoolPref(const char *prefRoot, const char *prefLeaf, char *scratch, PRBool defaultValue)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
    if (NS_FAILED(rv) || !pPref) 
		return defaultValue;

	PRBool value;
	PL_strcpy(scratch, prefRoot);
	PL_strcat(scratch, ".");
	PL_strcat(scratch, prefLeaf);

	if (PREF_NOERROR != pPref->GetBoolPref(scratch, &value))
		value = defaultValue;
	return value;
}


nsresult DIR_AttributeNameToId(DIR_Server *server, const char *attrName, DIR_AttributeId *id)
{
	nsresult status = NS_OK;

	/* Look for a default attribute with a matching name.
	 */
	switch (attrName[0])
	{
	case 'a':
        if (!nsCRT::strcasecmp(attrName, "auth"))
			*id = auth;
		else
			status = NS_ERROR_FAILURE;
		break;
	case 'b':
        if (!nsCRT::strcasecmp(attrName, "businesscategory"))
			*id = businesscategory;
		else
			status = NS_ERROR_FAILURE;
		break;
	case 'c' :
        if (!nsCRT::strcasecmp(attrName, "cn"))
			*id = cn;
        else if (!nsCRT::strcasecmp(attrName, "carlicense"))
			*id = carlicense;
        else if (!nsCRT::strncasecmp(attrName, "custom", 6))
		{
			switch (attrName[6])
			{
			case '1': *id = custom1; break;
			case '2': *id = custom2; break;
			case '3': *id = custom3; break;
			case '4': *id = custom4; break;
			case '5': *id = custom5; break;
			default: status = NS_ERROR_FAILURE; 
			}
		}
		else
			status = NS_ERROR_FAILURE;
		break;
	case 'd':
        if (!nsCRT::strcasecmp(attrName, "departmentnumber"))
			*id = departmentnumber;
		else
            if (!nsCRT::strcasecmp(attrName, "description"))
				*id = description;
		else
			status = NS_ERROR_FAILURE;
		break;
	case 'e':
        if (!nsCRT::strcasecmp(attrName, "employeetype"))
			*id = employeetype;
		else
			status = NS_ERROR_FAILURE;
		break;
	case 'f':
        if (!nsCRT::strcasecmp(attrName, "facsimiletelephonenumber"))
			*id = facsimiletelephonenumber;
		else
			status = NS_ERROR_FAILURE;
		break;
	case 'g':
        if (!nsCRT::strcasecmp(attrName, "givenname"))
			*id = givenname; 
		else
			status = NS_ERROR_FAILURE;
		break;
	case 'h':
        if (!nsCRT::strcasecmp(attrName, "homephone"))
			*id = homephone;
		else
			status = NS_ERROR_FAILURE;
		break;
	case 'l':
        if (!nsCRT::strcasecmp(attrName, "l"))
			*id = l;
		else
			status = NS_ERROR_FAILURE;
		break;
	case 'm':
        if (!nsCRT::strcasecmp(attrName, "mail"))
			*id = mail;
        else if (!nsCRT::strcasecmp(attrName, "manager"))
			*id = manager;
        else if (!nsCRT::strcasecmp(attrName, "mobiletelephonenumber"))
			*id = mobiletelephonenumber;
		else
			status = NS_ERROR_FAILURE;
		break;
	case 'n':
        if (!nsCRT::strcasecmp(attrName, "nickname"))
			*id = nickname;
		else
			status = NS_ERROR_FAILURE;
		break;
	case 'o':
        if (!nsCRT::strcasecmp(attrName, "o"))
			*id = o;
        else if (!nsCRT::strcasecmp(attrName, "ou"))
			*id = ou;
        else if (!nsCRT::strcasecmp(attrName, "objectclass"))
			*id = objectclass;
		else
			status = NS_ERROR_FAILURE;
		break;
	case 'p':
        if (!nsCRT::strcasecmp(attrName, "pager"))
			*id = pager;
        else if (!nsCRT::strcasecmp(attrName, "postalcode"))
			*id = postalcode;
        else if (!nsCRT::strcasecmp(attrName, "postaladdress"))
			*id = postaladdress;
		else
			status = NS_ERROR_FAILURE;
		break;
	case 's': 
        if (!nsCRT::strcasecmp(attrName, "street"))
			*id = street;
        else if (!nsCRT::strcasecmp(attrName, "sn"))
			*id = sn;
        else if (!nsCRT::strcasecmp(attrName, "secretary"))
			*id = secretary;
		else
			status = NS_ERROR_FAILURE;
		break;
	case 't':
        if (!nsCRT::strcasecmp(attrName, "telephonenumber"))
			*id = telephonenumber;
        else if (!nsCRT::strcasecmp(attrName, "title"))
			*id = title;
		else
			status = NS_ERROR_FAILURE;
		break;
	default:
		status = NS_ERROR_FAILURE;
	}

	return status;
}

static nsresult DIR_AddCustomAttribute(DIR_Server *server, const char *attrName, char *jsAttr)
{
	nsresult status = NS_OK;
	char *jsCompleteAttr = nsnull;
	char *jsAttrForTokenizing = jsAttr;

	DIR_AttributeId id;
	status = DIR_AttributeNameToId(server, attrName, &id);

	/* If the string they gave us doesn't have a ':' in it, assume it's one or more
	 * attributes without a pretty name. So find the default pretty name, and generate
	 * a "complete" string to use for tokenizing.
	 */
	if (NS_SUCCEEDED(status) && !PL_strchr(jsAttr, ':'))
	{
		const char *defaultPrettyName = DIR_GetAttributeName (server, id);
		if (defaultPrettyName)
		{
			jsCompleteAttr = PR_smprintf ("%s:%s", defaultPrettyName, jsAttr);
			if (jsCompleteAttr)
				jsAttrForTokenizing = jsCompleteAttr;
			else
				status = NS_ERROR_OUT_OF_MEMORY;
		}
	}

	if (NS_SUCCEEDED(status))
	{
        char *scratchAttr = nsCRT::strdup(jsAttrForTokenizing);
		DIR_Attribute *attrStruct = (DIR_Attribute*) PR_Malloc(sizeof(DIR_Attribute));
		if (!server->customAttributes)
			server->customAttributes = new nsVoidArray();

		if (attrStruct && server->customAttributes && scratchAttr)
		{
			char *attrToken = nsnull;
			PRUint32 attrCount = 0;

			XP_BZERO(attrStruct, sizeof(DIR_Attribute));

			/* Try to pull out the pretty name into the struct */
			attrStruct->id = id;
            attrStruct->prettyName = nsCRT::strdup(XP_STRTOK(scratchAttr, ":")); 

			/* Count up the attribute names */
			while ((attrToken = XP_STRTOK(nsnull, ", ")) != nsnull)
				attrCount++;

			/* Pull the attribute names into the struct */
			PL_strcpy(scratchAttr, jsAttrForTokenizing);
			XP_STRTOK(scratchAttr, ":"); 
			attrStruct->attrNames = (char**) PR_Malloc((attrCount + 1) * sizeof(char*));
			if (attrStruct->attrNames)
			{
				PRInt32 i = 0;
				while ((attrToken = XP_STRTOK(nsnull, ", ")) != nsnull)
                    attrStruct->attrNames[i++] = nsCRT::strdup(attrToken);
				attrStruct->attrNames[i] = nsnull; /* null-terminate the array */
			}

			if (NS_SUCCEEDED(status)) /* status is always NS_OK! */
				server->customAttributes->AppendElement(attrStruct);
			else
				DIR_DeleteAttribute (attrStruct);

			PR_Free(scratchAttr);
		}
		else 
			status = NS_ERROR_OUT_OF_MEMORY;
	}

	if (jsCompleteAttr)
		PR_smprintf_free(jsCompleteAttr);

	return status;
}

PRInt32 DIR_GetNumAttributeIDsForColumns(DIR_Server * server)
{
	PRInt32 count = 0;
	char * buffer = nsnull;
	char * marker = nsnull;
	if (server && server->columnAttributes)
	{
        buffer = nsCRT::strdup(server->columnAttributes);
		if (buffer)
		{
			marker = buffer;
			while (AB_pstrtok_r(nil, ", ", &buffer))
				count++;
			PR_FREEIF(marker);
		}
	}

	return count;
}

/* caller must free returned list of ids */
nsresult DIR_GetAttributeIDsForColumns(DIR_Server *server, DIR_AttributeId ** ids, PRInt32 * numIds)
{
	DIR_AttributeId * idArray = nsnull;
	nsresult status = NS_OK; 
	PRInt32 numAdded = 0;  /* number of ids we actually added to the array...*/
	PRInt32 indx = 0;
	PRInt32 numItems = 0; 
	char * idName = nsnull;
	char * marker = nsnull;
	char * columnIDs = nsnull;

	if (server && numIds && ids)
	{
		if (server->columnAttributes) 
		{
            columnIDs = nsCRT::strdup(server->columnAttributes);
			numItems = DIR_GetNumAttributeIDsForColumns(server);
		}

		if (columnIDs && numItems)
		{
			marker = columnIDs;
			idArray = (DIR_AttributeId *) PR_Malloc(sizeof(DIR_AttributeId) * numItems);
			if (idArray)
			{
				for (indx = 0; indx < numItems; indx++)
				{
					idName = AB_pstrtok_r(nil,", ",&marker);
					if (idName)
					{
						status = DIR_AttributeNameToId(server, idName, &idArray[numAdded]);
						if (NS_SUCCEEDED(status))
							numAdded++;
					}
					else
						break;
				}

			}
			else
				status = NS_ERROR_OUT_OF_MEMORY;
		}

		PR_FREEIF(columnIDs); 
	}

	if (ids)
		*ids = idArray;
	if (numIds)
		*numIds = numAdded;

	return status;
}

static nsresult dir_CreateTokenListFromWholePref(const char *pref, char ***outList, PRInt32 *outCount)
{
    nsresult result = NS_OK;
    nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &result)); 
    if (NS_FAILED(result)) 
		return result;

	char *commaSeparatedList = nsnull;

	if (PREF_NOERROR == pPref->CopyCharPref(pref, &commaSeparatedList) && commaSeparatedList)
	{
		char *tmpList = commaSeparatedList;
		*outCount = 1;
		while (*tmpList)
			if (*tmpList++ == ',')
				(*outCount)++;

		*outList = (char**) PR_Malloc(*outCount * sizeof(char*));
		if (*outList)
		{
			PRInt32 i;
			char *token = XP_STRTOK(commaSeparatedList, ", ");
			for (i = 0; i < *outCount; i++)
			{
                (*outList)[i] = nsCRT::strdup(token);
				token = XP_STRTOK(nsnull, ", ");
			}
		}
		else
			result = NS_ERROR_OUT_OF_MEMORY;

		PR_Free (commaSeparatedList);
	}
	else
		result = NS_ERROR_FAILURE;
	return result;
}


static nsresult dir_CreateTokenListFromPref
(const char *prefBase, const char *prefLeaf, char *scratch, char ***outList, PRInt32 *outCount)
{
	PL_strcpy (scratch, prefBase);
	PL_strcat (scratch, ".");
	PL_strcat (scratch, prefLeaf);

	return dir_CreateTokenListFromWholePref(scratch, outList, outCount);
}


static nsresult dir_ConvertTokenListToIdList
(DIR_Server *server, char **tokenList, PRInt32 tokenCount, DIR_AttributeId **outList)
{
	*outList = (DIR_AttributeId*) PR_Malloc(sizeof(DIR_AttributeId) * tokenCount);
	if (*outList)
	{
		PRInt32 i;
		for (i = 0; i < tokenCount; i++)
			DIR_AttributeNameToId(server, tokenList[i], &(*outList)[i]);
	}
	else
		return NS_ERROR_OUT_OF_MEMORY;
	return NS_OK;
}


static void dir_GetReplicationInfo(const char *prefstring, DIR_Server *server, char *scratch)
{
	char replPrefName[128];
	PR_ASSERT(server->replInfo == nsnull);

	server->replInfo = (DIR_ReplicationInfo *)PR_Calloc(1, sizeof (DIR_ReplicationInfo));
	if (server->replInfo && replPrefName)
	{
		PRBool prefBool;

		PL_strcpy(replPrefName, prefstring);
		PL_strcat(replPrefName, ".replication");

		prefBool = DIR_GetBoolPref(replPrefName, "never", scratch, kDefaultReplicateNever);
		DIR_ForceFlag(server, DIR_REPLICATE_NEVER, prefBool);

		prefBool = DIR_GetBoolPref(replPrefName, "enabled", scratch, kDefaultReplicaEnabled);
		DIR_ForceFlag(server, DIR_REPLICATION_ENABLED, prefBool);

		server->replInfo->description = DIR_GetStringPref(replPrefName, "description", scratch, kDefaultReplicaDescription);
		server->replInfo->syncURL = DIR_GetStringPref(replPrefName, "syncURL", scratch, nsnull);
		server->replInfo->filter = DIR_GetStringPref(replPrefName, "filter", scratch, kDefaultReplicaFilter);

		dir_CreateTokenListFromPref(replPrefName, "excludedAttributes", scratch, &server->replInfo->excludedAttributes, 
		                            &server->replInfo->excludedAttributesCount);

		/* The file name and data version must be set or we ignore the
		 * remaining replication prefs.
		 */
		server->replInfo->fileName = DIR_GetStringPref(replPrefName, "fileName", scratch, kDefaultReplicaFileName);
		server->replInfo->dataVersion = DIR_GetStringPref(replPrefName, "dataVersion", scratch, kDefaultReplicaDataVersion);
		if (server->replInfo->fileName && server->replInfo->dataVersion)
		{
			server->replInfo->lastChangeNumber = DIR_GetIntPref(replPrefName, "lastChangeNumber", scratch, kDefaultReplicaChangeNumber);
		}
	}
}


/* Called at startup-time to read whatever overrides the LDAP site administrator has
 * done to the attribute names
 */
static nsresult DIR_GetCustomAttributePrefs(const char *prefstring, DIR_Server *server, char *scratch)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
    if (NS_FAILED(rv) || !pPref) 
		return NS_ERROR_FAILURE;

	char **tokenList = nsnull;
	char *childList = nsnull;

	PL_strcpy(scratch, prefstring);
	PL_strcat(scratch, ".attributes");

	if (PREF_NOERROR == pPref->CreateChildList(scratch, &childList))
	{
		if (childList && childList[0])
		{
			char *child = nsnull;
			PRInt16 indx = 0;
			while ((pPref->NextChild (childList, &indx, &child)) == NS_OK)
			{
				char *jsValue = nsnull;
				if (PREF_NOERROR == pPref->CopyCharPref (child, &jsValue))
				{
					if (jsValue && jsValue[0])
					{
						char *attrName = child + PL_strlen(scratch) + 1;
						DIR_AddCustomAttribute (server, attrName, jsValue);
					}
					PR_FREEIF(jsValue);
				}
			}
		}

		PR_FREEIF(childList);
	}

	if (0 == dir_CreateTokenListFromPref (prefstring, "basicSearchAttributes", scratch, 
		&tokenList, &server->basicSearchAttributesCount))
	{
		dir_ConvertTokenListToIdList (server, tokenList, server->basicSearchAttributesCount, 
			&server->basicSearchAttributes);
		dir_DeleteTokenList (tokenList, server->basicSearchAttributesCount);
	}

	/* The DN, suppressed and url attributes can be attributes that
	 * we've never heard of, so they're stored by name, so we can match 'em
	 * as we get 'em from the server
	 */
	dir_CreateTokenListFromPref (prefstring, "html.dnAttributes", scratch, 
		&server->dnAttributes, &server->dnAttributesCount);
	dir_CreateTokenListFromPref (prefstring, "html.excludedAttributes", scratch, 
		&server->suppressedAttributes, &server->suppressedAttributesCount);
	dir_CreateTokenListFromPref (prefstring, "html.uriAttributes", scratch, 
		&server->uriAttributes, &server->uriAttributesCount);

	return NS_OK;
}


/* Called at startup-time to read whatever overrides the LDAP site administrator has
 * done to the filtering logic
 */
static nsresult DIR_GetCustomFilterPrefs(const char *prefstring, DIR_Server *server, char *scratch)
{
    nsresult status = NS_OK;
    nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &status)); 
    if (NS_FAILED(status) || !pPref) 
		return NS_ERROR_FAILURE;

	PRBool keepGoing = PR_TRUE;
	PRInt32 filterNum = 1;
	char *localScratch = (char*)PR_Malloc(128);
	if (!localScratch)
		return NS_ERROR_OUT_OF_MEMORY;

	server->tokenSeps = DIR_GetStringPref (prefstring, "wordSeparators", localScratch, kDefaultTokenSeps);
	while (keepGoing && NS_SUCCEEDED(status))
	{
		char *childList = nsnull;

		PR_snprintf (scratch, 128, "%s.filter%d", prefstring, filterNum);
		if (PREF_NOERROR == pPref->CreateChildList(scratch, &childList))
		{
			if ('\0' != childList[0])
			{
				DIR_Filter *filter = (DIR_Filter*) PR_Malloc (sizeof(DIR_Filter));
				if (filter)
				{
					PRBool tempBool;
					XP_BZERO(filter, sizeof(DIR_Filter));

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
						server->customFilters = new nsVoidArray();
					if (server->customFilters)
						server->customFilters->AppendElement(filter);
					else
						status = NS_ERROR_OUT_OF_MEMORY;
				}
				else
					status = NS_ERROR_OUT_OF_MEMORY;
				filterNum++;
			}
			else
				keepGoing = PR_FALSE;
			PR_Free(childList);
		}
		else
			keepGoing = PR_FALSE;
	}

	PR_Free(localScratch);
	return status;
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
	newLeafName = XP_STRRCHR (leafName, '/');
#endif
    pServer->fileName = newLeafName ? nsCRT::strdup(newLeafName + 1) : nsCRT::strdup(leafName);
	if (leafName) PR_Free(leafName);
}

/* This will generate a correct filename and then remove the path */
void DIR_SetFileName(char** fileName, const char* defaultName)
{
	nsresult rv = NS_OK;
	nsFileSpec* dbPath = nsnull;

	nsCOMPtr<nsIAddrBookSession> abSession = 
	         do_GetService(kAddrBookSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		abSession->GetUserProfileDirectory(&dbPath);
	if (dbPath)
	{
		(*dbPath) += defaultName;
		dbPath->MakeUnique(defaultName);
		char* file = nsnull;
		file = dbPath->GetLeafName();
        *fileName = nsCRT::strdup(file);
		if (file)
			nsCRT::free(file);

      delete dbPath;
	}
}

/****************************************************************
Helper function used to generate a file name from the description
of a directory. Caller must free returned string. 
An extension is not applied 
*****************************************************************/

char * dir_ConvertDescriptionToPrefName(DIR_Server * server)
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
		/* make sure we have a pref name...*/
		if (!server->prefName || !*server->prefName)
			server->prefName = DIR_CreateServerPrefName (server, nsnull);

		/* set default personal address book file name*/
		if (server->position == 1)
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
					server->fileName = PR_smprintf("%s%s", tempName, ABFileName_kCurrentSuffix);
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

/* This will reconstruct a correct filename including the path */
void DIR_GetServerFileName(char** filename, const char* leafName)
{
#ifdef XP_PlatformFileToURL
#ifdef XP_MAC
	char* realLeafName;
	char* nativeName;
	char* urlName;
	if (PL_strchr(leafName, ':') != nsnull)
		realLeafName = PL_strrchr(leafName, ':') + 1;	/* makes sure that leafName is not a fullpath */
	else
		realLeafName = leafName;

	nativeName = WH_FileName(realLeafName, xpAddrBookNew);
	urlName = XP_PlatformFileToURL(nativeName);
    (*filename) = nsCRT::strdup(urlName + PL_strlen("file://"));
	if (urlName) PR_Free(urlName);
#elif defined(XP_WIN)
	/* jefft -- Bug 73349. To allow users share same address book.
	 * This only works if user sepcified a full path name in his
	 * prefs.js
	 */ 
	char *nativeName = WH_FilePlatformName (leafName);
	char *fullnativeName;
#ifdef XP_FileIsFullPath
	if (XP_FileIsFullPath(nativeName)) {
		fullnativeName = nativeName;
		nativeName = nsnull;
	}
	else {
		fullnativeName = WH_FileName(nativeName, xpAddrBookNew);
	}
#endif /* XP_FileIsFullPath */
	(*filename) = fullnativeName;
#else
	char* nativeName = WH_FilePlatformName (leafName);
	char* fullnativeName = WH_FileName(nativeName, xpAddrBookNew);
	(*filename) = fullnativeName;
#endif
	if (nativeName) PR_Free(nativeName);
#endif /* XP_PlatformFileToURL */
}

char *DIR_CreateServerPrefName (DIR_Server *server, char *name)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
    if (NS_FAILED(rv) || !pPref) 
		return nsnull;

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
		char * children = nsnull;
		char * child = nsnull;
		/* we need to verify that this pref string name is unique */
		prefName = PR_smprintf(PREF_LDAP_SERVER_TREE_NAME".%s", leafName);
		isUnique = PR_FALSE;
		while (!isUnique && prefName)
		{ 
			isUnique = PR_TRUE; /* now flip the logic and assume we are unique until we find a match */
			if (pPref->CreateChildList(PREF_LDAP_SERVER_TREE_NAME, &children) == PREF_NOERROR)
			{
				PRInt16 i = 0; 
				while ( (pPref->NextChild(children, &i, &child)) == NS_OK && isUnique)
				{
                    if (!nsCRT::strcasecmp(child, prefName) ) /* are they the same name?? */
						isUnique = PR_FALSE;
				}
				PR_FREEIF(children);
				if (!isUnique) /* then try generating a new pref name and try again */
				{
					PR_smprintf_free(prefName);
					prefName = PR_smprintf(PREF_LDAP_SERVER_TREE_NAME".%s_%d", leafName, ++uniqueIDCnt);
				}
			} /* if we have a list of pref Names */
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
    nsresult rv = NS_OK;
    nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
    if (NS_FAILED(rv) || !pPref) 
		return;

	PRBool  prefBool;
	char    *prefstring = server->prefName;
	char     tempstring[256];
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
	server->position = DIR_GetIntPref (prefstring, "position", tempstring, kDefaultPosition);
	PRBool bIsLocked;
	pPref->PrefIsLocked(tempstring, &bIsLocked);
	DIR_ForceFlag(server, DIR_UNDELETABLE | DIR_POSITION_LOCKED, bIsLocked);

	server->isSecure = DIR_GetBoolPref (prefstring, "isSecure", tempstring, PR_FALSE);
	server->saveResults = DIR_GetBoolPref (prefstring, "saveResults", tempstring, PR_TRUE);				
	server->efficientWildcards = DIR_GetBoolPref (prefstring, "efficientWildcards", tempstring, PR_TRUE);				
	server->port = DIR_GetIntPref (prefstring, "port", tempstring, server->isSecure ? LDAPS_PORT : LDAP_PORT);
	if (server->port == 0)
		server->port = server->isSecure ? LDAPS_PORT : LDAP_PORT;
	server->maxHits = DIR_GetIntPref (prefstring, "maxHits", tempstring, kDefaultMaxHits);

	if (0 == PL_strcmp(prefstring, "ldap_2.servers.pab") || 
		0 == PL_strcmp(prefstring, "ldap_2.servers.history")) 
	{
		// get default address book name from addressbook.properties 
		server->description = DIR_GetLocalizedStringPref(prefstring, "description", tempstring, "");
	}
	else
		server->description = DIR_GetStringPref (prefstring, "description", tempstring, "");

	server->serverName = DIR_GetStringPref (prefstring, "serverName", tempstring, "");
	server->searchBase = DIR_GetStringPref (prefstring, "searchBase", tempstring, "");
	server->isOffline = DIR_GetBoolPref (prefstring, "isOffline", tempstring, kDefaultIsOffline);
	server->dirType = (DirectoryType)DIR_GetIntPref (prefstring, "dirType", tempstring, LDAPDirectory);
	if (server->dirType == PABDirectory)
	{
		/* make sure there is a PR_TRUE PAB */
		if (PL_strlen (server->serverName) == 0)
			server->isOffline = PR_FALSE;
		server->saveResults = PR_TRUE; /* never let someone delete their PAB this way */
	}

	/* load in the column attributes */
	if (server->dirType == PABDirectory)
		server->columnAttributes = DIR_GetStringPref(prefstring, "columns", tempstring, kDefaultPABColumnHeaders);
	else
		server->columnAttributes = DIR_GetStringPref(prefstring, "columns", tempstring, kDefaultLDAPColumnHeaders);

	server->fileName = DIR_GetStringPref (prefstring, "filename", tempstring, "");
	if ( (!server->fileName || !*(server->fileName)) && !oldstyle) /* if we don't have a file name and this is the new branch get a file name */
			DIR_SetServerFileName (server, server->serverName);
	if (server->fileName && *server->fileName)
		DIR_ConvertServerFileName(server);

    // the string "s" is the default uri ( <scheme> + "://" + <filename> )
    nsCString s(kMDBDirectoryRoot);
    s.Append (server->fileName);
    server->uri = DIR_GetStringPref (prefstring, "uri", tempstring, s.get ());
    
	server->lastSearchString = DIR_GetStringPref (prefstring, "searchString", tempstring, "");

	/* This is where site-configurable attributes and filters are read from JavaScript */
	DIR_GetCustomAttributePrefs (prefstring, server, tempstring);
	DIR_GetCustomFilterPrefs (prefstring, server, tempstring);

	/* The replicated attributes and basic search attributes can only be
	 * attributes which are in our predefined set (DIR_AttributeId) so
	 * store those in an array of IDs for more convenient access
	 */
	dir_GetReplicationInfo (prefstring, server, tempstring);

	/* Get authentication prefs */
	server->enableAuth = DIR_GetBoolPref (prefstring, "auth.enabled", tempstring, kDefaultEnableAuth);
	server->authDn = DIR_GetStringPref (prefstring, "auth.dn", tempstring, nsnull);
	server->savePassword = DIR_GetBoolPref (prefstring, "auth.savePassword", tempstring, kDefaultSavePassword);
	if (server->savePassword)
		server->password = DIR_GetStringPref (prefstring, "auth.password", tempstring, "");

	prefBool = DIR_GetBoolPref (prefstring, "autoComplete.enabled", tempstring, kDefaultAutoCompleteEnabled);
	DIR_ForceFlag (server, DIR_AUTO_COMPLETE_ENABLED, prefBool);
	prefBool = DIR_GetBoolPref (prefstring, "autoComplete.never", tempstring, kDefaultAutoCompleteNever);
	DIR_ForceFlag (server, DIR_AUTO_COMPLETE_NEVER, prefBool);
	server->autoCompleteFilter = DIR_GetStringPref (prefstring, "autoComplete.filter", tempstring, nsnull);

	/* read in the I18N preferences for the directory --> locale and csid */

	/* okay we used to write out the csid as a integer pref called "charset" then we switched to a string pref called "csid" 
	   for I18n folks. So we want to read in the old integer pref and if it is not kDefaultPABCSID (which is a bogus -1), 
	   then use it as the csid and when we save the server preferences later on we'll clear the old "charset" pref so we don't
	   have to do this again. Otherwise, we already have a string pref so use that one */

	csidString = DIR_GetStringPref (prefstring, "csid", tempstring, nsnull);
	if (csidString) /* do we have a csid string ? */
	{
		server->csid = CS_UTF8;
//		server->csid = INTL_CharSetNameToID (csidString);
		PR_Free(csidString);
	}
	else 
	{ 
		/* try to read it in from the old integer style char set preference */
		if (server->dirType == PABDirectory)
			server->csid = (PRInt16) DIR_GetIntPref (prefstring, "charset", tempstring, kDefaultPABCSID);
		else
			server->csid = (PRInt16) DIR_GetIntPref (prefstring, "charset", tempstring, kDefaultLDAPCSID);	

		forcePrefSave = PR_TRUE; /* since we read from the old pref we want to force the new pref to be written out */
	}

	if (server->csid == CS_DEFAULT || server->csid == CS_UNKNOWN)
		server->csid = CS_UTF8;
//		server->csid = INTL_GetCharSetID(INTL_DefaultTextWidgetCsidSel);

	/* now that the csid is taken care of, read in the locale preference */
	server->locale = DIR_GetStringPref (prefstring, "locale", tempstring, nsnull);
	
	prefBool = DIR_GetBoolPref (prefstring, "vlvDisabled", tempstring, kDefaultVLVDisabled);
	DIR_ForceFlag (server, DIR_LDAP_VLV_DISABLED | DIR_LDAP_ROOTDSE_PARSED, prefBool);

	server->customDisplayUrl = DIR_GetStringPref (prefstring, "customDisplayUrl", tempstring, "");

	if (!oldstyle /* we don't care about saving old directories */ && forcePrefSave && !dir_IsServerDeleted(server) )
		DIR_SavePrefsForOneServer(server); 
}

/* return total number of directories */
static PRInt32 dir_GetPrefsFrom40Branch(nsVoidArray **list)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
    if (NS_FAILED(rv) || !pPref) 
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
  nsresult result = NS_OK;
  nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &result)); 
  if (NS_FAILED(result) || !pPref) 
    return NS_ERROR_FAILURE;
  
  char *children;
  
  (*list) = new nsVoidArray();
  if (!(*list))
    return NS_ERROR_OUT_OF_MEMORY;
  
  if (obsoleteList)
  {
    (*obsoleteList) = new nsVoidArray();
    if (!(*obsoleteList))
      return NS_ERROR_OUT_OF_MEMORY;
  }
  
  if (pPref->CreateChildList(PREF_LDAP_SERVER_TREE_NAME, &children) == PREF_NOERROR)
  {	
  /* TBD: Temporary code to read broken "ldap" preferences tree.
  *      Remove line with if statement after M10.
    */
    if (dir_UserId == 0)
      pPref->GetIntPref(PREF_LDAP_GLOBAL_TREE_NAME".user_id", &dir_UserId);
    
    PRInt16 i = 0;
    char *child;
    
    while ((pPref->NextChild(children, &i, &child)) == NS_OK)
    {
      DIR_Server *server;
      
      server = (DIR_Server *)PR_Calloc(1, sizeof(DIR_Server));
      if (server)
      {
        DIR_InitServer(server);
        server->prefName = nsCRT::strdup(child);
        DIR_GetPrefsForOneServer(server, PR_FALSE, PR_FALSE);
        if (   server->description && server->description[0]
          && (   (server->dirType == PABDirectory || 
          server->dirType == MAPIDirectory ||
          server->dirType == FixedQueryLDAPDirectory)  
          
          || (server->serverName  && server->serverName[0])))
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
    
    PR_Free(children);
  }
  
  return result;
}


nsresult DIR_GetServerPreferences(nsVoidArray** list)
{
    nsresult err = NS_OK;
    nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &err)); 
    if (NS_FAILED(err) || !pPref) 
		return NS_ERROR_FAILURE;

	PRInt32 position = 1;
	PRInt32 version = -1;
	char *oldChildren = nsnull;
	PRBool savePrefs = PR_FALSE;
	PRBool migrating = PR_FALSE;
	nsVoidArray *oldList = nsnull;
	nsVoidArray *obsoleteList = nsnull;
	nsVoidArray *newList = nsnull;
	PRInt32 i, j, count;


	/* Update the ldap list version and see if there are old prefs to migrate. */
	if (pPref->GetIntPref(PREF_LDAP_VERSION_NAME, &version) == PREF_NOERROR)
	{
		if (version < kPreviousListVersion)
		{
			pPref->SetIntPref(PREF_LDAP_VERSION_NAME, kCurrentListVersion);

			/* Look to see if there's an old-style "ldap_1" tree in prefs */
			if (PREF_NOERROR == pPref->CreateChildList("ldap_1", &oldChildren))
			{
				if (PL_strlen(oldChildren))
				{
					migrating = PR_TRUE;
					position = dir_GetPrefsFrom40Branch(&oldList);
				}
				PR_Free(oldChildren);
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
						if (   dir_AreServersSame(newServer, oldServer, PR_FALSE)
							|| (   oldServer->dirType == PABDirectory
								&& newServer->dirType == PABDirectory
								&& oldServer->isOffline == PR_FALSE
								&& newServer->isOffline == PR_FALSE))
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
				newList->RemoveElementAt(i);
			}
			else
			{
				DIR_DecrementServerRefCount(newServer);
				newList->RemoveElementAt(i);
			}
		}
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

	/* Sort the list to make sure it is in order */
	DIR_SortServersByPosition(*list);

	if (version < kCurrentListVersion)
	{
		pPref->SetIntPref(PREF_LDAP_VERSION_NAME, kCurrentListVersion);
		// see if we have the ab upgrader.  if so, skip this, since we
		// will be migrating.
		nsresult rv;
		nsCOMPtr <nsIAbUpgrader> abUpgrader = do_GetService(NS_AB4xUPGRADER_CONTRACTID, &rv);
  		if (NS_FAILED(rv) || !abUpgrader) {
#ifdef DEBUG_sspitzer_
			printf("move the pab aside, since we don't have the ab upgrader\n");
#endif
			dir_ConvertToMabFileName();
		}
#ifdef DEBUG_sspitzer_
		else {
			printf("don't touch the 4.x pab.  we will migrate it\n");
		}
#endif
	}
	/* Write the merged list so we get it next time we ask */
	if (savePrefs)
		DIR_SaveServerPreferences(*list);

	return err;
}


void DIR_ClearPrefBranch(const char *branch)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
	if (NS_FAILED(rv) || !pPref) 
		return;

	pPref->DeleteBranch (branch);
}


static void DIR_ClearIntPref (const char *pref)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
    if (NS_FAILED(rv) || !pPref) 
		return;

	PRInt32 oldDefault;
	PRInt32 prefErr = pPref->GetDefaultIntPref (pref, &oldDefault);
	DIR_ClearPrefBranch (pref);
	if (prefErr >= 0)
		pPref->SetDefaultIntPref (pref, oldDefault);
}


static void DIR_ClearStringPref (const char *pref)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
    if (NS_FAILED(rv) || !pPref) 
		return;

	char *oldDefault = nsnull;
	PRInt32 prefErr = pPref->CopyDefaultCharPref (pref, &oldDefault);
	DIR_ClearPrefBranch (pref);
	if (prefErr >= 0)
		pPref->SetDefaultCharPref (pref, oldDefault);
	PR_FREEIF(oldDefault);
}


static void DIR_ClearBoolPref (const char *pref)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
    if (NS_FAILED(rv) || !pPref) 
		return;

	PRBool oldDefault;
	PRInt32 prefErr = pPref->GetDefaultBoolPref (pref, &oldDefault);
	DIR_ClearPrefBranch (pref);
	if (prefErr >= 0)
		pPref->SetDefaultBoolPref (pref, oldDefault);
}


static void DIR_SetStringPref (const char *prefRoot, const char *prefLeaf, char *scratch, const char *value, const char *defaultValue)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
    if (NS_FAILED(rv) || !pPref) 
		return;

	char *defaultPref = nsnull;
	PRInt32 prefErr = PREF_NOERROR;

	PL_strcpy(scratch, prefRoot);
	PL_strcat(scratch, ".");
	PL_strcat(scratch, prefLeaf);

	if (PREF_NOERROR == pPref->CopyDefaultCharPref (scratch, &defaultPref))
	{
		/* If there's a default pref, just set ours in and let libpref worry 
		 * about potential defaults in all.js
		 */
		 if (value) /* added this check to make sure we have a value before we try to set it..*/
		 	prefErr = pPref->SetCharPref (scratch, value);
		 else
			 DIR_ClearStringPref(scratch);

		PR_Free(defaultPref);
	}
	else
	{
		/* If there's no default pref, look for a user pref, and only set our value in
		 * if the user pref is different than one of them.
		 */
		char *userPref = nsnull;
		if (PREF_NOERROR == pPref->CopyCharPref (scratch, &userPref))
		{
            if (value && (defaultValue ? nsCRT::strcasecmp(value, defaultValue) : value != defaultValue))
				prefErr = pPref->SetCharPref (scratch, value);
			else
				DIR_ClearStringPref (scratch); 
		}
		else
		{
            if (value && (defaultValue ? nsCRT::strcasecmp(value, defaultValue) : value != defaultValue))
				prefErr = pPref->SetCharPref (scratch, value); 
		}

		PR_FREEIF(userPref);
	}

	PR_ASSERT(prefErr >= 0);
}


static void DIR_SetIntPref (const char *prefRoot, const char *prefLeaf, char *scratch, PRInt32 value, PRInt32 defaultValue)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
    if (NS_FAILED(rv) || !pPref) 
		return;

	PRInt32 defaultPref;
	PRInt32 prefErr = PREF_NOERROR;

	PL_strcpy(scratch, prefRoot);
	PL_strcat(scratch, ".");
	PL_strcat(scratch, prefLeaf);

	if (PREF_NOERROR == pPref->GetDefaultIntPref (scratch, &defaultPref))
	{
		/* solve the problem where reordering user prefs must override default prefs */
		pPref->SetIntPref (scratch, value);
	}
	else
	{
		PRInt32 userPref;
		if (PREF_NOERROR == pPref->GetIntPref (scratch, &userPref))
		{
			if (value != defaultValue)
				prefErr = pPref->SetIntPref(scratch, value);
			else
				DIR_ClearIntPref (scratch);
		}
		else
		{
			if (value != defaultValue)
				prefErr = pPref->SetIntPref (scratch, value); 
		}
	}

	PR_ASSERT(prefErr >= 0);
}


static void DIR_SetBoolPref (const char *prefRoot, const char *prefLeaf, char *scratch, PRBool value, PRBool defaultValue)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
    if (NS_FAILED(rv) || !pPref) 
		return;

	PRBool defaultPref;
	PRInt32 prefErr = PREF_NOERROR;

	PL_strcpy(scratch, prefRoot);
	PL_strcat(scratch, ".");
	PL_strcat(scratch, prefLeaf);

	if (PREF_NOERROR == pPref->GetDefaultBoolPref (scratch, &defaultPref))
	{
		/* solve the problem where reordering user prefs must override default prefs */
		prefErr = pPref->SetBoolPref (scratch, value);
	}
	else
	{
		PRBool userPref;
		if (PREF_NOERROR == pPref->GetBoolPref (scratch, &userPref))
		{
			if (value != defaultValue)
				prefErr = pPref->SetBoolPref(scratch, value);
			else
				DIR_ClearBoolPref (scratch);
		}
		else
		{
			if (value != defaultValue)
				prefErr = pPref->SetBoolPref (scratch, value);
		}

	}

	PR_ASSERT(prefErr >= 0);
}


static nsresult DIR_ConvertAttributeToPrefsString (DIR_Attribute *attrib, char **ppPrefsString)
{
	nsresult err = NS_OK;

	/* Compute size in bytes req'd for prefs string */
	PRUint32 length = PL_strlen(attrib->prettyName);
	PRInt32 i = 0;
	while (attrib->attrNames[i])
	{
		length += PL_strlen(attrib->attrNames[i]) + 1; /* +1 for comma separator */
		i++;
	}
	length += 1; /* +1 for colon */

	/* Allocate prefs string */
	*ppPrefsString = (char*) PR_Malloc(length + 1); /* +1 for null term */

	/* Unravel attrib struct back out into prefs */
	if (*ppPrefsString)
	{
		PRInt32 j = 0;
		PL_strcpy (*ppPrefsString, attrib->prettyName);
		PL_strcat (*ppPrefsString, ":");
		while (attrib->attrNames[j])
		{
			PL_strcat (*ppPrefsString, attrib->attrNames[j]);
			if (j + 1 < i)
				PL_strcat (*ppPrefsString, ",");
			j++;
		}
	}
	else
		err = NS_ERROR_OUT_OF_MEMORY;

	return err;
}


static nsresult DIR_SaveOneCustomAttribute (const char *prefRoot, char *scratch, DIR_Server *server, DIR_AttributeId id)
{
	const char *name = DIR_GetDefaultAttribute (id)->name;
	nsresult err = NS_OK;

	if (server->customAttributes)
	{
		DIR_Attribute *attrib = nsnull;
		nsVoidArray *walkList = server->customAttributes;
		PRInt32  count = walkList->Count();
		PRInt32  i;
		for (i = 0; i < count; i++)
		{
			if ((attrib = (DIR_Attribute *)walkList->ElementAt(i)) != nsnull)
			{
				if (attrib->id == id)
				{
					char *jsString = nsnull;
					nsresult res = DIR_ConvertAttributeToPrefsString(attrib, &jsString);
					if (NS_SUCCEEDED(res))
					{
						DIR_SetStringPref (prefRoot, name, scratch, jsString, "");
						PR_Free(jsString);
						return err;
					}
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


static nsresult DIR_SaveCustomAttributes (const char *prefRoot, char *scratch, DIR_Server *server)
{
	nsresult err = NS_OK;
	char *localScratch = (char*) PR_Malloc(256);

	if (localScratch)
	{
		PL_strcpy(scratch, prefRoot);
		PL_strcat(scratch, ".attributes");

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
		PR_Free(localScratch);
	}
	else
		err = NS_ERROR_OUT_OF_MEMORY;

	return err;
}


static nsresult DIR_SaveCustomFilters (const char *prefRoot, char *scratch, DIR_Server *server)
{
	char *localScratch = (char*) PR_Malloc(256);
	nsresult err = NS_OK;

	if (!localScratch)
		return NS_ERROR_OUT_OF_MEMORY;

	PL_strcpy (scratch, prefRoot);
	PL_strcat (scratch, ".filter1");
	if (server->customFilters)
	{
		/* Save the custom filters into the JS prefs */
		DIR_Filter *filter = nsnull;
		nsVoidArray *walkList = server->customFilters;
		PRInt32 count = walkList->Count();
		PRInt32 i;
		for (i = 0; i < count; i++)
		{
			if ((filter = (DIR_Filter *)walkList->ElementAt(i)) != nsnull)
			{
				DIR_SetBoolPref (scratch, "repeatFilterForWords", localScratch, 
					(filter->flags & DIR_F_REPEAT_FILTER_FOR_TOKENS) != 0, kDefaultRepeatFilterForTokens);
				DIR_SetStringPref (scratch, "string", localScratch, filter->string, kDefaultFilter);
			}
		}
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

	PR_Free(localScratch);
	return err;
}


static nsresult dir_SaveReplicationInfo (const char *prefRoot, char *scratch, DIR_Server *server)
{
	nsresult err = NS_OK;
	char *localScratch = (char*) PR_Malloc(256);
	if (!localScratch)
		return NS_ERROR_OUT_OF_MEMORY;

	PL_strcpy (scratch, prefRoot);
	PL_strcat (scratch, ".replication");

	DIR_SetBoolPref (scratch, "never", localScratch, DIR_TestFlag (server, DIR_REPLICATE_NEVER), kDefaultReplicateNever);
	DIR_SetBoolPref (scratch, "enabled", localScratch, DIR_TestFlag (server, DIR_REPLICATION_ENABLED), kDefaultReplicaEnabled);

	if (server->replInfo)
	{
		char *excludedList = nsnull;
		PRInt32 i;
		PRInt32 excludedLength = 0;
		for (i = 0; i < server->replInfo->excludedAttributesCount; i++)
			excludedLength += PL_strlen (server->replInfo->excludedAttributes[i]) + 2; /* +2 for ", " */
		if (excludedLength)
		{
			excludedList = (char*) PR_Malloc (excludedLength + 1);
			if (excludedList)
			{
				excludedList[0] = '\0';
				for (i = 0; i < server->replInfo->excludedAttributesCount; i++)
				{
					PL_strcat (excludedList, server->replInfo->excludedAttributes[i]);
					PL_strcat (excludedList, ", ");
				}
			}
			else
				err = NS_ERROR_OUT_OF_MEMORY;
		}

		DIR_SetStringPref (scratch, "excludedAttributes", localScratch, excludedList, kDefaultReplicaExcludedAttributes);

		DIR_SetStringPref (scratch, "description", localScratch, server->replInfo->description, kDefaultReplicaDescription);
		DIR_SetStringPref (scratch, "fileName", localScratch, server->replInfo->fileName, kDefaultReplicaFileName);
		DIR_SetStringPref (scratch, "filter", localScratch, server->replInfo->filter, kDefaultReplicaFilter);
		DIR_SetIntPref (scratch, "lastChangeNumber", localScratch, server->replInfo->lastChangeNumber, kDefaultReplicaChangeNumber);
		DIR_SetStringPref (scratch, "syncURL", localScratch, server->replInfo->syncURL, nsnull);
		DIR_SetStringPref (scratch, "dataVersion", localScratch, server->replInfo->dataVersion, kDefaultReplicaDataVersion);
	}
	else if (DIR_TestFlag (server, DIR_REPLICATION_ENABLED))
		server->replInfo = (DIR_ReplicationInfo *) PR_Calloc (1, sizeof(DIR_ReplicationInfo));

	PR_Free(localScratch);
	return err;
}


void DIR_SavePrefsForOneServer(DIR_Server *server)
{
	char *prefstring;
	char tempstring[256];
	char * csidAsString = nsnull;

	if (server->prefName == nsnull)
		server->prefName = DIR_CreateServerPrefName (server, nsnull);
	prefstring = server->prefName;

	DIR_SetFlag(server, DIR_SAVING_SERVER);

	DIR_SetIntPref (prefstring, "position", tempstring, server->position, kDefaultPosition);

	// Only save the non-default address book name
	if (0 != PL_strcmp(prefstring, "ldap_2.servers.pab") &&
		0 != PL_strcmp(prefstring, "ldap_2.servers.history")) 
		DIR_SetStringPref (prefstring, "description", tempstring, server->description, "");

	DIR_SetStringPref (prefstring, "serverName", tempstring, server->serverName, "");
	DIR_SetStringPref (prefstring, "searchBase", tempstring, server->searchBase, "");
	DIR_SetStringPref (prefstring, "filename", tempstring, server->fileName, "");
	if (server->port == 0)
		server->port = server->isSecure ? LDAPS_PORT : LDAP_PORT;
	DIR_SetIntPref (prefstring, "port", tempstring, server->port, server->isSecure ? LDAPS_PORT : LDAP_PORT);
	DIR_SetIntPref (prefstring, "maxHits", tempstring, server->maxHits, kDefaultMaxHits);
	DIR_SetBoolPref (prefstring, "isSecure", tempstring, server->isSecure, PR_FALSE);
	DIR_SetBoolPref (prefstring, "saveResults", tempstring, server->saveResults, PR_TRUE);
	DIR_SetBoolPref (prefstring, "efficientWildcards", tempstring, server->efficientWildcards, PR_TRUE);
	DIR_SetStringPref (prefstring, "searchString", tempstring, server->lastSearchString, "");
	DIR_SetIntPref (prefstring, "dirType", tempstring, server->dirType, LDAPDirectory);
	DIR_SetBoolPref (prefstring, "isOffline", tempstring, server->isOffline, kDefaultIsOffline);

	/* save the column attributes */
	if (server->dirType == PABDirectory)
		DIR_SetStringPref(prefstring, "columns", tempstring, server->columnAttributes, kDefaultPABColumnHeaders);
	else
		DIR_SetStringPref(prefstring, "columns", tempstring, server->columnAttributes, kDefaultLDAPColumnHeaders);

	DIR_SetBoolPref (prefstring, "autoComplete.enabled", tempstring, DIR_TestFlag(server, DIR_AUTO_COMPLETE_ENABLED), kDefaultAutoCompleteEnabled);
	DIR_SetStringPref (prefstring, "autoComplete.filter", tempstring, server->autoCompleteFilter, nsnull);
	DIR_SetBoolPref (prefstring, "autoComplete.never", tempstring, DIR_TestFlag(server, DIR_AUTO_COMPLETE_NEVER), kDefaultAutoCompleteNever);

	/* save the I18N information for the directory */
	
	/* I18N folks want us to save out the csid as a string.....*/
/*	csidAsString = (char *) INTL_CsidToCharsetNamePt(server->csid);*/ /* this string is actually static we should not free it!!! */
	csidAsString = NULL; /* this string is actually static we should not free it!!! */
	if (csidAsString)
		DIR_SetStringPref(prefstring, "csid", tempstring, csidAsString, nsnull);
	
	/* since we are no longer writing out the csid as an integer, make sure that preference is removed.
	   kDefaultPABCSID is a bogus csid value that when we read back in we can recognize as an outdated pref */

	/* this is dirty but it works...this is how we assemble the pref name in all of the DIR_SetString/bool/intPref functions */
	PL_strcpy(tempstring, prefstring);
	PL_strcat(tempstring, ".");
	PL_strcat(tempstring, "charset");
	DIR_ClearIntPref(tempstring);  /* now clear the pref */

	/* now save the locale string */
	DIR_SetStringPref(prefstring, "locale", tempstring, server->locale, nsnull);

	/* Save authentication prefs */
	DIR_SetBoolPref (prefstring, "auth.enabled", tempstring, server->enableAuth, kDefaultEnableAuth);
	DIR_SetBoolPref (prefstring, "auth.savePassword", tempstring, server->savePassword, kDefaultSavePassword);
	if (server->savePassword && server->authDn && server->password)
	{
		DIR_SetStringPref (prefstring, "auth.dn", tempstring, server->authDn, "");
		DIR_SetStringPref (prefstring, "auth.password", tempstring, server->password, "");
	}
	else
	{
		DIR_SetStringPref (prefstring, "auth.dn", tempstring, "", "");
		DIR_SetStringPref (prefstring, "auth.password", tempstring, "", "");
		PR_FREEIF (server->authDn);
		PR_FREEIF (server->password);
	}

	DIR_SetBoolPref (prefstring, "vlvDisabled", tempstring, DIR_TestFlag(server, DIR_LDAP_VLV_DISABLED), kDefaultVLVDisabled);

	DIR_SaveCustomAttributes (prefstring, tempstring, server);
	DIR_SaveCustomFilters (prefstring, tempstring, server);

	dir_SaveReplicationInfo (prefstring, tempstring, server);

	DIR_SetStringPref (prefstring, "customDisplayUrl", tempstring, server->customDisplayUrl, "");

	DIR_ClearFlag(server, DIR_SAVING_SERVER);
}

nsresult DIR_SaveServerPreferences (nsVoidArray *wholeList)
{
	if (wholeList)
	{
		nsresult rv = NS_OK;
		nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
		if (NS_FAILED(rv) || !pPref) 
			return NS_ERROR_FAILURE;

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

/*****************************************************************************
 * Functions for getting site-configurable preferences, from JavaScript if
 * the site admin has provided them, else out of thin air.
 */

static DIR_DefaultAttribute *DIR_GetDefaultAttribute (DIR_AttributeId id)
{
	PRInt32 i = 0;

	static DIR_DefaultAttribute defaults[32];

	if (defaults[0].name == nsnull)
	{
		defaults[0].id = cn;
		defaults[0].resourceId = MK_LDAP_COMMON_NAME;
		defaults[0].name = "cn";
	
		defaults[1].id = givenname;
		defaults[1].resourceId = MK_LDAP_GIVEN_NAME;
		defaults[1].name = "givenname";
	
		defaults[2].id = sn;
		defaults[2].resourceId = MK_LDAP_SURNAME;
		defaults[2].name = "sn";
	
		defaults[3].id = mail;
		defaults[3].resourceId = MK_LDAP_EMAIL_ADDRESS;
		defaults[3].name = "mail";
	
		defaults[4].id = telephonenumber;
		defaults[4].resourceId = MK_LDAP_PHONE_NUMBER;
		defaults[4].name = "telephonenumber";
	
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

		defaults[15].id = carlicense;
		defaults[15].resourceId = MK_LDAP_CAR_LICENSE;
		defaults[15].name = "carlicense";

		defaults[16].id = businesscategory;
		defaults[16].resourceId = MK_LDAP_BUSINESS_CAT;
		defaults[16].name = "businesscategory";

		defaults[17].id = departmentnumber;
		defaults[17].resourceId = MK_LDAP_DEPT_NUMBER;
		defaults[17].name = "businesscategory";

		defaults[18].id = description;
		defaults[18].resourceId = MK_LDAP_DESCRIPTION;
		defaults[18].name = "description";

		defaults[19].id = employeetype;
		defaults[19].resourceId = MK_LDAP_EMPLOYEE_TYPE;
		defaults[19].name = "employeetype";

		defaults[20].id = facsimiletelephonenumber;
		defaults[20].resourceId = MK_LDAP_FAX_NUMBER;
		defaults[20].name = "facsimiletelephonenumber";

		defaults[21].id = manager;
		defaults[21].resourceId = MK_LDAP_MANAGER;
		defaults[21].name = "manager";

		defaults[22].id = objectclass;
		defaults[22].resourceId = MK_LDAP_OBJECT_CLASS;
		defaults[22].name = "objectclass";

		defaults[23].id = postaladdress;
		defaults[23].resourceId = MK_LDAP_POSTAL_ADDRESS;
		defaults[23].name = "postaladdress";

		defaults[24].id = postalcode;
		defaults[24].resourceId = MK_LDAP_POSTAL_CODE;
		defaults[24].name = "postalcode";

		defaults[25].id = secretary;
		defaults[25].resourceId = MK_LDAP_SECRETARY;
		defaults[25].name = "secretary";

		defaults[26].id = title;
		defaults[26].resourceId = MK_LDAP_TITLE;
		defaults[26].name = "title";

		defaults[27].id = nickname;
		defaults[27].resourceId = MK_LDAP_NICK_NAME;
		defaults[27].name = "nickname";

		defaults[28].id = homephone;
		defaults[28].resourceId = MK_LDAP_HOMEPHONE;
		defaults[28].name = "homephone";

		defaults[29].id = pager;
		defaults[29].resourceId = MK_LDAP_PAGER;
		defaults[29].name = "pager";

		defaults[30].id = mobiletelephonenumber;
		defaults[30].resourceId = MK_LDAP_MOBILEPHONE;
		defaults[30].name = "mobiletelephonenumber";

		defaults[31].id = cn;
		defaults[31].resourceId = 0;
		defaults[31].name = nsnull;
	}

	while (defaults[i].name)
	{
		if (defaults[i].id == id)
			return &defaults[i];
		i++;
	}

	return nsnull;
}

const char *DIR_GetAttributeName (DIR_Server *server, DIR_AttributeId id)
{
	char *result = nsnull;

	/* First look in the custom attributes in case the attribute is overridden */
	nsVoidArray *list = server->customAttributes;
	DIR_Attribute *walkList = nsnull;

	PRInt32  count = list->Count();
	PRInt32  i;
	for (i = 0; i < count; i++)
	{
		if ((walkList = (DIR_Attribute *)list->ElementAt(i)) != nsnull)
		{
			if (walkList->id == id)
				result = walkList->prettyName;
		}
	}

	/* If we didn't find it, look in our own static list of attributes */
//	if (!result)
//	{
//		DIR_DefaultAttribute *def;
//		if ((def = DIR_GetDefaultAttribute(id)) != nsnull)
//			result = (char*)XP_GetString(def->resourceId);
//	}

	return result;
}


const char **DIR_GetAttributeStrings (DIR_Server *server, DIR_AttributeId id)
{
	const char **result = nsnull;

	if (server && server->customAttributes)
	{
		/* First look in the custom attributes in case the attribute is overridden */
		nsVoidArray *list = server->customAttributes;
		DIR_Attribute *walkList = nsnull;
		PRInt32  count = list->Count();
		PRInt32  i;
		for (i = 0; i < count; i++)
		{
			while ((walkList = (DIR_Attribute *)list->ElementAt(i)) != nsnull)
			{
				if (walkList->id == id)
					result = (const char**)walkList->attrNames;
			}
		}
	}

	/* If we didn't find it, look in our own static list of attributes */
	if (!result)
	{
		static const char *array[2];
		array[0] = DIR_GetDefaultAttribute(id)->name;
		array[1] = nsnull;
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
		return nsnull;
}

const char *DIR_GetFilterString (DIR_Server *server)
{
	if (!server)
		return nsnull;

	DIR_Filter *filter = (DIR_Filter *)server->customFilters->ElementAt(0);
	if (filter)
		return filter->string;
	return nsnull;
}


static DIR_Filter *DIR_LookupFilter (DIR_Server *server, const char *filter)
{
	if (!server)
		return nsnull;

	nsVoidArray *list = server->customFilters;
	DIR_Filter *walkFilter = nsnull;

	PRInt32  count = list->Count();
	PRInt32  i;
	for (i = 0; i < count; i++)
	{
		if ((walkFilter = (DIR_Filter *)list->ElementAt(i)) != nsnull)
            if (!nsCRT::strcasecmp(filter, walkFilter->string))
				return walkFilter;
	}
	return nsnull;
}


PRBool DIR_RepeatFilterForTokens (DIR_Server *server, const char *filter)
{
	if (!server)
		return nsnull;

	DIR_Filter *f;
	
	if (!filter)
		f = (DIR_Filter *)server->customFilters->ElementAt(0);
	else
		f = DIR_LookupFilter (server, filter);

	return f ? (f->flags & DIR_F_REPEAT_FILTER_FOR_TOKENS) != 0 : kDefaultRepeatFilterForTokens;
}


PRBool DIR_SubstStarsForSpaces (DIR_Server *server, const char *filter)
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

PRBool DIR_UseCustomAttribute (DIR_Server *server, DIR_AttributeId id)
{
	nsVoidArray *list = server->customAttributes;
	DIR_Attribute *walkList = nsnull;

	PRInt32  count = list->Count();
	PRInt32  i;
	for (i = 0; i < count; i++)
	{
		if ((walkList = (DIR_Attribute *)list->ElementAt(i)) != nsnull)
			if (walkList->id == id)
				return PR_TRUE;
	}
	return PR_FALSE;
}


PRBool DIR_IsDnAttribute (DIR_Server *s, const char *attrib)
{
	if (s && s->dnAttributes)
	{
		/* Look in the server object to see if there are prefs to tell
		 * us which attributes contain DNs
		 */
		PRInt32 i;
		for (i = 0; i < s->dnAttributesCount; i++)
		{
            if (!nsCRT::strcasecmp(attrib, s->dnAttributes[i]))
				return PR_TRUE;
		}
	}
	else
	{
		/* We have some default guesses about what attributes 
		 * are likely to contain DNs 
		 */
		switch (tolower(attrib[0]))
		{
		case 'm':
            if (!nsCRT::strcasecmp(attrib, "manager") || 
                !nsCRT::strcasecmp(attrib, "member"))
				return PR_TRUE;
			break;
		case 'o':
            if (!nsCRT::strcasecmp(attrib, "owner"))
				return PR_TRUE;
			break;
		case 'u':
            if (!nsCRT::strcasecmp(attrib, "uniquemember"))
				return PR_TRUE;
			break;
		}
	}
	return PR_FALSE;
}


PRBool DIR_IsAttributeExcludedFromHtml (DIR_Server *s, const char *attrib)
{
	if (s && s->suppressedAttributes)
	{
		/* Look in the server object to see if there are prefs to tell
		 * us which attributes shouldn't be shown in HTML
		 */
		PRInt32 i;
		for (i = 0; i < s->suppressedAttributesCount; i++)
		{
            if (!nsCRT::strcasecmp(attrib, s->suppressedAttributes[i]))
				return PR_TRUE;
		}
	}
	/* else don't exclude it. By default we show everything */

	return PR_FALSE;
}

PRBool DIR_IsUriAttribute (DIR_Server *s, const char *attrib)
{
	if (s && s->uriAttributes)
	{
		/* Look in the server object to see if there are prefs to tell
		 * us which attributes are URLs
		 */
		PRInt32 i;
		for (i = 0; i < s->uriAttributesCount; i++)
		{
            if (!nsCRT::strcasecmp(attrib, s->uriAttributes[i]))
				return PR_TRUE;
		}
	}
	else
	{
		/* We have some default guesses about what attributes 
		 * are likely to contain URLs
		 */
		switch (tolower(attrib[0]))
		{
		case 'l':
            if (   !nsCRT::strcasecmp(attrib, "labeleduri")
                || !nsCRT::strcasecmp(attrib, "labeledurl"))
				return PR_TRUE;
			break;
		case 'u':
            if (!nsCRT::strcasecmp(attrib, "url"))
				return PR_TRUE;
			break;
		}
	}
	return PR_FALSE;
}

void DIR_SetAuthDN (DIR_Server *s, const char *dn)
{
	char *tmp = nsnull;

	PR_ASSERT(dn && s);
	if (!dn || !s)
		return;
	if (s->authDn && !PL_strcmp(dn, s->authDn))
		return; /* no change - no need to broadcast */

    tmp = nsCRT::strdup (dn);
	if (tmp)
	{
		/* Always remember the authDn in the DIR_Server, so that users only
		 * have to authenticate to the server once during a session. Whether
		 * or not we push the authDN into the prefs is a separate issue, and 
		 * is covered by the prefs read/write code
		 */
		PR_FREEIF(s->authDn);
		s->authDn = tmp;
	}
	if (s->savePassword)
		DIR_SavePrefsForOneServer (s);
}


void DIR_SetPassword (DIR_Server *s, const char *password)
{
	char *tmp = nsnull;

	PR_ASSERT(password && s);
	if (!password || !s)
		return; 
	if (s->password && !PL_strcmp(password, s->password))
		return; /* no change - no need to broadcast */

    tmp = nsCRT::strdup (password);
	if (tmp)
	{
		/* Always remember the password in the DIR_Server, so that users only
		 * have to authenticate to the server once during a session. Whether
		 * or not we push the password into the prefs is a separate issue, and 
		 * is covered by the prefs read/write code
		 */
		PR_FREEIF(s->password);
		s->password = tmp;
	}
	if (s->savePassword)
		DIR_SavePrefsForOneServer (s);
}


PRBool DIR_IsEscapedAttribute (DIR_Server *s, const char *attrib)
{
	/* We're not exposing this setting in JS prefs right now, but in case we
	 * might want to in the future, leave the DIR_Server* in the prototype.
	 */

	switch (tolower(attrib[0]))
	{
	case 'p':
        if (!nsCRT::strcasecmp(attrib, "postaladdress"))
			return PR_TRUE;
		break;
	case 'f': 
        if (!nsCRT::strcasecmp(attrib, "facsimiletelephonenumber"))
			return PR_TRUE;
		break;
	case 'o':
        if (!nsCRT::strcasecmp(attrib, "othermail"))
			return PR_TRUE;
		break;
	}
	return PR_FALSE;
}


char *DIR_Unescape (const char *src, PRBool makeHtml)
{
/* Borrowed from libnet\mkparse.c */
#define UNHEX(C) \
	((C >= '0' && C <= '9') ? C - '0' : \
	((C >= 'A' && C <= 'F') ? C - 'A' + 10 : \
	((C >= 'a' && C <= 'f') ? C - 'a' + 10 : 0)))

	char *dest = nsnull;
	PRUint32 destLength = 0;

	PRUint32 dollarCount = 0;
	PRUint32 convertedLengthOfDollar = makeHtml ? 4 : 1;

	const char *tmpSrc = src;

	while (*tmpSrc)
		if (*tmpSrc++ == '$')
			dollarCount++;

	destLength = PL_strlen(src) + (dollarCount * convertedLengthOfDollar);
	dest = (char*) PR_Malloc (destLength + 1);
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
				PRBool didEscape = PR_FALSE;
				char c1 = *(tmpSrc + 1);
				if (c1 && (nsCRT::IsAsciiDigit(c1) || nsCRT::IsAsciiAlpha(c1)))
				{
					char c2 = *(tmpSrc + 2);
					if (c2 && (nsCRT::IsAsciiDigit(c2) || nsCRT::IsAsciiAlpha(c2)))
					{
						*tmpDst++ = (UNHEX(c1) << 4) | UNHEX(c2);
						tmpSrc +=2;
						didEscape = PR_TRUE;
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


/* Centralize this charset conversion so everyone can do the UTF8 conversion
 * in the same way. Also, if someone ever makes us do T.61 or some other silly
 * thing, we can use these bottlenecks.
 */

char *DIR_ConvertToServerCharSet(DIR_Server *server, char *src, PRInt16 srcCSID)
{
	return DIR_ConvertString(srcCSID, (PRInt16)(server ? server->csid : CS_DEFAULT), src);
}

char *DIR_ConvertFromServerCharSet(DIR_Server *server, char *src, PRInt16 dstCSID)
{
	return DIR_ConvertString((PRInt16)(server ? server->csid : CS_DEFAULT), dstCSID, src);
}

char *DIR_ConvertString(PRInt16 srcCSID, PRInt16 dstCSID, const char *string)
{
    return nsCRT::strdup(string);
}
