/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#ifndef _AB_COM_H_
#define _AB_COM_H_

#include "xp_core.h"
#include "msgcom.h"
#include "abdefn.h"
#include "dirprefs.h"

#ifdef XP_CPLUSPLUS
class AB_Pane;
class AB_ContainerPane;
class AB_ContainerInfo;
class AB_MailingListPane;
class AB_PersonPane;
#else
typedef struct AB_Pane AB_Pane;
typedef struct AB_ContainerPane AB_ContainerPane;
typedef struct AB_ContainerInfo AB_ContainerInfo;
typedef struct AB_MailingListPane AB_MailingListPane;
typedef struct AB_PersonPane AB_PersonPane;
#endif

XP_BEGIN_PROTOS

#define AB_API extern "C"

#if defined(XP_MAC)
#define FE_IMPLEMENTS_SHOW_PROPERTY_SHEET
#endif

const ABID AB_ABIDUNKNOWN = 0;   /* Find a better home for this somewhere! */

/* new errors added by mscott for the 2 pane AB UI. I will eventually name this enumerated type AB_Error */
enum
{
	AB_SUCCESS = 0,
	AB_FAILURE,
	AB_INVALID_PANE,
	AB_INVALID_CONTAINER,
	AB_INVALID_ATTRIBUTE,
	AB_OUT_OF_MEMORY
};

/* these are the types of possible containers */
typedef enum
{
	AB_LDAPContainer,
	AB_MListContainer,       /* a mailing list */
	AB_PABContainer          /* a personal AB */
} AB_ContainerType;

/* AB_ContainerAttributes define the types of information FEs can ask the BE about
   a AB_ContainerInfo in the ABContainerPane. */

typedef enum
{
	attribContainerType,	/* mailing list? LDAP directory? PAB? */
	attribName,				/* the name for the container */
	attribNumChildren,		/* how many child containers does this container have? */
	attribDepth,			/* container depth in the overall hierarchy. 0 == root ctr, 1 == one level below a root container, etc. */
	attribContainerInfo		/* FEs might want to get the container Info * for a line in the container pane */
} AB_ContainerAttribute;

typedef struct AB_ContainerAttribValue
{
	AB_ContainerAttribute attrib;	/* used to determine value of the union */
	union{
		char * string;			   
		int32 number;				/* used by numchildren & depth */
		AB_ContainerType containerType;
		AB_ContainerInfo * container;
	} u;
} AB_ContainerAttribValue;

typedef enum 
{
	AB_Drag_Not_Allowed	= 0x00000000,
	AB_Require_Copy		= 0x00000001,
	AB_Require_Move		= 0x00000002,
	AB_Default_Drag		= 0xFFFFFFFF
} AB_DragEffect; 

typedef enum 
{ 
		  AB_attribUnknown = 0,					/* unrecognized attribute */
          AB_attribEntryType,					/* mailing list or person entry */ 
		  AB_attribEntryID,
          AB_attribFullName, 
          AB_attribNickName, 
          AB_attribGivenName,					/* all of the following are person entry specific */ 
          AB_attribMiddleName, 
          AB_attribFamilyName, 
          AB_attribCompanyName, 
          AB_attribLocality, 
          AB_attribRegion, 
          AB_attribEmailAddress, 
          AB_attribInfo, 
          AB_attribHTMLMail, 
          AB_attribExpandedName, 
          AB_attribTitle, 
          AB_attribPOAddress, 
          AB_attribStreetAddress, 
          AB_attribZipCode, 
          AB_attribCountry, 
          AB_attribWorkPhone, 
          AB_attribFaxPhone, 
          AB_attribHomePhone, 
          AB_attribDistName,       
          AB_attribSecurity, 
          AB_attribCoolAddress, 
          AB_attribUseServer,
		  AB_attribPager,
		  AB_attribCellularPhone,
		  AB_attribDisplayName,
		  AB_attribWinCSID,			/* character set id */
		  AB_attribOther			/* any attrib with this value or higher is always a string type (our dyanmic attributes) */
} AB_AttribID;

/* WARNING!!! WILL BE PHASING THIS STRUCTURE OUT!!!! */
typedef struct AB_EntryAttributeItem{
	AB_AttribID id;
	char * description;  /* resource string specifying a user-readable descript for the attrib i.e. "First Name" */
	XP_Bool sortable;    /* is this attribute sortable? (may help FEs display the column if they know if it can be sorted or not */
}AB_EntryAttributeItem;

typedef enum{
	AB_MailingList = 0,
	AB_Person
} AB_EntryType;

typedef struct AB_AttributeValue
{
	AB_AttribID attrib; /* we need this to know how to read the union */
	union{
		char * string;
		XP_Bool boolValue;
		AB_EntryType entryType;  /* mailing list or person entry */
		int16 shortValue;			 /* use for server type or WinCSID field */
	} u;
} AB_AttributeValue;


/* New Column ID structure used to abstract the columns in the AB_Pane used by the FEs from the attributes thos columns actually
	represent. */
typedef enum{
	AB_ColumnID0 = 0,
	AB_ColumnID1,
	AB_ColumnID2,
	AB_ColumnID3,
	AB_ColumnID4,
	AB_ColumnID5,
	AB_ColumnID6,
	AB_NumberOfColumns   /* make sure this is always the last one!! */
} AB_ColumnID;

typedef struct AB_ColumnInfo{
	AB_AttribID attribID;   /* the attrib ID this column is displaying */
	char * displayString;   /* column display string */
	XP_Bool sortable;		/* is this column attribute sortable? i.e. phone number is not! */
} AB_ColumnInfo;


/**********************************************************************************************
FE Call back functions. We'll show the types here...NOTE: FEs must now register their call back
functions through these APIs so the back end knows which return to use. 
 *********************************************************************************************/

 /* FEs must register this function with the appropriate pane, it is called by the back end in 
response to the following commands: AB_PropertiesCmd, AB_AddUserCmd, AB_AddMailingListCmd */

/* typedef int AB_ShowPropertySheetForEntryFunc (MSG_Pane * pane, AB_EntryType entryType); */
typedef int AB_ShowPropertySheetForEntryFunc (MSG_Pane * pane, MWContext * context);

int AB_SetShowPropertySheetForEntryFunc(
		MSG_Pane * abPane,
		AB_ShowPropertySheetForEntryFunc * func);


/* BE sets the type for the DIR_Server and hands it to the FEs to be displayed & filled out in a 
pane. It is called by the back end in response to the following commands: AB_PropertiesCmd, 
AB_NewLDAPDirectory, AB_NewAddressBook. */

typedef int AB_ShowPropertySheetForDirFunc(DIR_Server * server, MWContext * context, XP_Bool newDirectory /* is it a new directory? */);

int AB_SetShowPropertySheetForDirFunc(
		MSG_Pane * abcPane,						/* container pane */
		AB_ShowPropertySheetForDirFunc * func);


/***************************************************************************************
DON'T USE THIS FUNCTION!!!! THIS WILL BE PHASING OUT!!!!!! #mscott 01/21/98
 This is a callback into the FE instructing them to bring up a person entry pane or a 
 mailing list pane. The back end has already created the pane. A type is included to let the FE
 determine if the pane is a mailing list or person entry pane. In addition, in the case of a mailing list
 pane, the FE must still initialize the mailing list pane before it can be used 
 ***************************************************************************************/
#ifdef FE_IMPLEMENTS_SHOW_PROPERTY_SHEET
extern int FE_ShowPropertySheetForAB2(
	MSG_Pane * pane, /* BE created mailing list or person entry pane */
	AB_EntryType entryType); /* mailing list or person entry */
#endif

/***************************************************************************************
 None pane-specific APIs. These function work on many panes 
 ***************************************************************************************/

int AB_ClosePane(MSG_Pane * pane);

/****************************************************************************************
 Address Book Pane General APIs - creating, initializing, closing, changing containers,
	searching, etc.
*****************************************************************************************/

int AB_CreateABPane(
		MSG_Pane ** abPane,
		MWContext * context,
		MSG_Master * master);

int AB_InitializeABPane(
		MSG_Pane * abPane,
		AB_ContainerInfo * abContainer);


/* to change the container the abPane is currently displaying */
int AB_ChangeABContainer(
		MSG_Pane * abPane,
		AB_ContainerInfo * container);   /* the new containerInfo to display */

int AB_GetEntryIndex(
		MSG_Pane * abPane,
		ABID id,  /* entry id in the database */
		MSG_ViewIndex * index); /* FE allocated, BE fills with index */

int AB_GetABIDForIndex(
		MSG_Pane * abPane,
		MSG_ViewIndex index,
		ABID * id);  /* FE allocated. BE fills with the id you want */

int AB_SearchDirectoryAB2(
		MSG_Pane * abPane,
		char * searchString);

int AB_LDAPSearchResultsAB2(
		MSG_Pane * abPane,
		MSG_ViewIndex index,
		int32 num);

int AB_FinishSearchAB2(MSG_Pane * abPane);

int AB_CommandAB2(
		MSG_Pane * srcPane,   /* NOTE: this can be a ABpane or an ABContainerPane!!! you can delete containers & entries */
		AB_CommandType command, /* delete or mailto are the only currently supported commands */
		MSG_ViewIndex * indices,
		int32 numIndices);

int AB_CommandStatusAB2(
		MSG_Pane * srcPane,    /* NOTE: Can be an ABPane or an ABContainerPane!! */
		AB_CommandType command,
		MSG_ViewIndex * indices,
		int32 numIndices,
		XP_Bool * selectable_p,
		MSG_COMMAND_CHECK_STATE * selected_p,
		const char ** displayString,
		XP_Bool * plural_p);

/* still need to add registering and unregistering compose windows */

/****************************************************************************************
 AB_ContainerInfo General APIs - adding users and a sender. Doesn't require a pane.
*****************************************************************************************/
int AB_AddUserAB2(
		AB_ContainerInfo * abContainer, /* the container to add the person to */
		AB_AttributeValue * values, /* FE defined array of attribute values for the new user. FE must free this array */
		uint16 numItems,
		ABID * entryID);			   /* BE returns the ABID for this new user */

int AB_AddUserWithUIAB2(
		AB_ContainerInfo * abContainer,
		AB_AttributeValue * values,
		uint16 numItems,
		XP_Bool lastOneToAdd);

int AB_AddSenderAB2(
		AB_ContainerInfo * abContainer,
		char * author,
		char * url);


/****************************************************************************************
Drag and Drop Related APIs - vcards, ab lines, containers, etc. 
*****************************************************************************************/

int AB_DragEntriesIntoContainer(
		MSG_Pane * srcPane,				/* could be an ABPane or ABCPane */
		const MSG_ViewIndex * srcIndices,  /* indices of items to be dragged */
		int32 numIndices,
		AB_ContainerInfo * destContainer,
		AB_DragEffect request);         /* copy or move? */

/* FE's should call this function to determine if the drag & drop they want to perform is
   valid or not. I would recommend calling it before the FE actually performs the drag & drop call */
AB_DragEffect AB_DragEntriesIntoContainerStatus(
		MSG_Pane * abPane,
		const MSG_ViewIndex * indices,
		int32 numIndices,
		AB_ContainerInfo * destContainer,
		AB_DragEffect request);        /* do you want to do a move? a copy? default drag? */

/****************************************************************************************
Importing and Exporting - ABs from files, vcards...
*****************************************************************************************/

typedef enum
{
	AB_Filename,      /* char * in import and export APIs contain an FE allocated/freed filename */
	AB_PromptForFileName, /* prompt for file name on import or export */
	AB_Vcard,        
	AB_CommaList,    /* comma separated list of email addresses */
	AB_RawData       /* we don't know what it is, will try to extract email addresses */
} AB_ImportExportType;  

int AB_ImportData(
		AB_ContainerInfo * destContainer,
		const char * buffer, /* could be a filename or NULL (if type = prompt for filename) or a block of data to be imported */
		int32 bufSize, /* how big is the buffer? */
		AB_ImportExportType dataType); /* valid types: All */

/* returns TRUE if the container accepts imports of the data type and FALSE otherwise */
XP_Bool AB_ImportDataStatus(
		AB_ContainerInfo * destContainer,
		AB_ImportExportType dataType);

/* exporting always occurs to a file unless the data type is vcard. Only valid export data types are: vcard, 
   filename, prompt for filename */

int AB_ExportData(
		AB_ContainerInfo * srcContainer,
		char ** buffer, /* filename or NULL. Or if type = Vcard, the BE allocated vcard. FE responosible for freeing it?? */
		int32 * bufSize, /* ignored unless VCard is data type in which case FE allocates, BE fills */
		AB_ImportExportType dataType); /* valid types: filename, prompt for filename, vcard */

/****************************************************************************************
ABContainer Pane --> Creation, Loading, getting line data for each container. 
*****************************************************************************************/
int AB_CreateContainerPane(
		MSG_Pane ** abContainerPane, /* BE will pass back ptr to pane through this */
		MWContext * context,
		MSG_Master * master);

int  AB_InitializeContainerPane(MSG_Pane * abContainerPane);

/* this will return MSG_VIEWINDEXNONE if the container info is not in the pane */
MSG_ViewIndex AB_GetIndexForContainer(
		MSG_Pane * abContainerPane,
		AB_ContainerInfo * container);   /* container you want the index for */ 

/* this will return NULL if the index is invalid */
AB_ContainerInfo * AB_GetContainerForIndex(
		MSG_Pane * abContainerPane,
		const MSG_ViewIndex index);

/* the following set of APIs support getting/setting container pane line data out such as the container's
   name, type, etc. We are going to try a particular discipline for memory management of AB_ContainerAttribValues.
   For APIs which get an attribute value, the BE will actually allocate the struct. To free the space, the FE should
   call AB_FreeContainerAttribValue. For any API which sets an attribute value, the FE is responsible for allocating and
   de-allocating the data. */

int AB_GetContainerAttributeForPane(
		MSG_Pane * abContainerPane,
		MSG_ViewIndex index,			  /* index of container you want information for */
		AB_ContainerAttribute attrib,     /* attribute FE wants to know */
		AB_ContainerAttribValue ** value);  /* BE allocates struct. FE should call AB_FreeContainerAttribValue to free space when done */

int AB_SetContainerAttributeForPane(
		MSG_Pane * abContainerPane,
		MSG_ViewIndex index,
		AB_ContainerAttribValue * value); /* FE handles all memory allocation! */

int AB_GetContainerAttribute(
		AB_ContainerInfo * ctr,
		AB_ContainerAttribute attrib,
		AB_ContainerAttribValue ** value); /* BE allocates struct. FE should call AB_FreeContainerAttribValue to free space when donee */

int AB_SetContainerAttribute(
		AB_ContainerInfo * ctr,
		AB_ContainerAttribValue * value); /* FE handles all memory allocateion / deallocation! */

int AB_GetContainerAttributes(
		AB_ContainerInfo * ctr,
		AB_ContainerAttribute * attribsArray,
		AB_ContainerAttribValue ** valuesArray,
		uint16 * numItems);

int AB_SetContainerAttributes(
		AB_ContainerInfo * ctr,
		AB_ContainerAttribValue * valuesArray,
		uint16 numItems);

/* getting and setting multiple container attributes on a per container pane basis */
int AB_GetContainerAttributesForPane(
		MSG_Pane * abContainerPane,
		MSG_ViewIndex index,
		AB_ContainerAttribute * attribsArray,
		AB_ContainerAttribValue ** valuesArray,
		uint16 * numItems);

int AB_SetContainerAttributesForPane(
		MSG_Pane * abContainerPane,
		MSG_ViewIndex index,
		AB_ContainerAttribValue * valuesArray,
		uint16 numItems);

int AB_FreeContainerAttribValue(AB_ContainerAttribValue * value);  /* BE will free the attribute value */

int AB_FreeContainerAttribValues(AB_ContainerAttribValue * valuesArray, uint16 numItems);

XP_Bool AB_IsStringContainerAttribValue(AB_ContainerAttribValue * value); /* use this to determine if your attrib is a string attrib */


/* Use the following two functions to build the combo box in the Address Window of all the root level containers.
	You first ask for the number of root level (PABs and LDAP directories). Use this number to allocate an array of
	AB_ContainerInfo ptrs. Give this array to the back end and we will fill it. FE can destroy array when done with it.
	FE shoud NOT be deleting the individual AB_ContainerInfo ptrs. */

int AB_GetNumRootContainers(
		MSG_Pane * abContainerPane,
		int32 * numRootContainers);

int AB_GetOrderedRootContainers(
		MSG_Pane * abContainerPane,
		AB_ContainerInfo ** ctrArray, /* FE Allocated & Freed */
		int32 * numCtrs); /* in - # of elements in ctrArray. out - BE fills with # root containers stored in ctrArray */

/* sometimes you want to get a handle on the DIR_Sever for a particular container. Both return NULL if for some
   reason there wasn't a DIR_Server. If the container is a mailing list, returns DIR_Server of the PAB the list is in. 
   NOTE: If you modify any of the DIR_Server properties, you should call AB_UpdateDIRServerForContainer to make sure that the container
   (and hence any pane views on the container) are updated */ 

DIR_Server * AB_GetDirServerForContainer(AB_ContainerInfo * container);

/* Please don't call this function. It is going away!!! Use AB_UpdateDirServerForContainerPane instead!! */
int AB_UpdateDIRServerForContainer(AB_ContainerInfo * container);

/* will create a new container in the container pane if the directory does not already exist. Otherwise updates the
   directory's container in the pane */
int AB_UpdateDIRServerForContainerPane(
		MSG_Pane * abContainerPane,
		DIR_Server * directory);


/*******************************************************************************************************************
 Old Column Header APIs. These will be phased out!!! Please don't use them
 ******************************************************************************************************************/

int AB_GetNumEntryAttributesForContainer(
		AB_ContainerInfo * container,
		uint16 * numItems);  /* BE will fill this integer with the number of available attributes for the container */

int AB_GetEntryAttributesForContainer(
		AB_ContainerInfo * container,
		AB_EntryAttributeItem * items, /* FE allocated array which BE fills with values */
		uint16 * maxItems);   /* FE passes in # elements allocated in array. BE returns # elements filled in array */

/********************************************************************************************************************
 Our New Column Header APIs. We'll be phasing out AB_GetNumEntryAttributesForContainer and AB_GetEntryAttributesForContainer
 ********************************************************************************************************************/

AB_ColumnInfo * AB_GetColumnInfo(
		AB_ContainerInfo * container,
		AB_ColumnID columnID);

int AB_GetNumColumnsForContainer(AB_ContainerInfo * container);

int AB_GetColumnAttribIDs(
		AB_ContainerInfo * container, 
		AB_AttribID * attribIDs, /* FE allocated array of attribs. BE fills with values */
		int * numAttribs);       /* FE passes in # elements allocated in array. BE returns # elements filled */

int AB_FreeColumnInfo(AB_ColumnInfo * columnInfo);

/****************************************************************************************
AB Pane List Data -> how to get ab pane line attributes such as name, address, phone, etc.
*****************************************************************************************/

/* to actually get an entry attribute */ 

int AB_GetEntryAttributeForPane(
		MSG_Pane * abPane,
		MSG_ViewIndex index,
		AB_AttribID attrib,            /* what attribute do you want? */
		AB_AttributeValue ** valueArray);     /* BE handles memory allocation. FE must call AB_FreeEntryAttributeValue when done */

int AB_GetEntryAttribute( 
		AB_ContainerInfo * container, 
		ABID entryid,               /* an identifier or key used to name the object in the container */ 
		AB_AttribID attrib,        /* attribute type the FE wants to know */ 
		AB_AttributeValue ** valueArray);   /* BE handles memory allocation. FE must call AB_FreeEntryAttributeValue when done */

int AB_SetEntryAttribute( 
		AB_ContainerInfo * container,
		ABID entryid,               /* an identifier or key used to name the object in the container */ 
		AB_AttributeValue * value);   /* FE handles all memory allocation */

int AB_SetEntryAttributeForPane( 
		MSG_Pane * abPane,
		MSG_ViewIndex index,
		AB_AttributeValue * value);   /* FE handles all memory allocation */

/* we also allow you to set entry attributes in batch by passing in an array of attribute values */
int AB_SetEntryAttributes(
		AB_ContainerInfo * container,
		ABID entryID,
		AB_AttributeValue * valuesArray, /* FE allocated array of attribute values to set */
		uint16 numItems);  /* FE passes in # items in array */

int AB_SetEntryAttributesForPane(
		MSG_Pane * abPane,
		MSG_ViewIndex index,
		AB_AttributeValue * valuesArray,
		uint16 numItems);

int AB_GetEntryAttributes(
		AB_ContainerInfo * container,
		ABID entryID,
		AB_AttribID * attribs, /* FE allocated array of attribs that you want */
		AB_AttributeValue ** values, /* BE allocates & fills an array of values for the input array of attribs */
		uint16 * numItems);  /* IN: size of attribs array. OUT: # of values in value array */

int AB_GetEntryAttributesForPane(
		MSG_Pane * abPane,
		MSG_ViewIndex index,
		AB_AttribID * attribs, /* FE allocated array of attribs that you want */
		AB_AttributeValue ** values,
		uint16 * numItems);

/* Memory allocation APIs for setting/getting entry attribute values */
int AB_FreeEntryAttributeValue(AB_AttributeValue  * value /* pointer to a value */); 
int AB_FreeEntryAttributeValues(AB_AttributeValue * values /* array of values */, uint16 numItems);

int AB_CopyEntryAttributeValue(
		AB_AttributeValue * srcValue, /* already allocated attribute value you want to copy from */
		AB_AttributeValue * destValue); /* already allocated attribute value you want to copy into */

XP_Bool AB_IsStringEntryAttributeValue(AB_AttributeValue * value);

/****************************************************************************************
Sorting 
*****************************************************************************************/

/* sorting by first name is a global setting. It is set per abPane and does not change
    when you load a new container into the abPane */

XP_Bool AB_GetSortByFirstNameAB2(MSG_Pane * abPane);  /* is the pane sorting by first name? */

void AB_SortByFirstNameAB2(
		MSG_Pane * abPane,
		XP_Bool sortByFirstName);   /* true for sort by first, false for last first */

/* Insert our sort by column stuff here */
int AB_SortByAttribute(
		MSG_Pane * abPane,
		AB_AttribID id,				/* attribute we want to sort by */
		XP_Bool sortAscending);

int AB_GetPaneSortedByAB2(
		MSG_Pane * abPane,
		AB_AttribID * attribID);  /* BE fills with the attribute we are sorting by */


XP_Bool AB_GetPaneSortedAscendingAB2(MSG_Pane * abPane);

/*****************************************************************************************
 APIs for the Mailing List Pane. In addition to these, the mailing list pane responds to 
 previous AB APIs such as AB_Close, MSG_GetNumLines(). 
 ****************************************************************************************/
int AB_InitializeMailingListPaneAB2(MSG_Pane * mailingListPane);

AB_ContainerInfo * AB_GetContainerForMailingList(MSG_Pane * mailingListPane); 

/* this could return ABID = 0 for a new entry that is not in the database */
ABID AB_GetABIDForMailingListIndex(
		MSG_Pane * mailingListPane,
		const MSG_ViewIndex index);

MSG_ViewIndex AB_GetMailingListIndexForABID(
		MSG_Pane * mailingListPane,
		ABID entryID); /* this function could return MSG_VIEWINDEXNONE if entryID = 0 or not in list */

/* Use these two APIs to get Mailing List ENTRY attributes (i.e. people or other mailing list attributes in this mailing list) */
int AB_SetMailingListEntryAttributes(
		MSG_Pane * pane,
		const MSG_ViewIndex index,
		AB_AttributeValue * valuesArray, /* FE allocated array of attribute values you want to set */
		uint16 numItems);

int AB_GetMailingListEntryAttributes(
		MSG_Pane * mailingListPane, 
		const MSG_ViewIndex index,
		AB_AttribID * attribs, /* FE allocated & freed array of attribs you want */
		AB_AttributeValue ** values, /* BE allocates & fills values for the input array of attribs */
		uint16 * numItems); /* FE provides # attribs in array. BE fills with # values returned in values */

/* Use these two APIs to Set and Get the Mailing List properties. */
int AB_GetMailingListAttributes(
		MSG_Pane * mailingListPane,
		AB_AttribID * attribs,			/* FE allocated array of attribs */
		AB_AttributeValue ** values,  /* BE allocates & fills values. FE must call a free to the back end */
		uint16 * numItems);
		
int AB_SetMailingListAttributes(
		MSG_Pane * mailingListPane, 
		AB_AttributeValue * valuesArray, /* FE allocated array of attribute values you want to set */
		uint16 numItems);


/*******************************************************************************************
APIs for the person entry pane aka the person property sheets. The person entry pane is created 
by the back end and given to the front end in the call FE_ShowPropertySheetFor. In the current
incarnation, person atributes are set and retrieved through the person entry pane and NOT through
the AB_ContainerInfo the person is in. If you "cheat" and go through the container directly, you may
not be getting the correct information. When the person entry pane is committed, the changes are pushed
back into the container. During the commit process, if it is a new person then a new entry is made in the
database.
********************************************************************************************/
AB_ContainerInfo * AB_GetContainerForPerson(MSG_Pane * personPane);

ABID AB_GetABIDForPerson(MSG_Pane * personPane); /* could return 0 if new user */

/* get and set the person attributes here */
int AB_SetPersonEntryAttributes(
		MSG_Pane * personPane,
		AB_AttributeValue * valuesArray,
		uint16 numItems);

int AB_GetPersonEntryAttributes(
		MSG_Pane * personPane,
		AB_AttribID * attribs, /* FE allocted & freed array of attribs they want */
		AB_AttributeValue ** values, /* BE allocates & fills values */
		uint16 * numItems); /* in - FE provides # of attribs. out - BE fills with # values */

int AB_CommitChanges(MSG_Pane * pane); /* commits changes to a mailing list pane or a person entry pane! */

XP_END_PROTOS

#endif /* _AB_COM_H */
