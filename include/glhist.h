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

#ifndef GLHIST_H
#define GLHIST_H

#include "ntypes.h"

typedef enum gh_SortColumn 
{
   eGH_NoSort = -1,
   eGH_NameSort,
   eGH_LocationSort,
   eGH_FirstDateSort,
   eGH_LastDateSort,
   eGH_VisitCountSort
}gh_SortColumn;

typedef struct _gh_HistEntry 
{
    char *  address;
    time_t  last_accessed;
    time_t  first_accessed;
    int32   iCount;
	int32   iFlags;
    char *  pszName;
}gh_HistEntry;

typedef struct _gh_HistEntryData
{
    time_t  last_accessed;
    time_t  first_accessed;
    int32   iCount;
    int32   iFlags;    
    char *  pszName;
}gh_HistEntryData;

typedef enum gh_FilterOp 
{
   eGH_FOEquals,
   eGH_FOEqualsNot,
   
   eGH_FOGreater,
   eGH_FOGreaterEqual,
   eGH_FOLess,
   eGH_FOLessEqual,
   
   eGH_FOHas,
   eGH_FOHasNot
}gh_FilterOp;

typedef enum gh_FilterLogOp
{
   eGH_FLOAnd,
   eGH_FLOOr
}gh_FilterLogOp;

typedef struct _gh_FilterCondition
{
   enum gh_SortColumn enCol;
   enum gh_FilterOp   enOp;
   
   union
   {
      char *  pszTest;
      int32   iTest;
   } tests;
}gh_FilterCondition;

typedef struct _gh_Filter
{
   int32                         iNumConditions;
   gh_FilterCondition *          pConditions;
   gh_FilterLogOp *              enOps;
}gh_Filter;

typedef struct _gh_NotifyMsg
{
   int32   iNotifyMsg;
   char *  pszKey;
   void *  pUserData;
} gh_NotifyMsg;

typedef void *  GHHANDLE;
typedef void *  GHURHANDLE;

#define GH_NOTIFY_UPDATE   1
#define GH_NOTIFY_DELETE   2

#ifdef XP_WIN
 typedef int (__cdecl *GHISTORY_NOTIFYPROC)( gh_NotifyMsg *pMsg );
#else
 typedef int (*GHISTORY_NOTIFYPROC)( gh_NotifyMsg *pMsg );
#endif

XP_BEGIN_PROTOS

/* if the url was found in the global history then the then number of seconds since
 * the last access is returned.  if the url is not found -1 is returned
 */
extern int GH_CheckGlobalHistory(char * url);

/* add or update the url in the global history
 */
extern void GH_UpdateGlobalHistory(URL_Struct * URL_s);

/* save the global history to a file and remove the list from memory
 */
/*extern void GH_CleanupGlobalHistory(void);*/

/* save the global history to a file and remove the list from memory
 */
extern void GH_SaveGlobalHistory(void);

/* free the global history list
 */
extern void GH_FreeGlobalHistory(void);

/* clear the entire global history list
 */
extern void GH_ClearGlobalHistory(void);

#if defined(XP_MAC) || defined(XP_UNIX)
/* set the maximum time for an object in the Global history in
 * number of seconds
 */
extern void GH_SetGlobalHistoryTimeout(int32 timeout_interval);
#endif

/* start global history tracking
 */
extern void GH_InitGlobalHistory(void);

/* create an HTML stream and push a bunch of HTML about
 * the global history
 *
 * returns -1
 */
extern int NET_DisplayGlobalHistoryInfoAsHTML( MWContext *context, URL_Struct *URL_s, int format_out );

/* 
// Context/Handle based functions to retrieve a pseudo cursor on the
// Global History list (using a specified sort/index).
*/
extern GHHANDLE        GH_GetContext( enum gh_SortColumn   enGHSort, 
                                      gh_Filter *          pFilter, 
                                      GHISTORY_NOTIFYPROC  pfNotifyProc,
                                      GHURHANDLE           hUR,                            
                                      void *               pUserData );
extern void            GH_ReleaseContext( GHHANDLE pContext, Bool bReleaseUR );
extern gh_HistEntry *  GH_GetRecord( GHHANDLE pContext, uint32 uRow );
extern void            GH_DeleteRecord( GHHANDLE pContext, uint32 uRow, Bool bGroup );
extern uint32          GH_GetNumRecords( GHHANDLE pContext );
extern gh_SortColumn   GH_GetSortField( GHHANDLE pContext );
extern int             GH_UpdateURLTitle( URL_Struct *pUrl, char *pszTitle, Bool bFrameCell );
extern int32           GH_GetRecordNum( GHHANDLE pContext, char *pszLocation );
extern int             GH_GetMRUPage( char *pszURL, int iMaxLen );
extern void            GH_FileSaveAsHTML( GHHANDLE pContext, MWContext *pMWContext );
extern GHURHANDLE      GH_GetURContext( GHHANDLE pContext );
extern void            GH_SupportUndoRedo( GHHANDLE pContext );
extern void            GH_Undo( GHHANDLE pContext );
extern void            GH_Redo( GHHANDLE pContext );
extern Bool            GH_CanUndo( GHHANDLE pContext );
extern Bool            GH_CanRedo( GHHANDLE pContext );

/* AutoComplete stuff */
enum autoCompStatus {foundDone, notFoundDone, stillSearching, dontCallOnIdle};
extern enum autoCompStatus urlMatch(const char *criteria, char **result, Bool freshStart, Bool scroll);
extern void NET_RegisterEnableUrlMatchCallback(void);

XP_END_PROTOS

#endif /* GLHIST_H */
