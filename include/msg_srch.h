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
/* 
 * Public API for searching mail, news, and LDAP 
 * pieces of this API are also used by filter rules and address book
 * 
 */

#ifndef _MSG_SRCH_H
#define _MSG_SRCH_H

#include "msgcom.h"   /* for MSG_PRIORITY, MessageKey */
#include "dirprefs.h" /* for DIR_AttributeId */

#define FE_IMPLEMENTS_BOOLEAN_OR
#define B3_SEARCH_API

typedef enum
{
    SearchError_First,	/* no functions return this; just for bookkeeping    */
    SearchError_Success,              /* no error                            */
    SearchError_NotImplemented,       /* coming soon                         */

    SearchError_OutOfMemory,          /* can't allocate required memory      */
    SearchError_NullPointer,          /* a req'd pointer parameter was null  */
    SearchError_ScopeAgreement,       /* attr or op not supp in this scope   */
    SearchError_ListTooSmall,         /* menu item array not big enough      */

    SearchError_ResultSetEmpty,       /* search done, no matches found       */
    SearchError_ResultSetTooBig,      /* too many matches to get them all    */

    SearchError_InvalidAttribute,     /* specified attrib not in enum        */
    SearchError_InvalidScope,         /* specified scope not in enum         */
    SearchError_InvalidOperator,      /* specified op not in enum            */
	
    SearchError_InvalidSearchTerm,    /* cookie for search term is bogus     */
    SearchError_InvalidSearchType,    /* search type is bogus                */
    SearchError_InvalidScopeTerm,     /* cookie for scope term is bogus      */
    SearchError_InvalidResultElement, /* cookie for result element is bogus  */
    SearchError_InvalidPane,          /* context probably bogus              */
    SearchError_InvalidStream,        /* in strm bad (too short? bad magic?) */
    SearchError_InvalidFolder,        /* given folderInfo isn't searchable   */
    SearchError_InvalidIndex,         /* the passed index is invalid         */

    SearchError_HostNotFound,         /* couldn't connect to server          */
    SearchError_Timeout,              /* network didn't respond              */
    SearchError_DBOpenFailed,         /* couldn't open off-line msg db       */
	SearchError_Busy,                 /* operation can not be performed now  */

    SearchError_NotAMatch,            /* used internally for term eval       */
    SearchError_ScopeDone,            /* used internally for scope list eval */

	SearchError_ValueRequired,        /* string to search for not provided   */
	SearchError_Unknown,              /* some random error                   */

    SearchError_Last      /* no functions return this; just for bookkeeping  */
} MSG_SearchError;

typedef enum
{
    scopeMailFolder,
    scopeNewsgroup,
    scopeLdapDirectory,
    scopeOfflineNewsgroup,
	scopeAllSearchableGroups
} MSG_ScopeAttribute;

typedef enum
{
    attribSubject = 0,	/* mail and news */
    attribSender,   
    attribBody,	
    attribDate,	

    attribPriority,	    /* mail only */
    attribMsgStatus,	
    attribTo,
    attribCC,
    attribToOrCC,

    attribCommonName,   /* LDAP only */
    attrib822Address,	
    attribPhoneNumber,
    attribOrganization,
    attribOrgUnit,
    attribLocality,
    attribStreetAddress,
    attribSize,
    attribAnyText,      /* any header or body */
	attribKeywords,

    attribDistinguishedName, /* LDAP result elem only */
    attribObjectClass,       
    attribJpegFile,

    attribLocation,          /* result list only */
    attribMessageKey,        /* message result elems */

	attribAgeInDays,    /* for purging old news articles */

	attribGivenName,    /* for sorting LDAP results */
	attribSurname, 

	attribFolderInfo,	/* for "view thread context" from result */

	attribCustom1,		/* custom LDAP attributes */
	attribCustom2,
	attribCustom3,
	attribCustom4,
	attribCustom5,

	attribMessageId, 
	attribOtherHeader,  /* for mail and news. MUST ALWAYS BE LAST attribute since we can have an arbitrary # of these...*/

    kNumAttributes      /* must be last attribute */
} MSG_SearchAttribute;

/* NB: If you add elements to this enum, add only to the end, since 
 *     RULES.DAT stores enum values persistently
 */
typedef enum
{
    opContains = 0,     /* for text attributes              */
    opDoesntContain,
    opIs,               /* is and isn't also apply to some non-text attrs */
    opIsnt, 
    opIsEmpty,

    opIsBefore,         /* for date attributes              */
    opIsAfter,
    
    opIsHigherThan,     /* for priority. opIs also applies  */
    opIsLowerThan,

    opBeginsWith,              
	opEndsWith,

    opSoundsLike,       /* for LDAP phoenetic matching      */
	opLdapDwim,         /* Do What I Mean for simple search */

	opIsGreaterThan,	
	opIsLessThan,

    kNumOperators       /* must be last operator            */
} MSG_SearchOperator;

/* FEs use this to help build the search dialog box */
typedef enum
{
    widgetText,
    widgetDate,
    widgetMenu,
	widgetInt,          /* added to account for age in days which requires an integer field */
    widgetNone
} MSG_SearchValueWidget;

/* Used to specify type of search to be performed */
typedef enum
{
	searchNone,
	searchRootDSE,
	searchNormal,
	searchLdapVLV
} MSG_SearchType;

/* Use this to specify the value of a search term */
typedef struct MSG_SearchValue
{
    MSG_SearchAttribute attribute;
    union
    {
        char *string;
        MSG_PRIORITY priority;
        time_t date;
        uint32 msgStatus; /* see MSG_FLAG in msgcom.h */
        uint32 size;
        MessageKey key;
		uint32 age; /* in days */
		MSG_FolderInfo *folder;
    } u;
} MSG_SearchValue;

/* Use this to help build menus in the search dialog. See APIs below */
#define kSearchMenuLength 64
typedef struct MSG_SearchMenuItem 
{
    int16 attrib;
    char name[kSearchMenuLength];
    XP_Bool isEnabled;
} MSG_SearchMenuItem;

#ifdef XP_CPLUSPLUS
    struct MSG_ScopeTerm;
    struct MSG_ResultElement;
    struct DIR_Server;
#else
    #include "dirprefs.h"
    typedef struct MSG_ScopeTerm MSG_ScopeTerm;
    typedef struct MSG_ResultElement MSG_ResultElement;
#endif

XP_BEGIN_PROTOS

/* manage lifetime of internal search memory */
MSG_SearchError MSG_SearchAlloc (MSG_Pane *);   /* alloc memory in context  */
MSG_SearchError MSG_SearchFree (MSG_Pane *);    /* free memory in context   */

MSG_SearchError MSG_AddSearchTerm (
    MSG_Pane *searchPane,          /* ptr to pane to add criteria            */
    MSG_SearchAttribute attrib,    /* attribute for this term                */
    MSG_SearchOperator op,         /* operator e.g. opContains               */
    MSG_SearchValue *value,        /* value e.g. "Dogbert"                   */
	XP_Bool BooleanAND, 		   /*  set to true if associated boolean operator is AND */
	char * arbitraryHeader);       /* user defined arbitrary header. ignored unless attrib = attribOtherHeader */

/* It's generally not necessary for the FE to read the list of terms after 
 * the list has been built. However, in our Basic/Advanced LDAP search dialogs
 * the FE is supposed to remember the criteria, and since that information is
 * lying around in the backend anyway, we'll just make it available to the FE
 */
MSG_SearchError MSG_CountSearchTerms (
	MSG_Pane *searchPane,
	int *numTerms);
MSG_SearchError MSG_GetNthSearchTerm (
	MSG_Pane *searchPane,
	int whichTerm,
	MSG_SearchAttribute *attrib,
	MSG_SearchOperator *op,
	MSG_SearchValue *value);

MSG_SearchError MSG_CountSearchScopes (
	MSG_Pane *searchPane, 
	int *numScopes);
MSG_SearchError MSG_GetNthSearchScope (
	MSG_Pane *searchPane,
	int which,
	MSG_ScopeAttribute *scopeId,
	void **scope);

/* add a scope (e.g. a mail folder) to the search */
MSG_SearchError MSG_AddScopeTerm (
    MSG_Pane *searchPane,          /* ptr to pane to add search scope        */
    MSG_ScopeAttribute attrib,     /* what kind of scope term is this        */
    MSG_FolderInfo *folder);       /* which folder to search                 */

/* special cases for LDAP since LDAP isn't really a folderInfo */
MSG_SearchError MSG_AddLdapScope (
    MSG_Pane *searchPane,
    DIR_Server *server);
MSG_SearchError MSG_AddAllLdapScopes (
    MSG_Pane *searchPane,
    XP_List *dirServerList);

/* Call this function everytime the scope changes! It informs the FE if 
   the current scope support custom header use. FEs should not display the
   custom header dialog if custom headers are not supported */

XP_Bool MSG_ScopeUsesCustomHeaders(
	MSG_Master * master,	
	MSG_ScopeAttribute scope,
	void * selection,              /* could be a folder or server based on scope */
	XP_Bool forFilters);		   /* is this a filter we are talking about? */

XP_Bool MSG_IsStringAttribute(     /* use this to determine if your attribute is a string attrib */
	MSG_SearchAttribute);

/* add all scopes of a given type to the search */
MSG_SearchError MSG_AddAllScopes (
    MSG_Pane *searchPane,          /* ptr to pane to add scopes              */
    MSG_Master *master,            /* mail or news scopes                    */
    MSG_ScopeAttribute attrib);    /* what kind of scopes to add             */

/* begin execution of the search */
MSG_SearchError MSG_Search (
    MSG_Pane *searchPane);        /* So we know how to work async            */
MSG_SearchError MSG_InterruptSearchViaPane (
	MSG_Pane *searchPane);        /* ptr to pane with search to interrupt    */
void *MSG_GetSearchParam (
	MSG_Pane *pane);              /* ptr to pane to retrieve param from      */
MSG_SearchType MSG_GetSearchType (
	MSG_Pane *pane);              /* ptr to pane to retrieve search type from*/
MSG_SearchError MSG_SetSearchParam (
	MSG_Pane *searchPane,         /* ptr to pane to add search param         */
	MSG_SearchType type,          /* type of specialized search to perform   */
	void *param);                 /* extended search parameter               */
uint32 MSG_GetNumResults (        /* ptr to pane to retrieve get # results   */
	MSG_Pane *pane);

/* manage elements in list of search hits */
MSG_SearchError MSG_GetResultElement (
    MSG_Pane *searchPane,        /* ptr to pane containing results           */
    MSG_ViewIndex idx,           /* zero-based index of result to get        */
    MSG_ResultElement **result); /* filled in resultElement. NOT allocated   */
MSG_SearchError MSG_GetResultAttribute (
    MSG_ResultElement *elem,     /* which result elem to get value for       */
    MSG_SearchAttribute attrib,  /* which attribute to get value for         */
    MSG_SearchValue **result);   /* filled in value                          */
MSG_SearchError MSG_OpenResultElement (
    MSG_ResultElement *elem,     /* which result elem to open                */
    void *window);               /* MSG_Pane* for mail/news, contxt for LDAP */
MWContextType MSG_GetResultElementType (
    MSG_ResultElement *elem);    /* context type needed for this elem        */
MWContext *MSG_IsResultElementOpen (
    MSG_ResultElement *elem);    /* current context if open, NULL if not     */
MSG_SearchError MSG_SortResultList (
    MSG_Pane *searchPane,         /* ptr to pane containing results          */
    MSG_SearchAttribute sortKey,  /* which attribute is the sort key         */
    XP_Bool descending);          /* T- sort descending, F- sort ascending   */
MSG_SearchError MSG_DestroySearchValue (
    MSG_SearchValue *value);      /* free struct and heap-based struct elems */
MSG_SearchError MSG_ModifyLdapResult (
    MSG_ResultElement *elem,      /* which result element to modify          */
    MSG_SearchValue *val);        /* new value to stuff in                   */
MSG_SearchError MSG_AddLdapResultsToAddressBook( 
    MSG_Pane *searchPane,         /* ptr to pane containing results          */
    MSG_ViewIndex *indices,       /* selection array                         */
	int count);                   /* size of array                           */
MSG_SearchError MSG_ComposeFromLdapResults(
    MSG_Pane *searchPane,         /* ptr to pane containing results          */
    MSG_ViewIndex *indices,       /* selection array                         */
    int count);                   /* size of array                           */

/* help FEs manage menu selections in Search dialog box */
MSG_SearchError MSG_GetSearchWidgetForAttribute (
    MSG_SearchAttribute attrib,     /* which attr to get UI widget type for  */
    MSG_SearchValueWidget *widget); /* what kind of UI widget specifies attr */

/* For referring to DIR_Servers and MSG_FolderInfos polymorphically */
typedef struct MSG_ScopeUnion
{
	MSG_ScopeAttribute *attribute;
	union
	{
		DIR_Server *server;
		MSG_FolderInfo *folder;
	} u;
} MSG_ScopeUnion;


/* always call this routine b4 calling MSG_GetAttributesForSearchScopes to 
   determine how many elements your MSG_SearchMenuItem array needs to be */
MSG_SearchError MSG_GetNumAttributesForSearchScopes(
	MSG_Master *master, 
	MSG_ScopeAttribute scope,
	void ** selArray,			  /* selected items for LCD calculation      */
	uint16 selCount,			  /* number of selected items				 */
	uint16 *numItems);			  /* out - number of attribute items for scope */

MSG_SearchError MSG_GetAttributesForSearchScopes (
    MSG_Master *master,         
	MSG_ScopeAttribute scope,
    void **selArray,              /* selected items for LCD calculation      */
    uint16 selCount,              /* number of selected items                */
    MSG_SearchMenuItem *items,    /* array of caller-allocated structs       */
    uint16 *maxItems);            /* in- max array size; out- num returned   */ 

MSG_SearchError MSG_GetOperatorsForSearchScopes (
    MSG_Master *master,         
	MSG_ScopeAttribute scope,
    void **selArray,              /* selected items for LCD calculation      */
    uint16 selCount,              /* number of selected items                */
    MSG_SearchAttribute attrib,   /* return available ops for this attrib    */
    MSG_SearchMenuItem *items,    /* array of caller-allocated structs       */
    uint16 *maxItems);            /* in- max array size; out- num returned   */

MSG_SearchError MSG_GetScopeMenuForSearchMessages (
	MSG_Master *master,
	MSG_FolderInfo **selArray,
	uint16 selCount,
	MSG_SearchMenuItem *items,
	uint16 *maxItems);

/* always call this routine b4 calling MSG_GetAttributesForFilterScopes to 
   determine how many elements your MSG_SearchMenuItem array needs to be */
MSG_SearchError MSG_GetNumAttributesForFilterScopes(
	MSG_Master *master, 
	MSG_ScopeAttribute scope,	  
	void ** selArray,			  /* selected items for LCD calculation */
	uint16 selCount,			  /* number of selected items			*/
	uint16 *numItems);			  /* out - number of attribute items for scope */

MSG_SearchError MSG_GetAttributesForFilterScopes (
    MSG_Master *master,         
	MSG_ScopeAttribute scope,
    void **selArray,              /* selected items for LCD calculation      */
    uint16 selCount,              /* number of selected items                */
    MSG_SearchMenuItem *items,    /* array of caller-allocated structs       */
    uint16 *maxItems);            /* in- max array size; out- num returned   */ 

MSG_SearchError MSG_GetOperatorsForFilterScopes (
    MSG_Master *master,         
	MSG_ScopeAttribute scope,
    void **selArray,              /* selected items for LCD calculation      */
    uint16 selCount,              /* number of selected items                */
    MSG_SearchAttribute attrib,   /* return available ops for this attrib    */
    MSG_SearchMenuItem *items,    /* array of caller-allocated structs       */
    uint16 *maxItems);            /* in- max array size; out- num returned   */

/*****************************************************************************
 These two functions have been added to the search APIs to help support Arbitrary
 Headers. In particular, the FEs need to be able to grab a semaphore when they
 create an edit headers dialog (we only want to allow 1 dialog to be open at a time).
 AcquireEditHeadersSemaphore returns TRUE if the FE successfully acquired the semaphore
 and FALSE if someone else acquired it. ReleaseEditHeaderSemaphore returns TRUE if you
 were the original holder of the semaphore and the semaphore was released. FALSE if you
 were not the original holder 
 **********************************************************************************/
XP_Bool MSG_AcquireEditHeadersSemaphore (MSG_Master * master, void * holder);
XP_Bool MSG_ReleaseEditHeadersSemaphore  (MSG_Master * master, void * holder);

MSG_SearchError MSG_SearchAttribToDirAttrib (
	MSG_SearchAttribute searchAttrib,
	DIR_AttributeId *dirAttrib);


MSG_SearchError MSG_GetBasicLdapSearchAttributes (
	DIR_Server *server, 
	MSG_SearchMenuItem *items, 
	int *maxItems);

/* maybe these belong in msgcom.h? */
void MSG_GetPriorityName (MSG_PRIORITY priority, char *name, uint16 max);
void MSG_GetUntranslatedPriorityName (MSG_PRIORITY priority, 
																			char *name, uint16 max);
void MSG_GetStatusName (uint32 status, char *name, uint16 max);
MSG_PRIORITY MSG_GetPriorityFromString(const char *priority);

/* support for profile searching in Dredd */
MSG_SearchError MSG_SaveProfileStatus (MSG_Pane *searchPane, XP_Bool *enabled);
MSG_SearchError MSG_SaveProfile (MSG_Pane *searchPane, const char *profileName);

/* support for searching all Dredd groups + all subscribed groups */
MSG_SearchError MSG_AddAllSearchableGroupsStatus(MSG_Pane *searchPane, XP_Bool *enabled);

/* support for opening a search result in its thread pane context */
XP_Bool MSG_GoToFolderStatus (MSG_Pane *searchPane, 
							  MSG_ViewIndex *indices,
							  int32 numIndices);

/* used between libnet and libmsg to allow searching for characters which
 * are otherwise significant in news: URLs
 */
extern char *MSG_EscapeSearchUrl (const char *value);
extern char *MSG_UnEscapeSearchUrl (const char *value);

/* This is how "search:" of different mail/news folder types works */
extern int MSG_ProcessSearch (MWContext *context);
extern int MSG_InterruptSearch (MWContext *context);

XP_END_PROTOS

#endif /* _MSG_SRCH_H */
