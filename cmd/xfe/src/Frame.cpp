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
   Frame.cpp -- class for toplevel XFE windows.
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
 */


#ifndef NO_SECURITY
#include "ssl.h"
#endif

#include "rosetta.h"
#include "Frame.h"

#include "layers.h"

#ifdef JAVA
#include "mozjava.h"
#endif

#include "libmocha.h"
#include "libevent.h"

#include <libi18n.h>		/* for the document encoding stuff. */
#include <intl_csi.h>		/* to get/set doc_csid/win_csid */
#include "libimg.h"             /* Image Library public API. */
#include "il_util.h"            /* Colormap/colorspace utilities. */

#include "DisplayFactory.h"
#include "MozillaApp.h"
#include "xfe2_extern.h"
#include "View.h"
#include "MenuBar.h"
#include "BookmarkMenu.h"
#include "BrowserFrame.h"
#include "HistoryFrame.h"
#include "NavCenterFrame.h"
#include "FrameListMenu.h"
#include "Minibuffer.h"
#include "Toolbox.h"
#include "Dashboard.h"
#include "Logo.h"
#include "Netcaster.h"
#include "ViewGlue.h"
#ifdef MOZ_MAIL_NEWS
#include "MNView.h" /* for MNView::getBiffState() */
#endif
#include "xpassert.h"
#include "xpgetstr.h"
#include "prefapi.h"
#include "e_kit.h"

#include <X11/IntrinsicP.h>
#include <X11/ShellP.h>

#include <Xm/Protocols.h>
#include <Xfe/Chrome.h>
#include <Xfe/FrameShell.h>
#include <Xfe/ToolBox.h>
#include <Xm/MwmUtil.h>      // For MWM_DECOR_XXX

#include "XmL/Grid.h"

#include <Xfe/Xfe.h>

#ifdef EDITOR
#include "xeditor.h"
#endif /*EDITOR*/

#ifndef __sgi
#include <X11/Xmu/Editres.h>	/* For editres to work on anything but Irix */
#endif

// For mozilla wm extensions
#include "MozillaWm.h"

#ifdef MOZILLA_GPROF
#include "gmon.h"
#endif /*MOZILLA_GPROF*/

#define MM_PER_INCH      (25.4)
#define POINTS_PER_INCH  (72.0)

#if DEBUG_toshok
#define D(x) x
#else
#define D(x)
#endif

extern char *fe_calendar_path;
extern char *fe_host_on_demand_path;

// baggage
extern "C" {
	MWContext* fe_WidgetToMWContext(Widget);
	void fe_ProtectContext(MWContext *context);
	void fe_UnProtectContext(MWContext *context);
	XP_Bool fe_IsContextDestroyed(MWContext *context);
	void fe_fixFocusAndGrab(MWContext *context);
	void fe_load_default_font (MWContext *context);
	void fe_find_scrollbar_sizes (MWContext *context);
	void fe_get_final_context_resources (MWContext *context);
	Colormap fe_getColormap(fe_colormap *colormap);
	void fe_cleanup_tooltips(MWContext *context);
#ifdef JAVA
	void fe_new_show_java_console_cb (Widget widget, XtPointer closure,
									  XtPointer call_data);
#endif /* JAVA */
	void fe_NetscapeCallback(Widget, XtPointer, XtPointer);

	XP_Bool fe_IsCalendarInstalled();
    XP_Bool fe_IsHostOnDemandInstalled();
	XP_Bool fe_IsPolarisInstalled();
	XP_Bool fe_IsConferenceInstalled();
	URL_Struct *fe_GetBrowserStartupUrlStruct();
}

extern MWContext *last_documented_xref_context;
extern LO_Element *last_documented_xref;
extern LO_AnchorData *last_documented_anchor_data;


// - Shared Menu Spec - These specs are defined here because they are the same
// menu spec(definition) for each different frame type

MenuSpec XFE_Frame::new_menu_spec[] = {
  { xfeCmdOpenBrowser,		PUSHBUTTON },
  { xfeCmdComposeMessage,	PUSHBUTTON },
#ifdef EDITOR
  MENU_SEPARATOR,
  MENU_PUSHBUTTON(xfeCmdNewBlank),
  MENU_PUSHBUTTON(xfeCmdNewTemplate),
  MENU_PUSHBUTTON(xfeCmdNewWizard),
#endif
  { NULL }
};

MenuSpec XFE_Frame::bookmark_submenu_spec[] = {
	{ xfeCmdAddBookmark,		PUSHBUTTON },
	{ "fileBookmarksSubmenu",     DYNA_FANCY_CASCADEBUTTON, NULL, NULL, False, (void*)True, XFE_BookmarkMenu::generate },
	{ xfeCmdOpenBookmarks,	PUSHBUTTON },
	MENU_SEPARATOR,
	{ "placesSubmenu",            CASCADEBUTTON, XFE_Frame::places_menu_spec},
	MENU_SEPARATOR,
	{ "bookmarkPlaceHolder",	DYNA_MENUITEMS, NULL, NULL, False, (void*)False, XFE_BookmarkMenu::generate },
	{ NULL }
};

MenuSpec XFE_Frame::tools_submenu_spec[] = {
	{ xfeCmdOpenHistory,		PUSHBUTTON },
	HG27632
#ifndef MOZ_LITE
	{ xfeCmdOpenFolders,			PUSHBUTTON },
#endif
	{ xfeCmdJavaConsole,		PUSHBUTTON },
	{ NULL }
};

MenuSpec XFE_Frame::servertools_submenu_spec[] = {
	{ xfeCmdPageServices,		PUSHBUTTON },
#ifndef MOZ_LITE
        { xfeCmdEditConfiguration,      PUSHBUTTON }, // Mail Account
        { xfeCmdManageMailingList,      PUSHBUTTON }, // Mail Account
        { xfeCmdManagePublicFolders,      PUSHBUTTON },
	{ xfeCmdModerateDiscussion,     PUSHBUTTON }, // Newsgroup
#endif
	{ NULL }
};

MenuSpec XFE_Frame::window_menu_spec[] = {
    { xfeCmdOpenNavCenter,  PUSHBUTTON },
	{ xfeCmdOpenOrBringUpBrowser,	PUSHBUTTON },
#ifdef MOZ_MAIL_NEWS
	{ xfeCmdOpenInbox,		PUSHBUTTON },
	{ xfeCmdOpenNewsgroups,	PUSHBUTTON },
#endif
#ifdef EDITOR
	{ xfeCmdOpenEditor,		PUSHBUTTON },
#endif
#ifdef MOZ_MAIL_NEWS
	{ xfeCmdOpenConference,       PUSHBUTTON },
	{ xfeCmdOpenCalendar,         PUSHBUTTON },
	{ xfeCmdOpenHostOnDemand,     PUSHBUTTON },
#endif
#ifdef MOZ_NETCAST
	{ xfeCmdOpenNetcaster,        PUSHBUTTON },
#endif
#ifdef MOZ_TASKBAR
	MENU_SEPARATOR,
	{ xfeCmdToggleTaskbarShowing,	PUSHBUTTON },
#endif
#ifdef MOZ_MAIL_NEWS
	{ xfeCmdOpenFolders,			PUSHBUTTON },
	{ xfeCmdOpenAddressBook,		PUSHBUTTON },
#endif
	{ "bookmarksSubmenu",	CASCADEBUTTON, bookmark_submenu_spec },
	{ xfeCmdOpenHistory,		PUSHBUTTON },
#ifdef JAVA
	{ xfeCmdJavaConsole,		PUSHBUTTON },
#endif
	HG87782
	MENU_SEPARATOR,
	{ "toolsSubmenu",	CASCADEBUTTON, tools_submenu_spec },
	{ "serverToolsSubmenu",	CASCADEBUTTON, servertools_submenu_spec },
	MENU_SEPARATOR,
 	{ "frameListPlaceHolder",	DYNA_MENUITEMS, NULL, NULL, False, NULL, XFE_FrameListMenu::generate },
	{ NULL }
};

// Shared between Mail folder/thread/msg
// Is there is a reason why this is here? It's the same as new_menu_spec
MenuSpec XFE_Frame::new_submenu_spec[] = {
  { xfeCmdOpenBrowser,		PUSHBUTTON },
#ifdef MOZ_MAIL_NEWS
  { xfeCmdComposeMessage,	PUSHBUTTON },
#endif
#ifdef EDITOR
  MENU_SEPARATOR,
  MENU_PUSHBUTTON(xfeCmdNewBlank),
  MENU_PUSHBUTTON(xfeCmdNewTemplate),
  MENU_PUSHBUTTON(xfeCmdNewWizard),
#endif
  { NULL }
};

#ifdef MOZ_MAIL_NEWS
MenuSpec XFE_Frame::newMsg_submenu_spec[] = {
  { xfeCmdGetNewMessages,	PUSHBUTTON },
  { xfeCmdGetNextNNewMsgs,	PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdSynchWithServer,	PUSHBUTTON },
  // Offline stuff is only happening for Windows
  //MENU_SEPARATOR,
  //{ xfeCmdGetSelectedMessagesForOffline,PUSHBUTTON },
  //{ xfeCmdGetFlaggedMessagesForOffline,	PUSHBUTTON },
  //{ xfeCmdChooseMessagesForOffline,	PUSHBUTTON },
  { NULL }
};
#endif  // MOZ_MAIL_NEWS

MenuSpec XFE_Frame::mark_submenu_spec[] = {
	{ xfeCmdMarkMessageUnread,	PUSHBUTTON },
	{ xfeCmdMarkMessageRead,	PUSHBUTTON },
	{ xfeCmdMarkThreadRead,	PUSHBUTTON },
	{ xfeCmdMarkAllMessagesRead,	PUSHBUTTON },
	{ xfeCmdMarkMessageByDate,	PUSHBUTTON },
	{ xfeCmdMarkMessageForLater,	PUSHBUTTON },
	MENU_SEPARATOR,
        { xfeCmdMarkMessage,                    PUSHBUTTON },
        { xfeCmdUnmarkMessage,          PUSHBUTTON },
	{ NULL }
};

MenuSpec XFE_Frame::addrbk_submenu_spec[] = {
	{ xfeCmdAddSenderToAddressBook,	PUSHBUTTON },
	{ xfeCmdAddAllToAddressBook,		PUSHBUTTON },
	//  { xfeCmdAddCardToAddressBook,		PUSHBUTTON } ?
	{ NULL }
};

MenuSpec XFE_Frame::attachments_submenu_spec[] = {
	{ xfeCmdViewAttachmentsInline,	
	  TOGGLEBUTTON, NULL, "ThreadViewAttachmentsGroup" },
	{ xfeCmdViewAttachmentsAsLinks,	
	  TOGGLEBUTTON, NULL, "ThreadViewAttachmentsGroup" },
	{ NULL }
};

MenuSpec XFE_Frame::sort_submenu_spec[] = {
	{ xfeCmdSortByDate,
	  TOGGLEBUTTON, NULL, "ThreadViewSortGroup" },
	{ xfeCmdSortByFlag,
	  TOGGLEBUTTON, NULL, "ThreadViewSortGroup" },
	{ xfeCmdSortByPriority,
	  TOGGLEBUTTON, NULL, "ThreadViewSortGroup" },
	{ xfeCmdSortBySender,
	  TOGGLEBUTTON, NULL, "ThreadViewSortGroup" },
	{ xfeCmdSortBySize,
	  TOGGLEBUTTON, NULL, "ThreadViewSortGroup" },
	{ xfeCmdSortByStatus,
	  TOGGLEBUTTON, NULL, "ThreadViewSortGroup" },
	{ xfeCmdSortBySubject,
	  TOGGLEBUTTON, NULL, "ThreadViewSortGroup" },
	{ xfeCmdSortByThread,
	  TOGGLEBUTTON, NULL, "ThreadViewSortGroup" },
	{ xfeCmdSortByUnread,
	  TOGGLEBUTTON, NULL, "ThreadViewSortGroup" },
	{ xfeCmdSortByMessageNumber,
	  TOGGLEBUTTON, NULL, "ThreadViewSortGroup" },
	MENU_SEPARATOR,
	{ xfeCmdSortAscending,
	  TOGGLEBUTTON, NULL, "ThreadViewSortOrderGroup" },
	{ xfeCmdSortDescending,
	  TOGGLEBUTTON, NULL, "ThreadViewSortOrderGroup" },
	{ NULL }
};

MenuSpec XFE_Frame::threads_submenu_spec[] = {
	{ xfeCmdViewNew,		
	  TOGGLEBUTTON, NULL, "ThreadViewThreadGroup" },
	{ xfeCmdViewThreadsWithNew,	
	  TOGGLEBUTTON, NULL, "ThreadViewThreadGroup" },
	{ xfeCmdViewWatchedThreadsWithNew,	
	  TOGGLEBUTTON, NULL, "ThreadViewThreadGroup" },
	{ xfeCmdViewAllThreads,		
	  TOGGLEBUTTON, NULL, "ThreadViewThreadGroup" },
	MENU_SEPARATOR,
	{ xfeCmdToggleKilledThreads, TOGGLEBUTTON },
	{ NULL }
};

MenuSpec XFE_Frame::headers_submenu_spec[] = {
	{ xfeCmdShowAllHeaders,	
	  TOGGLEBUTTON, NULL, "ThreadViewHeaderGroup" },
	{ xfeCmdShowNormalHeaders,	
	  TOGGLEBUTTON, NULL, "ThreadViewHeaderGroup" },
	{ xfeCmdShowBriefHeaders,	
	  TOGGLEBUTTON, NULL, "ThreadViewHeaderGroup" },
	{ NULL }
};

MenuSpec XFE_Frame::expand_collapse_submenu_spec[] = {
  { xfeCmdExpand,	    PUSHBUTTON },
  { xfeCmdExpandAll,	PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdCollapse,	    PUSHBUTTON },
  { xfeCmdCollapseAll,	PUSHBUTTON },
  { NULL }
};

// subMenu for edit_
MenuSpec XFE_Frame::select_submenu_spec[] = {
	{ xfeCmdSelectThread,			PUSHBUTTON },
	{ xfeCmdSelectFlaggedMessages,	PUSHBUTTON },
//	{ xfeCmdSelectAllMessages,	PUSHBUTTON },
	{ xfeCmdSelectAll,			PUSHBUTTON },
	{ NULL }
};

MenuSpec XFE_Frame::reply_submenu_spec[] = {
	{ xfeCmdReplyToSender,		PUSHBUTTON },
	{ xfeCmdReplyToAll,			PUSHBUTTON },
	{ xfeCmdReplyToNewsgroup,		PUSHBUTTON },
	{ xfeCmdReplyToSenderAndNewsgroup,	PUSHBUTTON },
	{ NULL }
};

MenuSpec XFE_Frame::compose_message_submenu_spec[] = {
	{ xfeCmdComposeMessagePlain,		PUSHBUTTON },
	{ xfeCmdComposeMessageHTML,		PUSHBUTTON },
	{ NULL }
};

MenuSpec XFE_Frame::compose_article_submenu_spec[] = {
	{ xfeCmdComposeArticlePlain,		PUSHBUTTON },
	{ xfeCmdComposeArticleHTML,		PUSHBUTTON },
	{ NULL }
};

MenuSpec XFE_Frame::next_submenu_spec[] = {
	{ xfeCmdNextMessage,			PUSHBUTTON },
	{ xfeCmdNextUnreadMessage,		PUSHBUTTON },
	{ xfeCmdNextFlaggedMessage,		PUSHBUTTON },
	{ xfeCmdNextUnreadThread,		PUSHBUTTON },
	{ xfeCmdNextCollection,		    PUSHBUTTON },
	{ xfeCmdNextUnreadCollection, 	PUSHBUTTON },
	{ NULL }
};

// Encoding Menu Spec - shared between Browsers, and Mail/News
MenuSpec XFE_Frame::encoding_menu_spec[] = {
	{ xfeCmdChangeDocumentEncoding,	TOGGLEBUTTON, NULL, "EncodingRadioGroup", False, (void*)CS_LATIN1 },
	MENU_SEPARATOR,
	{ xfeCmdChangeDocumentEncoding,	TOGGLEBUTTON, NULL, "EncodingRadioGroup", False, (void*)CS_LATIN2 },
	{ xfeCmdChangeDocumentEncoding,	TOGGLEBUTTON, NULL, "EncodingRadioGroup", False, (void*)CS_CP_1250 },
	MENU_SEPARATOR,
	{ xfeCmdChangeDocumentEncoding,	TOGGLEBUTTON, NULL, "EncodingRadioGroup", False, (void*)CS_EUCJP_AUTO },
	{ xfeCmdChangeDocumentEncoding,	TOGGLEBUTTON, NULL, "EncodingRadioGroup", False, (void*)CS_SJIS },
	{ xfeCmdChangeDocumentEncoding,	TOGGLEBUTTON, NULL, "EncodingRadioGroup", False, (void*)CS_EUCJP },
	MENU_SEPARATOR,
	{ xfeCmdChangeDocumentEncoding,	TOGGLEBUTTON, NULL, "EncodingRadioGroup", False, (void*)CS_BIG5 },
	{ xfeCmdChangeDocumentEncoding,	TOGGLEBUTTON, NULL, "EncodingRadioGroup", False, (void*)CS_CNS_8BIT },
	MENU_SEPARATOR,
	{ xfeCmdChangeDocumentEncoding,	TOGGLEBUTTON, NULL, "EncodingRadioGroup", False, (void*)CS_GB_8BIT },
	MENU_SEPARATOR,
	{ xfeCmdChangeDocumentEncoding,	TOGGLEBUTTON, NULL, "EncodingRadioGroup", False, (void*)CS_KSC_8BIT_AUTO },
	MENU_SEPARATOR,
	{ xfeCmdChangeDocumentEncoding,	TOGGLEBUTTON, NULL, "EncodingRadioGroup", False, (void*)CS_8859_5 },
	{ xfeCmdChangeDocumentEncoding,	TOGGLEBUTTON, NULL, "EncodingRadioGroup", False, (void*)CS_KOI8_R },
	{ xfeCmdChangeDocumentEncoding,	TOGGLEBUTTON, NULL, "EncodingRadioGroup", False, (void*)CS_CP_1251 },
	MENU_SEPARATOR,
	{ xfeCmdChangeDocumentEncoding,	TOGGLEBUTTON, NULL, "EncodingRadioGroup", False, (void*)CS_ARMSCII8 },
	MENU_SEPARATOR,
	{ xfeCmdChangeDocumentEncoding,	TOGGLEBUTTON, NULL, "EncodingRadioGroup", False, (void*)CS_8859_7 },
	{ xfeCmdChangeDocumentEncoding,	TOGGLEBUTTON, NULL, "EncodingRadioGroup", False, (void*)CS_CP_1253 },
	MENU_SEPARATOR,
	{ xfeCmdChangeDocumentEncoding,	TOGGLEBUTTON, NULL, "EncodingRadioGroup", False, (void*)CS_8859_9 },
	MENU_SEPARATOR,
	{ xfeCmdChangeDocumentEncoding,	TOGGLEBUTTON, NULL, "EncodingRadioGroup", False, (void*)CS_UTF8 },
	{ xfeCmdChangeDocumentEncoding,	TOGGLEBUTTON, NULL, "EncodingRadioGroup", False, (void*)CS_UTF7 },
	MENU_SEPARATOR,
	{ xfeCmdChangeDocumentEncoding,	TOGGLEBUTTON, NULL, "EncodingRadioGroup", False, (void*)CS_USRDEF2 },
	MENU_SEPARATOR,
	{ xfeCmdSetDefaultDocumentEncoding,	PUSHBUTTON },
	{ NULL }
};

#define STR_GEN(n) "menu.help.item_" # n

MenuSpec XFE_Frame::help_menu_spec[] = {
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(0)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(1)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(2)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(3)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(4)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(5)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(6)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(7)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(8)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(9)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(10)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(11)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(12)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(13)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(14)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(15)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(16)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(17)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(18)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(19)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(20)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(21)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(22)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(23)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(24)},
#ifdef MOZ_COMMUNICATOR_ABOUT
	MENU_PUSHBUTTON(xfeCmdAboutNetscape),
#else
	MENU_PUSHBUTTON(xfeCmdAboutMozilla),
#endif
#ifdef MOZILLA_GPROF
    MENU_SEPARATOR,
    { xfeCmdProfileStatus,     LABEL },
    MENU_SEPARATOR,
    { xfeCmdProfileStart,      PUSHBUTTON },
    { xfeCmdProfileStop,       PUSHBUTTON },
    { xfeCmdProfileReset,      PUSHBUTTON },
    MENU_SEPARATOR,
    { xfeCmdProfileOrder,  TOGGLEBUTTON, NULL, "profileRadio", 0, 0, 0,
	  xfeCmdProfileOrder, { "flatFirst" } },
    { xfeCmdProfileOrder,  TOGGLEBUTTON, NULL, "profileRadio", 0, 0, 0,
	  xfeCmdProfileOrder, { "graphFirst" } },
    MENU_SEPARATOR,
    { xfeCmdProfileSize,  TOGGLEBUTTON, NULL, "profileRadio", 0, 0, 0,
	  xfeCmdProfileSize, { "10" } },
    { xfeCmdProfileSize,  TOGGLEBUTTON, NULL, "profileRadio", 0, 0, 0,
	  xfeCmdProfileSize, { "100" } },
    { xfeCmdProfileSize,  TOGGLEBUTTON, NULL, "profileRadio", 0, 0, 0,
	  xfeCmdProfileSize, { "0" } },
    MENU_SEPARATOR,
    { xfeCmdProfileHtmlReport, PUSHBUTTON },
#endif /*MOZILLA_GPROF*/
	{ NULL }
};

#undef STR_GEN
#define STR_GEN(n) "menu.places.item_" # n

MenuSpec XFE_Frame::places_menu_spec[] = {
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(0)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(1)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(2)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(3)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(4)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(5)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(6)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(7)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(8)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(9)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(10)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(11)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(12)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(13)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(14)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(15)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(16)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(17)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(18)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(19)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(20)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(21)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(22)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(23)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(24)},
	{ NULL }
};

#undef STR_GEN
#define STR_GEN(n) "toolbar.places.item_" # n

MenuSpec XFE_Frame::tb_places_menu_spec[] = {
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(0)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(1)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(2)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(3)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(4)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(5)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(6)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(7)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(8)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(9)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(10)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(11)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(12)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(13)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(14)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(15)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(16)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(17)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(18)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(19)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(20)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(21)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(22)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(23)},
	{xfeCmdOpenCustomUrl, CUSTOMBUTTON, NULL, NULL, False, STR_GEN(24)},
	{ NULL }
};

// - End of Shared MenuSpec

const char *XFE_Frame::beforeDestroyCallback = "XFE_Frame::beforeDestroyCallback";
const char *XFE_Frame::userActivityHere = "XFE_Frame::userActivityHere";
const char *XFE_Frame::encodingChanged = "XFE_Frame::encodingChanged";
const char *XFE_Frame::allConnectionsCompleteCallback = "XFE_Frame::allConnectionsCompleteCallback";

#if !defined(GLUE_COMPO_CONTEXT)
// Progress bar cylon notifications
const char * XFE_Frame::progressBarCylonStart = "XFE_Frame::progressBarCylonStart";
const char * XFE_Frame::progressBarCylonStop = "XFE_Frame::progressBarCylonStop";
const char * XFE_Frame::progressBarCylonTick = "XFE_Frame::progressBarCylonTick";

// Progress bar percentage notifications
const char * XFE_Frame::progressBarUpdatePercent = "XFE_Frame::progressBarUpdatePercent";
const char * XFE_Frame::progressBarUpdateText = "XFE_Frame::progressBarUpdateText";

// Logo animation notifications
const char * XFE_Frame::logoStartAnimation = "XFE_Frame::logoStartAnimation";
const char * XFE_Frame::logoStopAnimation = "XFE_Frame::logoStopAnimation";
#endif /* GLUE_COMPO_CONTEXT */

const char* XFE_Frame::frameBusyCallback = "XFE_Frame::frameBusyCallback";
const char* XFE_Frame::frameNotBusyCallback = "XFE_Frame::frameNotBusyCallback";

// Forward decl:
static void resizeHandler(Widget, XtPointer, XEvent *, Boolean *);

static XFE_CommandList* my_commands;

//
static char myClassName[] = "XFE_Frame::className";

XFE_Command*
XFE_Frame::getCommand(CommandType cmd)
{
	return findCommand(my_commands, cmd);
}

#ifdef MOZILLA_GPROF
//
//    I18N people: please don't bug me about hard coded string in here,
//    it's all for internal profiling use. ...djw 05/25/1997
//
class ProfileStatusCommand : public XFE_FrameCommand
{
public:
	ProfileStatusCommand() : XFE_FrameCommand(xfeCmdProfileStatus) {};

	char*   getLabel(XFE_Frame*, XFE_CommandInfo*) {
		if (gmon_is_running())
			return "Profile monitor running";
		else
			return "Profile monitor stopped";
	}
	void    doCommand(XFE_Frame*, XFE_CommandInfo*) {}
};

class ProfileStartCommand : public XFE_FrameCommand
{
public:
	ProfileStartCommand() : XFE_FrameCommand(xfeCmdProfileStart) {};

	char*   getLabel(XFE_Frame*, XFE_CommandInfo*) {
		if (gmon_is_running())
			return "Restart profile monitor";
		else
			return "Start profile monitor";
	}
	void    doCommand(XFE_Frame*, XFE_CommandInfo*) {
		gmon_start();
	}
};

class ProfileStopCommand : public XFE_FrameCommand
{
public:
	ProfileStopCommand() : XFE_FrameCommand(xfeCmdProfileStop) {};

	XP_Bool isEnabled(XFE_Frame*, XFE_CommandInfo*) {
		return gmon_is_running();
	}
	char*   getLabel(XFE_Frame*, XFE_CommandInfo*) {
		return "Stop profile monitor";
	}
	void    doCommand(XFE_Frame*, XFE_CommandInfo*) {
		gmon_stop();
	}
};

class ProfileResetCommand : public XFE_FrameCommand
{
public:
	ProfileResetCommand() : XFE_FrameCommand(xfeCmdProfileReset) {};

	char*   getLabel(XFE_Frame*, XFE_CommandInfo*) {
		return "Reset profile data";
	}
	void    doCommand(XFE_Frame*, XFE_CommandInfo*) {
		if (gmon_is_running())
			gmon_stop();
		gmon_reset();
	}
};

class ProfileOrderCommand : public XFE_FrameCommand
{
public:
	ProfileOrderCommand() : XFE_FrameCommand(xfeCmdProfileOrder) {};

	XP_Bool isSelected(XFE_Frame*, XFE_CommandInfo* info) {
		unsigned match = False;
		if (info != NULL &&	*info->nparams > 0
			&&
			XP_STRCMP(info->params[0], "flatFirst") == 0) {
			match = True;
		}
		return (gmon_report_get_reverse() == match);
	}
	char*   getLabel(XFE_Frame*, XFE_CommandInfo* info) {
		if (info != NULL &&	*info->nparams > 0
			&&
			XP_STRCMP(info->params[0], "flatFirst") == 0) {
			return "Flat Data then Graph Data";
		} else {
			return "Graph Data then Flat Data";
		}
	}
	void    doCommand(XFE_Frame*, XFE_CommandInfo* info) {
		XP_Bool match = False;
		if (info != NULL &&	*info->nparams > 0
			&&
			XP_STRCMP(info->params[0], "flatFirst") == 0) {
			match = True;
		}
		gmon_report_set_reverse(match);
	}
};

static char profile_size_label_buf[256];

class ProfileSizeCommand : public XFE_FrameCommand
{
public:
	ProfileSizeCommand() : XFE_FrameCommand(xfeCmdProfileSize) {};

	XP_Bool isSelected(XFE_Frame*, XFE_CommandInfo* info) {
		unsigned state = gmon_report_get_size();
		unsigned match = 0;
		if (info != NULL &&	*info->nparams > 0) {
			match = atoi(info->params[0]);
		}

		return (state == match);
	}
	char*   getLabel(XFE_Frame*, XFE_CommandInfo* info) {
		unsigned match = 0;
		if (info != NULL &&	*info->nparams > 0) {
			match = atoi(info->params[0]);
		}

		if (match == 0)
			XP_STRCPY(profile_size_label_buf, "Report All Functions");
		else 
			sprintf(profile_size_label_buf, "Report Top %d Functions", match);

		return profile_size_label_buf;
	}
	void    doCommand(XFE_Frame*, XFE_CommandInfo* info) {
		unsigned match = 0;
		if (info != NULL &&	*info->nparams > 0) {
			match = atoi(info->params[0]);
		}
		gmon_report_set_size(match);
	}
};

static int
gmon_do_html_report(XFE_Frame* frame)
{
	char* gmon_out = NULL;
	char* gprof_out = NULL;
	char* html_out = NULL;
	int rv = 0;
	MWContext* context = frame->getContext();
	Widget     widget = frame->getBaseWidget();

	if (gmon_is_running())
		gmon_stop();

	if ((gmon_out = tempnam(NULL, "gmon")) == NULL) {
		fprintf(stderr, "could not create tmp file: ");
		perror(NULL);
		return -1;
	}

	if ((gprof_out = tempnam(NULL, "gprof")) == NULL) {
		fprintf(stderr, "could not create tmp file: ");
		perror(NULL);
		rv = -1;
		goto return_point;
	}

	if ((html_out = (char*)malloc(strlen(gprof_out) + 6)) == NULL) {
		fprintf(stderr, "out of memory: ");
		perror(NULL);
		rv = -1;
		goto return_point;
	}
	strcpy(html_out, gprof_out);
	strcat(html_out, ".html");

	/*
	 *    Dump gmon.out
	 */
	XFE_Progress(context, "dumping profile monitor data..");
	XmUpdateDisplay(widget);
	gmon_dump_to(gmon_out);

	XFE_Progress(context, "running gprof..");
	XmUpdateDisplay(widget);
	if (gmon_gprof_to(gmon_out, gprof_out, gmon_report_get_size()) == -1) {
		rv = -1;
		goto return_point;
	}
	unlink(gmon_out);

	XFE_Progress(context, "building html output..");
	XmUpdateDisplay(widget);
	if (gmon_html_filter_to(gprof_out, html_out,
							gmon_report_get_reverse()) == -1) {
		rv = -1;
		goto return_point;
	}
	unlink(gprof_out);

	XFE_Progress(context, "loading html report..");
	XmUpdateDisplay(widget);
	FE_GetURL(context, NET_CreateURLStruct(html_out, NET_DONT_RELOAD));

return_point:
	if (gmon_out != NULL) {
		unlink(gmon_out);
		free(gmon_out);
	}
	if (gprof_out != NULL) {
		unlink(gprof_out);
		free(gprof_out);
	}

	if (html_out != NULL) {
		if (rv != 0)
			unlink(html_out);
		free(html_out);
	}

	return rv;
}

class ProfileHtmlReportCommand : public XFE_FrameCommand
{
public:
	ProfileHtmlReportCommand() : XFE_FrameCommand(xfeCmdProfileHtmlReport) {};

	char*   getLabel(XFE_Frame*, XFE_CommandInfo*) {
		return "Make Html Report";
	}
	void    doCommand(XFE_Frame* frame, XFE_CommandInfo*) {

		gmon_do_html_report(frame);
	}
};

#endif /*MOZILLA_GPROF*/

XFE_Frame::XFE_Frame(char *name,
					 Widget toplevel,
					 XFE_Frame *parent_frame,
					 EFrameType frametype,
					 Chrome *chromespec,
					 XP_Bool haveHTMLDisplay,
					 XP_Bool haveMenuBar,
					 XP_Bool haveToolbars,
					 XP_Bool haveDashboard,
					 XP_Bool destroyOnClose) : XFE_Component()
{
        D(      printf ("in XFE_Frame::XFEFrame()\n");)

	// as good a place as any to get the colormap machinery going.
	fe_startDisplayFactory(toplevel);
	
	m_toplevelWidget	= toplevel;
	m_frametype			= frametype;
	m_destroyOnClose	= destroyOnClose;
	m_context			= NULL;
	m_toolbar			= NULL;
	m_toolbox			= NULL;
	m_activeLogo		= NULL;
	m_chrome			= NULL;

#ifdef NETCASTER_ZAXIS_HACKERY
	m_zaxis_BelowHandlerInstalled = False;
	m_zaxis_AboveHandlerInstalled = False;
#endif

	// Remember if a chromespec is provided to the constructor so that we 
	// can later ignore the user's toolbox and geometry preferences.
	m_chromespec_provided = (chromespec != NULL);
	m_first_showing = True;

	m_parentframe = parent_frame;

	if (!chromespec) {
	  //
	  // default values for chromespec-related members
	  //
	  m_allowresize = True;
	  m_hotkeysdisabled = False;
	  m_iswmclosable = True;
	  m_ismodal     = False;
	  m_topmost     = False;
	  m_bottommost  = False;
	  m_zlock       = False;
	  m_lhint		= 0;
	  m_thint		= 0;
	}
	else {
	  m_allowresize = chromespec->allow_resize;
	  m_iswmclosable = chromespec->allow_close;
	  if (chromespec->is_modal && m_parentframe)
		m_ismodal = True;
	  else
		m_ismodal = False;
	  m_topmost     = chromespec->topmost;
	  m_bottommost  = chromespec->bottommost;
	  m_zlock       = chromespec->z_lock;
	  m_hotkeysdisabled = chromespec->disable_commands;

	  D(printf ("lhint = %d, thint = %d\n", chromespec->l_hint, chromespec->t_hint);)
	  m_lhint		= chromespec->l_hint;
	  m_thint		= chromespec->t_hint;

	  D(printf ("Hot keys are %s.\n", m_hotkeysdisabled ? "disabled" : "enabled");)
	}

    geometryPrefName = 0;

	m_haveHTMLDisplay = haveHTMLDisplay;


	createBaseWidgetShell(toplevel,name);

	XP_ASSERT( XfeIsAlive(m_widget) );

	createChromeManager(m_widget,"chrome");

	XP_ASSERT( XfeIsAlive(m_chrome) );

	// XXXX
	// now we do all the funky MWContext stuff.
	initializeMWContext(m_frametype);
	ViewGlue_addMapping(this, m_context);

	// Initialize this after the context
    // This is used by fe_FindNonCustomBrowserContext()
    CONTEXT_DATA(m_context)->hasCustomChrome = m_chromespec_provided;
	if (m_chromespec_provided)
	  m_context->restricted_target=chromespec->restricted_target;

	if (haveDashboard)
	{
		m_dashboard = new XFE_Dashboard(this,m_chrome,this);
		
		if (!chromespec || (chromespec && chromespec->show_bottom_status_bar))
			m_dashboard->show();
		
	}
	else
		m_dashboard = NULL;

	if (chromespec)
		{
			m_chrome_close_callback = chromespec->close_callback;
			m_chrome_close_arg = chromespec->close_arg;
		}
	else
		{
			m_chrome_close_callback = NULL;
		}

	if (haveMenuBar)
		{
			m_menubar = new XFE_MenuBar(this, NULL);
			
			if (!chromespec || (chromespec && chromespec->show_menu))
				m_menubar->show();
		}
	else
		{
			m_menubar = NULL;
		}

	XP_Bool needNavigationToolbar = False;
	XP_Bool needToolbox = False;

	// Determine if we need navigation toolbars
	if (chromespec && chromespec->show_button_bar)
	{
		needNavigationToolbar = True;
	}
	else
	{
		needNavigationToolbar = haveToolbars;
	}

	//
	// show_url_bar and show_directory_buttons determine whether the url bar
	// and personal toolbar are shown.  These components will be created in
	// the XFE_BrowserFrame class.  Since the toolbox is created here
	// we need to take these into account even though (in theory) this class
	// should know nothing about its subclasses...
	//

	// Determine if we need a toolbox
	if (chromespec && (chromespec->show_url_bar || 
					   chromespec->show_directory_buttons))
	{
		needToolbox = True;
	}
	else
	{
		needToolbox = needNavigationToolbar;
	}

	// Create the toolbox if needed
	if (needToolbox)
	{
		// Create the toolbox as a child of the main form
		m_toolbox = new XFE_Toolbox(this,m_chrome);

		// Notify when a toolbox item is snapped in place
		m_toolbox->registerInterest(
			XFE_Toolbox::toolbarSnap,
			this,
			(XFE_FunctionNotification)toolboxSnapNotice_cb);
		
		// Notify when a toolbox item is swapped in place
		m_toolbox->registerInterest(
			XFE_Toolbox::toolbarSwap,
			this,
			(XFE_FunctionNotification)toolboxSwapNotice_cb);

		// Notify when a toolbox item is closed
		m_toolbox->registerInterest(
			XFE_Toolbox::toolbarClose,
			this,
			(XFE_FunctionNotification)toolboxCloseNotice_cb);
		
		// Notify when a toolbox item is opened
		m_toolbox->registerInterest(
			XFE_Toolbox::toolbarOpen,
			this,
			(XFE_FunctionNotification)toolboxOpenNotice_cb);
		
		// Always show the toolbox
		m_toolbox->show();
	}	
	else
	{
		m_toolbox = NULL;
	}

	// Create the navigation toolbox if needed
	if (needNavigationToolbar)
	{
		// Create the toolbar 
		m_toolbar = new XFE_Toolbar(this,m_toolbox,NULL);

		// chromespec takes precedence over prefs
		if (chromespec && chromespec->show_button_bar)
		{
			m_toolbar->show();
		}
	}
	else
	{
		m_toolbar = NULL;
	}

	// If a chromespec is given and all 3 toolbars are off, then we need
	// to unmanage the toolbox.  Otherwise the Frame's expect geometry 
	// will be wrong.
	if (chromespec)
	{
		if (!chromespec->show_button_bar && 
			!chromespec->show_url_bar && 
			!chromespec->show_directory_buttons)
		{
			if (m_toolbox)
				m_toolbox->hide();
		}
	}

	m_aboveview = NULL;
	m_view = NULL;
	m_belowview = NULL;

	if (needNavigationToolbar) 
	{
		m_toolbar->registerInterest(Command::doCommandCallback,
									this,
									doCommandCallback_cb);
		
		// Register the logo animation notifications with ourselves
#if defined(GLUE_COMPO_CONTEXT)
		registerInterest(XFE_Component::logoStartAnimation,
						 this,
						 logoAnimationStartNotice_cb);
		
		registerInterest(XFE_Component::logoStopAnimation,
						 this,
						 logoAnimationStopNotice_cb);
#else
		registerInterest(XFE_Frame::logoStartAnimation,
						 this,
						 logoAnimationStartNotice_cb);
		
		registerInterest(XFE_Frame::logoStopAnimation,
						 this,
						 logoAnimationStopNotice_cb);
#endif /* GLUE_COMPO_CONTEXT */
	}
	
	XFE_MozillaApp::theApp()->registerInterest(XFE_MozillaApp::changeInToplevelFrames,
											   this,
											   (XFE_FunctionNotification)toplevelWindowChangeOccured_cb);

	XFE_MozillaApp::theApp()->registerFrame(this);
	
	/* XXXM12N Create and initialize the Image Library JMC callback
	   interface.  Also create a new IL_GroupContext for this window.*/
	if (!fe_init_image_callbacks(m_context))
		{
			return; // XXXX how do we deal with errors here?
		}
	
	fe_InitColormap (m_context);

	// Reset the delete handler, and add a WM callback
	XtVaSetValues(m_widget,
				  XmNdeleteResponse, XmDO_NOTHING,
				  NULL);

	XmAddWMProtocolCallback(m_widget,
							XmInternAtom(XtDisplay(m_widget), "WM_DELETE_WINDOW", False),
							delete_response, this);

	if (!my_commands) {
#ifdef MOZILLA_GPROF
		registerCommand(my_commands, new ProfileStatusCommand);
		registerCommand(my_commands, new ProfileStartCommand);
		registerCommand(my_commands, new ProfileStopCommand);
		registerCommand(my_commands, new ProfileResetCommand);
		registerCommand(my_commands, new ProfileOrderCommand);
		registerCommand(my_commands, new ProfileSizeCommand);
		registerCommand(my_commands, new ProfileHtmlReportCommand);
#endif /*MOZILLA_GPROF*/
	}

	D(	printf ("leaving XFE_Frame::XFEFrame()\n");)
}

XFE_Frame::~XFE_Frame()
{
	D(	printf ("in XFE_Frame::~XFEFrame()\n");)

	if (m_ismodal 
		&& m_parentframe 
		&& fe_IsContextProtected(m_parentframe->getContext()))
	  {
		fe_UnProtectContext(m_parentframe->getContext());
		if (fe_IsContextDestroyed(m_parentframe->getContext()))
		  {
			D(printf ("The parent frame's context 0x%x was marked deleted,"
					  " so I'm deleting it now.\n",
					  m_parentframe->getContext());)
			 m_parentframe->delete_response();
			 m_parentframe = NULL;
		  }
		else
		  {
			D(printf ("The parent frame's context 0x%x wasn't marked deleted,"
					  " so I'm letting it hang around.\n",
					  m_parentframe->getContext());)
		  }
	  }

	// delete our menubar.
	if (m_menubar)
		{
			delete m_menubar;
			m_menubar = NULL;
		}
	
	// delete the above view area.
	if (m_aboveview)
		{
			delete m_aboveview;
			m_aboveview = NULL;
		}
	
	// delete the below view area.
	if (m_belowview)
		{
			delete m_belowview;
			m_belowview = NULL;
		}
	
	// delete the view itself.
	if (m_view)
		{
			delete m_view;
			
			m_view = NULL;
		}
	
	if (m_context)
		{
			PRBool observer_removed_p;

			if (m_context->color_space) {
				IL_ReleaseColorSpace(m_context->color_space);
                m_context->color_space = NULL;
            }

            /* Destroy the image group context after removing the image
               group observer. */
            observer_removed_p =
                IL_RemoveGroupObserver(m_context->img_cx,
                                       fe_ImageGroupObserver,
                                       (void *)m_context);

			IL_DestroyGroupContext(m_context->img_cx);
			m_context->img_cx = NULL;
			
			if (m_haveHTMLDisplay)
				SHIST_EndSession (m_context);
			
			/* Destroy the compositor associated with the context. */
			if (m_context->compositor)
				{
					CL_DestroyCompositor(m_context->compositor);
					m_context->compositor = NULL;
				}
			
#ifdef MOZ_MAIL_NEWS
			MimeDestroyContextData(m_context);
#endif

			if (m_context->title) free (m_context->title);
			m_context->title = 0;


			free (CONTEXT_DATA (m_context));
			free (m_context);
		}

	//
	//    This must be called last, as it may start the exit sequence,
	//    and never return.
	//
	XFE_MozillaApp::theApp()->unregisterFrame(this);
}

const char* 
XFE_Frame::getClassName()
{
	return myClassName;
}
//////////////////////////////////////////////////////////////////////////
//
// This method creates the toplevel shell of this frame.  It handles
// modality here as well.  So, as an offshoot, we can now support
// any type of modal window, not just dialogs.
//
//////////////////////////////////////////////////////////////////////////
void
XFE_Frame::createBaseWidgetShell(Widget parent,String name)
{
	XP_ASSERT( XfeIsAlive(parent) );
	XP_ASSERT( name != NULL );

	Arg			av[20];
	Cardinal	ac = 0;
	Widget		shell = NULL;

	XtSetArg(av[ac], XmNvisual, XFE_DisplayFactory::theFactory()->getVisual()); ac++;
	XtSetArg(av[ac], XmNdepth, XFE_DisplayFactory::theFactory()->getVisualDepth()); ac++;
	
	if (m_haveHTMLDisplay)
	{
		/* we only need our own colormap if we're displaying html */
		m_cmap = XFE_DisplayFactory::theFactory()->getPrivateColormap();
		
		XtSetArg (av[ac], XmNcolormap, fe_getColormap(m_cmap)); ac++;
	}
	else
	{
		m_cmap = XFE_DisplayFactory::theFactory()->getSharedColormap();
		
		XtSetArg (av[ac], XmNcolormap, fe_getColormap(m_cmap)); ac++;
	}
	
	XtSetArg (av[ac], XmNallowShellResize, False); ac++;

    // I think we should deal with modality in XfeFrameShell...that
    // would simplify XFE_Frame a lot. -re
	if (m_ismodal && m_parentframe)
	{
		XtSetArg (av[ac], XmNdialogType, XmDIALOG_PROMPT); ac++;
		XtSetArg (av[ac], XmNautoUnmanage, False); ac++;
		XtSetArg (av[ac], XmNborderWidth, 0); ac++;
		
		XtSetArg (av[ac], XmNautoUnmanage, False); ac++;
		XtSetArg (av[ac], XmNmarginWidth, 0); ac++;
		XtSetArg (av[ac], XmNmarginHeight, 0); ac++;
		XtSetArg (av[ac], XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL); ac++;
		
		XtSetArg (av[ac], XmNnoResize, !m_allowresize); ac++;
		XtSetArg (av[ac], XmNresizePolicy, XmRESIZE_ANY); ac++;
		
		shell = XmCreateDialogShell(m_parentframe->getDialogParent(),
									name,av,ac);
			
		MWContext *fooContext = fe_WidgetToMWContext(m_parentframe->getDialogParent());
		
		if (fooContext && ViewGlue_getFrame(fooContext) != NULL)
		{
			m_parentframe = ViewGlue_getFrame(fooContext);
			
			fe_ProtectContext(m_parentframe->getContext());
		}
	}
	else
	{
		m_ismodal = False;

		// Warning Warning!!

		// This hackery must die.  It will go away as soon as the 
		// XfeFrameShell widget supports all the XmNiconic magic
		// and friends.
		//
		// -ramiro
		
		// Make sure the iconic state is set before the shell gets 
		// created or else it does not work
		if (!fe_GetCommandLineDone() && 
			fe_globalData.startup_iconic &&
			!m_chromespec_provided)
		{
			XtSetArg (av[ac], XmNinitialState, IconicState); ac++;
			XtSetArg (av[ac], XmNiconic, True); ac++;
		}
		
		shell = XfeCreateFrameShell(parent,name,av,ac);
		
		// This hackery must die too.  It will go away as soon as the 
		// XfeFrameShell widget supports XmNresizeCallback and
		// XmNmoveCallback.
		XtAddEventHandler(shell, StructureNotifyMask,
						  True, (XtEventHandler)resizeHandler,
						  (XtPointer)this);
		
		if (!m_allowresize)
		{
			XtVaSetValues(shell,
						  XmNmwmFunctions,
						  MWM_FUNC_ALL|MWM_FUNC_RESIZE|MWM_FUNC_MAXIMIZE,
						  NULL);
		}
	}

	// This hackery must die too.  It will go away as soon as the 
	// XfeFrameShell widget supports XmNtrackEditres
#ifndef __sgi
	XtAddEventHandler(shell, (EventMask)0, True,
					  (XtEventHandler)_XEditResCheckMessages, 0);
#endif

	XP_ASSERT( XfeIsAlive(shell) );

	setBaseWidget(shell);
	installDestroyHandler();
}
//////////////////////////////////////////////////////////////////////////
//
// This method creates the chrome manager.  The chrome manager 
// takes care of placing all the chrome elements.
//
//////////////////////////////////////////////////////////////////////////
void
XFE_Frame::createChromeManager(Widget parent,String name)
{
	XP_ASSERT( XfeIsAlive(parent) );
	XP_ASSERT( name != NULL );

	XP_ASSERT( m_chrome == NULL );

	m_chrome = XtVaCreateWidget(name,
								xfeChromeWidgetClass,
								m_widget, 
								XmNusePreferredWidth,	False,
								XmNusePreferredHeight,	False,
								NULL);

	// Manage the chrome widget if we are not modal.
	if (!m_ismodal)
	{
		XtManageChild(m_chrome);
	}
}
//////////////////////////////////////////////////////////////////////////
static void resizeHandler(Widget, XtPointer p, XEvent *event, Boolean *)
{
    if (event->type == ConfigureNotify && p != 0)
    {
        XFE_Frame* frame = (XFE_Frame*)p;

		frame->frameResizeHandler(event->xconfigure.width,
								  event->xconfigure.height);
    }
}

void
XFE_Frame::frameResizeHandler(int width, int height)
{        
	//
	// The forcing and saving of prefs should only happen if a chromespec
	// was not specified in the constructor. -re
	//
	if (m_chromespec_provided)
	{
		return;
	}

	//
	// Sometimes the user might not want the client too save the current
	// frame geometry
	//
	if (fe_globalData.dont_save_geom_prefs)
	{
		return;
	}

    if (geometryPrefName != NULL)
    {
        PREF_SetIntPref(getWidthPrefString(), (int32)width);
        PREF_SetIntPref(getHeightPrefString(), (int32)height);
    }
}

// If you want to add special behavior to your frame when the user tries to
// close it, override this function.  It should return TRUE if the window
// should actually close, and FALSE if it should remain on the screen.  This
// means that any dialog that is popped up to prompt the user whether they
// want to save a file, send a message, etc, needs to be done synchronously,
// but you already knew that.
XP_Bool
XFE_Frame::isOkToClose()
{
	return TRUE;
}

static void
fe_focus_notify_eh (Widget /*w*/, XtPointer closure, XEvent *ev, Boolean * /*cont*/)
{
  MWContext *context = (MWContext *) closure;
  JSEvent *event;

  TRACEMSG (("fe_focus_notify_eh\n"));
  switch (ev->type) {
  case FocusIn:
    TRACEMSG (("focus in\n"));
      event = XP_NEW_ZAP(JSEvent);
      event->type = EVENT_FOCUS;
    ET_SendEvent (context, NULL, event, NULL, NULL);
    break;
  case FocusOut:
    TRACEMSG (("focus out\n"));
      event = XP_NEW_ZAP(JSEvent);
      event->type = EVENT_BLUR;
    ET_SendEvent (context, NULL, event, NULL, NULL);
    break;
  }
}

#ifdef NOBODY_IS_USING_THIS_AND_IAM_SICK_OF_SEEING_A_WARNING_MSG_ABOUT_IT
static void
fe_map_notify_eh (Widget /*w*/, XtPointer closure, XEvent *ev, Boolean * /*cont*/)
{
#ifdef JAVA
    MWContext *context = (MWContext *) closure;
    switch (ev->type) {
    case MapNotify:
	LJ_UniconifyApplets(context);
	break;
    case UnmapNotify:
	LJ_IconifyApplets(context);
	break;
    }
#endif /* JAVA */
}
#endif

void
XFE_Frame::doClose()
{
    XP_InterruptContext (m_context);
	
    if (m_chrome_close_callback)
    {
	(*m_chrome_close_callback)(m_chrome_close_arg);
    }

    if (m_destroyOnClose)
    {
	hide();
	// here we remove the context from all our lists, and
	// then voice our contempt with the current situation
	// by telling mocha we have no interest in this context
	// any longer.  We didn't like it anyway.  It was broken,
	// and stupid.  but then, we end up doing _more_ things
	// with the context after mocha says it doesn't want
	// it anymore.  amazing what a sad existence these
	// MWContexts lead... destined to be scorned not once,
	// but twice before destruction...

	// do this before notifying about beforeDestroyCallback,
		XFE_MozillaApp::theApp()->unregisterInterest(
			XFE_MozillaApp::changeInToplevelFrames, this,
			(XFE_FunctionNotification)toplevelWindowChangeOccured_cb);
					
		XFE_MozillaApp::theApp()->unregisterInterest(XFE_MozillaApp::appBusyCallback,
							 this,
							 (XFE_FunctionNotification)updateBusyState_cb,
							 (void*)True);

		XFE_MozillaApp::theApp()->unregisterInterest(XFE_MozillaApp::appNotBusyCallback,
							 this,
							 (XFE_FunctionNotification)updateBusyState_cb,
							 (void*)False);
		
		unregisterInterest(frameBusyCallback,
						   this,
						   (XFE_FunctionNotification)updateBusyState_cb,
						   (void*)True);

		unregisterInterest(frameNotBusyCallback,
						   this,
						   (XFE_FunctionNotification)updateBusyState_cb,
						   (void*)False);

	notifyInterested(XFE_Frame::beforeDestroyCallback);

	if (m_context)
	{
		fe_ContextData* d = CONTEXT_DATA(m_context);
		Widget w = CONTEXT_WIDGET (m_context);

		/* Fix for bug #29631 */
		if (m_context == last_documented_xref_context)
		{
			last_documented_xref_context = 0;
			last_documented_xref = 0;
			last_documented_anchor_data = 0;
		}
		/* This is a hack. If the mailcompose window is going away and 
		 * a tooltip was still there (or) was armed (a timer was set for it)
		 * for a widget in the mailcompose window, then the destroying
		 * mailcompose context would cause a core dump when the tooltip
		 * timer hits. This could happen to a javascript window with toolbars
		 * too if it is being closed on a timer.
		 *
		 * In this critical time of 3.x ship, we are fixing this by
		 * always killing any tooltip that was present anywhere and
		 * remove the timer for any armed tooltop anywhere in the
		 * navigator when any context is going away.
		 *----
		 * The proper thing to do would be
		 *  - to check if the fe_tool_tips_widget is a child of the
		 *    CONTEXT_WIDGET(context) and if it is, then cleanup the tooltip.
		 */
		fe_cleanup_tooltips(m_context);
							
	 	if (m_context->is_grid_cell)
			CONTEXT_DATA (m_context)->being_destroyed = True;
#ifdef notyet
		if (context->type == MWContextSaveToDisk) {
			/* We might be in an extend text selection on the text widgets.
			   If we destroy this now, we will dump core. So before destroying
			   we will disable extend of the selection. */
			XtCallActionProc(CONTEXT_DATA (context)->url_label, "extend-end",
				NULL, NULL, 0);
			XtCallActionProc(CONTEXT_DATA (context)->url_text, "extend-end",
				NULL, NULL, 0);
		}
#endif
		if (m_context->is_grid_cell)
			w = d->main_pane;
							
		if (d->refresh_url_timer)
			XtRemoveTimeOut (d->refresh_url_timer);
		if (d->refresh_url_timer_url)
			free (d->refresh_url_timer_url);
		fe_DisposeColormap(m_context);
							
							
#ifdef EDITOR
		/*
		 *  NOTE:  is_editor is set for both PageCompose & MailCompose...
		 */
        /* We have to call fe_EditorCleanup() before calling
         * fe_DestroyLayoutData(), because the latter routine zeros
         * the lo_topState, which prevents EDT_DestroyEditBuffer
         * from actually deleting memory or calling any destructors,
         * which leads to memory leaks and crashes.
         */
		if (EDT_IS_EDITOR(m_context))
		{
			fe_EditorCleanup(m_context);
		}
#endif /*EDITOR*/

		/*
		** We have to destroy the layout before calling XtUnmanageChild so that
		** we have a chance to reparent the applet windows to a safe
		** place. Otherwise they'll get destroyed.
		*/
		fe_DestroyLayoutData (m_context);

		XtUnmanageChild (w);
							
		fe_StopProgressGraph (m_context);
		fe_FindReset (m_context);
							
		fe_DisownSelection (m_context, 0, True);
		fe_DisownSelection (m_context, 0, False);
							

		if (m_haveHTMLDisplay) {
#if notyet /* invoking this causes a crash in LJ_UniconifyApplets */
			XtRemoveEventHandler(w, StructureNotifyMask, False, fe_map_notify_eh, m_context);
#endif
			XtRemoveEventHandler (w, FocusChangeMask, False, fe_focus_notify_eh, m_context);
		}
							
		if (CONTEXT_DATA (m_context)->ftd) free (CONTEXT_DATA (m_context)->ftd);
		if (CONTEXT_DATA (m_context)->sd) free (CONTEXT_DATA (m_context)->sd);
	}

	{
		struct fe_MWContext_cons *rest, *prev;
		for (prev = 0, rest = fe_all_MWContexts;
			rest; prev = rest, rest = rest->next)
		  if (rest->context == m_context)
			break;

#ifdef DEBUG_username

	if (!rest)
	{
	XFE_Frame *dbgframe = ViewGlue_getFrame(m_context);
	
	if ( dbgframe ) 
		printf("Frame::doClose()... cannot find m_context(%s) in context list\n", 
		XtName(dbgframe->getBaseWidget()));
	else printf("Frame::doClose()... no frame for m_context (0x%x)\n",
    		m_context);
	}
	 
#endif
		XP_ASSERT(rest!=NULL);
		if (rest)
		{
		   if (prev)
			prev->next = rest->next;
		   else 
			fe_all_MWContexts = rest->next;

		   free (rest);
		}
		else return;
	}
					
#ifdef JAVA
	if (m_haveHTMLDisplay)
		LJ_DiscardEventsForContext(m_context);
#endif /* JAVA */

	XP_RemoveContextFromList(m_context);
	ET_RemoveWindowContext(m_context, really_delete, this);

	}
	else
	{
		D(printf("m_destroyOnClose == False, postponing destroy.\n");)
		hide();
	}
}


void
XFE_Frame::delete_response()
{
	if (fe_IsContextProtected(m_context))
		{
			D(printf ("Context 0x%x is protected.  I'm not deleting it, but marking it deleted\n", m_context);)

			CONTEXT_DATA(m_context)->destroyed = 1;
			return;
		}

	if (isOkToClose())
		doClose();
}

static void
child_destruction_callback(Widget /* w */, XtPointer closure, XtPointer)
{
  XFE_Frame *	frame = (XFE_Frame*)closure;
  Widget		shell = frame->getBaseWidget();

  /* Move forward in destruction process only if no live popups exist. */
  if (XfePopupListCountAlive(shell) == 0)
  {
	  frame->doClose();
  }
}

void
XFE_Frame::app_delete_response()
{
  if (fe_IsContextProtected(m_context))
	{
			D(printf ("Context 0x%x is protected.  I'm not deleting it, but marking it deleted\n", m_context);)

			CONTEXT_DATA(m_context)->destroyed = 1;
			return;
	}

  if (isOkToClose())
  {
	  // here things are a bit more complicated than in the
	  // "user deleted me" case, since we want to hang around if
	  // there are any popup shells.
	  Cardinal number_of_live_popups = XfePopupListCountAlive(m_widget);
	  Cardinal i = 0;
	  
	  if (number_of_live_popups > 0)
	  {
		  D(printf ("Context 0x%x has some popups above it.  we're hanging out until they all are destroyed\n");)

			  for (i = 0; i < XfeNumPopups(m_widget); i ++)
			  {
				  if (XfeIsAlive(XfePopupListIndex(m_widget,i)))
				  {
					  XtAddCallback(XfePopupListIndex(m_widget,i),
									XmNdestroyCallback,
									child_destruction_callback,
									this);
				  }
			  }
	  }
	  else
	  {
		  doClose();
	  }
  }
}

void
XFE_Frame::delete_response(Widget /*widget*/, XtPointer closure, XtPointer /*call_data*/)
{
	XFE_Frame *obj = (XFE_Frame*)closure;

	if (obj->isWMClosable()) 
	{
		if (XFE_MozillaApp::theApp()->toplevelWindowCount() == 1) 
		{
			// If we are closing either the bookmark or history frames, we
			// dont need to exit after closing them, since they are singletons
			// and are only shown/hidden instead of created/destroyed.
			if ((obj->getType() == FRAME_HISTORY) ||
				(obj->getType() == FRAME_BOOKMARK))
			{
				obj->doCommand(xfeCmdClose);
			}
			else
			{
				obj->doCommand(xfeCmdExit);
			}
		}
		else 
		{
			obj->doCommand(xfeCmdClose);
		}
	}
}

void
XFE_Frame::really_delete(void *data)
{
    XFE_Frame *frame = (XFE_Frame*)data;

    Widget w = (Widget)frame->getBaseWidget();

    D(printf("really_delete ... %s\n", XtName(w));)

    XtDestroyWidget(w);
}

void
XFE_Frame::storeProperty (MWContext *context, 
			  char *property,
			  const unsigned char *data)
{
  Widget widget = CONTEXT_WIDGET (context);
  Display *dpy = XtDisplay (widget);
  Window window = XtWindow (widget);
  Atom atom = 0;

  // eventually we'll check to see if the atom is already present
  // if (! atom)
  
  atom = XInternAtom (dpy, property, False);
  XChangeProperty (dpy, window, atom, XA_STRING, 8,
		   PropModeReplace, (const unsigned char *) data, 
		   strlen ((const char *)data));
}
 
XP_Bool
XFE_Frame::isWMClosable()
{
	return m_iswmclosable;
}

void
XFE_Frame::initializeMWContext(EFrameType frame_type,
							   MWContext */*context_to_copy*/)
{
	fe_ContextData *fec;
	struct fe_MWContext_cons *cons;
	MWContextType type = MWContextAny;

	if (m_context) return;  

	switch (frame_type)
		{
		case FRAME_BROWSER: 			type = MWContextBrowser; break;
		case FRAME_EDITOR: 				type = MWContextEditor; break;
		case FRAME_MAILNEWS_COMPOSE: 	type = MWContextMessageComposition; break;
		case FRAME_MAILNEWS_MSG:		type = MWContextMailMsg; break;
		case FRAME_MAILNEWS_THREAD:		type = MWContextMail; break;
		case FRAME_MAILNEWS_FOLDER:		type = MWContextMail; break;
		case FRAME_MAILNEWS_DOWNLOAD:   type = MWContextMail; break;
		case FRAME_ADDRESSBOOK:			type = MWContextAddressBook; break;
		case FRAME_BOOKMARK:			type = MWContextBookmarks; break;
		case FRAME_MAILNEWS_SEARCH:		type = MWContextSearch; break;
		case FRAME_LDAP_SEARCH:			type = MWContextSearchLdap; break;
		case FRAME_DOWNLOAD:			type = MWContextSaveToDisk; break;
		case FRAME_HTML_DIALOG:			type = MWContextDialog; break;
        case FRAME_HISTORY:             type = MWContextHistory; break;
        case FRAME_NAVCENTER:           type = MWContextPane; break; //??
		default:
			XP_ASSERT(0);
			break;
		}

	m_context 	= XP_NewContext();
	if (m_context == NULL) return;
	cons		= XP_NEW_ZAP(struct fe_MWContext_cons);
	if (cons == NULL) return;
	fec		= XP_NEW_ZAP (fe_ContextData);
	if (fec == NULL) return;

	m_context->type = type;
	switch (type)
		{
		case MWContextEditor:
		case MWContextMessageComposition:
			m_context->is_editor = True;
			break;
		default:
			m_context->is_editor = False;
			break;
		}

	CONTEXT_DATA (m_context) = fec;
	CONTEXT_DATA (m_context)->colormap = m_cmap;
	CONTEXT_WIDGET (m_context) = m_widget;

	fe_InitRemoteServer (XtDisplay (m_widget));

	/* add the layout function pointers */
	m_context->funcs = fe_BuildDisplayFunctionTable();
	m_context->convertPixX = m_context->convertPixY = 1;
	m_context->is_grid_cell = FALSE;
	m_context->grid_parent = NULL;

	/* set the XFE default Document Character set */
	CONTEXT_DATA(m_context)->xfe_doc_csid = fe_globalPrefs.doc_csid;

	cons->context = m_context;
	cons->next = fe_all_MWContexts;
	fe_all_MWContexts = cons;
	XP_AddContextToList (m_context);

	fe_InitIconColors(m_context);

	if (m_frametype != FRAME_BOOKMARK)
      fe_createBookmarks(XtParent(m_widget), NULL, NULL);

//	fe_LicenseDialog (m_context);

	XtGetApplicationResources (m_widget,
							   (XtPointer) CONTEXT_DATA (m_context),
							   fe_Resources, fe_ResourcesSize,
							   0, 0);

	// Use colors from prefs

	LO_Color *color;

	color = &fe_globalPrefs.links_color;
	CONTEXT_DATA(m_context)->link_pixel = 
		fe_GetPixel(m_context, color->red, color->green, color->blue);

	color = &fe_globalPrefs.vlinks_color;
	CONTEXT_DATA(m_context)->vlink_pixel = 
		fe_GetPixel(m_context, color->red, color->green, color->blue);

	color = &fe_globalPrefs.text_color;
	CONTEXT_DATA(m_context)->default_fg_pixel = 
		fe_GetPixel(m_context, color->red, color->green, color->blue);

	color = &fe_globalPrefs.background_color;
	CONTEXT_DATA(m_context)->default_bg_pixel = 
		fe_GetPixel(m_context, color->red, color->green, color->blue);

	if (m_haveHTMLDisplay) {
        Display * dpy;
        int screen;
        double pixels;
        double millimeters;

        /* Determine pixels per point for back end font size calculations. */

        dpy = XtDisplay(m_widget);
        screen = XScreenNumberOfScreen(XtScreen(m_widget));

        /* N pixels    25.4 mm    1 inch
         * -------- *  ------- *  ------
         *   M mm       1 inch    72 pts
         */

        pixels      = DisplayWidth(dpy, screen);
        millimeters = DisplayWidthMM(dpy, screen);
        m_context->XpixelsPerPoint =
            ((pixels * MM_PER_INCH) / millimeters) / POINTS_PER_INCH;

        pixels      = DisplayHeight(dpy,screen);
        millimeters = DisplayHeightMM(dpy, screen);
        m_context->YpixelsPerPoint =
            ((pixels * MM_PER_INCH) / millimeters) / POINTS_PER_INCH;


        SHIST_InitSession (m_context);
	
        fe_load_default_font(m_context);

#if notyet /* invoking this causes a crash in LJ_UniconifyApplets */
		XtAddEventHandler(m_widget, StructureNotifyMask,
							  FALSE, (XtEventHandler)fe_map_notify_eh, m_context);
#endif
		XtAddEventHandler(m_widget, FocusChangeMask,
							  FALSE, (XtEventHandler)fe_focus_notify_eh, m_context);
		}

	/*
	 * set the default coloring correctly into the new context.
	 */
	{
		Pixel unused_select_pixel;
		XmGetColors (XtScreen (m_widget),
					 fe_cmap(m_context),
					 CONTEXT_DATA (m_context)->default_bg_pixel,
					 &(CONTEXT_DATA (m_context)->fg_pixel),
					 &(CONTEXT_DATA (m_context)->top_shadow_pixel),
					 &(CONTEXT_DATA (m_context)->bottom_shadow_pixel),
					 &unused_select_pixel);
	}

    // New field added by putterman for increase/decrease font
    m_context->fontScalingPercentage = 1.0;
}

void
XFE_Frame::allConnectionsComplete(MWContext  *context)
{
	/* Tao_17dec96
	 * Notify whoever interested in "allConnectionsComplete" event
	 */
	notifyInterested(XFE_Frame::allConnectionsCompleteCallback, (void*) context);
	notifyInterested(XFE_View::chromeNeedsUpdating);
}

XFE_Frame*
XFE_Frame::getActiveFrame()
{
	XFE_Frame *frame = NULL;

	if (fe_all_MWContexts->next)
		frame = ViewGlue_getFrame(fe_all_MWContexts->next->context);

	return frame;
}

void
XFE_Frame::hackTranslations(Widget widget)
{
	XtTranslations global_translations = 0;
	XtTranslations secondary_translations = 0;
  
	if (XmIsGadget (widget))
		return;

	/* To prevent focus problems, dont enable translations on the menubar
	   and its children. The problem was that when we had the translations
	   in the menubar too, we could do a translation and popup a modal
	   dialog when one of the menu's from the menubar was pulleddown. Now
	   motif gets too confused about who holds pointer and keyboard focus.
	   */

	if (XmIsRowColumn(widget)) {
		unsigned char type;
		XtVaGetValues(widget, XmNrowColumnType, &type, 0);
		if (type == XmMENU_BAR)
			return;
	}

	switch (m_frametype)
		{
		case FRAME_EDITOR:
			global_translations = fe_globalData.global_translations;
			secondary_translations = fe_globalData.editor_global_translations;
			break;

		case FRAME_BROWSER:
			global_translations = fe_globalData.global_translations;
			secondary_translations = fe_globalData.browser_global_translations;
			break;

		case FRAME_MAILNEWS_FOLDER:
			global_translations = fe_globalData.global_translations;
			secondary_translations = fe_globalData.mailnews_global_translations;
			break;

		case FRAME_MAILNEWS_THREAD:
		case FRAME_MAILNEWS_MSG:
			global_translations = fe_globalData.global_translations;
			secondary_translations = fe_globalData.messagewin_global_translations;
			break;

		case FRAME_MAILNEWS_SEARCH:
			global_translations = fe_globalData.mnsearch_global_translations;
			break;

		case FRAME_MAILNEWS_COMPOSE:
			global_translations = fe_globalData.global_translations;
			secondary_translations = fe_globalData.mailcompose_global_translations;
			break;

		case FRAME_BOOKMARK:
			global_translations = fe_globalData.global_translations;
			secondary_translations = fe_globalData.bm_global_translations;
			break;

		case FRAME_ADDRESSBOOK:
			global_translations = fe_globalData.global_translations;
			secondary_translations = fe_globalData.ab_global_translations;
			break;

        case FRAME_HISTORY:
            global_translations = fe_globalData.global_translations;
            secondary_translations = fe_globalData.gh_global_translations;

		default:
			break;
		}

	if (!XP_STRCMP( XtName(widget), "editorDrawingArea" )) {
		XtOverrideTranslations (widget, fe_globalData.global_translations);
		XtOverrideTranslations (widget, fe_globalData.editor_global_translations);
	}
	else {
		if (global_translations)
			XtOverrideTranslations (widget, global_translations);
		if (secondary_translations)
			XtOverrideTranslations (widget, secondary_translations);
	}

	if (XmIsTextField (widget) || XmIsText (widget) || XmIsList(widget))
		{
			/* Set up the editing translations (all text fields, everywhere.) */
			if (XmIsTextField (widget) || XmIsText (widget))
				hackTextTranslations (widget);

			/* Install globalTextFieldTranslations on single-line text fields in
			   windows which have an HTML display area (browser, mail, news) but
			   not in windows which don't have one (compose, download, bookmarks,
			   address book...)
			   */
			if (m_haveHTMLDisplay &&
				XmIsTextField (widget) &&
				fe_globalData.global_text_field_translations)
				XtOverrideTranslations (widget,
										fe_globalData.global_text_field_translations);
		}
	else
		{
			Widget *kids = 0;
			Cardinal nkids = 0;

			/* Not a Text or TextField.
			 */
			/* Install globalNonTextTranslations on non-text widgets in windows which
			   have an HTML display area (browser, mail, news, view source) but not
			   in windows which don't have one (compose, download, bookmarks,
			   address book...)
			   */

			/* Install globalNonTextTranslations on non-text widgets in windows 
			   which have an HTML display area (browser, mail, news, view source)
			   but not in windows which either don't have one (download, 
			   bookmarks, address book...) or have one that is editable.
			   */
			if (m_haveHTMLDisplay
				&& m_frametype != FRAME_EDITOR
				&& m_frametype != FRAME_MAILNEWS_COMPOSE
				&& fe_globalData.global_nontext_translations)
				XtOverrideTranslations (widget,
										fe_globalData.global_nontext_translations);

			/* Now recurse on the children.
			 */
			XtVaGetValues (widget, XmNchildren, &kids, XmNnumChildren, &nkids, 0);
			while (nkids--)
				hackTranslations (kids [nkids]);
		}
}

void
XFE_Frame::hackTextTranslations(Widget widget)
{
	Widget parent = widget;

	for (parent=widget; parent && !XtIsWMShell (parent); parent=XtParent(parent))
		if (XmLIsGrid(parent))
			/* We shouldn't be messing with Grid widget and its children */
			return;

	if (XmIsTextField(widget))
		{
			if (fe_globalData.editing_translations)
				XtOverrideTranslations (widget, fe_globalData.editing_translations);
			if (fe_globalData.single_line_editing_translations)
				XtOverrideTranslations (widget,
										fe_globalData.single_line_editing_translations);
		}
	else if (XmIsText(widget))
		{
			if (fe_globalData.editing_translations)
				XtOverrideTranslations (widget, fe_globalData.editing_translations);
			if (fe_globalData.multi_line_editing_translations)
				XtOverrideTranslations (widget,
										fe_globalData.multi_line_editing_translations);
		}
	else
		{
			XP_ASSERT(0);
		}
}

EFrameType
XFE_Frame::getType()
{
	return m_frametype;
}

void
XFE_Frame::updateToolbar()
{
	if (!m_toolbar)
		return;

	m_toolbar->update();
}

void
XFE_Frame::updateMenuBar()
{
	XP_ASSERT(m_menubar);

	if (!m_menubar)
		return;

	m_menubar->update();
}

void
XFE_Frame::setMenubar(MenuSpec *menu_bar_spec)
{
	XP_ASSERT(m_menubar);

	// Decide if we need to hide Polaris menu item.
	// Hide menu items by setting do_not_manage in MenuSpec to TRUE

	static XP_Bool have_been_here_before = False;

	if (! have_been_here_before) {
		if (! fe_IsPolarisInstalled()) {
			MenuSpec *spec = XFE_Frame::window_menu_spec;
			while (spec->menuItemName) {
				if ((XP_STRCMP(xfeCmdOpenCalendar, spec->menuItemName) == 0) ||
					(XP_STRCMP(xfeCmdOpenHostOnDemand, spec->menuItemName) == 0))
					spec->do_not_manage = True;
				spec++;
			}
		}
		have_been_here_before = True;
	}

	m_menubar->setMenuSpec(menu_bar_spec);
}

void
XFE_Frame::setToolbar(ToolbarSpec *toolbar_spec)
{
	XP_ASSERT(m_toolbar);

	m_toolbar->setToolbarSpec(toolbar_spec);

	return;
}

int
XFE_Frame::getSecurityStatus()
{
  int rval = XFE_UNSECURE;

  HG87111

  return rval;
}

// Next two routines return static data -- copy immediately
// if for some reason you want to keep them.
char* XFE_Frame::getWidthPrefString()
{
    if (geometryPrefName == 0)
        return 0;
    static char widthPref[80];
    XP_SPRINTF(widthPref, "%s.win_width", geometryPrefName);
    return widthPref;
}

char* XFE_Frame::getHeightPrefString()
{
    if (geometryPrefName == 0)
        return 0;
    static char heightPref[80];
    XP_SPRINTF(heightPref, "%s.win_height", geometryPrefName);
    return heightPref;
}

void
XFE_Frame::realize()
{
	Widget widget; /* the widget we want to realize */

//	widget = m_ismodal ? m_chromeparent : m_widget;
	widget = m_widget;

	if (!XtIsRealized(widget))
	{
		Arg			av[20];
		Cardinal	ac = 0;

		// Configure the logo for the first time
		configureLogo();

		// Warning Warning!!

		// All this geometry and command line hackery makes me sick.  
		// The XfeFrameShell widget will eventually take care of all this
		//
		// -ramiro

		//
		// The forcing and saving of prefs should only happen if a chromespec
		// was not specified in the constructor. -re
		//
		// Also, dont screw with modal shells.
		//
		if (!m_chromespec_provided && !m_ismodal)
		{
			int32 width = 0;
			int32 height = 0;
			int32 x = 10;
			int32 y = 10;

			// First, obtain the frame's geometry from prefs if any 
			// Also, ignore the prefs if the ignore_geom_prefs flag is false.
			if (geometryPrefName && !fe_globalData.ignore_geom_prefs)
			{
				PREF_GetIntPref(getWidthPrefString(), &width);
				PREF_GetIntPref(getHeightPrefString(), &height);
			}

			// Next, if we are still processing the command line, try
			// to obtain the frame geometry from the -geometry flag 
			// (-geometry overrides prefs)
			if (!fe_GetCommandLineDone())
			{
				Widget		shell = XfeAncestorFindApplicationShell(m_widget);
				int			mask;
				Position	fx;
				Position	fy;
				Dimension	fwidth;
				Dimension	fheight;

				// I've found that this does not always work.  More 
				// testing is needed to make sure that this works on 
				// all window managers.  For sure, the function could
				// probably be made more robust via some cleverness... -re
					
				// Get geometry from the app shell's XmNgeometry
				mask = XfeShellGetGeometryFromResource(shell,&fx,&fy,
													   &fwidth,&fheight);
					
				if (mask & XValue)
				{
					x = fx;
				}
					
				if (mask & YValue)
				{
					y = fy;
				}
				
				if (mask & WidthValue)
				{
					width = fwidth;
				}
				
				if (mask & HeightValue)
				{
					height = fheight;
				}
			}				

			// At this point (x,y) should be valid
			XtSetArg(av[ac],XmNx,x); ac++;
			XtSetArg(av[ac],XmNy,y); ac++;

			// Use the width and height only if they are non zero
			if (width > 0)
			{
				XtSetArg(av[ac],XmNwidth,width); ac++;
			}

			if (height > 0)
			{
				XtSetArg(av[ac],XmNheight,height); ac++;
			}
		}		

		// Set the startup / pref values if needed
		if (ac)
		{
			XtSetValues(m_widget,av,ac);
		}
		
		XtRealizeWidget(widget);

		// Add the most disgusting feature in the universe to XMozilla:
		// Z-Axis fascist hackery, puke puke puke.

		// Yeah...About a year ago I as given Netcaster koolaid to drink.
		// After drinking the sutff, I realized how the webtop would 
		// take over the universe and added the zaxis hackery support
		// needed by the webtop - there remained a funny feeling in my
		// stomach, though... -ramiro

		// Add z order support if needed
		if (XtIsTopLevelShell(m_widget) && m_chromespec_provided)
		{
			zaxis_AddSupport();
		}

		setCursor(XFE_MozillaApp::theApp()->getBusy());

		XFE_MozillaApp::theApp()->registerInterest(XFE_MozillaApp::appBusyCallback,
												   this,
												   (XFE_FunctionNotification)updateBusyState_cb,
												   (void*)True);
			
		XFE_MozillaApp::theApp()->registerInterest(XFE_MozillaApp::appNotBusyCallback,
												   this,
												   (XFE_FunctionNotification)updateBusyState_cb,
												   (void*)False);

		registerInterest(frameBusyCallback,
						 this,
						 (XFE_FunctionNotification)updateBusyState_cb,
						 (void*)True);

		registerInterest(frameNotBusyCallback,
						 this,
						 (XFE_FunctionNotification)updateBusyState_cb,
						 (void*)False);

		// XX assumes that the m_chromparent is the child of the shell
		hackTranslations(m_chrome); 
			
		notifyInterested(XFE_Component::afterRealizeCallback);

		notifyInterested(XFE_View::chromeNeedsUpdating);

		/* we don't (for various reasons)  want to let these windows do remote stuff. */
		if (m_frametype != FRAME_DOWNLOAD &&
			m_frametype != FRAME_MAILNEWS_DOWNLOAD &&
			m_frametype != FRAME_HISTORY &&
			m_frametype != FRAME_BOOKMARK &&
			m_frametype != FRAME_HTML_DIALOG)
		fe_InitRemoteServerWindow (m_context);
			
		fe_get_final_context_resources (m_context);
			
		fe_find_scrollbar_sizes (m_context);
			
		fe_InitScrolling (m_context);
			
#ifdef MOZ_MAIL_NEWS
        fe_InitIcons (m_context, XFE_MNView::getBiffState());
#endif

		// Add session management support if needed */
		if (fe_globalData.session_management)
		{
			sm_addSaveYourselfCB();
		}
	}
	
	// where should this *really* go?
	fe_VerifyDiskCache (m_context);
	/* spider begin */
	fe_VerifySARDiskCache (m_context);
	/* spider end */
}

void
XFE_Frame::show()
{
	D(printf ("In XFE_Frame::show()\n");)

  	if (!isShown())
	{
		D(printf("Calling realize()\n");)

		realize();

		if (m_ismodal)
		{
			struct fe_MWContext_cons *cons;
			
			for (cons = fe_all_MWContexts ; cons; cons = cons->next)
				fe_fixFocusAndGrab(cons->context); // XXXX No looping over contexts!!!
			
			D(printf ("Managing chrome widget\n");)
				
			XtManageChild(m_chrome);
		}
		else
		{
			// Warning Warning!!
			
			// All this geometry and command line hackery makes me sick.  
			// The XfeFrameShell widget will eventually take care of all this
			//
			// -ramiro
			
			// Do not XtPopup() an iconic shell the first time it is 
			// shown and the command line is still being processed.
			//
			// This is needed to support the -iconic flag properly
			if (XfeShellGetIconicState(m_widget) && 
				m_first_showing && 
				!fe_GetCommandLineDone())
			{
				XtAddEventHandler(m_widget, 
								  StructureNotifyMask,
								  True,
								  iconicFrameMappingEH,
								  (XtPointer) this);
			}
			else
			{
			  /* we return early from the show method if the specified chromespec
				 gave hints that the window should be offscreen.  This means the
				 window will not be mapped.  If at some point in the future 
				 UpdateChrome is called with positive coordinates for the window,
				 then it will be mapped. */
			  if (m_lhint < 0
				  && m_thint < 0)
				{
				  D(printf ("Not showing window, since the hints are offscreen\n");)
				  return;
				}
			  
				XtPopup(m_widget, XtGrabNone);
			}
		}
	}

	map();

//	m_first_showing = False;
}

void
XFE_Frame::map()
{
	if (m_topmost) 
	{
		XMapRaised(XtDisplay(m_widget), XtWindow(m_widget));
	}
	else if (m_bottommost) 
	{
		XLowerWindow(XtDisplay(m_widget), XtWindow(m_widget));
		XMapWindow(XtDisplay(m_widget), XtWindow(m_widget));
	}
	else 
	{
		/* we need this or selecting a window from the communicator menu when
		   that window is already on the screen won't do anything.  It should
		   raise it to the top. */
		XMapRaised(XtDisplay(m_widget), XtWindow(m_widget));
	}
}

void
XFE_Frame::hide()
{
	if (m_ismodal)
		XtUnmanageChild(m_chrome);
	else
		XtPopdown(m_widget);
}

XP_Bool
XFE_Frame::isShown()
{
	if (!XfeIsAlive(m_widget))
	{
		return False;
	}

	// This is safer cause it checks for the window's viewable attribute.
	//return XfeIsViewable(m_widget);

	return XfeShellIsPoppedUp(m_widget);
}


///////////////////////////////////////////
//

void XFE_Frame::showTitleBar()

//
// description:
//		Show the title bar and all other window manager decorations.
//
// reference:
//		Motif Reference Manual 1.2 p431
//
// returns:
//		n/a
//
////
{
  int decoration;

  //
  // The following code works for the Motif Window Manager (mwm)
  //

  decoration = MWM_DECOR_BORDER
	| MWM_DECOR_RESIZEH
	| MWM_DECOR_TITLE
	| MWM_DECOR_MENU
	| MWM_DECOR_MINIMIZE
	| MWM_DECOR_MAXIMIZE;

  XtVaSetValues(m_widget, XmNmwmDecorations, decoration, NULL);
}



///////////////////////////////////////////
//

void XFE_Frame::hideTitleBar()

//
// description:
//		Hide the title bar and all other window manager decorations.
//		Separate functions for showTitleBar and hideTitleBar are implemented
//		for uniformity with XFE_Frame::show() and XFE_Frame::hide().
//
// reference:
//		Motif Reference Manual 1.2 p431
//
// returns:
//		n/a
//
////
{
  int decoration;

  //
  // The following code works for the Motif Window Manager (mwm)
  //

  decoration = MWM_DECOR_BORDER
	| MWM_DECOR_RESIZEH
	| MWM_DECOR_TITLE
	| MWM_DECOR_MENU
	| MWM_DECOR_MINIMIZE
	| MWM_DECOR_MAXIMIZE;

  decoration |= MWM_DECOR_ALL;  // add this flag to hide

  XtVaSetValues(m_widget, XmNmwmDecorations, decoration, NULL);
}


///////////////////////////////////////////
// begin:

XP_Bool XFE_Frame::isTitleBarShown()

//
// description:
//		Determine if the title bar and other
//		window manager decorations are visible.
//
// returns:
//		TRUE or FALSE
//
////
{
  XP_Bool result = TRUE;
  int     decoration;

  //
  // works for mwm only
  //

  decoration = 0;
  XtVaGetValues(m_widget, XmNmwmDecorations, &decoration, NULL);

  if (decoration & MWM_DECOR_ALL)
	result = FALSE;

  return result;
}



void
XFE_Frame::queryChrome(Chrome * chrome)
{
  fe_ContextData *  context_data;
  Dimension view_width;
  Dimension view_height;
  Widget widget;

  if (!chrome)
	return;

  memset(chrome, 0, sizeof (Chrome));
  context_data = CONTEXT_DATA(m_context);

  chrome->show_button_bar        = m_toolbar && m_toolbar->isShown();
  chrome->show_bottom_status_bar = m_dashboard && m_dashboard->isShown();
  chrome->show_menu              = m_menubar && m_menubar->isShown();
  HG37211

  if (isTitleBarShown())
	chrome->hide_title_bar = FALSE;
  else
	chrome->hide_title_bar = TRUE;

  chrome->l_hint             = getX();
  chrome->t_hint             = getY();
  chrome->location_is_chrome = TRUE;
  chrome->outw_hint          = getWidth();
  chrome->outh_hint          = getHeight();

  widget = m_view->getBaseWidget();
  XtVaGetValues(widget, XtNwidth, &view_width, XtNheight, &view_height, 0);

  chrome->w_hint             = (int32)view_width;
  chrome->h_hint             = (int32)view_height;

  //
  // 10-Mar-97.  The X Window System doesn't allow clients
  // to maintain fixed stacking order.  The values set here
  // may not reflect the actual state of the screen.
  //
  chrome->topmost        = m_topmost;
  chrome->bottommost     = m_bottommost;
  chrome->z_lock         = m_zlock;
  chrome->show_scrollbar = m_view->getScrollbarsActive();
}

void
XFE_Frame::respectChrome(Chrome * chrome)
{
  Widget widget;

  if (!chrome)
	return;

  widget = m_view->getBaseWidget();

  // Set width and height of view
  if (chrome->w_hint > 0 && chrome->h_hint > 0)    {
	  XtVaSetValues(m_widget, XmNallowShellResize, TRUE, 0);
	  XtVaSetValues(widget, XmNwidth, chrome->w_hint,
				    XmNheight, chrome->h_hint, 0);
	  XtVaSetValues(m_widget, XmNallowShellResize, FALSE, 0);
  }

  m_view->setScrollbarsActive(chrome->show_scrollbar);

  if (chrome->hide_title_bar)
	hideTitleBar();
  else
	showTitleBar();


  // Respect the tool bar - aka button bar - chrome
  if (m_toolbar) 
  {
    m_toolbar->setShowingState(chrome->show_button_bar);
  }

  if (m_dashboard) 
  {
    m_dashboard->setShowingState(chrome->show_bottom_status_bar);
  }

  if (m_menubar) 
  {
    m_menubar->setShowingState(chrome->show_menu);
  }

  if (chrome->location_is_chrome){
	position(chrome->l_hint, chrome->t_hint);
  }

  // Set width and height of whole frame
  if (chrome->outw_hint > 0 && chrome->outh_hint > 0)    {
      setWidth(chrome->outw_hint);
      setHeight(chrome->outh_hint);
    }

  m_hotkeysdisabled = chrome->disable_commands;

  // Add z order support if needed
  m_topmost     = chrome->topmost;
  m_bottommost  = chrome->bottommost;
  m_zlock       = chrome->z_lock;

  if (XtIsTopLevelShell(m_widget) && XtIsRealized(m_widget))
  {
	  zaxis_AddSupport();
  }
  
  if (XtIsRealized(m_widget)) {
	if (m_topmost)
	  XRaiseWindow(XtDisplay(m_widget), XtWindow(m_widget));
	else if (m_bottommost)
	  XLowerWindow(XtDisplay(m_widget), XtWindow(m_widget));
  }
}

XFE_View *
XFE_Frame::getView()
{
	return m_view;
}

//////////////////////////////////////////////////////////////////////////
//
// Toolbox notices
//
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_Frame, toolboxSnapNotice)
	(XFE_NotificationCenter *	/* obj */, 
	 void *						/* clientData */, 
	 void *						callData)
{
	// If a the frame was constructed with a chromespec, then we ignore
	// all the preference magic.
	if (m_chromespec_provided)
	{
		return;
	}

	XFE_ToolboxItem * item = (XFE_ToolboxItem *) callData;

	XP_ASSERT( item != NULL );

	toolboxItemSnap(item);
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_Frame, toolboxSwapNotice)
	(XFE_NotificationCenter *	/* obj */, 
	 void *						/* clientData */, 
	 void *						 callData)
{
	configureLogo();

	// If a the frame was constructed with a chromespec, then we ignore
	// all the preference magic.
	if (m_chromespec_provided)
	{
		return;
	}

	XFE_ToolboxItem * item = (XFE_ToolboxItem *) callData;

	XP_ASSERT( item != NULL );

	toolboxItemSwap(item);
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_Frame, toolboxCloseNotice)
	(XFE_NotificationCenter *	/* obj */, 
	 void *						/* clientData */, 
	 void *						callData)
{
	configureLogo();

	// If a the frame was constructed with a chromespec, then we ignore
	// all the preference magic.
	if (m_chromespec_provided)
	{
		return;
	}

	XFE_ToolboxItem * item = (XFE_ToolboxItem *) callData;

	XP_ASSERT( item != NULL );

	toolboxItemClose(item);
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_Frame, toolboxOpenNotice)
	(XFE_NotificationCenter *	/* obj */, 
	 void *						/* clientData */, 
	 void *						callData)
{
	configureLogo();

	// If a the frame was constructed with a chromespec, then we ignore
	// all the preference magic.
	if (m_chromespec_provided)
	{
		return;
	}

	XFE_ToolboxItem * item = (XFE_ToolboxItem *) callData;

	XP_ASSERT( item != NULL );

	toolboxItemOpen(item);
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Toolbox methods
//
//////////////////////////////////////////////////////////////////////////
void
XFE_Frame::toolboxItemSnap(XFE_ToolboxItem * item)
{
	XP_ASSERT( item != NULL );
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Frame::toolboxItemSwap(XFE_ToolboxItem * item)
{
	XP_ASSERT( item != NULL );
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Frame::toolboxItemClose(XFE_ToolboxItem * item)
{
	XP_ASSERT( item != NULL );
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Frame::toolboxItemOpen(XFE_ToolboxItem * item)
{
	XP_ASSERT( item != NULL );
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Frame::toolboxItemChangeShowing(XFE_ToolboxItem * item)
{
	XP_ASSERT( item != NULL );
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Frame::configureLogo()
{
	if (!m_toolbox)
	{
		return;
	}

	XP_ASSERT( m_toolbox != NULL );

	XFE_Logo * old_logo = m_activeLogo;
	XFE_Logo * new_logo = getLogo();

	// Assign the new active logo
	m_activeLogo = new_logo;

	// Make sure the logo changed before reconfiguring it
	if (old_logo == new_logo)
	{
		return;
	}

	if (old_logo && new_logo)
	{
		// Make sure the new logo is the same type as the old one
		new_logo->setType(old_logo->getType());

		// If the old logo is running, stop it and continue on the new one
		if (old_logo->isRunning())
		{
			new_logo->setCurrentFrame(old_logo->getCurrentFrame());

			old_logo->stop();

			new_logo->start();
		}
	}

	Cardinal				i;
	XFE_ToolboxItem *	first;

	// Get the first item that is visible
	first = m_toolbox->firstOpenAndManagedItem();

	// Hide the logos of all the non-first items
	for(i = 0; i < m_toolbox->getNumItems(); i++)
	{
		XFE_ToolboxItem * item = m_toolbox->getItemAtIndex(i);

		if (item)
		{
			if (item != first)
			{
				item->hideLogo();
			}
		}
	}

	// Show the logo of the first item
	if (first)
	{
		first->showLogo();
	}
}
//////////////////////////////////////////////////////////////////////////
XFE_Logo *
XFE_Frame::getLogo()
{
	if (!m_toolbox)
	{
		return NULL;
	}

	XP_ASSERT( m_toolbox != NULL );

	XFE_Logo *				logo = NULL;
	XFE_ToolboxItem *	item = m_toolbox->firstOpenAndManagedItem();

	if (item)
	{
		logo = item->getLogo();
	}

	return logo;
}
//////////////////////////////////////////////////////////////////////////
XFE_Dashboard *
XFE_Frame::getDashboard()
{
	return m_dashboard;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Frame::configureToolbox()
{
	XP_ASSERT( m_toolbox != NULL );
}
//////////////////////////////////////////////////////////////////////////

/* use this method to position a frame before it actually pops up.  Let's
   us bypass interactive placement in window managers that use it. */
void
XFE_Frame::position(int x, int y)
{
	WMShellWidget wmshell = (WMShellWidget) m_widget;
    XSizeHints    size_hints;
	Boolean       is_realized;

    XtVaSetValues (m_widget, XtNx, x, XtNy, y, 0);

	/* Horrific kludge because Xt is difficult */
    wmshell->wm.size_hints.flags &= (~PPosition);
    wmshell->wm.size_hints.flags |= USPosition;

	/*
	 * Assigning the wm.size_hints above is
	 * apparently sufficient if the widget
	 * hasn't been realized.
	 */
	is_realized = XtIsRealized(m_widget);
	if (is_realized) {
	  Display *     display;
	  Window        window;
	  Status        status;

	  display = XtDisplay(m_widget);
	  window  = XtWindow(m_widget);
	  status  = XGetNormalHints(display, window, &size_hints);

	  if (status) {
		size_hints.x = wmshell->wm.size_hints.x;
		size_hints.y = wmshell->wm.size_hints.y;
		size_hints.flags &= (~PPosition);
		size_hints.flags |= USPosition;

		XSetNormalHints (display, window, &size_hints);
	  }

	  /* if we were initially created offscreen (we are not visible) and 
		 positioned onscreen, and we aren't mapped, we need to actually map
		 the window. */
	  if (m_lhint < 0 &&
		  m_thint < 0 &&
		  !XfeIsViewable(m_widget) &&
		  x + getWidth() > 0 &&
		  y + getHeight() > 0)
		{
		  D(printf ("Showing window, since the hints were offscreen and it's now on screen \n");)
		  map();
		}
	}
}

void
XFE_Frame::resize(int width, int height)
{
	setWidth(width);
	setHeight(height);  
}

int
XFE_Frame::getX()
{
  return (int) XfeX(m_widget);
}

int
XFE_Frame::getY()
{
  return (int) XfeY(m_widget);
}

int
XFE_Frame::getWidth()
{
    return XfeWidth(m_widget);
}

int
XFE_Frame::getHeight()
{
    return XfeHeight(m_widget);
}

void
XFE_Frame::setWidth(int width)
{
	XtVaSetValues(m_widget,
				  XmNwidth, width,
				  NULL);
}

void
XFE_Frame::setHeight(int height)
{
	XtVaSetValues(m_widget,
				  XmNheight, height,
				  NULL);
}

Widget
XFE_Frame::getChromeParent()
{
	XP_ASSERT( XfeIsAlive(m_chrome) );

	return m_chrome;
}

Widget
XFE_Frame::getDialogParent()
{
	return XfeGetParentDialog(m_widget);
}

fe_colormap *
XFE_Frame::getColormap()
{
	return m_cmap;
}

MWContext *
XFE_Frame::getContext()
{
	return m_context;
}

//////////////////////////////////////////////////////////////////////////
void
XFE_Frame::setView(XFE_View *new_view)
{
	XP_ASSERT( XfeIsAlive(m_chrome) );

	m_view = new_view;

	XtVaSetValues(m_chrome,XmNcenterView,m_view->getBaseWidget(),NULL);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Frame::setAboveViewArea(XFE_Component *above_view)
{
	XP_ASSERT( XfeIsAlive(m_chrome) );

	m_aboveview = above_view;

	XtVaSetValues(m_chrome,XmNtopView,m_aboveview->getBaseWidget(),NULL);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Frame::setBelowViewArea(XFE_Component *below_view)
{
	XP_ASSERT( XfeIsAlive(m_chrome) );

	m_belowview = below_view;

	XtVaSetValues(m_chrome,XmNbottomView,m_belowview->getBaseWidget(),NULL);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Frame::setTitle(char *title)
{
  XFE_SetDocTitle(m_context, title);
}

const char *
XFE_Frame::getTitle()
{
	char * title;

	XtVaGetValues(m_widget,XmNtitle,&title,NULL);

	return (const char *) title;
}

void
XFE_Frame::setIconTitle(const char */*icon_title*/)
{
}

void
XFE_Frame::setIconPixmap(fe_icon */*icon*/)
{
}

XFE_View *
XFE_Frame::widgetToView(Widget w)
{
	return m_view->widgetToView(w);
}

XP_Bool
XFE_Frame::hotKeysDisabled()
{
	return m_hotkeysdisabled;
}

XP_Bool
XFE_Frame::isCommandSelected(CommandType cmd,
							 void *calldata, XFE_CommandInfo* info)
{
	XFE_Command* handler = getCommand(cmd);

	if (handler != NULL)
		return handler->isSelected(this, info);

	/* This method is designed for toggle button */
	/* We want to keep the toggle button to have the same state
     as its matched view */
	// special hack for the history menu.
	if (cmd == xfeCmdOpenUrl)
	{
		return True;
	}
	else if (cmd == xfeCmdFloatingTaskBarAlwaysOnTop)
	{
		return fe_globalPrefs.task_bar_ontop;
	}

	return (m_view && m_view->isCommandSelected(cmd, calldata, info));
}

XP_Bool
XFE_Frame::isCommandEnabled(CommandType cmd,
							void *calldata, XFE_CommandInfo* info)
{
	XFE_Command* handler = getCommand(cmd);

	if (handler != NULL)
		return handler->isEnabled(this, info);

	/* first we handle the commands we know about.. */
	if (cmd == xfeCmdAboutMozilla
		|| cmd == xfeCmdOpenTargetUrl
		|| cmd == xfeCmdExit
		|| cmd == xfeCmdOpenBookmarks
		|| cmd == xfeCmdOpenBrowser
		|| cmd == xfeCmdOpenCustomUrl
		|| cmd == xfeCmdOpenOrBringUpBrowser
		|| cmd == xfeCmdToggleMenubar
		|| cmd == xfeCmdToggleNavigationToolbar
		|| cmd == xfeCmdWindowListRaiseItem
#ifdef MOZ_MAIL_NEWS
		|| cmd == xfeCmdComposeMessage
		|| cmd == xfeCmdComposeMessageHTML
		|| cmd == xfeCmdComposeMessagePlain
		|| cmd == xfeCmdOpenAddressBook
		|| cmd == xfeCmdOpenInbox
		|| cmd == xfeCmdOpenInboxAndGetNewMessages
		|| cmd == xfeCmdOpenFolders
		|| cmd == xfeCmdOpenNewsgroups
#endif
		|| cmd == xfeCmdOpenNavCenter
		)
		{
			return TRUE;
        }
#ifdef EDITOR
	else if (cmd == xfeCmdOpenEditor
		     || cmd == xfeCmdNewBlank
		     || cmd == xfeCmdEditPage
		     || cmd == xfeCmdNewTemplate
		     || cmd == xfeCmdNewWizard
             )
        {
            return ( !fe_IsEditorDisabled() );
		}
	else if (cmd == xfeCmdEditFrame)
		{
			return fe_IsGridParent(m_context) && !fe_IsEditorDisabled();
		}
#endif
	else if (cmd == xfeCmdJavaConsole)
		{
#ifdef JAVA
            return TRUE;
#else
			return FALSE;
#endif
		}
	else if (cmd == xfeCmdOpenConference)
		{
			return fe_IsConferenceInstalled();
		}
	else if (cmd == xfeCmdOpenNetcaster)
		{
#ifdef MOZ_NETCAST
			return fe_IsNetcasterInstalled();
#else
            return FALSE;
#endif
		}
	else if (cmd == xfeCmdOpenCalendar)
        {
            /* check if calendar is installed */
            return fe_IsCalendarInstalled();
		}
	else if (cmd == xfeCmdOpenHostOnDemand)
        {
            /* check if IBM Host On_Demand is installed */
            return fe_IsHostOnDemandInstalled();
		}
#ifdef MOZ_TASKBAR
	if (cmd == xfeCmdToggleTaskbarShowing ||
		cmd == xfeCmdFloatingTaskBarHorizontal ||
		cmd == xfeCmdFloatingTaskBarAlwaysOnTop ||
		cmd == xfeCmdFloatingTaskBarClose)
	{
		return XFE_Dashboard::floatingTaskBarIsAlive();
	}
#endif
	else if (cmd == xfeCmdClose)
		{
			// only enable this menu item if we're not the last window up.
			return (XFE_MozillaApp::theApp()->toplevelWindowCount() != 1);
		}
    else if (cmd == xfeCmdOpenHistory)
        {
            return !fe_globalData.all_databases_locked;
        }
	else
		{
			return (m_view && m_view->isCommandEnabled(cmd, calldata, info));
		}
}

//
//    Wrapper function that does accounting functions.
//
void
xfe_ExecuteCommand(XFE_Frame* frame,
				   CommandType cmd, void* cd, XFE_CommandInfo* info)
{
	XP_Bool slow = TRUE;

	XFE_Command* handler = frame->getCommand(cmd);

	if ((handler != NULL && handler->isSlow() != TRUE) ||
		//
		//    This is because ComposeFrame/ComposeView have this special
		//    handling of these commands (which have handlers, but don't
		//    want to admit it in open public.
		//
		cmd == xfeCmdCut        ||
		cmd == xfeCmdCopy       ||
		cmd == xfeCmdPaste      ||
		cmd == xfeCmdDeleteItem ||
		cmd == xfeCmdSelectAll  ||
		cmd == xfeCmdUndo       ||
		cmd == xfeCmdSpacebar) {
		slow = FALSE;
	}

	if (slow)
		XmUpdateDisplay(frame->getBaseWidget());

	//
	//    don't do notifies when exiting, they may be delivered to
	//    stuff that isn't there any more.
	//
	if (cmd == xfeCmdExit || cmd == xfeCmdClose)
		slow = FALSE;

	if (slow)
		frame->notifyInterested(XFE_Frame::frameBusyCallback);

	if (handler != NULL)
		handler->doCommand(frame, info);
	else
		frame->doCommand(cmd, cd, info);

	if (slow)
		frame->notifyInterested(XFE_Frame::frameNotBusyCallback);
}

void
XFE_Frame::doCommand(CommandType cmd, void *calldata, XFE_CommandInfo* info)
{
	XmUpdateDisplay(m_widget);

	/* first we handle the commands we know about. */
	D(	printf ("in XFE_Frame::doCommand()\n");)

	XFE_Command* handler = getCommand(cmd);
	if (handler != NULL) {
		handler->doCommand(this, info);
	} else if (cmd == xfeCmdToggleMenubar)
			{
				if (m_menubar)
					{
						m_menubar->toggleShowingState();

                        notifyInterested(XFE_View::chromeNeedsUpdating);
					}
			}
		else if (cmd == xfeCmdToggleNavigationToolbar)
			{
				if (m_toolbar)
					{
                      m_toolbar->toggleShowingState();
                      
					  // Configure the logo
					  configureLogo();
					  
					  // Update prefs
					  toolboxItemChangeShowing(m_toolbar);
					  
					  notifyInterested(XFE_View::chromeNeedsUpdating);
					}
			}
#ifdef MOZ_TASKBAR
	    else if (cmd == xfeCmdToggleTaskbarShowing)
		{
			XFE_Dashboard::toggleTaskBar();
		}
	    else if (cmd == xfeCmdFloatingTaskBarHorizontal)
		{
			XFE_Dashboard::setFloatingTaskBarHorizontal(!fe_globalPrefs.task_bar_horizontal);
		}
	    else if (cmd == xfeCmdFloatingTaskBarAlwaysOnTop)
		{
			XFE_Dashboard::setFloatingTaskBarOnTop(!fe_globalPrefs.task_bar_ontop);
		}
	    else if (cmd == xfeCmdFloatingTaskBarClose)
		{
			XFE_Dashboard::dockTaskBar();
		}
#endif
		else if (cmd == xfeCmdExit)
			{
				//
				//    First check the confirmExit resource, maybe post a warning
				//    alert. Allows the user to back out of exit.
				//
				MWContext* context = getContext();
				if (!CONTEXT_DATA(context)->confirm_exit_p ||
					FE_Confirm(context, fe_globalData.really_quit_message)) {
					XFE_MozillaApp::theApp()->exit(0);
				}
			}
		else if (cmd == xfeCmdClose)
			{
				if (m_destroyOnClose)
					delete_response();
				else
					hide();
			}
#ifdef MOZ_MAIL_NEWS
	        else if ( cmd == xfeCmdComposeMessage)
			{
			  if (info) {
				  CONTEXT_DATA(m_context)->stealth_cmd =
					  ((info->event->type == ButtonRelease) &&
					   (info->event->xkey.state & ShiftMask));
			  }

			  MSG_Mail(m_context);
          		}
		else if ( cmd == xfeCmdComposeMessageHTML)
        		{
				CONTEXT_DATA(m_context)->stealth_cmd = (fe_globalPrefs.send_html_msg == False); 
	    			MSG_Mail(m_context);
			}
		else if ( cmd == xfeCmdComposeMessagePlain)
			{
				CONTEXT_DATA(m_context)->stealth_cmd = (fe_globalPrefs.send_html_msg == True) ; 
				MSG_Mail(m_context);
			}		
		else if (cmd == xfeCmdOpenInbox)
			{
				fe_showInbox(m_toplevelWidget, this, NULL, fe_globalPrefs.reuse_thread_window, False);
			}
		else if (cmd == xfeCmdOpenInboxAndGetNewMessages)
			{
				fe_showInbox(m_toplevelWidget, this, NULL, fe_globalPrefs.reuse_thread_window, True);
			}
		else if (cmd == xfeCmdOpenFolders)
			{
				fe_showFolders(m_toplevelWidget, this, NULL);
			}
		else if (cmd == xfeCmdOpenNewsgroups)
			{
				fe_showNewsgroups(m_toplevelWidget, this, NULL);
			}
#endif  // MOZ_MAIL_NEWS
		else if (cmd == xfeCmdOpenBrowser)
			{
				char*       address = NULL;
				URL_Struct* url = NULL;

				if (info != NULL && *info->nparams > 0)
					address = info->params[0];

				if (address != NULL)
					url = NET_CreateURLStruct(address, NET_DONT_RELOAD);
				else
					url = fe_GetBrowserStartupUrlStruct();
				
				fe_showBrowser(m_toplevelWidget, this, NULL, url);
			}
		else if (cmd == xfeCmdOpenNavCenter)
			{
              fe_showNavCenter(m_toplevelWidget, this, NULL, NULL/*url*/);
			}
		else if (cmd == xfeCmdOpenOrBringUpBrowser)
			{
              XFE_BrowserFrame::bringToFrontOrMakeNew(m_toplevelWidget);
			}
		else if (cmd == xfeCmdOpenBookmarks)
			{
				fe_showBookmarks(m_toplevelWidget, this, NULL);
			}
        else if (cmd == xfeCmdOpenHistory)
            {
                XFE_HistoryFrame::showHistory(m_toplevelWidget, this, NULL);
            }
#ifdef MOZ_MAIL_NEWS
		else if (cmd == xfeCmdOpenAddressBook)
			{
				fe_showAddrBook(m_toplevelWidget, this, NULL);
			}
		else if (cmd == xfeCmdOpenConference)
			{
				fe_showConference(getBaseWidget(), 0 /* email */,
								  0 /* use */, 0 /* coolAddr */);

			}
#endif  // MOZ_MAIL_NEWS
		else if (cmd == xfeCmdOpenNetcaster)
			{
#ifdef MOZ_NETSCAST
				fe_showNetcaster(getBaseWidget());
#endif
			}
		else if (cmd == xfeCmdOpenCalendar)
			{
				fe_showCalendar(getBaseWidget());
			}
		else if (cmd == xfeCmdOpenHostOnDemand)
			{
				fe_showHostOnDemand();
			}
		else if (cmd == xfeCmdOpenMinibuffer)
			{
				XFE_Minibuffer *minibuffer;

				if (m_belowview)
					delete m_belowview;
				
				minibuffer = new XFE_Minibuffer(this,m_chrome);
				minibuffer->show();
				setBelowViewArea(minibuffer);
			}
		else if (cmd == xfeCmdJavaConsole)
			{
#ifdef JAVA
				fe_new_show_java_console_cb (m_widget, m_context, NULL);
#endif
			}
#ifdef EDITOR 
		else if (cmd == xfeCmdOpenEditor)
			{
				if (info != NULL && *info->nparams > 0)
					fe_EditorNew(m_context, this, NULL, info->params[0]);
				else
					fe_EditorNew(m_context, this, NULL, NULL);
			}
		else if (cmd == xfeCmdNewBlank)
			{
				char* address = NULL;

				if (info != NULL && *info->nparams > 0)
					address = info->params[0];

				fe_EditorNew(m_context, this, /*chromespec=*/NULL, address);
			}
		else if (cmd == xfeCmdEditPage)
			{
				char* address = NULL;

				if (info != NULL && *info->nparams > 0)
					address = info->params[0];

				fe_EditorEdit(m_context, this, /*chromespec=*/NULL, address);
			}
	    else if (cmd == xfeCmdEditFrame)
			{
				MWContext* f_context = fe_GetFocusGridOfContext(m_context);
		
				if (!f_context)
					f_context = m_context;

				fe_EditorEdit(f_context, this, /*chromespec=*/NULL, NULL);
	        }
	    /*
	     *    Template and Wizard, though considered part of the editor,
		 *    are just a browse to a hard coded url. The user can then choose
		 *    to edit "this page".
		 */
		else if (cmd == xfeCmdNewTemplate)
			{
				char* address = fe_EditorGetTemplateURL(m_context);

				if (address != NULL) {
					URL_Struct* url;
					url = NET_CreateURLStruct(address, NET_DONT_RELOAD);

					fe_showBrowser(m_toplevelWidget, this, NULL, url);
				}
			}
		else if (cmd == xfeCmdNewWizard)
			{
				char* address = fe_EditorGetWizardURL(m_context);

				if (address != NULL) {
					URL_Struct* url;
					url = NET_CreateURLStruct(address, NET_DONT_RELOAD);

					fe_showBrowser(m_toplevelWidget, this, NULL, url);
				}
			}
#endif
		else if (cmd == xfeCmdWindowListRaiseItem)
		{
			// Right now nothing
			return;
		}
		else if (cmd == xfeCmdAboutMozilla)
		{
			fe_about_cb(NULL, m_context, NULL);
		}
		else if (cmd == xfeCmdOpenCustomUrl)
		{
			MWContext * context = m_context;

			// We need to make sure the the ekit_LoadCustomUrl() call receives
			// a valid browser context or else layout will choke looking for 
			// a html drawing area on non browser contexts.  
			// (ie, AddressBook, MessageCenter)
			if (context->type != MWContextBrowser)
			{
				context = fe_FindNonCustomBrowserContext(context);
			}

			// Find the top most frame context
			MWContext * top_context = XP_GetNonGridContext(context);

			// If a browser context is still not found, create one
			if (!top_context || (top_context->type != MWContextBrowser))
			{
				top_context = fe_showBrowser(FE_GetToplevelWidget(), 
											 NULL, NULL, NULL);
			}

			ekit_LoadCustomUrl((char*) calldata, top_context);
		}
	else if (cmd == xfeCmdOpenTargetUrl)
		{
			fe_openTargetUrl(m_context,(LO_AnchorData *) calldata);
		}
		/* if we don't recognize the command, send it along to the view. */
		else if (m_view 
				 && m_view->handlesCommand(cmd, calldata, info))
			{
				m_view->doCommand(cmd, calldata, info);
			}
		else
			{
#if DEBUG
				printf ("Command %s not recognized!\n", Command::getString(cmd));
#endif
				
				XBell(XtDisplay(m_widget), 100);
			}

		D(	printf ("leaving XFE_Frame::doCommand()\n");)
}

XP_Bool
XFE_Frame::handlesCommand(CommandType cmd,
						  void *calldata, XFE_CommandInfo* info)
{
	XFE_Command* handler = getCommand(cmd);

	if (handler != NULL)
		return TRUE;
	/* first we handle the commands we know about.. */
	if (cmd == xfeCmdAboutMozilla
		|| cmd == xfeCmdClose
		|| cmd == xfeCmdOpenTargetUrl
		|| cmd == xfeCmdExit
		|| cmd == xfeCmdJavaConsole
		|| cmd == xfeCmdOpenBookmarks
		|| cmd == xfeCmdOpenBrowser
		|| cmd == xfeCmdOpenHistory
		|| cmd == xfeCmdOpenOrBringUpBrowser
		|| cmd == xfeCmdOpenCalendar
		|| cmd == xfeCmdOpenConference
		|| cmd == xfeCmdOpenCustomUrl
		|| cmd == xfeCmdOpenHostOnDemand
		|| cmd == xfeCmdOpenNetcaster
		|| cmd == xfeCmdToggleMenubar
		|| cmd == xfeCmdToggleNavigationToolbar
		|| cmd == xfeCmdWindowListRaiseItem
#ifdef MOZ_TASKBAR
		|| cmd == xfeCmdToggleTaskbarShowing
		|| cmd == xfeCmdFloatingTaskBarHorizontal
		|| cmd == xfeCmdFloatingTaskBarAlwaysOnTop
		|| cmd == xfeCmdFloatingTaskBarClose
#endif
#ifdef MOZ_MAIL_NEWS
		|| cmd == xfeCmdComposeMessage
		|| cmd == xfeCmdComposeMessageHTML
		|| cmd == xfeCmdComposeMessagePlain
		|| cmd == xfeCmdEditMailFilterRules
		|| cmd == xfeCmdOpenAddressBook
		|| cmd == xfeCmdOpenFolders
		|| cmd == xfeCmdOpenInbox
		|| cmd == xfeCmdOpenInboxAndGetNewMessages
		|| cmd == xfeCmdOpenNewsgroups
#endif
		|| cmd == xfeCmdOpenNavCenter
		)
		{
			return TRUE;
		}
#ifdef EDITOR
	else if (cmd == xfeCmdOpenEditor
		     || cmd == xfeCmdNewBlank
		     || cmd == xfeCmdNewTemplate
		     || cmd == xfeCmdNewWizard
		     || cmd == xfeCmdEditPage
		     || cmd == xfeCmdEditFrame
             )
		{
			return ( !fe_IsEditorDisabled() );
		}
#endif
	else
		{
			return (m_view && m_view->handlesCommand(cmd, calldata, info));
		}
}

char *
XFE_Frame::commandToString(CommandType cmd,
						   void *calldata, XFE_CommandInfo* info)
{
	XFE_Command* handler = getCommand(cmd);

	if (handler != NULL)
		return handler->getLabel(this, info);

	if (cmd == xfeCmdComposeMessage)
		{
			char *res;
			res = "composeMessage";
			return stringFromResource(res);
		}
	else if (cmd == xfeCmdToggleMenubar)
		{
			char *res = NULL;

			if (m_menubar->isShown())
				res = "hideMenubarCmdString";
			else
				res = "showMenubarCmdString";

			return stringFromResource(res);
		}
	else if (cmd == xfeCmdToggleNavigationToolbar)
		{
			char *res = NULL;

			if (m_toolbar->isShown())
				res = "hideNavToolbarCmdString";
			else
				res = "showNavToolbarCmdString";

			return stringFromResource(res);
		}
#ifdef MOZ_TASKBAR
	else if (cmd == xfeCmdToggleTaskbarShowing)
	{
		char *res = NULL;
		
		if (XFE_Dashboard::isTaskBarDocked())
		{
			res = "showTaskbarCmdString";
		}
		else
		{
			res = "dockTaskbarCmdString";
		}
		
		return stringFromResource(res);
	}
	else if (cmd == xfeCmdFloatingTaskBarHorizontal)
	{
		char *res = NULL;

		// Show the oposite label so that the user can toggle
		if (fe_globalPrefs.task_bar_horizontal)
		{
			res = "floatingTaskBarVerticalCmdString";
		}
		else
		{
			res = "floatingTaskBarHorizontalCmdString";
		}

		return stringFromResource(res);
	}
#endif
	else if (m_view)
		{
			return m_view->commandToString(cmd, calldata, info);
		}
	else
		{
			return NULL;
		}
}

void
XFE_Frame::setCursor(XP_Bool busy)
{
	Cursor c;
	
	if (busy)
		c = CONTEXT_DATA(m_context)->busy_cursor;
	else
		c = None;
	
	XDefineCursor(XtDisplay(m_widget), XtWindow(m_widget), c);
}

XFE_CALLBACK_DEFN(XFE_Frame, updateBusyState)(XFE_NotificationCenter */*obj*/,
											  void *clientData,
											  void */*callData*/)
{
	XP_Bool busy = (XP_Bool)(int)clientData;

	setCursor(busy);
}

XFE_CALLBACK_DEFN(XFE_Frame, doCommandCallback)(XFE_NotificationCenter */*obj*/,
                                                void */*clientData*/,
                                                void *callData)
{
    CommandType cmd = (CommandType)callData;

    doCommand(cmd);
}

XFE_CALLBACK_DEFN(XFE_Frame, toplevelWindowChangeOccured)(XFE_NotificationCenter */*obj*/,
														  void */*clientData*/,
														  void */*callData*/)
{
	notifyInterested(XFE_View::commandNeedsUpdating, (void*)xfeCmdClose);
}


//
// Logo animation notifications
//
XFE_CALLBACK_DEFN(XFE_Frame, logoAnimationStartNotice)
	(XFE_NotificationCenter *	/* obj */,
	 void *						/* clientData */,
	 void *						/* callData */)
{
	XFE_Logo * logo = getLogo();

	if (logo)
	{
		logo->start();
	}
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_Frame, logoAnimationStopNotice)
	(XFE_NotificationCenter *	/* obj */,
	 void *						/* clientData */,
	 void *						/* callData */)
{
	XFE_Logo * logo = getLogo();

	if (logo)
	{
		logo->stop();
	}
}
//////////////////////////////////////////////////////////////////////////

char *
XFE_Frame::prompt(const char *caption, const char *message, const char *deflt)
{
	return (char*)fe_prompt(m_context, m_widget, caption, message,
							TRUE, (deflt ? deflt : ""), TRUE, FALSE, 0);
}

Pixel
XFE_Frame::getFGPixel()
{
	return CONTEXT_DATA(m_context)->fg_pixel;
}

Pixel
XFE_Frame::getBGPixel()
{
	return CONTEXT_DATA(m_context)->default_bg_pixel;
}

Pixel
XFE_Frame::getTopShadowPixel()
{
	return CONTEXT_DATA(m_context)->top_shadow_pixel;
}

Pixel
XFE_Frame::getBottomShadowPixel()
{
	return CONTEXT_DATA(m_context)->bottom_shadow_pixel;
}

char *XFE_Frame::getDocString(CommandType cmd)
{
	char*        doc_string = NULL;
	XFE_Command* handler = getCommand(cmd);

	//    For now impliment info = NULL, because only composer
	//    plugins are using this, but, really need info..
	if (handler != NULL)
		doc_string = handler->getDocString(this, NULL);

	if (doc_string == NULL) 
	{
		doc_string = XfeSubResourceGetStringValue(getBaseWidget(),
												  (char *) cmd,
												  "NSCommand",
												  "documentationString", 
												  "DocumentationString",
												  NULL);
	}

	return doc_string;
}

char *XFE_Frame::getTipString(CommandType /* cmd */)
{
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//
// Keep track of the frame shell mapping.  On the first MapNotify, we
// do a show() on the frame so that all the 
//
//////////////////////////////////////////////////////////////////////////
/* static */ void 
XFE_Frame::iconicFrameMappingEH(Widget		shell,
								XtPointer	clientData,
								XEvent *	event,
								Boolean *	cont)
{
	XFE_Frame * frame = (XFE_Frame *) clientData;

	if (!frame || (frame && !frame->isAlive()))
	{
		return;
	}

	if (event && (event->type == MapNotify))
	{
		// We only need this event handler to be called once
		XtRemoveEventHandler(shell,StructureNotifyMask,True,
							 iconicFrameMappingEH,clientData);

		frame->show();

		// show() will not call XtPopup() which is needed in order for 
		// the proper grab to be done on the shell
		XtPopup(frame->getBaseWidget(), XtGrabNone);

	}

	*cont = True;
}
//////////////////////////////////////////////////////////////////////////

// Display calendar
extern "C" void fe_showCalendar(Widget w)
{
	char execu[1024];

	// Return if we didn't find calendar.
	XP_ASSERT(fe_calendar_path);
	if(!fe_calendar_path) {
		return;
	}

	XP_STRCPY(execu, fe_calendar_path);

#if defined(IRIX)
	pid_t mypid = fork();
#else
	pid_t mypid = vfork();
#endif
	if (mypid == 0) {

#if defined(DEBUG_lwei)
		printf("\n Child : mypid=%d \n", mypid);
		printf("\n *** calendar=%s\n", execu);
#endif
		close (ConnectionNumber(XtDisplay(w)));

		/* 
		 * int execlp (const char *file, const char *arg0, ..., 
		 * int execvp (const char *file, char *const *argv);
		 * const char *argn, (char *)0);
		 */
		execlp(execu, execu, 0);
		execlp(execu, execu, 0);

#if defined(DEBUG_lwei)
		printf("\nExiting calendar \n");
#endif
		_exit(0);
	}/* if */
#if defined(DEBUG_lwei)
	else if (mypid > 0) {
		printf("\n Parent: child's mypid=%d \n", mypid);
	}/* else */
#endif
	else if (mypid < 0)
		printf("\n ERROR: can not fork! \n");
	
}/* fe_showCalendar() */


// Display Host On-Demand
extern "C" void fe_showHostOnDemand()
{
	MWContext *context = XP_GetNonGridContext(fe_all_MWContexts->context);
	char       buf[2048];

	// Return if we didn't find host on demand.
	XP_ASSERT(fe_host_on_demand_path);
	if(!fe_host_on_demand_path) {
		return;
	}

	PR_snprintf (buf, sizeof (buf), "file:%.900s", fe_host_on_demand_path);
	FE_GetURL(context, NET_CreateURLStruct(buf, NET_DONT_RELOAD));
}


//////////////////////////////////////////////////////////////////////////
//
// Session management functions
//
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// The atoms for nuclear fission
//
//////////////////////////////////////////////////////////////////////////
/* static */ Atom XFE_Frame::sm_wm_command = None;
/* static */ Atom XFE_Frame::sm_wm_save_yourself = None;

//////////////////////////////////////////////////////////////////////////
//
// Return a list of all frames that are alive, top level shells & realzied.
//
//////////////////////////////////////////////////////////////////////////
/* static */ XP_List *
XFE_Frame::sm_getAliveShellFrames()
{
	XP_List *	frame_list = XFE_MozillaApp::theApp()->getAllFrameList();
	Cardinal	frame_count = XP_ListCount(frame_list);

	XP_List *	alive_frame_list = NULL;
	Cardinal	i;

	// Find the shown frames and add them to a list
	for (i = 0; i < frame_count; i++)
	{
		// Get the next frame
		XFE_Frame * frame = (XFE_Frame*) XP_ListNextObject(frame_list);

		// Add it to list if valid and shown
		if (frame && frame->isAlive())
		{
			Widget shell = frame->getBaseWidget();

			if (shell && XtIsRealized(shell) && XtIsWMShell(shell))
			{
				// Create a new list as soon as we find the first shown item
				if (!alive_frame_list)
				{
					alive_frame_list = XP_ListNew();
				}
				
				XP_ListAddObject(alive_frame_list,frame);
			}
		}
	}

	return alive_frame_list;
}
//////////////////////////////////////////////////////////////////////////
//
// Return the shell that talks with the session manager
//
//////////////////////////////////////////////////////////////////////////
/* static */ Widget
XFE_Frame::sm_getSessionManagerShell()
{
	// For CDE, the shell that talks with the sm is always the app shell
	if (!fe_globalData.irix_session_management)
	{
		return FE_GetToplevelWidget();
	}

	// For IRIX and maybe other session managers, chose one of the top
	// level shells
	XP_List *	frame_list = XFE_MozillaApp::theApp()->getAllFrameList();
	Cardinal	frame_count = XP_ListCount(frame_list);
	Cardinal	i;

	// Find the shown frames and add them to a list
	for (i = 0; i < frame_count; i++)
	{
		// Get the next frame
		XFE_Frame * frame = (XFE_Frame*) XP_ListNextObject(frame_list);

		// Use this frame if its shown
		if (frame && frame->isAlive() && frame->isShown())
		{
			return frame->getBaseWidget();
		}
	}

	XFE_Frame * active_frame = XFE_Frame::getActiveFrame();

	// Use this frame if its shown
	if (active_frame && active_frame->isAlive() && active_frame->isShown())
	{
		return active_frame->getBaseWidget();
	}
	
	return NULL;
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_Frame::sm_clearCommandProperties()
{
	// For CDE, clear WM_COMMAND on the app shell
	if (!fe_globalData.irix_session_management)
	{
		Widget top = FE_GetToplevelWidget();
		
		XP_ASSERT( XfeIsAlive(top) && XtIsRealized(top) );
		XP_ASSERT( XFE_Frame::sm_wm_command != None );
		
		XDeleteProperty(XtDisplay(top),XtWindow(top),XFE_Frame::sm_wm_command);

		return;
	}

	// For IRIX and other sms clear WM_COMMAND on all top level shells
	// that are valid
	XP_List *	frame_list = XFE_MozillaApp::theApp()->getAllFrameList();
	Cardinal	frame_count = XP_ListCount(frame_list);
	Cardinal	i;

	// Find the shown frames and add them to a list
	for (i = 0; i < frame_count; i++)
	{
		// Get the next frame
		XFE_Frame * frame = (XFE_Frame *) XP_ListNextObject(frame_list);
		
		Widget shell = frame->getBaseWidget();
		
		if (shell && XtIsRealized(shell) && XtIsWMShell(shell))
		{
			XP_ASSERT( XFE_Frame::sm_wm_command != None );

			XDeleteProperty(XtDisplay(shell),
							XtWindow(shell),
							XFE_Frame::sm_wm_command);
		}
	}
}
//////////////////////////////////////////////////////////////////////////
//
// Return the current address for the frame (what xfeCmdAddBookmark would use)
//
//////////////////////////////////////////////////////////////////////////
/* static */ char *
XFE_Frame::sm_getAddressForFrame(XFE_Frame * frame)
{
	char * address = NULL;

	if (frame)
	{
		MWContext *		context = frame->getContext();
 		History_entry *	h = (History_entry *)
			(context ? SHIST_GetCurrent(&context->hist) : NULL);

		if (h && h->address)
		{
			address = h->address;
		}						
	}
	
	return address;
}
//////////////////////////////////////////////////////////////////////////
//
// Allocate an argv from a XP_List of strings
//
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_Frame::sm_allocArgvForStringList(XP_List *	list,
									 char ***	argv_out,
									 int *		argc_out,
									 int		num_fixed)
{
	char **		argv = NULL;
	int			argc = 0;

	if (list)
	{
		int num_strings = XP_ListCount(list);

		argc = num_strings + num_fixed;

		if (argc > 0)
		{
			int i;

			argv = (char **) XtMalloc(sizeof(char *) * argc);

			for(i = 0; i < num_strings; i++)
			{
				char * next = (char *) XP_ListNextObject(list);

				if (next)
				{
					argv[i + num_fixed] = (char *) XtNewString(next);
				}
				else
				{
					argv[i + num_fixed] = NULL;
				}
			}
		}
	}

	*argc_out = argc;
	*argv_out = argv;
}
//////////////////////////////////////////////////////////////////////////
//
// Free an argv
//
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_Frame::sm_freeArgvForStringList(char ** argv,int argc)
{
	if (argc && argv)
	{
		int i;
		
		for(i = 0; i < argc; i++)
		{
			if (argv[i])
			{
				XtFree(argv[i]);

				argv[i] = NULL;
			}
		}

		XtFree((char *) argv);
	}
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_Frame::sm_freeStringList(XP_List * list)
{
	if (list)
	{
		int count = XP_ListCount(list);
		
		if (count)
		{
			int i;

			for (i = 0; i < count; i++)
			{
				char * str = (char *) XP_ListNextObject(list);

				if (str)
				{
					XtFree(str);
				}
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////
//
// Return an XP_List of the addresses of the frames that participate in 
// session management.
//
//////////////////////////////////////////////////////////////////////////
#define SM_DEBUG_SHOW_CURRENT(i,n,a,t) \
printf("session[%d of %d] address = '%s' , type = '%s' (%d)\n", \
	   (i) , (num_frames) , ( (a) ? (a) : "NULL" ) , "Its_Gone", t );
//////////////////////////////////////////////////////////////////////////
/* static */ XP_List *
XFE_Frame::sm_getFrameAddressList(XP_List * frames)
{
	XP_List * urls = NULL;

	if (frames)
	{
		int	num_frames = XP_ListCount(frames);
		int	i;

		for(i = 0; i < num_frames; i++)
		{
			XFE_Frame * frame = (XFE_Frame *) XP_ListNextObject(frames);

			if (frame)
			{
				char * address = NULL;
				char * str = NULL;

				//
				// SESSION MANAGER:
				//
				// To add session manager support to a frame add a case 
				// statement here.  Say you want to add support for a 
				// new startup flag ( -bookmarks ) that brings up bookmarks
				// on startup, add the following below:
				//
				// case FRAME_BOOKMARK:
				//   str = (char *) XtNewString("-bookmarks");
				//   break;
				//
				// Of course, the -bookmarks flag must be supported
				// properly.  Look in mozilla.c for starup flag madness.
				//
				//
				switch(frame->getType())
				{
  					// Mailbox: or news:   If the user does not re-use the 
					// Messenger frame, then multiple -mail or -news are
					// possible.  These will be treated as one by the 
					// command line parser in main() (look in mozilla.c)
				case FRAME_MAILNEWS_THREAD:

					address = XFE_Frame::sm_getAddressForFrame(frame);

					if (address)
					{
						int length = XP_STRLEN("news:");

						if (XP_STRNCMP(address,"news:",length) == 0)
						{
							str = (char *) XtNewString("-news");
						}
						else
						{
							str = (char *) XtNewString("-mail");
						}
					}

					break;

  					// Message Center
				case FRAME_MAILNEWS_FOLDER:

					str = (char *) XtNewString("-news");

					break;

					// Browser urls are pasted as is on the command line.
					// So are message windows.  Other things like nethelp:
					// and ldap: could also go here since they are supported
					// by mkgeturl.
				case FRAME_BROWSER:
				case FRAME_MAILNEWS_MSG:


					address = XFE_Frame::sm_getAddressForFrame(frame);

					if (address)
					{
						str = (char *) XtNewString(address);
					}

					break;

 				case FRAME_ADDRESSBOOK:

					str = (char *) XtNewString("addrbk:");

					break;

  				case FRAME_BOOKMARK:

					if (frame->isShown())
					{
						str = (char *) XtNewString("-bookmarks");
					}

 					break;
					
  				case FRAME_HISTORY:
					
					if (frame->isShown())
					{
						str = (char *) XtNewString("-history");
					}

					break;

				case FRAME_MAILFILTER:
				case FRAME_LDAP_SEARCH:
				case FRAME_MAILNEWS_DOWNLOAD:
				case FRAME_MAILNEWS_SEARCH:
				case FRAME_MAILNEWS_COMPOSE:
				case FRAME_HTML_DIALOG:
				case FRAME_EDITOR:
				case FRAME_DOWNLOAD:
				case FRAME_NAVCENTER:

					// Dont do anything for these
	
					break;

				} // end switch

				if (str)
				{
					// Create a new list as soon as we find a good str
					if (!urls)
					{
						urls = XP_ListNew();
					}
					
					XP_ListAddObject(urls,str);
				}

			} // if (frame)

		} // for 

	} // if (frames)
	
	return urls;
}
//////////////////////////////////////////////////////////////////////////
//
// Add support for the WM_SAVE_YOURSELF protocol
//
//////////////////////////////////////////////////////////////////////////
void 
XFE_Frame::sm_addSaveYourselfCB()
{
	XP_ASSERT( XfeIsAlive(m_widget) && XtIsRealized(m_widget) );

	// Initialize the atoms
	if (sm_wm_save_yourself == None)
	{
		XFE_Frame::sm_wm_save_yourself =
			XmInternAtom(XtDisplay(m_widget),"WM_SAVE_YOURSELF",False);
	}

	if (sm_wm_command == None)
	{
		XFE_Frame::sm_wm_command = 
			XmInternAtom(XtDisplay(m_widget),"WM_COMMAND",False);
	}

	// For CDE install the WM_SAVE_YOURSELF protocol on the app shell (once)
	// and make sure that this shell is realized and not mapped.  Otherwise
	// the session manager will not speak to it.
	if (!fe_globalData.irix_session_management)
	{
		static XP_Bool handler_installed_on_top_level = False;

		// Install the handler on the top level widget only once
		if (!handler_installed_on_top_level)
		{
			Widget top = FE_GetToplevelWidget();
			
			XP_ASSERT( XfeIsAlive(top) );
			
			if (!XfeIsAlive(top))
			{
				return;
			}

			// Make it invisible
			XtVaSetValues(top,XmNmappedWhenManaged,False,NULL);

			// Make sure its realized before doing any property stuff
			XtRealizeWidget(top);
			
			// Add the protocol
			XmAddWMProtocols(top,&XFE_Frame::sm_wm_save_yourself,1);
			
			// Add the protocol callback
			XmAddWMProtocolCallback(top,
									XFE_Frame::sm_wm_save_yourself,
									&XFE_Frame::sm_saveYourselfCB,
									(XtPointer) this);
			
			handler_installed_on_top_level = True;
		}

		return;
	}


	// For other sms add the WM_SAVE_YOURSELF protocol to every single
	// frame that is valid

	// Make sure m_widget is wm shell
	XP_ASSERT( XtIsWMShell(m_widget) );

	if (!XtIsWMShell(m_widget))
	{
		return;
	}

	// Add the protocol
	XmAddWMProtocols(m_widget,&XFE_Frame::sm_wm_save_yourself,1);

	// Add the protocol callback
	XmAddWMProtocolCallback(m_widget,
							XFE_Frame::sm_wm_save_yourself,
							&XFE_Frame::sm_saveYourselfCB,
							(XtPointer) this);
}
//////////////////////////////////////////////////////////////////////////
//
// Handle the WM_SAVE_YOURSELF protocol.  This callback will be invoked
// by the session manager once for each frame.  We dont care which frame
// caused the callback to be invoked.  All we need is one window manager
// shell (the active frame shell) to hold the WM_COMMAND property.  
//
// If this callback is called more than once (for many frames), the 
// XSetCommand() function wlll be called more than once for the active
// frame shell.  This seems to be ok, even though it seems like a waste.
//
//////////////////////////////////////////////////////////////////////////
/* static */ void 
XFE_Frame::sm_saveYourselfCB(Widget		/* w */,
							 XtPointer	/* clientData */,
							 XtPointer	/* callData */)
{
	// Obtain the shell that talks with the session manager
	Widget session_shell = XFE_Frame::sm_getSessionManagerShell();

	// Make sure the session shell is good
	if (!XfeIsAlive(session_shell) || 
		!XtIsRealized(session_shell) || 
		!XtIsWMShell(session_shell))
	{
		return;
	}

	// Clear all the WM_COMMAND properties
	XFE_Frame::sm_clearCommandProperties();

	// Obtain the saved argv and argc ( done in main() in mozilla.c )
	int			saved_argc = fe_GetSavedArgc();
	char **		saved_argv = fe_GetSavedArgv();

	// Obtain a list of all the frames that are alive
 	XP_List *	frames = XFE_Frame::sm_getAliveShellFrames();

	if (frames)
	{
		// If argc is 1, then it only contains the exe name.  This is the
		// case when the user starts the app simply as 'netscape' and then
		// creates new frames on the fly.  For this case we build the 
		// argv dynamically.
		if (saved_argc == 1)
		{
			// Obtain a list of current urls
			XP_List * urls = XFE_Frame::sm_getFrameAddressList(frames);

			if (urls)
			{
				int			target_argc = 0;
				char **		target_argv = NULL;

				// Allocate the argv (skip argv[0] for the exe name)
				XFE_Frame::sm_allocArgvForStringList(urls,
													 &target_argv,
													 &target_argc,
													 1);

				if (target_argc && target_argv)
				{
					// Set argv[0] to be the exe name
					target_argv[0] = (char *) XtNewString(saved_argv[0]);

					// Set the WM_COMMAND property for the session shell
					XSetCommand(XtDisplay(session_shell),
								XtWindow(session_shell), 
								target_argv,target_argc);

					// Free the argv
					XFE_Frame::sm_freeArgvForStringList(target_argv,
														target_argc);
				}

				// Free the url strings
				XFE_Frame::sm_freeStringList(urls);

				// Free the url string list
				XP_ListDestroy(urls);
			}
			
			// Free the frame list
			XP_ListDestroy(frames);
		}
		// If argc is anything other than 1, then the user invoked netscape
		// with flags.  We honor those flags and assume the user knows
		// what he/she is doing.
		//
		// This will also be the case if netscape was invoked by a session
		// manager with command line arguments.
		else
		{
			// Set the WM_COMMAND property for the shell
			XSetCommand(XtDisplay(session_shell),
						XtWindow(session_shell), 
						saved_argv,saved_argc);
		}
	}
}
//////////////////////////////////////////////////////////////////////////

//
// end of session management functions
//

//////////////////////////////////////////////////////////////////////////
//
// Z-Order support
//
// The files MozillaWm.[ch] define and implement a protocol by which a
// mozilla browser can provide a window manager hints to determine its
// z-order placement.  Window managers that support this protocol will 
// greatly improve the usability of z-locked windows such as netcaster
// webtop.
//
// If no such extension is detected on the root window, then we use 
// event tracking hackery instead.
//
//////////////////////////////////////////////////////////////////////////
void
XFE_Frame::zaxis_AddSupport()
{
	XP_ASSERT( XfeIsAlive(m_widget) );
	XP_ASSERT( XtIsRealized(m_widget) );
	XP_ASSERT( XtIsTopLevelShell(m_widget) );
        
	// Only browsers with a chromespec provided can use this stuff
	if ((m_frametype != FRAME_BROWSER) || !m_chromespec_provided)
	{
		return;
	}

	Screen *	screen = XtScreen(m_widget);
	Window		window = XtWindow(m_widget);

	// Check the window manager for Mozilla wm extension support
	long		version = MozillaWmGetWmVersion(screen);

	// If the version is valid, then add the mozilla wm hints to the shell
	if (version != MOZILLA_WM_VERSION_INVALID)
	{
		// Clear the old hints so that the next statement get a fresh mask
		MozillaWmSetHints(screen,window,MOZILLA_WM_NONE);

		// At least one of these must be true
		if (!m_topmost && !m_bottommost && !m_zlock)
		{
			return;
		}

		XP_ASSERT( ! (m_topmost && m_bottommost) );
                
		// Set topmost if needed
		if (m_topmost)
			MozillaWmSetHints(screen,window,MOZILLA_WM_ALWAYS_TOP);
		
		// Set bottomost if needed
		if (m_bottommost)
			MozillaWmSetHints(screen,window,MOZILLA_WM_ALWAYS_BOTTOM);
		
		// Add zlock if needed
		if (m_zlock)
			MozillaWmAddHints(screen,window,MOZILLA_WM_ZORDER_LOCK);
	}
#ifdef NETCASTER_ZAXIS_HACKERY
	//
	// Otherwise resort to hackery
	//
	// This stuff is hackery that somewhat honors the topmost and bottommost
	// chrome members by tracking visiblity events on frame shells and
	// then forcing a re-stacking.  In order to avoid having two or more 
	// frames that set these chrome options fight each other, a count of
	// the number of times per second visiblity events cause a re-stacking
	// and if this exceeds a magic number, we ignore the request.
	//
	else
	{
		// This startup flag gives users of netcaster the ability to bypass the
		// fascist window stacking madness implemented here.
		if (fe_globalData.dont_force_window_stacking)
		{
			return;
		}

		// Clear the handlers in case they were added before
		if (m_zaxis_BelowHandlerInstalled)
			zaxis_RemoveHandler(m_widget,Below);

		if (m_zaxis_AboveHandlerInstalled)
			zaxis_RemoveHandler(m_widget,Above);

		// At least one of these must be true
		if (!m_topmost && !m_bottommost && !m_zlock)
		{
			return;
		}
        
		// Install topmost handler
		if (m_topmost)
		{
			zaxis_InstallHandler(m_widget,Above);
		}
		// Install bottommost handler
		else if (m_bottommost)
		{
			zaxis_InstallHandler(m_widget,Below);
		}
		// Ignore zlock
	}
#endif
}

#ifdef NETCASTER_ZAXIS_HACKERY
//////////////////////////////////////////////////////////////////////////
//
// Netscaster z-axis topmost and bottommost hackery
//
//////////////////////////////////////////////////////////////////////////

// Max number of times per second to either raaise or lower a frame
#define ZAXIS_FASCIST_LIMIT	4

//////////////////////////////////////////////////////////////////////////
//
// Install the raise/lower hackery.  mode is either Above or Below
//
//////////////////////////////////////////////////////////////////////////
#define ZAXIS_EVENTS_TO_TRACK					\
(												\
	VisibilityChangeMask	| 					\
 	FocusChangeMask			| 					\
	StructureNotifyMask							\
)
//////////////////////////////////////////////////////////////////////////
#define ZAXIS_EVENT_IS_GOOD(event)				\
(												\
	(event->type == VisibilityNotify)	||		\
	(event->type == FocusIn)			||		\
	(event->type == ConfigureNotify)			\
)
//////////////////////////////////////////////////////////////////////////
//
// Add the event handler that keeps track of events related to stacking
//
//////////////////////////////////////////////////////////////////////////
void
XFE_Frame::zaxis_InstallHandler(Widget shell,int mode)
{
	XP_ASSERT( XtIsShell(shell) );
	XP_ASSERT( XfeIsAlive(shell) );
	XP_ASSERT( mode == Above || mode == Below );

	m_zaxis_LastStackingChangeTime = 0;
	m_zaxis_StackingChangesPerSecond = 0;

	// Add the appropiate visiblity event handler to the shell
	if (mode == Above)
	{
		XP_ASSERT( m_zaxis_AboveHandlerInstalled == False );

		if (m_zaxis_AboveHandlerInstalled)
			return;

		XtAddEventHandler(shell,
						  ZAXIS_EVENTS_TO_TRACK,
						  True,
						  &XFE_Frame::zaxis_AboveEH,
						  (XtPointer) this);

		m_zaxis_AboveHandlerInstalled = True;
	}
	else if (mode == Below)
	{
		XP_ASSERT( m_zaxis_BelowHandlerInstalled == False );

		if (m_zaxis_BelowHandlerInstalled)
		{
			return;
		}

		XtAddEventHandler(shell,
						  ZAXIS_EVENTS_TO_TRACK,
						  True,
						  &XFE_Frame::zaxis_BelowEH,
						  (XtPointer) this);

		m_zaxis_BelowHandlerInstalled = True;
	}
}
//////////////////////////////////////////////////////////////////////////
//
// Remove the event handler that keeps track of events related to stacking
//
//////////////////////////////////////////////////////////////////////////
void
XFE_Frame::zaxis_RemoveHandler(Widget shell,int mode)
{
	XP_ASSERT( XtIsShell(shell) );
	XP_ASSERT( XfeIsAlive(shell) );
	XP_ASSERT( mode == Above || mode == Below );

	// Add the appropiate visiblity event handler to the shell
	if (mode == Above)
	{
		XP_ASSERT( m_zaxis_AboveHandlerInstalled == True );

		if (!m_zaxis_AboveHandlerInstalled)
			return;

		XtRemoveEventHandler(shell,
							 ZAXIS_EVENTS_TO_TRACK,
							 True,
							 &XFE_Frame::zaxis_AboveEH,
							 (XtPointer) this);

		m_zaxis_AboveHandlerInstalled = False;
	}
	else if (mode == Below)
	{
		XP_ASSERT( m_zaxis_BelowHandlerInstalled == True );

		if (!m_zaxis_BelowHandlerInstalled)
			return;

		XtRemoveEventHandler(shell,
							 ZAXIS_EVENTS_TO_TRACK,
							 True,
							 &XFE_Frame::zaxis_BelowEH,
							 (XtPointer) this);

		m_zaxis_BelowHandlerInstalled = False;
	}
}
//////////////////////////////////////////////////////////////////////////
//
// Deal with a visiblity change in the frame's shell.  Keep track of the
// number of times per second that this operation is tried and only
// modify the shell stacking order of this count is less than a 
// magic number.  mode can be Above or Below.
//
//////////////////////////////////////////////////////////////////////////
void
XFE_Frame::zaxis_HandleEvent(Widget shell,int mode)
{
	XP_ASSERT( mode == Above || mode == Below );

	// The time 'now'
	time_t time_now = time(NULL);
	
	// The time difference between 'now' and the last time we were called
	time_t time_diff = time_now - m_zaxis_LastStackingChangeTime;
	
	// If the difference is 0, then we are being called more than once
	// in one second - increment the changes/sec counter.
	if (time_diff == 0)
	{
		m_zaxis_StackingChangesPerSecond++;
	}
	// Otherwise we are being called after a 'long' interval and we 
	// reset the changes/sec counter.
	else
	{
		m_zaxis_StackingChangesPerSecond = 0;
	}

	// If the times/sec counter is more than a magic number, then we
	// are being called too often and we are most likely in an infinite
	// loop fighting with another window that also wants to be always
	// on top.
	if (m_zaxis_StackingChangesPerSecond < ZAXIS_FASCIST_LIMIT)
	{
		// Place window on top
		XWindowChanges changes;
		
		changes.stack_mode = mode;
		
		XReconfigureWMWindow(XtDisplay(shell),
							 XtWindow(shell), 
							 XScreenNumberOfScreen(XtScreen(shell)),
							 CWStackMode,&changes);

		if (mode == Above)
		{
			m_topmost = True;
		}
		else if (mode == Below)
		{
			m_bottommost = True;
		}

		// Update the last last change time
		m_zaxis_LastStackingChangeTime = time_now;
	}
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_Frame::zaxis_AboveEH(Widget			shell,
						 XtPointer		clientData,
						 XEvent *		event,
						 Boolean *		cont)
{
// Not sure if this needs to be checked ???
//
//		(event->xvisibility.state != VisibilityUnobscured) &&
//

	// Make sure the shell and event are valid, the Xm menu system or dnd
	// are not grabbed and tips are not showing.  Also make sure the zaxis
	// stacking system is not locked.
	if (event && XfeIsAlive(shell) &&
		!XfeDisplayIsUserGrabbed(shell) &&
		!fe_ToolTipIsShowing())
	{
		if (ZAXIS_EVENT_IS_GOOD(event))
		{
			XFE_Frame * frame = (XFE_Frame *) clientData;

			frame->zaxis_HandleEvent(shell,Above);
		}
	}
	
	*cont = True;
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_Frame::zaxis_BelowEH(Widget			shell,
						 XtPointer		clientData,
						 XEvent *		event,
						 Boolean *		cont)
{
	// Make sure the shell and event are valid, the Xm menu system or dnd
	// are not grabbed and tips are not showing.  Also make sure the zaxis
	// stacking system is not locked.
	if (event && XfeIsAlive(shell) &&
		!XfeDisplayIsUserGrabbed(shell) &&
		!fe_ToolTipIsShowing())
	{
		if (ZAXIS_EVENT_IS_GOOD(event))
		{
			XFE_Frame * frame = (XFE_Frame *) clientData;

			frame->zaxis_HandleEvent(shell,Below);
		}
	}
	
	*cont = True;
}
//////////////////////////////////////////////////////////////////////////
#endif

//
// end of netcaster z axis hackery
//
