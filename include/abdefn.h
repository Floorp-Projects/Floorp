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

#ifndef _ABDefn_H_
#define	_ABDefn_H_

/*  max lengths for address book fields */
const int kMaxFullNameLength 		= 256;	/* common name */
const int kMaxNameLength 			= 64;	/* given, middle, family */
const int kMaxCompanyLength			= 128;	/* company name */
const int kMaxLocalityLength		= 128;	/* city */
const int kMaxRegionLength			= 128;	/* state */
const int kMaxCountryLength			= 128;	/* state */
const int kMaxEmailAddressLength 	= 256;
const int kMaxInfo					= 1024;
const int kMaxTitle					= 64;
const int kMaxAddress				= 128;
const int kMaxZipCode				= 40;
const int kMaxPhone					= 32;
const int32 kMaxDN					= 32768;
const int kMaxCoolAddress			= 256;
const int kMaxAddressEntry			= 1000;

const short kDefaultDLS = 0;
const short kSpecificDLS = 1;
const short kHostOrIPAddress = 2;

typedef uint32 ABID;

/* This structure represents a single line in the address book pane
 */
typedef struct AB_EntryLineTag
{
	ABID		entryType; 		/* type of entry either person or list */
	char		fullname[256];
	char		emailAddress[256];
	char		companyName[128];
	char		nickname[64];
	char		locality[128];
	char		workPhone[32];
#ifdef XP_CPLUSPLUS
public:
        void Initialize();
#endif
} AB_EntryLine;

typedef struct PersonEntryTag {
	char * pNickName;
	char * pGivenName;
	char * pMiddleName;
	char * pFamilyName;
	char * pCompanyName;
	char * pLocality;
	char * pRegion;
	char * pEmailAddress;
	char * pInfo;
	XP_Bool	HTMLmail;
	char * pTitle;
	char * pPOAddress;
	char * pAddress;
	char * pZipCode;
	char * pCountry;
	char * pWorkPhone;
	char * pFaxPhone;
	char * pHomePhone;
	char * pDistName;
	short  Security;
	char * pCoolAddress;
	short  UseServer;
	int16  WinCSID;


#ifdef XP_CPLUSPLUS
public:
        void Initialize();
		void CleanUp();
#endif
} PersonEntry; 


typedef struct MailingListEntryTag {
	char * pFullName;
	char * pNickName;
	char * pInfo;
	char * pDistName;
	int16  WinCSID;

#ifdef XP_CPLUSPLUS

public: 
	void Initialize();
	void CleanUp();
#endif
} MailingListEntry;  

const ABID	ABTypeAll		= 35;
const ABID	ABTypePerson	= 36;
const ABID  ABTypeList		= 37;

const unsigned long	ABTypeEntry			= 0x70634944;	/* ASCII - 'pcID' */
const unsigned long	ABFullName			= 0x636E2020;	/* ASCII - 'cn  ' */
const unsigned long	ABNickname			= 0x6E69636B;	/* ASCII - 'nick' */
const unsigned long	ABEmailAddress		= 0x6D61696C;	/* ASCII - 'mail' */
const unsigned long	ABLocality			= 0x6C6F6320;	/* ASCII - 'loc ' */
const unsigned long	ABCompany			= 0x6F726720;	/* ASCII - 'org ' */

/* defines for vcard support */
#define	vCardClipboardFormat		"+//ISBN 1-887687-00-9::versit::PDI//vCard"
#define vCardMimeFormat				"text/x-vcard"

typedef enum
{
 
  /* FILE MENU
	 =========
   */
  AB_NewMessageCmd,				/* Send a new message to the selected entries */

  AB_ImportCmd,					/* import a file into the address book */

  AB_SaveCmd,					/* export to a file */

  AB_ExportCmd,					/* same as AB_SaveCmd, but I like export better...*/

  AB_CloseCmd,					/* close the address book window */

  AB_NewAddressBook,			/* Create a new personal address book */

  AB_NewLDAPDirectory,			/* Create a new LDAP directory */


  /* EDIT MENU
	 =========
   */

  AB_UndoCmd,					/* Undoes the last operation. */

  AB_RedoCmd,					/* Redoes the last undone operation. */

  AB_DeleteCmd,					/* Causes the given entries to be
								   deleted. */

  AB_DeleteAllCmd,				/* Causes all occurrences of the given entries to be deleted. */

  AB_LDAPSearchCmd,				/* Perform an LDAP search */


  /* VIEW/SORT MENUS
	 ===============
   */

  AB_SortByTypeCmd,				/* Sort alphabetized by type. */
  AB_SortByFullNameCmd,			/* Sort alphabetizedby full name. */
  AB_SortByLocality,			/* Sort by state */
  AB_SortByNickname,			/* Sort by nickname */
  AB_SortByEmailAddress,		/* Sort by email address */
  AB_SortByCompanyName,			/* Sort by email address */
  AB_SortAscending,				/* Sort current column ascending */
  AB_SortDescending,			/* Sort current column descending */

  /* these are the new sort command IDs added for the 2 pane AB. Use these instead of the previous ones...*/
  AB_SortByColumnID0,
  AB_SortByColumnID1,
  AB_SortByColumnID2,
  AB_SortByColumnID3,
  AB_SortByColumnID4,
  AB_SortByColumnID5,
  AB_SortByColumnID6,

  /* ITEM MENU
	 ============
   */

   AB_AddUserCmd,				/* Add a user to the address book */
   AB_AddMailingListCmd,		/* Add a mailing list to the address book */
   AB_PropertiesCmd,			/* Get the properties of an entry */
   AB_CallCmd,					/* Call the entry using CallPoint */

   AB_ImportLdapEntriesCmd,		/* Add a user to the AB from an LDAP directory */

   /* Mailing List Pane Specific Commands!!! */
   AB_InsertLineCmd,			/* Insert a blank entry line into the mailing list pane */
   AB_ReplaceLineCmd			/* Replace the entry in the mailing list pane with a blank entry */

} AB_CommandType;

#endif
