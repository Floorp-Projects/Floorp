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
/*public APIs for the address books.
 *
 */

#include "xp.h"

#include "abcom.h"
#include "abcpane.h"
#include "abpane2.h"
#include "abcinfo.h"
#include "abpicker.h"
#include "addrprsr.h"
#include "prefapi.h"
#include "namecomp.h"

#include "msgurlq.h"

#include "libi18n.h"
#include "intl_csi.h"

extern "C" 
{
	extern int MK_ADDR_FIRST_LAST_SEP;
}

#if !(defined(SOLARIS) && defined(NS_USE_NATIVE)) && !(defined(HPUX))
inline
#endif
AB_Pane * CastABPane(MSG_Pane * pane)
{
	if (pane)
		if (pane->GetPaneType() == AB_ABPANE || pane->GetPaneType() == AB_PICKERPANE)
			return (AB_Pane *) pane;
	return NULL;
}


#if !(defined(SOLARIS) && defined(NS_USE_NATIVE)) && !(defined(HPUX))
inline
#endif
AB_ContainerPane * CastABContainerPane(MSG_Pane * pane)
{
	if (pane)
		if (pane->GetPaneType() == AB_CONTAINERPANE)
			return (AB_ContainerPane *) pane;
	return NULL;
}

#if !(defined(SOLARIS) && defined(NS_USE_NATIVE)) && !(defined(HPUX))
inline
#endif
AB_MailingListPane * CastABMailingListPane(MSG_Pane * pane)
{
	if (pane)
		if (pane->GetPaneType() == AB_MAILINGLISTPANE)
			return (AB_MailingListPane *) pane;
	return NULL;
}

#if !(defined(SOLARIS) && defined(NS_USE_NATIVE)) && !(defined(HPUX))
inline
#endif
AB_PersonPane * CastABPersonPane(MSG_Pane * pane)
{
	if (pane)
		if (pane->GetPaneType() == AB_PERSONENTRYPANE)
			return (AB_PersonPane * ) pane;
	return NULL;
}

#if !(defined(SOLARIS) && defined(NS_USE_NATIVE)) && !(defined(HPUX))
inline
#endif
AB_PickerPane * CastABPickerPane(MSG_Pane * pane)
{
	if (pane)
		if (pane->GetPaneType() == AB_PICKERPANE)
			return (AB_PickerPane *) pane;
	return NULL;
}


//-----------------------------------------------------------------------------
// ------------------------ Public API implementation -------------------------
//-----------------------------------------------------------------------------

AB_API int AB_SetShowPropertySheetForDirFunc(MSG_Pane * pane, AB_ShowPropertySheetForDirFunc * func)
{
	AB_ContainerPane * abcPane = CastABContainerPane(pane);
	if (abcPane)
	{
		 abcPane->SetShowPropSheetForDirFunc(func);
		 return AB_SUCCESS;
	}

	else
		return AB_INVALID_PANE;
}

AB_API int AB_SetShowPropertySheetForEntryFunc(MSG_Pane * pane,	AB_ShowPropertySheetForEntryFunc * func)
{
	if (pane)
	{
		pane->SetShowPropSheetForEntryFunc(func);
		return AB_SUCCESS;
	}
	else
		return AB_INVALID_PANE;
#if 0 
	AB_Pane * abPane = CastABPane(pane);
	if (abPane)
	{
		abPane->SetShowPropSheetForEntryFunc(func);
		return AB_SUCCESS;
	}
	else
	{
		AB_ContainerPane * abcPane = CastABContainerPane(pane);
		if (abcPane)
		{
			abcPane->SetShowPropSheetForEntryFunc(func);
			return AB_SUCCESS;
		}
	}
	
	return AB_INVALID_PANE;
#endif
}


/*******************************************************************************
The following are APIs just about all of the panes will respond to. 
********************************************************************************/

AB_API int AB_ClosePane(MSG_Pane * pane)
{
	// you know, you could really make a good argument that the ab panes should be 
	// subclassed from an abpane so we could make a virtual method for Close instead
	// of this...

	// WARNING: the picker pane can be cast as a AB_Pane as well...since Close is static and
	// as such cannot be virtual, make sure we cast as a picker pane before trying as an ab_pane....
	// oh what a tangled web we weave...
	AB_PickerPane * abPickerPane = CastABPickerPane(pane);
	if (abPickerPane)
		return AB_PickerPane::Close(abPickerPane);

	AB_Pane * abPane = CastABPane(pane);
	if (abPane)
		return AB_Pane::Close(abPane);

	AB_ContainerPane * abcPane = CastABContainerPane(pane);
	if (abcPane)
		return AB_ContainerPane::Close(abcPane);
	
	AB_MailingListPane * mlPane = CastABMailingListPane(pane);
	if (mlPane)
		return AB_MailingListPane::Close(mlPane);
	
	AB_PersonPane * personPane = CastABPersonPane(pane);
	if (personPane)
		return AB_PersonPane::Close(personPane);
	
	return AB_INVALID_PANE;
}

/**************************************************************************************
	Virtual List View APIs
***************************************************************************************/
AB_API int AB_SetFEPageSizeForPane(MSG_Pane * pane, uint32 pageSize)
{
	AB_Pane * abPane = CastABPane(pane);
	if (abPane) // valid cast?
		return abPane->SetPageSize(pageSize);
	else
		return AB_INVALID_PANE;
}

/**************************************************************************************
	Selection APIs
***************************************************************************************/
AB_API XP_Bool AB_UseExtendedSelection(MSG_Pane * pane)
{
	AB_Pane * abPane = CastABPane(pane);
	if (abPane)
		return abPane->UseExtendedSelection();
	else
		return FALSE;
}

AB_API int AB_AddSelection(MSG_Pane * pane, MSG_ViewIndex index)
{
	AB_Pane * abPane = CastABPane(pane);
	if (abPane && abPane->AddSelection(index))
		return AB_SUCCESS;
	else
		return AB_FAILURE;
}

AB_API XP_Bool AB_IsSelected(MSG_Pane * pane, MSG_ViewIndex index)
{
	AB_Pane * abPane = CastABPane(pane);
	if (abPane)
		return abPane->IsSelected(index);
	else
		return FALSE;
}

AB_API int AB_RemoveSelection(MSG_Pane * pane, MSG_ViewIndex index)
{
	AB_Pane * abPane = CastABPane(pane);
	if (abPane && abPane->RemoveSelection(index))
		return AB_SUCCESS;
	else
		return AB_FAILURE;
}

AB_API void AB_RemoveAllSelections(MSG_Pane * pane)
{
	AB_Pane * abPane = CastABPane(pane);
	if (abPane) // valid cast?
		abPane->RemoveSelection(MSG_VIEWINDEXNONE);
}

/***************************************************************************************
	Type down and name completion APIs. Both actions are asynchronous. Type down generates
	a MSG_PaneNotifyTypeDownCompleted pane notification and name completion calls a FE
	regsitered call back. 
****************************************************************************************/
AB_API int AB_TypedownSearch(MSG_Pane * pane, const char * typedownValue, MSG_ViewIndex startIndex)
{
	AB_Pane * abPane = CastABPane(pane);
	if (abPane) // valid cast??
		return abPane->TypedownSearch(typedownValue, startIndex);
	else
		return AB_INVALID_PANE;
}


#ifdef FE_IMPLEMENTS_VISIBLE_NC
AB_API int AB_NameCompletionSearch(MSG_Pane * pane, const char * completionValue, AB_NameCompletionExitFunction * exitFunction, XP_Bool userSeesPane, void * cookie)
{
	AB_PickerPane * abPickerPane = CastABPickerPane(pane);
	if (abPickerPane)
		return abPickerPane->NameCompletionSearch(completionValue, exitFunction, userSeesPane, cookie);
	else
		return AB_INVALID_PANE;
}
#else
AB_API int AB_NameCompletionSearch(MSG_Pane * pane, const char * completionValue, AB_NameCompletionExitFunction * exitFunction, void * cookie)
{
	AB_PickerPane * abPickerPane = CastABPickerPane(pane);
	if (abPickerPane)
		return abPickerPane->NameCompletionSearch(completionValue, exitFunction, TRUE /* assume pane is visible */, cookie);
	else
		return AB_INVALID_PANE;
}
#endif

/* Caller must free string!!! */
AB_API char * AB_GetNameCompletionDisplayString(AB_NameCompletionCookie * cookie)
{
	if (cookie)
		return cookie->GetNameCompletionDisplayString();
	else
		return NULL;
}

/* Caller must free string!!! */
AB_API char * AB_GetHeaderString(AB_NameCompletionCookie * cookie)
{
	if (cookie)
		return cookie->GetHeaderString();
	else
		return NULL;
}


/* Caller must free string!!! */
AB_API char * AB_GetExpandedHeaderString(AB_NameCompletionCookie * cookie)
{
	if (cookie)
		return cookie->GetExpandedHeaderString();
	else
		return NULL;
}

AB_API AB_EntryType AB_GetEntryTypeForNCCookie(AB_NameCompletionCookie * cookie)
{
	if (cookie)
		return cookie->GetEntryType();
	else
		return AB_NakedAddress;  // we need some default value for this...
}


AB_API int AB_FreeNameCompletionCookie(AB_NameCompletionCookie * cookie)
{
	if (cookie)
		AB_NameCompletionCookie::Close(cookie);
	return AB_SUCCESS;
}

/****************************************************************************************
	Name completion picker pane specific APIs. 
*****************************************************************************************/

AB_API int AB_CreateABPickerPane(MSG_Pane ** abPickerPane,MWContext * context,MSG_Master * master, uint32 pageSize)
{
	return AB_PickerPane::Create(abPickerPane, context, master, pageSize);
}

AB_API AB_ContainerType AB_GetEntryContainerType(MSG_Pane * pane,MSG_ViewIndex index)
{
	AB_PickerPane * abPickerPane = CastABPickerPane(pane);
	if (abPickerPane)
		return abPickerPane->GetEntryContainerType(index);
	else
		return AB_UnknownContainer;
}

AB_API AB_NameCompletionCookie * AB_GetNameCompletionCookieForNakedAddress(const char * nakedAddress)
{
	AB_NameCompletionCookie * cookie = new AB_NameCompletionCookie(nakedAddress);
	if (cookie)
		return cookie;  // caller must call AB_FreeNameCompletionCookie when done with this cookie
	else
		return NULL;
}

AB_API AB_NameCompletionCookie * AB_GetNameCompletionCookieForIndex(MSG_Pane * pane, MSG_ViewIndex index)
{
	AB_PickerPane * abPickerPane = CastABPickerPane(pane);
	if (abPickerPane)
		return abPickerPane->GetNameCompletionCookieForIndex(index);
	else
	{
		AB_Pane * abPane = CastABPane(pane);
		AB_MailingListPane * mList = CastABMailingListPane(pane);
		if (abPane || mList)
		{
			// anyone else, get the ctr for the index and convert the entry..
			ABID entryID = AB_ABIDUNKNOWN;
			AB_GetABIDForIndex(pane, index, &entryID);
			AB_ContainerInfo * ctr = AB_GetContainerForIndex(pane, index);
			if (ctr && entryID != AB_ABIDUNKNOWN)
			{
				AB_NameCompletionCookie * cookie = new AB_NameCompletionCookie(pane, ctr, entryID);
				if (cookie)
					return cookie;
			}
		}
	}

	return NULL;
}


/***********************************************************************************
 AB_Pane specific APIs
 **********************************************************************************/

AB_API int AB_CreateABPane(MSG_Pane ** pane, MWContext * context,MSG_Master * master)
{
	return AB_Pane::Create(pane, context, master);
}

AB_API int AB_InitializeABPane(MSG_Pane * pane, AB_ContainerInfo * abContainer)
{
	AB_Pane * abPane = CastABPane(pane);
	if (abPane)
		return abPane->LoadContainer(abContainer);
	else
		return AB_INVALID_PANE;
}

AB_API int AB_ChangeABContainer(MSG_Pane * pane, AB_ContainerInfo * container)
{
	AB_Pane * abPane = CastABPane(pane);
	if (abPane)
		return abPane->LoadContainer(container);
	else
		return AB_INVALID_PANE;
}

AB_API int AB_GetABIDForIndex(MSG_Pane * pane, MSG_ViewIndex index, ABID * id)
{
	AB_Pane * abPane = CastABPane(pane);
	if (abPane && id)
	{
		*id = abPane->GetABIDForIndex(index);
		return AB_SUCCESS;
	}
	else
		return AB_INVALID_PANE;
}

AB_API int AB_GetEntryIndex(MSG_Pane * pane, ABID id, MSG_ViewIndex * index)
{
	AB_Pane * abPane = CastABPane(pane);
	if (abPane)
	{
		*index = abPane->GetEntryIndexForID(id);
		return AB_SUCCESS; // because I changed GetEntryIndexForID...it now returns an index instead of an error code
	}
	return AB_INVALID_PANE;
}

AB_API int AB_SearchDirectoryAB2(MSG_Pane * pane, char * searchString)
{
	AB_Pane * abPane = CastABPane(pane);
	if (abPane)
		return abPane->SearchDirectory(searchString);
	return AB_INVALID_PANE;
}

AB_API int AB_LDAPSearchResultsAB2(MSG_Pane * pane, MSG_ViewIndex index, int32 num)
{
	AB_Pane * abPane = CastABPane(pane);
	if (abPane)
		return abPane->LDAPSearchResults(index, num);
	return AB_INVALID_PANE;
}

AB_API int AB_FinishSearchAB2(MSG_Pane * pane)
{
	AB_Pane * abPane = CastABPane(pane);
	if (abPane)
		return abPane->FinishSearch();
	else
		return AB_INVALID_PANE;
}

AB_API int AB_CommandAB2(MSG_Pane * pane, AB_CommandType command, MSG_ViewIndex * indices, int32 numIndices)
{
	if (pane)
		return pane->DoCommand((MSG_CommandType)command, indices, numIndices);
	else
		return AB_INVALID_PANE;
}

AB_API int AB_CommandStatusAB2(MSG_Pane * srcPane, AB_CommandType command, MSG_ViewIndex * indices, int32 numIndices,
							XP_Bool * selectable_p, MSG_COMMAND_CHECK_STATE * selected_p, const char ** displayString, 
							XP_Bool * plural_p)
{
	if (srcPane)
		return (int) srcPane->GetCommandStatus((MSG_CommandType)command, indices, numIndices, selectable_p, selected_p, displayString, plural_p);
	else
		return AB_INVALID_PANE;

}

/****************************************************************************************
AB_ContainerInfo General APIs - adding users and a sender. Does not require a pane
*****************************************************************************************/

AB_API int AB_AddUserAB2(AB_ContainerInfo * abContainer, AB_AttributeValue * valuesArray, uint16 numItems, ABID * entryID /* back end fills this value */)
{
	if (abContainer)
		return abContainer->AddEntry(valuesArray, numItems, entryID);
	else
		return AB_INVALID_CONTAINER;
}

AB_API int AB_AddUserWithUIAB2(MSG_Pane * srcPane, AB_ContainerInfo * abContainer, AB_AttributeValue * valuesArray, uint16 numItems, XP_Bool lastOneToAdd)
{
	if (abContainer)
		return abContainer->AddUserWithUI(srcPane, valuesArray, numItems, lastOneToAdd);
	else
		return AB_INVALID_CONTAINER;
}

AB_API int AB_AddSenderAB2(MSG_Pane * srcPane, AB_ContainerInfo * abContainer, const char * author, const char * url)
{
	int status = AB_SUCCESS;

	int num;
	int32 length;
	char * ptr;

	if (srcPane && url && author && (XP_STRNCASECMP(url, "mailto:?to=", 11) == 0)) /* make sure it is a mailto url */
	{
		// skip the mailto url info
		url += 11;

		// find the first address by finding the next section
		ptr = XP_STRCHR(url, '&');
		length = (ptr ? (ptr - url) : XP_STRLEN(url));
		char * buf = (char *) XP_ALLOC(length + 1);
		if (buf)
		{
			XP_MEMCPY(buf, url, length);
			buf[length] = 0;
			buf = NET_UnEscape(buf);

			// check if there is a name and address
			char * name  = NULL;
			char * address = NULL;
			num = MSG_ParseRFC822Addresses(buf, &name, &address);
			if (num)
			{
				if (!(*name))
					name = XP_STRDUP(author);
				status = AB_AddNameAndAddress(srcPane, abContainer, name, address, FALSE);

				// advance to the cc part
				if ( (XP_STRNCASECMP(ptr, "&cc=", 4) == 0) && (status == AB_SUCCESS))
				{
					ptr += 4; // skip over &cc=....
					char * ptr1 = XP_STRCHR(ptr, '&');
					length = (ptr1 ? (ptr1 - ptr) : XP_STRLEN(ptr));
					buf = (char *) XP_ALLOC(length + 1);
					if (buf)
					{
						XP_MEMCPY(buf, ptr, length);
						buf[length] = 0;
						buf = NET_UnEscape(buf);
						
						char * ccNames = NULL;
						char * ccAddresses = NULL;

						num = MSG_ParseRFC822Addresses(buf, &ccNames, &ccAddresses);

						char * currentName = ccNames;
						char * currentAddress = ccAddresses;
						int loopStatus = AB_SUCCESS;
						for (int i = 0; i < num && loopStatus == AB_SUCCESS; i++)
						{
							loopStatus = AB_AddNameAndAddress(srcPane, abContainer, currentName, currentAddress, FALSE);
							// advance name & address to the next pair.
							if (*currentName)
								currentName += XP_STRLEN(currentName) + 1;
							if (*currentAddress)
								currentAddress += XP_STRLEN(currentAddress) + 1;  
						}

						status = loopStatus;
						XP_FREEIF(buf);
						if (ccNames)
							XP_FREE(ccNames);
						if (ccAddresses)
							XP_FREEIF(ccAddresses);
					}
				} // end of if cc part
			} // end of if at least one address to parse

			XP_FREE(buf);
			if (name)
				XP_FREE(name);
			if (address)
				XP_FREE(address);
		}
		else
			status = AB_OUT_OF_MEMORY;
		
	}

	return status;
}

/* the following is called by the msgdbview when it wants to add a name and a address to the specified container */
AB_API int AB_AddNameAndAddress(MSG_Pane * pane, AB_ContainerInfo * abContainer, const char * name, const char * address, XP_Bool lastOneToAdd)
{
	int status = AB_SUCCESS;
	AB_AttribValueArray attribs;
	char * tempname = NULL;
	if (pane && abContainer)
	{
		INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(pane->GetContext());

		if (XP_STRLEN (name))
			tempname =	XP_STRDUP (name);
		else
		{
			int len = 0;
			char * at = NULL;
			if (address)
			{
				// if the mail address doesn't contain @
				// then the name is the whole email address
				if ((at = XP_STRCHR(address, '@')) == NULL)
					tempname = MSG_ExtractRFC822AddressNames (address);
				else 
				{
					// otherwise extract everything up to the @
					// for the name
					len = XP_STRLEN (address) - XP_STRLEN (at);
					tempname = (char *) XP_ALLOC (len + 1);
					XP_STRNCPY_SAFE (tempname, address, len + 1);
				}
			}
		}
	
		char *converted = INTL_DecodeMimePartIIStr((const char *) tempname, INTL_DefaultWinCharSetID(0), FALSE);
		if (converted && (converted != tempname))
		{
			XP_FREE(tempname);
			tempname = converted;
		}
	
		// initialize various attributes
		uint16 numAttributes = 6;
		AB_AttributeValue * values = (AB_AttributeValue *) XP_ALLOC(sizeof(AB_AttributeValue) * numAttributes);
		if (values)
		{
			// first, break tempname (display name) into components for first/last name
			char * firstName = NULL;
			char * lastName = NULL;

			AB_SplitFullName(tempname, &firstName, &lastName, INTL_GetCSIWinCSID(c));

			values[0].attrib = AB_attribGivenName;
			if (firstName)
				values[0].u.string = firstName;
			else
				AB_CopyDefaultAttributeValue(AB_attribGivenName, &values[0]);

			values[1].attrib = AB_attribEmailAddress;
			if (address)
				values[1].u.string = XP_STRDUP(address);
			else
				AB_CopyDefaultAttributeValue(AB_attribEmailAddress, &values[1]);
	
			values[2].attrib = AB_attribEntryType;
			values[2].u.entryType = AB_Person;
	
			values[3].attrib = AB_attribWinCSID;
			values[3].u.shortValue = INTL_GetCSIWinCSID(c);

			values[4].attrib = AB_attribDisplayName;
			values[4].u.string = tempname;   /* because temp name happens to be the display name */

			values[5].attrib = AB_attribFamilyName;
			if (lastName)
				values[5].u.string = lastName;
			else
				AB_CopyDefaultAttributeValue(AB_attribFamilyName, &values[5]);

			/* AB_BreakApartFirstName (FE_GetAddressBook(NULL), &person); */
			status = AB_AddUserWithUIAB2 (pane, abContainer, values, numAttributes, lastOneToAdd);
			AB_FreeEntryAttributeValues(values, numAttributes);
		}
	}
	else
		status = ABError_NullPointer;
		
	return status;
}

/****************************************************************************************
Drag and Drop Related APIs - vcards, ab lines, containers, etc. 
*****************************************************************************************/

AB_API int AB_DragEntriesIntoContainer(MSG_Pane * srcPane, const MSG_ViewIndex *indices, int32 numIndices, 
									   AB_ContainerInfo * destContainer, AB_DragEffect request)
{
	// we'll first try it as an AB_Pane, but it might be an ABContainerPane
	AB_Pane * srcABPane = CastABPane(srcPane); // try ab Pane first
	if (srcABPane)
		return srcABPane->DragEntriesIntoContainer(indices, numIndices, destContainer, request);
	else
	{
		AB_ContainerPane  * srcABCPane = CastABContainerPane(srcPane);
		if (srcABCPane)
			return srcABCPane->DragEntriesIntoContainer(indices, numIndices, destContainer, request);
		else
		{
			AB_MailingListPane * mlPane = CastABMailingListPane(srcPane);
			if (mlPane)
				return mlPane->DragEntriesIntoContainer(indices, numIndices, destContainer, request);
		}

	}

	return AB_INVALID_PANE;
}

AB_API AB_DragEffect AB_DragEntriesIntoContainerStatus(MSG_Pane * srcPane, const MSG_ViewIndex *indices, int32 numIndices,
														   AB_ContainerInfo * destContainer, AB_DragEffect request)
{
	// we'll first try it as an AB_Pane, but it might be an ABContainerPane
	AB_Pane * srcABPane = CastABPane(srcPane); // try ab Pane first
	if (srcABPane)
		return srcABPane->DragEntriesIntoContainerStatus(indices, numIndices, destContainer, request);
	else
	{
		AB_ContainerPane  * srcABCPane = CastABContainerPane(srcPane);
		if (srcABCPane)
			return srcABCPane->DragEntriesIntoContainerStatus(indices, numIndices, destContainer, request);
		else
		{
			AB_MailingListPane * mlPane = CastABMailingListPane(srcPane);
			if (mlPane)
				return mlPane->DragEntriesIntoContainerStatus(indices, numIndices, destContainer, request);
		}

	}
		return AB_Drag_Not_Allowed;
}

/***************************************************************************************

  Asynchronous copy APIs. These APIs are used by mkabook.cpp for asycnh copy/move/delete
  entry operations between containers. 

***************************************************************************************/

AB_API int AB_BeginEntryCopy(AB_ContainerInfo * srcContainer, MWContext * context, AB_AddressBookCopyInfo * copyInfo, 
							 void ** copyCookie, XP_Bool * copyFinished)
{
	if (srcContainer)
		return srcContainer->BeginCopyEntry(context, copyInfo, copyCookie, copyFinished);
	else
		return AB_INVALID_CONTAINER;
}

AB_API int AB_MoreEntryCopy(AB_ContainerInfo * srcContainer,MWContext * context, AB_AddressBookCopyInfo * copyInfo, 
							void ** copyCookie, XP_Bool * copyFinished)
{
	if (srcContainer)
		return srcContainer->MoreCopyEntry(context, copyInfo, copyCookie, copyFinished);
	else
		return AB_INVALID_CONTAINER;
}

AB_API int AB_FinishEntryCopy(AB_ContainerInfo * srcContainer, MWContext * context, AB_AddressBookCopyInfo * copyInfo, void ** copyCookie)
{
	if (srcContainer)
		return srcContainer->FinishCopyEntry(context, copyInfo, copyCookie);
	else
		return AB_INVALID_CONTAINER;
}

AB_API int AB_InterruptEntryCopy(AB_ContainerInfo * srcContainer, MWContext * context,AB_AddressBookCopyInfo * copyInfo, void ** copyCookie)
{
	if (srcContainer)
		return srcContainer->InterruptCopyEntry(context, copyInfo, copyCookie);
	else
		return AB_INVALID_CONTAINER;
}

/****************************************************************************************
Importing and Exporting - ABs from files, vcards...
*****************************************************************************************/
#ifdef FE_IMPLEMENTS_NEW_IMPORT
AB_API int AB_ImportData(MSG_Pane * pane, AB_ContainerInfo * destContainer, const char * buffer, int32 bufSize, AB_ImportExportType dataType)
{
	if (destContainer)
		return destContainer->ImportData(pane, buffer, bufSize, dataType);
	else
		return AB_INVALID_CONTAINER;
}

#else
AB_API int AB_ImportData(AB_ContainerInfo * destContainer, const char * buffer, int32 bufSize, AB_ImportExportType dataType)
{
	if (destContainer)
		return destContainer->ImportData(NULL, buffer, bufSize, dataType);
	else
		return AB_INVALID_CONTAINER;
}
#endif

AB_API XP_Bool AB_ImportDataStatus (AB_ContainerInfo * destContainer, AB_ImportExportType dataType)
{
	if (destContainer)
		return destContainer->ImportDataStatus(dataType);
	else
		return FALSE;
}

#ifdef FE_IMPLEMENTS_NEW_IMPORT
AB_API int AB_ExportData(MSG_Pane * pane, AB_ContainerInfo * srcContainer, char ** buffer, /* filename or NULL. or if type = vcard, then BE allocated & filled vcard */
				int32 * bufSize, /* ignored unless Vcard in which case FE allocates, BE fills */
				AB_ImportExportType dataType) /* valid types: filename, prompt for filename, VCard */
{
	if (srcContainer)
		return srcContainer->ExportData(pane, buffer, bufSize, dataType);
	else
		return AB_INVALID_CONTAINER;
}
#else
AB_API int AB_ExportData(AB_ContainerInfo * srcContainer, char ** buffer, /* filename or NULL. or if type = vcard, then BE allocated & filled vcard */
				int32 * bufSize, /* ignored unless Vcard in which case FE allocates, BE fills */
				AB_ImportExportType dataType) /* valid types: filename, prompt for filename, VCard */
{
	if (srcContainer)
		return srcContainer->ExportData(NULL, buffer, bufSize, dataType);
	else
		return AB_INVALID_CONTAINER;
}
#endif

AB_API int AB_ImportBegin(AB_ContainerInfo * container, MWContext * context, const char * fileName, void ** importCookie, XP_Bool * importFinished)
{
	if (container)
		return container->ImportBegin(context, fileName, importCookie, importFinished);
	else
		return AB_INVALID_CONTAINER;
}

AB_API int AB_ImportMore(AB_ContainerInfo * container, void ** importCookie, XP_Bool * importFinished)
{
	if (container)
		return container->ImportMore(importCookie, importFinished);
	else
		return AB_INVALID_CONTAINER;
}

AB_API int AB_ImportInterrupt(AB_ContainerInfo * container, void ** importCookie)
{
	if (container)
		return container->ImportInterrupt(importCookie);
	else
		return AB_INVALID_CONTAINER;
}

AB_API int AB_ImportFinish(AB_ContainerInfo * container, void ** importCookie)
{
	if (container)
		return container->ImportFinish(importCookie);
	else
		return AB_INVALID_CONTAINER;
}

AB_API int AB_ImportProgress(AB_ContainerInfo * container, void * importCookie, uint32 * position, uint32 * fileLength, uint32 * passCount)
{
	if (container)
		return container->ImportProgress(importCookie, position,fileLength, passCount);
	else
		return AB_INVALID_CONTAINER;
}


AB_API int AB_ExportBegin(AB_ContainerInfo * container,	MWContext * context, const char * fileName, void ** exportCookie, XP_Bool * exportFinished)
{
	if (container)
		return container->ExportBegin(context, fileName, exportCookie, exportFinished);
	else
		return AB_INVALID_CONTAINER;
}

AB_API int AB_ExportMore(AB_ContainerInfo * container, void ** exportCookie, XP_Bool * exportFinished)
{
	if (container)
		return container->ExportMore(exportCookie, exportFinished);
	else
		return AB_INVALID_CONTAINER;
}

AB_API int AB_ExportInterrupt(AB_ContainerInfo * container, void ** exportCookie)
{
	if (container)
		return container->ExportInterrupt(exportCookie);
	else
		return AB_INVALID_CONTAINER;
}

AB_API int AB_ExportFinish(AB_ContainerInfo * container, void ** exportCookie)
{
	if (container)
		return container->ExportFinish(exportCookie);
	else
		return AB_INVALID_CONTAINER;
}

AB_API int AB_ExportProgress(AB_ContainerInfo * container, void * exportCookie, uint32 * numberExported, uint32 * totalEntries)
{
	if (container)
		return container->ExportProgress(exportCookie, numberExported, totalEntries);
	else
		return AB_INVALID_CONTAINER;
}

/* used only in the back end to generate an addbook copy url for copying, moving and deleting entries from/to and address book */
AB_API int AB_CreateAndRunCopyInfoUrl (MSG_Pane * pane, AB_ContainerInfo * srcContainer, ABID * idArray, int32 numItems, AB_ContainerInfo * destContainer, AB_AddressCopyInfoState action)
{
	int status = AB_SUCCESS;
	if (srcContainer && idArray && pane && numItems)
	{
		char * url = AB_ContainerInfo::BuildCopyUrl();
		AB_AddressBookCopyInfo * copyInfo = (AB_AddressBookCopyInfo *) XP_ALLOC(sizeof(AB_AddressBookCopyInfo));
		if (copyInfo)
		{
			copyInfo->destContainer = destContainer;
			if (destContainer)
				copyInfo->destContainer->Acquire();  // ref count it...release will occurr when url is finished...
			copyInfo->srcContainer = srcContainer;
			if (srcContainer)
				copyInfo->srcContainer->Acquire();
			copyInfo->idArray = idArray;
			copyInfo->numItems = numItems;
			copyInfo->state = action;
			URL_Struct *url_s = NET_CreateURLStruct (url, NET_DONT_RELOAD);
			if (url_s)
			{
				url_s->owner_data = (void *) copyInfo;
				MSG_UrlQueue::AddUrlToPane(url_s, AB_ContainerInfo::PostAddressBookCopyUrlExitFunc, pane, FALSE, NET_DONT_RELOAD);
			}
			else
				AB_FreeAddressBookCopyInfo(copyInfo);
		} // if copyinfo
		else
			status = AB_OUT_OF_MEMORY;
		XP_FREEIF(url); 
	}
	else
		status = ABError_NullPointer;

	return status;
}

/* used only in the back end to generate add ldap to abook urls and adds them to the url queue...
	For each entry, we fire off an ldap url to fetch its attributes..*/
AB_API int AB_ImportLDAPEntriesIntoContainer(MSG_Pane * pane, const MSG_ViewIndex * indices, int32 numIndices, AB_ContainerInfo * destContainer)
{
	int status = AB_SUCCESS;

	// vars we need to get the distinguished name for each entry....
	AB_AttributeValue * value = NULL;
	uint16 numItems = 1;
	AB_AttribID attrib = AB_attribDistName;

	if (pane && destContainer && numIndices)
	{
		MSG_UrlQueue *pQueue = new MSG_AddLdapToAddressBookQueue (pane);

		for (int32 i = 0; i < numIndices; i++)
		{
			// for each index, get its container...
			AB_ContainerInfo * ctrToUse = AB_GetContainerForIndex(pane, indices[i]);
			// make sure src ctr is an ldap ctr
			AB_LDAPContainerInfo * ldapCtr = NULL;
			if (ctrToUse)
				ldapCtr = ctrToUse->GetLDAPContainerInfo();
			if (ldapCtr)
			{
				AB_GetEntryAttributesForPane(pane, indices[i], &attrib, &value, &numItems);
				if (value && (numItems == 1) && value->u.string && *(value->u.string))  // make sure we have a distinguished name..
				{
					char * url = ldapCtr->BuildLDAPUrl("addbook-ldap", value->u.string);
					AB_AddressBookCopyInfo * copyInfo = (AB_AddressBookCopyInfo *) XP_ALLOC(sizeof(AB_AddressBookCopyInfo));
					if (copyInfo)
					{
						copyInfo->destContainer = destContainer;
						copyInfo->destContainer->Acquire();  // ref count it...release will occurr when url is finished...
						copyInfo->srcContainer = NULL;
						copyInfo->idArray = NULL;
						copyInfo->numItems = NULL;
						URL_Struct *url_s = NET_CreateURLStruct (url, NET_DONT_RELOAD);
						if (url_s)
						{
							url_s->owner_data = (void *) copyInfo;
							pQueue->AddUrlToPane(url_s, AB_ContainerInfo::PostAddressBookCopyUrlExitFunc, pane, FALSE, NET_DONT_RELOAD);
						}
					} // if copyinfo
					else
						status = AB_OUT_OF_MEMORY;
					XP_FREEIF(url);
				} // if value
				
				AB_FreeEntryAttributeValue(value);
				value = NULL; // reset for next pass...
			} // if ldap ctr
		} // for each index loop
		
		pQueue->GetNextUrl(); // Start doing the work. Queue deletes itself when it's done
	}
	else
		status = ABError_NullPointer;

	return status;
}


/****************************************************************************************
ABContainer Pane --> Creation, Loading, getting line data for each container. 
*****************************************************************************************/
AB_API int AB_CreateContainerPane(MSG_Pane ** abContainerPane, MWContext * context, MSG_Master * master)
{
	AB_ContainerPane::Create(abContainerPane, context, master);
	if (abContainerPane)
		return AB_SUCCESS;
	else
		return AB_FAILURE;
}	

AB_API int AB_InitializeContainerPane(MSG_Pane * pane)
{
	AB_ContainerPane * abcPane = CastABContainerPane(pane);
	if (abcPane)
		return abcPane->Initialize();
	else
		return AB_INVALID_PANE;
}

AB_API MSG_ViewIndex AB_GetIndexForContainer(MSG_Pane * pane /* container pane */, AB_ContainerInfo * container)
{
	AB_ContainerPane * abcPane = CastABContainerPane(pane);
	if (abcPane)
		return abcPane->GetIndexForContainer(container);
	else
		return AB_INVALID_PANE;
}

AB_API AB_ContainerInfo * AB_GetContainerForIndex(MSG_Pane * pane, const MSG_ViewIndex index)
{
	AB_ContainerPane * abcPane = CastABContainerPane(pane);
	if (abcPane)
		return abcPane->GetContainerForIndex(index);
	else
	{
		AB_Pane * abPane = CastABPane(pane);
		if (abPane)
			return abPane->GetContainerForIndex(index);
	}
	
	return NULL;
}

/*****************************************************************************************************
	Getting/Setting container properties on a pane or directly on the container 
*****************************************************************************************************/

AB_API int AB_GetContainerAttributeForPane(MSG_Pane * pane, MSG_ViewIndex index, 
									AB_ContainerAttribute attrib, AB_ContainerAttribValue ** value)
{
	uint16 numItems = 1;
	return AB_GetContainerAttributesForPane(pane, index, &attrib, value, &numItems);
}

AB_API int AB_GetContainerAttribute(AB_ContainerInfo * container, AB_ContainerAttribute attrib, AB_ContainerAttribValue ** value)
{
	uint16 numItems = 1;
	return AB_GetContainerAttributes(container, &attrib, value, &numItems);
}

AB_API int AB_GetContainerAttributes(AB_ContainerInfo * container, AB_ContainerAttribute * attribsArray, AB_ContainerAttribValue ** valuesArray, 
									 uint16 * numItems)
{
	if (container)
		return container->GetAttributes(attribsArray, valuesArray, numItems);
	else
		return AB_INVALID_CONTAINER;
}

AB_API int AB_GetContainerAttributesForPane(MSG_Pane * pane, MSG_ViewIndex index,AB_ContainerAttribute * attribsArray,
											AB_ContainerAttribValue ** valuesArray, uint16 * numItems)
{
	AB_ContainerPane * abcPane = CastABContainerPane(pane);
	if (abcPane)
		return abcPane->GetContainerAttributes(index, attribsArray, valuesArray, numItems);
	else
	{
		AB_PickerPane * abPickerPane = CastABPickerPane(pane);
		if (abPickerPane)
			return abPickerPane->GetContainerAttributes(index, attribsArray, valuesArray, numItems);
	}
	
	return AB_INVALID_PANE;
}

AB_API int AB_SetContainerAttributeForPane(MSG_Pane * pane, MSG_ViewIndex index, AB_ContainerAttribValue * value)
{
	return AB_SetContainerAttributesForPane(pane, index, value, 1);
}

AB_API int AB_SetContainerAttribute(AB_ContainerInfo * container, AB_ContainerAttribValue * value)
{
	return AB_SetContainerAttributes(container, value, 1);
}

AB_API int AB_SetContainerAttributes(AB_ContainerInfo * container, AB_ContainerAttribValue * valuesArray,	uint16 numItems)
{
	if (container)
		return container->SetAttributes(valuesArray, numItems);
	else
		return AB_INVALID_CONTAINER;
}


AB_API int AB_SetContainerAttributesForPane(MSG_Pane * pane, MSG_ViewIndex index, AB_ContainerAttribValue * valuesArray,uint16 numItems)
{
	AB_ContainerPane * abcPane = CastABContainerPane(pane);
	if (abcPane)
		return abcPane->SetContainerAttributes(index,valuesArray, numItems);
	else
		return AB_INVALID_PANE;
}

AB_API int AB_FreeContainerAttribValues(AB_ContainerAttribValue * valuesArray, uint16 numItems)
{
	if (valuesArray)
	{
		for (int i = 0; i < numItems; i++)
		{
			if ( AB_IsStringContainerAttribValue(&valuesArray[i]) && XP_STRLEN(valuesArray[i].u.string) > 0 )
				XP_FREEIF(valuesArray[i].u.string);  // we can have a string attribute & it doesn't have a value
		}
		XP_FREE(valuesArray);
	}
	return AB_SUCCESS;
}

AB_API int AB_FreeContainerAttribValue(AB_ContainerAttribValue * value)
{
	return AB_FreeContainerAttribValues(value, 1);
}

AB_API XP_Bool AB_IsStringContainerAttribValue(AB_ContainerAttribValue * value)
{
	// only known string attribs are: name
	if (value)
		if (value->attrib == attribName)
			return TRUE;
	return FALSE;
}

AB_API int AB_GetNumRootContainers(MSG_Pane * pane, int32 * numRootContainers) /* FE allocated, BE fills */
{
	AB_ContainerPane * abcPane = CastABContainerPane(pane);
	if (abcPane)
		return abcPane->GetNumRootContainers(numRootContainers);
	else
		return AB_INVALID_PANE;
}

AB_API int AB_GetOrderedRootContainers(MSG_Pane * pane, AB_ContainerInfo ** ctrArray, /* FE allocated & freed */
								   int32 * numCtrs)
{
	AB_ContainerPane * abcPane = CastABContainerPane(pane);
	if (abcPane)
		return abcPane->GetOrderedRootContainers(ctrArray, numCtrs);
	else
		return AB_INVALID_PANE;
}


AB_API DIR_Server * AB_GetDirServerForContainer(AB_ContainerInfo * container)
{
	if (container)
		return container->GetDIRServer();
	return NULL;
}

AB_API int AB_UpdateDIRServerForContainer(AB_ContainerInfo * abContainer)
{
	if (abContainer)
		return abContainer->UpdateDIRServer();
	return AB_INVALID_CONTAINER;
}

AB_API int AB_UpdateDIRServerForContainerPane(MSG_Pane * pane, DIR_Server * directory)
{
	AB_ContainerPane * abcPane = CastABContainerPane(pane);
	if (abcPane)
		return abcPane->UpdateDIRServer(directory);
	else
		return AB_INVALID_PANE;
}

/***************************************************************************************************
  
	APIs for getting column information for containers and the picker pane...

****************************************************************************************************/

AB_API AB_ColumnInfo * AB_GetColumnInfo (AB_ContainerInfo * container, AB_ColumnID columnID)
{
	if (container)
		return container->GetColumnInfo(columnID);
	else
		return NULL;
}

AB_API int AB_GetColumnAttribIDs(AB_ContainerInfo * container, AB_AttribID * attribIDs, int * numAttribs)
{
	if (container)
		return container->GetColumnAttribIDs(attribIDs, numAttribs);
	else
	{
		if (numAttribs)
			*numAttribs = 0;
		return AB_INVALID_CONTAINER;
	}
}

AB_API int AB_GetNumColumnsForContainer(AB_ContainerInfo * container)
{
	if (container)
		return container->GetNumColumnsForContainer();
	else
		return 0;
}

/* we also have pane versions which are used by the picker pane */
AB_API AB_ColumnInfo * AB_GetColumnInfoForPane(MSG_Pane * pane,AB_ColumnID columnID)
{
	AB_PickerPane * abPickerPane = CastABPickerPane(pane);
	if (abPickerPane)
		return abPickerPane->GetColumnInfo(columnID);
	else
		return NULL;
}

AB_API int AB_GetNumColumnsForPane(MSG_Pane * pane)
{
	AB_PickerPane * abPickerPane = CastABPickerPane(pane);
	if (abPickerPane)
		return abPickerPane->GetNumColumnsForPicker();
	else
		return 0;
}

AB_API int AB_GetColumnAttribIDsForPane(MSG_Pane * pane, AB_AttribID * attribIDs, int * numAttribs)
{
	AB_PickerPane * abPickerPane = CastABPickerPane(pane);
	if (abPickerPane)
		return abPickerPane->GetColumnAttribIDs(attribIDs, numAttribs);
	else
	{
		if (numAttribs)
			*numAttribs = 0;
		return AB_INVALID_CONTAINER;
	}
}

AB_API int AB_FreeColumnInfo(AB_ColumnInfo * columnInfo)
{
	/* right now, I'm treating the display string as the resource string..no need to free it. This might change!!! */
	if (columnInfo)
	{
		if (columnInfo->displayString && XP_STRLEN(columnInfo->displayString) > 0)
			XP_FREE(columnInfo->displayString);

		XP_FREE(columnInfo);
	}
	return AB_SUCCESS;
}

/*****************************************************************************************************
	Getting/Setting entry properties on a pane or directly on the container 
*****************************************************************************************************/

AB_API int AB_GetEntryAttributeForPane(MSG_Pane * pane, MSG_ViewIndex index, AB_AttribID attrib, AB_AttributeValue ** value)
{ 
	uint16 numItems = 1;
	return AB_GetEntryAttributesForPane(pane, index, &attrib, value, &numItems);
}

AB_API int AB_GetEntryAttribute(AB_ContainerInfo * abContainer, ABID entryID, AB_AttribID attrib, AB_AttributeValue ** value)
{
	uint16 numItems = 1;
	return AB_GetEntryAttributes(abContainer,entryID,&attrib, value, &numItems);
}

AB_API int AB_GetEntryAttributes(AB_ContainerInfo * abContainer, ABID entryID, AB_AttribID * attribs, 
								 AB_AttributeValue ** values, uint16 * numItems)
{
	if (abContainer)
		return abContainer->GetEntryAttributes(NULL, entryID, attribs, values, numItems);
	else
		return AB_INVALID_CONTAINER;
}

AB_API int AB_GetEntryAttributesForPane(MSG_Pane * pane, MSG_ViewIndex index, AB_AttribID * attribs, 
										AB_AttributeValue ** values, uint16 * numItems)
{
	AB_Pane * abPane = CastABPane(pane);
	if (abPane)
		return abPane->GetEntryAttributes(index, attribs, values, numItems);
	else
		return AB_INVALID_PANE;
}

AB_API int AB_SetEntryAttributeForPane(MSG_Pane * pane, MSG_ViewIndex index, AB_AttributeValue * value)
{
	return AB_SetEntryAttributesForPane(pane, index, value, 1);
}

AB_API int AB_SetEntryAttribute(AB_ContainerInfo * abContainer, ABID entryID, AB_AttributeValue * value)
{
	return AB_SetEntryAttributes(abContainer, entryID, value, 1);
}

AB_API int AB_SetEntryAttributes(AB_ContainerInfo * abContainer, ABID entryID, AB_AttributeValue * valuesArray, uint16 numItems)
{
	if (abContainer)
		return abContainer->SetEntryAttributes(entryID, valuesArray, numItems);
	else
		return AB_INVALID_CONTAINER;
}

AB_API int AB_SetEntryAttributesForPane(MSG_Pane * pane, MSG_ViewIndex index, AB_AttributeValue * valuesArray, uint16 numItems)
{
	AB_Pane * abPane = CastABPane(pane);
	if (abPane)
		return abPane->SetEntryAttributes(index, valuesArray, numItems);
	else
		return AB_INVALID_PANE;
}

AB_API int AB_FreeEntryAttributeValue(AB_AttributeValue * value)
{
	return AB_FreeEntryAttributeValues(value, 1);
}

AB_API int AB_FreeEntryAttributeValues(AB_AttributeValue * values, uint16 numItems)
{
	if (values)
	{
		for (int i = 0; i < numItems; i++)
		{
			if ( AB_IsStringEntryAttributeValue(&values[i]) && values[i].u.string)
				XP_FREEIF(values[i].u.string);  // we can have a string attribute & it doesn't have a value
		}
		XP_FREE(values);
	}
	return AB_SUCCESS;
}

AB_API XP_Bool AB_IsStringEntryAttributeValue(AB_AttributeValue * value)
{
	if (value)
	{
		if (value->attrib == AB_attribUseServer || value->attrib == AB_attribEntryID || 
			value->attrib == AB_attribEntryType || value->attrib == AB_attribHTMLMail ||
			value->attrib == AB_attribSecurity  || value->attrib == AB_attribWinCSID)
				return FALSE;
		else
			return TRUE;
	}
	return FALSE;
}

AB_API int AB_CopyEntryAttributeValue(AB_AttributeValue * srcValue, AB_AttributeValue * destValue)
{
	if (srcValue && destValue)
	{
		destValue->attrib = srcValue->attrib;
		switch(srcValue->attrib)  // maintenance note: add cases for any non-string attribute!!
		{
			case AB_attribEntryType:
				destValue->u.entryType = srcValue->u.entryType;
				break;
			case AB_attribHTMLMail:
				destValue->u.boolValue = srcValue->u.boolValue;
				break;
			case AB_attribSecurity:
			case AB_attribWinCSID:
			case AB_attribUseServer:
				destValue->u.shortValue = srcValue->u.shortValue;
				break;

			default:  // treat string (most common case) as the default
				if (srcValue->u.string && XP_STRLEN(srcValue->u.string) > 0)
					destValue->u.string = XP_STRDUP(srcValue->u.string);
				else
					destValue->u.string = XP_STRDUP("");
		}
		return AB_SUCCESS;
	}
	return AB_FAILURE;
}

/* FE asked for an attribute we don't have. This routine fills the attribute with an appropriate
	 default value for the attribut. I expect this to be called only on the XP side of the world */
AB_API int AB_CopyDefaultAttributeValue(AB_AttribID attrib, AB_AttributeValue * value /* already allocated */)
{
	if (value)
	{
		value->attrib = attrib;
		switch(attrib)
		{
		case AB_attribEntryType:
			value->u.entryType = AB_Person;
			break;
		case AB_attribHTMLMail:
			value->u.boolValue = FALSE;
			break;
		case AB_attribSecurity:
		case AB_attribWinCSID:
		case AB_attribUseServer:
			value->u.shortValue = 0;
			break;

		default:
			XP_ASSERT(AB_IsStringEntryAttributeValue(value));  // Everything else should be a string!
			value->u.string = XP_STRDUP("");
		}

		return AB_SUCCESS;
	}
	return AB_FAILURE;
}

/* caller must free array of values returned */
/* This function is used to generate a set of atributes which can be committed into the database for a naked address */
AB_API int AB_CreateAttributeValuesForNakedAddress(char * nakedAddress, AB_AttributeValue ** valueArray, uint16 * numItems)
{
	uint16 numAttributes = 4; // right now we store: entryType, full name, display name, email address
	int status = AB_SUCCESS;
	char * stringToUse;

	if (nakedAddress)
		stringToUse = nakedAddress;
	else
		stringToUse = "";

	AB_AttributeValue * values = (AB_AttributeValue *) XP_ALLOC(sizeof(AB_AttributeValue) * numAttributes);
	if (values)
	{
		values[0].attrib = AB_attribEntryType;
		values[0].u.entryType = AB_Person;
		
		values[1].attrib = AB_attribFullName;
		values[1].u.string = XP_STRDUP(stringToUse);

		values[2].attrib = AB_attribDisplayName;
		values[2].u.string = XP_STRDUP(stringToUse);
		
		values[3].attrib = AB_attribEmailAddress;
		values[3].u.string = XP_STRDUP(stringToUse);
	}
	else
		status = AB_OUT_OF_MEMORY;

	if (valueArray)
		*valueArray = values;

	if (numItems)
		*numItems = numAttributes;

	return status;
}

AB_API int AB_GetAttributesForNakedAddress(char * nakedAddress, AB_AttribID * attribs, AB_AttributeValue ** valuesArray,uint16 * numItems)
{
	uint16 numItemsAdded = 0;
	AB_AttributeValue * values = NULL;
	int status = AB_FAILURE;

	AB_AddressParser * parser = NULL; // used to produce the email attribute

	if (attribs && numItems && *numItems > 0)
	{
		uint16 numAttributes = *numItems; // right now we store: entryType, full name, display name, email address
		values = (AB_AttributeValue *) XP_ALLOC(sizeof(AB_AttributeValue) * numAttributes);
		if (values)
		{
			for (uint16 index = 0; index < numAttributes; index++)
			{
				if (nakedAddress)
				{
					values[index].attrib = attribs[index];

					switch (attribs[index])
					{
						// naked addresses only have a couple values that we acknowledge..
						case AB_attribEntryType:
							values[index].u.entryType = AB_Person;
							break;
						case AB_attribEmailAddress:
							values[index].u.string = NULL;
							AB_AddressParser::Create (NULL, &parser);
							if (parser)
							{
								parser->FormatNakedAddress(&values[index].u.string, nakedAddress,TRUE);
								parser->Release();
								parser = NULL;
							}

							if (values[index].u.string)  // if we actually have a value, break out of the looop...
								break;
							// we used to use the naked address name as the value for full name and display name.
							// However, this doesn't really make sense because the naked address should be treated
							// as an email address, NOT the full/disply name. This is most notable in the name completion
							// pane where we display a naked address.....
						default:
							AB_CopyDefaultAttributeValue(attribs[index], &values[index]);
					}
				}
				else
					AB_CopyDefaultAttributeValue(attribs[index], &values[index]);
			} // for loop

			status = AB_SUCCESS;
			numItemsAdded = numAttributes;
		}
		else
			status = AB_OUT_OF_MEMORY;
	}

	if (numItems)
		*numItems = numItemsAdded;
	if (valuesArray)
		*valuesArray = values;

	return status;
}


/****************************************************************************************

  Sorting - what attribute should we sort by. Are things sorted by first name or last name? 

*****************************************************************************************/

AB_API int AB_SortByAttribute(MSG_Pane * pane, AB_AttribID id, XP_Bool sortAscending)
{
	AB_Pane * abPane = CastABPane(pane);
	if (abPane)
		return abPane->SortByAttribute(id, sortAscending);
	else
		return AB_INVALID_PANE;
}

AB_API int AB_GetPaneSortedByAB2(MSG_Pane * pane, AB_AttribID * id)
{
	AB_Pane * abPane = CastABPane(pane);
	if (abPane)
		return abPane->GetPaneSortedBy(id);
	else
		return AB_INVALID_PANE;
}

/***************************************************************************************
 Mailing List Pane APIs
 ***************************************************************************************/

AB_API int AB_InitializeMailingListPaneAB2 (MSG_Pane * pane)
{
	AB_MailingListPane * mlPane = CastABMailingListPane(pane);
	if (mlPane) 
		return mlPane->Initialize();
	else
		return AB_INVALID_PANE;
}

AB_API AB_ContainerInfo * AB_GetContainerForMailingList(MSG_Pane * pane)
{
	AB_MailingListPane * mlPane = CastABMailingListPane(pane);
	if (mlPane)
		return mlPane->GetContainerForMailingList();
	else
		return NULL;
}

AB_API ABID AB_GetABIDForMailingListIndex(MSG_Pane * pane, const MSG_ViewIndex index)
{
	AB_MailingListPane * mlPane = CastABMailingListPane(pane);
	if (mlPane)
		return mlPane->GetABIDForIndex(index);
	else
		return 0;
}

/* Use these two APIs to add entries to the mailing list that are the result of name completion. */
AB_API int AB_AddNakedEntryToMailingList(MSG_Pane * pane,const MSG_ViewIndex index, const char * nakedAddress,XP_Bool ReplaceExisting)
{
	AB_MailingListPane * mlPane = CastABMailingListPane(pane);
	if (mlPane)
		return mlPane->AddEntry(index, nakedAddress, ReplaceExisting);
	else
		return AB_INVALID_PANE;
}

AB_API int AB_AddNCEntryToMailingList(MSG_Pane * pane, const MSG_ViewIndex index, AB_NameCompletionCookie * cookie, XP_Bool ReplaceExisting)
{
	AB_MailingListPane * mlPane = CastABMailingListPane(pane);
	if (mlPane)
		return mlPane->AddEntry(index, cookie, ReplaceExisting);
	else
		return AB_INVALID_PANE;
}

AB_API MSG_ViewIndex AB_GetMailingListIndexForABID(MSG_Pane * pane, ABID entryID)
{
	AB_MailingListPane * mlPane = CastABMailingListPane(pane);
	if (mlPane)
		return mlPane->GetIndexForABID(entryID);
	else
		return MSG_VIEWINDEXNONE;
}

AB_API int AB_SetMailingListEntryAttributes(MSG_Pane * pane, const MSG_ViewIndex index, AB_AttributeValue * values,
											uint16 numItems)
{
	AB_MailingListPane * mlPane = CastABMailingListPane(pane);
	if (mlPane)
		return mlPane->SetEntryAttributes(index, values, numItems);
	else
		return AB_INVALID_PANE;
}

AB_API int AB_GetMailingListEntryAttributes(MSG_Pane * pane, const MSG_ViewIndex index, AB_AttribID * attribs, 
											AB_AttributeValue ** values, uint16 * numItems)
{
	AB_MailingListPane * mlPane = CastABMailingListPane(pane);
	if (mlPane)
		return mlPane->GetEntryAttributes(index, attribs, values, numItems);
	else
		return AB_INVALID_PANE;
}

AB_API int AB_GetMailingListAttributes(MSG_Pane * pane, AB_AttribID * attribs, AB_AttributeValue ** values, 
									   uint16 * numItems)
{
	AB_MailingListPane * mlPane = CastABMailingListPane(pane);
	if (mlPane)
		return mlPane->GetAttributes(attribs, values, numItems);
	else
		return AB_INVALID_PANE;
}

AB_API int AB_SetMailingListAttributes(MSG_Pane * pane, AB_AttributeValue * valuesArray, uint16 numItems)
{
	AB_MailingListPane * mlPane = CastABMailingListPane(pane);
	if (mlPane)
		return mlPane->SetAttributes(valuesArray, numItems);
	else
		return AB_INVALID_PANE;
}

/******************************************************************************************
Person Entry Pane aka person property sheet pane. Person entry panes already respond to the 
AB_ClosePane API. 
*******************************************************************************************/

AB_API AB_ContainerInfo * AB_GetContainerForPerson(MSG_Pane * pane)
{
	AB_PersonPane * personPane = CastABPersonPane(pane);
	if (personPane)
		return personPane->GetContainerForPerson();
	else
		return NULL;
}


AB_API ABID AB_GetABIDForPerson(MSG_Pane * pane)
{
	AB_PersonPane * personPane = CastABPersonPane(pane);
	if (personPane)
		return personPane->GetABIDForPerson();
	else
		return 0;   // probably not a good return value. I should change this..but to what???
}

AB_API int AB_SetPersonEntryAttributes(MSG_Pane * pane, AB_AttributeValue * valuesArray, uint16 numItems)
{
	AB_PersonPane * personPane = CastABPersonPane(pane);
	if (personPane)
		return personPane->SetAttributes(valuesArray, numItems);
	else
		return AB_INVALID_PANE;
}

AB_API int AB_GetPersonEntryAttributes(MSG_Pane * pane, AB_AttribID * attribs, AB_AttributeValue ** values, uint16 * numItems)
{
	AB_PersonPane * personPane = CastABPersonPane(pane);
	if (personPane)
		return personPane->GetAttributes(attribs, values, numItems);
	else
		return AB_INVALID_PANE;
}

AB_API int AB_CommitChanges(MSG_Pane * pane)
{
	AB_PersonPane * personPane = CastABPersonPane(pane);
	if (personPane)
		return personPane->CommitChanges();
	else
	{
		AB_MailingListPane * mlPane = CastABMailingListPane(pane);
		if (mlPane)
			return mlPane->CommitChanges();
	}
	
	return AB_INVALID_PANE;
}

/* FEs should use this to obtain a property sheet pane representing the user's identity vcard...*/
AB_API int AB_GetIdentityPropertySheet(MWContext *context, MSG_Master * master, MSG_Pane ** pane)
{
	// first, create the vcard identity and convert to attribute values..
	AB_AttributeValue * identityValues = NULL;
	uint16 numAttributes = 0;
	char * identityVCard = NULL;
	int status = AB_SUCCESS;

	if (pane)
	{
		status = AB_LoadIdentityVCard(&identityVCard);
		if (identityVCard)
		{
			status = AB_ConvertVCardToAttribValues(identityVCard,&identityValues,&numAttributes);
			if (status == AB_SUCCESS) // so far so good...
				status = AB_PersonPane::CreateIdentityPane(context, master, identityValues, numAttributes, pane);

			AB_FreeEntryAttributeValues(identityValues, numAttributes);
			XP_FREE(identityVCard);
		}

	}

	return status;
}


/*****************************************************************************************
	Various APIs that don't fit into a category...

******************************************************************************************/

AB_API int AB_SplitFullName (const char *fullName, char **firstName, char **lastName, int16 csid)
{
	*firstName = XP_STRDUP (fullName);
	if (NULL == *firstName)
		return AB_OUT_OF_MEMORY;

	char *plastSpace = *firstName;
	char *walkName = *firstName;
	char *plastName = NULL;

    while (walkName && *walkName)
	{
        if (*walkName == ' ')
		{
            plastSpace = walkName;
			plastName = INTL_NextChar(csid, plastSpace);
        }
        walkName = INTL_NextChar(csid, walkName);
    }

	if (plastName) 
	{
		*plastSpace = '\0';
		*lastName = XP_STRDUP (plastName);
	}

	return AB_SUCCESS;
}

/* use the default display name API for generating a display name in the person property sheet card for new users */
AB_API int AB_GenerateDefaultDisplayName(const char * firstName, const char * lastName, char ** defaultDisplayName /* calller must free with XP_FREE */)
{
	char * displayName = NULL;
	XP_Bool lastFirst = FALSE; /* need to read this in from prefs... */
	
	PREF_GetBoolPref("mail.addr_book.lastnamefirst", &lastFirst);

	if (!lastFirst)
	{
		if (firstName)
		{
			NET_SACat(&displayName, firstName);
			if (lastName) /* if we are going to have a last name, add a space for it..*/
				NET_SACat(&displayName, XP_GetString(MK_ADDR_FIRST_LAST_SEP));
		}

		if (lastName)
			NET_SACat(&displayName, lastName);
	}
	else  /* should be treated as Last, First */
	{
		if (lastName)
		{
			NET_SACat(&displayName, lastName);
			if (firstName)
				NET_SACat(&displayName, ", ");
		}

		if (firstName)
			NET_SACat(&displayName, firstName);
	}

	if (defaultDisplayName)
		*defaultDisplayName = displayName;
	else
		XP_FREEIF(displayName);

	return AB_SUCCESS;
}

AB_API XP_List * AB_AcquireAddressBookContainers(MWContext * context)
/* creates a list of reference counted containers for all the address books. No LDAP. */
/* Caller must later release the list using AB_ReleaseContainerList */
{
	int i = 0;

	XP_List * abServers = XP_ListNew();
	XP_List * containers = XP_ListNew();
	XP_List * servers = DIR_GetDirServers();

	DIR_GetPersonalAddressBooks(servers, abServers);

	if (abServers)
	{
		for (i = 1; i <= XP_ListCount(abServers); i++)
		{
			DIR_Server * server = (DIR_Server *) XP_ListGetObjectNum(abServers, i);
			if (server)
			{
				AB_ContainerInfo * ctr = NULL;
				AB_ContainerInfo::Create(context, server, &ctr);
				if (ctr)
					XP_ListAddObjectToEnd(containers, ctr);
			}
		}
	}

	return containers;
}


AB_API int AB_ReleaseContainersList(XP_List * containers)
/* releases each container and destroys the list */
{
	int i = 0; 
	if (containers)
	{
		for (i = 1; i <= XP_ListCount(containers); i++)
		{
			AB_ContainerInfo * ctr = (AB_ContainerInfo *) XP_ListGetObjectNum (containers, i);
			if (ctr)
				ctr->Release();
		}

		XP_ListDestroy (containers);
	}

	return AB_SUCCESS;
}

AB_API int AB_AcquireContainer(AB_ContainerInfo * container)
{
	int status = AB_SUCCESS;
	if (container)
		container->Acquire();
	else
		status = ABError_NullPointer;

	return status;

}

AB_API int AB_ReleaseContainer(AB_ContainerInfo * container)
{
	int status = AB_SUCCESS;
	if (container)
		container->Release();
	else
		status = ABError_NullPointer;

	return status;
}

AB_API int AB_FreeAddressBookCopyInfo(AB_AddressBookCopyInfo * abCopyInfo)  /* use this to free copy info structs */
{
	int status = AB_SUCCESS;

	if (abCopyInfo)
	{
		if (abCopyInfo->destContainer)
			AB_ReleaseContainer(abCopyInfo->destContainer);
		if (abCopyInfo->srcContainer)
			AB_ReleaseContainer(abCopyInfo->srcContainer);
		if (abCopyInfo->idArray)
			XP_FREE(abCopyInfo->idArray);

		XP_FREE(abCopyInfo);
	}
	else
		status = ABError_NullPointer;

	return status;
}


/******************************************************************************************
 Replication APIs
*******************************************************************************************/
AB_API AB_ContainerInfo *AB_BeginReplication(MWContext *context, DIR_Server *server)
{
	return AB_LDAPContainerInfo::BeginReplication(context, server);
}

AB_API int AB_EndReplication(AB_ContainerInfo *container)
{
	AB_LDAPContainerInfo *ldapContainer = container ? container->GetLDAPContainerInfo() : NULL;
	if (ldapContainer)
	{
		ldapContainer->EndReplication();
		return AB_SUCCESS;
	}
	else
		return AB_FAILURE;
}

AB_API int AB_AddReplicaEntry(AB_ContainerInfo *container, char **valueList)
{
	AB_LDAPContainerInfo *ldapContainer = container ? container->GetLDAPContainerInfo() : NULL;
	if (ldapContainer && ldapContainer->AddReplicaEntry(valueList))
		return AB_SUCCESS;
	else 
		return AB_FAILURE;
}

AB_API int AB_DeleteReplicaEntry(AB_ContainerInfo *container, char *targetDn)
{
	AB_LDAPContainerInfo *ldapContainer = container ? container->GetLDAPContainerInfo() : NULL;
	if (ldapContainer)
	{
		ldapContainer->DeleteReplicaEntry(targetDn);
		return AB_SUCCESS;
	}
	else 
		return AB_FAILURE;
}

AB_API int AB_RemoveReplicaEntries(AB_ContainerInfo *container)
{
	AB_LDAPContainerInfo *ldapContainer = container ? container->GetLDAPContainerInfo() : NULL;
	if (ldapContainer)
	{
		ldapContainer->RemoveReplicaEntries();
		return AB_SUCCESS;
	}
	else 
		return AB_FAILURE;
}

AB_API int AB_GetNumReplicaAttributes(AB_ContainerInfo *container)
{
	AB_LDAPContainerInfo *ldapContainer = container ? container->GetLDAPContainerInfo() : NULL;
	if (ldapContainer)
	{
		AB_ReplicaAttributeMap *attribMap = ldapContainer->GetReplicaAttributeMap();
	
		if (attribMap)
			return attribMap->numAttribs;
	}

	return 0;
}

AB_API char **AB_GetReplicaAttributeNames(AB_ContainerInfo *container)
{
	AB_LDAPContainerInfo *ldapContainer = container ? container->GetLDAPContainerInfo() : NULL;
	if (ldapContainer)
	{
		AB_ReplicaAttributeMap *attribMap = ldapContainer->GetReplicaAttributeMap();
	
		if (attribMap)
			return attribMap->attribNames;
	}

	return NULL;
}

AB_API XP_Bool AB_ReplicaAttributeMatchesId(AB_ContainerInfo *container, int attribIndex, DIR_AttributeId id)
{
	AB_LDAPContainerInfo *ldapContainer = container ? container->GetLDAPContainerInfo() : NULL;
	if (ldapContainer)
	{
		AB_ReplicaAttributeMap *attribMap = ldapContainer->GetReplicaAttributeMap();
	
		if (attribMap && attribIndex < attribMap->numAttribs)
			return ldapContainer->ReplicaAttributeMatchesId(attribMap, attribIndex, id);
	}
	return FALSE;
}

