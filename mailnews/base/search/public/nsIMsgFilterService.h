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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _nsMsgFilterService_H_
#define _nsMsgFilterService_H_



/* Front ends call MSG_OpenFilterList to get a handle to a FilterList, of existing nsMsgFilter *.
	These are manipulated by the front ends as a result of user interaction
   with dialog boxes. To apply the new list, fe's call MSG_CloseFilterList.

   For example, if the user brings up the rule management UI, deletes a rule,
   and presses OK, the front end calls MSG_RemoveFilterListAt, and
   then MSG_CloseFilterList.

*/
nsMsgFilterError MSG_OpenFilterList(nsMsgFilterType type, nsIMsgFilterList **filterList);
nsMsgFilterError MSG_OpenFolderFilterList(MSG_Pane *pane, MSG_FolderInfo *folder, nsMsgFilterType type, nsMsgFilterList **filterList);
nsMsgFilterError MSG_OpenFolderFilterListFromMaster(MSG_Master *master, MSG_FolderInfo *folder, nsMsgFilterType type, nsMsgFilterList **filterList);
nsMsgFilterError MSG_CloseFilterList(nsMsgFilterList *filterList);
nsMsgFilterError	MSG_SaveFilterList(nsMsgFilterList *filterList);	/* save without deleting */
nsMsgFilterError MSG_CancelFilterList(nsMsgFilterList *filterList);

MSG_FolderInfo *MSG_GetFolderInfoForFilterList(nsMsgFilterList *filterList);
nsMsgFilterError MSG_GetFilterCount(nsMsgFilterList *filterList, int32 *pCount);
nsMsgFilterError MSG_GetFilterAt(nsMsgFilterList *filterList, 
							nsMsgFilterIndex filterIndex, nsMsgFilter **filter);
/* these methods don't delete filters - they just change the list. FE still must
	call MSG_DestroyFilter to delete a filter.
*/
nsMsgFilterError MSG_SetFilterAt(nsMsgFilterList *filterList, 
							nsMsgFilterIndex filterIndex, nsMsgFilter *filter);
nsMsgFilterError MSG_RemoveFilterAt(nsMsgFilterList *filterList,
							nsMsgFilterIndex filterIndex);
nsMsgFilterError MSG_MoveFilterAt(nsMsgFilterList *filterList, 
							nsMsgFilterIndex filterIndex, nsMsgFilterMotion motion);
nsMsgFilterError MSG_InsertFilterAt(nsMsgFilterList *filterList, 
							nsMsgFilterIndex filterIndex, nsMsgFilter *filter);

nsMsgFilterError MSG_EnableLogging(nsMsgFilterList *filterList, XP_Bool enable);
XP_Bool			MSG_IsLoggingEnabled(nsMsgFilterList *filterList);

/* In general, any data gotten with MSG_*Get is good until the owning object
   is deleted, or the data is replaced with a MSG_*Set call. For example, the name
   returned in MSG_GetFilterName is valid until either the filter is destroyed,
   or MSG_SetFilterName is called on the same filter.
 */
nsMsgFilterError MSG_CreateFilter (nsMsgFilterType type,	char *name,	nsMsgFilter **result);			
nsMsgFilterError MSG_DestroyFilter(nsMsgFilter *filter);
nsMsgFilterError MSG_GetFilterType(nsMsgFilter *, nsMsgFilterType *filterType);
nsMsgFilterError MSG_EnableFilter(nsMsgFilter *, XP_Bool enable);
nsMsgFilterError MSG_IsFilterEnabled(nsMsgFilter *, XP_Bool *enabled);
nsMsgFilterError MSG_GetFilterRule(nsMsgFilter *, MSG_Rule ** result);
nsMsgFilterError MSG_GetFilterName(nsMsgFilter *, char **name);	
nsMsgFilterError MSG_SetFilterName(nsMsgFilter *, const char *name);
nsMsgFilterError MSG_GetFilterDesc(nsMsgFilter *, char **description);
nsMsgFilterError MSG_SetFilterDesc(nsMsgFilter*, const char *description);
nsMsgFilterError MSG_GetFilterScript(nsMsgFilter *, char **name);
nsMsgFilterError MSG_SetFilterScript(nsMsgFilter *, const char *name);

nsMsgFilterError MSG_RuleAddTerm(MSG_Rule *,     
	MSG_SearchAttribute attrib,    /* attribute for this term                */
    MSG_SearchOperator op,         /* operator e.g. opContains               */
    MSG_SearchValue *value,        /* value e.g. "Dogbert"                   */
	XP_Bool BooleanAND, 	       /* TRUE if AND is the boolean operator. FALSE if OR is the boolean operators */
	char * arbitraryHeader);       /* arbitrary header specified by user. ignored unless attrib = attribOtherHeader */

nsMsgFilterError MSG_RuleGetNumTerms(MSG_Rule *, int32 *numTerms);

nsMsgFilterError MSG_RuleGetTerm(MSG_Rule *, int32 termIndex, 
	MSG_SearchAttribute *attrib,    /* attribute for this term                */
    MSG_SearchOperator *op,         /* operator e.g. opContains               */
    MSG_SearchValue *value,         /* value e.g. "Dogbert"                   */
	XP_Bool *BooleanAnd,				/* TRUE if AND is the boolean operator. FALSE if OR is the boolean operator */
	char ** arbitraryHeader);        /* arbitrary header specified by user. ignore unless attrib = attribOtherHeader */

nsMsgFilterError MSG_RuleSetScope(MSG_Rule *, MSG_ScopeTerm *scope);
nsMsgFilterError MSG_RuleGetScope(MSG_Rule *, MSG_ScopeTerm **scope);

/* if type is acChangePriority, value is a pointer to priority.
   If type is acMoveToFolder, value is pointer to folder name.
   Otherwise, value is ignored.
*/
nsMsgFilterError MSG_RuleSetAction(MSG_Rule *, MSG_RuleActionType type, void *value);
nsMsgFilterError MSG_RuleGetAction(MSG_Rule *, MSG_RuleActionType *type, void **value);

/* help FEs manage menu choices in Filter dialog box */

/* Use this to help build menus in the filter dialogs. See APIs below */
typedef struct MSG_RuleMenuItem 
{
	int16 attrib;
	char name[32];
} MSG_RuleMenuItem;


nsMsgFilterError MSG_GetRuleActionMenuItems (
	nsMsgFilterType type,		/* type of filter                          */
    MSG_RuleMenuItem *items,    /* array of caller-allocated structs       */
    uint16 *maxItems);            /* in- max array size; out- num returned   */ 

nsMsgFilterError MSG_GetFilterWidgetForAction( MSG_RuleActionType action,
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
typedef void (*JSFilterCallback)(void* arg, nsMsgFilterIndex filter_index, XP_Bool filter_changed);

void MSG_EditJSFilter(MWContext *context, nsMsgFilterList *filter_list,
					  nsMsgFilterIndex filter_index,
					  JSFilterCallback cb, void *arg);
void MSG_NewJSFilter(MWContext *context, nsMsgFilterList *filter_list,
					 nsMsgFilterType filter_type, JSFilterCallback cb, void *arg);


#endif  // _nsMsgFilterService_H_

