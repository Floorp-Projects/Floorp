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

#ifndef _ABCINFO_H_
#define _ABCINFO_H_

#include "abcom.h"
#include "msg_srch.h" // needed for LDAP searching
#include "abntfy.h"  // address book notification classes
#include "dirprefs.h"
#include "msgmast.h"
#include "ptrarray.h"

// database include files
#include "abtable.h"
#include "abmodel.h"

/********************************************************************************************

  AB_ContainerInfo's are a powerful class. They were modeled off of MSG_FolderInfo. 
  We have containerInfo subclasses for LDAP, PABs and Mailing Lists as these are the three
  valid container types. We reference count each container. When you are done with it, you 
  should release the reference count. In addition, if you create one and we already have a ctr 
  then we pass it back to you by maintaining container caches. 

  A word (well several actually) about containers. AB_ContainerInfo is a base class used to
  describe some general methods that all containers respond to. Its responsibilities include
  container reference counting, and generic APIs for setting and getting attributes for entries 
  (people, mailing lists, etc) in the container and setting/getting container attributes. All
  subclasses containers must support these functions.

  Containers come in two general flavors: directory and non-directory containers. Directory containers
  like LDAP and Personal Address Books (PABs) container DIR_Server structs which are basically
  run time representations of the directry preferences among other things. It is a 
  structure left over from the 4.0 code that we have now wrapped up in a ContainerInfo class. Thus,
  we can use the DIR_Server as a means of identifying directory containers because there is a one to
  one relationship between DIR_Servers and directory containers.

  Non-directory containers are usually collections of things which can be contained in other
  containers. The only current non-directory container is a mailing list. Non-directory containers
  are identified by their parent container and an entry ID (ABID) use to identify the container
  in the parent container. The parent container can be a directory container or another non-directory
  container. We don't place restrictions on the parent type. 

  The directory, non-directory container info classes are responsible for keeping container caches, 
  and storing the DIR_Server or Parent Container / ABID pair. 

  Our final layer of container info's are subclassed from the directory / non-diretory container infos.
  LDAP and PAB containers are subclassed from the directory containers. Each of them interacts with
  the database in its own fashion so database support enters at this level. The mailing list container
  is the only type currently subclassed from the non-directory containers. It also handles database
  manipulation for the mailing list. All of these classes must support the AB_ContainerInfo virtual
  methods for getting/setting container attributes and entry attributes!! If you add a new container
  type, it will most likely be at this layer. Make sure you support all of the necessary virtual
  AB_ContainerInfo methods so that everyone can use AB_ContainerInfo's instead of the specific containers
  for getting and setting attributes. 
  
  Caching Notes: Occurs at the directory / non-directory level.
  To re-iterate, container's are cached. However, different containers have different attributes we cache 
  on for looking them up. In general, you can create a container (PAB or LDAP) given a DIR_Server. 
  So we can look containers up in the cache by their DIR_Server to determine if we have already created 
  this container or not. Directory containers do not need to be contained in another container. 
  They are root level containers. 

  Our Delete Model for containers. Okay, here's the process. Ideally, anyone who is hanging on to a 
  container should reference count the object by calling AB_ConainerInfo::AddReference(). The containers
  should never be directly deleted by users (hence the destructors are all protected!). When you are
  done with the container, call ::Release() to release the object. 

  Releasing: If the ref count on a continer hits zero, then the release code closes the container (all
  container types support a close method which releases any objects and frees any resources) and calls
  GoingAway. For most containers GoingAway is the same as deleting the object. However, ctr classes which
  are derived from the database notification class: ab_view are ref counted through ab_view as well
  as through AB_ContainerInfo. The GoingAway method for these classes just does a release through
  AB_View. Then, if the ab_view ref count hits zero, IT assumes responsiblity for deleting the container.
  The destructors for the containers should not do anything except scream very loudly if the classes
  Close method has not been called yet. 

**********************************************************************************************/

class AB_DirectoryContainerInfo;
class AB_NonDirectoryContainerInfo;
class AB_LDAPContainerInfo;
class AB_LDAPResultsContainerInfo;
class AB_PABContainerInfo;
class AB_MListContainerInfo;

class AB_AttribValueArray : public XPPtrArray
{
public:
	AB_AttributeValue * GetAt(int i) const { return (AB_AttributeValue *) XPPtrArray::GetAt(i);}
};

class AB_ContainerInfoArray : public XPPtrArray
{
public:
	AB_ContainerInfo * GetAt(int i) const { return (AB_ContainerInfo *) XPPtrArray::GetAt(i);}
};

class AB_ABIDArray : public XPPtrArray
{
public:
	ABID GetAt (int i) const { return (ABID) XPPtrArray::GetAt(i);}
};

/*****************************************************************************************************
  The following enumerated type is used to describe the possible states a container can be in. An Open
  Container is one which is open, and is still hanging on to any reference counted objects / memory allocations.
  A closed container, is one which has been closed via ::Close(). Close releases any ref counted objects the
  container is holding onto (i.e. database objects) and sets those pointers to NULL. It also frees any memory
  allocations. An object can be closed and still have a ref count > 0, once the ref count hits zero, the object
  is then deleted. The ClosingState is used to describe a situation where the container is in the process of
  closing. 
  *****************************************************************************************************/
typedef enum _AB_ObjectState
{
	AB_ObjectOpen,
	AB_ObjectClosed,
	AB_ObjectClosing
} AB_ObjectState;

/*****************************************************************************************************
 The following structure is used to provide some abstraction between the database implementation
 details and the container infos.  This is not so important for most of the container infos, but is
 important for LDAP, since it needs to maintain two databases.
 *****************************************************************************************************/
typedef struct _AB_Database
{
	int       db_fluxStack;
	AB_Store *db_store;
	AB_Env   *db_env;
	AB_Table *db_table;
	AB_Table *db_listTable;
	AB_Row   *db_row;
} AB_Database;

/************************************************************************************************************
 Helper object to abstract away the reference counting stuff. Ideally I would use AB_Object which was 
 written forthe database for reference counting but it requies database specifi objects such as AB_Env. 
 I don't plan on exposing AB_Env to other classes like AB_Pane. And these other objects may eventually require 
 a ref counted object.
 **************************************************************************************************************/

class AB_ReferencedObject
{
public:
	AB_ReferencedObject();
	virtual ~AB_ReferencedObject();

	// ref count getters and setters...
	virtual int32 Acquire();
	virtual int32 Release();
	virtual int Close();	// override this with whatever you need done on a close!!!

	// object state methods
	XP_Bool IsOpen() { return m_objectState == AB_ObjectOpen;}  // is the container open or has it been closed? 
	XP_Bool IsClosed() {return m_objectState == AB_ObjectClosed;}
	AB_ObjectState GetState() { return m_objectState;}

protected:
	virtual void GoAway() {delete this;} // if your object should not be deleted when ref count hits 0, then over ride this method!
	AB_ObjectState m_objectState;
	int32 m_refCount;
};

/************************************************************************************************************
The following structure is used to encapsulate all of the state we need to import into a container. It includes
the database objects we need to keep track of where we are in the import
 **************************************************************************************************************/
typedef struct _AB_ImportState
{
	AB_File  *db_file; /* db abstraction for the file we are importing from */
	AB_Thumb *db_thumb;
} AB_ImportState;

/* This is the closure the FE_PromptForFileName returns to us along with the file name */
typedef struct _AB_ImportClosure
{
	AB_ContainerInfo * container;
	MSG_Pane * pane;  
} AB_ImportClosure;


/************************************************************************************************************

  The base class for containers....

*************************************************************************************************************/

class AB_ContainerInfo : public AB_ContainerAnnouncer, public ab_View, public AB_ReferencedObject
{
public:
	// Two ways for creating a container...We will re-direct these calls to the appropriate subclass...
	static int Create(MWContext * context, DIR_Server * server, AB_ContainerInfo ** newCtr); 
	static int Create(MWContext * context, AB_ContainerInfo * parentCtr, ABID entryID, AB_ContainerInfo ** newCtr);

	virtual int Close();
	virtual AB_ContainerType GetType() const = 0;

	// Database related methods (Note: each subclass has its own behavior for dbase interaction...
	virtual void SeeBeginModelFlux(ab_Env* ev, ab_Model* m);
    virtual void SeeEndModelFlux(ab_Env* ev, ab_Model* m);    
    virtual void SeeChangedModel(ab_Env* ev, ab_Model* m, const ab_Change* c);  
    virtual void SeeClosingModel(ab_Env* ev, ab_Model* m, const ab_Change* c);  // database is going away

	AB_Table * GetTable() { return m_db->db_table;} // doesn't acquire so be careful
	AB_Store * GetStore() { return m_db->db_store;} // doesn't acquire so be careful
		
	// Generic container methods
	virtual uint32 GetNumEntries(); // total number of known entries
	virtual ABID GetABIDForIndex(const MSG_ViewIndex index);  // returns AB_UNKNOWNABID if bad index
	virtual MSG_ViewIndex GetIndexForABID(ABID entryID); // returns MSG_VIEWINDEXNONE if bad entryID
	virtual XP_Bool IsInContainer(ABID entryID);
	virtual XP_Bool IsIndexInContainer(MSG_ViewIndex index); // assumes view indices map directly into rows in the dbase table!

	// Selection methods
	virtual XP_Bool UseExtendedSelection(MSG_Pane * /*pane*/) { return FALSE; }
	virtual void *CopySelectionEntry(void * /*entry*/) { return NULL; }
	virtual void DeleteSelectionEntry(void * /*entry*/) { }
	virtual void *GetSelectionEntryForIndex(MSG_ViewIndex /*index*/) { return NULL; }
	static int CompareSelections(const void *e1, const void *e2);

	// entry column data methods
	virtual AB_ColumnInfo * GetColumnInfo(AB_ColumnID columnID);
	virtual int GetNumColumnsForContainer();
	virtual int GetColumnAttribIDs(AB_AttribID * attribIDs, int * numAttribs);

	// adding entries to the container
	virtual int AddEntry(AB_AttributeValue * values, uint16 numItems, ABID * entryID); /* could be person or list...*/

	virtual int AddUserWithUI(MSG_Pane * srcPane, AB_AttributeValue * values, uint16 numItems, XP_Bool lastOneToAdd);
	virtual int AddSender(char * author, char * url);

	// safe type casting
	virtual AB_LDAPContainerInfo * GetLDAPContainerInfo();  // returns NULL if we aren't a LDAP container
	virtual AB_PABContainerInfo * GetPABContainerInfo();		// returns NULL if we aren't a PAB container
	virtual AB_MListContainerInfo * GetMListContainerInfo();

	// Importing/Exporting
	int ImportData(MSG_Pane * pane, const char * buffer, int32 bufSize, AB_ImportExportType dataType);
	XP_Bool ImportDataStatus(AB_ImportExportType dataType);
	int ExportData(MSG_Pane * pane, char ** buffer, int32 * bufSize, AB_ImportExportType dataType);

	static void ExportFileCallback(MWContext * context, char * fileName, void * closure);
	int ExportFile(MSG_Pane * pane, MWContext * context, const char * fileName);
	XP_Bool IsExporting() {return m_isExporting;}
	
	static void ImportFileCallback(MWContext * context, char * fileName, void * closure); /* callback for FE_PromptForFileName */
	int ImportFile(MSG_Pane * pane, MWContext * context, const char * fileName); // imports the file into the current container
	XP_Bool IsImporting() { return m_isImporting;}
	
	// asynch import/export methods...
	int ImportBegin(MWContext * context, const char * fileName, void ** importCookie, XP_Bool * importFinished);
	int ImportMore(void ** importCookie, XP_Bool * importFinished);
	int ImportInterrupt(void ** importCookie); // free import state and termineate import
	int ImportFinish(void ** importCookie); // frees import structures...
	int ImportProgress(void * importCookie, uint32 * position, uint32 * fileLength, uint32 * passCount); /* the pass we are on (out of 2) */

	int ExportBegin(MWContext * context, const char * fileName, void ** exportCookie, XP_Bool * exportFinished);
	int ExportMore(void ** exportCookie, XP_Bool * exportFinished);
	int ExportInterrupt(void ** exportCookie);
	int ExportFinish(void ** exportCookie);
	int ExportProgress(void * exportCookie, uint32 * numberExported, uint32 * totalEntries);

	// child container methods
	virtual int32 GetNumChildContainers();  // returns # of child containers inside this container...
	virtual int AcquireChildContainers(AB_ContainerInfo ** arrayOfContainers /* allocated by caller */, 
										int32 * numContainers /* in - size of array, out - # containers added */);
	virtual XP_Bool IsEntryAContainer(ABID entryID);  // returns TRUE if entry is a container, FALSE otherwise

	virtual XP_Bool IsDirectory() = 0;
	virtual DIR_Server * GetDIRServer() { return NULL;}
	virtual int UpdateDIRServer();

	// Container Property Attributes (AB_ContainerAttribute)
	virtual int GetAttribute(AB_ContainerAttribute attrib, AB_ContainerAttribValue ** value);  // get one
	virtual int GetAttributes(AB_ContainerAttribute * attribArray, AB_ContainerAttribValue ** valueArray, uint16 * numItems); // get many 
	virtual int GetAttribute(AB_ContainerAttribute attrib, AB_ContainerAttribValue * value); // use this when value is already allocated

	virtual int SetAttribute(AB_ContainerAttribValue * value); 
	virtual int SetAttributes(AB_ContainerAttribValue * valuesArray, uint16 numItems);

	// Attribute Accessors
	virtual int GetEntryAttribute(MSG_Pane * srcPane, ABID entryID, AB_AttribID attrib, AB_AttributeValue ** value);
	virtual int GetEntryAttribute(MSG_Pane * srcPane, ABID entryID, AB_AttribID attrib, AB_AttributeValue * values /* for an already allocated value */);
	virtual int GetEntryAttributes(MSG_Pane * srcPane, ABID entryID, AB_AttribID * attrib, AB_AttributeValue ** values, uint16 * numItems);
	virtual int GetEntryAttributeForIndex(MSG_Pane * srcPane, MSG_ViewIndex index, AB_AttribID attrib, AB_AttributeValue ** value);
	virtual int GetEntryAttributesForIndex(MSG_Pane * srcPane, MSG_ViewIndex index, AB_AttribID * attribs, AB_AttributeValue ** values, uint16 * numItems);

	virtual int SetEntryAttribute(ABID entryID, AB_AttributeValue * value);
	virtual int SetEntryAttributes(ABID entryID, AB_AttributeValue * valuesArray, uint16 numItems);
	virtual int SetEntryAttributeForIndex(MSG_ViewIndex index, AB_AttributeValue * value);
	virtual int SetEntryAttributesForIndex(MSG_ViewIndex index, AB_AttributeValue * valuesArray, uint16 numItems);

	// Sorting
	virtual XP_Bool IsEntryAttributeSortable(AB_AttribID attrib) { return (attrib != AB_ColumnID0 && attrib != AB_ColumnID4); } // stub only.
	virtual int SortByAttribute(AB_AttribID attrib);
	virtual int SortByAttribute(AB_AttribID attrib, XP_Bool sortAscending);
	int GetSortedBy(AB_AttribID * attrib);
	XP_Bool GetSortAscending() {return m_sortAscending;}
	virtual	void SetSortAscending(XP_Bool ascending);

	// virtual list view methods
	virtual void UpdatePageSize(uint32 pageSize);  // every time a pane (with a ctr view) changes its page size, it notifies the ctr. 

	// Entry Related
	virtual XP_Bool AcceptsNewEntries() {return TRUE;} // used for determining drop status
	virtual XP_Bool CanDeleteEntries()  {return TRUE;} // used to determine drag status
	// asynchronous copy APIs...
	static char * BuildCopyUrl(); // url string for a addbook-copy url
	virtual int BeginCopyEntry(MWContext * context, AB_AddressBookCopyInfo * copyInfo, void ** copyCookie, XP_Bool * finished);
	virtual int MoreCopyEntry(MWContext * context, AB_AddressBookCopyInfo * copyInfo, void ** copyCookie, XP_Bool * finished);
	virtual int FinishCopyEntry(MWContext * context, AB_AddressBookCopyInfo * copyInfo, void ** copyCookie);
	virtual int InterruptCopyEntry(MWContext * context, AB_AddressBookCopyInfo * copyInfo, void ** copyCookie);

	// callback after running a copy url...
	static void PostAddressBookCopyUrlExitFunc (URL_Struct *URL_s, int status, MWContext *window_id);

	virtual int CopyEntriesTo(AB_ContainerInfo * destContainer, ABID * idArray, int32 numItems, XP_Bool deleteAterCopy = FALSE);
	virtual int MoveEntriesTo(AB_ContainerInfo * destContainer, ABID * idArray, int32 numItems);
	virtual int DeleteEntries(ABID *ids, int32 numIndices, AB_ContainerListener * instigator = NULL);

	// Non database entry attribute producers
	virtual char * GetFullAddress(MSG_Pane * srcPane, ABID entryID);  // caller must free the null terminated string returned...
	virtual char * GetVCard(MSG_Pane * srcPane, ABID entryID);		  // caller must free null terminated string returned

	static int GetExpandedHeaderForEntry(char ** expandedString, MSG_Pane * srcPane, AB_ContainerInfo * ctr, ABID entryID);
	int GetExpandedHeaderForEntry(char ** expandedString, MSG_Pane * srcPane, ABID entryID, AB_ABIDArray idsAlreadyAdded);

	// we need generic type down APIs. Type down is an asynch operation for all containers. If a future container is synch, make it look
	// asynch so everything is consistent!
	virtual int PreloadSearch(MSG_Pane *pane) { return AB_SUCCESS; }
	virtual int TypedownSearch(AB_Pane * /* requester */,const char * /* typeDownValue */,MSG_ViewIndex /* startIndex */);
	virtual int NameCompletionSearch(AB_Pane * /* requester */, const char * /* ncValue */) { return AB_SUCCESS; }

	char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
	virtual char * GetDescription(); // caller must free the char *

	static int CompareByType(const void * ctr1, const void * ctr2);

protected:
	// creating / destroying
	AB_ContainerInfo(MWContext * context);
	virtual ~AB_ContainerInfo();
	virtual void GoAway();  // call this when you would call the destructor...no one but GoAway should call the destructor

	virtual void InitializeDatabase(AB_Database *db) = 0;
	virtual void CloseDatabase(AB_Database *db);		// releases all of our database objects, saves the content, etc. 

	MWContext * m_context;
	char * m_description;
	int32 m_depth; // Root containers have a depth of 0, 
	XP_Bool m_IsCached; // flag used to mark if a ctr is cached or not. 
	XP_Bool m_isImporting; // TRUE if this ctr is currently importing...
	XP_Bool m_isExporting;
	
	// container generic virtual list view support variables
	uint32 m_pageSize;  // largest page size by any pane with a view on the container. Used by virtual list view Code
	AB_AttribID m_sortedBy;
	XP_Bool m_sortAscending;

	virtual void LoadDescription() = 0;  // different containers have different ways of storing the container attribute for description...
	virtual void LoadDepth() = 0; // different containers have different depths (where they are in the hierarchy...root = 0)

	int AssignEntryValues(AB_AttributeValue * srcValue, AB_AttributeValue * destValue); // copies src to destination including strings
	int AssignEmptyEntryValue(AB_AttribID attrib, AB_AttributeValue * value /* already allocated */);

	XP_Bool IsDatabaseAttribute(AB_AttribID attrib);
	int GetNonDBEntryAttribute(MSG_Pane * srcPane, ABID entryID, AB_AttribID attrib, AB_AttributeValue * value /* must already be allocated!!!*/);
	int GetNonDBEntryAttributeForIndex(MSG_Pane * srcPane, MSG_ViewIndex index, AB_AttribID attrib, AB_AttributeValue * value /* must already be allocated!!!*/);

	const char * ConvertAttribValueToDBase(AB_AttributeValue * value); // who frees this string returned? 
	int ConvertDBaseValueToAttribValue(const char * dbaseValue, AB_AttributeValue * value /* already allocated */, AB_AttribID attrib);
	AB_Column_eAttribute ConvertAttribToDBase(AB_AttribID attrib);

	// database specific member data
	AB_Database *m_db;
};

class AB_DirectoryContainerInfo : public AB_ContainerInfo
{
	typedef AB_ContainerInfo ParentClass;

public:
	// Creating
	static int Create(MWContext * context, DIR_Server * server, AB_ContainerInfo ** newCtr /* we allocate and return new ctr */);
	virtual int Close();
	virtual XP_Bool IsDirectory() { return TRUE;}
	virtual XP_Bool SelectDatabase() { return TRUE; }

	// DIR_Server Activity
	virtual DIR_Server * GetDIRServer() { return m_server;}
	virtual int UpdateDIRServer();

	// searching methods
	virtual int NameCompletionSearch(AB_Pane * requester, const char * ncValue);

	static void RegisterOfflineCallback();
	static void UnregisterOfflineCallback();
	
	// accessor for use by OfflineContainerCallback
	static XP_List* GetContainerCacheList() { return m_ctrCache; }

protected:
	DIR_Server * m_server; // all directory containers have a DIR_Server
	static int m_offlineCallbackCount;

	// Container Cache related methods and attributes
	static XP_List * m_ctrCache;  // we cache all containers so we only create 1 per DIR_Server
	void CacheAdd();
	void CacheRemove();

	// Cache LookUp Function
	static AB_DirectoryContainerInfo * CacheLookup (DIR_Server * server);  // looks in container cache and returns ctr with this DIR_Server. NULL otherwise

	virtual void LoadDescription();
	virtual void LoadDepth();

	// creating & deleting
	AB_DirectoryContainerInfo(MWContext * context, DIR_Server * server);
	virtual ~AB_DirectoryContainerInfo();
};


class AB_NonDirectoryContainerInfo : public AB_ContainerInfo
{
	typedef AB_ContainerInfo ParentClass;

public:
	// Creating
	static int Create(MWContext * context, AB_ContainerInfo * parentContainer, ABID entryID, AB_ContainerInfo ** newCtr);
	virtual int Close();

	virtual XP_Bool IsDirectory() { return FALSE;}

	virtual DIR_Server * GetDIRServer();

	// non-directory specific getters...
	AB_ContainerInfo * GetParentContainer() { return m_parentCtr;} // doesn't acquire the ctr!!!!
	ABID GetContainerABID() { return m_entryID;}

	virtual int DeleteSelfFromParent(AB_ContainerListener * instigator = NULL); 

protected:
	AB_NonDirectoryContainerInfo(MWContext * context, AB_ContainerInfo * parentCtr, ABID entryID);
	~AB_NonDirectoryContainerInfo();

	AB_ContainerInfo * m_parentCtr;
	ABID m_entryID;

	virtual void LoadDescription();
	virtual void LoadDepth();

	// Container Cache related methods and attributes
	static XP_List * m_ctrCache;  // we cache all containers so we only create 1 per DIR_Server
	void CacheAdd();
	void CacheRemove();

	// Cache LookUp Function
	static AB_NonDirectoryContainerInfo * CacheLookup(AB_ContainerInfo * parentContainer, ABID entryID);
};


#define AB_LDAP_UNINITIALIZED_INDEX (uint32)-1

typedef enum _AB_LDAPSearchType
{
	ABLST_None,
	ABLST_RootDSE,
	ABLST_Basic,
	ABLST_Extended,
	ABLST_Typedown,
	ABLST_NameCompletion
} AB_LDAPSearchType;

class AB_LDAPEntry
{
public:
	// Construction/destruction
	AB_LDAPEntry();

	// Sort order methods
	static AB_AttribID GetPrimarySortAttribute();
	static void SetSortOrder(AB_AttribID *list = NULL, uint32 num = 0);

	// Overloaded operator helpers
	int operator==(AB_LDAPEntry &entry) { return Compare(entry) == 0; }
	int operator!=(AB_LDAPEntry &entry) { return Compare(entry) != 0; }
	int operator<(AB_LDAPEntry &entry) { return Compare(entry) < 0; }
	int operator>(AB_LDAPEntry &entry) { return Compare(entry) > 0; }

	// Member data
	XP_Bool            m_isValid;
	uint16             m_numAttributes;
	AB_AttributeValue *m_values;

//protected:
	int Compare(AB_LDAPEntry &entry);

private:
	static AB_AttribID m_sortOrder[3];
	static uint32 m_sortAttributes;
};

class AB_LDAPEntryArray : public XPPtrArray
{
public:
	AB_LDAPEntry * GetAt(int i) const { return (AB_LDAPEntry *) XPPtrArray::GetAt(i);}
};

typedef struct AB_ReplicaAttributeMap
{
	int                    numAttribs;   /* Number of attributes in the list */
	AB_Column_eAttribute  *attribIDs;    /* List of address book IDs         */
	char                 **attribNames;  /* List of LDAP attribute names     */
} AB_ReplicaAttributeMap;

/**************************************************************************************************************

  LDAP ContainerInfo declarations.

*************************************************************************************************************/

class AB_LDAPContainerInfo : public AB_DirectoryContainerInfo
{
	friend AB_DirectoryContainerInfo;
	friend class AB_LDAPEntryList;

	typedef AB_DirectoryContainerInfo ParentClass;

protected:
	// Constructors/destructors
	AB_LDAPContainerInfo(MWContext * context, DIR_Server * server, XP_Bool UseDB = TRUE); // can only create through AB_ContainerInfo::Create
	virtual ~AB_LDAPContainerInfo();

public:
	// Initialization/deinitialization functions
	virtual int Close();

	// State and type methods
	virtual AB_ContainerType GetType() const { return AB_LDAPContainer; }
	virtual AB_LDAPContainerInfo * GetLDAPContainerInfo() { return this; }
	virtual XP_Bool UseExtendedSelection(MSG_Pane *pane);
	virtual ABID GetABIDForIndex(const MSG_ViewIndex index);  // returns AB_ABIDUNKNOWN if bad index
	virtual MSG_ViewIndex GetIndexForABID(ABID entryID); // returns MSG_VIEWINDEXNONE if bad entryID
	virtual XP_Bool UsingReplica() { return m_db == m_dbReplica;}

	// Search APIs
	// if you want a ctr that reflects results for a search like name completion, get a results ctr and 
	// perform the search on it. You are assured that n one else is giung to touch or clear away your search results. 
	virtual AB_LDAPResultsContainerInfo * AcquireLDAPResultsContainer(MWContext * context /* context to run the search in */);  

	int SearchDirectory (AB_Pane * pane, char * searchString);  
	virtual int TypedownSearch(AB_Pane * pane, const char * typeDownValue, MSG_ViewIndex startIndex);
	virtual int NameCompletionSearch(AB_Pane * pane, const char * ncValue);
	virtual int PreloadSearch(MSG_Pane *pane);
	virtual int LDAPSearchResults(MSG_Pane * pane, MSG_ViewIndex index, int32 num);
	virtual int FinishSearch(AB_Pane * pane);

	// Entry Related
	virtual int DeleteEntries(ABID *, int32, AB_ContainerListener * /*instigator = NULL*/) { return AB_INVALID_CONTAINER; }
	virtual uint32 GetNumEntries();

	// Sorting Related
	virtual const char *GetPairMatchingSortAttribute();
	virtual const char *GetSortAttributeNameFromID(AB_AttribID attrib);
	virtual XP_Bool IsEntryAttributeSortable(AB_AttribID attrib);
	virtual XP_Bool PairMatchesAttribute(const char *, const char *, const char *, XP_Bool);

	// Getting and Setting entry attributes.....
	virtual int GetEntryAttributes(MSG_Pane * srcPane, ABID entryID, AB_AttribID * attrib, AB_AttributeValue ** values, uint16 * numItems);
	virtual int GetEntryAttributesForIndex(MSG_Pane *srcPane, MSG_ViewIndex index, AB_AttribID * attribs, AB_AttributeValue ** values, uint16 * numItems);

	// Replication Methods
	static AB_LDAPContainerInfo *BeginReplication(MWContext *context, DIR_Server *server);
	virtual void EndReplication();
	virtual XP_Bool AddReplicaEntry(char **valueList);
	virtual void DeleteReplicaEntry(char *targetDn);
	virtual void RemoveReplicaEntries();
	virtual AB_ReplicaAttributeMap *CreateReplicaAttributeMap();
	virtual void FreeReplicaAttributeMap(AB_ReplicaAttributeMap *attribMap);
	virtual XP_Bool ReplicaAttributeMatchesId(AB_ReplicaAttributeMap *attribMap, int attribIndex, DIR_AttributeId id);
	virtual AB_ReplicaAttributeMap *GetReplicaAttributeMap() { return m_attribMap; }

	// Selection Methods
	virtual void *CopySelectionEntry(void *entry);
	virtual void DeleteSelectionEntry(void *entry);
	virtual void *GetSelectionEntryForIndex(MSG_ViewIndex index);
	static int CompareSelections(const void *e1, const void *e2);

	// Drag and Drop related
	virtual XP_Bool AcceptsNewEntries() {return FALSE;} // used for determining drop status. You cannot add elements by dragging into the ctr
	virtual XP_Bool CanDeleteEntries()  {return FALSE;} // used to determine drag status. You cannot move elements out of ctr
	// Note: you  cannot move or copy entries into an LDAP directory...this would be a cool feature in the future though...
	virtual int CopyEntries(AB_ContainerInfo *, ABID *, int32, XP_Bool) { return AB_FAILURE; }
	virtual int MoveEntries(AB_ContainerInfo *, ABID *, int32) { return AB_FAILURE; }

	virtual char * GetVCard(MSG_Pane * srcPane, ABID entryID);

	// url methods
	char * BuildLDAPUrl(const char *prefix, const char * distName); // use this to build an ldap url for this ldap ctr....

protected:
	// Database related
	virtual void InitializeDatabase(AB_Database *db);
	virtual XP_Bool SelectDatabase();

	// Search Related
	virtual int AbortSearch();
	virtual int PerformSearch(MSG_Pane *pane, AB_LDAPSearchType type, const char *attrib, MSG_ViewIndex index, XP_Bool removeEntries = TRUE);
	virtual char *GetSearchAttributeFromEntry(AB_LDAPEntry *entry);

	// Data conversion methods
	AB_AttribID ConvertLDAPToABAttribID(MSG_SearchAttribute);
	int ConvertResultElement(MSG_ResultElement * elem, MSG_SearchAttribute * LDAPattribs, AB_AttributeValue ** values, uint16 * numItems);

	// Entry Related
	int CopyEntryValues(MSG_Pane * srcPane, MSG_ViewIndex index, AB_LDAPEntry *entry, AB_AttribID *attribs, AB_AttributeValue **values, uint16 *numItems);

	// VLV support methods
	virtual void RemoveAllEntries();
	virtual XP_Bool ServerSupportsVLV();
	virtual void UpdatePageSize(uint32 pageSize);


	// Data members
	XP_Bool m_interruptingSearch;
	XP_Bool m_performingVLV;
	XP_Bool m_updateTotalEntries;
	uint32 m_totalEntries;          /* Total countent on server                   */
	uint32 m_searchPageSize;        /* Search page size at last search            */
	MSG_ViewIndex m_firstIndex;     /* Index of first entry in cache (0-based)    */
	MSG_ViewIndex m_lastIndex;      /* Index of last entry in cache (0-based)     */
	MSG_ViewIndex m_targetIndex;    /* Index of target entry (0-based, estimated) */
	AB_Pane *m_searchPane;          /* Pane performing most recent search         */
	AB_LDAPSearchType m_searchType; /* Type of search being performed             */

	XP_Bool m_replicating;			/* TRUE if replica is being updated           */
	AB_ReplicaAttributeMap *m_attribMap;

	AB_Database *m_dbNormal;
	AB_Database *m_dbReplica;
	
	AB_LDAPEntry m_entryMatch;
	XPPtrArray m_entryList;
	XPPtrArray m_ncEntryList;
};

class AB_LDAPResultsContainerInfo : public AB_LDAPContainerInfo
/* 
	This container can only be created from an LDAP ctr. It is not cached in the ctr cache but can be reference counted.
	It represents a set of search results on a particular container. It holds NO database state. It is designed for panes who do
	searches on an LDAP ctr and who want to keep the results of that search around. If you want to have multiple sets of search results
	on the same ctr, use this object to represent each set.
	*/
{
	friend AB_LDAPContainerInfo;  // the only object that can create a results container
protected:
	AB_LDAPResultsContainerInfo(MWContext * context, AB_LDAPContainerInfo * srcCtr);
	virtual ~AB_LDAPResultsContainerInfo();
public:
	// Initialization/deinitialization functions
	virtual int Close();
	
	// State and type methods
	virtual AB_ContainerType GetType() const { return AB_LDAPContainer; }
	virtual AB_LDAPContainerInfo * GetLDAPResultsContainerInfo() { return this; }
	virtual ABID GetABIDForIndex(const MSG_ViewIndex index);  // returns AB_ABIDUNKNOWN if bad index
	virtual MSG_ViewIndex GetIndexForABID(ABID entryID); // returns MSG_VIEWINDEXNONE if bad entryID

	// Search APIs
	virtual int NameCompletionSearch(AB_Pane * pane, const char * ncValue);
	
	virtual int LDAPSearchResults(MSG_Pane * pane, MSG_ViewIndex index, int32 num);
	virtual int FinishSearch(AB_Pane * pane);

	// Generic container methods
	virtual uint32 GetNumEntries(); // total number of search result entries
	virtual XP_Bool IsInContainer(ABID entryID);
	virtual XP_Bool IsIndexInContainer(MSG_ViewIndex index); // assumes view indices map directly into rows in the dbase table!

	// Moving, Copyig entries
	virtual int CopyEntriesTo(AB_ContainerInfo * destContainer, ABID * idArray, int32 numItems, XP_Bool deleteAterCopy = FALSE);
	virtual int MoveEntriesTo(AB_ContainerInfo * destContainer, ABID * idArray, int32 numItems);

	// getting result entries
	virtual int GetEntryAttributes(MSG_Pane * srcPane, ABID entryID, AB_AttribID * attrib, AB_AttributeValue ** values, uint16 * numItems);
	virtual int GetEntryAttributesForIndex(MSG_Pane *srcPane, MSG_ViewIndex index, AB_AttribID * attribs, AB_AttributeValue ** values, uint16 * numItems);

	// containers were initially designed around a database. Search result ctrs do not have ANY hooks to a database. So we implement all of the
	// ctr dbase methods here as empty methods...
	virtual void SeeBeginModelFlux(ab_Env* /* ev */, ab_Model* /* m */) { return;}
    virtual void SeeEndModelFlux(ab_Env* /* ev */, ab_Model* /* m */) { return; }
    virtual void SeeChangedModel(ab_Env* /* ev */, ab_Model* /* m */, const ab_Change* /* c */) {return;}
    virtual void SeeClosingModel(ab_Env* /* ev */, ab_Model* /* m */, const ab_Change* /* c */) { return;}  // database is going away

	AB_Table * GetTable() { return NULL;} // doesn't acquire so be careful
	AB_Store * GetStore() { return NULL;} // doesn't acquire so be careful

	virtual int AddEntry(AB_AttributeValue * /* values */, uint16 /* numItems */, ABID * /* entryID */) { return AB_SUCCESS;} /* could be person or list...*/
	virtual int AddUserWithUI(AB_AttributeValue * /* values */, uint16 /* numItems */, XP_Bool /* lastOneToAdd */) { return AB_SUCCESS;}
	virtual int AddSender(char * /*author*/, char * /*url*/) { return AB_SUCCESS;}

	virtual char * GetVCard(MSG_Pane * /*srcPane*/, ABID /*entryID*/) { return NULL;} // until we write our own...

protected:
	AB_LDAPEntryArray m_entryList; // list of LDAP entries reflecting the current search
	
	virtual void InitializeDatabase(AB_Database * db);  // each subclass should support code for how it wants to initialize the db.
//	virtual void CloseDatabase(AB_Database * /* db */) {return;}		// releases all of our database objects, saves the content, etc. 

	AB_LDAPEntry * GetEntryForIndex(const MSG_ViewIndex index);
	virtual void RemoveAllUsers();

};

class AB_PABContainerInfo : public AB_DirectoryContainerInfo
{
	typedef AB_DirectoryContainerInfo ParentClass;

	friend AB_DirectoryContainerInfo;
public:
	virtual int Close();
	virtual AB_ContainerType GetType() const { return AB_PABContainer;}
	virtual AB_PABContainerInfo * GetPABContainerInfo() { return this;}

	// database notification methods
	virtual XP_Bool IsInContainer(ABID entryID);
	virtual void SeeBeginModelFlux(ab_Env* ev, ab_Model* m);
    virtual void SeeEndModelFlux(ab_Env* ev, ab_Model* m);
    virtual void SeeChangedModel(ab_Env* ev, ab_Model* m, const ab_Change* c);  
    virtual void SeeClosingModel(ab_Env* ev, ab_Model* m, const ab_Change* c);  // database is going away

	// Entry Related
	virtual int DeleteEntries(ABID * ids, int32 numIndices, AB_ContainerListener * instigator = NULL);

protected:
	AB_PABContainerInfo(MWContext * context, DIR_Server * server); // can only create through AB_ContainerInfo::Create
	virtual ~AB_PABContainerInfo();
	virtual void InitializeDatabase(AB_Database *db);

};


/********************************************************************************************************

  Mailing list container Info class.

************************************************************************************************************/

class AB_MListContainerInfo : public AB_NonDirectoryContainerInfo
{
	typedef AB_NonDirectoryContainerInfo ParentClass;

	friend AB_NonDirectoryContainerInfo;
public:
	virtual int Close();  // releases Ref Count on parent container
	virtual AB_ContainerType GetType() const { return AB_MListContainer; }
	virtual AB_MListContainerInfo * GetMListContainerInfo() { return this;}

	// Entry Related
	virtual int DeleteEntries(ABID * ids, int32 numIndices, AB_ContainerListener * instigator = NULL);

	// database notification methods
//	virtual XP_Bool IsInContainer(ABID entryID);
//	virtual void SeeBeginModelFlux(ab_Env* ev, ab_Model* m);
//    virtual void SeeEndModelFlux(ab_Env* ev, ab_Model* m);    
    virtual void SeeChangedModel(ab_Env* ev, ab_Model* m, const ab_Change* c);  
//    virtual void SeeClosingModel(ab_Env* ev, ab_Model* m, const ab_Change* c);  // database is going away

protected:
	AB_MListContainerInfo(MWContext * context, AB_ContainerInfo * parentContainer, ABID entryID /* ABID for the mailing list in the ctr */);
	virtual ~AB_MListContainerInfo();

	virtual void InitializeDatabase(AB_Database *db);
	virtual void CloseDatabase(AB_Database *db);		// releases all of our database objects, saves the content, etc. 

	void Initialize();

};

#endif // _ABCINFO_H
