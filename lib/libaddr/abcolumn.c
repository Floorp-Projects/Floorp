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

/* file: abcolumn.c 
** Some portions derive from public domain IronDoc code and interfaces.
**
** Changes:
** <0> 15Dec1997 first draft
*/

#ifndef _ABTABLE_
#include "abtable.h"
#endif

#include "xp_str.h"

/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/

static const char* AB_Column_kStdNamesInEnumOrder[] = {
	"isperson",        /* AB_Attrib_kIsPerson = 0,  00 - "" (t or f) */ 
	"changeseed",      /* AB_Attrib_kChangeSeed,    01 - "" */
	"fullname",        /* AB_Attrib_kFullname,      02 - "" */
	"nickname",        /* AB_Attrib_kNickname,      03 - "" */
	"middlename",      /* AB_Attrib_kMiddleName,    04 - "" */
	
	"familyname",      /* AB_Attrib_kFamilyName,    05 - "" */
	"companyname",     /* AB_Attrib_kCompanyName,   06 - "" */
	"region",          /* AB_Attrib_kRegion,        07 - "" */
	"email",           /* AB_Attrib_kEmail,         08 - "" */
	"info",            /* AB_Attrib_kInfo,          09 - "" */  
	
	"htmlmail",        /* AB_Attrib_kHtmlMail,      10 - "" (t or f) */      
	"expandedname",    /* AB_Attrib_kExpandedName,  11 - "" */
	"title",           /* AB_Attrib_kTitle,         12 - "" */
	"address",         /* AB_Attrib_kAddress,       13 - "" */
	"zip",             /* AB_Attrib_kZip,           14 - "" */
	
	"country",         /* AB_Attrib_kCountry,       15 - "" */
	"workphone",       /* AB_Attrib_kWorkPhone,     16 - "" */
	"homephone",       /* AB_Attrib_kHomePhone,     17 - "" */
	"security",        /* AB_Attrib_kSecurity,      18 - "" */
	"cooladdress",     /* AB_Attrib_kCoolAddress,   19 - "" (conf related) */ 
	
	"useserver",       /* AB_Attrib_kUseServer,     20 - "" (conf related) */ 
	"pager",           /* AB_Attrib_kPager,         21 - "" */
	"fax",             /* AB_Attrib_kFax,           22 - "" */
	"displayname",     /* AB_Attrib_kDisplayName,   23 - "" */
	"sender",          /* AB_Attrib_kSender,        24 - "" (mail and news) */
	
	"subject",         /* AB_Attrib_kSubject,	    25 - "" */
	"body",            /* AB_Attrib_kBody,	        26 - "" */
	"date",            /* AB_Attrib_kDate,	        27 - "" */
	"priority",        /* AB_Attrib_kPriority,	    28 - "" (mail only) */
	"msgstatus",       /* AB_Attrib_kMsgStatus,	    29 - "" */
	
	"to",              /* AB_Attrib_kTo,            30 - "" */
	"cc",              /* AB_Attrib_kCC,            31 - "" */
	"toorcc",          /* AB_Attrib_kToOrCC,        32 - "" */
	"commonname",      /* AB_Attrib_kCommonName,    33 - "" (LDAP only) */
	"822address",      /* AB_Attrib_k822Address,    34 - "" */
	
	"phonenumber",     /* AB_Attrib_kPhoneNumber,   35 - "" */
	"organization",    /* AB_Attrib_kOrganization,  36 - "" */
	"orgunit",         /* AB_Attrib_kOrgUnit,       37 - "" */
	"locality",        /* AB_Attrib_kLocality,      38 - "" */
	"streetaddress",   /* AB_Attrib_kStreetAddress, 39 - "" */
	
	"size",            /* AB_Attrib_kSize,          40 - "" */
	"anytext",         /* AB_Attrib_kAnyText,       41 - "" (header/body) */
	"keywords",        /* AB_Attrib_kKeywords,      42 - "" */
	"distname",        /* AB_Attrib_kDistName,      43 - ""  */ 
    "objectclass",     /* AB_Attrib_kObjectClass,   44 - "" */
         
    "jpegfile",        /* AB_Attrib_kJpegFile,      45 - "" */
    "location",        /* AB_Attrib_kLocation,      46 - "" (result list */
    "messagekey",      /* AB_Attrib_kMessageKey,    47 - "" (message elems) */
	"ageindays",       /* AB_Attrib_kAgeInDays,     48 - "" (purging news) */
	"givenname",       /* AB_Attrib_kGivenName,     49 - "" (sorting LDAP) */

	"surname",         /* AB_Attrib_kSurname,       50 - "" */
	"folderinfo",      /* AB_Attrib_kFolderInfo,    51 - "" (view thread) */
	"custom1",         /* AB_Attrib_kCustom1,       52 - "" (custom attr) */
	"custom2",         /* AB_Attrib_kCustom2,       53 - "" */
	"custom3",         /* AB_Attrib_kCustom3,       54 - "" */
	
	"custom4",         /* AB_Attrib_kCustom4,       55 - "" */
	"custom5",         /* AB_Attrib_kCustom5,       56 - "" */
	"messageid",       /* AB_Attrib_kMessageId,     57 - "" */
	"homeurl",         /* AB_Attrib_kHomeUrl,       58 - "" */
	"workurl",         /* AB_Attrib_kWorkUrl,       59 - "" */
	
	"imapurl",         /* AB_Attrib_kImapUrl,       60 - "" */
	"notifyurl",       /* AB_Attrib_kNotifyUrl,     61 - "" */
	"prefurl",         /* AB_Attrib_kPrefUrl,       62 - "" */
	"pageremail",      /* AB_Attrib_kPagerEmail,    63 - "" */
	"parentphone",     /* AB_Attrib_kParentPhone,   64 - "" */
	
	"gender",          /* AB_Attrib_kGender,        65 - "" */
	"postaladdress",   /* AB_Attrib_kPostalAddress, 66 - "" */
	"employeeid",      /* AB_Attrib_kEmployeeId,    67 - "" */
	"agent",           /* AB_Attrib_kAgent,         68 - "" */
	"bbs",             /* AB_Attrib_kBbs,           69 - "" */
	
	"bday",            /* AB_Attrib_kBday,          70 - "" (birthdate) */
	"calendar",        /* AB_Attrib_kCalendar,      71 - "" */
	"car",             /* AB_Attrib_kCar,           72 - "" */
	"carphone",        /* AB_Attrib_kCarPhone,      73 - "" */
	"categories",      /* AB_Attrib_kCategories,    74 - "" */
	
	"cell",            /* AB_Attrib_kCell,          75 - "" */
	"cellphone",       /* AB_Attrib_kCellPhone,     76 - "" */
	"charset",         /* AB_Attrib_kCharSet,       77 - "" (cs, csid) */
	"class",           /* AB_Attrib_kClass,         78 - "" */
	"geo",             /* AB_Attrib_kGeo,           79 - "" */
	
	"gif",             /* AB_Attrib_kGif,           80 - "" */
	"key",             /* AB_Attrib_kKey,           81 - "" (publickey) */
	"language",        /* AB_Attrib_kLanguage,      82 - "" */
	"logo",            /* AB_Attrib_kLogo,          83 - "" */
	"modem",           /* AB_Attrib_kModem,         84 - "" */
	
	"msgphone",        /* AB_Attrib_kMsgPhone,      85 - "" */
	"n",               /* AB_Attrib_kN,             86 - "n" */
	"note",            /* AB_Attrib_kNote,          87 - "" */
	"pagerphone",      /* AB_Attrib_kPagerPhone,    88 - "" */
	"pgp",             /* AB_Attrib_kPgp,           89 - "" */
	
	"photo",           /* AB_Attrib_kPhoto,         90 - "" */
	"rev",             /* AB_Attrib_kRev,           91 - "" */
	"role",            /* AB_Attrib_kRole,          92 - "" */
	"sound",           /* AB_Attrib_kSound,         93 - "" */
	"sortstring",      /* AB_Attrib_kSortString,    94 - "" */
	
	"tiff",            /* AB_Attrib_kTiff,          95 - "" */
	"tz",              /* AB_Attrib_kTz,            96 - "" (timezone) */
	"uid",             /* AB_Attrib_kUid,           97 - "" (uniqueid) */
	"version",         /* AB_Attrib_kVersion,       98 - "" */
	"voice",           /* AB_Attrib_kVoice,         99 - "" */
	
	"?col-name?",
	
	(const char*) 0 /* null terminated */
};

AB_API_IMPL(const char*)
AB_ColumnUid_AsString(ab_column_uid inStdColumnUid, AB_Env* cev) /*i*/
{
	const char* outString = 0;
	/* ab_Env* ev = ab_Env::AsThis(cev); */
	AB_Env_BeginMethod(cev, "AB_ColumnUid", "AsString")

	if ( AB_Uid_IsStandardColumn(inStdColumnUid) )
	{
		ab_num attrib = AB_ColumnUid_AsDbUid(inStdColumnUid);
		if ( attrib < AB_Attrib_kNumColumnAttributes )
			outString = AB_Column_kStdNamesInEnumOrder[ attrib ];
	}
	AB_Env_EndMethod(cev)
	return outString;
}

/*| AsStandardColumnUid: find which uid corresponds to the input name.  This
**| implementation does an excrutiatingly slow linear search that examines
**| each of the standard names in turn. We'll either do a faster version of
**| this later, or else we'll let ab_NameSet subclasses do better.
|*/
AB_API_IMPL(ab_column_uid)
AB_String_AsStandardColumnUid(const char* inStdColumnName, AB_Env* cev) /*i*/
{
	ab_column_uid outUid = 0;
	/* ab_Env* ev = ab_Env::AsThis(cev); */
	AB_Env_BeginMethod(cev, "AB_String", "AsStandardColumnUid")
	
  	const char** nameVector = AB_Column_kStdNamesInEnumOrder;
  	const char* name = *nameVector++;
  	for ( ; name; name = *nameVector++ ) /* until null termination is reached */
  	{
  		if ( !XP_STRCASECMP(name, inStdColumnName ) )
  		{
  			ab_num index = (--nameVector - AB_Column_kStdNamesInEnumOrder);
  			outUid = AB_Attrib_AsStdColUid(index);
  			break;
  		}
  	}

	AB_Env_EndMethod(cev)
	return outUid;
}
