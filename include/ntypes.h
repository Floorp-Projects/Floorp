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


#ifndef _NetscapeTypes_
#define _NetscapeTypes_

#include "xp_core.h"

/*
	netlib
*/
typedef int FO_Present_Types;
typedef struct URL_Struct_ URL_Struct;
typedef struct _NET_StreamClass NET_StreamClass;


/*
 * libi18n
 */
typedef struct OpaqueCCCDataObject *CCCDataObject;
typedef struct OpaqueINTL_CharSetInfo *INTL_CharSetInfo;

/* How to refill when there's a cache miss */
typedef enum NET_ReloadMethod
{
    NET_DONT_RELOAD,            /* use the cache */
    NET_RESIZE_RELOAD,          /* use the cache -- special for resizing */
    NET_NORMAL_RELOAD,          /* use IMS gets for reload */
    NET_SUPER_RELOAD,           /* retransfer everything */
    NET_CACHE_ONLY_RELOAD       /* Don't do anything if we miss in the cache.
                                   (For the image library) */
} NET_ReloadMethod;

/*
   plugins
*/
typedef struct _NPEmbeddedApp NPEmbeddedApp;

/*
	history
*/
typedef struct _History_entry History_entry;
typedef struct History_ History;

/*
  bookmarks (so shist.h doesn't have to include all of bkmks.h.)
  
  Note, BM_Entry_struct is defined in bkmks.c.  Not good practice
  since this hides dependency info about the struct i.e., if you
  change the struct, clients of the struct in other source files
  will not indirectly recompile.
*/

typedef struct BM_Entry_struct BM_Entry;

/*
	parser
*/
typedef struct _PA_Functions PA_Functions;
typedef struct PA_Tag_struct PA_Tag;

/*
	layout
*/
typedef union LO_Element_struct LO_Element;

typedef struct LO_AnchorData_struct LO_AnchorData;
typedef struct LO_Color_struct LO_Color;
typedef struct LO_TextAttr_struct LO_TextAttr;
typedef struct LO_TextInfo_struct LO_TextInfo;
typedef struct LO_TextStruct_struct LO_TextStruct;
typedef struct LO_ImageAttr_struct LO_ImageAttr;
typedef struct LO_ImageStruct_struct LO_ImageStruct;
typedef struct LO_SubDocStruct_struct LO_SubDocStruct;
typedef struct LO_CommonPluginStruct_struct LO_CommonPluginStruct;
typedef struct LO_EmbedStruct_struct LO_EmbedStruct;
#ifdef SHACK
typedef struct LO_BuiltinStruct_struct LO_BuiltinStruct;
#endif /* SHACK */
typedef struct LO_JavaAppStruct_struct LO_JavaAppStruct;
typedef struct LO_EdgeStruct_struct LO_EdgeStruct;
typedef struct LO_ObjectStruct_struct LO_ObjectStruct;

typedef union LO_FormElementData_struct LO_FormElementData;

typedef struct lo_FormElementOptionData_struct lo_FormElementOptionData;
typedef struct lo_FormElementSelectData_struct lo_FormElementSelectData;
typedef struct lo_FormElementTextData_struct lo_FormElementTextData;
typedef struct lo_FormElementTextareaData_struct lo_FormElementTextareaData;
typedef struct lo_FormElementMinimalData_struct lo_FormElementMinimalData;
typedef struct lo_FormElementToggleData_struct lo_FormElementToggleData;
typedef struct lo_FormElementObjectData_struct lo_FormElementObjectData;
typedef struct lo_FormElementKeygenData_struct lo_FormElementKeygenData;

typedef struct LO_Any_struct LO_Any;
typedef struct LO_FormSubmitData_struct LO_FormSubmitData;
typedef struct LO_FormElementStruct_struct LO_FormElementStruct;
typedef struct LO_LinefeedStruct_struct LO_LinefeedStruct;
typedef struct LO_HorizRuleStruct_struct LO_HorizRuleStruct;
typedef struct LO_BulletStruct_struct LO_BulletStruct;
/* was misspelled as LO_BullettStruct */
#define LO_BullettStruct LO_BulletStruct
typedef struct LO_TableStruct_struct LO_TableStruct;
typedef struct LO_CellStruct_struct LO_CellStruct;
typedef struct LO_Position_struct LO_Position;
typedef struct LO_Selection_struct LO_Selection;
typedef struct LO_HitLineResult_struct LO_HitLineResult;
typedef struct LO_HitElementResult_struct LO_HitElementResult;
typedef union LO_HitResult_struct LO_HitResult;

/* Line style parameter for displaying borders */
typedef enum {
    LO_SOLID,
    LO_DASH,
    LO_BEVEL
} LO_LineStyle;


typedef struct LO_tabFocus_struct LO_TabFocusData;

/*
	XLation
*/
typedef struct PrintInfo_ PrintInfo;
typedef struct PrintSetup_ PrintSetup;

/*
	mother of data structures
*/
typedef struct MWContext_ MWContext;

/*
	Chrome structure
*/
typedef struct _Chrome Chrome;

/*
    Editor
*/
#include "edttypes.h"

typedef enum
{
  MWContextAny = -1,			/* Used as a noopt when searching for a context of a particular type */
  MWContextBrowser,				/* A web browser window */
  MWContextMail,				/* A mail reader window */
  MWContextNews,				/* A news reader window */
  MWContextMailMsg,				/* A window to display a mail msg */
  MWContextNewsMsg,				/* A window to display a news msg */
  MWContextMessageComposition,	/* A news-or-mail message editing window */
  MWContextSaveToDisk,			/* The placeholder window for a download */
  MWContextText,				/* non-window context for text conversion */
  MWContextPostScript,			/* non-window context for PS conversion */
  MWContextBiff,				/* non-window context for background mail
								   notification */
  MWContextJava,				/* non-window context for Java */
  MWContextBookmarks,			/* Context for the bookmarks */
  MWContextAddressBook,			/* Context for the addressbook */
  MWContextOleNetwork,			/* non-window context for the OLE network1 object */
  MWContextPrint,				/* non-window context for printing */
  MWContextDialog,				/* non-browsing dialogs. view-source/security */
  MWContextMetaFile,            /* non-window context for Windows metafile support */
  MWContextEditor,				/* An Editor Window */
  MWContextSearch,				/* a window for modeless search dialog */
  MWContextSearchLdap,			/* a window for modeless LDAP search dialog */
  MWContextHTMLHelp,            /* HTML Help context to load map files */
  MWContextMailFilters,         /* Mail filters context */
  MWContextHistory,             /* A history window */  
  MWContextMailNewsProgress,    /* a progress pane for mail/news URLs */
  MWContextPane,                /* Misc browser pane/window in weird parts of
                                 *  the UI, such as the navigation center */
  MWContextRDFSlave,            /* Slave context for RDF network loads */
  MWContextProgressModule,		/* Progress module (PW_ functions) */
  MWContextIcon                 /* Context for loading images as icons */
} MWContextType;

#define MAIL_NEWS_TYPE(x) ( \
			((x) == MWContextMail)    || \
			((x) == MWContextNews)    || \
			((x) == MWContextMailMsg) || \
			((x) == MWContextNewsMsg) )


struct LJAppletData;

#endif /* _NetscapeTypes_ */

