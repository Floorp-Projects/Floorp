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
/* 
   extern.h -- our C api -- things that get called from one directory up, and possibly from Frame.cpp.
   Created: Chris Toshok <toshok@netscape.com>, 12-Feb-97
 */



#ifndef _xfe_extern_h
#define _xfe_extern_h

#include "Frame.h"
#include "Xm/Xm.h"
#include "xp_core.h"
#include "structs.h"
#include "net.h"
#ifdef EDITOR
#include "xeditor.h"
#endif

XP_BEGIN_PROTOS

/* addressbook. */
extern MWContext* fe_showAddrBook(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec);

/* bookmarks */
extern MWContext* fe_showBookmarks(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec);
extern MWContext* fe_getBookmarkContext();
extern void fe_createBookmarks(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec);
extern char *fe_getDefaultBookmarks(void);

/* history */
extern MWContext* fe_showHistory(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec);
extern void fe_createHistory(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec);


/* browser */
extern MWContext *fe_showBrowser(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec, URL_Struct *url);
extern MWContext *fe_reuseBrowser(MWContext *context, URL_Struct *url);

/* compose */

/* Note: (XP_Bool defaultAddressIsNewsgroup flag is used to force empty
 * compose address table to be "Newsgroup:" instead of "To:. Only known
 * user of this flag is XFE_TaskBarDrop::openComposeWindow())
 */
extern MSG_Pane *fe_showCompose(Widget, Chrome *chromespec, MWContext*, MSG_CompositionFields*, const char *, XP_Bool preferToUseHtml, XP_Bool defaultAddressIsNewsgroup);

/* conference */
extern void fe_showConference(Widget w, char *email, short use, char *coolAddr);

/* calendar */
extern void fe_showCalendar(Widget w);

/* host on demand */
extern void fe_showHostOnDemand();

/* dialogs */
extern MWContext *fe_showHTMLDialog(Widget parent, XFE_Frame *parent_frame,
                                    Chrome *chromespec);
extern void fe_attach_field_to_labels (Widget value_field, ...);

/* download (save to disk) */
extern MWContext *fe_showDownloadWindow(Widget toplevel, XFE_Frame *parent_frame);

/* editor */
extern MWContext* fe_showEditor(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec, URL_Struct *url);

/* folders */
extern MWContext* fe_showFolders(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec);
extern MWContext* fe_showNewsgroups(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec);
extern MSG_Master *fe_getMNMaster(void);

/* ldap search */
extern MWContext* fe_showLdapSearch(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec);

/* mail search */
extern MWContext* fe_showSearch(Widget, XFE_Frame *parent_frame, Chrome *chromespec);

/* mail filters */
extern MWContext* fe_showMailFilters(Widget toplevel);

/* mail messages */
extern MWContext* fe_showMsg(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec, MSG_FolderInfo *folder_info, MessageKey msg_key, XP_Bool with_reuse);

/* mail threads */
extern MWContext* fe_showInbox(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec, XP_Bool with_reuse, XP_Bool getNewMail);
extern MWContext* fe_showMessages(Widget toplevel, XFE_Frame *parent_frame, Chrome *chromespec, MSG_FolderInfo *info, XP_Bool with_reuse, XP_Bool getNewMail, MessageKey key);

#ifdef MOZ_TASKBAR
/* task bar */
extern void fe_showTaskBar(Widget toplevel);
#endif

/* generic window creation. */
MWContext *
xfe2_MakeNewWindow(Widget toplevel, MWContext *context_to_copy,
				   URL_Struct *url, char *window_name, MWContextType type,
				   Boolean skip_get_url, Chrome *decor);

/* help menu */
extern void fe_about_cb (Widget widget, XtPointer closure, XtPointer call_data);
extern void fe_manual_cb (Widget widget, XtPointer closure, XtPointer call_data);

extern void XFE_SetDocTitle(MWContext *context, char *title);

extern void fe_openTargetUrl(MWContext * context,LO_AnchorData * anchor_data);

XP_END_PROTOS

#endif /* _xfe_extern_h */
