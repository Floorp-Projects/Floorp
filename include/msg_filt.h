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
/* foobar Public API for mail (and news?) filters */
#ifndef MSG_RULE_H
#define MSG_RULE_H

/*
	Terminology -	Filter - either a Rule (defined with GUI) or a (Java) Script
					Rule - 
*/
#include "msg_srch.h"

typedef enum
{
	FilterError_Success = 0,	/* no error */
	FilterError_First = SearchError_Last + 1,		/* no functions return this; just for bookkeeping */
	FilterError_NotImplemented,	/* coming soon */
	FilterError_OutOfMemory,	/* out of memory */
	FilterError_FileError,		/* error reading or writing the rules file */
	FilterError_InvalidVersion, /* invalid filter file version */
	FilterError_InvalidIndex,   /* Invalid filter index */
	FilterError_InvalidMotion,  /* invalid filter move motion */
	FilterError_InvalidFilterType, /* method doesn't accept this filter type */
	FilterError_NullPointer,	/* a required pointer parameter was null */
	FilterError_NotRule,		/* tried to get rule for non-rule filter */
	FilterError_NotScript,		/* tried to get a script name for a non-script filter */
	FilterError_InvalidAction,	/* invalid action */
	FilterError_SearchError,	/* error in search code */
	FilterError_Last		/* no functions return this; just for bookkeeping */
} MSG_FilterError;


typedef enum
 {
	 acNone,				/* uninitialized state */
         acMoveToFolder,
         acChangePriority,
         acDelete,
         acMarkRead,
         acKillThread,
	 acWatchThread
 } MSG_RuleActionType;

typedef enum 
{
	filterInboxRule = 0x1,
	filterInboxJavaScript = 0x2,
	filterInbox = 0x3,
	filterNewsRule = 0x4,
	filterNewsJavaScript = 0x8,
	filterNews=0xb,
	filterAll=0xf
} MSG_FilterType;

typedef enum
{
	filterUp,
	filterDown
} MSG_FilterMotion;

typedef int32 MSG_FilterIndex;

/* opaque struct defs - defined in libmsg/pmsgfilt.h */
#ifdef XP_CPLUSPLUS
	struct MSG_Filter;
	struct MSG_Rule;
	struct MSG_RuleAction;
	struct MSG_FilterList;
#else
	typedef struct MSG_FilterList MSG_FilterList;
	typedef struct MSG_Filter MSG_Filter;
	typedef struct MSG_Rule MSG_Rule;
	typedef struct MSG_RuleAction MSG_RuleAction;
#endif

XP_BEGIN_PROTOS

/* Front ends call MSG_OpenFilterList to get a handle to a FilterList, of existing MSG_Filter *.
	These are manipulated by the front ends as a result of user interaction
   with dialog boxes. To apply the new list, fe's call MSG_CloseFilterList.

   For example, if the user brings up the rule management UI, deletes a rule,
   and presses OK, the front end calls MSG_RemoveFilterListAt, and
   then MSG_CloseFilterList.

*/
MSG_FilterError MSG_OpenFilterList(MSG_Master *master, MSG_FilterType type, MSG_FilterList **filterList);
MSG_FilterError MSG_OpenFolderFilterList(MSG_Pane *pane, MSG_FolderInfo *folder, MSG_FilterType type, MSG_FilterList **filterList);
MSG_FilterError MSG_OpenFolderFilterListFromMaster(MSG_Master *master, MSG_FolderInfo *folder, MSG_FilterType type, MSG_FilterList **filterList);
MSG_FilterError MSG_CloseFilterList(MSG_FilterList *filterList);
MSG_FilterError	MSG_SaveFilterList(MSG_FilterList *filterList);	/* save without deleting */
MSG_FilterError MSG_CancelFilterList(MSG_FilterList *filterList);

MSG_FolderInfo *MSG_GetFolderInfoForFilterList(MSG_FilterList *filterList);
MSG_FilterError MSG_GetFilterCount(MSG_FilterList *filterList, int32 *pCount);
MSG_FilterError MSG_GetFilterAt(MSG_FilterList *filterList, 
							MSG_FilterIndex filterIndex, MSG_Filter **filter);
/* these methods don't delete filters - they just change the list. FE still must
	call MSG_DestroyFilter to delete a filter.
*/
MSG_FilterError MSG_SetFilterAt(MSG_FilterList *filterList, 
							MSG_FilterIndex filterIndex, MSG_Filter *filter);
MSG_FilterError MSG_RemoveFilterAt(MSG_FilterList *filterList,
							MSG_FilterIndex filterIndex);
MSG_FilterError MSG_MoveFilterAt(MSG_FilterList *filterList, 
							MSG_FilterIndex filterIndex, MSG_FilterMotion motion);
MSG_FilterError MSG_InsertFilterAt(MSG_FilterList *filterList, 
							MSG_FilterIndex filterIndex, MSG_Filter *filter);

MSG_FilterError MSG_EnableLogging(MSG_FilterList *filterList, XP_Bool enable);
XP_Bool			MSG_IsLoggingEnabled(MSG_FilterList *filterList);

/* In general, any data gotten with MSG_*Get is good until the owning object
   is deleted, or the data is replaced with a MSG_*Set call. For example, the name
   returned in MSG_GetFilterName is valid until either the filter is destroyed,
   or MSG_SetFilterName is called on the same filter.
 */
MSG_FilterError MSG_CreateFilter (MSG_FilterType type,	char *name,	MSG_Filter **result);			
MSG_FilterError MSG_DestroyFilter(MSG_Filter *filter);
MSG_FilterError MSG_GetFilterType(MSG_Filter *, MSG_FilterType *filterType);
MSG_FilterError MSG_EnableFilter(MSG_Filter *, XP_Bool enable);
MSG_FilterError MSG_IsFilterEnabled(MSG_Filter *, XP_Bool *enabled);
MSG_FilterError MSG_GetFilterRule(MSG_Filter *, MSG_Rule ** result);
MSG_FilterError MSG_GetFilterName(MSG_Filter *, char **name);	
MSG_FilterError MSG_SetFilterName(MSG_Filter *, const char *name);
MSG_FilterError MSG_GetFilterDesc(MSG_Filter *, char **description);
MSG_FilterError MSG_SetFilterDesc(MSG_Filter*, const char *description);
MSG_FilterError MSG_GetFilterScript(MSG_Filter *, char **name);
MSG_FilterError MSG_SetFilterScript(MSG_Filter *, const char *name);

MSG_FilterError MSG_RuleAddTerm(MSG_Rule *,     
	MSG_SearchAttribute attrib,    /* attribute for this term                */
    MSG_SearchOperator op,         /* operator e.g. opContains               */
    MSG_SearchValue *value,        /* value e.g. "Dogbert"                   */
	XP_Bool BooleanAND, 	       /* TRUE if AND is the boolean operator. FALSE if OR is the boolean operators */
	char * arbitraryHeader);       /* arbitrary header specified by user. ignored unless attrib = attribOtherHeader */

MSG_FilterError MSG_RuleGetNumTerms(MSG_Rule *, int32 *numTerms);

MSG_FilterError MSG_RuleGetTerm(MSG_Rule *, int32 termIndex, 
	MSG_SearchAttribute *attrib,    /* attribute for this term                */
    MSG_SearchOperator *op,         /* operator e.g. opContains               */
    MSG_SearchValue *value,         /* value e.g. "Dogbert"                   */
	XP_Bool *BooleanAnd,				/* TRUE if AND is the boolean operator. FALSE if OR is the boolean operator */
	char ** arbitraryHeader);        /* arbitrary header specified by user. ignore unless attrib = attribOtherHeader */

MSG_FilterError MSG_RuleSetScope(MSG_Rule *, MSG_ScopeTerm *scope);
MSG_FilterError MSG_RuleGetScope(MSG_Rule *, MSG_ScopeTerm **scope);

/* if type is acChangePriority, value is a pointer to priority.
   If type is acMoveToFolder, value is pointer to folder name.
   Otherwise, value is ignored.
*/
MSG_FilterError MSG_RuleSetAction(MSG_Rule *, MSG_RuleActionType type, void *value);
MSG_FilterError MSG_RuleGetAction(MSG_Rule *, MSG_RuleActionType *type, void **value);

/* help FEs manage menu choices in Filter dialog box */

/* Use this to help build menus in the filter dialogs. See APIs below */
typedef struct MSG_RuleMenuItem 
{
	int16 attrib;
	char name[32];
} MSG_RuleMenuItem;


MSG_FilterError MSG_GetRuleActionMenuItems (
	MSG_FilterType type,		/* type of filter                          */
    MSG_RuleMenuItem *items,    /* array of caller-allocated structs       */
    uint16 *maxItems);            /* in- max array size; out- num returned   */ 

MSG_FilterError MSG_GetFilterWidgetForAction( MSG_RuleActionType action,
											  MSG_SearchValueWidget *widget );

MSG_SearchError MSG_GetValuesForAction( MSG_RuleActionType action,
											   MSG_SearchMenuItem *items, 
											   uint16 *maxItems);

void			MSG_ViewFilterLog(MSG_Pane *pane);

/*
** Adding/editting javascript filters.
**
** The FE calls one of the below functions, along with a callback and some closure
** data.  This callback is invoked when the user clicks OK in the JS filter dialog.
** If CANCEL is pressed, the callback is not invoked.
**
** If the user called MSG_EditJSFilter, the filter_index parameter of the callback
** is the same one passed in.  If the user called MSG_NewJSFilter, the filter_index
** parameter is -1.
**
** The filter_changed parameter is TRUE if the user modified any of the fields of
** the javascript filter, and FALSE otherwise.
*/
typedef void (*JSFilterCallback)(void* arg, MSG_FilterIndex filter_index, XP_Bool filter_changed);

void MSG_EditJSFilter(MWContext *context, MSG_FilterList *filter_list,
					  MSG_FilterIndex filter_index,
					  JSFilterCallback cb, void *arg);
void MSG_NewJSFilter(MWContext *context, MSG_FilterList *filter_list,
					 MSG_FilterType filter_type, JSFilterCallback cb, void *arg);

XP_END_PROTOS

#endif
