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
// Implementation of search API for mail and news
//
//
#include "msg.h"
#include "net.h"
#include "libi18n.h"
#include "xpgetstr.h"
#include "msgstrob.h"
#include "msgprefs.h"
#include "mailhdr.h"  
#include "newshdr.h"
#include "intl_csi.h"
#include "ldap.h"


extern "C" 
{
	extern int MK_OUT_OF_MEMORY;
	extern int MK_UNABLE_TO_OPEN_FILE;

	extern int XP_SEARCH_SUBJECT;
	extern int XP_SEARCH_CONTAINS;
	extern int XP_SEARCH_AGE;		// mscott: hack for searching by age...
	extern int XP_SEARCH_IS_GREATER;
	extern int XP_SEARCH_IS_LESS;	// for searching by age

	extern int XP_STATUS_READ;
	extern int XP_STATUS_REPLIED;
	extern int XP_STATUS_FORWARDED;
	extern int XP_STATUS_REPLIED_AND_FORWARDED;
	extern int XP_STATUS_NEW;

	extern int MK_MSG_SEARCH_STATUS;

	extern int MK_SEARCH_HITS_COUNTER;

	extern int MK_SEARCH_SCOPE_ONE;
	extern int MK_SEARCH_SCOPE_SELECTED;
	extern int MK_SEARCH_SCOPE_OFFLINE_MAIL;
	extern int MK_SEARCH_SCOPE_ONLINE_MAIL;	
	extern int MK_SEARCH_SCOPE_SUBSCRIBED_NEWS;
	extern int MK_SEARCH_SCOPE_ALL_NEWS;

	extern int XP_PRIORITY_NONE;
	extern int XP_PRIORITY_LOWEST;
	extern int XP_PRIORITY_LOW;
	extern int XP_PRIORITY_NORMAL;
	extern int XP_PRIORITY_HIGH;
	extern int XP_PRIORITY_HIGHEST;

	extern int XP_SEARCH_VALUE_REQUIRED;
}

#include "pmsgsrch.h" // private search API 
#include "msgmast.h"
#include "msgfinfo.h"
#include "xp_time.h"  // for XP_LocalZoneOffset
#include "dirprefs.h"
#include "msgmpane.h" // for LoadMessage
#include "newshost.h" // for QueryExtension
#include "hosttbl.h"  // for AddAllScopes
#include "msgurlq.h"  // for MSG_UrlQueue
#include "prefapi.h"

#include "msgimap.h"  // hopefully just temporary..

//-----------------------------------------------------------------------------
// ------------------------ Public API implementation -------------------------
//-----------------------------------------------------------------------------
SEARCH_API MSG_SearchError MSG_SearchAlloc (MSG_Pane *pane)
{
	// Make sure there isn't another allocated searchFrame in this context
	XP_ASSERT (pane);
	XP_ASSERT (NULL == pane->GetContext()->msg_searchFrame);

	// Make sure pane really is a linedPane
	MSG_PaneType type = pane->GetPaneType();
	XP_ASSERT (MSG_ADDRPANE == type || AB_ABPANE == type || MSG_SEARCHPANE == type || type == AB_PICKERPANE);

	MSG_SearchFrame *frame = new MSG_SearchFrame ((MSG_LinedPane*) pane);
	return frame ? SearchError_Success : SearchError_OutOfMemory;
}


SEARCH_API MSG_SearchError MSG_SearchFree (MSG_Pane *pane)
{
	MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (pane);
	if (NULL != frame)
	{
		delete frame;
		pane->GetContext()->msg_searchFrame = NULL;
		return SearchError_Success;
	}
	return SearchError_NullPointer;
}

SEARCH_API MSG_SearchError MSG_AddSearchTerm (MSG_Pane *pane, 
	MSG_SearchAttribute attrib, MSG_SearchOperator op, MSG_SearchValue *value, XP_Bool BooleanAND, char * arbitraryHeader)
{
	MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (pane);
	if (!frame)
		return SearchError_InvalidPane;
	return frame->AddSearchTerm (attrib, op, value, BooleanAND, arbitraryHeader);
}

MSG_SearchError MSG_CountSearchTerms (MSG_Pane *pane, int *numTerms)
{
	MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (pane);
	if (!frame)
		return SearchError_InvalidPane;
	if (!numTerms)
		return SearchError_NullPointer;
	*numTerms = frame->m_termList.GetSize();
	return SearchError_Success;
}

MSG_SearchError MSG_GetNthSearchTerm (MSG_Pane *pane, int whichTerm,
	MSG_SearchAttribute *attrib, MSG_SearchOperator *op, MSG_SearchValue *value)
{
	MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (pane);
	if (!frame)
		return SearchError_InvalidPane;
	if (!attrib || !op || !value)
		return SearchError_NullPointer;

	MSG_SearchTerm *term = frame->m_termList.GetAt(whichTerm);
	*attrib = term->m_attribute;
	*op = term->m_operator;
	return MSG_ResultElement::AssignValues (&term->m_value, value);
}


MSG_SearchError MSG_CountSearchScopes (MSG_Pane *pane, int *numScopes)
{
	MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (pane);
	if (!frame)
		return SearchError_InvalidPane;
	if (!numScopes)
		return SearchError_NullPointer;

	*numScopes = frame->m_scopeList.GetSize();
	return SearchError_Success;
}


MSG_SearchError MSG_GetNthSearchScope (MSG_Pane *pane, int which, MSG_ScopeAttribute *scopeId, void **scope)
{
	MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (pane);
	if (!frame)
		return SearchError_InvalidPane;
	if (!scopeId || !scope)
		return SearchError_NullPointer;

	MSG_ScopeTerm *scopeTerm = frame->m_scopeList.GetAt(which);
	*scopeId = scopeTerm->m_attribute;
	if (*scopeId == scopeLdapDirectory)
		*scope = scopeTerm->m_server;
	else 
		*scope = scopeTerm->m_folder;
	return SearchError_Success;
}


SEARCH_API MSG_SearchError MSG_AddScopeTerm (MSG_Pane *pane, MSG_ScopeAttribute attrib, MSG_FolderInfo *folder)
{
	MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (pane);
	if (!frame)
		return SearchError_InvalidPane;
	
	// do not do a deep search starting from the IMAP INBOX
	XP_Bool deep = !((folder->GetType() == FOLDER_IMAPMAIL) && (folder->GetFlags() & MSG_FOLDER_FLAG_INBOX));
	return frame->AddScopeTree (attrib, folder, deep);
}


SEARCH_API MSG_SearchError MSG_AddAllScopes (MSG_Pane *pane, MSG_Master *master,	MSG_ScopeAttribute scope)
{
	XP_ASSERT (master);

	MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (pane);
	if (!frame)
		return SearchError_InvalidPane;
	if (!master)
		return SearchError_NullPointer;
	return frame->AddAllScopes (master, scope);
}


SEARCH_API MSG_SearchError MSG_AddLdapScope (MSG_Pane *pane, DIR_Server *server)
{
	XP_ASSERT (server);

	MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (pane);
	if (!frame)
		return SearchError_InvalidPane;
	return frame->AddScopeTerm (server);
}


SEARCH_API MSG_SearchError MSG_AddAllLdapScopes (MSG_Pane *pane, XP_List *dirServerList)
{
	// This is a convenience function which doesn't do anything the FE couldn't do themselves
	XP_ASSERT (dirServerList);
	XP_ASSERT (!XP_ListIsEmpty(dirServerList));

	MSG_SearchError err = SearchError_Success;
	DIR_Server *server;
	for (int i = 1; i < XP_ListCount(dirServerList); i++)
	{
		server = (DIR_Server*) XP_ListGetObjectNum (dirServerList, i);
		err = MSG_AddLdapScope (pane, server);
	}
	return err;
}


SEARCH_API MSG_SearchError MSG_Search (MSG_Pane *pane)
{
	MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (pane);
	if (!frame)
		return SearchError_InvalidPane;

	MSG_SearchError err = frame->Initialize ();
	if (SearchError_Success == err)
		err = frame->BeginSearching ();
	else
	{
		if (SearchError_ValueRequired == err)
			FE_Alert (pane->GetContext(), XP_GetString (XP_SEARCH_VALUE_REQUIRED));
	}
	return err;
}

SEARCH_API MSG_SearchError MSG_InterruptSearchViaPane (MSG_Pane *pane)
{
	MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (pane);
	if (!frame)
		return SearchError_InvalidPane;

	if (frame->m_handlingError)
		return SearchError_Busy;

	if (pane)
		pane->InterruptContext (TRUE);

	return SearchError_Success;
}


SEARCH_API MSG_SearchError MSG_SetSearchParam (MSG_Pane *pane, MSG_SearchType type, void *param)
{
	MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (pane);
	if (!frame)
		return SearchError_InvalidPane;

    return frame->SetSearchParam(type, param);
}

SEARCH_API void *MSG_GetSearchParam (MSG_Pane *pane)
{
	MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (pane);
	if (frame)
		return frame->GetSearchParam();

	return NULL;
}

SEARCH_API MSG_SearchType MSG_GetSearchType (MSG_Pane *pane)
{
	MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (pane);
	if (frame)
		return frame->GetSearchType();

	return searchNone;
}

SEARCH_API uint32 MSG_GetNumResults (MSG_Pane *pane)
{
	MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (pane);
	if (frame)
		return frame->m_resultList.GetSize();

	return 0;
}

SEARCH_API MSG_SearchError MSG_GetResultElement (MSG_Pane *pane, MSG_ViewIndex idx, MSG_ResultElement **result)
{
	XP_ASSERT(pane);
	XP_ASSERT(result);

	MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (pane);
	if (!frame)
		return SearchError_InvalidPane;
	return frame->GetResultElement (idx, result);
}


SEARCH_API MSG_SearchError MSG_GetResultAttribute (
	MSG_ResultElement *elem, MSG_SearchAttribute attr, MSG_SearchValue **value)     
{
	XP_ASSERT(elem->IsValid());
	if (attribLocation == attr)
		return elem->GetPrettyName (value);
	return elem->GetValue (attr, value);
}

SEARCH_API MSG_SearchError MSG_DestroySearchValue (MSG_SearchValue *value)
{
	XP_ASSERT(value);
	return MSG_ResultElement::DestroyValue (value);
}


SEARCH_API MSG_SearchError MSG_OpenResultElement (MSG_ResultElement *elem, void *window)
{
	XP_ASSERT(elem && elem->IsValid());
	return elem->Open (window);
}


SEARCH_API MWContextType MSG_GetResultElementType (MSG_ResultElement *elem)
{
	XP_ASSERT(elem->IsValid());
	return elem->GetContextType ();
}


SEARCH_API MWContext *MSG_IsResultElementOpen (MSG_ResultElement * /*elem*/)
{
	return NULL; //###phil hack for now
}


SEARCH_API MSG_SearchError MSG_ModifyLdapResult (MSG_ResultElement *elem, MSG_SearchValue *val)
{
	XP_ASSERT(elem && elem->IsValid());
	XP_ASSERT(val);
	XP_ASSERT(elem->m_adapter);
	return elem->m_adapter->ModifyResultElement (elem, val);
}


SEARCH_API MSG_SearchError MSG_AddLdapResultsToAddressBook(MSG_Pane *pane, MSG_ViewIndex *indices, int count)
{
	XP_ASSERT (pane);
	XP_ASSERT (indices);
	MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (pane);
	if (!frame)
		return SearchError_InvalidPane;
	return frame->AddLdapResultsToAddressBook (indices, count);
}


SEARCH_API MSG_SearchError MSG_ComposeFromLdapResults(MSG_Pane *pane, MSG_ViewIndex *indices, int count)
{
	XP_ASSERT (pane);
	XP_ASSERT (indices);
	MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (pane);
	if (!frame)
		return SearchError_InvalidPane;
	return frame->ComposeFromLdapResults (indices, count);
}


SEARCH_API MSG_SearchError MSG_SortResultList (MSG_Pane *pane, MSG_SearchAttribute attrib, XP_Bool descending)
{
	MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (pane);
	if (!frame)
		return SearchError_InvalidPane;
	return frame->SortResultList (attrib, descending);
}


SEARCH_API XP_Bool MSG_GoToFolderStatus (MSG_Pane *pane, MSG_ViewIndex *indices, int32 numIndices)
{
	MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (pane);
	if (!frame)
		return SearchError_InvalidPane;
	return frame->GetGoToFolderStatus (indices, numIndices);
}


typedef struct
{
	MSG_SearchAttribute	attrib;
	const char			*attribName;
} SearchAttribEntry;

SearchAttribEntry SearchAttribEntryTable[] =
{
    {attribSubject,		"subject"},
    {attribSender,		"from"},
    {attribBody,		"body"},
    {attribDate,		"date"},
    {attribPriority,	"priority"},
    {attribMsgStatus,	"status"},	
    {attribTo,			"to"},
    {attribCC,			"CC"},
    {attribToOrCC,		"to or CC"}
};

#if defined(XP_MAC) && defined (__MWERKS__)
#pragma require_prototypes off
#endif

// Take a string which starts off with an attribute
// return the matching attribute. If the string is not in the table, then we can conclude that it is an arbitrary header
SEARCH_API MSG_SearchError MSG_GetAttributeFromString(const char *string, int16 *attrib)
{
	if (NULL == string || NULL == attrib)
		return SearchError_NullPointer;
	XP_Bool found = FALSE;
	for (int idxAttrib = 0; idxAttrib < sizeof(SearchAttribEntryTable) / sizeof(SearchAttribEntry); idxAttrib++)
	{
		if (!XP_STRCASECMP(string, SearchAttribEntryTable[idxAttrib].attribName))
		{
			found = TRUE;
			*attrib = SearchAttribEntryTable[idxAttrib].attrib;
			break;
		}
	}
	if (!found)
		*attrib = attribOtherHeader; // assume arbitrary header if we could not find the header in the table
	return SearchError_Success;      // we always succeed now
}

SEARCH_API MSG_SearchError MSG_GetStringForAttribute(int16 attrib, const char **string)
{
	if (NULL == string)
		return SearchError_NullPointer;
	XP_Bool found = FALSE;
	for (int idxAttrib = 0; idxAttrib < sizeof(SearchAttribEntryTable) / sizeof(SearchAttribEntry); idxAttrib++)
	{
		// I'm using the idx's as aliases into MSG_SearchAttribute and 
		// MSG_SearchOperator enums which is legal because of the way the
		// enums are defined (starts at 0, numItems at end)
		if (attrib == SearchAttribEntryTable[idxAttrib].attrib)
		{
			found = TRUE;
			*string = SearchAttribEntryTable[idxAttrib].attribName;
			break;
		}
	}
	// we no longer return invalid attribute. If we cannot find the string in the table, 
	// then it is an arbitrary header. Return success regardless if found or not
//	return (found) ? SearchError_Success : SearchError_InvalidAttribute;
	return SearchError_Success;
}

typedef struct
{
	MSG_SearchOperator	op;
	const char			*opName;
} SearchOperatorEntry;

SearchOperatorEntry SearchOperatorEntryTable[] =
{
	{opContains,	"contains"},
    {opDoesntContain,"doesn't contain"},
    {opIs,           "is"},
    {opIsnt,		"isn't"},
    {opIsEmpty,		"is empty"},
	{opIsBefore,    "is before"},
    {opIsAfter,		"is after"},
    {opIsHigherThan, "is higher than"},
    {opIsLowerThan,	"is lower than"},
    {opBeginsWith,  "begins with"},
	{opEndsWith,	"ends with"}
};

SEARCH_API MSG_SearchError MSG_GetOperatorFromString(const char *string, int16 *op)
{
	if (NULL == string || NULL == op)
		return SearchError_NullPointer;
	
	XP_Bool found = FALSE;
	for (int idxOp = 0; idxOp < sizeof(SearchOperatorEntryTable) / sizeof(SearchOperatorEntry); idxOp++)
	{
		// I'm using the idx's as aliases into MSG_SearchAttribute and 
		// MSG_SearchOperator enums which is legal because of the way the
		// enums are defined (starts at 0, numItems at end)
		if (!XP_STRCASECMP(string, SearchOperatorEntryTable[idxOp].opName))
		{
			found = TRUE;
			*op = SearchOperatorEntryTable[idxOp].op;
			break;
		}
	}
	return (found) ? SearchError_Success : SearchError_InvalidOperator;
}

SEARCH_API MSG_SearchError MSG_GetStringForOperator(int16 op, const char **string)
{
	if (NULL == string)
		return SearchError_NullPointer;
	XP_Bool found = FALSE;
	for (int idxOp = 0; idxOp < sizeof(SearchOperatorEntryTable) / sizeof(SearchOperatorEntry); idxOp++)
	{
		// I'm using the idx's as aliases into MSG_SearchAttribute and 
		// MSG_SearchOperator enums which is legal because of the way the
		// enums are defined (starts at 0, numItems at end)
		if (op == SearchOperatorEntryTable[idxOp].op)
		{
			found = TRUE;
			*string = SearchOperatorEntryTable[idxOp].opName;
			break;
		}
	}

	return (found) ? SearchError_Success : SearchError_InvalidOperator;
}

static MSG_SearchError msg_GetValidityTableForFolders (MSG_Master *master, MSG_FolderInfo **folderArray, uint16 /* numFolders */, msg_SearchValidityTable **table, XP_Bool forFilters)
{
	MSG_SearchError err = SearchError_Success;

	MSG_FolderInfo *folder = folderArray[0]; 

	// determine if the user wants us to search locally or search the server by checking the pref...
	// remember: this pref onlines applies for online imap and online news folders...(filters are left alone)
	XP_Bool searchServer = master->GetPrefs()->GetSearchServer();  
	searchServer = forFilters ? TRUE : searchServer;  // small hack to simplify the if statements...


	if (folder->GetMailFolderInfo()) 
	{
		if (folder->GetType() == FOLDER_IMAPMAIL && searchServer)
		{
			if (forFilters)
				err = gValidityMgr.GetTable (msg_SearchValidityManager::onlineMailFilter, table);
			else
				err = gValidityMgr.GetTable (msg_SearchValidityManager::onlineMail, table);
		}
		else
			err = gValidityMgr.GetTable (msg_SearchValidityManager::offlineMail, table);
	}
	else if (folder->IsNews())
	{
		MSG_FolderInfoNews *newsFolder = (MSG_FolderInfoNews*) folder;
		if (!searchServer || NET_IsOffline())
			err = gValidityMgr.GetTable(msg_SearchValidityManager::localNews, table);
		else
		{
			if (newsFolder->KnowsSearchNntpExtension())
			{
				err = gValidityMgr.GetTable (msg_SearchValidityManager::newsEx, table);
				if (SearchError_Success == err)
					err = gValidityMgr.PostProcessValidityTable (newsFolder->GetHost());
			}
			else
				err = gValidityMgr.GetTable (msg_SearchValidityManager::news, table);
		}

	}
	else if (FOLDER_CONTAINERONLY == folder->GetType())
	{
		if (!searchServer || NET_IsOffline())
			err = gValidityMgr.GetTable (msg_SearchValidityManager::localNews, table);
		else
		{
		if (folder->KnowsSearchNntpExtension())
		{
			MSG_NewsHost *host = master->GetHostTable()->FindNewsHost(folder);
			err = gValidityMgr.GetTable (msg_SearchValidityManager::newsEx, table);
			if (SearchError_Success == err)
				err = gValidityMgr.PostProcessValidityTable (host);
		}
		else
			err = gValidityMgr.GetTable (msg_SearchValidityManager::news, table);
		}
	}
	else if (FOLDER_IMAPSERVERCONTAINER == folder->GetType())
	{
		if (forFilters)
			err = gValidityMgr.GetTable (msg_SearchValidityManager::onlineMailFilter, table);
		else
		{
			if (searchServer)
				err = gValidityMgr.GetTable (msg_SearchValidityManager::onlineMail, table);
			else
				err = gValidityMgr.GetTable (msg_SearchValidityManager::offlineMail, table);
		}
	}
	else
		err = SearchError_InvalidScope;

	return err;
}


static MSG_SearchError msg_GetNumAttributesForScopes (
	MSG_Master *master,
	MSG_ScopeAttribute scope,
	void ** array,
	uint16 numFolders,
	uint16 *numItems,
	XP_Bool forFilters)
{
	if (array == NULL)
		return SearchError_NullPointer;

	MSG_SearchError err = SearchError_Success;
	msg_SearchValidityTable *table = NULL;

	DIR_Server *server = NULL;
	MSG_FolderInfo **folderArray = NULL;

	if (scope != scopeLdapDirectory)
	{
		folderArray = (MSG_FolderInfo**) array;
		err = msg_GetValidityTableForFolders (master, folderArray, numFolders, &table, forFilters);
	}
	else
	{
		server = *(DIR_Server**) array;
		XP_ASSERT(numFolders == 1);
		if (numFolders > 1)
			return SearchError_ScopeAgreement;
		err = gValidityMgr.GetTable (msg_SearchValidityManager::Ldap, &table);
		if (SearchError_Success == err)
			gValidityMgr.PostProcessValidityTable (server);
	}

	*numItems = 0;
	if (SearchError_Success == err)
		*numItems = table->GetNumAvailAttribs() + master->GetPrefs()->GetNumCustomHeaders();

	return err;
}

static MSG_SearchError msg_GetAttributesForScopes (
	MSG_Master *master,
	MSG_ScopeAttribute scope,
    void **array,           /* return available attribs in this scope  */
	uint16 numFolders,            /* number of folders in the array          */
    MSG_SearchMenuItem *items,    /* array of caller-allocated structs       */
    uint16 *maxItems,             /* in- max array size; out- num returned   */ 
	XP_Bool forFilters)
{
	if (NULL == items || NULL == maxItems || NULL == array)
		return SearchError_NullPointer;

	MSG_SearchError err = SearchError_Success;
	int cnt = 0;
	msg_SearchValidityTable *table = NULL;

	DIR_Server *server = NULL;
	MSG_FolderInfo **folderArray = NULL;

	if (scope != scopeLdapDirectory)
	{
		folderArray = (MSG_FolderInfo**) array;
		err = msg_GetValidityTableForFolders (master, folderArray, numFolders, &table, forFilters);
	}
	else
	{
		server = *(DIR_Server**) array;
		XP_ASSERT(numFolders == 1);
		if (numFolders > 1)
			return SearchError_ScopeAgreement;
		err = gValidityMgr.GetTable (msg_SearchValidityManager::Ldap, &table);
		if (SearchError_Success == err)
			gValidityMgr.PostProcessValidityTable (server);
	}

	if (SearchError_Success == err)
	{
		// For each attribute, look through its operator array until we find one which
		// is available. Since only one available operator tells us that the attrib should
		// be included in the list, we can stop there.
		XP_Bool found;
		int numArbHdrs = master->GetPrefs()->GetNumCustomHeaders();
		uint16 numItems = table->GetNumAvailAttribs() + numArbHdrs;
		
		if (numItems > *maxItems)
		{
			XP_ASSERT(0);  // caller did not allocate enough menu items!! 
			numItems = *maxItems; // truncate # of options
		}

		uint16 arbHdrCnt = 0;
		int idxAttrib = 0;

		// loops while we have attributes we have not checked for and while we have
		// space in the array of items to fit these new attributes....
		while (idxAttrib < kNumAttributes && cnt < numItems)
		{
			found = FALSE;  
			for (int idxOp = 0; idxOp < kNumOperators && !found; idxOp++)
				if (table->GetAvailable (idxAttrib, idxOp))
				{
					// I'm using the idx's as aliases into MSG_SearchAttribute and 
					// MSG_SearchOperator enums which is legal because of the way the
					// enums are defined (starts at 0, numItems at end)
					items[cnt].attrib = idxAttrib;
					items[cnt].isEnabled = table->GetEnabled (idxAttrib, idxOp);
					if (scope != scopeLdapDirectory)
					{
						char * name;
						if (idxAttrib == attribOtherHeader && numArbHdrs) // do we have any arbitrary headers?
						{
							name = master->GetPrefs()->GetNthCustomHeader(arbHdrCnt);
							arbHdrCnt++;   // our offset for custom header prefs
							numArbHdrs--;  // we just processed a custom hdr
							XP_STRNCPY_SAFE (items[cnt].name, name, sizeof(items[cnt].name));
							XP_FREEIF(name);  // free memory allocated by prefs...
							cnt++;
							found = TRUE;
							break;
						}
						else
							if (idxAttrib != attribOtherHeader)  
						{
							// ##mscott: hack...attribAgeInDays is not in the proper order for the resource file...
							if (idxAttrib == attribAgeInDays)   // not indexed properly in the resource file
								name = XP_GetString(XP_SEARCH_AGE);
							else
								name = XP_GetString(idxAttrib + XP_SEARCH_SUBJECT);
							XP_STRNCPY_SAFE (items[cnt].name, name, sizeof(items[cnt].name));
							cnt++;
							found = TRUE;
							break;
						}
					}
					else
					{
						DIR_AttributeId id;
						if (SearchError_Success == MSG_SearchAttribToDirAttrib ((MSG_SearchAttribute) idxAttrib, &id))
						{
							const char *name = DIR_GetAttributeName (server, id);
							XP_STRNCPY_SAFE (items[cnt].name, name, sizeof(items[cnt].name));

						}
						else
							items[cnt].name[0] = '\0';
						cnt++;
						found = TRUE;
						break;
					}
				}
			// don't increment idxAtrib if it is an attribOtherHeader and we still have a arbitrary hdrs that
			// need processing. If we just looped through all the ops and did not find an available op for the
			// attrib, then we can also increment to the next attibute
			if (idxAttrib != attribOtherHeader || !numArbHdrs || !found)  // don't increment if we still have more arbitrary hdrs to process
				idxAttrib++;
		}
	}
	*maxItems = cnt; // final number of attributes we added ot the items array
	return err;
}


SEARCH_API XP_Bool MSG_ScopeUsesCustomHeaders(
	MSG_Master * master,		
	MSG_ScopeAttribute scope, 
	void * selection, /* could be a folder or LDAP server. Determined by the scope */
	XP_Bool forFilters) 		   /* is this a filter we are talking about? */
{
	MSG_FolderInfo * folder = NULL;

	if (scope != scopeLdapDirectory)
	{
		folder = (MSG_FolderInfo *) selection; // safe cast because we used scope to determine it was a folderInfo

		// determine if the user wants us to search locally or search the server by checking the pref...
		// remember: this pref onlines applies for online imap and online news folders...(filters are left alone)
		XP_Bool searchServer = master->GetPrefs()->GetSearchServer();  
		searchServer = forFilters ? TRUE : searchServer;  // small hack to simplify the if statements...
		if (folder)
		{
			if (folder->TestFlag(MSG_FOLDER_FLAG_NEWSGROUP) || folder->TestFlag(MSG_FOLDER_FLAG_NEWS_HOST))
			{
				// if we are searching the server && are not in offline mode, then we need to 
				// ask the server if it supports news extentions. If it does, we support custom headers,
				// otherwise we don't. 
				if (searchServer && !NET_IsOffline() && !folder->KnowsSearchNntpExtension())
					return FALSE;
			}
			return TRUE;
		}
	}
	return FALSE;  // currently LDAP doesn't support custom headers. ##mscott
}

SEARCH_API XP_Bool MSG_IsStringAttribute(
	MSG_SearchAttribute attrib)
{
	return IsStringAttribute(attrib);
}

SEARCH_API MSG_SearchError MSG_GetNumAttributesForFilterScopes (
	MSG_Master *master,
	MSG_ScopeAttribute scope,
	void ** array,				  /* return available attribs in this scope			 */
	uint16 numFolders,			  /* number of folders in the array					 */
	uint16 *numItems)			  /* out - number of attributes for the filter scope */
{
	return msg_GetNumAttributesForScopes (master, scope, array, numFolders, numItems, TRUE);
}

SEARCH_API MSG_SearchError MSG_GetAttributesForFilterScopes (
	MSG_Master *master,
	MSG_ScopeAttribute scope,
    void **array,				  /* return available attribs in this scope  */
	uint16 numFolders,            /* number of folders in the array          */
    MSG_SearchMenuItem *items,    /* array of caller-allocated structs       */
    uint16 *maxItems)             /* in- max array size; out- num returned   */ 
{
	return msg_GetAttributesForScopes (master, scope, array, numFolders, items, maxItems, TRUE);
}

SEARCH_API MSG_SearchError MSG_GetNumAttributesForSearchScopes (
	MSG_Master *master,
	MSG_ScopeAttribute scope,
	void ** array,				/* return available attribs in this scope	*/
	uint16 numFolders,			/* number of folders in the array			*/
	uint16 *numItems)			/* out - number of attributes for the scope */
{
	return msg_GetNumAttributesForScopes (master, scope, array, numFolders, numItems, FALSE);
}

SEARCH_API MSG_SearchError MSG_GetAttributesForSearchScopes (
	MSG_Master *master,
	MSG_ScopeAttribute scope,
    void **array,				  /* return available attribs in this scope  */
	uint16 numFolders,            /* number of folders in the array          */
    MSG_SearchMenuItem *items,    /* array of caller-allocated structs       */
    uint16 *maxItems)             /* in- max array size; out- num returned   */ 
{
	return msg_GetAttributesForScopes (master, scope, array, numFolders, items, maxItems, FALSE);
}


static MSG_SearchError msg_GetOperatorsForScopes (
    MSG_Master *master, 
	MSG_ScopeAttribute scope,
	void **array, 
	uint16 numFolders,
	MSG_SearchAttribute attrib,
    MSG_SearchMenuItem *items,    
    uint16 *maxItems,
	XP_Bool forFilters)             
{
	if (NULL == items || NULL == maxItems || NULL == array)
		return SearchError_NullPointer;

	int cnt = 0;
	msg_SearchValidityTable *table = NULL;
	MSG_SearchError err = SearchError_Success;

	MSG_FolderInfo **folderArray = NULL;
	DIR_Server *server = NULL;

	if (scope != scopeLdapDirectory)
	{
		folderArray = (MSG_FolderInfo **) array;
		err = msg_GetValidityTableForFolders (master, folderArray, numFolders, &table, forFilters);
	}
	else
	{
		server = *(DIR_Server**) array;
		err = gValidityMgr.GetTable (msg_SearchValidityManager::Ldap, &table);
	}

	// We know which attrib we're interested in, so we only need one 'for' loop
	// to look through the operators which match. We need to know all of the
	// matching operators, not just the first (as above)

	if (SearchError_Success == err)
	{
		for (int idxOp = 0; idxOp < kNumOperators && cnt < *maxItems; idxOp++)
			if (table->GetAvailable (attrib, idxOp))
			{
				// If we're talking about an LDAP server which can't do efficient
				// wildcard matching, then don't show the operators which generate that syntax
				if (scope == scopeLdapDirectory && (idxOp == opContains || idxOp == opDoesntContain) && !server->efficientWildcards)
					continue;

				char * name;
				// ## mscott: hack because is greater than and is less than are not in the proper order in the resource file...
				if (idxOp == opIsGreaterThan)
					name = XP_GetString(XP_SEARCH_IS_GREATER);
				else
					if (idxOp == opIsLessThan)
					name = XP_GetString(XP_SEARCH_IS_LESS);
				else
					name = XP_GetString(idxOp + XP_SEARCH_CONTAINS);
	
				// This uses idxOp as an alias into MSG_SearchOperator
				items[cnt].attrib = idxOp;
				items[cnt].isEnabled = table->GetEnabled (attrib, idxOp);
//				char *name = XP_GetString (idxOp + XP_SEARCH_CONTAINS);
				XP_STRNCPY_SAFE (items[cnt].name, name, sizeof(items[cnt].name));
				cnt++;
			}
	}

	*maxItems = cnt;
	return err;
}


SEARCH_API MSG_SearchError MSG_GetOperatorsForFilterScopes (
    MSG_Master *master, 
	MSG_ScopeAttribute scope,
	void **array, 
	uint16 numFolders,
	MSG_SearchAttribute attrib,
    MSG_SearchMenuItem *items,    
    uint16 *maxItems)             
{
	return msg_GetOperatorsForScopes (master, scope, array, numFolders, attrib, items, maxItems, TRUE);
}


SEARCH_API MSG_SearchError MSG_GetOperatorsForSearchScopes (
    MSG_Master *master, 
	MSG_ScopeAttribute scope,
	void **array, 
	uint16 numFolders,
	MSG_SearchAttribute attrib,
    MSG_SearchMenuItem *items,    
    uint16 *maxItems)             
{
	return msg_GetOperatorsForScopes (master, scope, array, numFolders, attrib, items, maxItems, FALSE);
}


static XP_Bool msg_FoldersAreHomogeneous (MSG_FolderInfo **folders, uint16 folderCount)
{
	FolderType firstType=(FolderType)0;
	XP_Bool firstIsRFC977bis = FALSE;
	for (int i = 0; i < folderCount; i++)
	{
		FolderType type = folders[i]->GetType();
		XP_Bool isRFC977bis = folders[i]->KnowsSearchNntpExtension();
		if (i == 0)
		{
			firstType = type;
			firstIsRFC977bis = isRFC977bis;
		}
		else
		{
			if (type != firstType)
				return FALSE;
			if (isRFC977bis != firstIsRFC977bis)
				return FALSE;
		}
	}
	return TRUE;
}


SEARCH_API MSG_SearchError MSG_GetScopeMenuForSearchMessages (
	MSG_Master *master,
	MSG_FolderInfo **selArray,
	uint16 selCount,
	MSG_SearchMenuItem *items,
	uint16 *maxItems)
{
	if (!master || !selArray || !items || !maxItems)
	{
		*maxItems = 0;
		return SearchError_NullPointer;
	}

	if (*maxItems < 6)
	{
		*maxItems = 0;
		return SearchError_ListTooSmall;
	}

	int idx = 0;
	if (selCount == 1)
	{
		items[idx].attrib = -1;  //###phil hacky way to tell the FE that "selected" is enabled
		PR_snprintf (items[idx++].name, kSearchMenuLength, XP_GetString(MK_SEARCH_SCOPE_ONE), selArray[0]->GetPrettiestName());
	}
	else if (msg_FoldersAreHomogeneous (selArray, selCount))
	{
		items[idx].attrib = -1;  //###phil hacky way to tell the FE that "selected" is enabled
		XP_STRNCPY_SAFE (items[idx++].name, XP_GetString(MK_SEARCH_SCOPE_SELECTED), kSearchMenuLength );
	}

	if (master->GetPrefs()->GetMailServerIsIMAP4())
	{
		items[idx].attrib = scopeMailFolder;
		XP_STRNCPY_SAFE (items[idx++].name, XP_GetString(MK_SEARCH_SCOPE_ONLINE_MAIL), kSearchMenuLength);
	}
	
	items[idx].attrib = scopeMailFolder;
	XP_STRNCPY_SAFE (items[idx++].name, XP_GetString(MK_SEARCH_SCOPE_OFFLINE_MAIL), kSearchMenuLength);

	items[idx].attrib = scopeNewsgroup;
	XP_STRNCPY_SAFE (items[idx++].name, XP_GetString(MK_SEARCH_SCOPE_SUBSCRIBED_NEWS), kSearchMenuLength);

	int i;
	XP_Bool allAreRFC977bis = TRUE;
	for (i = 0; i < selCount && allAreRFC977bis; i++)
		allAreRFC977bis = selArray[i]->KnowsSearchNntpExtension();
	if (allAreRFC977bis)
	{
		items[idx].attrib = scopeAllSearchableGroups;
		XP_STRNCPY_SAFE (items[idx++].name, XP_GetString(MK_SEARCH_SCOPE_ALL_NEWS), kSearchMenuLength);
	}

	*maxItems = idx;

	for (i = 0; i < idx; i++)
		items[i].isEnabled = TRUE;

	return SearchError_Success;
}

static MSG_SearchAttribute msg_DirAttribToSearchAttrib (DIR_AttributeId id)
{
	switch (id)
	{
		case cn: return attribCommonName; 
		case mail : return attrib822Address;
		case telephonenumber : return attribPhoneNumber;
		case o: return attribOrganization;
		case ou: return attribOrgUnit;
		case l: return attribLocality;
		case street : return attribStreetAddress;
		case givenname :return attribGivenName;
		case sn: return attribSurname;
		case custom1: return attribCustom1;
		case custom2: return attribCustom2; 
		case custom3: return attribCustom3;
		case custom4: return attribCustom4;
		case custom5: return attribCustom5;
		default:
			XP_ASSERT(FALSE);
	}
	return attribCommonName; // shouldn't get here
}


SEARCH_API MSG_SearchError MSG_GetBasicLdapSearchAttributes (DIR_Server *server, MSG_SearchMenuItem *items, int *maxItems)
{
	XP_ASSERT(server && items && maxItems);
	if (!server || !items || !maxItems)
		return SearchError_NullPointer;

	DIR_AttributeId defaults[4] = {cn, mail, o, ou};
	MSG_SearchAttribute attrib; 
	const char *attribName = NULL;

	for (int i = 0; i < *maxItems; i++)
	{
		if (i < server->basicSearchAttributesCount)
		{
			attrib = msg_DirAttribToSearchAttrib (server->basicSearchAttributes[i]);
			attribName = DIR_GetAttributeName (server, server->basicSearchAttributes[i]);
		}
		else
		{
			attrib = msg_DirAttribToSearchAttrib (defaults[i]);
			attribName = DIR_GetAttributeName (server, defaults[i]);
		}

		items[i].attrib = attrib;
		items[i].isEnabled = TRUE; //probably pointless these days?
		XP_ASSERT(attribName);
		if (attribName)
			XP_STRNCPY_SAFE (items[i].name, attribName, kSearchMenuLength);
	}

	return SearchError_Success;
}


SEARCH_API MSG_SearchError MSG_SearchAttribToDirAttrib (MSG_SearchAttribute attrib, DIR_AttributeId *id)
{
	switch (attrib)
	{
		case attribCommonName: *id = cn; break;
		case attrib822Address : *id = mail; break;
		case attribPhoneNumber : *id = telephonenumber; break;
		case attribOrganization: *id = o; break;
		case attribOrgUnit: *id = ou; break;
		case attribLocality: *id = l; break;
		case attribStreetAddress : *id = street; break;
		case attribGivenName : *id = givenname; break;
		case attribSurname: *id = sn; break;
		case attribCustom1: *id = custom1; break;
		case attribCustom2: *id = custom2; break;
		case attribCustom3: *id = custom3; break;
		case attribCustom4: *id = custom4; break;
		case attribCustom5: *id = custom5; break;
		default:
			return SearchError_ScopeAgreement;
	}
	return SearchError_Success;
}

SEARCH_API MSG_SearchError MSG_GetSearchWidgetForAttribute (
    MSG_SearchAttribute attrib,
    MSG_SearchValueWidget *widget)
{
	if (NULL == widget)
		return SearchError_NullPointer;

	MSG_SearchError err = SearchError_Success;

	switch (attrib)
	{
	case attribDate:
		*widget = widgetDate;
		break;
	case attribAgeInDays:
		*widget = widgetInt;
		break;
	case attribMsgStatus:
	case attribPriority:
		*widget = widgetMenu;
		break;
	default:
		if (attrib < kNumAttributes)
			if (attrib != attribLocation)
				*widget = widgetText;
			else
				err = SearchError_ScopeAgreement;
		else
			err = SearchError_InvalidAttribute;
	}
	return err;
}

SEARCH_API MSG_PRIORITY MSG_GetPriorityFromString(const char *priority)
{
	if (strcasestr(priority, "Normal") != NULL)
		return MSG_NormalPriority;
	else if (strcasestr(priority, "Lowest") != NULL)
		return MSG_LowestPriority;
	else if (strcasestr(priority, "Highest") != NULL)
		return MSG_HighestPriority;
	else if (strcasestr(priority, "High") != NULL || 
			 strcasestr(priority, "Urgent") != NULL)
		return MSG_HighPriority;
	else if (strcasestr(priority, "Low") != NULL ||
			 strcasestr(priority, "Non-urgent") != NULL)
		return MSG_LowPriority;
	else if (strcasestr(priority, "1") != NULL)
		return MSG_HighestPriority;
	else if (strcasestr(priority, "2") != NULL)
		return MSG_HighPriority;
	else if (strcasestr(priority, "3") != NULL)
		return MSG_NormalPriority;
	else if (strcasestr(priority, "4") != NULL)
		return MSG_LowPriority;
	else if (strcasestr(priority, "5") != NULL)
	    return MSG_LowestPriority;
	else
		return MSG_NormalPriority;
		//return MSG_NoPriority;
}


SEARCH_API void MSG_GetUntranslatedPriorityName (MSG_PRIORITY p, char *outName, uint16 maxOutName)
{
	char *tmpOutName = NULL;
	switch (p)
	{
	case MSG_PriorityNotSet:
	case MSG_NoPriority:
		tmpOutName = "None";
		break;
	case MSG_LowestPriority:
		tmpOutName = "Lowest";
		break;
	case MSG_LowPriority:
		tmpOutName = "Low";
		break;
	case MSG_NormalPriority:
		tmpOutName = "Normal";
		break;
	case MSG_HighPriority:
		tmpOutName = "High";
		break;
	case MSG_HighestPriority:
		tmpOutName = "Highest";
		break;
	default:
		XP_ASSERT(FALSE);
	}

	if (tmpOutName)
		XP_STRNCPY_SAFE (outName, tmpOutName, maxOutName);
	else
		outName[0] = '\0';
}


SEARCH_API void MSG_GetPriorityName (MSG_PRIORITY p, char *outName, uint16 maxOutName)
{
	char *tmpOutName = NULL;
	switch (p)
	{
	case MSG_PriorityNotSet:
	case MSG_NoPriority:
		tmpOutName = XP_GetString(XP_PRIORITY_NONE);
		break;
	case MSG_LowestPriority:
		tmpOutName = XP_GetString(XP_PRIORITY_LOWEST);
		break;
	case MSG_LowPriority:
		tmpOutName = XP_GetString(XP_PRIORITY_LOW);
		break;
	case MSG_NormalPriority:
		tmpOutName = XP_GetString(XP_PRIORITY_NORMAL);
		break;
	case MSG_HighPriority:
		tmpOutName = XP_GetString(XP_PRIORITY_HIGH);
		break;
	case MSG_HighestPriority:
		tmpOutName = XP_GetString(XP_PRIORITY_HIGHEST);
		break;
	default:
		XP_ASSERT(FALSE);
	}

	if (tmpOutName)
		XP_STRNCPY_SAFE (outName, tmpOutName, maxOutName);
	else
		outName[0] = '\0';
}


SEARCH_API void MSG_GetStatusName (uint32 s, char *outName, uint16 maxOutName)
{
	char *tmpOutName = NULL;
#define MSG_STATUS_MASK (MSG_FLAG_READ | MSG_FLAG_REPLIED | MSG_FLAG_FORWARDED | MSG_FLAG_NEW)
	uint32 maskOut = (s & MSG_STATUS_MASK);

	// diddle the flags to pay attention to the most important ones first, if multiple
	// flags are set. Should remove this code from the winfe.
	if (maskOut & MSG_FLAG_NEW) 
		maskOut = MSG_FLAG_NEW;
	if ( maskOut & MSG_FLAG_REPLIED &&
		 maskOut & MSG_FLAG_FORWARDED ) 
		maskOut = MSG_FLAG_REPLIED|MSG_FLAG_FORWARDED;
	else if ( maskOut & MSG_FLAG_FORWARDED )
		maskOut = MSG_FLAG_FORWARDED;
	else if ( maskOut & MSG_FLAG_REPLIED ) 
		maskOut = MSG_FLAG_REPLIED;

	switch (maskOut)
	{
	case MSG_FLAG_READ:
		tmpOutName = XP_GetString(XP_STATUS_READ);
		break;
	case MSG_FLAG_REPLIED:
		tmpOutName = XP_GetString(XP_STATUS_REPLIED);
		break;
	case MSG_FLAG_FORWARDED:
		tmpOutName = XP_GetString(XP_STATUS_FORWARDED);
		break;
	case MSG_FLAG_FORWARDED|MSG_FLAG_REPLIED:
		tmpOutName = XP_GetString(XP_STATUS_REPLIED_AND_FORWARDED);
		break;
	case MSG_FLAG_NEW:
		tmpOutName = XP_GetString(XP_STATUS_NEW);
		break;
	default:
		// This is fine, status may be "unread" for example
        break;
	}

	if (tmpOutName)
		XP_STRNCPY_SAFE (outName, tmpOutName, maxOutName);
	else
		outName[0] = '\0';
}

SEARCH_API uint32 MSG_GetStatusValueFromName(char *name)
{
	char *tmpOutName;

	tmpOutName = XP_GetString(XP_STATUS_READ);
	if (tmpOutName && !XP_STRCMP(tmpOutName, name))
		return MSG_FLAG_READ;
	tmpOutName = XP_GetString(XP_STATUS_REPLIED);
	if (tmpOutName && !XP_STRCMP(tmpOutName, name))
		return MSG_FLAG_REPLIED;
	tmpOutName = XP_GetString(XP_STATUS_FORWARDED);
	if (tmpOutName && !XP_STRCMP(tmpOutName, name))
		return MSG_FLAG_FORWARDED;
	tmpOutName = XP_GetString(XP_STATUS_REPLIED_AND_FORWARDED);
	if (tmpOutName && !XP_STRCMP(tmpOutName, name))
		return MSG_FLAG_FORWARDED|MSG_FLAG_REPLIED;
	tmpOutName = XP_GetString(XP_STATUS_NEW);
	if (tmpOutName && !XP_STRCMP(tmpOutName, name))
		return MSG_FLAG_NEW;
	return 0;
}

SEARCH_API MSG_SearchError MSG_GetLdapObjectClasses (MSG_Pane *pane, 
							char **objectClassList, uint16 *maxItems)
{
	XP_ASSERT(objectClassList);
	XP_ASSERT(maxItems);

	MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (pane);
	if (!frame)
		return SearchError_InvalidPane;
	return frame->GetLdapObjectClasses (objectClassList, maxItems);
}


SEARCH_API MSG_SearchError MSG_AddAllSearchableGroupsStatus (MSG_Pane * /*searchPane*/, XP_Bool * /*enabled*/)
{
	return SearchError_NotImplemented;
}


SEARCH_API MSG_SearchError MSG_AddAllSearchableGroups (MSG_Pane * /*searchPane*/)
{
	return SearchError_NotImplemented;
}


//-----------------------------------------------------------------------------
//--------------------- These are called from libnet --------------------------
//-----------------------------------------------------------------------------


// Since each search can run over multiple scopes, and scopes of different
// types (e.g. online mail and offline mail), we use one meta-URL of the
// format "search:", which watches to see when each scope finishes, and 
// then fires off the next one in serial.
//
SEARCH_API int MSG_ProcessSearch (MWContext *context)
{
	MSG_SearchFrame *frame = MSG_SearchFrame::FromContext (context);
	XP_ASSERT(frame);

	return frame->TimeSlice();
}


SEARCH_API int MSG_InterruptSearch (MWContext *context)
{
	MSG_SearchFrame *frame = MSG_SearchFrame::FromContext (context);
	XP_ASSERT(frame);

	if (frame)
	{
		msg_SearchAdapter *adapter = frame->GetRunningAdapter();
		if (adapter)
			return frame->GetRunningAdapter()->Abort();
	}

	return 0;
}

SEARCH_API MSG_SearchTermArray *MSG_CreateSearchTermArray()
{
	return new MSG_SearchTermArray;
}

SEARCH_API MSG_SearchError MSG_SearchGetNumTerms(MSG_SearchTermArray *terms, int32 *numTerms)
{
	if (!terms || !numTerms)
		return SearchError_NullPointer;
	*numTerms = terms->GetSize();
	return SearchError_Success;
}

SEARCH_API MSG_SearchError  MSG_AddSearchTermToArray(
	MSG_SearchTermArray *array, 
    MSG_SearchAttribute attrib,    /* attribute for this term                */
    MSG_SearchOperator op,         /* operator e.g. opContains               */
    MSG_SearchValue *value,       /* value e.g. "Fred"                   */
	XP_Bool BooleanAND,			  /* boolean AND operator?					*/
	char * arbitraryHeader)		  /* user defined arbitrary header. ignored unless attrib = attribOtherHeader */
{
	MSG_SearchTerm *newSearchTerm = new MSG_SearchTerm(attrib, op, value, BooleanAND, arbitraryHeader);
	if (newSearchTerm == NULL)
		return SearchError_OutOfMemory;
	array->Add(newSearchTerm);
	return SearchError_Success;
}

SEARCH_API MSG_SearchError MSG_SearchGetTerm(MSG_SearchTermArray *array, int32 termIndex, 
	MSG_SearchAttribute *attrib,    /* attribute for this term                */
    MSG_SearchOperator *op,         /* operator e.g. opContains               */
    MSG_SearchValue *value)       /* value e.g. "Fred"                   */
{
	MSG_SearchTerm *term = (MSG_SearchTerm *) array->GetAt(termIndex);
	if (term == NULL)
		return SearchError_InvalidIndex;
	*attrib = term->m_attribute;
	*op = term->m_operator;
	*value = term->m_value;
	return SearchError_Success;
}


SEARCH_API void MSG_DestroySearchTermArray(MSG_SearchTermArray *array)
{
	delete array;
}


SEARCH_API char *MSG_EscapeSearchUrl (const char *nntpCommand)
{
	char *result = NULL;
	// max escaped length is two extra characters for every character in the cmd.
	char *scratchBuf = (char*) XP_ALLOC (3*XP_STRLEN(nntpCommand) + 1);
	if (scratchBuf)
	{
		char *scratchPtr = scratchBuf;
		while (1)
		{
			char ch = *nntpCommand++;
			if (!ch)
				break;
			if (ch == '#' || ch == '?' || ch == '@' || ch == '\\')
			{
				*scratchPtr++ = '\\';
				sprintf (scratchPtr, "%X", ch);
				scratchPtr += 2;
			}
			else
				*scratchPtr++ = ch;
		}
		*scratchPtr = '\0';
		result = XP_STRDUP (scratchBuf); // realloc down to smaller size
		XP_FREE (scratchBuf);
	}
	return result;
}


static
char *msg_EscapeImapSearchProtocol(const char *imapCommand)
{
	char *result = NULL;
	// max escaped length is one extra character for every character in the cmd.
	char *scratchBuf = (char*) XP_ALLOC (2*XP_STRLEN(imapCommand) + 1);
	if (scratchBuf)
	{
		char *scratchPtr = scratchBuf;
		while (1)
		{
			char ch = *imapCommand++;
			if (!ch)
				break;
			if (ch == '\\')
			{
				*scratchPtr++ = '\\';
				*scratchPtr++ = '\\';
			}
			else
				*scratchPtr++ = ch;
		}
		*scratchPtr = '\0';
		result = XP_STRDUP (scratchBuf); // realloc down to smaller size
		XP_FREE (scratchBuf);
	}
	return result;
}


SEARCH_API char *MSG_UnEscapeSearchUrl (const char *commandSpecificData)
{
	char *result = (char*) XP_ALLOC (XP_STRLEN(commandSpecificData) + 1);
	if (result)
	{
		char *resultPtr = result;
		while (1)
		{
			char ch = *commandSpecificData++;
			if (!ch)
				break;
			if (ch == '\\')
			{
				char scratchBuf[3];
				scratchBuf[0] = (char) *commandSpecificData++;
				scratchBuf[1] = (char) *commandSpecificData++;
				scratchBuf[2] = '\0';
				int accum = 0;
				sscanf (scratchBuf, "%X", &accum);
				*resultPtr++ = (char) accum;
			}
			else
				*resultPtr++ = ch;
		}
		*resultPtr = '\0';
	}
	return result;
}


SEARCH_API XP_Bool MSG_AcquireEditHeadersSemaphore (MSG_Master * master, void * holder)
{
	if (master)
		return master->AcquireEditHeadersSemaphore(holder);
	else
		return FALSE;
}


SEARCH_API XP_Bool MSG_ReleaseEditHeadersSemaphore (MSG_Master * master, void * holder)
{
	if (master)
		return master->ReleaseEditHeadersSemaphore(holder);
	return FALSE;
}


//-----------------------------------------------------------------------------
//------------------- Implementation of public opaque types -------------------
//-----------------------------------------------------------------------------

uint32 MSG_ResultElement::m_expectedMagic = 0x73726573; // 'sres'
uint32 MSG_SearchTerm::m_expectedMagic    = 0x7374726d; // 'strm'
uint32 MSG_ScopeTerm::m_expectedMagic     = 0x7373636f; // 'ssco'
uint32 MSG_SearchFrame::m_expectedMagic   = 0x7366726d; // 'sfrm'

//-----------------------------------------------------------------------------
// MSG_ResultElement implementation
//-----------------------------------------------------------------------------


MSG_ResultElement::MSG_ResultElement(msg_SearchAdapter *adapter) : msg_OpaqueObject (m_expectedMagic)
{
	m_adapter = adapter;
}


MSG_ResultElement::~MSG_ResultElement () 
{
	for (int i = 0; i < m_valueList.GetSize(); i++)
	{
		MSG_SearchValue *value = m_valueList.GetAt(i);
		if (value->attribute == attribJpegFile)
		{
			char *url = XP_PlatformFileToURL (value->u.string);
			char *tmp = url + XP_STRLEN("file://");
			XP_FileRemove (tmp, xpMailFolder /*###phil hacky*/);
			XP_FREE(url);
		}
		MSG_ResultElement::DestroyValue (value);
	}
}


MSG_SearchError MSG_ResultElement::AddValue (MSG_SearchValue *value)
{ 
	m_valueList.Add (value); 
	return SearchError_Success;
}


MSG_SearchError MSG_ResultElement::DestroyValue (MSG_SearchValue *value)
{
	if (IsStringAttribute(value->attribute))
	{
		XP_ASSERT(value->u.string);
		XP_FREE (value->u.string);
	}
	delete value;
	return SearchError_Success;
}


MSG_SearchError MSG_ResultElement::AssignValues (MSG_SearchValue *src, MSG_SearchValue *dst)
{
	// Yes, this could be an operator overload, but MSG_SearchValue is totally public, so I'd
	// have to define a derived class with nothing by operator=, and that seems like a bit much
	MSG_SearchError err = SearchError_Success;
	switch (src->attribute)
	{
	case attribPriority:
		dst->attribute = src->attribute;
		dst->u.priority = src->u.priority;
		break;
	case attribDate:
		dst->attribute = src->attribute;
		dst->u.date = src->u.date;
		break;
	case attribMsgStatus:
		dst->attribute = src->attribute;
		dst->u.msgStatus = src->u.msgStatus;
		break;
	case attribMessageKey:
		dst->attribute = src->attribute;
		dst->u.key = src->u.key;
		break;
	case attribAgeInDays:
		dst->attribute = src->attribute;
		// hack, mscott...setting age to 1 by default until the FEs implement Age by day...
		// this will of course break the purge feature right now. WILL REPLACE ONCE FEs implement age by Day
		// ##mscott WINFE has checked in age in days support...
#if defined(XP_WIN)
		dst->u.age = src->u.age;
#else
		dst->u.age = 1; // only temporary!
#endif

		break;
	default:
		if (src->attribute < kNumAttributes)
		{
			XP_ASSERT(IsStringAttribute(src->attribute));
			dst->attribute = src->attribute;
			dst->u.string = XP_STRDUP(src->u.string);
			if (!dst->u.string)
				err = SearchError_OutOfMemory;
		}
		else
			err = SearchError_InvalidAttribute;
	}
	return err;
}


MSG_SearchError MSG_ResultElement::GetValue (MSG_SearchAttribute attrib, MSG_SearchValue **outValue) const
{
	MSG_SearchError err = SearchError_ScopeAgreement;
	MSG_SearchValue *value = NULL;
	*outValue = NULL;

	for (int i = 0; i < m_valueList.GetSize() && err != SearchError_Success; i++)
	{
		value = m_valueList.GetAt(i);
		if (attrib == value->attribute)
		{
			*outValue = new MSG_SearchValue;
			if (*outValue)
			{
				err = AssignValues (value, *outValue);
				err = SearchError_Success;
			}
			else
				err = SearchError_OutOfMemory;
		}
	}

	// No need to store the folderInfo separately; we can always get it if/when
	// we need it. This code is to support "view thread context" in the search dialog
	if (SearchError_ScopeAgreement == err && attrib == attribFolderInfo)
	{
		MSG_FolderInfo *targetFolder = m_adapter->FindTargetFolder (this);
		if (targetFolder)
		{
			*outValue = new MSG_SearchValue;
			if (*outValue)
			{
				(*outValue)->u.folder = targetFolder;
				(*outValue)->attribute = attribFolderInfo;
				err = SearchError_Success;
			}
		}
	}

	return err;
}


const MSG_SearchValue *MSG_ResultElement::GetValueRef (MSG_SearchAttribute attrib) const 
{
	MSG_SearchValue *value =  NULL;
	for (int i = 0; i < m_valueList.GetSize(); i++)
	{
		value = m_valueList.GetAt(i);
		if (attrib == value->attribute)
			return value;
	}
	return NULL;
}


MSG_SearchError MSG_ResultElement::GetPrettyName (MSG_SearchValue **value)
{
	MSG_SearchError err = GetValue (attribLocation, value);
	if (SearchError_Success == err)
	{
		MSG_FolderInfo *folder = m_adapter->m_scope->m_folder;
		MSG_NewsHost *host = NULL;
		if (folder)
		{
			// Find the news host because only the host knows whether pretty
			// names are supported. 
			if (FOLDER_CONTAINERONLY == folder->GetType())
				host = ((MSG_NewsFolderInfoContainer*) folder)->GetHost();
			else if (folder->IsNews())
				host = folder->GetNewsFolderInfo()->GetHost();

			// Ask the host whether it knows pretty names. It isn't strictly
			// necessary to avoid calling folder->GetPrettiestName() since it
			// does the right thing. But we do have to find the folder from the host.
			if (host && host->QueryExtension ("LISTPNAMES"))
			{
				folder = host->FindGroup ((*value)->u.string);
				if (folder)
				{
					char *tmp = XP_STRDUP (folder->GetPrettiestName());
					if (tmp)
					{
						XP_FREE ((*value)->u.string);
						(*value)->u.string = tmp;
					}
				}
			}
		}
	}
	return err;
}

int MSG_ResultElement::CompareByFolderInfoPtrs (const void *e1, const void *e2)
{
	MSG_ResultElement * re1 = *(MSG_ResultElement **) e1;
	MSG_ResultElement * re2 = *(MSG_ResultElement **) e2;

	// get the src folder for each one
	
	const MSG_SearchValue * v1 = re1->GetValueRef(attribFolderInfo);
	const MSG_SearchValue * v2 = re2->GetValueRef(attribFolderInfo);

	if (!v1 || !v2)
		return 0;

	return (v1->u.folder - v2->u.folder);
}



int MSG_ResultElement::Compare (const void *e1, const void *e2)
{
	// Bad idea to cast away const, but they're my objects anway.
	// Maybe if we go through and const everything this should be a const ptr.
	MSG_ResultElement *re1 = *(MSG_ResultElement**) e1;
	MSG_ResultElement *re2 = *(MSG_ResultElement**) e2;

	XP_ASSERT(re1->IsValid());
	XP_ASSERT(re2->IsValid());

	MSG_SearchAttribute attrib = re1->m_adapter->m_scope->m_frame->m_sortAttribute;

	const MSG_SearchValue *v1 = re1->GetValueRef (attrib);
	const MSG_SearchValue *v2 = re2->GetValueRef (attrib);

	int ret = 0;
	if (!v1 || !v2)
		return ret; // search result doesn't contain the attrib we want to sort on

	switch (attrib)
	{
	case attribDate:
		{
			// on Win 3.1, the requisite 'int' return type is a short, so use a 
			// real time_t for comparison
			time_t date = v1->u.date - v2->u.date;
			if (date)
				ret = ((long)date) < 0 ? -1 : 1;
			else
				ret = 0;
		}
		break;
	case attribPriority:
		ret = v1->u.priority - v2->u.priority;
		break;
	case attribMsgStatus:
		{
			// Here's an arbitrary sorting protocol for msg status
			uint32 s1, s2;

			s1 = v1->u.msgStatus & ~MSG_FLAG_REPLIED;
			s2 = v2->u.msgStatus & ~MSG_FLAG_REPLIED;
			if (s1 || s2)
				ret = s1 - s2;
			else
			{
				s1 = v1->u.msgStatus & ~MSG_FLAG_FORWARDED;
				s2 = v2->u.msgStatus & ~MSG_FLAG_FORWARDED;
				if (s1 || s2)
					ret = s1 - s2;
				else
				{
					s1 = v1->u.msgStatus & ~MSG_FLAG_READ;
					s2 = v2->u.msgStatus & ~MSG_FLAG_READ;
					if (s1 || s2)
						ret = s1 - s2;
					else
						// statuses don't contain any flags we're interested in, 
						// so they're equal as far as we care
						ret = 0;
				}
			}
		}
		break;
	default:
		if (attrib == attribSubject)
		{
			// Special case for subjects, so "Re:foo" sorts under 'f' not 'r'
			const char *s1 = v1->u.string;
			const char *s2 = v2->u.string;
			msg_StripRE (&s1, NULL);
			msg_StripRE (&s2, NULL);
			ret = strcasecomp (s1, s2);
		}
		else
			ret = strcasecomp (v1->u.string, v2->u.string);
	}

	// qsort's default sort order is ascending, so in order to get descending
	// behavior, we'll tell qsort a lie and reverse the comparison order.
	if (re1->m_adapter->m_scope->m_frame->m_descending && ret != 0)
		if (ret < 0)
			ret = 1;
		else
			ret = -1;

	// <0 --> e1 less than e2
	// 0  --> e1 equal to e2
	// >0 --> e1 greater than e2
	return ret;
}


MWContextType MSG_ResultElement::GetContextType ()
{
	MWContextType type=(MWContextType)0;
	switch (m_adapter->m_scope->m_attribute)
	{
	case scopeMailFolder:
		type = MWContextMailMsg;
		break;
	case scopeOfflineNewsgroup:    // added by mscott could be bug fix...
	case scopeNewsgroup:
	case scopeAllSearchableGroups:
		type = MWContextNewsMsg;
		break;
	case scopeLdapDirectory:
		type = MWContextBrowser;
		break;
	default:
		XP_ASSERT(FALSE); // should never happen
	}
	return type;
}


MSG_SearchError MSG_ResultElement::Open (void *window)
{
	MSG_MessagePane *msgPane = NULL;
	MWContext *context = NULL;

	// ###phil this is a little ugly, but I'll wait before spending more time on it
	// until the libnet rework is done and I know what kind of context we'll end up with

	if (window)
	{
		if (m_adapter->m_scope->m_attribute != scopeLdapDirectory)
		{
			msgPane = (MSG_MessagePane *) window; 
			XP_ASSERT (MSG_MESSAGEPANE == msgPane->GetPaneType());
			return m_adapter->OpenResultElement (msgPane, this);
		}
		else
		{
			context = (MWContext*) window;
			XP_ASSERT (MWContextBrowser == context->type);
			msg_SearchLdap *thisAdapter = (msg_SearchLdap*) m_adapter;
			return thisAdapter->OpenResultElement (context, this);
		}
	}
	return SearchError_NullPointer;
}


//-----------------------------------------------------------------------------
//----------------------- Base class for search objects -----------------------
//-----------------------------------------------------------------------------

msg_SearchAdapter::msg_SearchAdapter (MSG_ScopeTerm *scope, MSG_SearchTermArray &termList) : m_searchTerms(termList)
{
	m_abortCalled = FALSE;
	m_scope = scope;
}


msg_SearchAdapter::~msg_SearchAdapter ()
{
}


MSG_SearchError msg_SearchAdapter::ValidateTerms ()
{
	if (!m_scope->IsValid())
		return SearchError_InvalidScopeTerm;

	MSG_SearchTerm *searchTerm = NULL;
	for (int i = 0; i < m_searchTerms.GetSize(); i++)
	{
		searchTerm = m_searchTerms.GetAt(i);
		if (!searchTerm->IsValid())
			return SearchError_InvalidSearchTerm;
	}

	return SearchError_Success;
}


int msg_SearchAdapter::Abort ()
{
	// Don't leave progress artifacts in the search dialog
	FE_SetProgressBarPercent (m_scope->m_frame->GetContext(), 0);

	m_abortCalled = TRUE;
	return 0;
}


MSG_SearchError msg_SearchAdapter::OpenNewsResultInUnknownGroup (MSG_MessagePane *pane, MSG_ResultElement *result)
{
	// Here's a hacky little way to open a news article given only the host and message-id
	//
	// A better fix might be to open the newsgroup (via MessagePane::LoadFolder)
	// and then chain a LoadMessage after that.

	MSG_SearchError err = SearchError_Unknown;

	MSG_NewsHost *host = NULL;
	MSG_FolderInfoNews *newsFolder = m_scope->m_folder->GetNewsFolderInfo();
	if (newsFolder)
		host = newsFolder->GetHost();
	else if (FOLDER_CONTAINERONLY == m_scope->m_folder->GetType())
		host = ((MSG_NewsFolderInfoContainer*) m_scope->m_folder)->GetHost();

	if (host)
	{
		const MSG_SearchValue *messageIdValue = result->GetValueRef (attribMessageId);
		if (messageIdValue)
		{
			char *url = PR_smprintf ("%s/%s", host->GetURLBase(), messageIdValue->u.string);
			if (url)
			{
				URL_Struct *urlStruct = NET_CreateURLStruct (url, NET_DONT_RELOAD);
				if (urlStruct)
				{
					urlStruct->internal_url = TRUE;
					if (0 == pane->GetURL (urlStruct, TRUE))
						err = SearchError_Success;
				}
				else
					err = SearchError_OutOfMemory;
				XP_FREE(url);
			}
			else
				err = SearchError_OutOfMemory;
		}
	}

	return err;
}


MSG_SearchError msg_SearchAdapter::OpenResultElement (MSG_MessagePane *pane, MSG_ResultElement *result)
{
	MSG_SearchError err = SearchError_DBOpenFailed; // not true, really
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(pane->GetContext());

	const MSG_SearchValue *keyValue = result->GetValueRef (attribMessageKey);
	if (keyValue)
	{
		MSG_FolderInfo *targetFolder = FindTargetFolder (result);

		if (targetFolder)
		{
			int16 csid = INTL_GetCSIWinCSID(c);
			if (!csid)
			{
				csid = targetFolder->GetFolderCSID();
				if (!csid)
					csid = INTL_DefaultWinCharSetID(0);
				INTL_SetCSIWinCSID(c, csid);
			}

			if (0 == pane->LoadMessage (targetFolder, keyValue->u.key, NULL, TRUE))
				err = SearchError_Success;
		}
		else
			err = OpenNewsResultInUnknownGroup (pane, result);
	}

	return err;
}


MSG_FolderInfo *msg_SearchAdapter::FindTargetFolder (const MSG_ResultElement *result)
{
	MSG_FolderInfo *targetFolder = m_scope->m_folder;
	if (!targetFolder)
		return NULL;

	if (m_scope->m_attribute == scopeAllSearchableGroups)
	{
		const MSG_SearchValue *locValue = result->GetValueRef (attribLocation);
		if (locValue)
		{
			XP_ASSERT(m_scope->m_folder->GetType() == FOLDER_CONTAINERONLY);
			MSG_NewsHost *host = ((MSG_NewsFolderInfoContainer*) m_scope->m_folder)->GetHost();
			
			if (host)
				targetFolder = host->FindGroup (locValue->u.string);
		}
	}
	return targetFolder;
}


MSG_SearchError msg_SearchAdapter::ModifyResultElement (MSG_ResultElement*, MSG_SearchValue*)
{
	// The derived classes should override if they want to allow modification.
	// So if we got to the base class, we don't allow it.
	return SearchError_ScopeAgreement;
}

// This stuff lives in the base class because the IMAP search syntax 
// is used by the RFC977bis SEARCH command as well as IMAP itself

// km - the NOT and HEADER strings are not encoded with a trailing
//      <space> because they always precede a mnemonic that has a
//      preceding <space> and double <space> characters cause some
//		imap servers to return an error.
const char *msg_SearchAdapter::m_kImapBefore   = " SENTBEFORE ";
const char *msg_SearchAdapter::m_kImapBody     = " BODY ";
const char *msg_SearchAdapter::m_kImapCC       = " CC ";
const char *msg_SearchAdapter::m_kImapFrom     = " FROM ";
const char *msg_SearchAdapter::m_kImapNot      = " NOT";
const char *msg_SearchAdapter::m_kImapDeleted  = " DELETED";
const char *msg_SearchAdapter::m_kImapOr       = " OR";
const char *msg_SearchAdapter::m_kImapSince    = " SENTSINCE ";
const char *msg_SearchAdapter::m_kImapSubject  = " SUBJECT ";
const char *msg_SearchAdapter::m_kImapTo       = " TO ";
const char *msg_SearchAdapter::m_kImapHeader   = " HEADER";
const char *msg_SearchAdapter::m_kImapAnyText  = " TEXT ";
const char *msg_SearchAdapter::m_kImapKeyword  = " KEYWORD ";
const char *msg_SearchAdapter::m_kNntpKeywords = " KEYWORDS ";
const char *msg_SearchAdapter::m_kImapSentOn = " SENTON ";
const char *msg_SearchAdapter::m_kImapSeen = " SEEN ";
const char *msg_SearchAdapter::m_kImapAnswered = " ANSWERED ";
const char *msg_SearchAdapter::m_kImapNotSeen = " UNSEEN ";
const char *msg_SearchAdapter::m_kImapNotAnswered = " UNANSWERED ";
const char *msg_SearchAdapter::m_kImapCharset = " CHARSET ";

char *
msg_SearchAdapter::TryToConvertCharset(char *sourceStr, int16 src_csid, int16 dest_csid, XP_Bool useMime2)
{
	char *result = NULL;

	if (sourceStr == NULL) 
		return NULL;

	if ((src_csid != dest_csid) || (useMime2))
	{
		// Need to convert. See if we can.

		// ### mwelch Much of this code is taken from 
		//     lib/libi18n/mime2fun.c (in particular,
		//     intl_EncodeMimePartIIStr).
		CCCDataObject obj = NULL;
		CCCFunc cvtfunc = NULL;
		int srcLen = XP_STRLEN(sourceStr);

		obj = INTL_CreateCharCodeConverter();
		if (obj == NULL)
			return 0;

		/* setup converter from src_csid --> dest_csid */
		INTL_GetCharCodeConverter(src_csid, dest_csid, obj) ;
		cvtfunc = INTL_GetCCCCvtfunc(obj);

		if (cvtfunc)
		{
			// We can convert, so do it.
			if (useMime2)
				// Force MIME-2 encoding so that the charset (if necessary) gets
				// passed to the RFC977bis server inline
				result = INTL_EncodeMimePartIIStr(sourceStr, src_csid, TRUE);
			else
			{
				// Copy the source string before using it for conversion.
				// You just don't know where those bits have been.
				char *temp = XP_STRDUP(sourceStr);
				if (temp)
				{
					// (result) will differ from (temp) iff a larger string 
					// were needed to contain the converted chars.
					// (or so I understand)
					result = (char *) cvtfunc(obj, 
											  (unsigned char*)temp, 
											  srcLen);
					if (result != temp)
						XP_FREE(temp);
				}
			}
		}
		XP_FREEIF(obj);
	}
	return result;
}

char *
msg_SearchAdapter::GetImapCharsetParam(int16 dest_csid)
{
	char *result = NULL;

	// Specify a character set unless we happen to be US-ASCII.
	if (! ((dest_csid == CS_ASCII) ||
		   (dest_csid == CS_UNKNOWN)))
	{
		char csname[128];
		INTL_CharSetIDToName(dest_csid, csname);
	    result = PR_smprintf("%s%s", msg_SearchAdapter::m_kImapCharset, csname);
	}
	return result;
}

void
msg_SearchAdapter::GetSearchCSIDs(int16& src_csid, int16& dst_csid)
{
	src_csid = CS_DEFAULT;
	dst_csid = CS_DEFAULT;

	if (m_scope)
	{
		if (m_scope->m_frame)
		{
			// If we can get the source charset from a context,
			// that is preferable to just using the app default.
			MWContext *ctxt = m_scope->m_frame->GetContext();
			if (ctxt) {
				INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(ctxt);
				src_csid = INTL_GetCSIWinCSID(c);
			}
		}

		// Ask the newsgroup/folder for its csid.
		if (m_scope->m_folder)
			dst_csid = m_scope->m_folder->GetFolderCSID() & ~CS_AUTO;
	}

	// default means that our best guess is to get the default window char set ID
	if (src_csid == CS_DEFAULT)
	{
		src_csid = INTL_DefaultWinCharSetID(0);
		XP_ASSERT(src_csid != CS_DEFAULT);
	}


	// If 
	// the destination is still CS_DEFAULT, make the destination match
	// the source. (CS_DEFAULT is an indication that the charset
	// was undefined or unavailable.)
	if (dst_csid == CS_DEFAULT)
		dst_csid = src_csid;

	XP_Bool forceAscii = FALSE;
	PREF_GetBoolPref("mailnews.force_ascii_search", &forceAscii);
	if (forceAscii)
	{
		// Special cases to use in order to force US-ASCII searching with Latin1
		// or MacRoman text. This only has to happen because IMAP
		// and RFC977bis servers currently (4/23/97) only support US-ASCII.
		// 
		// If the dest csid is ISO Latin 1 or MacRoman, attempt to convert the 
		// source text to US-ASCII. (Not for now.)
		// if ((dst_csid == CS_LATIN1) || (dst_csid == CS_MAC_ROMAN))
			dst_csid = CS_ASCII;
	}
}

#ifdef XP_MAC
#include <ConditionalMacros.h>

#pragma global_optimizer on
#pragma optimization_level 4

#endif

MSG_SearchError msg_SearchAdapter::EncodeImapTerm (MSG_SearchTerm *term, XP_Bool reallyRFC977bis, int16 src_csid, int16 dest_csid, char **ppOutTerm)
{
	MSG_SearchError err = SearchError_Success;
	XP_Bool useNot = FALSE;
	XP_Bool useQuotes = FALSE;
	XP_Bool excludeHeader = FALSE;
	XP_Bool ignoreValue = FALSE;
	char *arbitraryHeader = NULL;
	const char *whichMnemonic = NULL;
	const char *orHeaderMnemonic = NULL;

	*ppOutTerm = NULL;

	if (term->m_operator == opDoesntContain || term->m_operator == opIsnt)
		useNot = TRUE;

	switch (term->m_attribute)
	{
	case attribToOrCC:
		orHeaderMnemonic = m_kImapCC;
		// fall through to case attribTo:
	case attribTo:
		whichMnemonic = m_kImapTo;
		break;
	case attribCC:
		whichMnemonic = m_kImapCC;
		break;
	case attribSender:
		whichMnemonic = m_kImapFrom;
		break;
	case attribSubject:
		whichMnemonic = m_kImapSubject;
		break;
	case attribBody:
		whichMnemonic = m_kImapBody;
		excludeHeader = TRUE;
		break;
	case attribOtherHeader:  // arbitrary header? if so create arbitrary header string
		if (term->m_arbitraryHeader)
		{
			arbitraryHeader = new char [XP_STRLEN(term->m_arbitraryHeader) + 6];  // 6 bytes for SPACE \" .... \" SPACE
			if (!arbitraryHeader)
				return SearchError_InvalidSearchTerm;
			arbitraryHeader[0] = '\0';
			XP_STRCAT(arbitraryHeader, " \"");
			XP_STRCAT(arbitraryHeader, term->m_arbitraryHeader);
			XP_STRCAT(arbitraryHeader, "\" ");
			whichMnemonic = arbitraryHeader;
		}
		else
			return SearchError_InvalidSearchTerm;
		break;
	case attribAgeInDays:  // added for searching online for age in days...
		// for AgeInDays, we are actually going to perform a search by date, so convert the operations for age
		// to the IMAP mnemonics that we would use for date!
		switch (term->m_operator)
		{
		case opIsGreaterThan:
			whichMnemonic = m_kImapBefore;
			break;
		case opIsLessThan:
			whichMnemonic = m_kImapSince;
			break;
		case opIs:
			whichMnemonic = m_kImapSentOn;
			break;
		default:
			XP_ASSERT(FALSE);
			return SearchError_InvalidSearchTerm;
		}
		excludeHeader = TRUE;
		break;
	case attribDate:
		switch (term->m_operator)
		{
		case opIsBefore:
			whichMnemonic = m_kImapBefore;
			break;
		case opIsAfter:
			whichMnemonic = m_kImapSince;
			break;
		case opIs:
			whichMnemonic = m_kImapSentOn;
			break;
		default:
			XP_ASSERT(FALSE);
			return SearchError_InvalidSearchTerm;
		}
		excludeHeader = TRUE;
		break;
	case attribAnyText:
		whichMnemonic = m_kImapAnyText;
		excludeHeader = TRUE;
		break;
	case attribKeywords:
		whichMnemonic = m_kNntpKeywords;
		break;
	case attribMsgStatus:
		useNot = FALSE; // NOT SEEN is wrong, but UNSEEN is right.
		ignoreValue = TRUE; // the mnemonic is all we need
		excludeHeader = TRUE;
		switch (term->m_value.u.msgStatus)
		{
		case MSG_FLAG_READ:
			whichMnemonic = term->m_operator == opIs ? m_kImapSeen : m_kImapNotSeen;
			break;
		case MSG_FLAG_REPLIED:
			whichMnemonic = term->m_operator == opIs ? m_kImapAnswered : m_kImapNotAnswered;
			break;
		default:
			XP_ASSERT(FALSE);
			return SearchError_InvalidSearchTerm;
		}
		break;
	default:
		XP_ASSERT(FALSE);
		return SearchError_InvalidSearchTerm;
	}

	char *value = "";
	char dateBuf[100];
	dateBuf[0] = '\0';
	
	XP_Bool valueWasAllocated = FALSE;
	if (term->m_attribute == attribDate)
	{
		// note that there used to be code here that encoded an RFC822 date for imap searches.
		// RFC 2060 seems to require an RFC822
		// date but really it expects dd-mmm-yyyy, and refers to the RFC822 date only in that the
		// dd-mmm-yyyy date will match the RFC822 date within the message.
		strftime (dateBuf, sizeof(dateBuf), "%d-%b-%Y", localtime (&term->m_value.u.date));
		value = dateBuf;
	}
	else
	{
		if (term->m_attribute == attribAgeInDays)
		{
			// okay, take the current date, subtract off the age in days, then do an appropriate Date search on 
			// the resulting day.
			time_t now = XP_TIME();
			time_t matchDay = now - term->m_value.u.age * 60 * 60 * 24;
			strftime (dateBuf, sizeof(dateBuf), "%d-%b-%Y", localtime (&matchDay));
			value = dateBuf;
		}
		else

		if (IsStringAttribute(term->m_attribute))
		{
			char *unconvertedValue = reallyRFC977bis ? MSG_EscapeSearchUrl (term->m_value.u.string) : msg_EscapeImapSearchProtocol(term->m_value.u.string);
			// Switch for Korean mail/news charsets.
			// We want to do this here because here is where
			// we know what charset we want to use.
			if (reallyRFC977bis)
				dest_csid = INTL_DefaultNewsCharSetID(dest_csid);
			else
				dest_csid = INTL_DefaultMailCharSetID(dest_csid);

			value = TryToConvertCharset(unconvertedValue,
										src_csid,
										dest_csid,
										reallyRFC977bis);
			if (value)
				XP_FREEIF(unconvertedValue);
			else
				value = unconvertedValue; // couldn't convert, send as is

			valueWasAllocated = TRUE;
			useQuotes = !reallyRFC977bis || (XP_STRCHR(value, ' ') != NULL);
		}
	}

	int len = XP_STRLEN(whichMnemonic) + XP_STRLEN(value) + (useNot ? XP_STRLEN(m_kImapNot) : 0) + 
		(useQuotes ? 2 : 0) + XP_STRLEN(m_kImapHeader) + 
		(orHeaderMnemonic ? (XP_STRLEN(m_kImapHeader) + XP_STRLEN(m_kImapOr) + (useNot ? XP_STRLEN(m_kImapNot) : 0) + 
		XP_STRLEN(orHeaderMnemonic) + XP_STRLEN(value) + 2 /*""*/) : 0) + 1;
	char *encoding = new char[len];
	if (encoding)
	{
		encoding[0] = '\0';
		// Remember: if ToOrCC and useNot then the expression becomes NOT To AND Not CC as opposed to (NOT TO) || (NOT CC)
		if (orHeaderMnemonic && !useNot)
			XP_STRCAT(encoding, m_kImapOr);
		if (useNot)
			XP_STRCAT (encoding, m_kImapNot);
		if (!excludeHeader)
			XP_STRCAT (encoding, m_kImapHeader);
		XP_STRCAT (encoding, whichMnemonic);
		if (!ignoreValue)
			err = EncodeImapValue(encoding, value, useQuotes, reallyRFC977bis);
		
		if (orHeaderMnemonic)
		{
			if (useNot)
				XP_STRCAT(encoding, m_kImapNot);
			
			XP_STRCAT (encoding, m_kImapHeader);

			XP_STRCAT (encoding, orHeaderMnemonic);
			if (!ignoreValue)
				err = EncodeImapValue(encoding, value, useQuotes, reallyRFC977bis);
		}
		
		// don't let the encoding end with whitespace, 
		// this throws off later url STRCMP
		if (*encoding && *(encoding + XP_STRLEN(encoding) - 1) == ' ')
			*(encoding + XP_STRLEN(encoding) - 1) = '\0';
	}

	if (value && valueWasAllocated)
		XP_FREE (value);
	if (arbitraryHeader)
		delete arbitraryHeader;

	*ppOutTerm = encoding;

	return err;
}

#ifdef XP_MAC
#pragma global_optimizer reset
#endif

MSG_SearchError msg_SearchAdapter::EncodeImapValue(char *encoding, const char *value, XP_Bool useQuotes, XP_Bool reallyRFC977bis)
{
	// By NNTP RFC, SEARCH HEADER SUBJECT "" is legal and means 'find messages without a subject header'
	if (!reallyRFC977bis)
	{
		// By IMAP RFC, SEARCH HEADER SUBJECT "" is illegal and will generate an error from the server
		if (!value || !value[0])
			return SearchError_ValueRequired;
	}
	
	if (useQuotes)
		XP_STRCAT(encoding, "\"");
	XP_STRCAT (encoding, value);
	if (useQuotes)
		XP_STRCAT(encoding, "\"");

	return SearchError_Success;
}


MSG_SearchError msg_SearchAdapter::EncodeImap (char **ppOutEncoding, MSG_SearchTermArray &searchTerms, int16 src_csid, int16 dest_csid, XP_Bool reallyRFC977bis)
{
	// i've left the old code (before using CBoolExpression for debugging purposes to make sure that
	// the new code generates the same encoding string as the old code.....

	MSG_SearchError err = SearchError_Success;
	*ppOutEncoding = NULL;

	int termCount = searchTerms.GetSize();
	int i = 0;
	int encodingLength = 0;

	// Build up an array of encodings, one per query term
	char **termEncodings = new char *[termCount];
	if (!termEncodings)
		return SearchError_OutOfMemory;
	// create our expression
	CBoolExpression * expression = new CBoolExpression();
	if (!expression)
		return SearchError_OutOfMemory;

	for (i = 0; i < termCount && SearchError_Success == err; i++)
	{
		err = EncodeImapTerm (searchTerms.GetAt(i), reallyRFC977bis, src_csid, dest_csid, &termEncodings[i]);
		if (SearchError_Success == err && NULL != termEncodings[i])
		{
			encodingLength += XP_STRLEN(termEncodings[i]) + 1;
			expression = expression->AddSearchTerm(searchTerms.GetAt(i),termEncodings[i]);
		}
	}

	if (SearchError_Success == err) 
	{
		// Catenate the intermediate encodings together into a big string
		char *totalEncoding = new char [encodingLength + (!reallyRFC977bis ? (XP_STRLEN(m_kImapNot) + XP_STRLEN(m_kImapDeleted)) : 0) + 1];
		int32 encodingBuffSize = expression->CalcEncodeStrSize() + (!reallyRFC977bis ? (XP_STRLEN(m_kImapNot) + XP_STRLEN(m_kImapDeleted)) : 0) + 1;
		char *encodingBuff = new char [encodingBuffSize];

		if (encodingBuff && totalEncoding)
		{
			totalEncoding[0] = '\0';
			encodingBuff[0] = '\0';

			int offset = 0;       // offset into starting place for the buffer
			if (!reallyRFC977bis)
			{
				XP_STRCAT(totalEncoding, m_kImapNot);
				XP_STRCAT(totalEncoding, m_kImapDeleted);
			}

			if (!reallyRFC977bis)
			{
				XP_STRCAT(encodingBuff, m_kImapNot);
				XP_STRCAT(encodingBuff, m_kImapDeleted);
				offset = XP_STRLEN(m_kImapDeleted) + XP_STRLEN(m_kImapNot);
			}

			expression->GenerateEncodeStr(encodingBuff+offset,encodingBuffSize);

			for (i = 0; i < termCount; i++)
			{
				if (termEncodings[i])
				{
					XP_STRCAT (totalEncoding, termEncodings[i]);
					delete [] termEncodings[i];
				}
			}
		}
		else
			err = SearchError_OutOfMemory;

		delete totalEncoding;
		delete expression;

		// Set output parameter if we encoded the query successfully
		if (encodingBuff && SearchError_Success == err)
			*ppOutEncoding = encodingBuff;
	}

	delete [] termEncodings;

	return err;
}


char *msg_SearchAdapter::TransformSpacesToStars (const char *spaceString)
{
	char *starString = XP_STRDUP (spaceString);
	if (starString)
	{
		char *star = NULL;
		while (1)
		{
			star = XP_STRCHR(starString, ' ');
			if (!star)
				break;
			*star = '*';
		}
	}
	return starString;
}


//-----------------------------------------------------------------------------
//-------------------- Implementation of MSG_BodyHandler ----------------------
//-----------------------------------------------------------------------------

MSG_BodyHandler::MSG_BodyHandler (MSG_ScopeTerm * scope, uint32 offset, uint32 numLines, DBMessageHdr * msg, MessageDB * db)
{
	m_scope = scope;
	m_localFileOffset = offset;
	m_numLocalLines = numLines;
	m_msgHdr = msg;
	m_db = db;

	// the following are variables used when the body handler is handling stuff from filters....through this constructor, that is not the
	// case so we set them to NULL.
	m_headers = NULL;
	m_headersSize = 0;
	m_Filtering = FALSE; // make sure we set this before we call initialize...

	Initialize();  // common initialization stuff

	if (m_scope->IsOfflineIMAPMail() || m_scope->m_folder->GetType() == FOLDER_IMAPMAIL) // if we are here with an IMAP folder, assume offline!
		m_OfflineIMAP = TRUE;
	else
   	   OpenLocalFolder();	    // POP so open the mail folder file
}

MSG_BodyHandler::MSG_BodyHandler(MSG_ScopeTerm * scope, uint32 offset, uint32 numLines, DBMessageHdr * msg, MessageDB * db,
								 char * headers, uint32 headersSize, XP_Bool Filtering)
{
	m_scope = scope;
	m_localFileOffset = offset;
	m_numLocalLines = numLines;
	m_msgHdr = msg;
	m_db = db;
	m_headersSize = headersSize;
	m_Filtering = Filtering;

	Initialize();

	if (m_Filtering)
		m_headers = headers;
	else
		if (m_scope->IsOfflineIMAPMail() || m_scope->m_folder->GetType() == FOLDER_IMAPMAIL)
			m_OfflineIMAP = TRUE;
		else
			OpenLocalFolder();  // if nothing else applies, then we must be a POP folder file
}

void MSG_BodyHandler::Initialize()
// common initialization code regardless of what body type we are handling...
{
	// Default transformations for local message search and MAPI access
	m_stripHeaders = TRUE;
	m_stripHtml = TRUE;
	m_messageIsHtml = FALSE;
	m_passedHeaders = FALSE;

	// set our offsets to 0 since we haven't handled any bytes yet...
	m_IMAPMessageOffset = 0;
	m_NewsArticleOffset = 0;
	m_headerBytesRead = 0;

	m_OfflineIMAP = FALSE;
}

MSG_BodyHandler::~MSG_BodyHandler()
{
	if (m_scope->m_file)
	{
		XP_FileClose (m_scope->m_file);
		m_scope->m_file = NULL;
	}
}

		
void MSG_BodyHandler::OpenLocalFolder()
{
	if (!m_scope->m_file)
	{
		const char *path = m_scope->GetMailPath();
		if (path)
			m_scope->m_file = XP_FileOpen (path, xpMailFolder, XP_FILE_READ_BIN);    // open the folder
	}
	if (m_scope->m_file)
		XP_FileSeek (m_scope->m_file, m_localFileOffset, SEEK_SET); 
}


int32 MSG_BodyHandler::GetNextLine (char * buf, int bufSize)
{
	int32 length = 0;
	XP_Bool eatThisLine = FALSE;

	do {
		// first, handle the filtering case...this is easy....
		if (m_Filtering)
			length = GetNextFilterLine(buf, bufSize);
		else
		{
			// 3 cases: Offline IMAP, POP, or we are dealing with a news message....
			if (m_db)
			{
				MailDB * mailDB = m_db->GetMailDB();
				if (mailDB)    // a mail data base?
				{
					if (m_OfflineIMAP)
						length = GetNextIMAPLine (buf, bufSize);  // (1) IMAP Offline
					else
						length = GetNextLocalLine (buf, bufSize); // (2) POP
				}
				else
				{
					NewsGroupDB * newsDB = m_db->GetNewsDB();
					if (newsDB)
						length = GetNextNewsLine (newsDB, buf, bufSize);  // (3) News
				}
			}
		}

		if (length > 0)
			length = ApplyTransformations (buf, length, eatThisLine);
	} while (eatThisLine && length);  // if we hit eof, make sure we break out of this loop.

	return length;  
}


int32 MSG_BodyHandler::GetNextFilterLine(char * buf, int bufSize)
{
	// m_nextHdr always points to the next header in the list....the list is NULL terminated...
	int numBytesCopied = 0;
	if (m_headersSize > 0)
	{
		// #mscott. hack. filter headers list have CRs & LFs inside the NULL delimited list of header
		// strings. It is possible to have: To NULL CR LF From. We want to skip over these CR/LFs if they start
		// at the beginning of what we think is another header.
		while ((m_headers[0] == CR || m_headers[0] == LF || m_headers[0] == ' ' || m_headers[0] == '\0') && m_headersSize > 0)
		{
			m_headers++;  // skip over these chars...
			m_headersSize--;
		}

		if (m_headersSize > 0)
		{
			numBytesCopied = XP_STRLEN(m_headers)+1 /* + 1 to include NULL */ < bufSize ? XP_STRLEN(m_headers)+1 : bufSize;
			XP_MEMCPY(buf, m_headers, numBytesCopied);
			m_headers += numBytesCopied;  
			// be careful...m_headersSize is unsigned. Don't let it go negative or we overflow to 2^32....*yikes*	
			if (m_headersSize < numBytesCopied)
				m_headersSize = 0;
			else
				m_headersSize -= numBytesCopied;  // update # bytes we have read from the headers list

			return numBytesCopied;
		}
	}
	return 0;
}


int32 MSG_BodyHandler::GetNextNewsLine (NewsGroupDB * /* newsDB */, char * buf, int bufSize)
{
	// we know we have a safe downcasting on m_msgHdr to a NewsMessageHdr because we checked that
	// m_db is a news data base before calling this routine
	int32 msgLength = ((NewsMessageHdr *)m_msgHdr)->GetOfflineMessageLength (m_db->GetDB()) - m_NewsArticleOffset;
	if (buf && msgLength != 0) // make sure the news article exists....
	{
		int32 bytesToCopy = (msgLength < bufSize-2) ? msgLength : bufSize - 2; // this -2 is a small hack
		int32 bytesCopied = ((NewsMessageHdr *)m_msgHdr)->ReadFromArticle (buf, bytesToCopy, m_NewsArticleOffset, m_db->GetDB());
		if (bytesCopied == 0) // reached end of message?
			return bytesCopied;

		// now determine the location of the nearest CR/LF pairing...
		char * tmp = strncasestr (buf, "\x0D\x0A", bytesCopied); // get pointer to end of line
		if (tmp)
			// a line is contained within the buffer. Null terminate 2 positions past the CR/LF pair, update new offset value
			// we know we have at least 2 bytes leftover in the buffer
			*(tmp+2) = '\0';  // null terminate string after CR/LF
		else
		    buf[bytesCopied] = '\0';
		m_NewsArticleOffset += XP_STRLEN (buf);
		return XP_STRLEN (buf);      // return num bytes stored in the buf
	}
	return 0;
}


int32 MSG_BodyHandler::GetNextLocalLine(char * buf, int bufSize)
// returns number of bytes copied
{
	char * line = NULL;
	if (m_numLocalLines)
	{
		if (m_passedHeaders)
			m_numLocalLines--; // the line count is only for body lines
		line = XP_FileReadLine (buf, bufSize, m_scope->m_file);
		if (line)
			return XP_STRLEN(line);
	}

	return 0;
}


int32 MSG_BodyHandler::GetNextIMAPLine(char * buf, int bufSize)
{
	// we know we have safe downcasting on m_msgHdr because we checked that m_db is a mail data base before calling
	// this routine.
    int32 msgLength = ((MailMessageHdr *) m_msgHdr)->GetOfflineMessageLength (m_db->GetDB()) - m_IMAPMessageOffset;
	if (buf && msgLength != 0)  // make sure message exists
	{
		int32 bytesToCopy = (msgLength < bufSize-2) ? msgLength : bufSize-2; // the -2 is a small hack

		int32 bytesCopied = ((MailMessageHdr *) m_msgHdr)->ReadFromOfflineMessage (buf, bytesToCopy, m_IMAPMessageOffset, m_db->GetDB());
		if (bytesCopied == 0)  // we reached end of message
			return bytesCopied;

		// now determine the location of the nearest CR/LF pairing....
		char * tmp = strncasestr (buf, "\x0D\x0A",bytesCopied);   // get pointer to end of line
		if (tmp)
			// a line is contained within the buffer. Null terminate 2 positions past the CR/LF pair, update new offset value
			// we know we have at least 2 bytes leftover in the buffer
			*(tmp+2) = '\0';  // null terminate string after CR/LF
		else
		    buf[bytesCopied] = '\0';
		m_IMAPMessageOffset += XP_STRLEN (buf);
		return XP_STRLEN (buf);      // return num bytes stored in the buf
	}
	return 0;
}


int32 MSG_BodyHandler::ApplyTransformations (char *buf, int32 length, XP_Bool &eatThisLine)
{
	int32 newLength = length;
	eatThisLine = FALSE;

	if (!m_passedHeaders)	// buf is a line from the message headers
	{
		if (m_stripHeaders)
			eatThisLine = TRUE;

		if (!XP_STRNCASECMP(buf, "Content-Type:", 13) && strcasestr (buf, "text/html"))
			m_messageIsHtml = TRUE;

		m_passedHeaders = EMPTY_MESSAGE_LINE(buf);
	}
	else	// buf is a line from the message body
	{
		if (m_stripHtml && m_messageIsHtml)
		{
			StripHtml (buf);
			newLength = XP_STRLEN (buf);
		}
	}

	return newLength;
}


void MSG_BodyHandler::StripHtml (char *pBufInOut)
{
	char *pBuf = (char*) XP_ALLOC (XP_STRLEN(pBufInOut) + 1);
	if (pBuf)
	{
		char *pWalk = pBuf;
		char *pWalkInOut = pBufInOut;
		XP_Bool inTag = FALSE;
		while (*pWalkInOut) // throw away everything inside < >
		{
			if (!inTag)
				if (*pWalkInOut == '<')
					inTag = TRUE;
				else
					*pWalk++ = *pWalkInOut;
			else
				if (*pWalkInOut == '>')
					inTag = FALSE;
			pWalkInOut++;
		}
		*pWalk = 0; // null terminator

		// copy the temp buffer back to the real one
		pWalk = pBuf;
		pWalkInOut = pBufInOut;
		while (*pWalk)
			*pWalkInOut++ = *pWalk++;
		*pWalkInOut = *pWalk; // null terminator
		XP_FREE (pBuf);
	}
}

//-----------------------------------------------------------------------------
//-------------------- Implementation of MSG_SearchTerm -----------------------
//-----------------------------------------------------------------------------

// Needed for DeStream method.
MSG_SearchTerm::MSG_SearchTerm() : msg_OpaqueObject (m_expectedMagic)
{
	m_arbitraryHeader = NULL;
}



MSG_SearchTerm::MSG_SearchTerm (
	MSG_SearchAttribute attrib, 
	MSG_SearchOperator op, 
	MSG_SearchValue *val,
	XP_Bool BooleanAND,
	char * arbitraryHeader) : msg_OpaqueObject (m_expectedMagic)
{
	m_operator = op;
	m_booleanOp = (BooleanAND) ? MSG_SearchBooleanAND : MSG_SearchBooleanOR;
	if (attrib == attribOtherHeader && arbitraryHeader)
		m_arbitraryHeader = XP_STRDUP(arbitraryHeader);
	else
		m_arbitraryHeader = NULL;
	m_attribute = attrib;

	MSG_ResultElement::AssignValues (val, &m_value);
}

MSG_SearchTerm::MSG_SearchTerm (
	MSG_SearchAttribute attrib, 
	MSG_SearchOperator op, 
	MSG_SearchValue *val,
	MSG_SearchBooleanOp boolOp,
	char * arbitraryHeader) : msg_OpaqueObject (m_expectedMagic)
{
	m_operator = op;
	m_attribute = attrib;
	m_booleanOp = boolOp;
	if (attrib == attribOtherHeader && arbitraryHeader)
		m_arbitraryHeader = XP_STRDUP(arbitraryHeader);
	else
		m_arbitraryHeader = NULL;

	MSG_ResultElement::AssignValues (val, &m_value);
}



MSG_SearchTerm::~MSG_SearchTerm ()
{
	if (IsStringAttribute (m_attribute))
		XP_FREE(m_value.u.string);
	if (m_arbitraryHeader)
		XP_FREE(m_arbitraryHeader);
}

// Perhaps we could find a better place for this?
// Caller needs to free.
/* static */char *MSG_SearchTerm::EscapeQuotesInStr(const char *str)
{
	int	numQuotes = 0;
	for (const char *strPtr = str; *strPtr; strPtr++)
		if (*strPtr == '"')
			numQuotes++;
	int escapedStrLen = XP_STRLEN(str) + numQuotes;
	char	*escapedStr = (char *) XP_ALLOC(escapedStrLen + 1);
	if (escapedStr)
	{
		char *destPtr;
		for (destPtr = escapedStr; *str; str++)
		{
			if (*str == '"')
				*destPtr++ = '\\';
			*destPtr++ = *str;
		}
		*destPtr = '\0';
	}
	return escapedStr;
}


MSG_SearchError MSG_SearchTerm::OutputValue(XPStringObj &outputStr)
{
	if (IsStringAttribute(m_attribute))
	{
		XP_Bool	quoteVal = FALSE;
		// need to quote strings with ')' - filter code will escape quotes
		if (XP_STRCHR(m_value.u.string, ')'))
		{
			quoteVal = TRUE;
			outputStr += "\"";
		}
		if (XP_STRCHR(m_value.u.string, '"'))
		{
			char *escapedString = MSG_SearchTerm::EscapeQuotesInStr(m_value.u.string);
			if (escapedString)
			{
				outputStr += escapedString;
				XP_FREE(escapedString);
			}

		}
		else
		{
			outputStr += m_value.u.string;
		}
		if (quoteVal)
			outputStr += "\"";
	}
	else
	{
	    switch (m_attribute)
		{
		case attribDate:
		{
			struct tm *p = localtime(&m_value.u.date);
			if (p)
			{
				// wow, so tm_mon is 0 based, tm_mday is 1 based.
				char dateBuf[100];
				strftime (dateBuf, sizeof(dateBuf), "%d-%b-%Y", p);
				outputStr += dateBuf;
			}
			else 
				outputStr += "01/01/70";
			break;
		}
		case attribMsgStatus:
		{
			char status[40];
			MSG_GetStatusName (m_value.u.msgStatus, status, sizeof(status));
			outputStr += status;
			break;
		}
		case attribPriority:
		{
			char priority[40];
			MSG_GetUntranslatedPriorityName( m_value.u.priority, 
											 priority, 
											 sizeof(priority));
			outputStr += priority;
			break;
		}
		default:
			XP_ASSERT(FALSE);
			break;
		}
	}
	return SearchError_Success;
}

MSG_SearchError MSG_SearchTerm::EnStreamNew (char **outStream, int16 *length)
{
	const char	*attrib, *operatorStr;
	XPStringObj	outputStr;
	MSG_SearchError	ret;

	ret = MSG_GetStringForAttribute(m_attribute, &attrib);
	if (ret != SearchError_Success)
		return ret;

	if (m_attribute == attribOtherHeader)  // if arbitrary header, use it instead!
	{
		outputStr = "\"";
		if (m_arbitraryHeader)
			outputStr += m_arbitraryHeader;
		outputStr += "\"";
	}
	else
		outputStr = attrib;

	outputStr += ',';

	ret = MSG_GetStringForOperator(m_operator, &operatorStr);
	if (ret != SearchError_Success)
		return ret;

	outputStr += operatorStr;
	outputStr += ',';

	OutputValue(outputStr);
	*length = XP_STRLEN(outputStr);
	*outStream = (char *) XP_ALLOC(*length + 1);
	if (*outStream)
	{
		XP_STRCPY(*outStream, outputStr);
		return SearchError_Success;
	}
	else
		return SearchError_OutOfMemory;
}

// fill in m_value from the input stream.
MSG_SearchError MSG_SearchTerm::ParseValue(char *inStream)
{
	if (IsStringAttribute(m_attribute))
	{
		XP_Bool	quoteVal = FALSE;
		while (XP_IS_SPACE(*inStream))
			inStream++;

		// need to remove pair of '"', if present
		if (*inStream == '"')
		{
			quoteVal = TRUE;
			inStream++;
		}
		int valueLen = XP_STRLEN(inStream);
		if (quoteVal && inStream[valueLen - 1] == '"')
			valueLen--;

		m_value.u.string = (char *) XP_ALLOC(valueLen + 1);
		XP_STRNCPY_SAFE(m_value.u.string, inStream, valueLen + 1);
	}
	else
	{
	    switch (m_attribute)
		{
		case attribDate:
			m_value.u.date = XP_ParseTimeString (inStream, FALSE);
			break;
		case attribMsgStatus:
			m_value.u.msgStatus = MSG_GetStatusValueFromName(inStream);
			break;
		case attribPriority:
			m_value.u.priority = MSG_GetPriorityFromString(inStream);
			break;
		default:
			XP_ASSERT(FALSE);
			break;
		}
	}
	m_value.attribute = m_attribute;
	return SearchError_Success;
}

// find the operator code for this operator string.
MSG_SearchOperator MSG_SearchTerm::ParseOperator(char *inStream)
{
	int16				operatorVal;
	MSG_SearchError		err;

	while (XP_IS_SPACE(*inStream))
		inStream++;

	char *commaSep = XP_STRCHR(inStream, ',');

	if (commaSep)
		*commaSep = '\0';

	err = MSG_GetOperatorFromString(inStream, &operatorVal);
	return (MSG_SearchOperator) operatorVal;
}

// find the attribute code for this comma-delimited attribute. 
MSG_SearchAttribute MSG_SearchTerm::ParseAttribute(char *inStream)
{
	XPStringObj			attributeStr;
	int16				attributeVal;
	MSG_SearchError		err;

	while (XP_IS_SPACE(*inStream))
		inStream++;

	// if we are dealing with an arbitrary header, it may be quoted....
	XP_Bool quoteVal = FALSE;
	if (*inStream == '"')
	{
		quoteVal = TRUE;
		inStream++;
	}

	char *separator;
	if (quoteVal)      // arbitrary headers are quoted...
		separator = XP_STRCHR(inStream, '"');
	else
		separator = XP_STRCHR(inStream, ',');
	
	if (separator)
		*separator = '\0';

	err = MSG_GetAttributeFromString(inStream, &attributeVal);
	MSG_SearchAttribute attrib = (MSG_SearchAttribute) attributeVal;
	
	if (attrib == attribOtherHeader)  // if we are dealing with an arbitrary header....
	{
		if (m_arbitraryHeader)
			XP_FREE(m_arbitraryHeader);
		m_arbitraryHeader =  (char *) XP_ALLOC(XP_STRLEN(inStream)+1);
		if (m_arbitraryHeader)
		{
			m_arbitraryHeader[0] = '\0';
			XP_STRCAT(m_arbitraryHeader, inStream);
		}
	}
	
	return attrib;
}

// De stream one search term. If the condition looks like
// condition = "(to or CC, contains, r-thompson) AND (body, doesn't contain, fred)"
// This routine should get called twice, the first time
// with "to or CC, contains, r-thompson", the second time with
// "body, doesn't contain, fred"

MSG_SearchError MSG_SearchTerm::DeStreamNew (char *inStream, int16 /*length*/)
{
	char *commaSep = XP_STRCHR(inStream, ',');
	m_attribute = ParseAttribute(inStream);  // will allocate space for arbitrary header if necessary
	if (!commaSep)
		return SearchError_InvalidSearchTerm;
	char *secondCommaSep = XP_STRCHR(commaSep + 1, ',');
	if (commaSep)
		m_operator = ParseOperator(commaSep + 1);
	if (secondCommaSep)
		ParseValue(secondCommaSep + 1);
	return SearchError_Success;
}


void MSG_SearchTerm::StripQuotedPrintable (unsigned char *src)
{
	// decode quoted printable text in place

	unsigned char *dest = src;
	int srcIdx = 0, destIdx = 0;
	
	while (src[srcIdx] != 0)
	{
		if (src[srcIdx] == '=')
		{
			unsigned char *token = &src[srcIdx];
			unsigned char c = 0;

			// decode the first quoted char
			if (token[1] >= '0' && token[1] <= '9')
				c = token[1] - '0';
			else if (token[1] >= 'A' && token[1] <= 'F')
				c = token[1] - ('A' - 10);
			else if (token[1] >= 'a' && token[1] <= 'f')
				c = token[1] - ('a' - 10);
			else
			{
				// first char after '=' isn't hex. copy the '=' as a normal char and keep going
				dest[destIdx++] = src[srcIdx++]; // aka token[0]
				continue;
			}
			
			// decode the second quoted char
			c = (c << 4);
			if (token[2] >= '0' && token[2] <= '9')
				c += token[2] - '0';
			else if (token[2] >= 'A' && token[2] <= 'F')
				c += token[2] - ('A' - 10);
			else if (token[2] >= 'a' && token[2] <= 'f')
				c += token[2] - ('a' - 10);
			else
			{
				// second char after '=' isn't hex. copy the '=' as a normal char and keep going
				dest[destIdx++] = src[srcIdx++]; // aka token[0]
				continue;
			}

			// if we got here, we successfully decoded a quoted printable sequence,
			// so bump each pointer past it and move on to the next char;
			dest[destIdx++] = c; 
			srcIdx += 3;

		}
		else
			dest[destIdx++] = src[srcIdx++];
	}

	dest[destIdx] = src[srcIdx]; // null terminate
}

// Looks in the MessageDB for the user specified arbitrary header, if it finds the header, it then looks for a match against
// the value for the header. 
MSG_SearchError MSG_SearchTerm::MatchArbitraryHeader (MSG_ScopeTerm *scope, uint32 offset, uint32 length /* in lines*/, int16 foldcsid,
														DBMessageHdr *msg, MessageDB * db, char * headers, uint32 headersSize, XP_Bool ForFiltering)
{
	if (!m_arbitraryHeader)
		return SearchError_NotAMatch;

	MSG_SearchError err = SearchError_NotAMatch;
	MSG_BodyHandler * bodyHan = new MSG_BodyHandler (scope, offset,length, msg, db, headers, headersSize, ForFiltering);
	if (!bodyHan)
		return SearchError_OutOfMemory;

	bodyHan->SetStripHeaders (FALSE);

	if (MatchAllBeforeDeciding())
		err = SearchError_Success;
	else
		err = SearchError_NotAMatch;

	const int kBufSize = 512; // max size of a line??
	char * buf = (char *) XP_ALLOC(kBufSize);
	if (buf)
	{
		XP_Bool searchingHeaders = TRUE;
		while (searchingHeaders && bodyHan->GetNextLine(buf, kBufSize))
		{
			if (!XP_STRNCASECMP(buf,m_arbitraryHeader, XP_STRLEN(m_arbitraryHeader)))
			{
				if (XP_STRLEN(m_arbitraryHeader) < XP_STRLEN(buf)) // make sure buf has info besides just the header
				{
					MSG_SearchError err2 = MatchString(buf+XP_STRLEN(m_arbitraryHeader), foldcsid);  // match value with the other info...
					if (err != err2) // if we found a match
					{
						searchingHeaders = FALSE;   // then stop examining the headers
						err = err2;
					}
				}
			}
			if (buf[0] == CR || buf[0] == LF || buf[0] == '\0')
				searchingHeaders = FALSE;
		}
		delete bodyHan;
		XP_FREE(buf); 
		return err;
	}
	else
	{
		delete bodyHan;
		return SearchError_OutOfMemory;
	}
}

MSG_SearchError MSG_SearchTerm::MatchBody (MSG_ScopeTerm *scope, uint32 offset, uint32 length /*in lines*/, int16 foldcsid,
										   DBMessageHdr *msg, MessageDB * db)
{
	MSG_SearchError err = SearchError_NotAMatch;

	// Small hack so we don't look all through a message when someone has
	// specified "BODY IS foo"
	if ((length > 0) && (m_operator == opIs || m_operator == opIsnt))
		length = XP_STRLEN (m_value.u.string);

	MSG_BodyHandler * bodyHan  = new MSG_BodyHandler (scope, offset, length, msg, db);
	if (!bodyHan)
		return SearchError_OutOfMemory;

	const int kBufSize = 512; // max size of a line???
	char *buf = (char*) XP_ALLOC(kBufSize);
	if (buf)
	{
		XP_Bool endOfFile = FALSE;  // if retValue == 0, we've hit the end of the file
		uint32 lines = 0;

		CCCDataObject conv = INTL_CreateCharCodeConverter();
		XP_Bool getConverter = FALSE;
		int16 win_csid = INTL_DocToWinCharSetID(foldcsid);
		int16 mail_csid = INTL_DefaultMailCharSetID(win_csid);    // to default mail_csid (e.g. JIS for Japanese)
		if ((NULL != conv) && INTL_GetCharCodeConverter(mail_csid, win_csid, conv)) 
			getConverter = TRUE;

		// Change the sense of the loop so we don't bail out prematurely
		// on negative terms. i.e. opDoesntContain must look at all lines
		MSG_SearchError errContinueLoop;
		if (MatchAllBeforeDeciding())
			err = errContinueLoop = SearchError_Success;
		else
			err = errContinueLoop = SearchError_NotAMatch;

		// If there's a '=' in the search term, then we're not going to do
		// quoted printable decoding. Otherwise we assume everything is
		// quoted printable. Obviously everything isn't quoted printable, but
		// since we don't have a MIME parser handy, and we want to err on the
		// side of too many hits rather than not enough, we'll assume in that
		// general direction. ### FIX ME 
		// bug fix #88935: for stateful csids like JIS, we don't want to decode
		// quoted printable since it contains '='.
		XP_Bool isQuotedPrintable = !(mail_csid & STATEFUL) &&
										(XP_STRCHR (m_value.u.string, '=') == NULL);

		while (!endOfFile && err == errContinueLoop)
		{
			if (bodyHan->GetNextLine(buf, kBufSize))
			{
				// Do in-place decoding of quoted printable
				if (isQuotedPrintable)
					StripQuotedPrintable ((unsigned char*)buf);

				char *compare = buf;
				if (getConverter) 
				{	
					// In here we do I18N conversion if we get the converter
					char *newBody = NULL;
					newBody = (char *)INTL_CallCharCodeConverter(conv, (unsigned char *) buf, (int32) XP_STRLEN(buf));
					if (newBody && (newBody != buf))
					{
						// CharCodeConverter return the char* to the orginal string
						// we don't want to free body in that case 
						compare = newBody;
					}
				}
				if (*compare && *compare != CR && *compare != LF)
				{
					err = MatchString (compare, win_csid, TRUE);
					lines++; 
				}
				if (compare != buf)
					XP_FREEIF(compare);
			}
			else 
				endOfFile = TRUE;
		}

		if(conv) 
			INTL_DestroyCharCodeConverter(conv);
		XP_FREEIF(buf);
		delete bodyHan;
	}
	else
		err = SearchError_OutOfMemory;
	return err;
}


MSG_SearchError MSG_SearchTerm::MatchString (const char *stringToMatch, int16 csid, XP_Bool body)
{
	MSG_SearchError err = SearchError_NotAMatch;
	unsigned char* n_str = NULL;
	unsigned char* n_header = NULL;
	if(opIsEmpty != m_operator)	// Save some performance for opIsEmpty
	{
		n_str = INTL_GetNormalizeStr(csid , (unsigned char*)m_value.u.string);	// Always new buffer unless not enough memory
		if (!body)
			n_header = INTL_GetNormalizeStrFromRFC1522(csid , (unsigned char*)stringToMatch);	// Always new buffer unless not enough memory
		else
			n_header = INTL_GetNormalizeStr(csid , (unsigned char*)stringToMatch);	// Always new buffer unless not enough memory

		XP_ASSERT(n_str);
		XP_ASSERT(n_header);
	}
	switch (m_operator)
	{
	case opContains:
		if((NULL != n_str) && (NULL != n_header) && (n_str[0]) && INTL_StrContains(csid, n_header, n_str))
			err = SearchError_Success;
		break;
	case opDoesntContain:
		if((NULL != n_str) && (NULL != n_header) && (n_str[0]) && (! INTL_StrContains(csid, n_header, n_str)))
			err = SearchError_Success;
		break;
	case opIs:
		if(n_str && n_header)
		{
			if (n_str[0])
			{
				if (INTL_StrIs(csid, n_header, n_str))
					err = SearchError_Success;
			}
			else if (n_header[0] == '\0') // Special case for "is <the empty string>"
				err = SearchError_Success;
		}
		break;
	case opIsnt:
		if(n_str && n_header)
		{
			if (n_str[0])
			{
				if (! INTL_StrIs(csid, n_header, n_str))
					err = SearchError_Success;
			}
			else if (n_header[0] != '\0') // Special case for "isn't <the empty string>"
				err = SearchError_Success;
		}
		break;
	case opIsEmpty:
		if (!stringToMatch || stringToMatch[0] == '\0')
			err = SearchError_Success;
		break;
	case opBeginsWith:
		if((NULL != n_str) && (NULL != n_header) && INTL_StrBeginWith(csid, n_header, n_str))
			err = SearchError_Success;
		break;
	case opEndsWith: 
		{
		if((NULL != n_str) && (NULL != n_header) && INTL_StrEndWith(csid, n_header, n_str))
			err = SearchError_Success;
		}
		break;
	default:
		XP_ASSERT(FALSE);
	}

	if(n_str)	// Need to free the normalized string 
		XP_FREE(n_str);

	if(n_header)	// Need to free the normalized string 
		XP_FREE(n_header);

	return err;
}

XP_Bool MSG_SearchTerm::MatchAllBeforeDeciding ()
{
	if (m_operator == opDoesntContain || m_operator == opIsnt)
		return TRUE;
	return FALSE;
}


MSG_SearchError MSG_SearchTerm::MatchRfc822String (const char *string, int16 csid)
{
	// Isolate the RFC 822 parsing weirdnesses here. MSG_ParseRFC822Addresses
	// returns a catenated string of null-terminated strings, which we walk
	// across, tring to match the target string to either the name OR the address

	char *names = NULL, *addresses = NULL;

	// Change the sense of the loop so we don't bail out prematurely
	// on negative terms. i.e. opDoesntContain must look at all recipients
	MSG_SearchError err;
	MSG_SearchError errContinueLoop;
	if (MatchAllBeforeDeciding())
		err = errContinueLoop = SearchError_Success;
	else
		err = errContinueLoop = SearchError_NotAMatch;

	int count = MSG_ParseRFC822Addresses (string, &names, &addresses);
	if (count > 0)
	{
		XP_ASSERT(names);
		XP_ASSERT(addresses);
		if (!names || !addresses)
			return err;

		char *walkNames = names;
		char *walkAddresses = addresses;

		for (int i = 0; i < count && err == errContinueLoop; i++)
		{
			err = MatchString (walkNames, csid);
			if (errContinueLoop == err)
				err = MatchString (walkAddresses, csid);

			walkNames += XP_STRLEN(walkNames) + 1;
			walkAddresses += XP_STRLEN(walkAddresses) + 1;
		}

		XP_FREEIF(names);
		XP_FREEIF(addresses);
	}

	return err;
}


MSG_SearchError MSG_SearchTerm::GetLocalTimes (time_t a, time_t b, struct tm &aTm, struct tm &bTm)
{
	// Isolate the RTL time weirdnesses here: 
	// (1) Must copy the tm since localtime has a static tm
	// (2) localtime can fail if it doesn't like the time_t. Must check the tm* for NULL

	struct tm *p = localtime(&a);
	if (p)
	{
		XP_MEMCPY (&aTm, p, sizeof(struct tm));
		p = localtime(&b);
		if (p) 
		{
			XP_MEMCPY (&bTm, p, sizeof(struct tm));
			return SearchError_Success;
		}
	}
	return SearchError_InvalidAttribute;
}


MSG_SearchError MSG_SearchTerm::MatchDate (time_t dateToMatch)
{
	MSG_SearchError err = SearchError_NotAMatch;
	switch (m_operator)
	{
	case opIsBefore:
		if (dateToMatch < m_value.u.date)
			err = SearchError_Success;
		break;
	case opIsAfter:
		if (dateToMatch > m_value.u.date)
			err = SearchError_Success;
		break;
	case opIs:
		{
			struct tm tmToMatch, tmThis;
			if (SearchError_Success == GetLocalTimes (dateToMatch, m_value.u.date, tmToMatch, tmThis))
			{
				if (tmThis.tm_year == tmToMatch.tm_year &&
					tmThis.tm_mon == tmToMatch.tm_mon &&
					tmThis.tm_mday == tmToMatch.tm_mday)
					err = SearchError_Success;
			}
		}
		break;
	case opIsnt:
		{
			struct tm tmToMatch, tmThis;
			if (SearchError_Success == GetLocalTimes (dateToMatch, m_value.u.date, tmToMatch, tmThis))
			{
				if (tmThis.tm_year != tmToMatch.tm_year ||
					tmThis.tm_mon != tmToMatch.tm_mon ||
					tmThis.tm_mday != tmToMatch.tm_mday)
					err = SearchError_Success;
			}
		}
		break;
	default:
		XP_ASSERT(FALSE);
	}
	return err;
}


MSG_SearchError MSG_SearchTerm::MatchAge (time_t msgDate)
{
	MSG_SearchError err = SearchError_NotAMatch;
	time_t now = XP_TIME();
	time_t matchDay = now - (m_value.u.age * 60 * 60 * 24);
	struct tm * matchTime = localtime(&matchDay);
	
	// localTime axes previous results so save these.
	int day = matchTime->tm_mday;
	int month = matchTime->tm_mon;
	int year = matchTime->tm_year;

	struct tm * msgTime = localtime(&msgDate);

	switch (m_operator)
	{
	case opIsGreaterThan: // is older than 
		if (msgDate < matchDay)
			err = SearchError_Success;
		break;
	case opIsLessThan: // is younger than 
		if (msgDate > matchDay)
			err = SearchError_Success;
		break;
	case opIs:
		if (matchTime && msgTime)
			if ((day == msgTime->tm_mday) 
				&& (month == msgTime->tm_mon)
				&& (year == msgTime->tm_year))
				err = SearchError_Success;
		break;
	default:
		XP_ASSERT(FALSE);
	}

	return err;
}


MSG_SearchError MSG_SearchTerm::MatchSize (uint32 sizeToMatch)
{
	MSG_SearchError err = SearchError_NotAMatch;
	switch (m_operator)
	{
	case opIsHigherThan:
		if (sizeToMatch > m_value.u.size)
			err = SearchError_Success;
		break;
	case opIsLowerThan:
		if (sizeToMatch < m_value.u.size)
			err = SearchError_Success;
		break;
	default:
		break;
	}
	return err;
}


MSG_SearchError MSG_SearchTerm::MatchStatus (uint32 statusToMatch)
{
	MSG_SearchError err = SearchError_NotAMatch;
	XP_Bool matches = FALSE;

	if (statusToMatch & m_value.u.msgStatus)
		matches = TRUE;

	switch (m_operator)
	{
	case opIs:
		if (matches)
			err = SearchError_Success;
		break;
	case opIsnt:
		if (!matches)
			err = SearchError_Success;
		break;
	default:
		XP_ASSERT(FALSE);
	}

	return err;	
}


MSG_SearchError MSG_SearchTerm::MatchPriority (MSG_PRIORITY priorityToMatch)
{
	MSG_SearchError err = SearchError_NotAMatch;

	// Use this ittle hack to get around the fact that enums don't have
	// integer compare operators
	int p1 = (int) priorityToMatch;
	int p2 = (int) m_value.u.priority;

	switch (m_operator)
	{
	case opIsHigherThan:
		if (p1 > p2)
			err = SearchError_Success;
		break;
	case opIsLowerThan:
		if (p1 < p2)
			err = SearchError_Success;
		break;
	case opIs:
		if (p1 == p2)
			err = SearchError_Success;
		break;
	default:
		XP_ASSERT(FALSE);
	}
	return err;
}

//-----------------------------------------------------------------------------
//-------------------- Implementation of MSG_ScopeTerm ------------------------
//-----------------------------------------------------------------------------

MSG_ScopeTerm::MSG_ScopeTerm (MSG_SearchFrame *frame, MSG_ScopeAttribute attrib, MSG_FolderInfo *folder) : msg_OpaqueObject (m_expectedMagic)
{
	m_attribute = attrib;
	m_folder = folder;
	m_adapter = NULL;
	m_frame = frame;
	m_name = NULL;
	m_server = NULL;
	m_file = 0;

	m_searchServer = TRUE; 
	MSG_LinedPane * pane = m_frame ? m_frame->GetPane() : (MSG_LinedPane*)NULL;
	if (pane)
		m_searchServer = pane->GetPrefs()->GetSearchServer(); // should the scope term be local or on the server?

}


MSG_ScopeTerm::MSG_ScopeTerm (MSG_SearchFrame *frame, DIR_Server *server) : msg_OpaqueObject (m_expectedMagic)
{
	m_server = server;
	DIR_IncrementServerRefCount (m_server);
	m_attribute = scopeLdapDirectory;
	m_adapter = NULL;
	m_frame = frame;
	m_name = NULL;
	m_folder = NULL;
	m_file = NULL;

	m_searchServer = TRUE; 
	MSG_LinedPane * pane = m_frame ? m_frame->GetPane() : (MSG_LinedPane *)NULL;
	if (pane)
		m_searchServer = pane->GetPrefs()->GetSearchServer(); // should the scope term be local or on the server?

}


MSG_ScopeTerm::~MSG_ScopeTerm () 
{
	if (NULL != m_name)
		XP_FREE(m_name);
	if (NULL != m_adapter)
		delete m_adapter;
	if (m_server)
		DIR_DecrementServerRefCount (m_server);
	if (m_file)
		XP_FileClose (m_file);
}


MSG_SearchError MSG_ScopeTerm::InitializeAdapter (MSG_SearchTermArray &termList)
{
	XP_ASSERT (m_adapter == NULL);
	MSG_SearchError err = SearchError_Success;

	// mscott: i have added m_searchServer into this switch to take into account the user's preference
	// for searching locally or on the server...
	switch (m_attribute)
	{
		case scopeMailFolder:    
			if (m_folder->GetType() == FOLDER_IMAPMAIL && (NET_IsOffline() || !m_searchServer))    // Are we in Offline IMAP mode? 
					m_adapter = new msg_SearchIMAPOfflineMail (this, termList);
				else
					if (!IsOfflineMail() && m_searchServer)   // Online IMAP && searching the server?
				       m_adapter = new msg_SearchOnlineMail (this, termList);
				else
				m_adapter = new msg_SearchOfflineMail (this, termList);
			break;
		case scopeNewsgroup:
			if (NET_IsOffline() || !m_searchServer)
				m_adapter = new msg_SearchOfflineNews (this, termList);
			else
			if (m_folder->KnowsSearchNntpExtension())
				m_adapter = new msg_SearchNewsEx (this, termList);
			else
				m_adapter = new msg_SearchNews (this, termList);
			break;
		case scopeAllSearchableGroups:
			if (!m_searchServer)
				m_adapter = new msg_SearchOfflineNews(this, termList);  // mscott: is this the behavior we want?
			else
				m_adapter = new msg_SearchNewsEx (this, termList);
			break;
		case scopeLdapDirectory:
			m_adapter = new msg_SearchLdap (this, termList);
			break;
		case scopeOfflineNewsgroup:
			m_adapter = new msg_SearchOfflineNews (this, termList);
			break;
		default:
			XP_ASSERT(FALSE);
			err = SearchError_InvalidScope;
	}

	if (m_adapter)
		err = m_adapter->ValidateTerms ();

	return err;
}


XP_Bool MSG_ScopeTerm::IsOfflineMail ()
{
	// Find out whether "this" mail folder is online or offline
	XP_ASSERT(m_folder);
	if (m_folder->GetType() == FOLDER_IMAPMAIL && !NET_IsOffline() && m_searchServer)    // make sure we are not in offline IMAP (mscott)
		return FALSE;
	return TRUE;  // if POP or IMAP in offline mode
}

XP_Bool MSG_ScopeTerm::IsOfflineIMAPMail()
{
	// Find out whether "this" mail folder is an offline IMAP folder
	XP_ASSERT(m_folder);
	if (m_folder->GetType() == FOLDER_IMAPMAIL && (NET_IsOffline() || !m_searchServer))
		return TRUE;
	return FALSE;       // we are not an IMAP folder that is offline
}


const char *MSG_ScopeTerm::GetMailPath ()
{
	// if we are POP or OFFLINE IMAP, then we have a Pathname which can be generated (mscott)
	if (IsOfflineMail())
//	if (FOLDER_MAIL == m_folder->GetType())
		return ((MSG_FolderInfoMail*) m_folder)->GetPathname();
	return NULL;
}


MSG_SearchError MSG_ScopeTerm::TimeSlice ()
{
	// For scope terms, time slice means give some processing time to
	// the adapter we own
	return m_adapter->Search();
}


char *MSG_ScopeTerm::GetStatusBarName ()
{
	switch (m_attribute)
	{
	case scopeLdapDirectory:
		if (m_server->description)
			return XP_STRDUP(m_server->description);
		else
			return XP_STRDUP(m_server->serverName);
	default:
		{
			char *statusBarName = XP_STRDUP(m_folder->GetName ());
			NET_UnEscape(statusBarName); // Mac apparently stores as "foo%20bar"
			return statusBarName;
		}
	}
}


//-----------------------------------------------------------------------------
// MSG_SearchFrame is a search session. There is one pane object per running
// customer of search (mail/news, ldap, rules, etc.)
//-----------------------------------------------------------------------------

MSG_SearchFrame::MSG_SearchFrame (MSG_LinedPane *pane) : msg_OpaqueObject (m_expectedMagic)
{
	// link in the searchFrame with the pane and context
	m_pane = pane;
	MWContext *context = pane->GetContext();
	XP_ASSERT(context);
	context->msg_searchFrame = this;

	m_sortAttribute = attribSender;
	m_descending = FALSE;
	m_ldapObjectClass = NULL;
	m_idxRunningScope = 0; 
	m_parallel = FALSE;
	m_calledStartingUpdate = FALSE;
	m_handlingError = FALSE;
	m_urlStruct = NULL;

	m_searchType = searchNormal;
	m_pSearchParam = NULL;

	m_inCylonMode = FALSE;
	m_cylonModeTimer = NULL;

	m_offlineProgressTotal = 0;
	m_offlineProgressSoFar = 0;
}


MSG_SearchFrame::~MSG_SearchFrame ()
{
	DestroyResultList ();
	DestroyScopeList ();
	DestroyTermList ();
	XP_ASSERT(m_viewList.GetSize() == 0);  // are all of our views closed? (Search As View related)
	DestroySearchViewList(); // what if we have open views before we kill this?

	if (m_ldapObjectClass)
		XP_FREE (m_ldapObjectClass);

    if (m_pSearchParam)
	    XP_FREE (m_pSearchParam);

	if (m_inCylonMode)
		EndCylonMode();
}


MSG_SearchFrame *MSG_SearchFrame::FromContext (MWContext *context)
{
	if (!context)
		return NULL;
	MSG_SearchFrame *frame = context->msg_searchFrame;
	if (!frame || !frame->IsValid())
		return NULL;
	return frame;
}


MSG_SearchFrame *MSG_SearchFrame::FromPane (MSG_Pane *pane)
{
	if (!pane)
		return NULL;
	return FromContext (pane->GetContext());
}


MSG_SearchError MSG_SearchFrame::Initialize ()
{
	// Loop over scope terms, initializing an adapter per term. This 
	// architecture is necessitated by two things: 
	// 1. There might be more than one kind of adapter per if online 
	//    *and* offline mail mail folders are selected, or if newsgroups
	//    belonging to RFC977bis *and* non-RFC977bis are selected
	// 2. Most of the protocols are only capable of searching one scope at a
	//    time, so we'll do each scope in a separate adapter on the client

	MSG_ScopeTerm *scopeTerm = NULL;
	MSG_SearchError err = SearchError_Success;

	// Ensure that the FE has added scopes and terms to this search
	XP_ASSERT(m_termList.GetSize() > 0);
	if (m_scopeList.GetSize() == 0 || m_termList.GetSize() == 0)
		return SearchError_InvalidPane;

	// If this term list (loosely specified here by the first term) should be
	// scheduled in parallel, build up a list of scopes to do the round-robin scheduling
	scopeTerm = m_scopeList.GetAt(0);
	if (scopeTerm->m_attribute == scopeLdapDirectory)
		m_parallel = TRUE;

	for (int i = 0; i < m_scopeList.GetSize() && err == SearchError_Success; i++)
	{
		scopeTerm = m_scopeList.GetAt(i);
		XP_ASSERT(scopeTerm->IsValid());

		err = scopeTerm->InitializeAdapter (m_termList);
		if (SearchError_Success == err && m_parallel)
			m_parallelScopes.Add (scopeTerm);

		if (scopeTerm->m_attribute != scopeLdapDirectory && scopeTerm->m_folder->GetType() == FOLDER_MAIL)
			m_offlineProgressTotal += scopeTerm->m_folder->GetTotalMessages();
	}

	return err;
}


MSG_SearchError MSG_SearchFrame::BeginSearching ()
{
	MSG_SearchError err = SearchError_Success;

	// Here's a way to start the URL, but I don't really have time to
	// unify the scheduling mechanisms. If the first scope is a newsgroup, and
	// it's not RFC977bis-capable, we build the URL queue. All other searches can be
	// done with one URL

	MSG_ScopeTerm *scope = m_scopeList.GetAt(0);
	if (scope->m_attribute == scopeNewsgroup && !scope->m_folder->KnowsSearchNntpExtension() && scope->m_searchServer)
		err = BuildUrlQueue (&msg_SearchNewsEx::PreExitFunction);
	else if (scope->m_attribute == scopeMailFolder && !scope->IsOfflineMail())
		err = BuildUrlQueue (&msg_SearchOnlineMail::PreExitFunction);
	else
		err = GetUrl();

	return err;
}


MSG_SearchError MSG_SearchFrame::BuildUrlQueue (Net_GetUrlExitFunc *exitFunction)
{
	MSG_UrlQueue *queue = new MSG_UrlQueue (m_pane);

	for (int i = 0; i < m_scopeList.GetSize(); i++)
	{
		msg_SearchAdapter *adapter = m_scopeList.GetAt(i)->m_adapter;
		queue->AddUrl (adapter->GetEncoding(), exitFunction);
	}

	if (queue->GetSize() > 0)
		BeginCylonMode();

	queue->GetNextUrl();

	return SearchError_Success;
}


MSG_SearchError MSG_SearchFrame::GetUrl ()
{
	MSG_SearchError err = SearchError_Success;
	if (!m_urlStruct)
		m_urlStruct = NET_CreateURLStruct ("search-libmsg:", NET_DONT_RELOAD);

	if (m_urlStruct)
	{
		// Set the internal_url flag so just in case someone else happens to have
		// a search-libmsg URL, it won't fire my code, and surely crash.
		m_urlStruct->internal_url = TRUE;

		// Initiate the asynchronous search
		int getUrlErr = m_pane->GetURL (m_urlStruct, FALSE);
		if (getUrlErr)
			err = (MSG_SearchError) -1; //###phil impedance mismatch
		else 
			if (!XP_STRNCMP(m_urlStruct->address, "news:", 5) || !XP_STRNCMP(m_urlStruct->address, "snews:", 6))
				BeginCylonMode();
	}
	else
		err = SearchError_OutOfMemory;

	return err;
}


void MSG_SearchFrame::DestroySearchViewList()
{
	for (int i = 0; i < m_viewList.GetSize(); i++)
		delete m_viewList.GetAt(i);
}

void MSG_SearchFrame::DestroyResultList ()
{
	MSG_ResultElement *result = NULL;
	for (int i = 0; i < m_resultList.GetSize(); i++)
	{
		result = m_resultList.GetAt(i);
		XP_ASSERT (result->IsValid());
		delete result;
	}
}


void MSG_SearchFrame::DestroyScopeList()
{
	MSG_ScopeTerm *scope = NULL;
	for (int i = 0; i < m_scopeList.GetSize(); i++)
	{
		scope = m_scopeList.GetAt(i);
		XP_ASSERT (scope->IsValid());
		delete scope;
	}
}


void MSG_SearchFrame::DestroyTermList ()
{
	MSG_SearchTerm *term = NULL;
	for (int i = 0; i < m_termList.GetSize(); i++)
	{
		term = m_termList.GetAt(i);
		XP_ASSERT (term->IsValid());
		delete term;
	}
}


MSG_SearchError MSG_SearchFrame::AddSearchTerm (MSG_SearchAttribute attrib, MSG_SearchOperator op, MSG_SearchValue *value, XP_Bool BooleanAND, 
												char * arbitraryHeader)
{
	MSG_SearchTerm *pTerm = new MSG_SearchTerm (attrib, op, value, BooleanAND, arbitraryHeader);
	if (NULL == pTerm)
		return SearchError_OutOfMemory;
	m_termList.Add (pTerm);
	return SearchError_Success;
}

void *MSG_SearchFrame::GetSearchParam()
{
	return m_pSearchParam;
}

MSG_SearchType MSG_SearchFrame::GetSearchType()
{
	return m_searchType;
}

MSG_SearchError MSG_SearchFrame::SetSearchParam(MSG_SearchType type, void *param)
{
	if (m_pSearchParam != NULL && m_pSearchParam != param)
	{
		if (m_searchType == searchLdapVLV)
		{
			XP_FREEIF(((LDAPVirtualList *)m_pSearchParam)->ldvlist_attrvalue);
			XP_FREEIF(((LDAPVirtualList *)m_pSearchParam)->ldvlist_extradata);
		}
		XP_FREE(m_pSearchParam);
	}

	m_searchType = type;
	m_pSearchParam = param;

	return SearchError_Success;
}

MSG_SearchError MSG_SearchFrame::AddScopeTerm (MSG_ScopeAttribute attrib, MSG_FolderInfo *folder)
{
	if (attrib != scopeAllSearchableGroups)
	{
		XP_ASSERT(folder);
		if (!folder)
			return SearchError_NullPointer;
	}

	MSG_SearchError err = SearchError_Success;

	if (attrib == scopeMailFolder)
	{
		// It's legal to have a folderInfo which is only a directory, but has no
		// mail folder or summary file. However, such a folderInfo isn't a legal 
		// scopeTerm, so turn it away here
		MSG_FolderInfoMail *mailFolder = mailFolder = folder->GetMailFolderInfo();
		XP_StatStruct fileStat;
		if (mailFolder && !XP_Stat(mailFolder->GetPathname(), &fileStat, xpMailFolder) && S_ISDIR(fileStat.st_mode))
			err = SearchError_InvalidFolder;

		// IMAP folders can have a \NOSELECT flag which means that they can't
		// ever be opened. Since we have to SELECT a folder in order to search
		// it, we'll just omit this folder from the list of search scopes
		MSG_IMAPFolderInfoMail *imapFolder = folder->GetIMAPFolderInfoMail();
		if (imapFolder && !imapFolder->GetCanIOpenThisFolder())
			return SearchError_Success;
	}

	if ((attrib == scopeNewsgroup || attrib == scopeOfflineNewsgroup) && folder->IsNews())
	{
		// Even unsubscribed newsgroups have a folderInfo, so filter them
		// out here, adding only the newsgroups we are subscribed to
		MSG_FolderInfoNews * newsFolder = (MSG_FolderInfoNews *) folder;
		if (!newsFolder->IsSubscribed())
			return SearchError_Success;

		// It would be nice if the FEs did this, but I guess no one knows
		// that offline news searching is supposed to work
		if (NET_IsOffline())
			attrib = scopeOfflineNewsgroup;
	}

	if (attrib == scopeAllSearchableGroups)
	{
		// Try to be flexible about what we get here. It could be a news group,
		// news host, or NULL, which uses the default host.
		if (folder == NULL)
		{
			// I don't know how much of this can be NULL, so I'm not assuming anything
			MSG_NewsHost *host = NULL;
			msg_HostTable *table = m_pane->GetMaster()->GetHostTable();
			if (table)
			{
				host = table->GetDefaultHost(FALSE /*###tw*/);
				if (host)
					folder = host->GetHostInfo();
			}
		}
		else
		{
			switch (folder->GetType())
			{
			case FOLDER_CONTAINERONLY:
				break; // this is what we want -- nothing to do 
			case FOLDER_NEWSGROUP:
			case FOLDER_CATEGORYCONTAINER:
				{
					MSG_NewsHost *host = ((MSG_FolderInfoNews*) folder)->GetHost();
					folder = host->GetHostInfo();
				}
		    default:
			  break;
			}
		}
	}

	if (SearchError_Success == err)
	{
		MSG_ScopeTerm *pScope = new MSG_ScopeTerm (this, attrib, folder);
		if (pScope)
			m_scopeList.Add (pScope);
		else
			err = SearchError_OutOfMemory;
	}

	return err;
}


MSG_SearchError MSG_SearchFrame::AddScopeTerm (DIR_Server *server)
{
	MSG_ScopeTerm *pScope = new MSG_ScopeTerm (this, server);
	if (NULL == pScope)
		return SearchError_OutOfMemory;
	m_scopeList.Add (pScope);
	return SearchError_Success;
}


MSG_SearchError MSG_SearchFrame::SetLdapObjectClass (char *objectClass)
{
	m_ldapObjectClass = XP_STRDUP(objectClass);
	return SearchError_Success;
}


MSG_SearchError MSG_SearchFrame::AddLdapResultsToAddressBook (MSG_ViewIndex *indices, int count)
{
	MSG_SearchError err = SearchError_Success;
	MSG_UrlQueue *queue = new MSG_AddLdapToAddressBookQueue (m_pane);
	if (queue)
	{
		for (int i = 0; i < count; i++)
		{
			MSG_ResultElement *result = m_resultList.GetAt(indices[i]);
			char *url = NULL;
			MSG_SearchValue *dnValue = NULL;
			err = result->GetValue (attribDistinguishedName, &dnValue);
			if (SearchError_Success == err)
			{
				err = ((msg_SearchLdap*)result->m_adapter)->BuildUrl (dnValue->u.string, &url, TRUE /*addToAB*/);
				if (SearchError_Success == err)
					queue->AddUrl (url);
				FREEIF(url);
				MSG_DestroySearchValue (dnValue);
			}

		}
		queue->GetNextUrl();
	}
	return err;
}


MSG_SearchError MSG_SearchFrame::ComposeFromLdapResults (MSG_ViewIndex * indices, int count)
{
#define kMailToUrlScheme "mailto:"

	MSG_SearchError err = SearchError_Success;
	int32 urlLength = XP_STRLEN(kMailToUrlScheme);
	int i;

	char **addresses = (char**) XP_CALLOC(count, sizeof(char*));
	if (addresses)
	{
		// First build up an array of properly quoted names
		for (i = 0; i < count; i++)
		{
			MSG_ResultElement *elem = m_resultList.GetAt(indices[i]);

			const MSG_SearchValue *cnValue = elem->GetValueRef (attribCommonName);
			const MSG_SearchValue *mailValue = elem->GetValueRef (attrib822Address);
			if (cnValue && cnValue->u.string && mailValue && mailValue->u.string)
			{
				addresses[i] = MSG_MakeFullAddress (cnValue->u.string, mailValue->u.string);
				if (addresses[i])
					urlLength += XP_STRLEN (addresses[i]) + 1; //+1 for comma
			}
		}

		// Now catenate them all together
		char *mailtoUrl = (char*) XP_ALLOC (urlLength + 1); // +1 for null term
		if (mailtoUrl)
		{
			XP_STRCPY (mailtoUrl, kMailToUrlScheme);
			for (i = 0; i < count; i++)
			{
				if (addresses[i])
				{
					XP_STRCAT(mailtoUrl, addresses[i]);
					XP_FREE(addresses[i]);

					// If more than one address, separate with a comma
					if (count > 1 && i < count-1)
						XP_STRCAT(mailtoUrl, ",");
				}

			}

			// Send off the mailto URL
			URL_Struct *mailtoStruct = NET_CreateURLStruct (mailtoUrl, NET_DONT_RELOAD);
			if (mailtoStruct)
				m_pane->GetURL (mailtoStruct, FALSE);
			else
				err = SearchError_OutOfMemory;

			XP_FREE (mailtoUrl);
		}
		else
			err = SearchError_OutOfMemory;

		XP_FREE(addresses);
	}
	else
		err = SearchError_OutOfMemory;

	return err;
}


MSG_SearchError MSG_SearchFrame::AddAllScopes (MSG_Master *master, MSG_ScopeAttribute scope)
{
	MSG_SearchError err = SearchError_Success;
	MSG_FolderInfo *tree = NULL;
	switch (scope)
	{
	case scopeMailFolder:
		tree = master->GetLocalMailFolderTree();
		if (tree)
			err = AddScopeTree (scope, tree);
		else
		{
			XP_ASSERT(FALSE);
			err = SearchError_NullPointer;
		}
		break;
	case scopeNewsgroup:
		{
			msg_HostTable *table = master->GetHostTable();
			for (int i = 0; i < table->getNumHosts(); i++)
			{
				tree = table->getHost(i)->GetHostInfo();
				if (tree)
					AddScopeTree (scope, tree);
				else
				{
					XP_ASSERT(FALSE);
					err = SearchError_NullPointer;
				}
			}
		}
		break;
	default:
		err = SearchError_InvalidScope;
	}


	return err;
}


MSG_SearchError MSG_SearchFrame::AddScopeTree (MSG_ScopeAttribute scope, MSG_FolderInfo *tree, XP_Bool deep)
{
	MSG_SearchError err = SearchError_Success;
	if (tree->GetDepth() > 1)
	{
		if (scope == scopeMailFolder && tree->GetMailFolderInfo() != NULL) 
		{
			err = AddScopeTerm (scope, tree);

			// It's ok for 'tree' to be not searchable, but we should still
			// look down its subtree for searchable folders
			if (SearchError_InvalidFolder == err)
				err = SearchError_Success; 
		}
		else
			if ((scope == scopeNewsgroup || scope == scopeOfflineNewsgroup) && tree->IsNews())
				err = AddScopeTerm (scope, tree);

	}

	// okay, we want to skip this part if the user does not want us seaching sub folders / sub categories!
	XP_Bool searchSubFolders = m_pane->GetPrefs()->GetSearchSubFolders();
	
	if (SearchError_Success == err && searchSubFolders)
	{
		// Don't recurse the scopes for RFC977bis categories
		MSG_FolderInfoNews *newsTree = tree->GetNewsFolderInfo();
		// okay, FEs do not set the FolderInfo if we are in offline mode...i.e. newsTree
		// will still be listed as Xpat or NNTPExtensions for example. Of course, if we
		// are in Offline mode, we want to recurse each folderInfo!!
		if (newsTree && newsTree->KnowsSearchNntpExtension() && !NET_IsOffline())  // skip if offline mode....
			return err;
		// Don't recurse the scopes for RFC977bis news hosts
		else if (tree->KnowsSearchNntpExtension() && !NET_IsOffline()) // skip if offline mode...
			err = AddScopeTerm (scopeAllSearchableGroups, tree);
		else if (deep)
		{
			// It's a regular old mail folder or non-RFC977bis news host, so we 
			// recurse each folderInfo and build a new scope term for it.
			const MSG_FolderArray *subTree = tree->GetSubFolders();
			for (int i = 0; i < subTree->GetSize() && SearchError_Success == err; i++)
				err = AddScopeTree (scope, subTree->GetAt(i));
		}
	}

	return err;
}


MSG_SearchError MSG_SearchFrame::EncodeRFC977bisScopes (char **ppOutEncoding)
{
	MSG_SearchError err = SearchError_Success;
	MSG_ScopeTerm *firstScope = m_scopeList.GetAt(0);
	if (firstScope && scopeAllSearchableGroups == firstScope->m_attribute)
	{
		*ppOutEncoding = XP_STRDUP("\"*\"");			// NNTP protocol string -- do not localize
		if (!*ppOutEncoding)
			err = SearchError_OutOfMemory;
	}
	else 
	{
		int i;
		int cchNames = 0;
		for (i = 0; i < m_scopeList.GetSize(); i++)
		{
			MSG_FolderInfo *folder = m_scopeList.GetAt(i)->m_folder;
			int nameLen = XP_STRLEN (folder->GetName());
			cchNames += nameLen + 1; // Comma between names
			if (FOLDER_CATEGORYCONTAINER == folder->GetType())
			{
				cchNames += nameLen + 3; 
				cchNames += 1; // Searching category container "foo" means "foo,foo.*"
			}
		}

		*ppOutEncoding = (char*) XP_ALLOC(cchNames + 3);// Double-quotes and NULL term
		if (*ppOutEncoding)
		{
			XP_STRCPY (*ppOutEncoding, "\"");			// NNTP protocol string -- do not localize
			int count = m_scopeList.GetSize();
			for (i = 0; i < count; i++)
			{
				MSG_FolderInfo *folder = m_scopeList.GetAt(i)->m_folder;
				XP_STRCAT (*ppOutEncoding, folder->GetName());
				if (FOLDER_CATEGORYCONTAINER == folder->GetType())
				{
					XP_STRCAT(*ppOutEncoding, ",");
					XP_STRCAT(*ppOutEncoding, folder->GetName());
					XP_STRCAT(*ppOutEncoding, ".*");	// NNTP protocol string -- do not localize
				}
				if (i + 1 < count)						// Some servers (RFC977bis?) don't like a trailing space on the last name
					XP_STRCAT (*ppOutEncoding, ",");	// NNTP protocol string -- do not localize
			}
			XP_STRCAT(*ppOutEncoding, "\"");			// NNTP protocol string -- do not localize
		}
		else
			err = SearchError_OutOfMemory;
	}
	return err;
}


MSG_SearchError MSG_SearchFrame::GetResultElement (MSG_ViewIndex idx, MSG_ResultElement **result)

{
	MSG_SearchError err = SearchError_Success;
	if ((int) idx >= m_resultList.GetSize())
		return SearchError_InvalidResultElement;

	MSG_ResultElement *res = m_resultList.GetAt(idx);
	if (res && res->IsValid())
		*result = res;
	else
		err = SearchError_InvalidResultElement;
	return err;
}


MSG_SearchError MSG_SearchFrame::SortResultList (MSG_SearchAttribute attrib, XP_Bool descending)
{
	m_pane->StartingUpdate (MSG_NotifyScramble, 0, 0);

	// Play a little dynamic-scoping trick to get these params into the Compare function
	m_sortAttribute = attrib;
	m_descending = descending;
	m_resultList.QuickSort (MSG_ResultElement::Compare);

	m_pane->EndingUpdate (MSG_NotifyScramble, 0, 0);

	return SearchError_Success;
}



MSG_SearchError MSG_SearchFrame::AddResultElement (MSG_ResultElement *element)
{
	XP_ASSERT(element);
	XP_ASSERT(m_pane);

	XP_Bool bVLV = FALSE;
	MSG_PaneType type = m_pane->GetPaneType();
	XP_ASSERT (MSG_ADDRPANE == type || AB_ABPANE == type || MSG_SEARCHPANE == type || type == AB_PICKERPANE);

	// Don't tell the FE about VLV results
	if (m_searchType == searchLdapVLV)
		bVLV = TRUE;

	// Add a new line to the FE's list
	int count = m_resultList.GetSize(), index;

	if (!bVLV)
		m_pane->StartingUpdate (MSG_NotifyInsertOrDelete, count, 1);
	index = m_resultList.Add (element);
	if (!bVLV)
		m_pane->EndingUpdate (MSG_NotifyInsertOrDelete, count, 1);

	// Tell the status bar to display new number of hits
	if (!bVLV && m_resultList.GetSize() > 0)
		UpdateStatusBar (MK_MSG_SEARCH_STATUS);

	return SearchError_Success;
}

void MSG_SearchFrame::AddViewToList(MSG_FolderInfo * folder, MessageDBView * view)
{
	MSG_SearchView * srchView = new MSG_SearchView;
	if (srchView)
	{
		srchView->folder = folder;
		srchView->view = view;
		m_viewList.Add(srchView);
	}
}

MessageDBView * MSG_SearchFrame::GetView(MSG_FolderInfo * folder)
{
	MSG_SearchView * srchView;
	// find the first instance of the folder in the search view array
	for (int i = 0; i < m_viewList.GetSize(); i++)
	{
		srchView = m_viewList.GetAt(i);
		if (srchView->folder == folder)  // do they point to the same folder?
			return srchView->view;
	}
	return NULL;  // not in the list
}

void MSG_SearchFrame::CloseView(MSG_FolderInfo * folder)
{
	MSG_SearchView * srchView;
	XP_Bool found = FALSE;
	// find the first instance of the folder in the search view array and delete it
	for (int i = 0; i < m_viewList.GetSize() && !found; i++)
	{
		srchView = m_viewList.GetAt(i);
		if (srchView->folder == folder)
		{
			m_viewList.RemoveAt(i);
			srchView->view->Close();     // we opened...so we close it...
			delete srchView;
			found = TRUE;
		}
	}
}


MSG_SearchResultArray * MSG_SearchFrame::IndicesToResultElements (const MSG_ViewIndex *indices, int32 numIndices)
{
	MSG_SearchResultArray * results = new MSG_SearchResultArray;
	for (int i = 0; i < numIndices; i++)
	{
		MSG_ResultElement * elem = m_resultList.GetAt(indices[i]);
		if (elem)
			results->Add(elem);
	}

	// now let's sort this new array based on the src folder
	results->QuickSort (MSG_ResultElement::CompareByFolderInfoPtrs);
	return results;  // return array...caller must deallocate the memory!
}


MSG_SearchError MSG_SearchFrame::GetLdapObjectClasses (char ** /*array*/, uint16 * /*maxItems*/)
{
	MSG_SearchError err = SearchError_Success;
	return err;
}


MsgERR MSG_SearchFrame::MoveMessages (const MSG_ViewIndex *indices,
									  int32 numIndices,
									  MSG_FolderInfo * destFolder)
{
	return ProcessSearchAsViewCmd(indices, numIndices, destFolder, MSG_SearchAsViewMove);
}

MsgERR MSG_SearchFrame::CopyMessages (const MSG_ViewIndex *indices, 
									  int32 numIndices, 
									  MSG_FolderInfo * destFolder,
									  XP_Bool deleteAfterCopy)
{
	if (deleteAfterCopy)
		return ProcessSearchAsViewCmd(indices, numIndices, destFolder, MSG_SearchAsViewMove);
	else
		return ProcessSearchAsViewCmd(indices, numIndices, destFolder, MSG_SearchAsViewCopy);
}

MsgERR MSG_SearchFrame::TrashMessages  (const MSG_ViewIndex * indices,
										int32 numIndices)
{
	return ProcessSearchAsViewCmd(indices, numIndices, NULL, MSG_SearchAsViewDelete);
}

MsgERR MSG_SearchFrame::DeleteMessages (const MSG_ViewIndex * indices,
										int32 numIndices)
{
	return ProcessSearchAsViewCmd(indices, numIndices, NULL, MSG_SearchAsViewDeleteNoTrash);
}

MSG_DragEffect MSG_SearchFrame::DragMessagesStatus(const MSG_ViewIndex *indices, int32 numIndices,MSG_FolderInfo * destFolder, MSG_DragEffect request)
{
	if (!destFolder)
		return MSG_Drag_Not_Allowed;

	// how to handle source folders for search as view where they can all be different. Fortunately, the source folders should be either
	// all IMAP folders, all POP, or all News since we cannot currently support searching over different servers. If this fact changes, then
	// this step is going to be complicated.

	MSG_SearchValue * value;
	MSG_ResultElement * elem = NULL;;

	// If there are indices, then we have a source folder.  Otherwise, just check the destination
	// folder.
	if (numIndices > 0)
		elem = m_resultList.GetAt(indices[0]);  // we don't care which source folder we get.
	if ((numIndices > 0) ? (elem != NULL) : TRUE)
	{
		// Which destinations can have messages added to them?

		if (destFolder->GetDepth() <= 1) // root of tree or server.
			return MSG_Drag_Not_Allowed; // (needed because local mail server has type "FOLDER_MAIL").
	
		FolderType destType = destFolder->GetType();

		if (destType == FOLDER_CONTAINERONLY // can't drag a message to a server
			|| destType == FOLDER_CATEGORYCONTAINER
			|| destType == FOLDER_IMAPSERVERCONTAINER
			|| destType == FOLDER_NEWSGROUP) // should we offer to post the message? - jrm
			return MSG_Drag_Not_Allowed;

		MSG_IMAPFolderInfoMail *imapDest = destFolder->GetIMAPFolderInfoMail();
		if (imapDest)
		{
			if (!imapDest->GetCanDropMessagesIntoFolder())
				return MSG_Drag_Not_Allowed;
		}

		if (numIndices <= 0 || !elem)
		{
			// This is as far as we can go;  there is no source folder, so the FE
			// only wants to find out if drops are allowed in the destination.
			return request;
		}
		
		elem->GetValue(attribFolderInfo, & value);
		MSG_FolderInfo *srcFolder = value->u.folder;
		if (srcFolder)
		{
			FolderType sourceType = srcFolder->GetType();

			// Which drags are required to be copies?
			XP_Bool mustCopy = FALSE;
			if ((sourceType == FOLDER_MAIL) != (destType == FOLDER_MAIL)) // Bug #55177 drag between local and server...
				mustCopy = TRUE;
			else if (sourceType == FOLDER_NEWSGROUP)
				mustCopy = TRUE;
			else if (sourceType == FOLDER_IMAPMAIL)	// if IMAP, we actually have to go through each source folder and check ACLs
			{
				if (numIndices < 50)	// we probably don't want to check the ACL on too many folders, just for drag/drop status
				{
					int k = 0;
					while (!mustCopy && (k < numIndices))	// drop out once we hit the first one that fails
					{
						MSG_ResultElement *imapElem = m_resultList.GetAt(indices[k]);
						if (imapElem)
						{
							elem->GetValue(attribFolderInfo, & value);
							MSG_FolderInfo *folderK = value->u.folder;
							if (folderK)
							{
								MSG_IMAPFolderInfoMail *imapFolderK = folderK->GetIMAPFolderInfoMail();
								if (imapFolderK && !imapFolderK->GetCanDeleteMessagesInFolder())
									mustCopy = TRUE;
							}
						}
						k++;
					}
				}
			}

			if (mustCopy)
				return (MSG_DragEffect)(request & MSG_Require_Copy);
	
			// Now, if they don't care, give them a move
			if ((request & MSG_Require_Move) == MSG_Require_Move)
				return MSG_Require_Move;
		
			// Otherwise, give them what they want
				return request;
		}
	}
	return MSG_Drag_Not_Allowed;
}



MsgERR MSG_SearchFrame::ProcessSearchAsViewCmd (const MSG_ViewIndex * indices, int32 numIndices,
														 MSG_FolderInfo * destFolder, /* might be NULL if cmd is delete */
														 SearchAsViewCmd cmdType)
// indices contains search result elements from different folders!! Our job is to break it down into
// folders and message keys for each folder. Then perform the SearchAsViewCmd on each folder and its associated
// set of message keys. Some of the arguments passed into this routine are ignored depending on the 
// search as view command being performed.

{

	MSG_ResultElement * elem;
	MSG_SearchValue * value;
	MSG_FolderInfo * srcFolder = NULL;
	
	if (numIndices == 0)
		return eSUCCESS;

	MSG_SearchResultArray * effectedResults = IndicesToResultElements(indices, numIndices); // also sorts by folderInfo
	if (!effectedResults)
		return eOUT_OF_MEMORY;  // should this be a different error condition?

	int index = 0;

	// repeat until we have looked at every search result element
	while (index < effectedResults->GetSize())
	{
		// each new iteration through, means we are looking at a new folder...
		elem = effectedResults->GetAt(index);
		if (elem)
		{
			IDArray * ids = new IDArray; // create a new ID Array
			elem->GetValue(attribFolderInfo, &value);
			srcFolder = value->u.folder;  // get the src folder

			if (!srcFolder)
				break;

			elem->GetValue(attribMessageKey, &value);   // add our first element
			ids->Add(value->u.key);  
			
			index++; // This ensures that we never have an infinite loop in the outer while loop! 

			XP_Bool sameFolder = TRUE;
			// add the message IDs for all messages also in this same folder
			while (index < effectedResults->GetSize() && sameFolder)
			{
				elem = effectedResults->GetAt(index);
				elem->GetValue(attribFolderInfo, &value);
				if (value->u.folder == srcFolder) 
				{
					elem->GetValue(attribMessageKey, &value);
					ids->Add(value->u.key);
					index++;
				}
				else
					sameFolder = FALSE;
			}

			// small hack.. If the source folder is a Newsgroup, then we can only copy it!
			// so change the FE command if they were trying a move. This simplifies FE work since they can always
			// call MOVE without ever having to check if a source folder is news group.
			MsgERR status = eSUCCESS;

			if ((srcFolder->GetType() == FOLDER_NEWSGROUP) && (cmdType == MSG_SearchAsViewMove))
				cmdType = MSG_SearchAsViewCopy;
			
			// if the srcFolder is a news group, 
			if (srcFolder->IsNews() && (cmdType == MSG_SearchAsViewDelete || cmdType == MSG_SearchAsViewDeleteNoTrash))
				status = eFAILURE;

			// All the message IDs for this folder have been added, so perform the command...
			if (status == eSUCCESS)
			{
				switch (cmdType)
				{
				case MSG_SearchAsViewMove:
					CopyResultElements(srcFolder, destFolder, ids, TRUE /* delete after copy */);
					break;
				case MSG_SearchAsViewCopy:
					CopyResultElements(srcFolder, destFolder, ids, FALSE /* do not delete after copy */);
					break;
				case MSG_SearchAsViewDelete:
					TrashResultElements(srcFolder, ids);  
					break;
				case MSG_SearchAsViewDeleteNoTrash:
					DeleteResultElements(srcFolder, ids);
					break;
				default:
					XP_ASSERT(0);
				}
			}

		}
		else
			index++;  // if we can do nothing else...
	}

	delete effectedResults; 

	for (int i = m_resultList.GetSize() - 1; i >= 0; i--)
	{
		elem = m_resultList.GetAt(i);
		// for each result element, determine if its index is in indices...
		for (int k = 0; k < numIndices; k++)
			if (indices[k] == i)	
		{
			numIndices--;
			m_pane->StartingUpdate(MSG_NotifyInsertOrDelete, i, -1);
			m_resultList.RemoveAt(i);
			if (elem)
			{
				delete elem;
				elem = NULL;
			}
			m_pane->EndingUpdate(MSG_NotifyInsertOrDelete, i, -1);
		}
	}

	return eSUCCESS;
}


MSG_SearchError MSG_SearchFrame::CopyResultElements (MSG_FolderInfo * srcFolder, MSG_FolderInfo * destFolder, 
													 IDArray * ids, XP_Bool deleteAfterCopy)
{
  if((srcFolder == NULL) || (destFolder == NULL))
    return SearchError_NullPointer;

	MessageDBView * view = NULL;
	// now all the message keys have been added...so perform this copy
	char * url = srcFolder->BuildUrl(NULL, MSG_MESSAGEKEYNONE);
	MessageDBView::OpenURL(url, m_pane->GetMaster(), ViewAny, & view, FALSE);
	AddViewToList(srcFolder, view);  // register the view in our search frame so it can close it when copy finished.
	srcFolder->StartAsyncCopyMessagesInto (destFolder, m_pane, view->GetDB(), ids, ids->GetSize(),
		m_pane->GetContext(), NULL, deleteAfterCopy, MSG_MESSAGEKEYNONE);
	return SearchError_Success;
}


MSG_SearchError MSG_SearchFrame::TrashResultElements (MSG_FolderInfo * srcFolder, IDArray * ids)
{
	XP_Bool imapDeleteIsMoveToTrash = (srcFolder) ? srcFolder->DeleteIsMoveToTrash() : FALSE;

	// first set up a folder Info for the trash.
	char * path = NULL;
	MSG_FolderInfoMail * trashFolder = NULL;

	// make sure we are are NOT a Trash folder OR an IMAP folder where delete is NOT move to trash...
	if (srcFolder && !(srcFolder->GetFlags() & MSG_FOLDER_FLAG_TRASH) && 
			!((srcFolder->GetType() == FOLDER_IMAPMAIL) && !imapDeleteIsMoveToTrash))
	{
		// determine if folder is a local mail folder...use local trash
		if (srcFolder->GetType() == FOLDER_MAIL)
		{
			path = m_pane->GetPrefs()->MagicFolderName (MSG_FOLDER_FLAG_TRASH);
			trashFolder = m_pane->GetMaster()->FindMailFolder(path, TRUE);
		}
		else // must be an IMAP folder...
		{
			// IMAP trash folder...
			MSG_IMAPFolderInfoMail *imapInfo = srcFolder->GetIMAPFolderInfoMail();
			if (imapInfo)
			{
				MSG_FolderInfo *foundTrash = MSG_GetTrashFolderForHost(imapInfo->GetIMAPHost());
				trashFolder = foundTrash ? foundTrash->GetMailFolderInfo() :
                  (MSG_FolderInfoMail *)NULL;
			}
		}
	
		XP_FREEIF(path);
		MSG_SearchError err = CopyResultElements (srcFolder, trashFolder, ids, TRUE);
		return err;

	}
	else
		DeleteResultElements(srcFolder, ids);

	return SearchError_Success;
}

MSG_SearchError MSG_SearchFrame::DeleteResultElements(MSG_FolderInfo * srcFolder, IDArray * ids)
// actually delete (not move to trash) messages with the appropriate indices from the srcFolder...
{
	MessageDBView * view = NULL;

	if (srcFolder)
	{
		if (srcFolder->GetType () == FOLDER_MAIL)
		{
			char * url = srcFolder->BuildUrl(NULL, MSG_MESSAGEKEYNONE);
			MessageDBView::OpenURL(url, m_pane->GetMaster(), ViewAny, & view, FALSE);
			if (view)
			{
				view->GetDB()->DeleteMessages(*ids, NULL); // delete the message from the data base....
				AddViewToList(srcFolder, view);  // register the view in our search frame so it can close it when copy finished.
			}

		}
		else if (srcFolder->GetType() == FOLDER_IMAPMAIL)
			((MSG_IMAPFolderInfoMail *) srcFolder)->DeleteSpecifiedMessages(m_pane, *ids);
	}
	return SearchError_Success;
}


MSG_SearchError MSG_SearchFrame::DeleteResultElementsInFolder (MSG_FolderInfo *folder)
{
	// When the user deletes a mail folder or newsgroup while we have search
	// results open on it, we should remove any such results from the view.
	// Otherwise we have stale folderInfo pointers hanging around.

	if (!folder)
		return SearchError_NullPointer;

	for (int i = m_resultList.GetSize() - 1; i >= 0; i--)
	{
		MSG_ResultElement *elem = m_resultList.GetAt(i);
		if (elem->m_adapter->m_scope->m_folder == folder)
		{
			m_pane->StartingUpdate (MSG_NotifyInsertOrDelete, i, -1);
			m_resultList.RemoveAt (i);
			delete elem;
			m_pane->EndingUpdate (MSG_NotifyInsertOrDelete, i, -1);
		}
	}

	return SearchError_Success;
}


int MSG_SearchFrame::TimeSlice ()
{
	MSG_ScopeTerm *scope = GetRunningScope();
	if (scope && scope->m_adapter->m_abortCalled)
		return -1;

	// Decide whether each scope in this search should get time in parallel, or whether
	// each scope should finish before the next one starts
	return m_parallel ? TimeSliceParallel() : TimeSliceSerial();
}


int MSG_SearchFrame::TimeSliceSerial ()
{
	// This version of TimeSlice runs each scope term one at a time, and waits until one
	// scope term is finished before starting another one. When we're searching the local
	// disk, this is the fastest way to do it.

	MSG_ScopeTerm *scope = GetRunningScope();
	if (scope)
	{
		MSG_SearchError err = scope->TimeSlice ();
		if (err == SearchError_ScopeDone)
		{
			m_idxRunningScope++;
			if (m_idxRunningScope < m_scopeList.GetSize())
				UpdateStatusBar (MK_MSG_SEARCH_STATUS);
		}

		return 0;
	}
	else
	{
		FE_SetProgressBarPercent (GetContext(), 0);
		return -1;
	}
}


int MSG_SearchFrame::TimeSliceParallel ()
{
	// This version of TimeSlice runs all scope terms at once, not waiting for any
	// to complete. It stops when all scope terms are done. When we're searching
	// a remote server, this is the fastest way to do it.

	MSG_SearchError err = SearchError_Success;
	int count = m_parallelScopes.GetSize();
	if (count > 0)
	{
		for (int i = 0; i < count; i++)
		{
			MSG_ScopeTerm *scope = m_parallelScopes.GetAt (i);
			err = scope->TimeSlice();

			// This will remove this scope from the round-robin in the 
			// event of an error, or of normal completion (ScopeDone)
			if (SearchError_Success != err)
				m_parallelScopes.RemoveAt(i);
		}

		return 0;
	}
	else
		return -1;
}


void MSG_SearchFrame::UpdateStatusBar (int message)
{
	// Fudge factor for a %d in a printf format string
	const int kPercentDFudge = 11;

	char *hitsString = NULL;
	int hitsStringLength = 0;

	if (message == MK_MSG_SEARCH_STATUS && m_resultList.GetSize() > 0)
	{
		// Build the "Found 32 matches" string 
		char *hitsTemplate = XP_GetString (MK_SEARCH_HITS_COUNTER);
		hitsStringLength = XP_STRLEN(hitsTemplate) + kPercentDFudge;
		hitsString = new char [hitsStringLength];
		if (hitsString)
			PR_snprintf (hitsString, hitsStringLength, hitsTemplate, m_resultList.GetSize());
	}

	// Build the progress string based on "message". If we built a hitsString, use it
	MSG_ScopeTerm *scope = GetRunningScope ();
	char *searchTemplate = XP_GetString (message);
	char *scopeName = scope->GetStatusBarName ();
	int statusStringLength = XP_STRLEN(searchTemplate) + XP_STRLEN(scopeName) + kPercentDFudge;
	if (hitsString)
		statusStringLength += hitsStringLength;
	char *statusString = new char [statusStringLength];
	if (statusString)
	{
		PR_snprintf (statusString, statusStringLength, searchTemplate, scopeName);
		if (hitsString)
			XP_STRNCAT_SAFE (statusString, hitsString, statusStringLength - XP_STRLEN(statusString));
		FE_Progress (GetContext(), statusString);
		delete [] statusString;
	}

	if (hitsString)
		delete [] hitsString;

	XP_FREEIF(scopeName);
}


msg_SearchAdapter *MSG_SearchFrame::GetRunningAdapter ()
{
	MSG_ScopeTerm *scope = GetRunningScope();
	if (scope)
		return GetRunningScope()->m_adapter;
	return NULL;
}


MSG_ScopeTerm *MSG_SearchFrame::GetRunningScope ()
{ 
	if (m_idxRunningScope < m_scopeList.GetSize())
		return m_scopeList.GetAt (m_idxRunningScope); 
	return NULL;
}


const char *MSG_SearchFrame::GetLdapObjectClass ()
{
	// If the caller has set an object class, we should use it. Otherwise
	// the DWIM setting is to search for people.
	return m_ldapObjectClass ? m_ldapObjectClass : "Person";
}


XP_Bool MSG_SearchFrame::GetGoToFolderStatus (MSG_ViewIndex *indices, int32 numIndices)
{
	for (int32 i = 0; i < numIndices; i++)
	{
		MSG_ResultElement *result = m_resultList.GetAt(indices[i]);
		if (!result)
			return FALSE;

		MSG_SearchValue *folderValue = NULL;
		if (SearchError_Success == result->GetValue (attribFolderInfo, &folderValue))
		{
			if (folderValue)
			{
				if (!folderValue->u.folder)
				{
					delete folderValue;
					return FALSE;
				}
				delete folderValue;
			}
			else
				return FALSE;
		}
		else
			return FALSE;

	}

	return TRUE;
}


XP_Bool MSG_SearchFrame::GetSaveProfileStatus ()
{
	if (m_scopeList.GetSize() > 0 && m_resultList.GetSize() > 0)
	{
		// Figure out whether the news host for the selected folder is capable of virtual groups
		MSG_FolderInfo *folder = m_scopeList.GetAt(0)->m_folder; 
		MSG_NewsHost *host = NULL;
		MSG_FolderInfoNews *newsFolder = folder->GetNewsFolderInfo();

		if (newsFolder)
			host = newsFolder->GetHost();
		else if (FOLDER_CONTAINERONLY == folder->GetType())
			host = ((MSG_NewsFolderInfoContainer*) folder)->GetHost();
		if (host)
			return host->QueryExtension("PROFILE");
	}

	return FALSE;
}


msg_SearchNewsEx *MSG_SearchFrame::GetProfileAdapter()
{
	if (GetSaveProfileStatus())
		return (msg_SearchNewsEx*) m_scopeList.GetAt(0)->m_adapter;
	return NULL;
}


void MSG_SearchFrame::BeginCylonMode ()
{
	if (m_cylonModeTimer) 
	{
		FE_ClearTimeout (m_cylonModeTimer);
		m_cylonModeTimer = NULL;
	}

	m_cylonModeTimer = FE_SetTimeout ((TimeoutCallbackFunction)MSG_SearchFrame::CylonModeCallback, this, 333 /*0.333 sec*/); 
	if (m_cylonModeTimer)
		m_inCylonMode = TRUE;
}


void MSG_SearchFrame::EndCylonMode ()
{
	m_inCylonMode = FALSE;
	if (m_cylonModeTimer) 
	{
		FE_ClearTimeout (m_cylonModeTimer);
		m_cylonModeTimer = NULL;
	}
	FE_SetProgressBarPercent (GetContext(), 0);
}


void MSG_SearchFrame::CylonModeCallback (void *closure)
{
	MSG_SearchFrame *frame = (MSG_SearchFrame*) closure;

	if (frame->m_inCylonMode)
	{
		FE_SetProgressBarPercent (frame->GetContext(), -1);
		frame->m_cylonModeTimer = NULL;
		frame->BeginCylonMode ();
	}
}


void MSG_SearchFrame::IncrementOfflineProgress ()
{
	// Increment the thermometer in the search dialog
	int32 percent = (int32) (100 * (((double) ++m_offlineProgressSoFar) / ((double) m_offlineProgressTotal)));
	FE_SetProgressBarPercent (GetContext(), percent);
}


//-----------------------------------------------------------------------------
//------------------- Validity checking for menu items etc. -------------------
//-----------------------------------------------------------------------------

msg_SearchValidityTable::msg_SearchValidityTable ()
{
	// Set everything to be unavailable and disabled
	for (int i = 0; i < kNumAttributes; i++)
		for (int j = 0; j < kNumOperators; j++)
		{
			SetAvailable (i, j, FALSE);
			SetEnabled (i, j, FALSE);
			SetValidButNotShown (i,j, FALSE);
		}
	m_numAvailAttribs = 0;   // # of attributes marked with at least one available operator
}

int msg_SearchValidityTable::GetNumAvailAttribs()
{
	m_numAvailAttribs = 0;
	for (int i = 0; i < kNumAttributes; i++)
		for (int j = 0; j < kNumOperators; j++)
			if (GetAvailable(i, j))
			{
				m_numAvailAttribs++;
				break;
			}
	return m_numAvailAttribs;
}

MSG_SearchError msg_SearchValidityTable::ValidateTerms (MSG_SearchTermArray &termList)
{
	MSG_SearchError err = SearchError_Success;

	for (int i = 0; i < termList.GetSize(); i++)
	{
		MSG_SearchTerm *term = termList.GetAt(i);
		XP_ASSERT(term->IsValid());
		if (!GetEnabled(term->m_attribute, term->m_operator) || 
			!GetAvailable(term->m_attribute, term->m_operator))
		{
			if (!GetValidButNotShown(term->m_attribute, term->m_operator))
				err = SearchError_ScopeAgreement;
		}
	}

	return err;
}


// global variable with destructor allows automatic cleanup
msg_SearchValidityManager gValidityMgr; 


msg_SearchValidityManager::msg_SearchValidityManager ()
{
	m_offlineMailTable = NULL;
	m_onlineMailTable = NULL;
	m_onlineMailFilterTable = NULL;
	m_newsTable = NULL;
	m_localNewsTable = NULL;
	m_newsExTable = NULL;
	m_ldapTable = NULL;
}


msg_SearchValidityManager::~msg_SearchValidityManager ()
{
	if (NULL != m_offlineMailTable)
		delete m_offlineMailTable;
	if (NULL != m_onlineMailTable)
		delete m_onlineMailTable;
	if (NULL != m_onlineMailFilterTable)
		delete m_onlineMailFilterTable;
	if (NULL != m_newsTable)
		delete m_newsTable;
	if (NULL != m_localNewsTable)
		delete m_localNewsTable;
	if (NULL != m_newsExTable)
		delete m_newsExTable;
	if (NULL != m_ldapTable)
		delete m_ldapTable;
}


//-----------------------------------------------------------------------------
// Bottleneck accesses to the objects so we can allocate and initialize them
// lazily. This way, there's no heap overhead for the validity tables until the
// user actually searches that scope.
//-----------------------------------------------------------------------------

MSG_SearchError msg_SearchValidityManager::GetTable (int whichTable, msg_SearchValidityTable **ppOutTable)
{
	if (NULL == ppOutTable)
		return SearchError_NullPointer;

	MSG_SearchError err = SearchError_Success;

	// hack...currently FEs are setting scope to News even if it should be set to OfflineNewsgroups...
	// i'm fixing this by checking if we are in offline mode...
	if (NET_IsOffline() && (whichTable == news || whichTable == newsEx))
		whichTable = localNews;


	switch (whichTable)
	{
	case offlineMail:
		if (!m_offlineMailTable)
			err = InitOfflineMailTable ();
		*ppOutTable = m_offlineMailTable;
		break;
	case onlineMail:
		if (!m_onlineMailTable)
			err = InitOnlineMailTable ();
		*ppOutTable = m_onlineMailTable;
		break;
	case onlineMailFilter:
		if (!m_onlineMailFilterTable)
			err = InitOnlineMailFilterTable ();
		*ppOutTable = m_onlineMailFilterTable;
		break;
	case news:
		if (!m_newsTable)
			err = InitNewsTable ();
		*ppOutTable = m_newsTable;
		break;
	case localNews:
		if (!m_localNewsTable)
			err = InitLocalNewsTable();
		*ppOutTable = m_localNewsTable;
		break;
	case newsEx:
		if (!m_newsExTable)
			err = InitNewsExTable ();
		*ppOutTable = m_newsExTable;
		break;
	case Ldap:
		if (!m_ldapTable)
			err = InitLdapTable ();
		*ppOutTable = m_ldapTable;
		break;
	default:                 
		XP_ASSERT(0);
		err = SearchError_NotAMatch;
	}

	return err;
}


MSG_SearchError msg_SearchValidityManager::NewTable (msg_SearchValidityTable **ppOutTable)
{
	XP_ASSERT (ppOutTable);
	*ppOutTable = new msg_SearchValidityTable;
	if (NULL == *ppOutTable)
		return SearchError_OutOfMemory;

	return SearchError_Success;
}

