/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "xp.h"

#include "abcinfo.h"
#include "abpane2.h"
#include "abpicker.h"
#include "abmodel.h" 
#include "dirprefs.h"
#include "xpgetstr.h"
#include "prefapi.h"
#include "msgurlq.h"
#include "ldap.h"

#include "addrprsr.h"

extern "C" {
extern int XP_BKMKS_IMPORT_ADDRBOOK;
extern int XP_BKMKS_SAVE_ADDRBOOK;
extern int MK_MSG_ADDRESS_BOOK;
extern int MK_ADDR_MOVE_ENTRIES;
extern int MK_ADDR_COPY_ENTRIES;
extern int MK_ADDR_DELETE_ENTRIES;

#if !defined(MOZADDRSTANDALONE)
extern int	MK_ADDR_FIRST_LAST_SEP;
/* extern int	MK_ADDR_LAST_FIRST_SEP; */
#endif
}

// eventually we'll want to apply some smart heuristic to increase these values during import...
// but for now we'll just use constants...
#define AB_kImportRowAmount 100  /* # of entries to import in a time slice.. */
#define AB_kImportFileByteCount 5000 /* file byte count limit */
#define AB_kExportRowCount  100
#define AB_kExportFileByteCount 5000

#define AB_kCopyInfoAmount 5   /* # of entries to copy/move/delete at a time */

AB_ReferencedObject::AB_ReferencedObject()
{
	m_refCount = 1;
	m_objectState = AB_ObjectOpen;
}

AB_ReferencedObject::~AB_ReferencedObject()
{
	XP_ASSERT(m_refCount == 0);
}

int32 AB_ReferencedObject::Acquire()
{
	return ++ m_refCount;
}

int32 AB_ReferencedObject::Release()
{
	int32 refCount = --m_refCount;
	if (m_refCount <= 0)
	{
		if (m_objectState == AB_ObjectOpen)
		{
			Close();
			GoAway();  /* do not call delete here...if the object is deletable, GoAway will do so */
		}
	}
	return refCount;
}

int AB_ReferencedObject::Close() 
{ 
	m_objectState = AB_ObjectClosed;
	return AB_SUCCESS;
}

AB_ContainerInfo::AB_ContainerInfo(MWContext * context) : ab_View(ab_Usage::kHeap, ab_Change_kAllRowChanges | ab_Change_kClosing), AB_ReferencedObject()
{
	m_context = context;
	m_sortAscending = TRUE;
	m_sortedBy = AB_attribDisplayName;
	m_pageSize = 0; 
	m_IsCached = TRUE;  // by default, all ctrs are cached. If you develop a ctr that is not cached, set this flag in its constructor
	m_isImporting = FALSE;
	m_isExporting = FALSE;

	m_db = (AB_Database *)XP_CALLOC(1, sizeof(AB_Database));
}

AB_ContainerInfo::~AB_ContainerInfo()
{
	XP_ASSERT(m_db == NULL);
}

void AB_ContainerInfo::GoAway()
{
	// this is only called when our ref count is zero!!!
	XP_ASSERT(m_refCount <= 0);   // this insures that close has been called
	AB_Env * env = AB_Env_New();
	env->sEnv_DoTrace = 0;
	ab_Env * ev = ab_Env::AsThis(env);
	ab_View::ReleaseObject(ev); 
	AB_Env_Release(env);
}

/* static */ int AB_ContainerInfo::Create(MWContext * context, AB_ContainerInfo * parentContainer, ABID entryID, AB_ContainerInfo ** newCtr)
{
	// re-direct this because we know we want to create a non-directory container with this create call.....
	return AB_NonDirectoryContainerInfo::Create(context, parentContainer, entryID, newCtr);
}


/* static */ int AB_ContainerInfo::Create(MWContext * context, DIR_Server * server, AB_ContainerInfo ** newCtr)
{
	// re-direct this because we know we want to create a directory container. Why? 'cause we have a DIR_Server
	return AB_DirectoryContainerInfo::Create(context, server, newCtr);
}

int AB_ContainerInfo::Close()
{
	// this basically acts like the destructor! the destructor assumes that Close has always been called.
	// it will yell at us through assertion failures if we have not come through here first!

	m_objectState = AB_ObjectClosing;  // no longer open for business!!
	NotifyAnnouncerGoingAway(this);
	if (m_db)
	{
		CloseDatabase(m_db);

		XP_ASSERT(m_refCount <= 0);
		XP_ASSERT(m_db->db_row == NULL);
		XP_ASSERT(m_db->db_table == NULL);
		XP_ASSERT(m_db->db_env == NULL);
		XP_ASSERT(m_db->db_store == NULL);

		XP_FREE(m_db);
		m_db = NULL;
	}

	

	// we aren't responsible for anything else on closing...
	m_objectState = AB_ObjectClosed;  // put a fork in this container....it's done...
	return AB_SUCCESS;
}


void AB_ContainerInfo::CloseDatabase(AB_Database *db)		// releases all of our database objects, saves the content, etc. 
{
	if (db->db_row)
	{
		AB_Row_Release(db->db_row, db->db_env);
		db->db_row = NULL;
	}

	// (1) release the table
	if (db->db_table)
	{
		AB_Env_ForgetErrors(db->db_env);
		if ( ( (ab_Table *) db->db_table)->IsOpenObject() )
			((ab_Table *) db->db_table)->CutView(ab_Env::AsThis(db->db_env), this);
		
		AB_Table_Close(db->db_table, db->db_env);
		AB_Table_Release(db->db_table, db->db_env);

		// check errors...
		db->db_table = NULL;
	}

	if (db->db_listTable)
	{
		AB_Env_ForgetErrors(db->db_env);
		AB_Table_Close(db->db_listTable, db->db_env);
		AB_Table_Release(db->db_listTable, db->db_env);
		db->db_listTable = NULL;
	}

	// (2) release the store
	if (db->db_store)
	{
		AB_Store_SaveStoreContent(db->db_store, db->db_env); /* commit */
		// report any errors on the commit??? 
		AB_Env_ForgetErrors(db->db_env);
		AB_Store_CloseStoreContent(db->db_store, db->db_env);
		AB_Store_Release(db->db_store, db->db_env);
		AB_Env_ForgetErrors(db->db_env);
		db->db_store = NULL;
	}

	if (db->db_env)
	{
		AB_Env_Release(db->db_env);
		db->db_env = NULL;
	}
}

uint32 AB_ContainerInfo::GetNumEntries() // right now returns total # known entries.
{
	if (m_db->db_table && m_db->db_env)
		return AB_Table_CountRows(m_db->db_table, m_db->db_env);
	else
		return 0;
}

MSG_ViewIndex AB_ContainerInfo::GetIndexForABID(ABID entryID)
{
	MSG_ViewIndex index = MSG_VIEWINDEXNONE;
	if (m_db->db_env && m_db->db_table && entryID != AB_ABIDUNKNOWN)
	{
		AB_Env_ForgetErrors(m_db->db_env);
		ab_row_pos rowPos = AB_Table_RowPos(m_db->db_table, m_db->db_env, (ab_row_uid) entryID);
		AB_Env_ForgetErrors(m_db->db_env); // we can forget any errors 'cause if it failed, we'll return MSG_VIEWINDEXNONE
		if (rowPos) // 1 based index...
			index = (MSG_ViewIndex) (rowPos - 1); // - 1 because database is one indexed...our world is zero indexed.
	}

	return index;
}

char * AB_ContainerInfo::GetDescription() // caller must free the char *
{
	if (m_description)
		return XP_STRDUP(m_description);
	else
		return NULL;
}

AB_LDAPContainerInfo * AB_ContainerInfo::GetLDAPContainerInfo()
{ 
	return NULL;
}

AB_PABContainerInfo * AB_ContainerInfo::GetPABContainerInfo()
{
	return NULL;
}

AB_MListContainerInfo * AB_ContainerInfo::GetMListContainerInfo()
{
	return NULL;
}

XP_Bool AB_ContainerInfo::IsInContainer(ABID entryID)
{
	if (entryID != AB_ABIDUNKNOWN)
		return TRUE;
	else
		return FALSE;
}

XP_Bool AB_ContainerInfo::IsIndexInContainer(MSG_ViewIndex index)
{
	if (index != MSG_VIEWINDEXNONE)
	{
		if (index + 1 <= AB_ContainerInfo::GetNumEntries())
			return TRUE;
	}
	
	return FALSE;
}

/* CompareSelections
 *
 * This is sort of a virtual selection comparison function.  We must implement
 * it this way because the compare function must be a static function.
 */
int AB_ContainerInfo::CompareSelections(const void *e1, const void *e2)
{
 	/* Currently, only the LDAP container implements extended selection, so
	 * we can just call its comparison function.  In the future, if we have
	 * several classes that support extended selection the first element
	 * in the selection entry could be a type and we could switch on that
	 * for the proper comparison function to call.
	 */
	return AB_LDAPContainerInfo::CompareSelections(e1, e2);
}

ABID AB_ContainerInfo::GetABIDForIndex(const MSG_ViewIndex index)
{
	ABID entryID = AB_ABIDUNKNOWN;
	if (IsOpen() && m_db->db_env && m_db->db_table && index != MSG_VIEWINDEXNONE && index < GetNumEntries())
	{
		AB_Env_ForgetErrors(m_db->db_env);  // in case someone didn't clear any previous errors
		ab_row_uid rowUID = AB_Table_GetRowAt(m_db->db_table, m_db->db_env, (ab_row_pos) (index + 1));  // + 1 because db is one indexed, we are zero indexed
		AB_Env_ForgetErrors(m_db->db_env);  // we can ignore error codes because if it failed, then index not in table..
		if (rowUID)  // is it in the table i.e. > 0
			entryID = (ABID) rowUID;
	}

	return entryID;
}

void AB_ContainerInfo::UpdatePageSize(uint32 pageSize)
{
	if (pageSize > m_pageSize)
		m_pageSize = pageSize;
	
	// probably need to notify whoever is running the virtual list stuff that the page size has changed..
}

int AB_ContainerInfo::GetNumColumnsForContainer()
{
	// for right now, we have a fixed number of columns...0 based...
	return AB_NumberOfColumns;   // enumerated columnsID type
}

int AB_ContainerInfo::GetColumnAttribIDs(AB_AttribID * attribIDs, int * numAttribs)
{
	// this is just for starters...eventually we want to hook into the DIR_Server and update ourselves
	// with any changes....

	// okay this is hacky and not good since I fail completely if the array isn't big enough...
	// but the code should only be temporary =)

	if (*numAttribs == AB_NumberOfColumns)
	{
		attribIDs[0] = AB_attribEntryType;
		attribIDs[1] = /* AB_attribFullName */ AB_attribDisplayName;
		attribIDs[2] = AB_attribEmailAddress;
		attribIDs[3] = AB_attribCompanyName;
		attribIDs[4] = AB_attribWorkPhone;
		attribIDs[5] = AB_attribLocality;
		attribIDs[6] = AB_attribNickName;
	}

	else
		*numAttribs = 0;

	return AB_SUCCESS;
}


AB_ColumnInfo * AB_ContainerInfo::GetColumnInfo(AB_ColumnID columnID)
{
	AB_ColumnInfo * columnInfo = (AB_ColumnInfo *) XP_ALLOC(sizeof(AB_ColumnInfo));
	if (columnInfo)
	{
		switch (columnID)
		{
			case AB_ColumnID0:
				columnInfo->attribID = AB_attribEntryType;
				columnInfo->displayString = NULL;
				break;
			case AB_ColumnID1:
				columnInfo->attribID= AB_attribDisplayName;
				columnInfo->displayString = XP_STRDUP("Name");
				break;
			case AB_ColumnID2:
				columnInfo->attribID = AB_attribEmailAddress;
				columnInfo->displayString = XP_STRDUP("Email Address");
				break;
			case AB_ColumnID3:
				columnInfo->attribID = AB_attribCompanyName;
				columnInfo->displayString = XP_STRDUP("Organization");
				break;
			case AB_ColumnID4:
				columnInfo->attribID = AB_attribWorkPhone;
				columnInfo->displayString = XP_STRDUP("Phone");
				break;
			case AB_ColumnID5:
				columnInfo->attribID = AB_attribLocality;
				columnInfo->displayString = XP_STRDUP("City");
				break;
			case AB_ColumnID6:
				columnInfo->attribID = AB_attribNickName;
				columnInfo->displayString = XP_STRDUP("NickName");
				break;
			default:
				XP_ASSERT(0);
				XP_FREE(columnInfo);
				columnInfo = NULL;
		}

		if (columnInfo)
			columnInfo->sortable = IsEntryAttributeSortable(columnInfo->attribID);
	}

	return columnInfo;
}

// Methods for obtaining a container's child containers.
int32 AB_ContainerInfo::GetNumChildContainers()  // returns # of child containers inside this container...
{
	int childContainerCount = 0;
	if (m_db->db_listTable)
	{
		childContainerCount =  AB_Table_CountRows(m_db->db_listTable, m_db->db_env);
		if (AB_Env_Good(m_db->db_env))
			return childContainerCount;
		AB_Env_ForgetErrors(m_db->db_env);
	}

	return childContainerCount;
}

int AB_ContainerInfo::AcquireChildContainers(AB_ContainerInfo ** arrayOfContainers /* allocated by caller */, 
										int32 * numContainers /* in - size of array, out - # containers added */)
// All acquired child containers are reference counted here! Caller does not need to do it...but the do need to
// release these containers when they are done!
{
	int status = AB_FAILURE;
	int32 numAdded = 0;

	// enumerate through the list table getting the ABID for each index. Make a ctr from each one
	if (numContainers && *numContainers > 0 && m_db->db_listTable && m_db->db_env)
	{
		for (int32 index = 0; index < GetNumChildContainers()  && index < *numContainers; index++)
		{
			arrayOfContainers[index] = NULL;  // always make sure the entry will have some value...
			ab_row_uid rowUID = AB_Table_GetRowAt(m_db->db_listTable, m_db->db_env, (ab_row_pos) (index + 1));  // + 1 because db is one indexed, we are zero indexed
			if (rowUID)
				AB_ContainerInfo::Create(m_context,this, (ABID) rowUID, &(arrayOfContainers[index]));

		}

		numAdded = *numContainers; // we actually stored something for every value
		status = AB_SUCCESS;
	}

	if (numContainers)
		*numContainers = numAdded;

	return status;
}

XP_Bool AB_ContainerInfo::IsEntryAContainer(ABID entryID)
{
	// right now a mailing list is our only container entry in a container....
	AB_AttributeValue * value = NULL;
	XP_Bool containerEntry = FALSE;
	GetEntryAttribute(NULL, entryID, AB_attribEntryType, &value);
	if (value)
	{
		if (value->u.entryType == AB_MailingList)
			containerEntry = TRUE;
		AB_FreeEntryAttributeValue(value);
	}
	return containerEntry;
}

// Adding Entries to the Container
int AB_ContainerInfo::AddEntry(AB_AttributeValue * values, uint16 numItems, ABID * entryID /* our return value */)
{
	int status = AB_FAILURE;
	if (m_db->db_row && m_db->db_env)
	{
		// clear the row out...
		AB_Row_ClearAllCells(m_db->db_row, m_db->db_env);
		const char * content;
		// now write the attributes for our new value into the row....
		for (uint16 i = 0; i < numItems; i++)
		{
			content = ConvertAttribValueToDBase(&values[i]);
			AB_Row_WriteCell(m_db->db_row, m_db->db_env, content, AB_Attrib_AsStdColUid(ConvertAttribToDBase(values[i].attrib)));
		}

		// once all the cells have been written, write the row to the table
		ab_row_uid RowUid = AB_Row_NewTableRowAt(m_db->db_row, m_db->db_env, 0);
		AB_Env_ForgetErrors(m_db->db_env);

		if (RowUid > 0) // did it succeed?
		{
			if (entryID)
				*entryID = (ABID) RowUid;
			status = AB_SUCCESS;
		}

		AB_Env_ForgetErrors(m_db->db_env);
	}

	return status;
}

int AB_ContainerInfo::AddUserWithUI(MSG_Pane * srcPane, AB_AttributeValue * values,  uint16 numItems, XP_Bool /* lastOneToAdd */)
{
	int status = AB_SUCCESS;
	// create a person pane, initialize it with our values, 
	if (srcPane)
	{
		AB_PersonPane * personPane = new AB_PersonPane(srcPane->GetContext(), srcPane->GetMaster(), this, AB_ABIDUNKNOWN, NULL);
		if (personPane)
		{
			personPane->InitializeWithAttributes(values, numItems);
			AB_ShowPropertySheetForEntryFunc * func = srcPane->GetShowPropSheetForEntryFunc();
			if (func)
				func (personPane, srcPane->GetContext());
		}
		else
			status = AB_FAILURE;

	}
	else
		status = ABError_NullPointer;
	return status;
}

int AB_ContainerInfo::AddSender(char * /* author */, char * /* url */)
{
	return AB_SUCCESS;
}

/*******************************************
	Asynchronous import/export methods...
********************************************/
int AB_ContainerInfo::ImportBegin(MWContext * /* context */, const char * fileName, void ** importCookie, XP_Bool * importFinished)
{
	if (importCookie)
		*importCookie = NULL;
	int status = AB_SUCCESS;
	if (/* !m_isImporting && */ m_db->db_store && m_db->db_env && fileName)
	{
//		m_isImporting = TRUE;
		AB_ImportState * importState = (AB_ImportState *) XP_ALLOC(sizeof(AB_ImportState));
		if (importState)
		{
			importState->db_file = AB_Store_NewImportFile(m_db->db_store, m_db->db_env, fileName);
			importState->db_thumb = AB_Store_NewThumb(m_db->db_store, m_db->db_env, AB_kImportRowAmount, AB_kImportFileByteCount /* file byte count limit */);
			if (AB_Env_Good(m_db->db_env))
			{
				if (importCookie)
					*importCookie = (void *) (importState);
				status = ImportMore(importCookie, importFinished);
			}
			else
			{
				AB_Env_ForgetErrors(m_db->db_env);
				// need some suitable warning to the user saying that the import failed.....
			}
		}
		else
			status = AB_OUT_OF_MEMORY;
	}
	else
		status = AB_FAILURE;

	return status;
}

int AB_ContainerInfo::ImportMore(void ** importCookie, XP_Bool * importFinished)
// if we are finished, import cookie is released...
{
	int status = AB_SUCCESS;
	XP_Bool keepImporting = FALSE;
	if (importCookie && *importCookie && m_db->db_store && m_db->db_env)
	{
		AB_ImportState * importState = (AB_ImportState *) (*importCookie);
		keepImporting = AB_Store_ContinueImport(m_db->db_store, m_db->db_env, importState->db_file, importState->db_thumb);
		if (AB_Env_Bad(m_db->db_env))
		{
			// display some suitable error message....need more info...
			AB_Env_ForgetErrors(m_db->db_env);
		}

		if (importFinished)
			*importFinished = !keepImporting;
	}

	return status;
}

int AB_ContainerInfo::ImportInterrupt(void ** importCookie) // free import state and termineate import
{

	// right now, import always means finish import...i.e. we always stop the import on interrupt...
	return ImportFinish(importCookie);
}

int AB_ContainerInfo::ImportFinish(void ** importCookie)
{
	AB_ImportState * importState = NULL;
	if (importCookie)
		importState = (AB_ImportState *) (*importCookie);

	if (importState)
	{
		AB_File_Release(importState->db_file, m_db->db_env);
		AB_Env_ForgetErrors(m_db->db_env);
		AB_Thumb_Release(importState->db_thumb, m_db->db_env);
		AB_Env_ForgetErrors(m_db->db_env);
		XP_FREE(importState);
	}

	if (importCookie) 
		*importCookie = NULL; // we have freed the cookie...

	m_isImporting = FALSE; // we are no longer attempting to import a file...

	return AB_SUCCESS;
}

/* used to determine progress on the import...*/
int AB_ContainerInfo::ImportProgress(void * importCookie, uint32 * position, uint32 * fileLength, uint32 * passCount)
{
	AB_ImportState * importState = (AB_ImportState *) importCookie;
	int status = AB_SUCCESS;
	if (importState && m_db->db_env)
	{
		uint32 pos = AB_Thumb_ImportProgress(importState->db_thumb, m_db->db_env, (ab_pos *) fileLength, (ab_count *) passCount);
		if (position)
			*position = pos;
		AB_Env_ForgetErrors(m_db->db_env);
	}
	else
		status = ABError_NullPointer;

	return status;
}

int AB_ContainerInfo::ExportBegin(MWContext * context, const char * fileName, void ** exportCookie, XP_Bool * exportFinished)
{
	if (exportCookie)
		*exportCookie = NULL;
	int status = AB_SUCCESS;
	if (m_db->db_store && m_db->db_env && fileName)
	{
		AB_ImportState * exportState = (AB_ImportState *) XP_ALLOC(sizeof(AB_ImportState));
		if (exportState)
		{
			exportState->db_file = AB_Store_NewExportFile(m_db->db_store, m_db->db_env, fileName);
			exportState->db_thumb = AB_Store_NewThumb(m_db->db_store, m_db->db_env, AB_kExportRowCount, AB_kExportFileByteCount);
			if (AB_Env_Good(m_db->db_env))
			{
				if (exportCookie)
					*exportCookie = (void *) (exportState);
				status = ExportMore(exportCookie, exportFinished);
			}
			else
			{
				AB_Env_ForgetErrors(m_db->db_env);
				// need some suitable warning to the user saying that the export failed.....
			}
		}
		else
			status = AB_OUT_OF_MEMORY;
	}
	else
		status = AB_FAILURE;

	return status;
}

int AB_ContainerInfo::ExportMore(void ** exportCookie, XP_Bool * exportFinished)
{
	int status = AB_SUCCESS;
	XP_Bool keepExporting = FALSE;
	if (exportCookie && *exportCookie && m_db->db_store && m_db->db_env)
	{
		AB_ImportState * exportState = (AB_ImportState *) (*exportCookie);
		keepExporting = AB_Store_ContinueExport(m_db->db_store, m_db->db_env, exportState->db_file, exportState->db_thumb);
		if (AB_Env_Bad(m_db->db_env))
		{
			// display some suitable error message....need more info...
			AB_Env_ForgetErrors(m_db->db_env);
		}

		if (exportFinished)
			*exportFinished = !keepExporting;
	}

	return status;
}

int AB_ContainerInfo::ExportInterrupt(void ** exportCookie)
{

	// right now, import always means finish import...i.e. we always stop the import on interrupt...
	return ExportFinish(exportCookie);
}

int AB_ContainerInfo::ExportProgress(void * exportCookie, uint32 * numberExported, uint32 * totalEntries)
{
	AB_ImportState * exportState = (AB_ImportState *) exportCookie;
	int status = AB_SUCCESS;
	if (exportState && m_db->db_env)
	{
		uint32 num = AB_Thumb_ExportProgress(exportState->db_thumb, m_db->db_env, (ab_row_count *) totalEntries);
		if (numberExported)
			*numberExported = num;
		AB_Env_ForgetErrors(m_db->db_env);
	}
	else
		status = ABError_NullPointer;

	return status;
}

int AB_ContainerInfo::ExportFinish(void ** exportCookie)
{
	AB_ImportState * exportState = NULL;
	if (exportCookie)
		exportState = (AB_ImportState *) (*exportCookie);
	if (exportState)
	{
		AB_File_Release(exportState->db_file, m_db->db_env);
		AB_Env_ForgetErrors(m_db->db_env);
		AB_Thumb_Release(exportState->db_thumb, m_db->db_env);
		AB_Env_ForgetErrors(m_db->db_env);
		XP_FREE(exportState);
	}
	if (exportCookie)
		*exportCookie = NULL; 

	m_isExporting = FALSE;

	return AB_SUCCESS;
}

/*****************************************************************

  Basic import methods......(i.e. those called by panes, FEs, etc.

******************************************************************/


/* static */ void AB_ContainerInfo::ImportFileCallback(MWContext * context, char * fileName, void * closure)
{
	// notice that we don't ref count the ctr inside the closure....control eventually returns to the
	// caller so it is responsible for acquiring/releasing the ctr as necessary..
	AB_ImportClosure * importClosure = (AB_ImportClosure *) closure;
	if (importClosure)
	{
		if (importClosure->container)
			importClosure->container->ImportFile(importClosure->pane, context, fileName);
	}

	XP_FREEIF(fileName);
}

int AB_ContainerInfo::ImportFile(MSG_Pane * pane, MWContext * /* context */, const char * fileName)
{
	if (!m_isImporting && m_db->db_store && m_db->db_env)
	{
		m_isImporting = TRUE;
		// now create an import URL and add it to the queue
		char * url = PR_smprintf("%s%s", "addbook:import?file=", fileName);
		URL_Struct *url_struct = NET_CreateURLStruct(url, NET_DONT_RELOAD);
		if (url_struct)
		{
			Acquire();  // add a ref count for the URL copy 
			url_struct->owner_data = (void *) this;
			if (pane)
				MSG_UrlQueue::AddUrlToPane(url_struct, NULL, pane, FALSE, NET_DONT_RELOAD);
			else
				NET_GetURL(url_struct, FO_PRESENT, m_context, NULL);
		}

		XP_FREEIF(url);

	}
	return AB_SUCCESS;
}
	
int AB_ContainerInfo::ImportData(MSG_Pane * pane, const char * buffer, int32 /* bufSize */, AB_ImportExportType dataType)
{
	int status = AB_SUCCESS;
	AB_ImportClosure * closure = NULL;
	if (m_db->db_env && m_db->db_store)
	{
		switch (dataType)
		{
			case AB_PromptForFileName:
				closure = (AB_ImportClosure *) XP_ALLOC(sizeof(AB_ImportClosure));
				if (closure)
				{
					closure->container = this; // note: we aren't ref counting this...
					closure->pane = pane;
					FE_PromptForFileName(m_context,XP_GetString(XP_BKMKS_IMPORT_ADDRBOOK),0, TRUE, FALSE, AB_ContainerInfo::ImportFileCallback, closure);
					XP_FREE(closure);
				}
				else
					status = AB_OUT_OF_MEMORY;
				break;

			case AB_Vcard:
				status = AB_ImportVCards(this, buffer); /* buffer should be NULL terminated */
				break;

			/* we'll add comma list and raw data here as they come on line...for now...*/
			default:
				/* don't call the database here....FEs pass in prompt for file name.....what if the type is vcard? 
				   that is a non - db type... */
				/* AB_Store_ImportWithPromptForFileName(m_db->db_store, m_db->db_env, m_context); */
				break;
		}
	}
	return status;
}

XP_Bool AB_ContainerInfo::ImportDataStatus(AB_ImportExportType /* dataType */)
{
	// Keep it simple for now....PABs and Mailing lists will accept importing from any type (filename,vcard, raw data, etc.)
	if (GetType() != AB_LDAPContainer)
		return TRUE;
	else
		return FALSE;
}

int AB_ContainerInfo::ExportData(MSG_Pane * pane, char ** /* buffer */, int32 * /* bufSize */, AB_ImportExportType dataType)
{
	int status = AB_SUCCESS;
	AB_ImportClosure * closure = NULL;
	if (m_db->db_env && m_db->db_store)
	{
		switch (dataType)
		{
			case AB_PromptForFileName:
				closure = (AB_ImportClosure *) XP_ALLOC(sizeof(AB_ImportClosure));
				if (closure)
				{
					closure->container = this;
					closure->pane = pane;
					FE_PromptForFileName(m_context, XP_GetString(XP_BKMKS_SAVE_ADDRBOOK), 0, FALSE, FALSE, AB_ContainerInfo::ExportFileCallback, closure);
					XP_FREE(closure);
				}
				else
					status = AB_OUT_OF_MEMORY;
				break;
			default:
				/* don't call dbase....some types (like vcard) when I fill them in don't invole dbase's export APIs...*/
				break;
		}
	}
	return status;
}
/* static */ void AB_ContainerInfo::ExportFileCallback(MWContext * context, char * fileName, void * closure)
{
	// notice that we don't ref count the ctr inside the closure....control eventually returns to the
	// caller so it is responsible for acquiring/releasing the ctr as necessary..
	AB_ImportClosure * importClosure = (AB_ImportClosure *) closure;
	if (importClosure)
	{
		if (importClosure->container)
			importClosure->container->ExportFile(importClosure->pane, context, fileName);
	}

	XP_FREEIF(fileName);
}

int AB_ContainerInfo::ExportFile(MSG_Pane * pane, MWContext * /* context */, const char * fileName)
{
	if (!m_isExporting && m_db->db_store && m_db->db_env)
	{
		m_isExporting = TRUE;
		
		// now create an export URL and add it to the queue
		char * url = PR_smprintf("%s%s", "addbook:export?file=", fileName);
		URL_Struct *url_struct = NET_CreateURLStruct(url, NET_DONT_RELOAD);
		if (url_struct)
		{
			Acquire();  // add a ref count for the URL copy 
			url_struct->owner_data = (void *) this;
			if (pane)
				pane->GetURL(url_struct, TRUE); // launch the url...
			else
				NET_GetURL(url_struct, FO_PRESENT, m_context, NULL);
		}

		XP_FREEIF(url);

	}
	return AB_SUCCESS;
}


int AB_ContainerInfo::UpdateDIRServer() // FE has changed DIR_Server, update self and notify any listeners
{
	return AB_SUCCESS;
}
/*
	Getting / Setting Container Attributes
*/

int AB_ContainerInfo::GetAttribute(AB_ContainerAttribute attrib, AB_ContainerAttribValue * value /* already allocated */)
{
	if (value)
	{
		value->attrib = attrib;
		switch(attrib)
		{
		case attribContainerType:
			value->u.containerType = GetType();
			break;
		case attribName:
			if (m_description)
				value->u.string = XP_STRDUP(m_description);
			else
				value->u.string = XP_STRDUP(""); // assign empty string as a default value? 
			break;
		case attribNumChildren:
			value->u.number = GetNumChildContainers();
			break;
		case attribDepth:
			value->u.number = m_depth;
			break;
		case attribContainerInfo:
			value->u.container = this;
			break;
		default:   // most unexpected this is...
			return AB_INVALID_ATTRIBUTE;
		}

		return AB_SUCCESS;
	}

	return AB_FAILURE;
}

// Container Attributes
int AB_ContainerInfo::GetAttribute(AB_ContainerAttribute attrib, AB_ContainerAttribValue ** newValue)
{
	AB_ContainerAttribValue * value = new AB_ContainerAttribValue;
	if (!value)
		return AB_OUT_OF_MEMORY;
	
	*newValue = value;  // point to our new attribute value
	return GetAttribute(attrib, value);
}

int AB_ContainerInfo::SetAttribute(AB_ContainerAttribValue * value)
{
	if (value)
	{
		switch(value->attrib)
		{
		case attribName:
			// WARNING! What do we do with the old name? free it?
			XP_FREE(m_description);
			m_description = XP_STRDUP(value->u.string);
			break;
		default: // otherwise the attribute is not setable!
			return AB_INVALID_ATTRIBUTE;  
		}
		
		return AB_SUCCESS;
	}

	return AB_FAILURE;
}


int AB_ContainerInfo::GetAttributes(AB_ContainerAttribute * attribArray, AB_ContainerAttribValue ** valueArray, uint16 * numItems)
{
	int errorCode = AB_SUCCESS;
	int error = AB_SUCCESS; // error returned from calling another function
	*valueArray = NULL;
	if (*numItems > 0)
	{
		AB_ContainerAttribValue * values = (AB_ContainerAttribValue *) XP_ALLOC(sizeof(AB_ContainerAttribValue) * (*numItems));
		if (values)
		{
			for (uint16 i = 0; i < *numItems; i++)
				if ( (error = GetAttribute(attribArray[i], &values[i])) != AB_SUCCESS)
					errorCode = error;
			*valueArray = values;
		}
		else
			errorCode = AB_OUT_OF_MEMORY;
	}

	return errorCode;
}

int AB_ContainerInfo::SetAttributes(AB_ContainerAttribValue * valuesArray, uint16 numItems)
{
	int errorCode = AB_SUCCESS;
	int error = AB_SUCCESS;

	// set each attribute...return last non-success error message
	for (uint16 i = 0; i < numItems; i++)
		if ((error = SetAttribute(&valuesArray[i])) != AB_SUCCESS)
			errorCode = error;

	return errorCode;
}

// assumes destValue has already been allocated
int AB_ContainerInfo::AssignEntryValues(AB_AttributeValue *srcValue, AB_AttributeValue * destValue)
{
	return AB_CopyEntryAttributeValue(srcValue, destValue);
}

int AB_ContainerInfo::AssignEmptyEntryValue(AB_AttribID attrib, AB_AttributeValue * value /* already allocated */)
{
	return AB_CopyDefaultAttributeValue(attrib, value);
}

// Entry Attribute Accessors 
int AB_ContainerInfo::GetEntryAttribute(MSG_Pane * srcPane, ABID entryID, AB_AttribID attrib, AB_AttributeValue ** newValue)
{
	uint16 numItems = 1;
	return GetEntryAttributes(srcPane, entryID, &attrib, newValue, &numItems); 
}

XP_Bool AB_ContainerInfo::IsDatabaseAttribute(AB_AttribID attrib)
{
	switch (attrib)
	{
		case AB_attribFullAddress:
		case AB_attribVCard:
			return FALSE;
		default:
			return TRUE;
	}

}

int AB_ContainerInfo::GetNonDBEntryAttributeForIndex(MSG_Pane * srcPane, MSG_ViewIndex index, AB_AttribID attrib, AB_AttributeValue * value /* must already be allocated!!!*/)
{
	ABID entryID = GetABIDForIndex(index);
	return GetNonDBEntryAttribute(srcPane, entryID, attrib, value);
}

int AB_ContainerInfo::GetNonDBEntryAttribute(MSG_Pane * srcPane, ABID entryID, AB_AttribID attrib, AB_AttributeValue * value /* must already be allocated!!!*/)
{
	int status = AB_SUCCESS;

	if (value)
	{
		value->attrib = attrib;
		switch (attrib)
		{
		case AB_attribFullAddress:
				value->u.string = GetFullAddress(srcPane, entryID);
				break;
			case AB_attribVCard:
				value->u.string = GetVCard(srcPane, entryID);
				break;
			default:
				AB_CopyDefaultAttributeValue(attrib,value);
				break;
		}
	}
	else
		status = AB_FAILURE;

	return status;
}

int AB_ContainerInfo::GetEntryAttribute(MSG_Pane * srcPane, ABID entryID, AB_AttribID attrib, AB_AttributeValue * value /* must already be allocated!!!*/)
{	
	int status = AB_FAILURE;
	if (IsDatabaseAttribute(attrib))
	{
		if (m_db->db_row && value)
		{
			if (AB_Row_ReadTableRow(m_db->db_row, m_db->db_env, entryID))  // read in the row from the table
			{
				const AB_Cell * cell = AB_Row_GetColumnCell(m_db->db_row, m_db->db_env, AB_Attrib_AsStdColUid(ConvertAttribToDBase(attrib)));
				AB_Env_ForgetErrors(m_db->db_env);
				if (cell)
				{
					value->attrib = attrib;
					status = ConvertDBaseValueToAttribValue(cell->sCell_Content, value, attrib);
				}
			}
		}
		// if we had a problem, return a default attribute
		if (status == AB_FAILURE)
			status = AB_CopyDefaultAttributeValue(attrib,value);
	}
	else
		status = GetNonDBEntryAttribute(srcPane, entryID, attrib, value);

	return status;
}

int AB_ContainerInfo::GetEntryAttributeForIndex(MSG_Pane * srcPane, MSG_ViewIndex index, AB_AttribID attrib, AB_AttributeValue ** value)
{
	uint16 numItems = 1;
	return GetEntryAttributesForIndex(srcPane, index, &attrib, value, &numItems /* special case of get entry attributes */);
}
	

int AB_ContainerInfo::GetEntryAttributesForIndex(MSG_Pane * srcPane, MSG_ViewIndex index, AB_AttribID * attribs, AB_AttributeValue ** valuesArray, uint16 * numItems)
{
	uint32 numEntries = GetNumEntries();

	if (index != MSG_VIEWINDEXNONE && index < numEntries)
	{
		int status = AB_SUCCESS;
		// okay, row positions in the database are ONE based. MSG_ViewIndices are ZERO based...need to convert to a row position..
		ab_row_pos rowPos = (ab_row_pos) (index + 1);
		uint16 numItemsAdded = 0; 
		AB_AttributeValue * values = NULL;

		if (numItems && *numItems > 0)
		{
			values = (AB_AttributeValue *) XP_ALLOC(sizeof(AB_AttributeValue) * (*numItems));
			if (values)
			{
				if (m_db->db_row)
				{
					AB_Row_ClearAllCells(m_db->db_row, m_db->db_env);

					if (AB_Env_Good(m_db->db_env) && AB_Row_ReadTableRowAt(m_db->db_row, m_db->db_env, rowPos))
					{
						for (uint16 i = 0; i < *numItems; i++) // for each attribute, process it
						{
							int loopStatus = AB_FAILURE;

							if (IsDatabaseAttribute(attribs[i])) // try to read in the database attribute
							{
								const AB_Cell * cell = AB_Row_GetColumnCell(m_db->db_row, m_db->db_env,AB_Attrib_AsStdColUid(ConvertAttribToDBase(attribs[i])));
								AB_Env_ForgetErrors(m_db->db_env); // come back later and pick up errors...
								values[numItemsAdded].attrib = attribs[numItemsAdded];
								if (cell)
									loopStatus = ConvertDBaseValueToAttribValue(cell->sCell_Content, &values[numItemsAdded], attribs[i]);
								else // if we had a problem, return a default attribute
									loopStatus = AB_CopyDefaultAttributeValue(attribs[numItemsAdded], &values[numItemsAdded]);
							} // if not a database attribute, generate it
							else
								loopStatus = GetNonDBEntryAttributeForIndex(srcPane, index, attribs[i], &values[numItemsAdded]);

							if (loopStatus == AB_SUCCESS) // then we succeeded in adding the entry
								numItemsAdded++;
						} /* for loop */
					} /* reading the row */
				}
			} /* values exists? */
			else
				status = AB_OUT_OF_MEMORY;
		}

		if (status != AB_SUCCESS && values)
		{
			XP_FREE(values); 
			values = NULL;
		}

		*valuesArray = values;
		*numItems = numItemsAdded;
		return status;
	}
	else
	{
		*valuesArray = NULL;
		return AB_FAILURE;  // we had an invalid index
	}
}

int AB_ContainerInfo::GetEntryAttributes(MSG_Pane * srcPane, ABID entryID, AB_AttribID * attribs, AB_AttributeValue ** valuesArray, uint16 * numItems)
{
	MSG_ViewIndex index = GetIndexForABID(entryID);
	return GetEntryAttributesForIndex(srcPane, index, attribs, valuesArray, numItems);
}

/*
	Setting entry attributes
*/

int AB_ContainerInfo::SetEntryAttributeForIndex(MSG_ViewIndex index, AB_AttributeValue * value)
{
	return SetEntryAttributesForIndex(index, value, 1);
}

int AB_ContainerInfo::SetEntryAttributesForIndex(MSG_ViewIndex index, AB_AttributeValue * valuesArray, uint16 numItems)
{
	// setting converts to an ABID and performs the set. Right now, database does not allow us to set attributes
	// based on row position. 
	ABID entryID = GetABIDForIndex(index);
	return SetEntryAttributes(entryID, valuesArray, numItems);
}
int AB_ContainerInfo::SetEntryAttribute(ABID entryID, AB_AttributeValue * value)
{
	return SetEntryAttributes(entryID, value, 1);
}

int AB_ContainerInfo::SetEntryAttributes(ABID entryID, AB_AttributeValue * valuesArray, uint16 numItems)
{
	int status = AB_FAILURE;
	AB_Env_ForgetErrors(m_db->db_env);

	if (m_db->db_row)
	{
		if (AB_Row_ReadTableRow(m_db->db_row, m_db->db_env, entryID)) // read in the row from the table
		{
			AB_Env_ForgetErrors(m_db->db_env);  // come back later and check for errors...
			const char * content = NULL;
			status = AB_SUCCESS;
			for (int i = 0; i < numItems && status == AB_SUCCESS; i++)
			{
				content = ConvertAttribValueToDBase(&valuesArray[i]);
				if (!AB_Row_WriteCell(m_db->db_row, m_db->db_env, content, AB_Attrib_AsStdColUid(ConvertAttribToDBase(valuesArray[i].attrib))))
					status = AB_FAILURE;  // it didn't write!!!
			}
			AB_Env_ForgetErrors(m_db->db_env);  // status would have caught the write cell error
			if (status == AB_SUCCESS)
			{
				AB_Row_UpdateTableRow(m_db->db_row, m_db->db_env, entryID);  // commit changes back to the database
				if (AB_Env_Bad(m_db->db_env))
					status = AB_FAILURE;
			}
		}
	}

	return status;
}

int AB_ContainerInfo::SortByAttribute(AB_AttribID attrib, XP_Bool ascending)
{
	m_sortedBy = attrib;
	m_sortAscending = ascending;
	return AB_SUCCESS;
}

int AB_ContainerInfo::SortByAttribute(AB_AttribID attrib)
{
	m_sortedBy = attrib;
	return AB_SUCCESS;
}

void AB_ContainerInfo::SetSortAscending(XP_Bool ascending)
{
	m_sortAscending = ascending;
}

int AB_ContainerInfo::GetSortedBy(AB_AttribID * attrib)
{
	*attrib = m_sortedBy;
	return AB_SUCCESS;
}

char * AB_ContainerInfo::GetVCard(MSG_Pane * /*srcPane*/, ABID entryID)		  // caller must free null terminated string returned
{
	char * VCardString = NULL;

	if (m_db && m_db->db_row && m_db->db_env && entryID != AB_ABIDUNKNOWN)
	{
		AB_Row_ClearAllCells(m_db->db_row, m_db->db_env);
		if ( AB_Env_Good(m_db->db_env) && AB_Row_ReadTableRow(m_db->db_row, m_db->db_env, (ab_row_pos) entryID) )
		{
			VCardString = AB_Row_AsVCardString(m_db->db_row, m_db->db_env);
			if (AB_Env_Bad(m_db->db_env))
			{
				if (VCardString)
				{
					XP_FREE(VCardString);
					VCardString = NULL;
				}

				AB_Env_ForgetErrors(m_db->db_env);
			}
		}
	}

	return VCardString;
}


/* The expandedString returned has been parsed, quoted, domain names added and can be used to send the message */
/* static */ int AB_ContainerInfo::GetExpandedHeaderForEntry(char ** expandedString, MSG_Pane * srcPane, AB_ContainerInfo * ctr, ABID entryID)
{
	// call the recursive version....we also need an IDArray to make sure we don't add duplicates as we visit all of the
	// entries on our journey...

	AB_ABIDArray idsAlreadyAdded; // we'll eventually want to make this a sorted array for performance gains on the look ups..
	int status = AB_SUCCESS;

	if (ctr)
		status = ctr->GetExpandedHeaderForEntry(expandedString, srcPane, entryID, idsAlreadyAdded);

	idsAlreadyAdded.RemoveAll();
	return status;
}

int AB_ContainerInfo::GetExpandedHeaderForEntry(char ** expandedString, MSG_Pane * srcPane, ABID entryID, AB_ABIDArray idsAlreadyAdded)
{
	// this is a recursive function because if we are a mailing list, we need to expand its header entries...
	int status = AB_FAILURE; // i'm feeling pessimistic...
	AB_AttributeValue * value = NULL;

	// make sure we haven't already added this ABID....
	if (idsAlreadyAdded.FindIndex(0, (void *) entryID) != -1 /* make sure we can't find it */)
		return AB_SUCCESS; // we've already visited this entry 

	// we have 2 cases:
	// (1) the base case is a person entry...format the person, and add them to the expand string
	// (2) the recursive call...for mailing lists...create a mailing list ctr, add each entry in the mlist ctr

	if (entryID != AB_ABIDUNKNOWN)
	{
		GetEntryAttribute(srcPane, entryID, AB_attribEntryType, &value);
		if (value)
		{
			// we have two cases: the base case is a person entry...make sure the person was not added already, 
			// and 
			if (value->u.entryType == AB_Person)
			{
				char * fullAddress = GetFullAddress(srcPane, entryID);
				if (fullAddress)
				{
					char * name = NULL;
					char * address = NULL;
					MSG_ParseRFC822Addresses(fullAddress, &name, &address);
					AB_AddressParser * parser = NULL;
					AB_AddressParser::Create(NULL, &parser);
					if (parser)
					{
						parser->FormatAndAddToAddressString(expandedString, name, address, TRUE /* ensure domain name */);
						parser->Release();
						parser = NULL;
						status = AB_SUCCESS;
					}
					XP_FREEIF(name);
					XP_FREEIF(address);
					XP_FREEIF(fullAddress);
				}
			}
			else	// we are a mailing list ctr
			{
				AB_ContainerInfo * mListCtr = NULL;
				AB_ContainerInfo::Create(m_context, this, entryID, &mListCtr);
				if (mListCtr)
				{
					for (MSG_ViewIndex index = 0; index < mListCtr->GetNumEntries(); index++)
					{
						ABID memberID = mListCtr->GetABIDForIndex(index);
						if (memberID != AB_ABIDUNKNOWN)
							mListCtr->GetExpandedHeaderForEntry(expandedString,srcPane, memberID, idsAlreadyAdded);
					}
					mListCtr->Release();
					status = AB_SUCCESS;
				}
				else
					status = AB_FAILURE;
			}
			AB_FreeEntryAttributeValue(value);
		}
		else
			status = ABError_NullPointer;
	}
	else
		status = ABError_NullPointer;

	return status;
}

char * AB_ContainerInfo::GetFullAddress(MSG_Pane * srcPane, ABID entryID)
{
	AB_AttributeValue * emailAddress = NULL;
	AB_AttributeValue * displayName = NULL;
	char * displayString = NULL;
	char * email = NULL;

	// get necessary attributes
	GetEntryAttribute(srcPane, entryID, AB_attribEmailAddress, &emailAddress);
	GetEntryAttribute(srcPane, entryID, AB_attribDisplayName, &displayName);

	if (displayName)
		displayString = displayName->u.string;
	if (emailAddress)
		email = emailAddress->u.string;

	char * fullAddress = MSG_MakeFullAddress(displayString, email);
	if (!fullAddress) // then try our best...
		if (displayString)
			fullAddress = XP_STRDUP(displayString);
		else
			if (email)
				fullAddress = XP_STRDUP(email);
			

	AB_FreeEntryAttributeValue(emailAddress);
	AB_FreeEntryAttributeValue(displayName);

	return fullAddress;
}

AB_Column_eAttribute AB_ContainerInfo::ConvertAttribToDBase(AB_AttribID attrib)
{
	// this stuff really should be just temporary. Once things settle down, we want the FEs to use the database
	// attribute types. When they do, then we won't need to convert...
	switch(attrib)
	{
		case AB_attribEntryType:
			return AB_Attrib_kIsPerson;
		case AB_attribEntryID:
			return AB_Attrib_kUid;
		case AB_attribFullName:
			return AB_Attrib_kFullName;
		case AB_attribNickName:
			return AB_Attrib_kNickname;
		case AB_attribGivenName:
			return AB_Attrib_kGivenName;
		case AB_attribMiddleName:
			return AB_Attrib_kMiddleName;
		case AB_attribFamilyName:
			return AB_Attrib_kFamilyName;
		case AB_attribCompanyName:
			return AB_Attrib_kCompanyName;
		case AB_attribLocality:
			return AB_Attrib_kLocality;
		case AB_attribRegion:
			return AB_Attrib_kRegion;
		case AB_attribEmailAddress:
			return AB_Attrib_kEmail;
		case AB_attribInfo:
			return AB_Attrib_kInfo;
		case AB_attribHTMLMail:
			return AB_Attrib_kHtmlMail;
		case AB_attribExpandedName:
			return AB_Attrib_kExpandedName;
		case AB_attribTitle:
			return AB_Attrib_kTitle;
		case AB_attribPOAddress:
			return AB_Attrib_kPostalAddress;
		case AB_attribStreetAddress:
			return AB_Attrib_kStreetAddress;
		case AB_attribZipCode:
			return AB_Attrib_kZip;
		case AB_attribCountry:
			return AB_Attrib_kCountry;
		case AB_attribWorkPhone:
			return AB_Attrib_kWorkPhone;
		case AB_attribFaxPhone:
			return AB_Attrib_kFax;
		case AB_attribHomePhone:
			return AB_Attrib_kHomePhone;
		case AB_attribDistName:
			return AB_Attrib_kDistName;
		case AB_attribSecurity:
			return AB_Attrib_kSecurity;
		case AB_attribCoolAddress:
			return AB_Attrib_kCoolAddress;
		case AB_attribUseServer:
			return AB_Attrib_kUseServer;
		case AB_attribDisplayName:
/*			return AB_Attrib_kDisplayName; */
			return AB_Attrib_kFullName;
		case AB_attribWinCSID:
			return AB_Attrib_kCharSet;
			/* WARNING: RIGHT NOW DBASE DOES NOT SUPPORT PAGER AND CELL PHONE YET THE FE CODE IS TRYING TO SET THEM....
			THIS IS CAUSING SOME BAD PROBLEMS. TO FIX THIS, WE ARE GOING TO MAP THESE TO A VALID ATTRIBUTE: 

  */
		case AB_attribPager:
/*			return AB_Attrib_kPager; */
			return AB_Attrib_kDistName;
		case AB_attribCellularPhone:
/*			return AB_Attrib_kCellPhone; */
			return AB_Attrib_kDistName;
			
		default:
			XP_ASSERT(0);
			return AB_Attrib_kNumColumnAttributes;   // we need an invalid attribute enum from the database for here!!
	}
}

// Database related methods (Note: each subclass has its own behavior for dbase interaction...
const char * AB_ContainerInfo::ConvertAttribValueToDBase(AB_AttributeValue * value)
{
	// all database values are strings! So we must map any of our attributeValues that are not strings, to strings...
	// if you add a non-string attribute here, make sure you also modify AB_CopyEntryAttributeValue in abglue.cpp
	if (value)
	{
		switch (value->attrib)
		{
		case AB_attribHTMLMail:
			if (value->u.boolValue)
				return "t";
			else
				return "f";
		case AB_attribEntryType:
			if (value->u.entryType == AB_Person)
				return "t";
			else
				return "f";
		case AB_attribWinCSID:
		case AB_attribSecurity:
		case AB_attribUseServer:
			// convert short value to a string...
			// for now just return NULL
			return NULL;
		case AB_attribDisplayName:
			// HACK ALERT!!! Right now we have a database corruption at 20 characters....until this if fixed we are going
			// to truncate long display names to prevent the corruption....
			if (value->u.string && XP_STRLEN(value->u.string) >= 18)
				value->u.string[17] = '\0'; // insert the null termination early!!!!!
		default: // string attribute
			if (AB_IsStringEntryAttributeValue(value))
				return value->u.string;

		}
	}

	return NULL;  // we received an invalid attribute!
}

int AB_ContainerInfo::ConvertDBaseValueToAttribValue(const char * dbaseValue, AB_AttributeValue * value /* already allocated */, AB_AttribID attrib)
// if you call this function, make sure value is a fresh value because I do not free any string attribute values that might be inside it.
// Why? Because I have no idea if you are passing me an unitialized value or a previously used value so I can't really ask the value
// its type. Caller is responsible for making sure it is a fresh value.
{
	if (dbaseValue)
	{

		value->attrib = attrib;

		switch(attrib)
		{
		case AB_attribHTMLMail:
			if (XP_STRCMP(dbaseValue, "t") == 0)
				value->u.boolValue = TRUE;
			else
				value->u.boolValue = FALSE;
			break;
		case AB_attribEntryType:
			if (XP_STRCMP(dbaseValue, "t") == 0)
				value->u.entryType = AB_Person;
			else
				value->u.entryType = AB_MailingList;
			break;
		case AB_attribWinCSID:
		case AB_attribSecurity:
		case AB_attribUseServer:
			// convert string to integer
			// HACK JUST FOR NOW...
			value->u.shortValue = 0;
			break;
		default:  // as always, default is a string attribute
			if (XP_STRLEN(dbaseValue) > 0)
				value->u.string = XP_STRDUP(dbaseValue);
			else
				value->u.string = XP_STRDUP("");
			break;
		}

		return AB_SUCCESS;

	}
	else
		return AB_FAILURE;  // we didn't have a dbaseValue to convert
}

int AB_ContainerInfo::TypedownSearch(AB_Pane * requester, const char * typeDownValue, MSG_ViewIndex startIndex)
{
	// begin a database search on the type down value. This gives us a table with rows for all of the matches.
	// For now, ignore the start Index (LDAP needs this not us). Get the row_uid for the first row in the 
	// search results table. Turn around and get the index for this row in the big dbaseTable. This is our type down
	// index!!

	int status = AB_SUCCESS;
	MSG_ViewIndex typedownIndex = startIndex;

	AB_Env_ForgetErrors(m_db->db_env);
	if (m_db->db_table)
	{
		ab_column_uid colID = AB_Table_GetSortColumn(m_db->db_table, m_db->db_env);
		AB_Env_ForgetErrors(m_db->db_env); // come back and pick up errors later

		ab_row_uid rowUID = AB_Table_FindFirstRowWithPrefix(m_db->db_table, m_db->db_env, typeDownValue, colID);
		// convert the row_uid we got back into a row positon
		if (AB_Env_Good(m_db->db_env) && rowUID)
		{
			ab_row_pos rowPos = AB_Table_RowPos(m_db->db_table, m_db->db_env, rowUID);
			if (rowPos)
				typedownIndex = (MSG_ViewIndex) rowPos - 1; // row positions are 1 based. viewIndices are 0 based
		}
	}

	AB_Env_ForgetErrors(m_db->db_env);

	if (requester)
		FE_PaneChanged(requester, TRUE, MSG_PaneNotifyTypeDownCompleted, typedownIndex);

	return status;
}

/* Okay, the policy on database notifications is that the base class will ignore them!!! It is up to the 
   container subclasses to decide what actions they wish to take. */

void AB_ContainerInfo::SeeBeginModelFlux(ab_Env* /* ev */, ab_Model* /* m */)
{
}

void AB_ContainerInfo::SeeEndModelFlux(ab_Env* /* ev */, ab_Model* /* m */)
{
}

void AB_ContainerInfo::SeeChangedModel(ab_Env* /* ev */, ab_Model* /* m */, const ab_Change* /* c */)
{
}
    
void AB_ContainerInfo::SeeClosingModel(ab_Env* /* ev */, ab_Model* /* m */, const ab_Change* /* c */)  // database is going away
{
	if (m_objectState == AB_ObjectOpen)  // if we aren't already closing or closed...
		Close();
}

/*******************************************************************************************

  Asynchronous copy methods -> begin, more, interrupt and finish

*********************************************************************************************/
/* static */ char * AB_ContainerInfo::BuildCopyUrl() // url string for a addbook-copy url
{
	return XP_STRDUP("addbook:copy");
}

/* static */ void AB_ContainerInfo::PostAddressBookCopyUrlExitFunc (URL_Struct *URL_s, int /* status */, MWContext * /* window_id */)
{
	// it is also possible to enter this routine if the url was interrupted!!!

	// address book copy urls always have an AB_AddressBookCopyInfo in the owner_data field...we need to free this.
	 if (URL_s)
	 {
		 AB_AddressBookCopyInfo * copyInfo = (AB_AddressBookCopyInfo *) URL_s->owner_data;
		 AB_FreeAddressBookCopyInfo(copyInfo);
		 URL_s->owner_data = NULL;
	 }

	 // url struct gets freed in netlib....
}

int AB_ContainerInfo::BeginCopyEntry(MWContext * context, AB_AddressBookCopyInfo * abCopyInfo, void ** copyCookie, XP_Bool * copyFinished)
{
	// (1) set status text based on action
	// (2) set copyCookie to 0 because the first id we want to look at
	int status = AB_SUCCESS;
	if (abCopyInfo)
	{
		char * statusText = NULL;
		char * destDescription = NULL;
		if (abCopyInfo->destContainer)
			destDescription = abCopyInfo->destContainer->GetDescription();

		switch (abCopyInfo->state)
		{
			case AB_CopyInfoCopy:
				if (destDescription)
					statusText = PR_smprintf (XP_GetString(MK_ADDR_COPY_ENTRIES), destDescription);
				else
					statusText = PR_smprintf (XP_GetString(MK_ADDR_COPY_ENTRIES), XP_GetString(MK_MSG_ADDRESS_BOOK));
				break;
			case AB_CopyInfoMove:
				if (destDescription)
					statusText = PR_smprintf (XP_GetString(MK_ADDR_MOVE_ENTRIES), destDescription);
				else
					statusText = PR_smprintf (XP_GetString(MK_ADDR_MOVE_ENTRIES), XP_GetString(MK_MSG_ADDRESS_BOOK));
				break;
			case AB_CopyInfoDelete:
				if (m_description)
					statusText = PR_smprintf (XP_GetString(MK_ADDR_DELETE_ENTRIES), m_description);
				else
					statusText = PR_smprintf (XP_GetString(MK_ADDR_DELETE_ENTRIES), XP_GetString(MK_MSG_ADDRESS_BOOK));
				break;
			default:
				XP_ASSERT(FALSE);
				break;
		}

		// now set the status text
		if (statusText)
		{
			FE_Progress (context, statusText);
			XP_FREE(statusText);
		}

		XP_FREEIF(destDescription);
		// now initialize the progress to 0....since we are just starting
		FE_SetProgressBarPercent (context, 0);
		
		// intialize our copyInfo state
		if (copyCookie)
		{
			*copyCookie = (void *) 0;  // we always start from 0;
			MoreCopyEntry(context, abCopyInfo, copyCookie, copyFinished);
		}
		else
			status = ABError_NullPointer;
	}
	else
		status = ABError_NullPointer;

	return status;
}

int AB_ContainerInfo::MoreCopyEntry(MWContext * context, AB_AddressBookCopyInfo * abCopyInfo, void ** copyCookie, XP_Bool * copyFinished)
{
	int32 actionPosition = 0;
	int32 numToCopy = 0;
	int status = AB_SUCCESS;
	XP_Bool finished = FALSE;

	if (copyCookie && abCopyInfo)
	{
		actionPosition = (int32) (*copyCookie); // get the position we are in...
		if ( abCopyInfo->numItems - actionPosition > 0)
		{
			numToCopy = AB_kCopyInfoAmount < (abCopyInfo->numItems - actionPosition) ? AB_kCopyInfoAmount : abCopyInfo->numItems - actionPosition;
			switch (abCopyInfo->state)
			{
				case AB_CopyInfoCopy:
					status = CopyEntriesTo(abCopyInfo->destContainer, &abCopyInfo->idArray[actionPosition], numToCopy, FALSE);
					break;
				case AB_CopyInfoMove:
					status = MoveEntriesTo(abCopyInfo->destContainer, &abCopyInfo->idArray[actionPosition], numToCopy);
					break;
				case AB_CopyInfoDelete:
					status = DeleteEntries(&abCopyInfo->idArray[actionPosition], numToCopy, NULL);
					break;
				default:
					XP_ASSERT(FALSE);
					status = AB_FAILURE;
					break;
			}

			// update progress bar...
			actionPosition += numToCopy;
			int32 percent = (int32) (100 * (((double) (actionPosition+1)) / ((double) abCopyInfo->numItems)));
			FE_SetProgressBarPercent (context, percent);
			// update action position to mark ourselves as done...

			*copyCookie = (void *) actionPosition; // update our position for the next pass...
			if (actionPosition == abCopyInfo->numItems)
				finished = TRUE;
		}
		else
			finished = TRUE;
	}
	else
		status = ABError_NullPointer;

	if (copyFinished)
		*copyFinished = finished;

	return status;
}

int AB_ContainerInfo::FinishCopyEntry(MWContext * /* context */, AB_AddressBookCopyInfo * /* copyInfo */, void ** /* copyCookie */)
{
	return AB_SUCCESS;

}

int AB_ContainerInfo::InterruptCopyEntry(MWContext * context, AB_AddressBookCopyInfo * copyInfo, void ** copyCookie)
{
	return FinishCopyEntry(context, copyInfo, copyCookie);
}

int AB_ContainerInfo::DeleteEntries(ABID * /* ids */, int32 /* numIndices */, AB_ContainerListener * /* instigator*/ )
{
	// what do I know as a basic container about deleting? not much....
	return AB_SUCCESS;
}

int AB_ContainerInfo::CopyEntriesTo(AB_ContainerInfo * destContainer, ABID * idArray, int32 numItems, XP_Bool deleteAfterCopy)
{
	// base class implementation, again assumes the src ctr uses a database...
	// we could rewrite this stuff to not copy rows from dbase tables, instead generating an array of attributes
	// and calling addUser on those attributes....
	if (destContainer && destContainer != this && m_db->db_table && destContainer->GetTable())
	{
		for (int32 i = 0; i < numItems; i++)
		{
			if (idArray[i] != AB_ABIDUNKNOWN)
			{
				AB_Env_ForgetErrors(m_db->db_env);
				ab_row_uid rowUid = AB_Table_CopyRow(destContainer->GetTable(),m_db->db_env,m_db->db_table, (ab_row_uid) idArray[i]);

				// what should we do if an individual copy fails????
				if (deleteAfterCopy && rowUid && AB_Env_Good(m_db->db_env)) // no errors, copy succeeded and we need to delete after copy
					DeleteEntries(&idArray[i], 1, NULL);
			}
		}
	}
	return AB_SUCCESS;
}

int AB_ContainerInfo::MoveEntriesTo(AB_ContainerInfo * destContainer, ABID * idArray, int32 numItems)
{
	return CopyEntriesTo(destContainer, idArray, numItems, TRUE /* remove after copy */);
}


// added to aid debugging with the databae.
char* AB_ContainerInfo::ObjectAsString(ab_Env* ev, char* outXmlBuf) const
{
        AB_USED_PARAMS_1(ev);
		char * description = "";
		if (m_description)
			description = m_description;
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
        sprintf(outXmlBuf,
                "<AB_ContainerInfo me=\"^%lX\" describe=\"%.64s\"/>",
                (long) this,                        // me=\"^%lX\"
                description              // describe=\"%.64s\"
                );
#else
        *outXmlBuf = 0; /* empty string */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
        return outXmlBuf;
}

/* static */ int AB_ContainerInfo::CompareByType(const void * ctr1, const void * ctr2)
{

/* Ranked as: PAB, LDAP, Mailing List, unknown...
	0 means they are equal
	-1 means ctr1 < ctr2
	1 means ctr 1 > ctr 2 */

	int status = 0; // assume equal by default
	AB_ContainerType ctr1Type = AB_UnknownContainer;
	AB_ContainerType ctr2Type = AB_UnknownContainer;

	if (ctr1)
		((AB_ContainerInfo *) ctr1)->GetType();
	
	if (ctr2)
		((AB_ContainerInfo *) ctr2)->GetType();

	if (ctr1Type == ctr2Type) // simplest case
		status = 0;
	else
		switch (ctr1Type)
		{
			case AB_PABContainer:
				status = -1;
				break;
			case AB_LDAPContainer:
				if (ctr2Type == AB_PABContainer) // is it less ? 
					status = 1;
				else
					status = -1;
				break;
			case AB_MListContainer:
				if (ctr2Type == AB_UnknownContainer) 
					status = -1;
				else
					status = 1;
				break;
			case AB_UnknownContainer:
			default:
				status = 1;  // ctr1 has to be greater
				break;
		}

	return status;
}

/*****************************************************************************************************************

  Definitions for AB_DirectoryContainerInfo class. Responsibilities include container cache and maintaining/freeing 
	description character string. 

  ****************************************************************************************************************/

AB_DirectoryContainerInfo::AB_DirectoryContainerInfo(MWContext * context, DIR_Server * server) : AB_ContainerInfo(context)
{
	m_server = server;
	m_description = NULL;
	LoadDescription();
	LoadDepth();
}

AB_DirectoryContainerInfo::~AB_DirectoryContainerInfo()
{
	// make sure we are not in the cache!
	XP_ASSERT(m_description == NULL);
	XP_ASSERT(m_server == NULL);
	if (m_IsCached) // only look in cache if it is a cached object
		XP_ASSERT(CacheLookup(m_server) == NULL);

}

int AB_DirectoryContainerInfo::UpdateDIRServer()
{
	// what attributes do we need to update?? 
	LoadDescription();
	LoadDepth();
	// the description could have changed so notify listeners!
	NotifyContainerAttribChange(this, AB_NotifyPropertyChanged, NULL);
	XP_List * servers = DIR_GetDirServers();
	DIR_SaveServerPreferences(servers);      // make sure the change is committed 
	return AB_SUCCESS;
}

int AB_DirectoryContainerInfo::Close()
{
	CacheRemove();

	if (m_server)
		m_server = NULL;

	if (m_description)
	{
		XP_FREE(m_description);
		m_description = NULL;
	}
	return AB_ContainerInfo::Close();
}
	
/* static */ int AB_DirectoryContainerInfo::Create(MWContext * context, DIR_Server * server, AB_ContainerInfo ** newCtr)
{
	int status = AB_SUCCESS;
	// first look up the dir server in the cache..
	AB_DirectoryContainerInfo * ctr = CacheLookup(server);
	if (ctr) // is it in the cache?
		ctr->Acquire(); 
	else
	{
		// here's where we need to decide what type of container we are now creating...
		// we're going to cheat and look into the DIR_Server for its type.
		if (server)
		{
			AB_Env * env = AB_Env_New();
			env->sEnv_DoTrace = 0;
			if (env)
			{
				ab_Env * ev = ab_Env::AsThis(env);
				if (server->dirType == LDAPDirectory)
					ctr = new (*ev) AB_LDAPContainerInfo(context, server);
				else
					if (server->dirType == PABDirectory)
						ctr = new (*ev) AB_PABContainerInfo(context, server);
				env = ev->AsSelf();
				if (AB_Env_Bad(env) || !ctr)  // error during creation,
				{
					if (ctr)  // object is now useless...
					{
						status = AB_FAILURE;
						ctr->GoAway();
						ctr = NULL;
					}
					else
						status = AB_OUT_OF_MEMORY;
				}
				
				AB_Env_Release(env);
			}		
			
			if (!ctr)
				status =  AB_OUT_OF_MEMORY;
			else
				ctr->CacheAdd(); // add the new container to the cache
		}
	}

	*newCtr = ctr;
	return status;
}


/* Use this call to look up directory containers where you have a DIR_Server */
/* static */ AB_DirectoryContainerInfo * AB_DirectoryContainerInfo::CacheLookup (DIR_Server * server)
{
	if (m_ctrCache)
	{
		XP_List * walkCache = m_ctrCache;
		AB_DirectoryContainerInfo * walkCtr;

		do
		{
			walkCtr = (AB_DirectoryContainerInfo *) XP_ListNextObject(walkCache);
			if (walkCtr && (DIR_AreServersSame(walkCtr->m_server,server)))
				return walkCtr;
		} while (walkCtr);
	}
	return NULL;
}

XP_List * AB_DirectoryContainerInfo::m_ctrCache = NULL;

void AB_DirectoryContainerInfo::CacheAdd()
{
	if (!m_ctrCache)
		m_ctrCache = XP_ListNew();
	XP_ListAddObject(m_ctrCache, this);
}

void AB_DirectoryContainerInfo::CacheRemove ()
{
	if (m_IsCached) // only remove from cache if it is a cached object
	{
		XP_ListRemoveObject (m_ctrCache, this);

		// delete list when empty
		if (XP_ListCount (m_ctrCache) == 0)
		{
			XP_ListDestroy (m_ctrCache);
			m_ctrCache = NULL;
		}
	}
}

void AB_DirectoryContainerInfo::LoadDepth()
{
	m_depth = 0; // all directory containers are at the root level!
}

void AB_DirectoryContainerInfo::LoadDescription()
{
	if (m_description)
		XP_FREE(m_description); 
	if (m_server && m_server->description)
	{
		if (XP_STRLEN(m_server->description) > 0)
			m_description = XP_STRDUP(m_server->description);
	}
	else
		m_description = NULL;
}


int AB_DirectoryContainerInfo::m_offlineCallbackCount = 0;

static int PR_CALLBACK OfflineContainerCallback(const char * /* prefName */, void *)
{
	XP_List * walkCache = AB_DirectoryContainerInfo::GetContainerCacheList();
	if (walkCache)
	{
		AB_DirectoryContainerInfo * walkCtr;

		do
		{
			walkCtr = (AB_DirectoryContainerInfo *) XP_ListNextObject(walkCache);
			if (walkCtr)
				walkCtr->SelectDatabase();
		} while (walkCtr);
	}

	return PREF_NOERROR;
}

/* static */ void AB_DirectoryContainerInfo::RegisterOfflineCallback()
{
	if (m_offlineCallbackCount++ == 0)
		PREF_RegisterCallback("network.online", OfflineContainerCallback, NULL);
}

/* static */ void AB_DirectoryContainerInfo::UnregisterOfflineCallback()
{
	if (--m_offlineCallbackCount == 0)
		PREF_UnregisterCallback("network.online", OfflineContainerCallback, NULL);
}


int AB_DirectoryContainerInfo::NameCompletionSearch(AB_Pane * requester, const char * ncValue)
{
	int status = AB_SUCCESS;
	AB_Env_ForgetErrors(m_db->db_env);
	if (m_db->db_table && ncValue)
	{
		ab_column_uid colID = AB_Table_GetSortColumn(m_db->db_table, m_db->db_env);
		AB_Env_ForgetErrors(m_db->db_env); // come back and pick up errors later

		ab_column_uid colIDs[2];
		colIDs[0] = colID;
		colIDs[1] = 0;

		AB_Table * results = AB_Table_AcquireSearchTable(m_db->db_table, m_db->db_env, ncValue, colIDs);
		if (results)
		{
			// for each search result match, have the requester process it...
			for (ab_row_pos pos = 1; pos <= AB_Table_CountRows(results, m_db->db_env) && AB_Env_Good(m_db->db_env); pos++)
			{
				ab_row_uid rowUID = AB_Table_GetRowAt(results, m_db->db_env, pos);
				if (rowUID) // notify requester that we found a match
					requester->ProcessSearchResult(this, (ABID) rowUID); // notify requester that we found a match
			}

			AB_Table_Release(results, m_db->db_env);
			results = NULL;
		}
	}
	
	AB_Env_ForgetErrors(m_db->db_env);
	requester->SearchComplete(this);  // our work here is done
	return status;
}

/***********************************************************************************************************************

  Definitions for the Non-Directory Container Info Class. Responsibilities include container caching, description string 
	and maintaining parent container / entry ID pair. 

 ***********************************************************************************************************************/

AB_NonDirectoryContainerInfo::AB_NonDirectoryContainerInfo(MWContext * context, AB_ContainerInfo * parentCtr, ABID entryID) : AB_ContainerInfo(context)
{
	m_description = NULL;
	m_parentCtr = parentCtr;
	m_entryID = entryID;
	
	m_description = NULL;
	LoadDescription();
	m_depth = 0;
	LoadDepth();
}

AB_NonDirectoryContainerInfo::~AB_NonDirectoryContainerInfo()
{
	XP_ASSERT(m_description == NULL);
	XP_ASSERT(CacheLookup(m_parentCtr, m_entryID) == NULL);
}

int AB_NonDirectoryContainerInfo::Close()
{
	if (IsOpen())
	{
		CacheRemove();
		if (m_description)
		{
			XP_FREE(m_description);
			m_description = NULL;
		}

		AB_ContainerInfo::Close();

		if (m_parentCtr)
		{
			m_parentCtr->Release();
			m_parentCtr = NULL;
		}
	}
	return AB_SUCCESS;
}

/* static */ int AB_NonDirectoryContainerInfo::Create(MWContext * context, AB_ContainerInfo * parentCtr, ABID entryID, AB_ContainerInfo ** newCtr)
{
	int status = AB_SUCCESS;

	// first look to see if we have already created this container...
	AB_NonDirectoryContainerInfo * ctr = AB_NonDirectoryContainerInfo::CacheLookup(parentCtr, entryID);
	if (ctr) // is it in the cache?
		ctr->Acquire();
	else
	{
		// what type of container are we creating? Right now, the only non-directory container we have is a mailing list
		// container...we might have more types later on...insert them here
		if (parentCtr)
		{
			AB_Env * env = AB_Env_New();
			env->sEnv_DoTrace = 0;
			if (env)
			{
				ab_Env * ev = ab_Env::AsThis(env);
				ctr = new (* ev) AB_MListContainerInfo (context, parentCtr, entryID);

				env = ev->AsSelf();
				if (AB_Env_Bad(env) || !ctr)  // error during creation,
				{
					if (ctr)  // object is now useless...
					{
						status = AB_FAILURE;
						ctr->GoAway();
					}
					else
						status = AB_OUT_OF_MEMORY;
				}
				AB_Env_Release(env);
			}
			if (!ctr)
			{
				*newCtr = NULL;
				return AB_OUT_OF_MEMORY;
			}
			ctr->CacheAdd();
		}
	}

	*newCtr = ctr;
	return status;
}

/* static */ AB_NonDirectoryContainerInfo * AB_NonDirectoryContainerInfo::CacheLookup(AB_ContainerInfo * parentCtr, ABID entryID /* ABID in parent */)
{
	if (m_ctrCache)
	{
		XP_List * walkCache = m_ctrCache;
		AB_NonDirectoryContainerInfo * walkCtr;
		do
		{
			walkCtr = (AB_NonDirectoryContainerInfo *) XP_ListNextObject(walkCache);
			if (walkCtr && (walkCtr->m_parentCtr == parentCtr) && walkCtr->m_entryID == entryID)
				return walkCtr;
		} while (walkCtr);
	}

	return NULL;
}

XP_List * AB_NonDirectoryContainerInfo::m_ctrCache = NULL;

void AB_NonDirectoryContainerInfo::CacheAdd()
{
	if (!m_ctrCache)
		m_ctrCache = XP_ListNew();
	XP_ListAddObject(m_ctrCache, this);
}

void AB_NonDirectoryContainerInfo::CacheRemove ()
{
	XP_ListRemoveObject (m_ctrCache, this);

	// delete list when empty
	if (XP_ListCount (m_ctrCache) == 0)
	{
		XP_ListDestroy (m_ctrCache);
		m_ctrCache = NULL;
	}
}

void AB_NonDirectoryContainerInfo::LoadDepth()
{
	// our depth is one greater than our parents depth!
	m_depth = 0;
	AB_ContainerAttribValue * value = NULL;
	if (m_parentCtr)
	{
		m_parentCtr->GetAttribute(attribDepth, &value);
		if (value)
		{
			m_depth = value->u.number + 1;
			AB_FreeContainerAttribValue(value);
		}
	}
}

void AB_NonDirectoryContainerInfo::LoadDescription()
{
	// our description is actually obtained from a field in the parent container entryID
	if (IsOpen())
	{
		AB_AttributeValue * value;
		if (m_parentCtr)
			if (m_parentCtr->GetEntryAttribute(NULL, m_entryID, /* AB_attribFullName */ AB_attribDisplayName, &value) == AB_SUCCESS)
			{
				if (value->u.string)
					m_description = XP_STRDUP(value->u.string);
				else
					m_description = NULL;
				AB_FreeEntryAttributeValue(value);
			}
	}
}

DIR_Server * AB_NonDirectoryContainerInfo::GetDIRServer()
{
	if (m_parentCtr)
		return m_parentCtr->GetDIRServer();
	else
		return NULL;
}

// we want to remove the ctr from its parent...
int AB_NonDirectoryContainerInfo::DeleteSelfFromParent(AB_ContainerListener * instigator)
{
	// this causes an ugly FE crash...Not a release for the brave stopper so I'm going to
	// leave it out for now....

	if (m_parentCtr)
		return m_parentCtr->DeleteEntries(&m_entryID,1, instigator); 
	else
		return AB_FAILURE;  // not implemented yet...
}


/*****************************************************************************

  Definitions for the LDAP Container Info Class.

 *****************************************************************************/
uint32 AB_LDAPEntry::m_sortAttributes = 0;
AB_AttribID AB_LDAPEntry::m_sortOrder[3];

/* AB_LDAPEntry Constructor
 */
AB_LDAPEntry::AB_LDAPEntry()
{
	SetSortOrder();
}

/* GetPrimarySortAttribute
 */
AB_AttribID AB_LDAPEntry::GetPrimarySortAttribute()
{
	return m_sortOrder[0];
}

/* SetSortOrder
 */
void AB_LDAPEntry::SetSortOrder(AB_AttribID *list, uint32 num)
{
	if (list == NULL)
	{
		m_sortOrder[0] = AB_attribDisplayName;
		m_sortAttributes = 1;
	}
	else
	{
		if (num > 3)
			num = 3;

		m_sortAttributes = num;
		for (uint32 i = 0; i < num; i++)
			m_sortOrder[i] = list[i];
	}
}

/* Compare
 *
 * AB_LDAPEntry overloaded operator helper
 *
 * NOTE: this function assumes that the attributes for both entries will
 *       be in the same order and positions within the values array.  This
 *       a safe assumption given the current implementation of
 *       ConvertResultElement.
 */
int AB_LDAPEntry::Compare(AB_LDAPEntry &entry)
{
	int rc = 0;
	uint32 i, j;

	/* First, compare the sort attributes.  The attribute at index 0
	 * is always AB_attribEntryType == AB_Person, we can skip it.
	 */
	for (i = 0; i < m_sortAttributes; i++)
	{
		for (j = 1; j < m_numAttributes; j++)
			if (m_values[j].attrib == m_sortOrder[i])
				break;

		if (j < m_numAttributes)
		{
			if (m_values[j].u.string && entry.m_values[j].u.string)
				rc = XP_STRCASECMP(m_values[j].u.string, entry.m_values[j].u.string);
			else if (m_values[j].u.string)
				rc = 1;
			else if (entry.m_values[j].u.string)
				rc = -1;

			if (rc != 0)
				return rc;
		}
	}

	/* Second, if we're doing an extended compare (necessary for equality
	 * comparisons), compare the remaining attributes.  This is a bit redundant
	 * but it is perfectly valid since we know the values in the sort
	 * attributes are identical.
	 */
	for (i = 1; i < m_numAttributes; i++)
	{
		if (m_values[i].u.string && entry.m_values[i].u.string)
			rc = XP_STRCASECMP(m_values[i].u.string, entry.m_values[i].u.string);
		else if (m_values[i].u.string)
			rc = 1;
		else if (entry.m_values[i].u.string)
			rc = -1;

		if (rc != 0)
			return rc;
	}

	return 0;
}


/* Constructor - AB_LDAPContainerInfo
 *
 * This is a protected constructor, it should only be called by
 * AB_ContainerInfo::Create which calls it if the container does not already
 * exist.
 */

AB_LDAPContainerInfo::AB_LDAPContainerInfo(MWContext *context,
                                           DIR_Server *server, XP_Bool useDB)
                     :AB_DirectoryContainerInfo(context, server)
{
	m_interruptingSearch = FALSE;
	m_performingVLV = FALSE;
	m_replicating = FALSE;

	m_updateTotalEntries = TRUE;
	m_totalEntries = 0;
	m_firstIndex = AB_LDAP_UNINITIALIZED_INDEX;
	m_lastIndex = AB_LDAP_UNINITIALIZED_INDEX;
	m_searchType = ABLST_None;

	m_entryMatch.m_isValid = FALSE;

	m_dbNormal = m_db;
	if (server->replInfo)
	{
		m_dbReplica = (AB_Database *)XP_CALLOC(1, sizeof(AB_Database));

		/* If we are currently offline, switch to the replica database.
		 */
		if (NET_IsOffline())
			m_db = m_dbReplica;
	}
	if (useDB)
		InitializeDatabase(m_db);
}

/* Destructor - AB_LDAPContainerInfo
 */
AB_LDAPContainerInfo::~AB_LDAPContainerInfo()
{
	// assert on any object that should have been closed or deleted
	// by now. (i.e. make sure close was called b4 you get here)
}

int AB_LDAPContainerInfo::Close()
{
	uint32 i;

	/* Free the VLV match entry.
	 */
	if (m_entryMatch.m_isValid)
	{
		m_entryMatch.m_isValid = FALSE;
		AB_FreeEntryAttributeValues(m_entryMatch.m_values,
		                            m_entryMatch.m_numAttributes);
	}

	/* Free the VLV cache.
	 */
	for (i = m_entryList.GetSize(); i > 0; i--)
	{
		AB_LDAPEntry *entry = (AB_LDAPEntry *)m_entryList[i-1];
		if (entry->m_isValid)
		{
			entry->m_isValid = FALSE;
			AB_FreeEntryAttributeValues(entry->m_values,
		   	                            entry->m_numAttributes);
		}
		XP_FREE(entry);
	}
	m_entryList.RemoveAll();

	/* Free the name completion cache.
	 */
	for (i = m_ncEntryList.GetSize(); i > 0; i--)
	{
		AB_LDAPEntry *entry = (AB_LDAPEntry *)m_ncEntryList[i-1];
		AB_FreeEntryAttributeValues(entry->m_values, entry->m_numAttributes);
		XP_FREE(entry);
	}
	m_ncEntryList.RemoveAll();

	/* Close the entry databases.
	 */
	m_db = NULL;
	if (m_dbNormal)
	{
		if (m_dbNormal->db_env)
			CloseDatabase(m_dbNormal);
		XP_FREE(m_dbNormal);
		m_dbNormal = NULL;
	}
	if (m_dbReplica)
	{
		if (m_dbReplica->db_env)
			CloseDatabase(m_dbReplica);
		XP_FREE(m_dbReplica);
		m_dbReplica = NULL;
	}

	return AB_DirectoryContainerInfo::Close();
}

void AB_LDAPContainerInfo::InitializeDatabase(AB_Database *db)
{
	// Select the appropriate file name for the database we want to open.
	char *fileName, *fullFileName;
	if (db == m_dbNormal)
		fileName = m_server->fileName;
	else
		fileName = m_server->replInfo->fileName;
	DIR_GetServerFileName(&fullFileName, fileName);

	// okay, we need to get the environment, open a store based on the filename, and then open a table for the address book
	db->db_fluxStack = 0;
	db->db_listTable = NULL; // LDAP directories can't have lists....

	// (1) get the environment
	db->db_env = AB_Env_New();
	db->db_env->sEnv_DoTrace = 0;

	// (2) open a store given the filename in the DIR_Server. What if we don't have a filename? 
	db->db_store = AB_Env_NewStore(db->db_env, fullFileName, AB_Store_kGoodFootprintSpace);
	if (db->db_store)
	{
		AB_Store_OpenStoreContent(db->db_store, db->db_env);
		// (3) get an ab_table for the store
		db->db_table = AB_Store_GetTopStoreTable(db->db_store, db->db_env);

		if (AB_Env_Good(db->db_env) && db->db_table)
		{
			((ab_Table *) db->db_table)->AddView(ab_Env::AsThis(db->db_env), this);

			// (4) open a row which has all the columns in the address book
			db->db_row = AB_Table_MakeDefaultRow(db->db_table, db->db_env);	
		}
	}

	if (fullFileName)
		XP_FREE(fullFileName);
}

/* SelectDatabase
 *
 * Returns TRUE if the replica has been selected.
 */
XP_Bool AB_LDAPContainerInfo::SelectDatabase()
{
	XP_Bool bSwitching = FALSE;

	/* Select the database that is appropriate for the current network state.
	 */
	if (NET_IsOffline() && m_dbReplica != NULL && !m_replicating)
	{
		if (m_db != m_dbReplica)
		{
			bSwitching = TRUE;
			m_db = m_dbReplica;
			m_performingVLV = FALSE;
		}
	}
	else
	{
		if (m_db != m_dbNormal)
		{
			bSwitching = TRUE;
			m_db = m_dbNormal;
		}
	}

	/* If we have to switch databases:
	 *   - make sure any ongoing search is cancelled.
	 *   - notify all the panes that their views are obsolete.
	 */
	if (bSwitching)
	{
		AbortSearch();
		NotifyContainerAttribChange(this, AB_NotifyAll, NULL);

		/* If the selected database is not open, open it.
		 */
		if (m_db->db_env == NULL)
			InitializeDatabase(m_db);
	}

	return m_db == m_dbReplica;
}


ABID AB_LDAPContainerInfo::GetABIDForIndex(const MSG_ViewIndex index)
// what is going on here...well we are assigning our own ABIDs for this ctr because we are not storing any entries in the database.
// Best case scenarion, we would just map view indices directly to ABIDs (i.e. they would be the same). But view indices are zero based
// and 0 is a reserved ABID value for AB_ABIDUNKNOWN. So our ABID designation for an entry will always be 1 + the view index for the entry.
{
	if (m_performingVLV)
	{
		if (index != MSG_VIEWINDEXNONE)
			return (ABID) (index + 1);
		else
			return AB_ABIDUNKNOWN;
	}
	else  // otherwise it is in the database so you its implementation
		return AB_ContainerInfo::GetABIDForIndex(index);
}
 
MSG_ViewIndex AB_LDAPContainerInfo::GetIndexForABID(ABID entryID)
{
	if (m_performingVLV)
	{
		MSG_ViewIndex index = MSG_VIEWINDEXNONE;
		if (entryID != AB_ABIDUNKNOWN)
			index = (MSG_ViewIndex) (entryID - 1);
		return index;
	}
	else
		return AB_ContainerInfo::GetIndexForABID(entryID);
}

char * AB_LDAPContainerInfo::GetVCard(MSG_Pane * srcPane, ABID entryID)
{
	if (m_performingVLV)
	{
		return NULL; // we haven't written our own VCard code yet.....
	}
	else // it is in the database...the database knows how to convert the entry to a vcard....
		return AB_ContainerInfo::GetVCard(srcPane, entryID);
}


/* PreloadSearch
 *
 * This function serves two purposes, it:
 *  1) queries the server's LDAP capabilities (only the first time it is
 *     called, per container info);
 *  2) performs a search to retrieve the first block of VLV entries from the
 *     server (iff the server supports VLV)
 */
int AB_LDAPContainerInfo::PreloadSearch(MSG_Pane *pane)
{
	/* Select the database that is appropriate for the current network state.
	 * If the replica is selected, don't do anything.
	 */
	if (!SelectDatabase() && ServerSupportsVLV())
		return PerformSearch(pane, ABLST_Typedown, NULL, 0, FALSE);

	return AB_SUCCESS;
}

/* SearchDirectory
 *
 * Perform a basic or extended search.  This function performs the following
 * different types of searches:
 *  1) v2 Basic - A basic search on an LDAP server that does not support VLV.
 *                Returns a subset of entries that match a single search
 *                attribute.  May return all the matches if the set is small.
 *  2) v3 Basic - A basic search on an LDAP server that does support VLV.
 *                Initiates management a VLV for the all of the entries that
 *                match a single search attribute.
 *  3) Extended - Returns a subset of entries that match one or more pre-
 *                specified search attributes.  Never initiates management of
 *                a VLV.
 */
int AB_LDAPContainerInfo::SearchDirectory(AB_Pane *pane, char *searchString)
{
	/* Select the database that is appropriate for the current network state.
	 * If the replica is selected, use it to do the search.
	 */
	if (SelectDatabase())
		return ParentClass::TypedownSearch(pane, searchString, 0);

	/* The search string is a NULL pointer or nul string if we are to do an
	 * extended search.
	 */
	AB_LDAPSearchType searchType;
	if (searchString && (XP_STRLEN(searchString) > 0))
		searchType = (ServerSupportsVLV()) ? ABLST_Typedown : ABLST_Basic;
	else
		searchType = ABLST_Extended;

	return PerformSearch(pane, searchType, searchString, 0, TRUE);
}

/* TypedownSearch
 */
int AB_LDAPContainerInfo::TypedownSearch(AB_Pane *pane,
                                         const char *typeDownValue,
                                         MSG_ViewIndex startIndex)
{
	/* Select the database that is appropriate for the current network state.
	 * If the replica is selected, use it to do the search.
	 */
	if (SelectDatabase())
		return ParentClass::TypedownSearch(pane, typeDownValue, startIndex);

	AB_LDAPSearchType searchType;
	searchType = (ServerSupportsVLV()) ? ABLST_Typedown : ABLST_Basic;

	/* Put ourselves in batch mode!!
	 */
	if (m_db->db_store && m_db->db_env)
	{
		AB_Store_StartBatchMode(m_db->db_store, m_db->db_env, 150 /* JUST A TEMPORARY MADE UP VALUE!!! */);
		AB_Env_ForgetErrors(m_db->db_env); // if we failed we have no errors to really report....
	}

	return PerformSearch(pane, searchType, typeDownValue, startIndex, TRUE);
}

/* NameCompletionSearch
 */
int AB_LDAPContainerInfo::NameCompletionSearch(AB_Pane *pane, const char *ncValue)
{
	/* Select the database that is appropriate for the current network state.
	 * If the replica is selected, use it to do the search.
	 */
	if (SelectDatabase())
		return ParentClass::NameCompletionSearch(pane, ncValue);

	/* A NULL search string indicates that we should abort the current name
	 * completion search.
	 */
	if (ncValue == NULL && m_searchPane == pane)
		return AbortSearch();

	/* Otherwise, start a new name completion search.
	 */
	else
		return PerformSearch(pane, ABLST_NameCompletion, ncValue, 0, TRUE);
}


/* AbortSearch
 */
int AB_LDAPContainerInfo::AbortSearch()
{
	if (!m_searchPane)
		return AB_SUCCESS;

	if (m_entryMatch.m_isValid)
	{
		m_entryMatch.m_isValid = FALSE;
		AB_FreeEntryAttributeValues(m_entryMatch.m_values, m_entryMatch.m_numAttributes);
	}

	m_interruptingSearch = TRUE;
	MSG_SearchError err = MSG_InterruptSearchViaPane(m_searchPane);
	m_interruptingSearch = FALSE;

	if (err != SearchError_Busy)
	{
		if (m_db && m_db->db_store && m_db->db_env) // if we were in batch mode and we terminated a search, terminate the batch mode..
		{
			AB_Store_EndBatchMode(m_db->db_store, m_db->db_env);
			AB_Env_ForgetErrors(m_db->db_env);
		}

		/* Tell the old pane that it's not searching anymore.
		 */
		NotifyContainerAttribChange(this, AB_NotifyStopSearching, m_searchPane);
		m_searchPane = NULL;
		return AB_SUCCESS;
	}
	else
		return AB_FAILURE;
}

/* GetSearchAttributeFromEntry
 */
char *AB_LDAPContainerInfo::GetSearchAttributeFromEntry(AB_LDAPEntry *entry)
{
	AB_AttribID attrib = AB_LDAPEntry::GetPrimarySortAttribute();

	for (uint16 i = 0; i < entry->m_numAttributes; i++)
	{
		if (entry->m_values[i].attrib == attrib)
		{
			char *value = entry->m_values[i].u.string;

			if (value && XP_STRLEN(value) > 0)
				return XP_STRDUP(value);
			else
				return NULL;
		}
	}

	return NULL;
}

/* PerformSearch
 *
 * General purpose search function.
 */
int AB_LDAPContainerInfo::PerformSearch(MSG_Pane *pane,
                                        AB_LDAPSearchType type,
                                        const char *attrib,
                                        MSG_ViewIndex index,
                                        XP_Bool removeEntries)
{
	/* The target index is ignored for "new" searches.
	 */
	m_targetIndex = AB_LDAP_UNINITIALIZED_INDEX;

	/* First thing we do is see if this is one of the following "new" searches:
	 *  - A basic or extended search
	 *  - A different type of search than the previous search
	 *  - Any Attribute search
	 *  - An index search that is a page size or more away from the cache
	 *
	 * A "new" search is one that is discontiguous from the previous search,
	 * either by nature or by location.
	 */
	XP_Bool bNew = FALSE;
	if (   type == ABLST_RootDSE || type != m_searchType || attrib
	    || type == ABLST_Basic || type == ABLST_Extended || type == ABLST_NameCompletion)
	{
		bNew = TRUE;
	}
	else
	{
		AB_LDAPEntry *entry;


		/* Check for an index search one page size or more outside of cache
		 */
		if (index + m_pageSize - 1 < m_firstIndex || m_lastIndex + m_pageSize - 1 < index)
		{
			bNew = TRUE;
			m_targetIndex = index;
		}

		/* Check for an index near but before the beginning of the cache.
		 */
		else if (index < m_firstIndex)
		{
			entry = (AB_LDAPEntry *)m_entryList[0];
			if (entry->m_isValid)
			{
				if (AbortSearch() == AB_FAILURE) return AB_FAILURE;
				attrib = GetSearchAttributeFromEntry(entry);
				m_entryMatch.m_isValid = TRUE;
				m_entryMatch.m_values = entry->m_values;
				m_entryMatch.m_numAttributes = entry->m_numAttributes;
				entry->m_isValid = FALSE;
				m_targetIndex = m_firstIndex;
			}
			else
			{
				bNew = TRUE;
				m_targetIndex = index;
			}
		}

		/* Check for an index near but after the end of the cache.
		 */
		else if (index > m_lastIndex)
		{
			entry = (AB_LDAPEntry *)m_entryList[m_lastIndex - m_firstIndex];
			if (entry->m_isValid)
			{
				if (AbortSearch() == AB_FAILURE) return AB_FAILURE;
				attrib = GetSearchAttributeFromEntry(entry);
				m_entryMatch.m_isValid = TRUE;
				m_entryMatch.m_values = entry->m_values;
				m_entryMatch.m_numAttributes = entry->m_numAttributes;
				entry->m_isValid = FALSE;
				m_targetIndex = m_lastIndex;
			}
			else
			{
				bNew = TRUE;
				m_targetIndex = index;
			}
		}

		/* The request is for an index we already have; this should never
		 * happen, but if it does we just return.
		 */
		else
			return AB_SUCCESS;
	}

	/* Abort any search that may already be going on.  We don't do the
	 * abort here for non-"new" searches because we must do it above.
	 */
	if (bNew)
	{
		if (AbortSearch() == AB_FAILURE)
			return AB_FAILURE;
	}

	m_searchType = type;
	m_searchPane = (AB_Pane *)pane;

	/* Clear out the cache/DB
	 */
	if (removeEntries)
		RemoveAllEntries();
	
	if (type != ABLST_Extended)
	{
		MSG_SearchFree(pane);
		MSG_SearchAlloc(pane);
		MSG_AddLdapScope(pane, m_server);
	}

	MSG_SearchValue value;
	if (type == ABLST_Typedown)
	{
		m_performingVLV = TRUE;
		if (bNew)
			m_updateTotalEntries = TRUE;
	
		LDAPVirtualList *pLdapVLV = (LDAPVirtualList *)XP_ALLOC(sizeof(LDAPVirtualList));
		if (pLdapVLV)
		{
			/* Cap the search page size at 33. Most servers will return no
			 * more than 100 entries per search.
			 */
			m_searchPageSize = MIN(m_entryList.GetSize() / 3, 33);

			pLdapVLV->ldvlist_before_count = m_searchPageSize;
			pLdapVLV->ldvlist_after_count = m_searchPageSize * 2 - 1;
			pLdapVLV->ldvlist_size = (m_totalEntries ? m_totalEntries : 2);
			pLdapVLV->ldvlist_extradata = (void *)GetPairMatchingSortAttribute();

			if (attrib)
			{
				pLdapVLV->ldvlist_attrvalue = (char *)(bNew ? XP_STRDUP(attrib) : attrib);
			}
			else
			{
				/* Must add one to the target index because LDAPVirtualList
				 * indices are 1-based and search basis index is 0-based.
				 */
				pLdapVLV->ldvlist_index = m_targetIndex + 1;
				pLdapVLV->ldvlist_attrvalue = NULL;
			}

			/* If the we won't be able to retreive a page size worth of entries
			 * before the target entry, adjust the before and after cound values
			 * to maximize the hits.
			 */
			if (m_targetIndex < m_searchPageSize)
			{
				pLdapVLV->ldvlist_before_count = m_targetIndex;
				pLdapVLV->ldvlist_after_count = m_searchPageSize * 3 - m_targetIndex - 1;
			}

			/* TBD: For some reason this optimization really messes things up.
			 * TBD: I think it may be due to a server bug.
			 *
			 * else if (   m_totalEntries > 0
			 *          && m_targetIndex + pLdapVLV->ldvlist_after_count > m_totalEntries)
			 * {
			 *     pLdapVLV->ldvlist_after_count = m_totalEntries - m_targetIndex;
			 *     pLdapVLV->ldvlist_before_count = m_searchPageSize * 3 - pLdapVLV->ldvlist_after_count - 1;
			 * }
			 */

			/* Estimate the first and last index; the actual indices may
			 * differ depending upon how many entries the server returns.
			 */
			if (m_targetIndex == AB_LDAP_UNINITIALIZED_INDEX)
			{
				m_firstIndex = AB_LDAP_UNINITIALIZED_INDEX;
			}
			else
			{
				m_firstIndex = m_targetIndex - pLdapVLV->ldvlist_before_count;
				m_lastIndex = m_firstIndex + m_searchPageSize * 3 - 1;
			}
		}
		value.u.string = "";
		MSG_SetSearchParam(pane, searchLdapVLV, pLdapVLV);
	}
	else
	{
		/* Any search that get here is a non-VLV search and should set
		 * m_performSearch to FALSE, except name completion searches which
		 * should just leave the flag alone (since they can co-exist with
		 * a VLV search).
		 */
		if (type != ABLST_NameCompletion)
			m_performingVLV = FALSE;

		if (type != ABLST_Extended)
		{
			XP_ASSERT(attrib);
			value.u.string = (char *)(bNew ? XP_STRDUP(attrib) : attrib);
		}

		MSG_SetSearchParam(pane, type == ABLST_RootDSE ? searchRootDSE : searchNormal, NULL);
	}

	if (type != ABLST_Extended)
	{
		value.attribute = attribCommonName;
		MSG_AddSearchTerm(pane, attribCommonName, opLdapDwim, &value, TRUE, NULL);
	}

	if (MSG_Search(pane) == SearchError_Success)
	{
	    NotifyContainerAttribChange(this, AB_NotifyStartSearching, (AB_Pane *)pane); 
		return AB_SUCCESS;
	}
	else
		return AB_FAILURE;
}


/* RemoveAllEntries
 *
 * Removes all entries from the cache or the database depending upon what
 * type of search we are going to perform.
 */
void AB_LDAPContainerInfo::RemoveAllEntries()
{
	if (m_searchType == ABLST_NameCompletion)
	{
		for (uint32 i = m_ncEntryList.GetSize(); i > 0; i--)
		{
			AB_LDAPEntry *entry = (AB_LDAPEntry *)m_ncEntryList[i-1];
			AB_FreeEntryAttributeValues(entry->m_values, entry->m_numAttributes);
			XP_FREE(entry);
		}
		m_ncEntryList.RemoveAll();
	}
	else if (m_searchType == ABLST_Typedown)
	{
		for (uint32 i = m_entryList.GetSize(); i > 0; i--)
		{
			AB_LDAPEntry *entry = (AB_LDAPEntry *)m_entryList[i-1];
			if (entry->m_isValid)
				AB_FreeEntryAttributeValues(entry->m_values,
			   	                            entry->m_numAttributes);

			entry->m_isValid = FALSE;
		}
	}

	/* The case for removing entries from the DB is rather convoluted.  In
	 * general we want to free them if we are NOT doing a typedown or name
	 * completion search.  But, we also need to free them if we are doing
	 * a typedown and we don't know if the server supports VLV, since that
	 * will actually result in a basic search if it doesn't support VLV.
	 */
	if (   m_db == m_dbNormal
	    && (   (m_searchType != ABLST_Typedown && m_searchType != ABLST_NameCompletion)
	        || (m_searchType == ABLST_Typedown && !DIR_TestFlag(m_server, DIR_LDAP_ROOTDSE_PARSED))))
	{
		// if we are performing a new search and there are things left in the 
		if (GetNumEntries())  // database is not empty?	
		{
			CloseDatabase(m_dbNormal);
			AB_Env * env = AB_Env_New();
			env->sEnv_DoTrace = 0;
			// to be safe, we should kill the file...
			char * fileName = NULL;
			DIR_GetServerFileName(&fileName, m_server->fileName);
			AB_Env_DestroyStoreWithFileName(env, fileName);
			if (fileName)
				XP_FREE(fileName);
			// error detection??
			InitializeDatabase(m_dbNormal);  // re-opens the database, table, row, store, etc..
		}

		NotifyContainerEntryChange(this, AB_NotifyAll, 0, 0, NULL);  // notify all listeners that we have changed! 
	}
}

int AB_LDAPContainerInfo::GetEntryAttributes(MSG_Pane * srcPane, ABID entryID, AB_AttribID * attrib, AB_AttributeValue ** values, uint16 * numItems)
{
	MSG_ViewIndex index = GetIndexForABID(entryID);
	return GetEntryAttributesForIndex(srcPane, index, attrib, values, numItems);
}


int AB_LDAPContainerInfo::GetEntryAttributesForIndex(MSG_Pane *pane,
                                                     MSG_ViewIndex index,
                                                     AB_AttribID *attribs,
                                                     AB_AttributeValue **values,
                                                     uint16 *numItems)
{
	/* Select the database that is appropriate for the current network state.
	 * If the replica is selected, retreive the result entry from it.
 	 */
	if (SelectDatabase())
		return ParentClass::GetEntryAttributesForIndex(pane, index, attribs, values, numItems);

	/* Look in the name completion cache if this is a picker pane.
	 * Special case for offline:  if a name completion search was performed
 	 * before we went offline, we honor those results.  But, if we perform a
 	 * name completion search after we go off line, we use the replica.
  	 */
 	if (pane->GetPaneType() == AB_PICKERPANE && m_ncEntryList.GetSize() > 0)
	{
		if (index >= (MSG_ViewIndex)(m_ncEntryList.GetSize()))
			return AB_FAILURE;

		return CopyEntryValues(pane, index, (AB_LDAPEntry *)m_ncEntryList[index], attribs, values, numItems);
	}

	/* Look in the cache if we are doing a VLV search and the current pane is
	 * not a picker (i.e. name completion) pane.
	 */
	else if (m_performingVLV)
	{
		/* Check for the case where we have started a search but we don't
		 * know which indices will be returned.
		 */
		if (m_firstIndex == AB_LDAP_UNINITIALIZED_INDEX)
			return AB_ENTRY_BEING_FETCHED;
 
		/* Check for an invalid index.
		 */
		else if (index == MSG_VIEWINDEXNONE || index >= m_totalEntries)
			return AB_FAILURE;

		AB_LDAPEntry *entry = NULL;
		if (m_firstIndex <= index && index <= m_lastIndex)
			entry = (AB_LDAPEntry *)m_entryList[index - m_firstIndex];

		/* Return the entry if we already have it.
	 	 */
		if (entry && entry->m_isValid)
			return CopyEntryValues(pane, index, entry, attribs, values, numItems);

		/* We don't have the entry yet, but we have already started a search
		 * for it.
		 */
		else if (entry)
			return AB_ENTRY_BEING_FETCHED;

		/* We don't have the entry and we aren't in the process of retreiving it
		 * yet; start a search to get it.
		 */
		else if (PerformSearch(pane, ABLST_Typedown, NULL, index) == AB_SUCCESS)
			return AB_ENTRY_BEING_FETCHED;

		/* We don't have the entry and the search we tried to start failed.
		 */
		else
			return AB_FAILURE;
	}

	/* A normal search result request, look in the database.
	 */
	else
	{
		return ParentClass::GetEntryAttributesForIndex(pane, index, attribs, values, numItems);
	}
}

/* GetNumEntries
 */
uint32 AB_LDAPContainerInfo::GetNumEntries()
{
	/* Select the database that is appropriate for the current network state.
	 */
	SelectDatabase();

	if (m_performingVLV)
		return m_totalEntries;
	else
		return AB_DirectoryContainerInfo::GetNumEntries();
}


/* GetPairMatchingSortAttribute
 */
const char *AB_LDAPContainerInfo::GetPairMatchingSortAttribute()
{
	/* Use the standard filter if there is no VLV search pair list for the server.
	 */
	if (!m_server->searchPairList || !m_server->searchPairList[0])
		return NULL;

	/* Use the standard filter if the sort attribute is an unsupported sort
	 * attribute for LDAP; this should probably never happen.
	 */
	const char *attribName = GetSortAttributeNameFromID(m_sortedBy);
	if (!attribName)
		return NULL;

	/* Try to find a search pair that matches the sort attribute.
	 */
	char *pair = m_server->searchPairList;
	char *end = XP_STRCHR(pair, ';');
	while (pair && *pair)
	{
		if (PairMatchesAttribute(pair, end, attribName, m_sortAscending))
		{
			int len = (end ? end - pair : XP_STRLEN(pair));
			char *match = (char *)XP_ALLOC(len + 1);
			if (match)
				return XP_STRNCPY_SAFE(match, pair, len + 1);
		}

		if (end)
			pair = end+1;
		else
			pair = NULL;

		end = XP_STRCHR(pair, ';');
	}

	/* As a special case, we always allow sorting on the full name or the
	 * surname even if there was no matching search pair.
	 */
	if (m_sortedBy == AB_attribDisplayName)
	{
		/* TBD: add cases for lastname, firstname display/sorting.
		 */

		return PR_smprintf("(%s=*):%s%s", attribName, m_sortAscending ? "" : "-", attribName);
	}

	return NULL;
}

/* GetSortAttributeNameFromID
 *
 * Returns a string with the LDAP attribute name corresponding to the
 * address book ID passed in.  The string returned from this function
 * does not need to be freed.  Returns NULL if the attribute is not
 * a supported sortable attribute.
 */
const char *AB_LDAPContainerInfo::GetSortAttributeNameFromID(AB_AttribID attrib)
{
	/* Get the LDAP attribute name that corresponds to the AB attribute ID
	 * passed in.
	 */
	switch (attrib) {
	case AB_attribDisplayName:
	case AB_attribFullName:
		return DIR_GetFirstAttributeString(m_server, cn);
	case AB_attribEmailAddress:
		return DIR_GetFirstAttributeString(m_server, mail);
	case AB_attribCompanyName:
		return DIR_GetFirstAttributeString(m_server, o);
	case AB_attribWorkPhone:
		return DIR_GetFirstAttributeString(m_server, telephonenumber);
	case AB_attribLocality:
		return DIR_GetFirstAttributeString(m_server, l);
	case AB_attribNickName:
	default:
		/* We don't even request this value in LDAP searches.
		 */
		return NULL;
	}

	/* If the attribute does not correspond to one of the above, then it
	 * isn't sortable.
	 */
	return NULL;
}

/* IsEntryAttributeSortable
 *
 * Returns true if the given attribute can be used to sort the results.  For an
 * LDAP server that supports VLV, in general, that means the server must have a
 * VLV set up for that attribute.
 *
 * Note: for the return code to be valid for a VLV searches, the client must be
 *       performing a VLV search at the time this function is called.
 */
XP_Bool AB_LDAPContainerInfo::IsEntryAttributeSortable(AB_AttribID attrib)
{
	if (m_performingVLV)
	{
		/* We always assume cn is sortable, even if it is not.  This allows us
		 * to display a VLV on small servers that support VLV but don't have
		 * any VLV entries set up.
		 */
		if (attrib == AB_attribDisplayName)
			return TRUE;

		/* If there is no VLV search list or the attribute is not sortable,
		 * return FALSE.
		 */
		const char *attribName = GetSortAttributeNameFromID(attrib);
		if (!m_server->searchPairList || !m_server->searchPairList[0] || !attribName)
			return FALSE;

		/* Try to find ascending and descending search pairs that match the sort
		 * attribute.
		 */
		XP_Bool ascending = FALSE, descending = FALSE;
		char *pair = m_server->searchPairList;
		char *end = XP_STRCHR(pair, ';');
		while (pair && *pair && (!ascending || !descending))
		{
			if (!ascending && PairMatchesAttribute(pair, end, attribName, TRUE))
				ascending = TRUE;
			else if (!descending && PairMatchesAttribute(pair, end, attribName, FALSE))
				descending = TRUE;
	
			if (end)
				pair = end+1;
			else
				pair = NULL;

			end = XP_STRCHR(pair, ';');
		}

		return ascending && descending;
	}
	else
		return ParentClass::IsEntryAttributeSortable(attrib);
}

/* MatchPairToAttribute
 */
XP_Bool AB_LDAPContainerInfo::PairMatchesAttribute(const char *pair, const char *end, const char *attrib, XP_Bool ascending)
{
	char *p_s = XP_STRCHR(pair, ':');
	if (!p_s || p_s == pair || (++p_s >= end && end))
		return FALSE;
	
	if ((*p_s != '-') != ascending)
		return FALSE;
	else if (*p_s == '-')
		++p_s;

	char *p_e;
	for (p_e = p_s; *p_e && isalnum((int)*p_e); p_e++) ;
	if (p_e > end)
		return FALSE;

	return XP_STRNCASECMP(p_s, attrib, p_e - p_s) == 0;
}


/* The ValidLDAPAttribtesTable defines the order in which the attributes for
 * each entry are stored in memory.  As an optimization certain internal
 * functions may be coded to assume that particular attributes are at
 * particular indices.  If the order of attributes in this table is changed
 * these functions must be updated.  Keep in mind that this table is 1-based
 * with respect to the m_values element indices; the 0 index attribute is
 * always AB_attribEntryType with a numeric value of AB_Person.
 *
 * In general, for effeciency, this table should be sorted in order of the
 * default AB_LDAPEntry sort order.
 *
 * The functions that are implicitly dependant upon the order of attributes
 * in this table are:
 *   AB_LDAPEntry::Compare           - does not consider index 0
 *   AB_LDAPContainerInfo::GetLdapDN - retrieves distinguished name
 */
MSG_SearchAttribute ValidLDAPAttributesTable[] =
{
	attribCommonName,
    attribGivenName,
    attribSurname,
	attrib822Address,
	attribOrganization,
	attribLocality,
	attribPhoneNumber,
	attribDistinguishedName
};

const uint16 AB_NumValidLDAPAttributes = 8;

/* ConvertLDAPToABAttribID
 *
 * We really should eventually phase this routine out and have
 * MSG_SearchAttributes and AB_AttribIDs be the same thing!!!
 */
AB_AttribID AB_LDAPContainerInfo::ConvertLDAPToABAttribID (MSG_SearchAttribute attrib)
{
	switch(attrib)
	{
	case attribGivenName:
		return AB_attribGivenName;
	case attribSurname:
		return AB_attribFamilyName;
	case attrib822Address:
		return AB_attribEmailAddress;
	case attribOrganization:
		return AB_attribCompanyName;
	case attribLocality:
		return AB_attribLocality;
	case attribPhoneNumber:
		return AB_attribWorkPhone;
	case attribDistinguishedName:
		return AB_attribDistName;
	case attribCommonName:
		return AB_attribDisplayName;    /* was fullname */
	default:
		XP_ASSERT(0); // we were given an attribute we don't know how to convert
		return AB_attribGivenName;  // why not....
	}
}

/* ConvertResultElement
 *
 * We need to be able to take a result element and a list of LDAP attributes
 * and produce a list of address book attribute values which could be added as
 * entries to the container. Caller must free the array of attributes by
 * calling AB_FreeAttributeValues.
 */
int AB_LDAPContainerInfo::ConvertResultElement(MSG_ResultElement * elem, MSG_SearchAttribute * attribs, AB_AttributeValue ** retValues, uint16 * numItems)
{
	MSG_SearchValue * result;
	if (!numItems || *numItems == 0)
		return AB_FAILURE;

	AB_AttributeValue * values = (AB_AttributeValue *) XP_ALLOC(sizeof(AB_AttributeValue) * (*numItems+1));  // + 1 to account for the type (person)
	uint16 valuesIndex = 0;   
	
	if (values)
	{
		// LDAP Entries are alwyas person entries! So we are going to manually enter their type into the values list
		values[0].attrib = AB_attribEntryType;
		values[0].u.entryType = AB_Person;

		uint16 i = 0;
		valuesIndex++;  // already added one type
		for (i = 0; i < *numItems; i++,valuesIndex++)
		{
			values[valuesIndex].attrib = ConvertLDAPToABAttribID(attribs[i]);
			values[valuesIndex].u.string = NULL;
			if (MSG_GetResultAttribute(elem, attribs[i], &result) == SearchError_Success)
			{
				if (MSG_IsStringAttribute(attribs[i]))
				{
					if (result->u.string && XP_STRLEN(result->u.string) > 0)
						values[valuesIndex].u.string = XP_STRDUP(result->u.string);
					else
						values[valuesIndex].u.string = NULL;
				}
				else  // add your customized non string attributes here to check for them
					XP_ASSERT(0);  // right now, we have none. All LDAP attribs should be strings.
				MSG_DestroySearchValue(result);
			}
		}

		// Now we need to perform the common name check. If first or last names are empty, use the common name as the first name
		// POTENTIAL HACK ALERT: FOR RIGHT NOW, I am just going to use the common name if I have no first name...
		for (i = 0; i < *numItems; i++)
			if (values[i].attrib == AB_attribGivenName && values[i].u.string && XP_STRLEN(values[i].u.string) == 0)
			{
				if (MSG_GetResultAttribute(elem, attribCommonName, &result) == SearchError_Success)
					if (result->u.string)
						values[i].u.string = XP_STRDUP(result->u.string);
					else
						values[i].u.string = NULL;
				break;
			}

		*numItems = valuesIndex;
		*retValues = values;
		return AB_SUCCESS;
	}
	
	return AB_OUT_OF_MEMORY;
}

int AB_LDAPContainerInfo::CopyEntryValues(MSG_Pane * srcPane, MSG_ViewIndex index, AB_LDAPEntry *entry, AB_AttribID *attribs, AB_AttributeValue **values, uint16 *numItems)
{
	AB_AttributeValue *newValues = (AB_AttributeValue *)XP_ALLOC(sizeof(AB_AttributeValue) * (*numItems));

 	if (newValues)
	{
		int newValueMarker = 0;
		XP_Bool found = FALSE;

		// for each attribute we want...
		for (uint16 j = 0; j < *numItems; j++)
		{
			int loopStatus = AB_FAILURE;
			// by asking if it is a dbase attrib, we are also asking, is it an attribute that would be in the LDAP
			// entry attribute array? Attributes that would not be in this list are attributes that need to be computed
			// like full address or vcard....			
			if (IsDatabaseAttribute(attribs[j]))
			{
				found = FALSE;
				// ...scan through list of known attributes in attempt to find attrib[j]
				for (uint16 index = 0; index < entry->m_numAttributes; index++)
				{
					if (attribs[j] == entry->m_values[index].attrib)
					{
						loopStatus = AssignEntryValues(&(entry->m_values[index]), &newValues[newValueMarker]);
						found = TRUE;
						break;
					}
				}
					
				if (!found)
					loopStatus = AssignEmptyEntryValue(attribs[j], &newValues[newValueMarker]);
			}  // if database attribute
			else
				loopStatus = GetNonDBEntryAttributeForIndex(srcPane, index, attribs[j], &newValues[newValueMarker]);

			if (loopStatus == AB_SUCCESS) // if any of them succeeded,
				newValueMarker++;

		}

		*numItems = newValueMarker;
		*values = newValues;
		return AB_SUCCESS;
	}

	return AB_FAILURE;
}

/* ServerSupportsVLV
 *
 * Returns TRUE if the server supports the virtual list view control or if we
 * don't know if it does.  In the later case, we can always fall-back to a
 * limited implementation of whatever type of search we are going to perform.
 */
XP_Bool AB_LDAPContainerInfo::ServerSupportsVLV()
{
	return    !DIR_TestFlag(m_server, DIR_LDAP_ROOTDSE_PARSED)
	       || DIR_TestFlag(m_server, DIR_LDAP_VIRTUALLISTVIEW);
}

/* UpdatePageSize
 *
 * - Hash table is an array of AB_LDAPEntry pointers
 * - m_lastHashEntry points to the last hash entry retrieved from the server
 * - Hash table size is three times the largest page size.
 */
void AB_LDAPContainerInfo::UpdatePageSize(uint32 pageSize)
{
	AB_DirectoryContainerInfo::UpdatePageSize(pageSize);

	if (ServerSupportsVLV())
	{
		uint32 oldListSize = (uint32)m_entryList.GetSize();
		uint32 newListSize = m_pageSize * 3;

		/* If the size of the entry list we need to accomodate the new page size is
		 * greater than the old entry list size, we need to resize the entry list.
		 */
		if (newListSize > oldListSize)
		{
			m_entryList.SetSize(newListSize);

			AB_LDAPEntry *entry;
			for (;oldListSize < newListSize; oldListSize++)
			{
				entry = (AB_LDAPEntry *)XP_ALLOC(sizeof(AB_LDAPEntry));
				if (entry)
				{
					entry->m_isValid = FALSE;
					m_entryList.SetAt(oldListSize, entry);
				}
				else
				{
					/* Ack! We ran out of memory.  Do the best we can with what
					 * we have.
					 */
					m_entryList.SetSize(oldListSize - 1);
				}
			}
		}
	}
}


/* UseExtendedSelection
 */
XP_Bool AB_LDAPContainerInfo::UseExtendedSelection(MSG_Pane *pane)
{
	return m_performingVLV && pane->GetPaneType() != AB_PICKERPANE;
}

/* CopySelectionEntry
 */
void *AB_LDAPContainerInfo::CopySelectionEntry(void *entry)
{
	return XP_STRDUP((char *)entry);
}

/* DeleteSelectionEntry
 */
void AB_LDAPContainerInfo::DeleteSelectionEntry(void *entry)
{
	XP_FREEIF(entry);
}

/* GetSelectionEntryForIndex
 *
 * Returns the distinguished name for the entry at the given index.  Returns
 * NULL if the index is not for an entry in cache or if we are not currently
 * performing a VLV search.
 */
void *AB_LDAPContainerInfo::GetSelectionEntryForIndex(MSG_ViewIndex index)
{
	if (m_performingVLV)
	{
		AB_LDAPEntry *entry = NULL;
		if (m_firstIndex <= index && index <= m_lastIndex)
			entry = (AB_LDAPEntry *)m_entryList[index - m_firstIndex];

		/* The eighth value is always the distinguished name.
		 */
		if (entry && entry->m_isValid)
			return entry->m_values[8].u.string;
	}

	return NULL;
}

/* CompareSelections
 */
int AB_LDAPContainerInfo::CompareSelections(const void *s1, const void *s2)
{
	return XP_STRCASECMP(*((const char **)s1), *((const char **)s2));
}


/* LDAPSearchResults
 *
 * Called by the front end for each result entry returned from a search.
 */
int AB_LDAPContainerInfo::LDAPSearchResults(MSG_Pane * pane, MSG_ViewIndex index, int32 num)
{
	/* VLV and name completion entry results get added when the search is
	 * complete.
	 */
	if (m_searchType == ABLST_Typedown && ServerSupportsVLV())
		return AB_SUCCESS;

	/* We have a name completion search result, to the NC cache.
	 */
	else if (m_searchType == ABLST_NameCompletion)
	{
		MSG_ResultElement *elem;

		for (MSG_ViewIndex i = index; i < num + index; i++) 
		{
			if (MSG_GetResultElement (pane, index, &elem) == SearchError_Success) 
			{
				AB_LDAPEntry *entry = (AB_LDAPEntry *)XP_ALLOC(sizeof(AB_LDAPEntry));
				if (entry)
				{
					entry->m_numAttributes = AB_NumValidLDAPAttributes;

					if (ConvertResultElement(elem, ValidLDAPAttributesTable, &entry->m_values,
					                         &entry->m_numAttributes) == AB_SUCCESS) 
					{
						int position = m_ncEntryList.Add(entry); // record position to act as ABID for call back
						if (pane->GetPaneType() == AB_PICKERPANE && position >= 0)
							((AB_PickerPane *) pane)->ProcessSearchResult(this, (ABID) position); // call back into NC picker with result
					}
					else
						XP_FREE(entry);
				}
			}
		}
	}

	/* We've got a non-VLV, non-namecompletion result, store it in the DB.
	 */
	else
	{
		uint16 numAttribs = AB_NumValidLDAPAttributes;
		AB_AttributeValue *newValues;  // ptr to array of attribs we want to add to represent the new entry
		MSG_ResultElement *elem;

		for (MSG_ViewIndex i = index; i < num + index; i++) 
		{
			if (MSG_GetResultElement (pane, index, &elem) == SearchError_Success) 
			{
				if (ConvertResultElement(elem, ValidLDAPAttributesTable, &newValues, &numAttribs) == AB_SUCCESS) 
				{
					ABID entryID;
					AddEntry(newValues, numAttribs, &entryID);
					AB_FreeEntryAttributeValues(newValues, numAttribs);
				}
			}
		}
	}

	return AB_SUCCESS;
}

/* FinishSearch
 */
int AB_LDAPContainerInfo::FinishSearch(AB_Pane *pane)
{
	XP_Bool bTotalContentChanged = TRUE;

	if (m_interruptingSearch)
		return AB_SUCCESS;

	/* For LDAP VLV searches we add the entries here and update a variety of
	 * member variables.
	 */
	m_searchPane = NULL;
	if (MSG_GetSearchType(pane) == searchLdapVLV)
	{
		LDAPVirtualList *pLdapVLV;
		pLdapVLV = (LDAPVirtualList *)MSG_GetSearchParam(pane);

		if (pLdapVLV)
		{
			uint32 num = MSG_GetNumResults(pane);
			if (num > 0)
			{
				XP_Bool bFoundMatch = FALSE;
				MSG_ResultElement *elem;
				AB_LDAPEntry *entry;

				if (m_entryMatch.m_isValid)
					m_firstIndex = m_targetIndex;

				for (MSG_ViewIndex i = 0; i < num; i++) 
				{
					entry = (AB_LDAPEntry *)m_entryList[i];
					entry->m_numAttributes = AB_NumValidLDAPAttributes;
					if (MSG_GetResultElement (pane, i, &elem) == SearchError_Success) 
					{
						if (ConvertResultElement(elem, ValidLDAPAttributesTable, &entry->m_values,
							                     &entry->m_numAttributes) == AB_SUCCESS) 
							entry->m_isValid = TRUE;
					}

					if (!bFoundMatch && m_entryMatch.m_isValid)
					{
						if (m_entryMatch != *entry)
							m_firstIndex--;
						else
							bFoundMatch = TRUE;
					}
				}

				/* Update the content count, if necessary.  Do this before
				 * sending any notifications, since that can a new search
				 * which in turn can cause the LDAPVirtualList object to be
				 * destroyed.
				 */
				if (m_updateTotalEntries)
					m_totalEntries = pLdapVLV->ldvlist_size;
				else
					bTotalContentChanged = FALSE;

				/* We need to recompute the first index for any "new" search and
				 * for the special case where our match entry was deleted from
				 * server between the time we retrieved it and now.
				 */
				if (!bFoundMatch)
				{
					/* Must subtract one VLV index index because LDAPVirtualList indices
					 * are 1-based and m_firstIndex is 0-based.
					 */
					if (pLdapVLV->ldvlist_index > m_searchPageSize)
						m_firstIndex = pLdapVLV->ldvlist_index - m_searchPageSize - 1;
					else
						m_firstIndex = 0;
				}
				m_lastIndex = m_firstIndex + num - 1;

				/* When we're doing a new search, we need to notify the FE of
				 * the new top index.
				 */
				if (!bFoundMatch)
					NotifyContainerEntryChange(this, AB_NotifyNewTopIndex, pLdapVLV->ldvlist_index - 1, GetABIDForIndex(pLdapVLV->ldvlist_index - 1), pane);

				/* Notify the FE of the new entries.
				 */
				m_performingVLV = TRUE;
				NotifyContainerEntryRangeChange(this, AB_NotifyPropertyChanged, m_firstIndex, m_lastIndex - m_firstIndex + 1, pane);
			}
			else
			{
				m_performingVLV = FALSE;
				m_searchType = ABLST_None;
				bTotalContentChanged = TRUE;
				m_totalEntries = 0;
				m_firstIndex = 0;
				m_lastIndex = 0;
			}
		}
		else
		{
			m_performingVLV = FALSE;
		}
	}
	else
	{
		m_performingVLV = FALSE;
	}

	/* Free the match entry if it was used for this search.  We do it here
	 * instead of in the VLV case (where it is used) because certain errors
	 * in the search adapter may change the search type and we may no longer
	 * be in a VLV search.
	 */
	if (m_entryMatch.m_isValid)
	{
		m_entryMatch.m_isValid = FALSE;
		AB_FreeEntryAttributeValues(m_entryMatch.m_values, m_entryMatch.m_numAttributes);
	}

	if (bTotalContentChanged)
		NotifyContainerEntryRangeChange(this, AB_NotifyLDAPTotalContentChanged, 0, GetNumEntries(), pane);

	if (m_db && m_db->db_store && m_db->db_env) // if we were in batch mode and we finished a search, end the batch mode...
	{
		AB_Store_EndBatchMode(m_db->db_store, m_db->db_env);
		AB_Env_ForgetErrors(m_db->db_env);
	}


	NotifyContainerAttribChange(this, AB_NotifyStopSearching, pane);
	return AB_SUCCESS;
}


/*
 * Replication Functions
 */

/* StartReplication
 */
AB_LDAPContainerInfo *AB_LDAPContainerInfo::BeginReplication(MWContext *context, DIR_Server *server)
{
	AB_ContainerInfo *ctr;
	if (Create(context, server, &ctr) != AB_SUCCESS)
		return NULL;

	/* It is an error to call this function with a non-LDAP server or when
	 * we're already replicating.
	 */
	AB_LDAPContainerInfo *lctr = ctr->GetLDAPContainerInfo();
	if (!lctr || lctr->m_replicating)
	{
		ctr->Release();
		return NULL;
	}

	/* Allocate the replication database.
	 */
	if (!lctr->m_dbReplica)
	{
		lctr->m_dbReplica = (AB_Database *)XP_CALLOC(1, sizeof(AB_Database));
		if (!lctr->m_dbReplica)
		{
			lctr->Release();
			return NULL;
		}
	}

	/* Allocate the replica attribute map structure.
	 */
	lctr->m_attribMap = lctr->CreateReplicaAttributeMap();
	if (!lctr->m_attribMap)
	{
		lctr->Release();
		return NULL;
	}

	/* Open the replication database.
	 */
	if (!lctr->m_dbReplica->db_env) 
		lctr->InitializeDatabase(lctr->m_dbReplica);

	lctr->m_replicating = TRUE;
	return lctr;
}

/* EndReplication
 */
void AB_LDAPContainerInfo::EndReplication()
{
	if (!m_replicating)
		return;
	
	if (m_attribMap)
		FreeReplicaAttributeMap(m_attribMap);

	m_replicating = FALSE;
	Release();
}

/* AddEntry
 */
XP_Bool AB_LDAPContainerInfo::AddReplicaEntry(char **valueList)
{
	XP_Bool bSuccess = FALSE;

	if (m_replicating)
	{
		// clear the row out...
		AB_Row_ClearAllCells(m_dbReplica->db_row, m_dbReplica->db_env);

		// now write the attributes for our new value into the row....
		AB_Row_WriteCell(m_dbReplica->db_row, m_dbReplica->db_env, "t", AB_Attrib_AsStdColUid(AB_Attrib_kIsPerson));
		for (uint16 i = 0; i < m_attribMap->numAttribs; i++)
			AB_Row_WriteCell(m_dbReplica->db_row, m_dbReplica->db_env, valueList[i], AB_Attrib_AsStdColUid(m_attribMap->attribIDs[i]));

		// once all the cells have been written, write the row to the table
		ab_row_uid RowUid = AB_Row_NewTableRowAt(m_dbReplica->db_row, m_dbReplica->db_env, 0);
		AB_Env_ForgetErrors(m_dbReplica->db_env);

		// did it succeed?
		if (RowUid > 0)
		{
			bSuccess = TRUE;

			if (m_db == m_dbReplica)
				NotifyContainerEntryChange(this, AB_NotifyInserted, GetIndexForABID((ABID) RowUid), (ABID) RowUid, NULL);
		}

		AB_Env_ForgetErrors(m_dbReplica->db_env);
	}

	return bSuccess;
}

/* DeleteEntry
 */
void AB_LDAPContainerInfo::DeleteReplicaEntry(char *targetDn)
{
	if (m_replicating)
	{
		ab_row_uid rowUID = AB_Table_FindFirstRowWithPrefix(m_dbReplica->db_table, m_dbReplica->db_env,
		                                                    targetDn, AB_Attrib_kDistName);

	    if (AB_Env_Good(m_dbReplica->db_env) && rowUID)
		{
			AB_Table_CutRow(m_dbReplica->db_table, m_dbReplica->db_env, rowUID);
			AB_Env_ForgetErrors(m_dbReplica->db_env);
		}
	}
}

/* RemoveReplicaEntries
 */
void AB_LDAPContainerInfo::RemoveReplicaEntries()
{
	if (m_replicating && AB_Table_CountRows(m_dbReplica->db_table, m_dbReplica->db_env))
	{
		CloseDatabase(m_dbReplica);

		char *fullFileName;
		DIR_GetServerFileName(&fullFileName, m_server->replInfo->fileName);
		if (fullFileName)
		{
			/* To be safe, we should kill the file...
			 */
			AB_Env *env = AB_Env_New();
			AB_Env_DestroyStoreWithFileName(env, fullFileName);

			XP_FREE(fullFileName);
		}

		InitializeDatabase(m_dbReplica);
	}
}


/* These arrays provide a way to efficiently set up the replica attribute
 * structure, which itself provides an effecient mapping between ldap attribute
 * names and address book column IDs.
 *
 * Oh how nice it would be if attribute IDs were compatible amongst all the
 * different objects...
 */
static DIR_AttributeId _dirList[8] = 
    { cn,                    givenname,
      sn,                    mail,
      telephonenumber,       o,
      l,                     street
    };
static char *_strList[5] =
    { "title",               "postaladdress",
      "postalcode",          "facsimiletelephonenumber"
    };
static AB_Column_eAttribute _abList[13] =
    { AB_Attrib_kFullName,   AB_Attrib_kGivenName,
      AB_Attrib_kFamilyName, AB_Attrib_kEmail,
      AB_Attrib_kWorkPhone,  AB_Attrib_kCompanyName,
      AB_Attrib_kLocality,   AB_Attrib_kStreetAddress,

      /* The following IDs have no equivalent      * 
       * preference but get replicated.  The LDAP  * 
       * equivalent attribute names come from      *
       * _strList.                                 */
      AB_Attrib_kTitle,      AB_Attrib_kPostalAddress,
      AB_Attrib_kZip,        AB_Attrib_kFax
    };

/* CreateAttributeMap
 */
AB_ReplicaAttributeMap *AB_LDAPContainerInfo::CreateReplicaAttributeMap()
{
	int i, j, cAttribs, cPrefAttribs;
	AB_ReplicaAttributeMap *attribMap;

	/* Allocate space for the attribute list struct and the two lists within
	 * it.  The attribNames list is NULL terminated so that it may be passed
	 * as is to ldap_search.  The maximum number of attributes is the number of
	 * IDs in _abList.
	 */
	cAttribs = sizeof(_abList) / sizeof(AB_Column_eAttribute);
	attribMap = (AB_ReplicaAttributeMap *)XP_CALLOC(1, sizeof(AB_ReplicaAttributeMap));
	if (attribMap)
	{
		attribMap->numAttribs = cAttribs;
		attribMap->attribIDs = (AB_Column_eAttribute *)XP_ALLOC(cAttribs * sizeof(AB_Column_eAttribute));
		attribMap->attribNames = (char **)XP_ALLOC((cAttribs + 1) * sizeof(char *));
	}
	if (!attribMap || !attribMap->attribIDs || !attribMap->attribNames)
		return NULL;

	/* Initialize the attribute list mapping structure with the default values,
	 * and any preference adjustments.
	 */
	cPrefAttribs = sizeof(_dirList) / sizeof(DIR_AttributeId);
	for (cAttribs = 0; cAttribs < cPrefAttribs; cAttribs++)
	{
		attribMap->attribNames[cAttribs] = (char *)DIR_GetFirstAttributeString(m_server, _dirList[cAttribs]);
		attribMap->attribIDs[cAttribs] = _abList[cAttribs];
	}
	for (i = 0; cAttribs < attribMap->numAttribs; i++, cAttribs++)
	{
		attribMap->attribNames[cAttribs] = _strList[i];
		attribMap->attribIDs[cAttribs] = _abList[cAttribs];
	}

	/* Remove any attributes in the exclude list from our replication list.
	 */
	if (m_server->replInfo)
	{
		for (i = 0; i < m_server->replInfo->excludedAttributesCount; i++)
		{
			for (j = 0; j < cAttribs; )
			{
				/* If the current attribute is to be excluded, move the last entry
				 * to the current position and decrement the attribute count.
				 */
				if (!XP_STRCASECMP(attribMap->attribNames[j], m_server->replInfo->excludedAttributes[i]))
				{
					cAttribs--;
					attribMap->attribNames[j] = attribMap->attribNames[cAttribs];
					attribMap->attribIDs[j] = attribMap->attribIDs[cAttribs];
				}
				else
					j++;
			}
		}
	}

	/* Make sure the attribute names list is NULL terminated.
	 */
	attribMap->attribNames[cAttribs] = NULL;

	return attribMap;
}

/* FreeReplicaAttributeMap
 */
void AB_LDAPContainerInfo::FreeReplicaAttributeMap(AB_ReplicaAttributeMap *attribMap)
{
	if (attribMap)
	{
		XP_FREEIF(attribMap->attribIDs);
		XP_FREEIF(attribMap->attribNames);
		XP_FREE(attribMap);
	}
}

/* AttributeMatchesId
 */
XP_Bool AB_LDAPContainerInfo::ReplicaAttributeMatchesId(AB_ReplicaAttributeMap *attribMap, int attribIndex, DIR_AttributeId id)
{
	AB_Column_eAttribute columnId = attribMap->attribIDs[attribIndex];

	switch (columnId) {
    case AB_Attrib_kFullName:
		return id == cn;
	case AB_Attrib_kGivenName:
		return id == givenname;
    case AB_Attrib_kFamilyName:
		return id == sn;
	case AB_Attrib_kEmail:
		return id == mail;
    case AB_Attrib_kWorkPhone:
		return id == telephonenumber;
	case AB_Attrib_kCompanyName:
		return id == o;
    case AB_Attrib_kLocality:
		return id == l;
	case AB_Attrib_kStreetAddress:
		return id == street;

    case AB_Attrib_kTitle:
	case AB_Attrib_kPostalAddress:
    case AB_Attrib_kZip:
	case AB_Attrib_kFax:
	default:
		break;
	}

	return FALSE;
}

AB_LDAPResultsContainerInfo * AB_LDAPContainerInfo::AcquireLDAPResultsContainer(MWContext * context /* context to use for the search */)
{
	AB_LDAPResultsContainerInfo * ctr = NULL;
	if (m_db->db_env)
	{
		ab_Env * ev = ab_Env::AsThis(m_db->db_env);
		ctr = new (*ev) AB_LDAPResultsContainerInfo(context, this);
		m_db->db_env = ev->AsSelf();
		if (AB_Env_Bad(m_db->db_env) || !ctr)  // error during creation,
		{
			if (ctr)  // object is now useless...
			{
				ctr->GoAway();
				ctr = NULL;
			}
		}
		
		AB_Env_ForgetErrors(m_db->db_env);
	}

	return ctr;
}

char * AB_LDAPContainerInfo::BuildLDAPUrl(const char *prefix, const char * distName)
{
	// I pulled this pretty much as is from the old address book code...
	// Generate the URL, using the portnumber only if it isn't the standard one.
	// The 'prefix' is different depending on whether the URL is supposed to open
	// the entry or add it to the Address Book.

	char *url = NULL;

	if (m_server)
	{
		if (m_server->isSecure) 
		{
			if (m_server->port == LDAPS_PORT)
				url = PR_smprintf ("%ss://%s/%s", prefix, m_server->serverName, distName);
			else
				url = PR_smprintf ("%ss://%s:%d/%s", prefix, m_server->serverName, m_server->port, distName);	
		}
		else 
		{
			if (m_server->port == LDAP_PORT)
				url = PR_smprintf ("%s://%s/%s", prefix, m_server->serverName, distName);
			else
				url = PR_smprintf ("%s://%s:%d/%s", prefix, m_server->serverName, m_server->port, distName);
		}
	}

	return url;
}


/***********************************************************************************************************************************

  Definitions for LDAP Result Container Info 

 **********************************************************************************************************************************/

AB_LDAPResultsContainerInfo::AB_LDAPResultsContainerInfo(MWContext * context, AB_LDAPContainerInfo * LDAPCtr)
                            :AB_LDAPContainerInfo(context,LDAPCtr->GetDIRServer(), FALSE)
{
	m_IsCached = FALSE;  // we are not going to be found in the cache!!
	InitializeDatabase(m_db);
}

AB_LDAPResultsContainerInfo::~AB_LDAPResultsContainerInfo()
{
	XP_ASSERT(m_entryList.GetSize() == 0);
}

int AB_LDAPResultsContainerInfo::Close()
{
	RemoveAllUsers();
	return AB_LDAPContainerInfo::Close(); // propgate the close back up
}

void AB_LDAPResultsContainerInfo::RemoveAllUsers()
{
	// free each entry in the results list...
	for (int32 i = m_entryList.GetSize(); i > 0; i--)
	{
		AB_LDAPEntry *entry = (AB_LDAPEntry *) m_entryList[i-1];
		AB_FreeEntryAttributeValues(entry->m_values, entry->m_numAttributes);
		XP_FREE(entry);
	}

	m_entryList.RemoveAll();
}

uint32 AB_LDAPResultsContainerInfo::GetNumEntries()
{
	return m_entryList.GetSize();
}

AB_LDAPEntry * AB_LDAPResultsContainerInfo::GetEntryForIndex(const MSG_ViewIndex index)
{
	if (m_entryList.IsValidIndex(index))
		return m_entryList.GetAt(index);
	else
		return NULL;  // no entry for index
}
 
XP_Bool AB_LDAPResultsContainerInfo::IsInContainer(ABID entryID)
{
	return IsIndexInContainer( GetIndexForABID(entryID) );
}

XP_Bool AB_LDAPResultsContainerInfo::IsIndexInContainer(MSG_ViewIndex index)
{
	return m_entryList.IsValidIndex(index);
}

// entry operations......
int AB_LDAPResultsContainerInfo::CopyEntriesTo(AB_ContainerInfo * destContainer, ABID * idArray, int32 numItems, XP_Bool /*deleteAterCopy*/)
{
	// for each entry, get its LDAP entry & turn it into an array of attribuutes....call AddUser on the dest field
	if (destContainer && destContainer->AcceptsNewEntries())
	{
		for (int32 i = 0; i < numItems; i++)
		{
			MSG_ViewIndex index = GetIndexForABID(idArray[i]);
			AB_LDAPEntry * entry = GetEntryForIndex(index);
			if (entry) // if we have an entry for that index, add it to the destination container...
			{
				ABID entryID = AB_ABIDUNKNOWN;
				destContainer->AddEntry(entry->m_values, entry->m_numAttributes, &entryID); 

				if (CanDeleteEntries())
					DeleteEntries(&idArray[i], 1, NULL);
			}
		}
	}

	return AB_SUCCESS;
}


int AB_LDAPResultsContainerInfo::MoveEntriesTo(AB_ContainerInfo * destContainer, ABID * idArray, int32 numItems)
{
	// can we ever remove elements from here? Not really....
	return CopyEntriesTo(destContainer, idArray, numItems, TRUE);

}

// Getting / Setting entry attributes 

int AB_LDAPResultsContainerInfo::GetEntryAttributes(MSG_Pane * srcPane, ABID entryID, AB_AttribID * attribs, AB_AttributeValue ** values, uint16 * numItems)
{
	MSG_ViewIndex index = GetIndexForABID(entryID);
	return GetEntryAttributesForIndex(srcPane, index, attribs, values, numItems);
}

int AB_LDAPResultsContainerInfo::GetEntryAttributesForIndex(MSG_Pane * srcPane, MSG_ViewIndex index, AB_AttribID * attribs, AB_AttributeValue ** values, uint16 * numItems)
{
	AB_LDAPEntry * entry = GetEntryForIndex(index);
	if (entry)
	{
		AB_AttributeValue *newValues = (AB_AttributeValue *)XP_ALLOC(sizeof(AB_AttributeValue) * (*numItems));
		if (newValues)
		{
			int newValueMarker = 0;
			XP_Bool found = FALSE;

			// for each attribute we want...
			for (uint16 j = 0; j < *numItems; j++)
			{
				int loopStatus = AB_FAILURE;
				if (IsDatabaseAttribute(attribs[j])) // if it is, then it should be in our attribute list, otherwise it is something that needs built like vcard...
 				{
					found = FALSE;
					// ...scan through list of known attributes in attempt to find attrib[j]
					for (uint16 index = 0; index < entry->m_numAttributes; index++)
					{
						if (attribs[j] == entry->m_values[index].attrib)
						{
							loopStatus = AssignEntryValues(&(entry->m_values[index]), &newValues[newValueMarker]);
							found = TRUE;
							break;
						}
					}
					
					if (!found)
						loopStatus = AssignEmptyEntryValue(attribs[j], &newValues[newValueMarker]);
				} // if a database attribute
				else
					loopStatus = GetNonDBEntryAttributeForIndex(srcPane, index, attribs[j], &newValues[newValueMarker]);

				if (loopStatus == AB_SUCCESS) // if someone succeeded....
					newValueMarker++;
			}

			*numItems = newValueMarker;
			*values = newValues;
			return AB_SUCCESS;
		}
		else
			return AB_OUT_OF_MEMORY;
	}
	
	return AB_FAILURE;
 }

// Methods for processung a search
int AB_LDAPResultsContainerInfo::LDAPSearchResults(MSG_Pane * pane, MSG_ViewIndex index, int32 num)
{
	MSG_ResultElement *elem;
	for (MSG_ViewIndex i = index; i < num + index; i++) 
	{
		if (MSG_GetResultElement (pane, index, &elem) == SearchError_Success) 
		{
			AB_LDAPEntry *entry = (AB_LDAPEntry *)XP_ALLOC(sizeof(AB_LDAPEntry));
			if (entry)
			{
				entry->m_numAttributes = AB_NumValidLDAPAttributes;
				if (ConvertResultElement(elem, ValidLDAPAttributesTable, &entry->m_values, &entry->m_numAttributes) == AB_SUCCESS) 
				{
					uint32 position = m_entryList.Add(entry); // record position to act as ABID for call back
					if (pane->GetPaneType() == AB_PICKERPANE && position != MSG_VIEWINDEXNONE)
						((AB_PickerPane *) pane)->ProcessSearchResult(this, GetABIDForIndex(position)); // call back into NC picker with result
				}
				else
					XP_FREE(entry);
			}
		}
	}

	return AB_SUCCESS;
}

int AB_LDAPResultsContainerInfo::FinishSearch(AB_Pane *pane)
{

	if (m_interruptingSearch)
		return AB_SUCCESS;

	NotifyContainerAttribChange(this, AB_NotifyStopSearching, pane);
	if (pane)
		pane->SearchComplete(this); // call back to search (used by name completion)
	m_searchPane = NULL;
	return AB_SUCCESS;
}

int AB_LDAPResultsContainerInfo::NameCompletionSearch(AB_Pane *pane, const char *ncValue)
{
	/* A NULL search string indicates that we should abort the current name
	 * completion search.
	 */
	if (ncValue == NULL)
		return AbortSearch();

	/* Otherwise, start a new name completion search.
	 */
	else
		return PerformSearch(pane, ABLST_NameCompletion, ncValue, 0);
}

ABID AB_LDAPResultsContainerInfo::GetABIDForIndex(const MSG_ViewIndex index)
// what is going on here...well we are assigning our own ABIDs for this ctr because we are not storing any entries in the database.
// Best case scenarion, we would just map view indices directly to ABIDs (i.e. they would be the same). But view indices are zero based
// and 0 is a reserved ABID value for AB_ABIDUNKNOWN. So our ABID designation for an entry will always be 1 + the view index for the entry.
{
	if (index != MSG_VIEWINDEXNONE)
		return (ABID) (index + 1);
	else
		return AB_ABIDUNKNOWN;
}
 
MSG_ViewIndex AB_LDAPResultsContainerInfo::GetIndexForABID(ABID entryID)
{
	MSG_ViewIndex index = MSG_VIEWINDEXNONE;
	if (entryID != AB_ABIDUNKNOWN)
		index = (MSG_ViewIndex) (entryID - 1);
	return index;
}

void AB_LDAPResultsContainerInfo::InitializeDatabase(AB_Database * db)  // each subclass should support code for how it wants to initialize the db.
{
	// okay, we need to get the environment, open a store based on the DIR_Server filename, and then open a table for the address book
	db->db_fluxStack = 0;
	db->db_env = NULL;
	db->db_table = NULL;
	db->db_listTable = NULL;
	db->db_row = NULL;

	// (1) get the environment
	db->db_env = AB_Env_New();
	db->db_env->sEnv_DoTrace = 0;
}

/***********************************************************************************************************************************
 Definitions for PAB Container Info 
 **********************************************************************************************************************************/

AB_PABContainerInfo::AB_PABContainerInfo(MWContext * context, DIR_Server * server) : AB_DirectoryContainerInfo (context, server)
{
	InitializeDatabase(m_db);
}

int AB_PABContainerInfo::Close()
{
	// if we need to do anything for a PAB, insert it here...
	return AB_DirectoryContainerInfo::Close(); 
}


AB_PABContainerInfo::~AB_PABContainerInfo()
{
}

int AB_PABContainerInfo::DeleteEntries(ABID * ids, int32 numIndices, AB_ContainerListener * /*instigator*/)
{
	if (numIndices > 0)
	{
		for (int i = 0; i < numIndices; i++)
		{
			if (m_db->db_table)
			{
				AB_Table_CutRow(m_db->db_table, m_db->db_env, (ab_row_uid) ids[i]);
				AB_Env_ForgetErrors(m_db->db_env);
			}
		}
	}

	return AB_SUCCESS;
}

/* Getting / Setting Entry Attributes with the database implemented */
void AB_PABContainerInfo::InitializeDatabase(AB_Database *db)
{
	// okay, we need to get the environment, open a store based on the DIR_Server filename, and then open a table for the address book
	db->db_fluxStack = 0;
	db->db_env = NULL;
	db->db_table = NULL;
	db->db_listTable = NULL;
	db->db_row = NULL;

	// (1) get the environment
	db->db_env = AB_Env_New();
	db->db_env->sEnv_DoTrace = 0;

	// (2) open a store given the filename in the DIR_Server. What if we don't have a filename? 

	// hack for development only!!!! make sure we don't over write developer's PAB. So always use another
	// file name if our server is the personal address book.
	char * fileName = m_server->fileName;
	if (!XP_STRCASECMP("abook.nab", m_server->fileName))
		fileName = AB_kGromitDbFileName;

	char * myFileName = NULL;
	DIR_GetServerFileName(&myFileName, fileName);
	db->db_store = AB_Env_NewStore(db->db_env, myFileName /* AB_kGromitDbFileName */, AB_Store_kGoodFootprintSpace);
	if (myFileName)
		XP_FREE(myFileName);
	if (db->db_store)
	{
		AB_Store_OpenStoreContent(db->db_store, db->db_env);
		// (3) get an ab_table for the store
		db->db_table = AB_Store_GetTopStoreTable(db->db_store,db->db_env);

		if (AB_Env_Good(db->db_env))
		{
			((ab_Table *) db->db_table)->AddView(ab_Env::AsThis(db->db_env), this);

			// (4) open a row which has all the columns in the address book
			db->db_row = AB_Table_MakeDefaultRow(db->db_table, db->db_env);
			
			AB_Env_ForgetErrors(db->db_env);

			// get table for all entries which are lists...
			db->db_listTable = AB_Table_AcquireListsTable(db->db_table, db->db_env);
		}
	}
}

void AB_PABContainerInfo::SeeBeginModelFlux(ab_Env* /* ev */, ab_Model* /* m */)
{
	if (m_db->db_fluxStack == 0)
	{
		// what do we need to do for this notification?
	}
	
	m_db->db_fluxStack++;
}

void AB_PABContainerInfo::SeeEndModelFlux(ab_Env* /* ev */, ab_Model* /* m */)
{
	if (m_db->db_fluxStack)
		m_db->db_fluxStack--;
	if (m_db->db_fluxStack == 0)
	{
		// what do we need to do for this notification?
		NotifyContainerEntryChange(this, AB_NotifyAll, 0, 0, NULL);
	}
}

void AB_PABContainerInfo::SeeChangedModel(ab_Env* /* ev */, ab_Model* /* m */, const ab_Change* c)
{
	ABID entryID = AB_ABIDUNKNOWN;
	MSG_ViewIndex index = MSG_VIEWINDEXNONE;
	if (c)
	{
		entryID = (ABID) c->mChange_Row; // extract the row UID effected
		index = (MSG_ViewIndex) c->mChange_RowPos;
	}

	if (m_db->db_fluxStack == 0)  // only perform actions if we aren't in a nested model flux...
	{
		if (c->mChange_Mask & ab_Change_kKillRow) /* for delete, remove by index value...*/
			NotifyContainerEntryChange(this, AB_NotifyDeleted, index, entryID, NULL);
		else 
		if (c->mChange_Mask & ab_Change_kAddRow || c->mChange_Mask & ab_Change_kNewRow)
			NotifyContainerEntryChange(this, AB_NotifyInserted, index, entryID, NULL);
		else if (c->mChange_Mask & ab_Change_kPutRow)
		{
			if (index == 0) // database is currently not giving us a position...this needs to change!!!
				index = GetIndexForABID(entryID);
			NotifyContainerEntryChange(this, AB_NotifyPropertyChanged, index, entryID, NULL);
		}
		else
			// let's do worst case and notify all
			NotifyContainerEntryChange(this, AB_NotifyAll, 0, 0, NULL);
	}
}
    
void AB_PABContainerInfo::SeeClosingModel(ab_Env* /* ev */, ab_Model* /* m */, const ab_Change* /* c */)  // database is going away
{
	// stop what we're doing & release all of our database objects, setting them to NULL.
	
	if (GetState() == AB_ObjectOpen)
	{
		// database is closing!!! We should close.....
		Close();  // this should notify listeners that we are going away

	}
}

XP_Bool AB_PABContainerInfo::IsInContainer(ABID entryID)
{
	if (m_db->db_table && m_db->db_env && entryID != AB_ABIDUNKNOWN)
	{
		AB_Env_ForgetErrors(m_db->db_env);
		ab_row_pos pos = AB_Table_RowPos(m_db->db_table, m_db->db_env, (ab_row_uid) entryID);  // if pos > 0, then it is in the table
		if (AB_Env_Good(m_db->db_env) && pos > 0)  
			return TRUE;
		
		// so we had an error or it wasn't in the list...in either case, clear env, return false

		AB_Env_ForgetErrors(m_db->db_env);
	}
	return FALSE; 
}

/**********************************************************************************************************

  Mailing List Container Info class definitions

************************************************************************************************************/

AB_MListContainerInfo::AB_MListContainerInfo(MWContext * context, AB_ContainerInfo * parentContainer, ABID entryID) : AB_NonDirectoryContainerInfo(context, parentContainer, entryID)
{
	// will need to get the actual mailing list table from the database 
	Initialize();
}


AB_MListContainerInfo::~AB_MListContainerInfo()
{
	
}

int AB_MListContainerInfo::Close()
{
	return AB_NonDirectoryContainerInfo::Close();
}

void AB_MListContainerInfo::CloseDatabase(AB_Database *db)		// releases all of our database objects, saves the content, etc. 
{
	if (db->db_row)
	{
		AB_Row_Release(db->db_row, db->db_env);
		db->db_row = NULL;
	}

	// (1) release the table
	if (db->db_table)
	{
		if ( ( (ab_Table *) db->db_table)->IsOpenObject() )
			((ab_Table *) db->db_table)->CutView(ab_Env::AsThis(db->db_env), this);
		
		AB_Table_Close(db->db_table, db->db_env);
		AB_Table_Release(db->db_table, db->db_env);
		// check errors...

		db->db_table = NULL;
	}

	// we don't close the store...only the parent does...

	// (2) release the store
	if (db->db_store)
	{
		AB_Store_SaveStoreContent(db->db_store, db->db_env); /* commit */
		AB_Store_Release(db->db_store, db->db_env);
		AB_Env_ForgetErrors(db->db_env);
		db->db_store = NULL;
	}

	if (db->db_env)
	{
		AB_Env_Release(db->db_env);
		db->db_env = NULL;
	}
}

void AB_MListContainerInfo::Initialize()
{
	// Need to ref count the parent container so that it cannot go away until we are done with it!!
	if (m_parentCtr)
		m_parentCtr->Acquire();

	InitializeDatabase(m_db);
}

void AB_MListContainerInfo::InitializeDatabase(AB_Database *db)
{
	// okay, we need to get the environment, get a table from the parent
	db->db_fluxStack = 0;
	db->db_env = NULL;
	db->db_table = NULL;
	db->db_row = NULL;
	db->db_listTable = NULL;

	// (1) get the environment
	db->db_env = AB_Env_New();
	db->db_env->sEnv_DoTrace = 0;

	// (2) open a store given the filename in the DIR_Server. What if we don't have a filename? 
	db->db_store = m_parentCtr->GetStore(); // do we need to ref count this object??

	if (db->db_store)
	{
		AB_Store_Acquire(db->db_store, db->db_env);  // we need to acquire the store
		AB_Env_ForgetErrors(db->db_env);

		// (3) get an ab_table for the store
		db->db_table = AB_Table_AcquireRowChildrenTable(m_parentCtr->GetTable(), db->db_env, m_entryID);

		if (AB_Env_Good(db->db_env))
		{
			((ab_Table *) db->db_table)->AddView(ab_Env::AsThis(db->db_env), this);
			// (4) open a row which has all the columns in the address book
			db->db_row = AB_Table_MakeDefaultRow(db->db_table, db->db_env);	

			AB_Env_ForgetErrors(db->db_env);

			// get table for all entries which are lists...
//			db->db_listTable = AB_Table_AcquireListsTable(db->db_table, db->db_env);
		}
	}
}

int AB_MListContainerInfo::DeleteEntries(ABID * ids, int32 numIndices, AB_ContainerListener * /* instigator */)
{
	if (numIndices > 0)
	{
		for (int i = 0; i < numIndices; i++)
		{
			if (m_db->db_table)
			{
				AB_Table_CutRow(m_db->db_table, m_db->db_env, (ab_row_uid) ids[i]);
				AB_Env_ForgetErrors(m_db->db_env);
			}
		}
	}

	return AB_SUCCESS;
}

/* database notification handlers */
void AB_MListContainerInfo::SeeChangedModel(ab_Env* /* ev */, ab_Model* /* m */, const ab_Change* c)
{
	ABID entryID = AB_ABIDUNKNOWN;
	MSG_ViewIndex index = MSG_VIEWINDEXNONE;
	if (c)
	{
		entryID = (ABID) c->mChange_Row; // extract the row UID effected
		index = (MSG_ViewIndex) c->mChange_RowPos;
	}


	if (m_db->db_fluxStack == 0)  // only perform actions if we aren't in a nested model flux...
	{
		if (c->mChange_Mask & ab_Change_kKillRow)
			NotifyContainerEntryChange(this, AB_NotifyDeleted, index, entryID, NULL);
		else 
		if (c->mChange_Mask & ab_Change_kAddRow || c->mChange_Mask & ab_Change_kNewRow)
			NotifyContainerEntryChange(this, AB_NotifyInserted, index, entryID, NULL);
		else if (c->mChange_Mask & ab_Change_kPutRow)
		{
			if (index == 0) // database is currently not giving us a position...this needs to change!!!
				index = GetIndexForABID(entryID);
			NotifyContainerEntryChange(this, AB_NotifyPropertyChanged, index, entryID, NULL);
		}
		else if (c->mChange_Mask & ab_Change_kClosing)
			Close();  // we are closing so clean up the ctr and notify all listeners that we are closing
		else
			// let's do worst case and notify all
			NotifyContainerEntryChange(this, AB_NotifyAll, 0, 0, NULL);
	}
}
