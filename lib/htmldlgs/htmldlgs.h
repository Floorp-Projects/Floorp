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

#ifndef _HTMLDLGS_H_
#define _HTMLDLGS_H_

/*
 * Header for cross platform html dialogs
 *
 *
 */

#include "ntypes.h"
#ifndef NSPR20
#include "prarena.h"
#else
#include "plarena.h"
#endif

XP_BEGIN_PROTOS

/*
 * dialog button flags
 */
#define XP_DIALOG_CANCEL_BUTTON		(1<<0)
#define XP_DIALOG_CONTINUE_BUTTON	(1<<1)
#define XP_DIALOG_OK_BUTTON		(1<<2)
#define XP_DIALOG_NEXT_BUTTON		(1<<3)
#define XP_DIALOG_BACK_BUTTON		(1<<4)
#define XP_DIALOG_FINISHED_BUTTON	(1<<5)
#define XP_DIALOG_FETCH_BUTTON		(1<<6)
#define XP_DIALOG_MOREINFO_BUTTON	(1<<7)

/* chunk size for string arenas */
#define XP_STRINGS_CHUNKSIZE 512

/*
 * structured types
 */
typedef struct _XPDialogState XPDialogState;
typedef struct _XPDialogInfo XPDialogInfo;
typedef struct _XPPanelDesc XPPanelDesc;
typedef struct _XPPanelInfo XPPanelInfo;
typedef struct _XPPanelState XPPanelState;
typedef struct _XPDialogStrings XPDialogStrings;
typedef struct _XPDialogStringEntry XPDialogStringEntry;

typedef PRBool (* XP_HTMLDialogHandler)(XPDialogState *state, char **argv,
					int argc, unsigned int button);

typedef int (* XP_HTMLPanelHandler)(XPPanelState *state, char **argv,
				    int argc, unsigned int button);

typedef XPDialogStrings * (* XP_HTMLPanelContent)(XPPanelState *state);

typedef void (* XP_PanelFinish)(XPPanelState *state, PRBool cancel);

struct _XPDialogState {
    PRArenaPool *arena;
    void *window;
    void *proto_win;
    XPDialogInfo *dialogInfo;
    void *arg;
    void (* deleteCallback)(void *arg);
    void *cbarg;
    PRBool deleted;
};

struct _XPDialogInfo {
    unsigned int buttonFlags;
    XP_HTMLDialogHandler handler;
    int width;
    int height;
};

#define XP_PANEL_FLAG_FINAL	(1<<0)
#define XP_PANEL_FLAG_FIRST	(1<<1)
#define XP_PANEL_FLAG_ONLY	(1<<2)

struct _XPPanelDesc {
    XP_HTMLPanelHandler handler;
    XP_HTMLPanelContent content;
    unsigned int flags;
};

struct _XPPanelInfo {
    XPPanelDesc *desc;
    int panelCount;
    XP_PanelFinish finishfunc;
    int width;
    int height;
};

struct _XPPanelState {
    PRArenaPool *arena;
    void *window;
    XPPanelDesc *panels;
    XPPanelInfo *info;
    int panelCount;
    int curPanel;
    XPDialogStrings *curStrings;
    XP_PanelFinish finish;
    int titlenum;
    void *arg;
    void (* deleteCallback)(void *arg);
    void *cbarg;
    PRBool deleted;
};

struct _XPDialogStrings
{
    PRArenaPool *arena;
    int basestringnum;
    int nargs;
    char **args;
    char *contents;
};

struct _XPDialogStringEntry {
    int tag;
    char **strings;
    int nstrings;
};

extern XPDialogStringEntry dialogStringTable[];

extern XPDialogState *XP_MakeHTMLDialogWithChrome(void *proto_win, 
						  XPDialogInfo *dialogInfo, 
						  int titlenum,
						  XPDialogStrings *strings, 
						  Chrome *chrome, 
						  void *arg, 
						  PRBool utf8CharSet);

extern XPDialogState *XP_MakeHTMLDialog(void *proto_win,
					XPDialogInfo *dialogInfo,
					int titlenum,
					XPDialogStrings *strings,
					void *arg,
					PRBool utf8CharSet);

extern XPDialogState *XP_MakeRawHTMLDialog(void *proto_win,
					   XPDialogInfo *dialogInfo,
					   int titlenum,
					   XPDialogStrings *strings,
					   int handlestring, void *arg);

extern int XP_RedrawRawHTMLDialog(XPDialogState *state,
				  XPDialogStrings *strings,
				  int handlestring);

extern void XP_HandleHTMLDialog(URL_Struct *url);

extern void XP_MakeHTMLPanel(void *proto_win, XPPanelInfo *panelInfo,
			     int titlenum, void *arg);

extern void XP_HandleHTMLPanel(URL_Struct *url);

extern int XP_PutStringsToStream(NET_StreamClass *stream, char **strings);

/*
 * functions for handling html dialog strings
 */
extern XPDialogStrings *XP_GetDialogStrings(int stringtag);

extern void XP_SetDialogString(XPDialogStrings *strings, int stringNum,
				char *string);

extern void XP_CopyDialogString(XPDialogStrings *strings, int stringNum,
				const char *string);

extern void XP_FreeDialogStrings(XPDialogStrings *strings);

/*
 * return the value of a name value pair in an arg list
 *  used by internal form post processing code for html dialogs
 */
extern char *XP_FindValueInArgs(const char *name, char **av, int ac);

extern void XP_MakeHTMLAlert(void *proto_win, char *string);

/*
 * These next two functions are internal routines.  Don't use them.
 * They may change at any time.  They are used to invoke HTML dialogs
 * from a javascript native method in the mocha thread.
 */
void *
xp_MakeHTMLDialogPass1(void *proto_win, XPDialogInfo *dialogInfo);

XPDialogState *
xp_MakeHTMLDialogPass2(void *proto_win, void *cx, XPDialogInfo *dialogInfo,
		       int titlenum, XPDialogStrings *strings,
		       void *arg, PRBool utf8CharSet);

/*
 * destroy an HTML dialog window that has not had anything written to it yet
 */
void
XP_DestroyHTMLDialogWindow(void *window);

XP_END_PROTOS

#endif /* _HTMLDLGS_H_ */
