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

#ifndef _AddrBook_H_
#define	_AddrBook_H_

#include "xp_core.h"
#include "msgcom.h"
#include "abdefn.h"
#include "dirprefs.h"

#ifdef XP_CPLUSPLUS
class MLPane;
class ABPane;
class AddressPane;
class ABook;
#else
typedef struct MLPane MLPane;
typedef struct ABPane ABPane;
typedef struct AddressPane AddressPane;
typedef struct ABook ABook;
#endif

XP_BEGIN_PROTOS

#define AB_kGromitDbFileName "da5id.nab"
#define AB_CONFIG_USE_GROMIT_FILE_FORMAT 1

/****************************************************************************/
/* Get the address book .*/
/****************************************************************************/
ABook* FE_GetAddressBook(MSG_Pane* pane);

/****************************************************************************/
/* This is a callback into the FE to bring up a modal property sheet */
/* for modifying an existing entry or creating a new one from a */
/* person structure. If entryId != MSG_MESSAGEIDNONE then it is the */
/* entryID of the entry to modify.  Each FE should Return TRUE if the user */
/* hit ok return FALSE if they hit cancel and return -1 if there was a */
/* problem creating the window or something */
/****************************************************************************/
int FE_ShowPropertySheetFor (MWContext* context, ABID entryID, 
		PersonEntry* pPerson);

/****************************************************************************/
/* Return whether or not to attach the users */
/* vcard to outgoing messages */
/****************************************************************************/
XP_Bool AB_AttachUsersvCard(void);

/****************************************************************************/
/* Convert the address book error code to  .*/
/****************************************************************************/
int AB_ConvertABErrToMKErr(uint32 err); 

/****************************************************************************/
/* Create and initialize the address book pane which is the a view on an */
/* address book. It will provide a sorted list of all the address book */
/* entries ids . This is intended to be used in a two step */
/* process of create and then initialize */
/****************************************************************************/
int AB_InitAddressBookPane(ABPane** ppABookPane, 
								DIR_Server* dir,
								ABook* pABook,
								MWContext* context,
								MSG_Master* master,
								unsigned long sortBy,
								XP_Bool sortForward);

/****************************************************************************/
/* Create the address book pane which is the a view on an */
/* address book. */
/****************************************************************************/
int AB_CreateAddressBookPane(ABPane** ppABookPane, 
								MWContext* context,
								MSG_Master* master);

/****************************************************************************/
/* Initialize the address book pane which is the a view on an */
/* address book. It will provide a sorted list of all the address book */
/* entries ids .*/
/****************************************************************************/
int AB_InitializeAddressBookPane(ABPane* ppABookPane, 
								DIR_Server* dir,
								ABook* pABook,
								unsigned long sortBy,
								XP_Bool sortForward);

/****************************************************************************/
/* Close the address book pane. Called when the view on an address book */
/* is being closed */
/****************************************************************************/
int AB_CloseAddressBookPane(ABPane** ppABookPane);


/****************************************************************************/
/* Create and initialize a mailing list pane which is the a view on an */
/* mailing list. It will provide a sorted view of all the entries in a */
/* mailing list. This is intended to be used in as a one one step process */
/****************************************************************************/
int AB_InitMailingListPane(MLPane** ppABookPane, 
 								ABID* listID,
								DIR_Server* dir,
								ABook* pABook,
								MWContext* context,
								MSG_Master* master,
								unsigned long sortBy,
								XP_Bool sortForward);

/****************************************************************************/
/* Create a mailing list pane which is the a view on an */
/* mailing list. This is intended to be used in a two step */
/* process of create and then initialize */
/****************************************************************************/
int AB_CreateMailingListPane(MLPane** ppABookPane, 
								MWContext* context,
								MSG_Master* master);

/****************************************************************************/
/* Initialize a mailing list pane which is the a view on an */
/* mailing list. It will provide a sorted view of all the entries in a */
/* mailing list.*/
/****************************************************************************/
int AB_InitializeMailingListPane(MLPane* pABookPane, 
 								ABID* listID,
								DIR_Server* dir,
								ABook* pABook);

/****************************************************************************/
/* Close the mailing list pane. Called when the view on an mailing list */
/* is being closed */
/****************************************************************************/
int AB_CloseMailingListPane(MLPane** ppMLPane);

/****************************************************************************/
/* Register a compose window with the address book */
/* The composition pane should do this everytime it opens */
/* so that name completion can take place */
/****************************************************************************/
int AB_RegisterComposeWindow(ABook* pABook, DIR_Server* directory);

/****************************************************************************/
/* Unregister a compose window with the address book */
/* The composition pane should do this when it is getting closed */
/****************************************************************************/
int AB_UnregisterComposeWindow(ABook* pABook, DIR_Server* directory);

/****************************************************************************/
/* Change the Current directory */
/****************************************************************************/
int AB_ChangeDirectory(ABPane* pABookPane, DIR_Server* directory);

/****************************************************************************/
/* Begin a search on the LDAP directory */
/****************************************************************************/
int AB_SearchDirectory(ABPane* pABookPane, char* searchString);

/****************************************************************************/
/* Process LDAP search results*/
/****************************************************************************/
int AB_LDAPSearchResults(ABPane* pABookPane, MSG_ViewIndex index, int32 num);

/****************************************************************************/
/* Finish an LDAP search */
/****************************************************************************/
int AB_FinishSearch(ABPane* pABookPane, MWContext* context);

/****************************************************************************/
/* Create and initialize the address book database */
/****************************************************************************/
int AB_InitAddressBook(DIR_Server* directory, ABook** ppABook);


/****************************************************************************/
/* Create and initialize the address book database */
/* upgrading the old html address book if we need to*/
/****************************************************************************/
int AB_InitializeAddressBook(DIR_Server* directory, ABook** ppABook,
	const char * pOldHTMLBook);


/****************************************************************************/
/* Close the address book database */
/****************************************************************************/
int AB_CloseAddressBook(ABook** ppABook);

/****************************************************************************/
/* Add a person entry to the database */
/****************************************************************************/
int AB_AddUser(DIR_Server* dir, ABook* pABook, PersonEntry* pPerson, 
			   ABID* entryID);
int AB_AddUserWithUI (MWContext *context, PersonEntry *person,
					  DIR_Server *pab, XP_Bool lastOneToAdd);

/****************************************************************************/
/* Add an entry to the database from a url*/
/****************************************************************************/
int AB_AddSenderToAddBook(ABook* pABook, MWContext* context, char* author, 
						  char* url);

/****************************************************************************/
/* Add a mailing list to the database */
/****************************************************************************/
int AB_AddMailingList(DIR_Server* dir, ABook* pABook, 
					  MailingListEntry* pABList, ABID* entryID);

/****************************************************************************/
/* Perform a command on the selected indices */
/* Right now this will only work on delete and mail to */
/****************************************************************************/
int AB_Command (ABPane* pane, AB_CommandType command,
				 MSG_ViewIndex* indices, int32 numindices);

/****************************************************************************/
/* Before the front end displays any menu (each time), it should call this */
/* function for each command on that menu to determine how it should be */
/* displayed. */
/****************************************************************************/
 int AB_CommandStatus (ABPane* pane,
						  AB_CommandType command,
						  MSG_ViewIndex* indices, int32 numindices,
						  XP_Bool *selectable_p,
						  MSG_COMMAND_CHECK_STATE *selected_p,
						  const char **display_string,
						  XP_Bool *plural_p);

/****************************************************************************/
/* Get and Set how full names are constructed for people entries */
/* first last or last first */
/****************************************************************************/
XP_Bool AB_GetSortByFirstName(ABook* pABook);
void	AB_SetSortByFirstName(ABook* pABook, XP_Bool sortby);

/****************************************************************************/
/* Import Export various formats that will prompt for filename */
/****************************************************************************/
int AB_ImportFromFile(ABPane* pABookPane, MWContext* context);
int AB_ExportToFile(ABPane* pABookPane, MWContext* context);

/****************************************************************************/
/* This is only used right now for drop/paste of a vcard */
/****************************************************************************/
int AB_ImportFromVcard(AddressPane* pABookPane, const char* pVcard);

/****************************************************************************/
/* This is only used right now for drop of a vcard */
/* It it a helper function that can be called by the FE to */
/* convert a vcard to an valid rfc822 address that can be used */
/* in the address widget of the compose window */
/****************************************************************************/
int AB_ConvertVCardsToExpandedName(ABook* pABook, const char* vCardString, 
		XP_List ** ppEntries, int32 * numEntries);

/****************************************************************************/
/* Import Export various formats that wont do any prompting */
/* This is needed for the command line interface and automatic upgrading */
/****************************************************************************/
int AB_ImportFromFileNamed(ABook* pABook, char* filename);
int AB_ExportToFileNamed(ABook* pABook, char* filename);

/****************************************************************************/
/* This function is only called from the backend. */
/* It is used to process the addbook url that is only used in mimevcrd.c */
/* It probably will never be called by an FE */
/****************************************************************************/
int AB_ImportFromVcardURL(ABook* pABook, MWContext* context, const char* pVcard);

/****************************************************************************/
/* This will create a buffer with a vcard in it.  It is used for constructing */
/* a buffer that can be used in drag/drop or copy to the clipboard */
/****************************************************************************/
int AB_ExportToVCard(ABook* pABook, DIR_Server* dir, ABID entryID, 
					 char** ppVcard);
int AB_ExportToVCardFromPerson(ABook* pABook, PersonEntry* pPerson, 
					 char** ppVcard);

/****************************************************************************/
/* This will create a temp file for a vcard.  This function is only called */
/* from the backend compose window for constructing a vcard attachment */
/* It probably will never be called by an FE */
/****************************************************************************/
int AB_ExportToVCardTempFile(ABook* pABook, DIR_Server* dir, ABID entryID, 
							 char** filename);

/****************************************************************************/
/* Operations on panes/view */
/****************************************************************************/

/****************************************************************************/
/* Get the unique database id at a particular pane index */
/****************************************************************************/
ABID	AB_GetEntryIDAt(AddressPane* pABookPane, uint32 index);
uint32	AB_GetIndexOfEntryID (AddressPane* pABookPane, ABID entryID);
int		AB_GetEntryLine (ABPane * pABookPane, uint32 index,
					 AB_EntryLine * pEntryLine);

/****************************************************************************/
/* Get info about how a pane is sorted */
/****************************************************************************/
XP_Bool AB_GetPaneSortedAscending(ABPane * pABookPane);
ABID AB_GetPaneSortedBy(ABPane * pABookPane);

/****************************************************************************/
/* Get number of entries */
/* For the address book you can ask for ALL, people, or mailing lists */
/* For mailing list panes you will only be returned ALL */
/****************************************************************************/
int AB_GetEntryCount(DIR_Server* dir, ABook* pABook, uint32* count, 
					   ABID etype, ABID listID);

/****************************************************************************/
/* Modify information for an entry (person or mailing list) */
/****************************************************************************/
int AB_ModifyUser(DIR_Server* dir, ABook* pABook, ABID entryID, PersonEntry* pPerson);
int AB_ModifyMailingList(DIR_Server* dir, ABook* pABook, ABID entryID, 
						 MailingListEntry* pEntry);


/****************************************************************************/
/* Modify information for a mailing list that has been modified */
/* but not committed in a mailing list pane */
/****************************************************************************/
int AB_ModifyMailingListAndEntries(MLPane* pMLPane, MailingListEntry* pABList);

/****************************************************************************/
/* Modify information for a mailing list that has been modified */
/* but not committed in a mailing list pane.  Some of the error checking */
/* that was perfomed in add/replace in a mailing list had to be moved to */
/* here.  The fe's should be calling this function instead of the one above */
/****************************************************************************/
int AB_ModifyMailingListAndEntriesWithChecks(MLPane* pMLPane, MailingListEntry* pList,
	MSG_ViewIndex *index, MSG_ViewIndex begIndex);

/****************************************************************************/
/* Get information for every entry (person or mailing list) */
/****************************************************************************/
int AB_GetType(DIR_Server* dir, ABook* pABook, ABID entryID, ABID* type);
int AB_GetFullName(DIR_Server* dir, ABook* pABook, ABID entryID, char* pname);
int AB_GetNickname(DIR_Server* dir, ABook* pABook, ABID entryID, char* pname);

/****************************************************************************/
/* Get information for every person entry */
/****************************************************************************/
int AB_GetGivenName(DIR_Server* dir, ABook* pABook, ABID entryID, char* pname);
int AB_GetMiddleName(DIR_Server* dir, ABook* pABook, ABID entryID, char* pname);
int AB_GetFamilyName(DIR_Server* dir, ABook* pABook, ABID entryID, char* pname);
int AB_GetCompanyName(DIR_Server* dir, ABook* pABook, ABID entryID, char* pname);
int AB_GetLocality(DIR_Server* dir, ABook* pABook, ABID entryID, char* pname);
int AB_GetRegion(DIR_Server* dir, ABook* pABook, ABID entryID, char* pname);
int AB_GetEmailAddress(DIR_Server* dir, ABook* pABook, ABID entryID, char* paddress);
int AB_GetInfo(DIR_Server* dir, ABook* pABook, ABID entryID, char* pinfo);
int AB_GetHTMLMail(DIR_Server* dir, ABook* pABook, ABID entryID, XP_Bool* pHTML);
int AB_GetExpandedName(DIR_Server* dir, ABook* pABook, ABID entryID, char** pname);
int AB_GetTitle(DIR_Server* dir, ABook* pABook, ABID entryID, char* ptitle);
int AB_GetPOAddress(DIR_Server* dir, ABook* pABook, ABID entryID, char* ppoaddress);
int AB_GetStreetAddress(DIR_Server* dir, ABook* pABook, ABID entryID, char* pstreet);
int AB_GetZipCode(DIR_Server* dir, ABook* pABook, ABID entryID, char* pzip);
int AB_GetCountry(DIR_Server* dir, ABook* pABook, ABID entryID, char* pcountry);
int AB_GetWorkPhone(DIR_Server* dir, ABook* pABook, ABID entryID, char* pphone);
int AB_GetFaxPhone(DIR_Server* dir, ABook* pABook, ABID entryID, char* pphone);
int AB_GetHomePhone(DIR_Server* dir, ABook* pABook, ABID entryID, char* pphone);
int AB_GetDistName(DIR_Server* dir, ABook* pABook, ABID entryID, char* pname);
int AB_GetSecurity(DIR_Server* dir, ABook* pABook, ABID entryID, short* security);
int AB_GetCoolAddress(DIR_Server* dir, ABook* pABook, ABID entryID, char* paddress);
int AB_GetUseServer(DIR_Server* dir, ABook* pABook, ABID entryID, short* use);

int AB_GetEntryIDForPerson(DIR_Server* dir, ABook* pABook, ABID* entryID, 
						   PersonEntry* pPerson);

/****************************************************************************/
/* Add/Remove entries for a mailing list */
/****************************************************************************/
int AB_GetEntryCountInMailingList(MLPane* pMLPane, uint32* count);
int AB_AddIDToMailingListAt(MLPane* pMLPane, ABID entryID, MSG_ViewIndex index);
int AB_RemoveIDFromMailingListAt(MLPane* pMLPane, MSG_ViewIndex index);
int AB_ReplaceIDInMailingListAt(MLPane* pMLPane, ABID entryID, MSG_ViewIndex index);
int AB_AddPersonToMailingListAt(MLPane* pMLPane, PersonEntry* person, 
								MSG_ViewIndex index, ABID* entryID);

/****************************************************************************/
/* Find index to first entry in a pane that matches the typedown */
/****************************************************************************/
int AB_GetIndexMatchingTypedown(ABPane* pABookPane, 
	MSG_ViewIndex* index, const char* aValue, MSG_ViewIndex startIndex);

/****************************************************************************/
/* Return the id of the name/nickname entries that match a string.  If we */
/* find a nickname that uniquely matches then return otherwise check the */
/* full name field. */
/****************************************************************************/
int AB_GetIDForNameCompletion(ABook* pABook, DIR_Server* dir, ABID* entryID, 
							 ABID* field, const char* aValue);

/****************************************************************************/
/* Return a string of expanded addresses from the address book */
/****************************************************************************/
char* AB_ExpandHeaderString(ABook* pABook, const char* addresses, 
							  XP_Bool expandfull);

/****************************************************************************/
/* Return the ids of mailing lists that a entry is a member of */
/* This was mentioned at one time as a requirement in the ui but it may */
/* go away */
/****************************************************************************/
int AB_GetMailingListsContainingID(ABook* pABook, MSG_ViewIndex* plist, 
									 ABID entryID);

/****************************************************************************/
/* This should not be called by any of the fes.  I placed it here so it */
/* could be called in the backend by other functions such as those in libmsg */
/* This allows for an increase in performance when adding a bunch of entries */
/* to the database */
/****************************************************************************/
int AB_SetIsImporting(ABook* pABook, XP_Bool isImporting);

/****************************************************************************/
/* This should not be called by any of the fes.  I placed it here so it */
/* could be called in the backend by other functions in libmsg */
/* This will set the wantsHTML boolean to be set for all address book */
/* entries with that match the name and email address. */
/****************************************************************************/
int AB_SetHTMLForPerson(DIR_Server* dir, ABook* pABook, PersonEntry* pPerson);

/****************************************************************************/
/* This should not be called by any of the fes.  I placed it here so it */
/* could be called in the backend by other functions in libmsg */
/* This will break a name apart into first name and last name. */
/* Be sure and set the WinCSID in the person structure */
/****************************************************************************/
int AB_BreakApartFirstName (ABook* pABook, PersonEntry* pPerson);

XP_END_PROTOS

#endif
