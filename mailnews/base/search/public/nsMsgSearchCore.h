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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _nsMsgSearchCore_H_
#define _nsMsgSearchCore_H_

#include "MailNewsTypes.h"
#include "nsString2.h"


typedef enum
{
    nsMsgSearchScopeMailFolder,
    nsMsgSearchScopeNewsgroup,
    nsMsgSearchScopeLdapDirectory,
    nsMsgSearchScopeOfflineNewsgroup,
	nsMsgSearchScopeAllSearchableGroups
} nsMsgSearchScopeAttribute;

typedef enum
{
    nsMsgSearchAttribSubject = 0,	/* mail and news */
    nsMsgSearchAttribSender,   
    nsMsgSearchAttribBody,	
    nsMsgSearchAttribDate,	

    nsMsgSearchAttribPriority,	    /* mail only */
    nsMsgSearchAttribMsgStatus,	
    nsMsgSearchAttribTo,
    nsMsgSearchAttribCC,
    nsMsgSearchAttribToOrCC,

    nsMsgSearchAttribCommonName,   /* LDAP only */
    nsMsgSearchAttrib822Address,	
    nsMsgSearchAttribPhoneNumber,
    nsMsgSearchAttribOrganization,
    nsMsgSearchAttribOrgUnit,
    nsMsgSearchAttribLocality,
    nsMsgSearchAttribStreetAddress,
    nsMsgSearchAttribSize,
    nsMsgSearchAttribAnyText,      /* any header or body */
	nsMsgSearchAttribKeywords,

    nsMsgSearchAttribDistinguishedName, /* LDAP result elem only */
    nsMsgSearchAttribObjectClass,       
    nsMsgSearchAttribJpegFile,

    nsMsgSearchAttribLocation,          /* result list only */
    nsMsgSearchAttribMessageKey,        /* message result elems */

	nsMsgSearchAttribAgeInDays,    /* for purging old news articles */

	nsMsgSearchAttribGivenName,    /* for sorting LDAP results */
	nsMsgSearchAttribSurname, 

	nsMsgSearchAttribFolderInfo,	/* for "view thread context" from result */

	nsMsgSearchAttribCustom1,		/* custom LDAP attributes */
	nsMsgSearchAttribCustom2,
	nsMsgSearchAttribCustom3,
	nsMsgSearchAttribCustom4,
	nsMsgSearchAttribCustom5,

	nsMsgSearchAttribMessageId, 
	/* the following are LDAP specific attributes */
	nsMsgSearchAttribCarlicense,
	nsMsgSearchAttribBusinessCategory,
	nsMsgSearchAttribDepartmentNumber,
	nsMsgSearchAttribDescription,
	nsMsgSearchAttribEmployeeType,
	nsMsgSearchAttribFaxNumber,
	nsMsgSearchAttribManager,
	nsMsgSearchAttribPostalAddress,
	nsMsgSearchAttribPostalCode,
	nsMsgSearchAttribSecretary,
	nsMsgSearchAttribTitle,
	nsMsgSearchAttribNickname,
	nsMsgSearchAttribHomePhone,
	nsMsgSearchAttribPager,
	nsMsgSearchAttribCellular,

	nsMsgSearchAttribOtherHeader,  /* for mail and news. MUST ALWAYS BE LAST attribute since we can have an arbitrary # of these...*/
    kNumMsgSearchNumAttributes      /* must be last attribute */
} nsMsgSearchAttribute;

/* NB: If you add elements to this enum, add only to the end, since 
 *     RULES.DAT stores enum values persistently
 */
typedef enum
{
    nsMsgSearchOpContains = 0,     /* for text attributes              */
    nsMsgSearchOpDoesntContain,
    nsMsgSearchOpIs,               /* is and isn't also apply to some non-text attrs */
    nsMsgSearchOpIsnt, 
    nsMsgSearchOpIsEmpty,

    nsMsgSearchOpIsBefore,         /* for date attributes              */
    nsMsgSearchOpIsAfter,
    
    nsMsgSearchOpIsHigherThan,     /* for priority. nsMsgSearchOpIs also applies  */
    nsMsgSearchOpIsLowerThan,

    nsMsgSearchOpBeginsWith,              
	nsMsgSearchOpEndsWith,

    nsMsgSearchOpSoundsLike,       /* for LDAP phoenetic matching      */
	nsMsgSearchOpLdapDwim,         /* Do What I Mean for simple search */

	nsMsgSearchOpIsGreaterThan,	
	nsMsgSearchOpIsLessThan,

	nsMsgSearchOpNameCompletion,   /* Name Completion operator...as the name implies =) */

    kNumMsgSearchOperators       /* must be last operator            */
} MSG_SearchOperator;

/* FEs use this to help build the search dialog box */
typedef enum
{
    nsMsgSearchWidgetText,
    nsMsgSearchWidgetDate,
    nsMsgSearchWidgetMenu,
	nsMsgSearchWidgetInt,          /* added to account for age in days which requires an integer field */
    nsMsgSearchWidgetNone
} nsMsgSearchValueWidget;

/* Used to specify type of search to be performed */
typedef enum
{
	nsMsgSearchNone,
	nsMsgSearchRootDSE,
	nsMsgSearchNormal,
	nsMsgSearchLdapVLV,
	nsMsgSearchNameCompletion
} nsMsgSearchType;

/* Use this to specify the value of a search term */
typedef struct nsMsgSearchValue
{
    nsMsgSearchAttribute attribute;
    union
    {
        char *string;
        MSG_PRIORITY priority;
        time_t date;
        PRUint32 msgStatus; /* see MSG_FLAG in msgcom.h */
        PRUint32 size;
        nsMsgKey key;
		PRUint32 age; /* in days */
		MSG_FolderInfo *folder;
    } u;
} nsMsgSearchValue;

/* Use this to help build menus in the search dialog. See APIs below */
#define kMsgSearchMenuLength 64
typedef struct nsMsgSearchMenuItem 
{
    int16 attrib;
    char name[kSearchMenuLength];
    PRBool isEnabled;
} nsMsgSearchMenuItem;

struct nsMsgScopeTerm;
struct nsMsgResultElement;
struct nsMsgDIRServer;


#endif // _nsMsgSearchCore_H_
