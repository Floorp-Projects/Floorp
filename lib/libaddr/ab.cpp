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

/* Implementation of public C interfaces */
#include "xp.h"

#include "xpassert.h"
#include "aberror.h"
#include "abdefn.h"
#include "prefapi.h"
#include "addrbook.h"
#include "csid.h"

extern "C"
{
	extern int MK_OUT_OF_MEMORY;
	extern int MK_ADDR_FIRST_LAST_SEP;
	extern int MK_MSG_ENTRY_EXISTS_UPDATE;
	extern int MK_MSG_CONTINUE_ADDING;
}

// dummy class definition.....
class ABook {

};


/*
static int AB_GetSortType(ABPane* pABookPane, unsigned long* pSortby);
static int AB_GetSortAscending(ABPane* pABookPane, XP_Bool* pSortForward);
*/

XP_Bool AB_AttachUsersvCard ()
{
	XP_Bool prefBool = FALSE;
	PREF_GetBoolPref("mail.attach_vcard",&prefBool);

    return prefBool;
}

/****************************************************************************/
/* Create and initialize a mailing list pane which is the a view on an */
/* mailing list. It will provide a sorted view of all the entries in a */
/* mailing list.*/
/****************************************************************************/
int AB_InitMailingListPane(MLPane** ppMLPane,
								ABID* listID,
								DIR_Server* dir,
								ABook* pABook,
								MWContext* context,
								MSG_Master* master,
								unsigned long sortBy,
								XP_Bool sortForward)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

/****************************************************************************/
/* Create  a mailing list pane which is the a view on an */
/* mailing list. */
/****************************************************************************/
int AB_CreateMailingListPane(MLPane** ppMLPane,
								MWContext* context,
								MSG_Master* master)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}


/****************************************************************************/
/* Initialize a mailing list pane which is the a view on an */
/* mailing list. It will provide a sorted view of all the entries in a */
/* mailing list.*/
/****************************************************************************/
int AB_InitializeMailingListPane(MLPane* pMLPane,
								ABID* listID,
								DIR_Server* dir,
								ABook* pABook)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

/****************************************************************************/
/* Close the mailing list pane. Called when the view on an mailing list */
/* is being closed */
/****************************************************************************/
int AB_CloseMailingListPane(MLPane** ppMLPane)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}


/****************************************************************************/
/* Create and initialize the address book database */
/* This function is only included until all the fe's can begin using the */
/* new one */
/****************************************************************************/
int AB_InitAddressBook(DIR_Server* directory, ABook** ppABook)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}	


/****************************************************************************/
/* Create and initialize the address book database  */
/* automatically upgrade from the old html address book if there is one*/
/****************************************************************************/
int AB_InitializeAddressBook(DIR_Server* directory, ABook** ppABook, const char * pOldHTMLBook)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}	


/****************************************************************************/
/* Close the address book database */
/****************************************************************************/
int AB_CloseAddressBook(ABook** ppABook)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}	


/****************************************************************************/
/* Register a compose window with the address book */
/* The composition pane should do this everytime it opens */
/* so that name completion can take place */
/****************************************************************************/
int AB_RegisterComposeWindow(ABook* pABook, DIR_Server* directory)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}	

/****************************************************************************/
/* Unregister a compose window with the address book */
/* The composition pane should do this when it is getting closed */
/****************************************************************************/
int AB_UnregisterComposeWindow(ABook* pABook, DIR_Server* directory)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}	


/****************************************************************************/
/* Create and initialize the address book pane which is the a view on an */
/* address book. It will provide a sorted list of all the address book */
/* entries ids .*/
/****************************************************************************/
int AB_InitAddressBookPane(ABPane** ppABookPane,
								DIR_Server* dir,
								ABook* pABook,
								MWContext* context,
								MSG_Master* master,
								unsigned long sortBy,
								XP_Bool sortForward)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}


/****************************************************************************/
/* Create the address book pane which is the a view on an */
/* address book. */
/****************************************************************************/
int AB_CreateAddressBookPane(ABPane** ppABookPane,
								MWContext* context,
								MSG_Master* master)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}


/****************************************************************************/
/* Initialize the address book pane which is the a view on an */
/* address book. It will provide a sorted list of all the address book */
/* entries ids .*/
/****************************************************************************/
int AB_InitializeAddressBookPane(ABPane* pABookPane,
								DIR_Server* dir,
								ABook* pABook,
								unsigned long sortBy,
								XP_Bool sortForward)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}


/****************************************************************************/
/* Close the address book pane. Called when the view on an address book */
/* is being closed */
/****************************************************************************/
int AB_CloseAddressBookPane(ABPane** ppABookPane)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}	


/****************************************************************************/
/* Get number of entries in a particular pane */
/* For the address book you can ask for ALL, people, or mailing lists */
/* For mailing list contexts you will only be returned ALL */
/****************************************************************************/
int AB_GetEntryCount(DIR_Server* dir, ABook* pABook, uint32* count, ABID /* type */, ABID listID)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}


/****************************************************************************/
/* Change the Current directory */
/****************************************************************************/
int AB_ChangeDirectory(ABPane* pABookPane, DIR_Server* directory)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}


/****************************************************************************/
/* Process LDAP search results*/
/****************************************************************************/
int AB_LDAPSearchResults(ABPane* pABookPane, MSG_ViewIndex index, int32 num)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}


/****************************************************************************/
/* Finish a search */
/****************************************************************************/
int AB_FinishSearch(ABPane* pABookPane, MWContext* context)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

/****************************************************************************/
/* Get and Set how full names are constructed for people entries */
/* first last or last first */
/****************************************************************************/
XP_Bool AB_GetSortByFirstName(ABook* pABook)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}


void	AB_SetSortByFirstName(ABook* pABook, XP_Bool sortByFirst)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
}


/****************************************************************************/
/* Perform a command on the selected indices */
/* Right now this will only work on delete and mail to */
/****************************************************************************/
int AB_Command (ABPane* pABookPane, AB_CommandType command,
				 MSG_ViewIndex* indices, int32 numIndices)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

/****************************************************************************/
/* Before the front end displays any menu (each time), it should call this */
/* function for each command on that menu to determine how it should be */
/* displayed. */
/****************************************************************************/
 int AB_CommandStatus (ABPane* pABookPane,
						  AB_CommandType command,
						  MSG_ViewIndex* indices, int32 numindices,
						  XP_Bool *selectable_p,
						  MSG_COMMAND_CHECK_STATE *selected_p,
						  const char **display_string,
						  XP_Bool *plural_p)
 {
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
 }

/* Public implemetation of maitaining person entry */
int AB_AddUser(DIR_Server* dir, ABook* pABook, PersonEntry* pPerson, ABID* entryID)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_AddUserWithUI (MWContext *context, PersonEntry *person, DIR_Server *pab, XP_Bool lastOneToAdd)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_AddSenderToAddBook(ABook* pABook, MWContext* context, char* author, char* url) 
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}
/* Public implemetation of maitaining mailing list entry */
int AB_AddMailingList(DIR_Server* dir, ABook* pABook, MailingListEntry* pABList, ABID* entryID)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

/****************************************************************************/
/* Import Export various formats */
/****************************************************************************/
int AB_ImportFromFile(ABPane* pABookPane, MWContext* context)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_ImportFromVcard(AddressPane* pABookPane, const char* pVcard)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_ImportFromVcardURL(ABook* pABook, MWContext* context, const char* pVcard)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;

}

int AB_ExportToFile(ABPane* pABookPane, MWContext* context)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_ExportToVCard(ABook* pABook, DIR_Server* dir, ABID entryID, char** ppVcard)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_ExportToVCardFromPerson(ABook* pABook, PersonEntry* person, char** ppVcard)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_ExportToVCardTempFile(ABook* pABook, DIR_Server* dir, ABID entryID, char** filename)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_ConvertVCardsToExpandedName(ABook* pABook, const char* vCardString, XP_List ** ppEntries, int32 * numEntries)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}


/****************************************************************************/
/* Import Export various formats that wont do any prompting */
/* This is needed for the command line interface and automatic upgrading */
/****************************************************************************/
int AB_ImportFromFileNamed(ABook* pABook, char* filename)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_ExportToFileNamed(ABook* pABook, char* filename)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

ABID	AB_GetEntryIDAt(AddressPane* pABookPane, uint32 index)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return MSG_MESSAGEIDNONE;
}

uint32	AB_GetIndexOfEntryID (AddressPane* pABookPane, ABID entryID)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return MSG_VIEWINDEXNONE;
}

int	AB_GetEntryLine (ABPane * pABookPane, uint32 index,
					 AB_EntryLine * pEntryLine)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return MSG_VIEWINDEXNONE;
}


XP_Bool AB_GetPaneSortedAscending(ABPane * pABookPane)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return FALSE;
}

ABID AB_GetPaneSortedBy(ABPane * pABookPane)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return MSG_MESSAGEIDNONE;
}

/****************************************************************************/
/* Get information for every person entry */
/****************************************************************************/
int AB_GetType(DIR_Server* dir, ABook* pABook, ABID entryID, ABID* anID)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_GetFullName(DIR_Server* dir, ABook* pABook, ABID entryID, char* pname)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_GetNickname(DIR_Server* dir, ABook* pABook, ABID entryID, char* pname)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_GetGivenName(DIR_Server* dir, ABook* pABook, ABID entryID, char* pname)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_GetMiddleName(DIR_Server* dir, ABook* pABook, ABID entryID, char* pname)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_GetFamilyName(DIR_Server* dir, ABook* pABook, ABID entryID, char* pname)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_GetCompanyName(DIR_Server* dir, ABook* pABook, ABID entryID, char* pname)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_GetLocality(DIR_Server* dir, ABook* pABook, ABID entryID, char* pname)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_GetRegion(DIR_Server* dir, ABook* pABook, ABID entryID, char* pname)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_GetEmailAddress(DIR_Server* dir, ABook* pABook, ABID entryID, char* paddress)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_GetInfo(DIR_Server* dir, ABook* pABook, ABID entryID, char* pinfo)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_GetHTMLMail(DIR_Server* dir, ABook* pABook, ABID entryID, XP_Bool* pmail)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_GetExpandedName(DIR_Server* dir, ABook* pABook, ABID entryID, char** pname)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_GetTitle(DIR_Server* dir, ABook* pABook, ABID entryID, char* ptitle)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_GetPOAddress(DIR_Server* dir, ABook* pABook, ABID entryID, char* ppoaddress)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_GetStreetAddress(DIR_Server* dir, ABook* pABook, ABID entryID, char* pstreet)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_GetZipCode(DIR_Server* dir, ABook* pABook, ABID entryID, char* pzip)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_GetCountry(DIR_Server* dir, ABook* pABook, ABID entryID, char* pcountry)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_GetWorkPhone(DIR_Server* dir, ABook* pABook, ABID entryID, char* pphone)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_GetFaxPhone(DIR_Server* dir, ABook* pABook, ABID entryID, char* pphone)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_GetHomePhone(DIR_Server* dir, ABook* pABook, ABID entryID, char* pphone)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_GetDistName(DIR_Server* dir, ABook* pABook, ABID entryID, char* pname)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_GetSecurity(DIR_Server* dir, ABook* pABook, ABID entryID, short* security)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_GetCoolAddress(DIR_Server* dir, ABook* pABook, ABID entryID, char* paddress)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_GetUseServer(DIR_Server* dir, ABook* pABook, ABID entryID, short* use)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_GetEntryIDForPerson(DIR_Server* dir, ABook* pABook, ABID* entryID, 
						   PersonEntry* pPerson)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}


/****************************************************************************/
/* Modify information for an entry (person or mailing list) */
/****************************************************************************/
int AB_ModifyUser(DIR_Server* dir, ABook* pABook, ABID entryID, PersonEntry* pPerson)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_ModifyMailingList(DIR_Server* dir, ABook* pABook, ABID entryID, MailingListEntry* pList)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_ModifyMailingListAndEntries(MLPane* pMLPane, MailingListEntry* pList)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_ModifyMailingListAndEntriesWithChecks(MLPane* pMLPane, MailingListEntry* pList,
	MSG_ViewIndex *index, MSG_ViewIndex begIndex)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

/****************************************************************************/
/* Add/Remove entries for a mailing list */
/****************************************************************************/
int AB_GetEntryCountInMailingList(MLPane* pMLPane, uint32* count)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}


int AB_AddIDToMailingListAt(MLPane* pMLPane, ABID entryID, MSG_ViewIndex index)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_AddPersonToMailingListAt(MLPane* pMLPane, PersonEntry* person, MSG_ViewIndex index, ABID* entryID)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_RemoveIDFromMailingListAt(MLPane* pMLPane, MSG_ViewIndex index)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

int AB_ReplaceIDInMailingListAt(MLPane* pMLPane, ABID entryID, MSG_ViewIndex index)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}



/****************************************************************************/
/* Find index to first entry in a pane that matches the typedown */
/****************************************************************************/
int AB_GetIndexMatchingTypedown (ABPane* pABookPane, MSG_ViewIndex* index, 
	const char* aValue, MSG_ViewIndex startIndex)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}


/****************************************************************************/
/* Perform a search on an ldap directory */
/****************************************************************************/
int AB_SearchDirectory(ABPane* pABookPane, char * searchString)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}


/****************************************************************************/
/* Return the id of the name/nickname entries that match a string.  If we */
/* find a nickname that uniquely matches then return otherwise check the */
/* full name field. */
/****************************************************************************/
int AB_GetIDForNameCompletion(ABook* pABook, DIR_Server* dir, ABID* entryID, 
							 ABID* field, const char* aValue)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}


/****************************************************************************/
/* Return a string of expanded addresses from the personal address book */
/****************************************************************************/
char* AB_ExpandHeaderString(ABook* pABook, const char* addresses, 
							  XP_Bool expandfull)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return NULL;
}


int AB_GetMailingListsContainingID(ABook* /* pABook */, MSG_ViewIndex* /* plist */, ABID /* entryID */)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

/****************************************************************************/
/* This should not be called by any of the fes.  I placed it here so it		*/
/* could be called in the backend by other functions such as those in libmsg */
/* This allows for an increase in performance when adding a bunch of entries */
/* to the database															*/
/****************************************************************************/
int AB_SetIsImporting(ABook* pABook, XP_Bool isImporting)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

/****************************************************************************/
/* This should not be called by any of the fes.  I placed it here so it */
/* could be called in the backend by other functions in libmsg */
/* This will set the wantsHTML boolean to be set for all address book */
/* entries with that match the name and email address. */
/****************************************************************************/
int AB_SetHTMLForPerson(DIR_Server* dir, ABook* pABook, PersonEntry* pPerson)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

/****************************************************************************/
/* This should not be called by any of the fes.  I placed it here so it */
/* could be called in the backend by other functions in libmsg */
/* This will break a name apart into first name and last name. */
/* Be sure and set the WinCSID in the person structure */
/****************************************************************************/
int AB_BreakApartFirstName (ABook* pABook, PersonEntry* pPerson)
{
	// please ask your local FE Address book engineer why he is calling
	// this very very old API. 
	XP_ASSERT(FALSE);
	return -1;
}

///////////////////// PersonEntry struct methods
void PersonEntry::Initialize()
{
  	pNickName = nil;
	pGivenName = nil;
	pMiddleName = nil;
	pFamilyName = nil;
	pCompanyName = nil;
	pLocality = nil;
	pRegion = nil;
	pEmailAddress = nil;
	pInfo = nil;
	HTMLmail = FALSE;
	pTitle = nil;
	pAddress = nil;
	pPOAddress = nil;
	pZipCode = nil;
	pCountry = nil;
	pWorkPhone = nil;
	pFaxPhone = nil;
	pHomePhone = nil;
	pDistName = nil;
	Security = 0;
	pCoolAddress = nil;
	UseServer = 0;
	WinCSID = CS_DEFAULT;
}

///////////////////// PersonEntry struct methods
void PersonEntry::CleanUp()
{
  	XP_FREEIF (pNickName);
	XP_FREEIF (pGivenName);
	XP_FREEIF (pMiddleName);
	XP_FREEIF (pFamilyName);
	XP_FREEIF (pCompanyName);
	XP_FREEIF (pLocality);
	XP_FREEIF (pRegion);
	XP_FREEIF (pEmailAddress);
	XP_FREEIF (pInfo);
	HTMLmail = FALSE;
	XP_FREEIF (pTitle);
	XP_FREEIF (pPOAddress);
	XP_FREEIF (pAddress);
	XP_FREEIF (pZipCode);
	XP_FREEIF (pCountry);
	XP_FREEIF (pWorkPhone);
	XP_FREEIF (pFaxPhone);
	XP_FREEIF (pHomePhone);
	XP_FREEIF (pDistName);
	Security = 0;
	XP_FREEIF (pCoolAddress);
	UseServer = 0;
	WinCSID = CS_DEFAULT;
}

///////////////////// MailingListEntry struct methods
void MailingListEntry::Initialize()
{
  	pNickName = nil;
	pFullName = nil;
	pInfo = nil;
	pDistName = nil;
	WinCSID = CS_DEFAULT;
}


void MailingListEntry::CleanUp()
{
  	XP_FREEIF (pNickName);
	XP_FREEIF (pFullName);
	XP_FREEIF (pInfo);
	XP_FREEIF (pDistName);
	WinCSID = CS_DEFAULT;
}

///////////////////// AB_EntryLine struct methods
void AB_EntryLine::Initialize()
{
	// ask your local address book FE engineer why they are calling this stale API.
	XP_ASSERT(FALSE); 
  	entryType = 0 /* kABNone */;
	XP_STRCPY (fullname, "");
	XP_STRCPY (emailAddress, "");
	XP_STRCPY (companyName, "");
	XP_STRCPY (nickname, "");
	XP_STRCPY (locality, "");
	XP_STRCPY (workPhone, "");
}
