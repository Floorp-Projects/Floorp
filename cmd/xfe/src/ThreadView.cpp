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
   ThreadView.cpp -- presents view of mail threads.
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
   */



#include "MozillaApp.h"
#include "ThreadView.h"
#include "MsgFrame.h"
#include "FolderFrame.h"
#include "FolderMenu.h"
#include "ViewGlue.h"

#if defined(USE_MOTIF_DND)
#include "OutlinerDrop.h"
#endif /* USE_MOTIF_DND */

#include "xpassert.h"
#include "xfe.h"
#include "icondata.h"
#include "msgcom.h"

#if defined(USE_ABCOM)
#include "ABListSearchView.h"
#endif /* USE_ABCOM */

//#include "allxpstr.h"
#include "felocale.h" /* for fe_ConvertToLocalEncoding */
#include "xp_mem.h"
#include "Xfe/Xfe.h"

#include <Xm/ArrowB.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/PanedW.h>

#include "MNSearchFrame.h"
#include "MailFilterDlg.h"
#include "LdapSearchFrame.h"
#include "prefs.h"

#include "prefapi.h"

#ifdef DEBUG_toshok
#define D(x) x
#else
#define D(x)
#endif

#ifdef DEBUG_dora
#define DD(x) x
#else
#define DD(x)
#endif

#define OUTLINER_GEOMETRY_PREF "mail.threadpane.outliner_geometry"
#define MESSAGEPANE_HEIGHT_PREF "mail.threadpane.messagepane_height"

#include "xpgetstr.h"

/*
extern int MSG_HasAttachments(MSG_Pane* pane, MessageKey msgKey, XP_Bool *pHasThem);
*/
extern int XFE_RE;
extern int XFE_WINDOW_TITLE_NEWSGROUP;
extern int XFE_WINDOW_TITLE_FOLDER;
extern int XFE_SIZE_IN_LINES;
extern int XFE_SIZE_IN_BYTES;
extern int XFE_THREAD_OUTLINER_COLUMN_SUBJECT;
extern int XFE_THREAD_OUTLINER_COLUMN_DATE;
extern int XFE_THREAD_OUTLINER_COLUMN_PRIORITY;
extern int XFE_THREAD_OUTLINER_COLUMN_STATUS;
extern int XFE_THREAD_OUTLINER_COLUMN_SENDER;
extern int XFE_THREAD_OUTLINER_COLUMN_RECIPIENT;

extern int XP_STATUS_NEW;
extern int XP_STATUS_REPLIED_AND_FORWARDED;
extern int XP_STATUS_FORWARDED;
extern int XP_STATUS_REPLIED;

extern int MK_MSG_CANCEL_MESSAGE;
extern int MK_MSG_DELETE_SEL_MSGS;

extern int XFE_DND_MESSAGE_ERROR;

const int XFE_ThreadView::OUTLINER_COLUMN_SUBJECT = 0;
const int XFE_ThreadView::OUTLINER_COLUMN_SENDERRECIPIENT = 1;
const int XFE_ThreadView::OUTLINER_COLUMN_UNREADMSG = 2;
const int XFE_ThreadView::OUTLINER_COLUMN_DATE = 3;
const int XFE_ThreadView::OUTLINER_COLUMN_PRIORITY = 4;
const int XFE_ThreadView::OUTLINER_COLUMN_FLAG = 5;
const int XFE_ThreadView::OUTLINER_COLUMN_STATUS = 6;
const int XFE_ThreadView::OUTLINER_COLUMN_SIZE = 7;

// Minimum and maximum sizes for the paned window panes:
#define PANE_MIN 50
#define PANE_MAX 6000

fe_icon XFE_ThreadView::threadonIcon = { 0 };
fe_icon XFE_ThreadView::threadoffIcon = { 0 };

MenuSpec XFE_ThreadView::priority_popup_submenu[] = {
  { xfeCmdSetPriorityHighest,	PUSHBUTTON },
  { xfeCmdSetPriorityHigh,	PUSHBUTTON },
  { xfeCmdSetPriorityNormal,	PUSHBUTTON },
  { xfeCmdSetPriorityLow,	PUSHBUTTON },
  { xfeCmdSetPriorityLowest,	PUSHBUTTON },
  { NULL }
};

MenuSpec XFE_ThreadView::news_popup_spec[] = {
  { xfeCmdOpenSelected,     PUSHBUTTON },
// open in new window
  MENU_SEPARATOR,
  // Setting the call data to non-null is a signal to commandToString
  // not to reset the menu string
  { xfeCmdReplyToSender,	PUSHBUTTON, NULL, NULL, False, "reply-to-sender" },
  { xfeCmdReplyToAll,	PUSHBUTTON, NULL, NULL, False, "reply-to-all" },
  { xfeCmdReplyToNewsgroup,	PUSHBUTTON, NULL, NULL, False, "reply-to-newsgroup" },
  { xfeCmdReplyToSenderAndNewsgroup,    PUSHBUTTON, NULL, NULL, False, "reply-to-sender-and-newsgroup" },
  { xfeCmdForwardMessage,		PUSHBUTTON },
  { xfeCmdForwardMessageQuoted,	PUSHBUTTON },
  { xfeCmdForwardMessageInLine,	PUSHBUTTON },
  MENU_SEPARATOR,
  { "addToABSubmenu",   CASCADEBUTTON,
    (MenuSpec *) &addrbk_submenu_spec },
  MENU_SEPARATOR,
  { xfeCmdIgnoreThread,		PUSHBUTTON },
  { xfeCmdWatchThread,		PUSHBUTTON },
  MENU_SEPARATOR,
  { "fileSubmenu",              DYNA_CASCADEBUTTON, NULL, NULL, False, (void*)xfeCmdMoveMessage, XFE_FolderMenu::generate },
  { xfeCmdDeleteMessage,	PUSHBUTTON },
  { xfeCmdSaveMessagesAs,	PUSHBUTTON },
  { xfeCmdPrint,		PUSHBUTTON },
  { NULL }

};

MenuSpec XFE_ThreadView::mail_popup_spec[] = {
  { xfeCmdOpenSelected,     PUSHBUTTON },
  MENU_SEPARATOR,
  // Setting the call data to non-null is a signal to commandToString
  // not to reset the menu string
  { xfeCmdReplyToSender,	PUSHBUTTON, NULL, NULL, False, "reply-to-sender" },
  { xfeCmdReplyToAll,		PUSHBUTTON, NULL, NULL, False, "reply-to-all" },
  { xfeCmdForwardMessage,	PUSHBUTTON },
  { xfeCmdForwardMessageQuoted,	PUSHBUTTON },
  { xfeCmdForwardMessageInLine,	PUSHBUTTON },
  MENU_SEPARATOR,
  { "addToABSubmenu",   CASCADEBUTTON,
    (MenuSpec *) &addrbk_submenu_spec },
  MENU_SEPARATOR,
  { xfeCmdIgnoreThread,		PUSHBUTTON },
  { xfeCmdWatchThread,		PUSHBUTTON },
  MENU_SEPARATOR,
  { "changePriority",		CASCADEBUTTON, priority_popup_submenu },
  MENU_SEPARATOR,
  { "fileSubmenu",              DYNA_CASCADEBUTTON, NULL, NULL, False, (void*)xfeCmdMoveMessage, XFE_FolderMenu::generate },
  { xfeCmdDeleteMessage,	PUSHBUTTON },
  { xfeCmdSaveMessagesAs,	PUSHBUTTON },
  { xfeCmdPrint,		PUSHBUTTON },
  { NULL }
};

MenuSpec XFE_ThreadView::addrbk_submenu_spec[] = {
	{ xfeCmdAddSenderToAddressBook,	PUSHBUTTON },
	{ xfeCmdAddAllToAddressBook,		PUSHBUTTON },
	//  { xfeCmdAddCardToAddressBook,		PUSHBUTTON } ?
	{ NULL }
};

extern "C" void fe_showSubscribeDialog(XFE_NotificationCenter *toplevel,
                                       Widget parent, MWContext *context,
                                       MSG_Host *host = NULL);


XFE_ThreadView::XFE_ThreadView(XFE_Component *toplevel_component,
			       Widget parent,
			       XFE_View *parent_view, MWContext *context,
			       MSG_Pane *p) 
  : XFE_MNListView(toplevel_component, parent_view, context, p)
{
  /* initialize
   */
  m_nLoadingFolders = 0;

  m_frameDeleted = False;

  /* timer
   */
  m_scrollTimer = 0 ;
  m_targetIndex = MSG_VIEWINDEXNONE;
#if HANDLE_CMD_QUEUE

  m_lastLoadedInd = MSG_VIEWINDEXNONE;
  m_lastLoadedKey = MSG_MESSAGEKEYNONE;
  m_lineChanged = MSG_VIEWINDEXNONE;

  m_selected = NULL;
  m_selectedCount = 0;

  m_deleted = NULL;
  m_deletedCount = 0;

  m_collapsed = NULL;
  m_collapsedCount = 0;

#endif /* HANDLE_CMD_QUEUE */

  Widget panedw;
  int num_columns = 8;
  XtWidgetGeometry arrow_geo;
  static int default_column_widths[] = {30, 20, 3, 20, 9, 3, 15, 6};

  panedw = XtVaCreateWidget("panedw",
							xmPanedWindowWidgetClass,
							parent,
							NULL);


  // create the outliner message list.
  m_outliner = new XFE_Outliner("messageList",
								this,
								getToplevel(),
								panedw,
								FALSE,  // constantSize
								TRUE,   // hasHeadings
								num_columns, 
								5,
								default_column_widths,
								OUTLINER_GEOMETRY_PREF);

  // initialize the icons if they haven't already been
  {
    Pixel bg_pixel;
    
    XtVaGetValues(m_outliner->getBaseWidget(), XmNbackground, &bg_pixel, 0);

	initMessageIcons(getToplevel()->getBaseWidget(),
					 getToplevel()->getFGPixel(),
					 bg_pixel);
  }

  // now that we've created the icons, we can size the two icon-only columns to be
  // the widths required of the pixmaps.

  m_outliner->setColumnWidth ( OUTLINER_COLUMN_UNREADMSG, msgUnreadIcon.width + 2 /* for the outliner's shadow */);
  m_outliner->setColumnWidth ( OUTLINER_COLUMN_FLAG, msgFlagIcon.width + 2 /* for the outliner's shadow */);

  m_outliner->setPipeColumn( OUTLINER_COLUMN_SUBJECT );
  m_outliner->setMultiSelectAllowed( True );

  m_outliner->setColumnResizable( OUTLINER_COLUMN_UNREADMSG, False );
  m_outliner->setColumnResizable( OUTLINER_COLUMN_FLAG, False );

  m_outliner->setHideColumnsAllowed( True );

#if !defined(USE_MOTIF_DND)
  fe_dnd_CreateDrop(m_outliner->getBaseWidget(), drop_func, this);
#endif /* USE_MOTIF_DND */

  XtVaSetValues(m_outliner->getBaseWidget(),
				XmNallowResize, TRUE,
				XmNpaneMinimum, PANE_MIN, // should this be a resource?
				XmNpaneMaximum, PANE_MAX, // why is the limit in HPaned 1000???
				NULL);

  m_outliner->show();

  // create the arrow button stuff

  m_arrowform = XtVaCreateManagedWidget("arrowform",
										xmFormWidgetClass,
										panedw,
										XmNallowResize, FALSE,
										XmNskipAdjust, TRUE,
										NULL);

  m_arrowb = XtVaCreateManagedWidget("arrowb",
				     xmArrowButtonWidgetClass,
				     m_arrowform,
				     XmNleftAttachment, XmATTACH_FORM,
				     XmNtopAttachment, XmATTACH_FORM,
				     XmNbottomAttachment, XmATTACH_FORM,
				     XmNrightAttachment, XmATTACH_NONE,
				     XmNarrowDirection, XmARROW_DOWN,
				     XmNshadowThickness, 0,
				     NULL);

  XtAddCallback(m_arrowb, XmNactivateCallback, toggleMsgExpansionCallback, this);

  // Arrow label should be blank initially.
  XmString tempString = XmStringCreateSimple(" ");
  m_arrowlabel = XtVaCreateManagedWidget("arrowlabel",
					 xmLabelWidgetClass,
					 m_arrowform,
					 XmNleftAttachment, XmATTACH_WIDGET,
					 XmNleftWidget, m_arrowb,
					 XmNtopAttachment, XmATTACH_FORM,
					 XmNbottomAttachment, XmATTACH_FORM,
					 XmNrightAttachment, XmATTACH_FORM,
					 XmNalignment, XmALIGNMENT_BEGINNING,
					 XmNlabelString, tempString,
					 NULL);
  XmStringFree(tempString);

  arrow_geo.request_mode = CWHeight;
  XtQueryGeometry(m_arrowform, NULL, &arrow_geo);

  XtVaSetValues(m_arrowform,
		XmNpaneMaximum, arrow_geo.height,
		XmNpaneMinimum, arrow_geo.height,
		NULL);

  // create the message viewing area.
  m_msgview = new XFE_MsgView(toplevel_component, panedw, this, m_contextData);
  addView(m_msgview);

  XtVaSetValues(m_msgview->getBaseWidget(),
				XmNallowResize, TRUE,
				XmNpaneMinimum, PANE_MIN, // should this be a resource?
				XmNpaneMaximum, PANE_MAX, // why is the limit in PanedW 1000???
				NULL);

  m_msgview->registerInterest(XFE_MsgView::spacebarAtMsgBottom,
			      this,
			      (XFE_FunctionNotification)spaceAtMsgEnd_cb);

  m_msgview->registerInterest(XFE_MsgView::messageHasChanged,
							  this,
							  (XFE_FunctionNotification)newMessageLoading_cb);

  getToplevel()->registerInterest(XFE_Frame::allConnectionsCompleteCallback,
								  this,
								  (XFE_FunctionNotification)allConnectionsComplete_cb);

  if (!p)
    setPane(MSG_CreateThreadPane(m_contextData,
				 XFE_MNView::m_master));


  m_msgExpanded = TRUE;

  {
	  Widget scrolled = XtNameToWidget (m_msgview->getBaseWidget(), "*scroller");
	  int32 message_pane_desired_height;
	  PREF_GetIntPref(MESSAGEPANE_HEIGHT_PREF, &message_pane_desired_height);

	  XtVaSetValues(scrolled,
					XmNheight, message_pane_desired_height,
					NULL);
	  
	  m_msgview->show();
  }

  /* safe default, for now.  It'll be overriden (if need be), when
	 a folder is loaded. */

  m_displayingDraft = FALSE;

  m_commandPending = noPendingCommand;
  m_getNewMsg = False;

  m_selectionAfterDeleting = MSG_VIEWINDEXNONE;
  m_popup = NULL;
  m_folderInfo = NULL;
#if defined(USE_ABCOM)
  int error = 
	  AB_SetShowPropertySheetForEntryFunc((MSG_Pane *) m_pane,
										  &XFE_ABListSearchView::ShowPropertySheetForEntryFunc);
#endif /* USE_ABCOM */

  setBaseWidget(panedw);
}

XFE_ThreadView::~XFE_ThreadView()
{
D(printf ("In XFE_ThreadView::~XFE_ThreadView()\n");)


	/* save off the height of the message pane. */
	{
		Dimension message_pane_height;
		Widget scrolled = XtNameToWidget (m_msgview->getBaseWidget(), "*scroller");
		
		XtVaGetValues(scrolled,
					  XmNheight, &message_pane_height,
					  NULL);
		
		PREF_SetIntPref(MESSAGEPANE_HEIGHT_PREF, (int32)message_pane_height);
	}		

  delete m_outliner;

  destroyPane();

  if (m_popup)
    delete m_popup;

  // XFE_View destroys m_msgview for us.
}

void XFE_ThreadView::setGetNewMsg(XP_Bool b)
{
	m_getNewMsg = b;
}

fe_icon*
XFE_ThreadView::flagToIcon(int folder_flags, int message_flags)
{
    if (folder_flags & MSG_FOLDER_FLAG_NEWSGROUP)
		return (message_flags & MSG_FLAG_READ ? &newsPostIcon : &newsNewIcon);
    else if (folder_flags & MSG_FOLDER_FLAG_DRAFTS)
		return &draftIcon;
    else if (message_flags & MSG_FLAG_IMAP_DELETED
             || message_flags & MSG_FLAG_EXPUNGED)
        return &deletedIcon;

    else if (message_flags & MSG_FLAG_ATTACHMENT)
    {
	return &mailMessageAttachIcon;
    }
    else if (folder_flags & MSG_FOLDER_FLAG_TEMPLATES)
    {
	return &templateIcon;
    }
    else
    {
	return (message_flags & MSG_FLAG_READ ? &mailMessageReadIcon : &mailMessageUnreadIcon);
    }
}

#if defined(USE_MOTIF_DND)
fe_icon_data*
XFE_ThreadView::flagToIconData(int folder_flags, int message_flags)
{
  if (folder_flags & MSG_FOLDER_FLAG_NEWSGROUP)
	return (message_flags & MSG_FLAG_READ ? &MN_Newspost : &MN_NewsNew);
  else if (folder_flags & MSG_FOLDER_FLAG_DRAFTS)
	return &MN_Draftfile;
  else if (message_flags & MSG_FLAG_IMAP_DELETED
  else if (folder_flags & MSG_FOLDER_FLAG_DRAFTS)
		   || message_flags & MSG_FLAG_EXPUNGED)
	return &MN_Delete;
  else
	return (message_flags & MSG_FLAG_READ ? &MN_MailRead : &MN_MailUnread);
}
#endif /* USE_MOTIF_DND */

void
XFE_ThreadView::initMessageIcons(Widget widget,
								 Pixel bg_pixel,
								 Pixel fg_pixel)
{
	static Boolean init_icons_done = False;

	if (init_icons_done)
		return;

	init_icons_done = True;

    if (!mailMessageReadIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &mailMessageReadIcon,
					   NULL, 
					   MN_MailRead.width, MN_MailRead.height,
					   MN_MailRead.mono_bits, MN_MailRead.color_bits, MN_MailRead.mask_bits, FALSE); 

    if (!mailMessageUnreadIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &mailMessageUnreadIcon,
					   NULL, 
					   MN_MailUnread.width, MN_MailUnread.height,
					   MN_MailUnread.mono_bits, MN_MailUnread.color_bits, MN_MailUnread.mask_bits, FALSE);

    if (!draftIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &draftIcon,
					   NULL, 
					   MN_Draftfile.width, MN_Draftfile.height,
					   MN_Draftfile.mono_bits, MN_Draftfile.color_bits, MN_Draftfile.mask_bits, FALSE);
	
    if (!templateIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &templateIcon,
					   NULL, 
					   MN_Templatefile.width, MN_Templatefile.height,
					   MN_Templatefile.mono_bits, MN_Templatefile.color_bits, MN_Templatefile.mask_bits, FALSE);
	
    if (!newsPostIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &newsPostIcon,
					   NULL, 
					   MN_Newspost.width, MN_Newspost.height,
					   MN_Newspost.mono_bits, MN_Newspost.color_bits, MN_Newspost.mask_bits, FALSE);

    if (!newsNewIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &newsNewIcon,
					   NULL, 
					   MN_NewsNew.width, MN_NewsNew.height,
					   MN_NewsNew.mono_bits, MN_NewsNew.color_bits, MN_NewsNew.mask_bits, FALSE);
	
    if (!msgReadIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &msgReadIcon,
					   NULL, 
					   MN_DotRead.width, MN_DotRead.height,
					   MN_DotRead.mono_bits, MN_DotRead.color_bits, MN_DotRead.mask_bits, FALSE);
	
    if (!msgUnreadIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &msgUnreadIcon,
					   NULL, 
					   MN_Unread.width, MN_Unread.height,
					   MN_Unread.mono_bits, MN_Unread.color_bits, MN_Unread.mask_bits, FALSE);

    if (!msgFlagIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &msgFlagIcon,
					   NULL, 
					   MN_Flag.width, MN_Flag.height,
					   MN_Flag.mono_bits, MN_Flag.color_bits, MN_Flag.mask_bits, FALSE);
    
    if (!mailMessageAttachIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &mailMessageAttachIcon,
					   NULL, 
					   MN_MailAttach.width, MN_MailAttach.height,
					   MN_MailAttach.mono_bits, MN_MailAttach.color_bits, MN_MailAttach.mask_bits, FALSE); 

	if (!threadonIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &threadonIcon,
					   NULL, 
					   threadon.width, threadon.height,
					   threadon.mono_bits, threadon.color_bits, 
					   threadon.mask_bits, FALSE);
    
	if (!threadoffIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &threadoffIcon,
					   NULL, 
					   threadoff.width, threadoff.height,
					   threadoff.mono_bits, threadoff.color_bits, 
					   threadoff.mask_bits, FALSE);

	if (!openSpoolIgnoredIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &openSpoolIgnoredIcon,
					   NULL,
					   MN_ThreadIgnoreO.width, MN_ThreadIgnoreO.height,
					   MN_ThreadIgnoreO.mono_bits, MN_ThreadIgnoreO.color_bits,
					   MN_ThreadIgnoreO.mask_bits, FALSE);

	if (!closedSpoolIgnoredIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &closedSpoolIgnoredIcon,
					   NULL,
					   MN_ThreadIgnore.width, MN_ThreadIgnore.height,
					   MN_ThreadIgnore.mono_bits, MN_ThreadIgnore.color_bits,
					   MN_ThreadIgnore.mask_bits, FALSE);

	if (!openSpoolWatchedIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &openSpoolWatchedIcon,
					   NULL,
					   MN_ThreadWatchO.width, MN_ThreadWatchO.height,
					   MN_ThreadWatchO.mono_bits, MN_ThreadWatchO.color_bits,
					   MN_ThreadWatchO.mask_bits, FALSE);

	if (!closedSpoolWatchedIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &closedSpoolWatchedIcon,
					   NULL,
					   MN_ThreadWatch.width, MN_ThreadWatch.height,
					   MN_ThreadWatch.mono_bits, MN_ThreadWatch.color_bits,
					   MN_ThreadWatch.mask_bits, FALSE);

	if (!openSpoolNewIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &openSpoolNewIcon,
					   NULL,
					   MN_ThreadNew/*O*/.width, MN_ThreadNew/*O*/.height,
					   MN_ThreadNew/*O*/.mono_bits, MN_ThreadNew/*O*/.color_bits,
					   MN_ThreadNew/*O*/.mask_bits, FALSE);

	if (!closedSpoolNewIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &closedSpoolNewIcon,
					   NULL,
					   MN_ThreadNew.width, MN_ThreadNew.height,
					   MN_ThreadNew.mono_bits, MN_ThreadNew.color_bits,
					   MN_ThreadNew.mask_bits, FALSE);

	if (!openSpoolIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &openSpoolIcon,
					   NULL,
					   MN_ThreadO.width, MN_ThreadO.height,
					   MN_ThreadO.mono_bits, MN_ThreadO.color_bits,
					   MN_ThreadO.mask_bits, FALSE);

	if (!closedSpoolIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &closedSpoolIcon,
					   NULL,
					   MN_Thread.width, MN_Thread.height,
					   MN_Thread.mono_bits, MN_Thread.color_bits,
					   MN_Thread.mask_bits, FALSE);

	if (!deletedIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &deletedIcon,
					   NULL,
					   MN_Delete.width, MN_Delete.height,
					   MN_Delete.mono_bits, MN_Delete.color_bits,
					   MN_Delete.mask_bits, FALSE);
}

#if HANDLE_CMD_QUEUE

/* It looks like a selectInvalid here
 */
void XFE_ThreadView::processCmdQueue()
{
	int count =0;
	int *Arr = NULL;

	if (m_deletedCount && m_deleted) {
		count = m_deletedCount;
		Arr = m_deleted;
	}/* else if */

#if defined(DEBUG_tao)
	printf("\n->XFE_ThreadView::processCmdQueue, count=%d", count);
	if (count > 1)
		XP_ASSERT(0);
#endif

	int lastInd = count?Arr[count-1]:-1;
	for (int i=count-2; i >= 0; i--)
		if (Arr[i] < lastInd)
			lastInd--;

	int nLines = MSG_GetNumLines(m_pane);
	if (lastInd >= nLines)
		/* the worst case is -1 ;
		 * which we do deal with it in paneChanged already!
		 */
		lastInd = nLines-1;

	if (lastInd >= 0 && 
		m_lastLoadedInd != MSG_VIEWINDEXNONE)
		showMessage(lastInd);
#if defined(DEBUG_tao)
	else
		printf("\n->XFE_ThreadView::processCmdQueue,m_lastLoadedInd=%d,",
			   m_lastLoadedInd);	
#endif

	/* final step
	 */

	/* reset 
	 */
	XP_FREEIF(m_selected);
	m_selectedCount = 0;
	XP_FREEIF(m_deleted);
	m_deletedCount = 0;
	XP_FREEIF(m_collapsed);
	m_collapsedCount = 0;
}

#endif /* HANDLE_CMD_QUEUE */

#if defined(DEL_5_0)
void
XFE_ThreadView::listChangeStarting(XP_Bool /* asynchronous */, 
								   MSG_NOTIFY_CODE notify, 
								   MSG_ViewIndex /* where */, 
								   int32 /* num */)
{
#if defined(DEBUG_tao)
	int listChangeDepth = m_outliner->getListChangeDepth()-1;
	printf("\n>>XFE_ThreadView::listChangeStarting, depth=%d, m_lastLoadedInd=%d", 
		   listChangeDepth, m_lastLoadedInd);
#endif

	if (notify == MSG_NotifyScramble ||
		notify == MSG_NotifyAll) {
		m_lastLoadedInd = MSG_VIEWINDEXNONE;
	}/* if */
	else if (notify == MSG_NotifyNone) {
	}/* MSG_NotifyNone */
}
#endif /* DEL_5_0 */

void
XFE_ThreadView::listChangeFinished(XP_Bool /* asynchronous */, 
								   MSG_NOTIFY_CODE notify, 
								   MSG_ViewIndex where, 
								   int32 num)
{
	int listChangeDepth = m_outliner->getListChangeDepth()-1;
#if defined(DEBUG_tao)
	printf("\n>>XFE_ThreadView::listChangeFinished, depth=%d, m_lastLoadedInd=%d", 
		   listChangeDepth, m_lastLoadedInd);
#endif

	if (notify == MSG_NotifyNone && 
		where == 0 && 
		num == 0 &&
		listChangeDepth == 0 &&
		m_deletedCount &&
		m_deleted) {
		processCmdQueue();
	}/* */
	else if (notify == MSG_NotifyScramble ||
			 notify == MSG_NotifyAll) {
		m_lastLoadedInd = MSG_GetMessageIndexForKey(m_pane, 
													m_lastLoadedKey,
													False);
	}/* if */
	else if (notify == MSG_NotifyChanged) {
		m_lineChanged = where; 
		/* when a msg header info changed on certain row,
		   backend will use this notification to warn fe.
	           we are suppose to re-acquire line m_messageLine info				
	           from the backend and invalidate this row on display only
                */
		acquireLineData((int)m_lineChanged); 
		return;
	}/* if */
	else if (notify == MSG_NotifyInsertOrDelete &&
			 m_lastLoadedInd != MSG_VIEWINDEXNONE) {
		/* there was a msg loaded
		 */
		if (num < 0) {
			/* 1. collapsing ??
			 */
			if ((m_lineChanged != MSG_VIEWINDEXNONE) &&
				(m_lineChanged == (where -1)) && /* essential */
				(listChangeDepth == 0)) { /* must be collapsing */
				/* collapsed
				 */
				if ((m_lastLoadedInd >= where) && 
					(m_lastLoadedInd < (where-num))) { 
					/* one of the "reply" is being hidden
					 * selection would go to thread leader; 
					 * thus needs reload right away
					 */
					/* load the thread leader: where -1*/
					if (!m_deletedCount && !m_deleted) {
						m_deleted = (int *) XP_CALLOC(1, sizeof(int));
						m_deleted[m_deletedCount++] = where-1;		
						processCmdQueue();
					}/* if */
#if 0
					else
						XP_ASSERT(0);
#endif 
				}/* if collapsed */
				else {
#if defined(DEBUG_tao)
					printf("\n***XFE_ThreadView::listChangeFinished, collapsing not selected\n");
#endif /* DEBUG_tao */
					m_lastLoadedInd += num; /* re-adjust */
				}/* else */
			}/* collapsed */
			else if (where == m_lastLoadedInd) {
				/* the selected one is gone
				 */
				if (!m_deletedCount && !m_deleted) {
					m_deleted = (int *) XP_CALLOC(1, sizeof(int));
					m_deleted[m_deletedCount++] = where;
					if (listChangeDepth == 0) // non-nested
#if 1
						processCmdQueue();
#endif

				}/* if */
#if 0
				else
					XP_ASSERT(0);
#endif 
			}/* where == m_lastLoadedInd */
			else if (where <= m_lastLoadedInd)
				m_lastLoadedInd += num; /* re-adjust */
		}/* num < 0 */
		else if (num == 0) {
			/* SPECIAL case: the header got deleted
			 */

			/* we do not care deletion on non-focusing line
			 */
			if (m_lastLoadedInd == where) {
				/* deleted
				 */
				if (!m_deletedCount && !m_deleted) {
					m_deleted = (int *) XP_CALLOC(1, sizeof(int));
					m_deleted[m_deletedCount++] = where;		
					processCmdQueue();
				}/* if */
#if 0
				else
					XP_ASSERT(0);
#endif 

#if defined(DEBUG_tao)
				printf("\n**>SPECIAL case:listChangeFinished=%d, m_deletedCount=%d\n", 
					   where, m_deletedCount);
#endif		
			}/* m_lastLoadedInd == where */
		}/* if num == 0 */
		else if (num > 0 && where <= m_lastLoadedInd)
			m_lastLoadedInd += num; /* re-adjust */

	}/* notify == MSG_NotifyInsertOrDelete */
#if defined(DEBUG_tao)
	printf("\n<<XFE_ThreadView::listChangeFinished, m_lastLoadedInd=%d", 
		   m_lastLoadedInd);
#endif

	m_lineChanged = MSG_VIEWINDEXNONE;
}/* XFE_ThreadView::listChangeFinished() */

void
XFE_ThreadView::paneChanged(XP_Bool asynchronous,
							MSG_PANE_CHANGED_NOTIFY_CODE code,
							int32 value)
{
#if defined(DEBUG_tao)
	printf("\n  XFE_ThreadView::paneChanged, pane=0x%x, asynchronous=%d, code=%d, value=%d",
		   m_pane, asynchronous, code, value);
#endif

  switch (code)
    {
	case MSG_PaneNotifyFolderDeleted:
		{
			/* bug 95564: check folderInfo before closing
			 */
			MSG_FolderInfo *deleted = (MSG_FolderInfo *) value;

			DD(printf("Blank out message view...\n");)

#if defined(USE_3PANE)
			if (m_folderInfo == deleted) {
				/* clear msgPane
				 */
				m_msgview->loadMessage(deleted, MSG_MESSAGEKEYNONE );
				showMessage(-1);
				m_outliner->change(0,0,0);
			}/* if m_folderInfo == deleted */
#else
			m_msgview->loadMessage(deleted, MSG_MESSAGEKEYNONE );
			if (m_folderInfo == deleted) {
				/* test frame type
				 */
				XFE_Frame *frame = (XFE_Frame*)m_toplevel;
				if (frame &&
					FRAME_MAILNEWS_THREAD == frame->getType()) {
					/* standalone frame
					 */	
					if (!m_frameDeleted) {
						frame->delete_response();
						m_frameDeleted = True;
					}/* if */
				}/* if */
			}/* if m_folderInfo == deleted */
#endif /* USE_3PANE */

#if defined(DEBUG_tao)
			else
				printf("\n---Wrong folder IGNORing MSG_PaneNotifyFolderDeleted\n");
#endif

		}
		/* shall we update banner or simply return?
		 */
		return;

	case MSG_PaneNotifyCopyFinished:
#if defined(DEBUG_tao)
		printf("\n++XFE_ThreadView, MSG_PaneNotifyCopyFinished++\n");
		/* It happens in loading mails.
		 * XP_ASSERT(0);
		 */ 
#endif
		break;

	case MSG_PaneNotifyMessageLoaded:
#if defined(DEBUG_tao)
		printf("\n++XFE_ThreadView, MSG_PaneNotifyMessageLoaded++\n");
		XP_ASSERT(0);
#endif
		break;

	case MSG_PaneNotifyLastMessageDeleted:
		/* clear msgPane
		 */
		showMessage(-1);
		break;

	case MSG_PaneNotifyMessageDeleted:
		{
		// If we delete a message from the threadview, and
		// there's currently a message view showing that message,
		// we need to tell the msg view so it can pop down:
		XFE_MozillaApp::theApp()->notifyInterested(msgWasDeleted, (void*)value);

#if HANDLE_CMD_QUEUE

#if defined(DEBUG_tao)
		MessageKey id = (MessageKey) value;
		MSG_ViewIndex index = MSG_GetMessageIndexForKey(m_pane, 
														id,
														False);
		printf("\n MSG_PaneNotifyMessageDeleted,index=%d", index);
#endif
#endif /* HANDLE_CMD_QUEUE */

		}
		break;

	case MSG_PaneNotifyFolderLoadedSync:
	case MSG_PaneNotifyFolderLoaded:
	{
		D(printf ("Inside pane changed -- folder loaded\n");)
#if defined(DEBUG_tao)
		if (code == MSG_PaneNotifyFolderLoadedSync)
			printf("\n**XFE_ThreadView::paneChanged: code == MSG_PaneNotifyFolderLoadedSync \n");
#endif
		if (m_nLoadingFolders > 0) {
			/* release the block
			 */
			m_outliner->setBlockSel(False);

			m_nLoadingFolders--;
		}/* if */
		
		/* Call this after folder is loaded
		 */
		if ((code == MSG_PaneNotifyFolderLoaded) &&
			m_getNewMsg) {

			/* Do it for movemail/pop3 only
			 */ 
			if (fe_globalPrefs.mail_server_type != MAIL_SERVER_IMAP)
				getNewMail();

			/* reset
			 */
			m_getNewMsg = False;
		}/* if */

		/* select first msg
		 */
		if (m_commandPending == noPendingCommand
			&& m_outliner->getTotalLines() > 0)
			{
				m_commandPending = selectByIndex;
				m_pendingSelectionIndex = 0;

			/* when a msg being loaded after folder loaded, we should change
			   focus of the three pane UI to thread view bug #109659
 			*/
        		   getToplevel()->notifyInterested(XFE_MNListView::changeFocus, (void*) this);
			/* end of #109659 */
			}

		/* we only actually handle the command here 
		 * if the folder was loaded synchronously. 
		 */
		if (code == MSG_PaneNotifyFolderLoadedSync)
			handlePendingCommand();

		XFE_MozillaApp::theApp()->notifyInterested(XFE_MNView::MNChromeNeedsUpdating);

		notifyInterested(XFE_MNView::bannerNeedsUpdating, (void*)code);

		XP_Bool invalidate_needed = False;
		if (MSG_GetToggleStatus(m_pane, MSG_SortByThread, NULL, 0) == MSG_Checked)
			{
				invalidate_needed = m_outliner->getDescendantSelectAllowed() == False;
				m_outliner->setDescendantSelectAllowed( True );
				m_outliner->setSortColumn(-1, OUTLINER_SortAscending);
			}
		else
			{
				invalidate_needed = m_outliner->getDescendantSelectAllowed() == True;
				m_outliner->setDescendantSelectAllowed( False );
			}

		if (invalidate_needed)
			m_outliner->invalidate();
	}
		break;

	default:
		break;
    }

    // This can result in flickering of the banner, but if we don't call it,
    // the number of unread and total messages isn't updated correctly
    XFE_MNListView::paneChanged(asynchronous, code, value);
}

void
XFE_ThreadView::loadFolder(MSG_FolderInfo *folderInfo)
{
  MSG_FolderLine folderLine;
  char *newsgroup_format = XP_GetString(XFE_WINDOW_TITLE_NEWSGROUP);
  char *folder_format = XP_GetString(XFE_WINDOW_TITLE_FOLDER);
  char *window_title;
  int window_title_length;

  // only need to do everything below here if the folder being
  // loaded is different than what's currently displayed.
  if (m_folderInfo != folderInfo) {
	  
	  // must be valid.
	  XP_ASSERT(folderInfo);
	  
	  // clear the currently display message, since we're in a different folder now.
	  DD(printf("Load Folder: Blank out message view...\n");)
          m_msgview->loadMessage(folderInfo, MSG_MESSAGEKEYNONE );

	  updateExpandoFlippyText(-1);

	  m_outliner->deselectAllItems();
	  
	  MSG_GetFolderLineById(XFE_MNView::getMaster(), folderInfo, &folderLine);

#if defined(USE_MOTIF_DND)
	  m_outliner->enableDragDrop(this,
								 NULL, /* no dropping -- of anything but outliner columns. */
								 XFE_ThreadView::getDragTargets,
								 XFE_ThreadView::getDragIconData,
								 XFE_ThreadView::dragConvert);
#endif /* USE_MOTIF_DND */
	  
	  if (folderLine.flags & MSG_FOLDER_FLAG_NEWSGROUP)
		  {
			  // it's a newsgroup
			  window_title_length = strlen(newsgroup_format) - 2 /* %s */ + strlen(folderLine.name) + 1;
			  
			  window_title = (char*)XP_CALLOC(sizeof(char), window_title_length);
			  
			  sprintf (window_title, newsgroup_format, folderLine.name);
			  
			  m_displayingNewsgroup = TRUE;
			  m_displayingDraft = FALSE;
			  
#if !defined(USE_MOTIF_DND)
			  m_outliner->setDragType( FE_DND_NEWS_MESSAGE, &newsPostIcon,
									   this, (fe_dnd_SourceDropFunc)&XFE_ThreadView::source_drop_func);
#endif /* USE_MOTIF_DND */
			  
		  }
	  else 
		  {
			  window_title_length = strlen(folder_format) - 2 /* %s */ + strlen(folderLine.name) + 1;
			  
			  window_title = (char*)XP_CALLOC(sizeof(char), window_title_length);
			  
			  sprintf (window_title, folder_format, folderLine.name);
					  

			  if (folderLine.flags & MSG_FOLDER_FLAG_DRAFTS)
				  {
					  // it's the draft folder
					  m_displayingDraft = TRUE;
					  m_displayingNewsgroup = FALSE;
#if !defined(USE_MOTIF_DND)

					  m_outliner->setDragType(FE_DND_MAIL_MESSAGE, &mailMessageUnreadIcon,
											  this, 
											  (fe_dnd_SourceDropFunc)&XFE_ThreadView::source_drop_func);
#endif /* USE_MOTIF_DND */
				  }
			  else
				  {
					  // it's a normal mail folder
					  m_displayingNewsgroup = FALSE;
					  m_displayingDraft = FALSE;
					  
#if !defined(USE_MOTIF_DND)
					  m_outliner->setDragType(FE_DND_MAIL_MESSAGE, &mailMessageUnreadIcon, 
											  this,
											  (fe_dnd_SourceDropFunc)&XFE_ThreadView::source_drop_func);
#endif /* USE_MOTIF_DND */
				  }
		  }
	  
	  //  getContainer()->setTitle(window_title);
	  
	  XP_FREE(window_title);
	  
	  D(printf("Loading folder into ThreadView\n");)
		  
	  /* if it doesn't have categories, we can load it directly
		 into the threadview's pane. */
	  m_folderInfo = folderInfo;
			  
	  /* keep track of on loading folders
	   */
	  if (m_nLoadingFolders == 0) {
		  m_nLoadingFolders++; // FYI

		  /* block selection
		   */
		  m_outliner->setBlockSel(True);
	  }/* if */ 
	  else
		  XP_ASSERT(0);

	  MSG_LoadFolder(m_pane, m_folderInfo);


	  XFE_MozillaApp::theApp()->notifyInterested(XFE_MNView::MNChromeNeedsUpdating);
	  /* either open or close the message view, depending on the folder prefs */
	  {
		  int32 new_folder_prefs = MSG_GetFolderPrefFlags(m_folderInfo);
		  
		  if (new_folder_prefs & MSG_FOLDER_PREF_FEVALID)
			  {
				  if (new_folder_prefs & MSG_FOLDER_PREF_ONEPANE)
					  {
						  if (m_msgview->isShown())
							  {
								  toggleMsgExpansion();
							  }
					  }
				  else
					  {
						  if (!m_msgview->isShown())
							  {
								  toggleMsgExpansion();
							  }
					  }
			  }
	  }
  }/* if  diff folder */
  else  /* if  the same loaded folder, we need to invoke handlePendingCommand */
	handlePendingCommand();
}

MSG_FolderInfo *
XFE_ThreadView::getFolderInfo()
{
  return m_folderInfo;
}

void
XFE_ThreadView::updateExpandoFlippyText(int row)
{
	char *subjectStr;
	XmString subject;
	XmString oldsubject = 0;
	char *oldsubjectStr = NULL;
	XP_Bool needsChange = TRUE;

#if defined(DEBUG_tao)
	printf("\nXFE_ThreadView::updateExpandoFlippyText(row=%d)\n",row);
#endif
	if (row == -1)
		{
			subjectStr = "";
		}
	else
		{
			acquireLineData(row);
			
			subjectStr = getColumnText(OUTLINER_COLUMN_SUBJECT);
		}      
	
	XtVaGetValues (m_arrowlabel, 
				   XmNlabelString, &oldsubject,
				   NULL);
	
	if (XmStringGetLtoR(oldsubject, XmFONTLIST_DEFAULT_TAG, &oldsubjectStr))
		if (oldsubjectStr && !strcmp(oldsubjectStr, subjectStr)) 
			{
				needsChange = FALSE;
				if (oldsubjectStr) XtFree(oldsubjectStr);
			}
	
	if (needsChange) 
		{
			subject = XmStringCreate (subjectStr, XmFONTLIST_DEFAULT_TAG);
			
			XtVaSetValues (m_arrowlabel, 
						   XmNlabelString, subject, 
						   NULL);
			
			XmStringFree (subject);
		}
	
	XmStringFree (oldsubject);
	XFlush(XtDisplay(m_arrowlabel));
}

//
// show a message at a particular row in the thread's outliner.
// sending -1 to this function will clear the label and the message
// area.
//
void
XFE_ThreadView::showMessage(int row)
{
	/* sometimes this can get called before we've loaded a folder. */
	if (!m_folderInfo)
		return;

	if (row > -1) {
		m_lastLoadedInd = (MSG_ViewIndex) row;

		m_lastLoadedKey = MSG_GetMessageKey(m_pane, row);
#if defined(DEBUG_tao)
		printf("\nXFE_ThreadView::showMessage m_lastLoadedInd=%d\n", 
			   m_lastLoadedInd);
#endif	
	}/* if */

	/*
	** if the message area is expanded we just load the message, since
	** we will turn around and call the makeVisible/selectItemExclusive/etc
	** in our newMessageLoading method.
	** If the message area isn't expanded, we handle it here.
	*/
	if (m_msgExpanded)
		{
			MSG_ViewIndex index;
			MessageKey key;
			MSG_FolderInfo *info = NULL;
			
			MSG_GetCurMessage(m_msgview->getPane(), &info, &key, &index);
			if (key != m_lastLoadedKey) {
				m_msgview->loadMessage(m_folderInfo,
									   (row == -1 ? 
										MSG_MESSAGEKEYNONE : 
										MSG_GetMessageKey(m_pane, row)));
			}/* if */
		}
	else
		{
			if (row != -1)
				{
					m_outliner->makeVisible(row);
					m_outliner->selectItemExclusive(row);
				}
		}
	updateExpandoFlippyText(row);
}

char *
XFE_ThreadView::commandToString(CommandType cmd, void * calldata, XFE_CommandInfo* info)
{
#define IS_CMD(command) cmd == (command)  
	if (IS_CMD(xfeCmdToggleMessageExpansion))
		{
			if (m_msgview->isShown())
				return stringFromResource("hideMsgAreaCmdString");
			else
				return stringFromResource("showMsgAreaCmdString");
		}
    else if (IS_CMD(xfeCmdDeleteAny) || IS_CMD(xfeCmdDeleteMessage) || IS_CMD(xfeCmdCancelMessages)) {
		if (isDisplayingNews())
			return XP_GetString(MK_MSG_CANCEL_MESSAGE);
		else
			return XP_GetString(MK_MSG_DELETE_SEL_MSGS);			
	}
    else if ( calldata &&
              (IS_CMD(xfeCmdReplyToSender)
               || IS_CMD(xfeCmdReplyToAll)
               || IS_CMD(xfeCmdReplyToNewsgroup)
               || IS_CMD(xfeCmdReplyToSenderAndNewsgroup) ) )
    {
        // if calldata is non-null for a Reply button,
        // that's a signal not to change the
        // widget name in the resources file:
        return 0;
    }

    else
		return XFE_MNListView::commandToString(cmd, calldata, info);
  
#undef IS_CMD
}

Boolean
XFE_ThreadView::isCommandSelected(CommandType cmd, void *calldata, XFE_CommandInfo*)
{
#define IS_CMD(command) cmd == (command)
  if (IS_CMD(xfeCmdShowAllHeaders)
	   || IS_CMD(xfeCmdShowNormalHeaders)
	   || IS_CMD(xfeCmdShowBriefHeaders)
	   || IS_CMD(xfeCmdWrapLongLines)
	   || IS_CMD(xfeCmdViewAttachmentsInline)
       || IS_CMD(xfeCmdEditMessage)
       || IS_CMD(xfeCmdOpenSelected)
       || IS_CMD(xfeCmdUpdateMessageCount)
	   || IS_CMD(xfeCmdViewAttachmentsAsLinks))
    {
      return m_msgExpanded && m_msgview->isCommandSelected(cmd);
    }
  else if (IS_CMD(xfeCmdPrintSetup)
           || IS_CMD(xfeCmdPrintPreview))
      return True;
  else if (IS_CMD(xfeCmdSortAscending))
  {
      return ( MSG_GetToggleStatus( m_pane, MSG_SortBackward, NULL, 0 )
               != MSG_Checked );
  }
  else if (IS_CMD(xfeCmdSortDescending))
  {
      return ( MSG_GetToggleStatus( m_pane, MSG_SortBackward, NULL, 0 )
               == MSG_Checked );
  }
  else
    return XFE_MNListView::isCommandSelected(cmd, calldata);
#undef IS_CMD
}

Boolean
XFE_ThreadView::isCommandEnabled(CommandType cmd, void *calldata, XFE_CommandInfo* info)
{
#define IS_CMD(command) cmd == (command)

	if (IS_CMD(xfeCmdDeleteAny)|| IS_CMD(xfeCmdDeleteMessage)) {
		/* Given a flag or set of flags, returns the number of folders that have
		 *  that flag set.  If the result pointer is not NULL, fills it in with the
		 *  list of folders (providing up to resultsize entries).  
		 */
		int32 resultsize = MSG_GetFoldersWithFlag(XFE_MNView::getMaster(),
												  MSG_FOLDER_FLAG_TRASH,
												  NULL,
												  0);
		if (resultsize == 0)
			return False;
	}/* IS_CMD(xfeCmdDeleteMessage */

    const int *selected;
    int count;
    m_outliner->getSelection(&selected, &count);

#if defined(USE_ABCOM)
	if ((IS_CMD(xfeCmdAddSenderToAddressBook)||
		 IS_CMD(xfeCmdAddAllToAddressBook)) &&
		count > 0) {

		XP_List *abList = AB_AcquireAddressBookContainers(m_contextData);
		XP_ASSERT(abList);
		int nDirs = XP_ListCount(abList);

		XP_Bool selectable = False;
		if (!nDirs)
			return selectable;
		
		/* XFE always returns the first one
		 */
		AB_ContainerInfo *destAB = 
			(AB_ContainerInfo *) XP_ListGetObjectNum(abList,1);
		XP_ASSERT(destAB);

		int error = MSG_AddToAddressBookStatus(m_pane,
											   commandToMsgCmd(cmd),
											   (MSG_ViewIndex*)selected,
											   count,
											   &selectable, NULL, NULL, NULL,
											   destAB);
		error = AB_ReleaseContainersList(abList);

		return selectable;	
	}/* if */
#endif /* USE_ABCOM */

    // DeleteMessage stands for CancelMessage in the ThreadView,
    // so intercept it early:
    if ( (IS_CMD(xfeCmdDeleteAny) || 
	 IS_CMD(xfeCmdDeleteMessage) || IS_CMD(xfeCmdCancelMessages))
              && isDisplayingNews() )
    {
      XP_Bool selectable = False;
      MSG_CommandStatus(m_pane, MSG_CancelMessage,
                        /* just need some integer pointer here...*/
                        (MSG_ViewIndex*)selected, count,
                        &selectable, NULL, NULL, NULL);
      return selectable;
    }

    // Check if selectThread should be enabled or not...
    if ( IS_CMD(xfeCmdSelectThread))
    {
        Boolean status =
                MSG_GetToggleStatus(m_pane, MSG_SortByThread, NULL, 0)==MSG_Checked;
        return ((count > 0) && status);
    }
    if ( !m_displayingNewsgroup && IS_CMD(xfeCmdEditConfiguration) )
    {
        // Manage Mail Account
        return True;
    }
    if ( m_displayingNewsgroup && IS_CMD(xfeCmdModerateDiscussion) )
    {
        // Moderate Newsgroup Discussion
        return True;
    }
 	if (IS_CMD(xfeCmdUndo)
 		|| IS_CMD(xfeCmdRedo)) {
 		XP_Bool selectable = False;
 		MSG_CommandType msg_cmd = commandToMsgCmd(cmd);
 		MSG_CommandStatus(m_pane, msg_cmd,
 						  /* just need some integer pointer here...*/
 						  (MSG_ViewIndex*)selected, count,
 						  &selectable, NULL, NULL, NULL);

#if defined(DEBUG_tao_)
 		if (IS_CMD(xfeCmdUndo))
 			printf("\n XFE_ThreadView, MSG_CommandStatus:: m_pane=%x, xfeCmdUndo=%d", m_pane, selectable);
 		else if (IS_CMD(xfeCmdRedo))
 			printf("\n XFE_ThreadView, MSG_CommandStatus:: m_pane=%x, xfeCmdRedo=%d", m_pane, selectable);
#endif
       return selectable;
     }
 

  MSG_MotionType nav_cmd;

  nav_cmd = commandToMsgNav(cmd);

  if ((nav_cmd != (MSG_MotionType)~0)&&
	  (!(IS_CMD(xfeCmdBack)||IS_CMD(xfeCmdForward))))
    {
      XP_Bool selectable = FALSE;
      if (count == 1)
		  MSG_NavigateStatus(m_pane, nav_cmd, selected[0],
							 &selectable, NULL);

      return selectable;
    }

  else if (IS_CMD(xfeCmdToggleMessageExpansion))
	  {
		  return True;
	  }
  else if (IS_CMD(xfeCmdGetNextNNewMsgs))
	  {
		  return m_displayingNewsgroup;
	  }
  else if (IS_CMD(xfeCmdEditPreferences))
    {
      return True;
    }
  //  Always available even if nothing is selected:
  else if (IS_CMD(xfeCmdComposeArticle)
           || IS_CMD(xfeCmdComposeArticleHTML)
           || IS_CMD(xfeCmdComposeArticlePlain)
           || IS_CMD(xfeCmdPrintSetup)
           || IS_CMD(xfeCmdPrintPreview)
           || IS_CMD(xfeCmdUpdateMessageCount))
	{
	  return True;
	}

  // the move and copy commands are available if there are messages selected.
  else if (IS_CMD(xfeCmdMoveMessage)
	   || IS_CMD(xfeCmdCopyMessage)
	   || IS_CMD(xfeCmdSetPriorityHighest)
	   || IS_CMD(xfeCmdSetPriorityHigh)
	   || IS_CMD(xfeCmdSetPriorityNormal)
	   || IS_CMD(xfeCmdSetPriorityLow)
	   || IS_CMD(xfeCmdSetPriorityLowest)
	   || IS_CMD(xfeCmdSetPriorityNone)
       || IS_CMD(xfeCmdEditMessage)
       || IS_CMD(xfeCmdOpenSelected))
    {
      return (count > 0);
    }
  // the sort commands are always available, if there are messages
  else if (IS_CMD(xfeCmdSortBySender)
		   || IS_CMD(xfeCmdSortByDate)
		   || IS_CMD(xfeCmdSortBySubject)
		   || IS_CMD(xfeCmdSortByPriority)
		   || IS_CMD(xfeCmdSortByThread)
		   || IS_CMD(xfeCmdSortByMessageNumber)
		   || IS_CMD(xfeCmdSortBySize)
		   || IS_CMD(xfeCmdSortByFlag)
		   || IS_CMD(xfeCmdSortByStatus)
		   || IS_CMD(xfeCmdSortByUnread)
		   || IS_CMD(xfeCmdSortAscending)
		   || IS_CMD(xfeCmdSortDescending)
		   || IS_CMD(xfeCmdMarkMessageByDate))
    {
      return MSG_GetNumLines(m_pane) > 0;
    }
  // commands that work on the msgview.
  else if (IS_CMD(xfeCmdShowAllHeaders)
		   || IS_CMD(xfeCmdPrint)
		   || IS_CMD(xfeCmdShowNormalHeaders)
		   || IS_CMD(xfeCmdShowBriefHeaders)
		   || IS_CMD(xfeCmdWrapLongLines)
		   || IS_CMD(xfeCmdViewAttachmentsInline)
		   || IS_CMD(xfeCmdViewAttachmentsAsLinks)
		   || IS_CMD(xfeCmdViewPageSource)
		   || IS_CMD(xfeCmdFindInObject)
		   || IS_CMD(xfeCmdFindAgain))
    {
      return m_msgExpanded && m_msgview->isCommandEnabled(cmd);
    }

  else if (IS_CMD(xfeCmdSearch))
	  {
		  return !XP_IsContextBusy(m_contextData);
	  }
  else if (IS_CMD(xfeCmdExpand)
           || IS_CMD(xfeCmdExpandAll)
           || IS_CMD(xfeCmdCollapse)
           || IS_CMD(xfeCmdCollapseAll))
      return True;
  else
    {
      return XFE_MNListView::isCommandEnabled(cmd, calldata, info);
    }
#undef IS_CMD
}

Boolean
XFE_ThreadView::handlesCommand(CommandType cmd, void *calldata, XFE_CommandInfo*)
{
#define IS_CMD(command) cmd == (command)
  if (IS_CMD(xfeCmdToggleMessageExpansion)
	  || IS_CMD(xfeCmdGetNewMessages)
	  || IS_CMD(xfeCmdGetNextNNewMsgs)
      || IS_CMD(xfeCmdAddSenderToAddressBook)
      || IS_CMD(xfeCmdAddAllToAddressBook)
      || IS_CMD(xfeCmdEditMessage)
	  || IS_CMD(xfeCmdSendMessagesInOutbox)
      || IS_CMD(xfeCmdPrint)
	  || IS_CMD(xfeCmdSaveAs)
      || IS_CMD(xfeCmdReplyToSender)
	  || IS_CMD(xfeCmdReplyToSenderAndNewsgroup)
      || IS_CMD(xfeCmdReplyToAll)
      || IS_CMD(xfeCmdForwardMessage)
      || IS_CMD(xfeCmdForwardMessageQuoted)
      || IS_CMD(xfeCmdForwardMessageInLine)
      || IS_CMD(xfeCmdNextMessage)
      || IS_CMD(xfeCmdNextUnreadMessage)
      || IS_CMD(xfeCmdPreviousMessage)
      || IS_CMD(xfeCmdPreviousUnreadMessage)
      || IS_CMD(xfeCmdDeleteAny)
      || IS_CMD(xfeCmdDeleteMessage)
      || IS_CMD(xfeCmdSortBySender)
      || IS_CMD(xfeCmdSortByDate)
      || IS_CMD(xfeCmdSortBySubject)
      || IS_CMD(xfeCmdSortByPriority)
      || IS_CMD(xfeCmdSortByThread)
      || IS_CMD(xfeCmdSortByMessageNumber)
      || IS_CMD(xfeCmdSortBySize)
      || IS_CMD(xfeCmdSortByFlag)
      || IS_CMD(xfeCmdSortByStatus)
      || IS_CMD(xfeCmdSortByUnread)
      || IS_CMD(xfeCmdSortAscending)
      || IS_CMD(xfeCmdSortDescending)
	  || IS_CMD(xfeCmdViewNew)
      || IS_CMD(xfeCmdViewAllThreads)
      || IS_CMD(xfeCmdViewThreadsWithNew)
      || IS_CMD(xfeCmdViewWatchedThreadsWithNew)
      || IS_CMD(xfeCmdShowAllHeaders)
      || IS_CMD(xfeCmdShowNormalHeaders)
      || IS_CMD(xfeCmdShowBriefHeaders)
      || IS_CMD(xfeCmdViewAttachmentsInline)
      || IS_CMD(xfeCmdViewAttachmentsAsLinks)
      || IS_CMD(xfeCmdRot13Message)
      || IS_CMD(xfeCmdMarkMessageRead)
      || IS_CMD(xfeCmdMarkMessageUnread)
      || IS_CMD(xfeCmdMarkMessageByDate)
      || IS_CMD(xfeCmdMarkThreadRead)
      || IS_CMD(xfeCmdMarkAllMessagesRead)
      || IS_CMD(xfeCmdMarkMessage)
      || IS_CMD(xfeCmdUnmarkMessage)
      || IS_CMD(xfeCmdToggleThreadKilled)
      || IS_CMD(xfeCmdToggleThreadWatched)
      || IS_CMD(xfeCmdRenameFolder)
      || IS_CMD(xfeCmdSearch)
      || IS_CMD(xfeCmdSearchAddress)
      || IS_CMD(xfeCmdEditPreferences)
      || IS_CMD(xfeCmdWrapLongLines)

      /* priorities. */
      || IS_CMD(xfeCmdSetPriorityHighest)
      || IS_CMD(xfeCmdSetPriorityHigh)
      || IS_CMD(xfeCmdSetPriorityNormal)
      || IS_CMD(xfeCmdSetPriorityLow)
      || IS_CMD(xfeCmdSetPriorityLowest)
      || IS_CMD(xfeCmdSetPriorityNone)

      /* we handle these command to place more constraints on it being sensitive. */
      || IS_CMD(xfeCmdViewPageSource)
	  || IS_CMD(xfeCmdFindInObject)
	  || IS_CMD(xfeCmdFindAgain)

      || IS_CMD(xfeCmdShowPopup)
      || IS_CMD(xfeCmdModerateDiscussion)

      || IS_CMD(xfeCmdEditMessage)
      || IS_CMD(xfeCmdOpenSelected)
      || IS_CMD(xfeCmdUpdateMessageCount)
      || IS_CMD(xfeCmdNewFolder)
      || IS_CMD(xfeCmdCompressAllFolders)
      || IS_CMD(xfeCmdCompressFolders)
      || IS_CMD(xfeCmdAddNewsgroup)
      || IS_CMD(xfeCmdPrintSetup)
      || IS_CMD(xfeCmdPrintPreview)
      || IS_CMD(xfeCmdExpand)
      || IS_CMD(xfeCmdExpandAll)
      || IS_CMD(xfeCmdCollapse)
      || IS_CMD(xfeCmdCollapseAll)
      || IS_CMD(xfeCmdToggleKilledThreads)
      || IS_CMD(xfeCmdSelectThread)
      || IS_CMD(xfeCmdEditConfiguration)
      || IS_CMD(xfeCmdModerateDiscussion)
  	|| IS_CMD(xfeCmdMommy))
    {
      return TRUE;
    }
  else
    {
      return XFE_MNListView::handlesCommand(cmd, calldata);
    }
#undef IS_CMD
}

void
XFE_ThreadView::addCollapsed(int where)
{

	if (m_collapsedCount && m_collapsed)
		m_collapsed = (int *) XP_REALLOC(m_collapsed, 
									   (m_collapsedCount+1)*sizeof(int));
	else
		m_collapsed = (int *) XP_CALLOC(1, sizeof(int));
	m_collapsed[m_collapsedCount++] = where+1;
#if defined(DEBUG_tao)
	printf("\n-->addCollapsed=%d, m_collapsedCount=%d\n", 
		   where+1, m_collapsedCount);
#endif		
}

XP_Bool
XFE_ThreadView::isCollapsed(int where)
{
	if (!m_collapsedCount || !m_collapsed)
		return False;

	for (int i=0; i < m_collapsedCount; i++)
		if (m_collapsed[i] == where) {
			/* clear it
			 * thread children's pos > 0
			 */
			m_collapsed[i] = 0-where;
			return True;
		}/* for i */
	return False;
}

void
XFE_ThreadView::doCommand(CommandType cmd,
						  void *calldata, XFE_CommandInfo* info)
{
#define IS_CMD(command) cmd == (command)
  const int *selected;
  int count;
  
  m_outliner->getSelection(&selected, &count);

  if (IS_CMD(xfeCmdToggleMessageExpansion))
	  {
		  toggleMsgExpansion();
		  getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
	  }
#if defined(USE_ABCOM)
  else if ((IS_CMD(xfeCmdAddSenderToAddressBook)||
			IS_CMD(xfeCmdAddAllToAddressBook)) &&
		   count > 0) {

	  XP_List *abList = AB_AcquireAddressBookContainers(m_contextData);
	  XP_ASSERT(abList);
	  int nDirs = XP_ListCount(abList);
	  
	  if (nDirs) {
		  /* XFE always returns the first one
		   */
		  AB_ContainerInfo *destAB = 
			  (AB_ContainerInfo *) XP_ListGetObjectNum(abList,1);
		  XP_ASSERT(destAB);

		  int error = MSG_AddToAddressBook(m_pane,
										   commandToMsgCmd(cmd),
										   (MSG_ViewIndex*)selected,
										   count,
										   destAB);
		  error = AB_ReleaseContainersList(abList);
	  }/* if */
  }/* if */
#endif /* USE_ABCOM */
  else if (IS_CMD(xfeCmdMommy))
	  {
		  fe_showFoldersWithSelected(XtParent(getToplevel()->getBaseWidget()),
									 /* Tao: we might need to check if this returns a 
									  * non-NULL frame
									  */
									 ViewGlue_getFrame(m_contextData),
									 NULL,
									 m_folderInfo);
	  }
  else if (IS_CMD(xfeCmdShowPopup))
    {
		XEvent *event = info->event;
		int x, y, clickrow;
		Widget w = XtWindowToWidget(event->xany.display, event->xany.window);
		
		if (m_popup)
			delete m_popup;

		if (w == NULL) w = m_widget;
		
		/* Need to make the view that has the pop up the current focus so that menu items
		   can be enabled on the context menu again */
        	getToplevel()->notifyInterested(XFE_MNListView::changeFocus, (void*) this);
		m_popup = new XFE_PopupMenu("popup",(XFE_Frame*)m_toplevel, // XXXXXXX
						XfeAncestorFindApplicationShell(w));
		
		if (m_displayingNewsgroup)
			m_popup->addMenuSpec(news_popup_spec);
		else
			m_popup->addMenuSpec(mail_popup_spec);

		m_outliner->translateFromRootCoords(event->xbutton.x_root,
											event->xbutton.y_root,
											&x, &y);
      
		clickrow = m_outliner->XYToRow(x, y);
		
		if (clickrow != -1) /* if it was actually in the outliner's content rows. */
			{
				if (! m_outliner->isSelected(clickrow))
				{
					m_outliner->selectItemExclusive(clickrow);
					showMessage(clickrow);				
				}
			}

		m_popup->position(event);
		
		m_popup->show();
    }
  else if (IS_CMD(xfeCmdGetNewMessages))
	  {
		  getNewMail();
	  }
	else if (IS_CMD(xfeCmdAddNewsgroup))
		{
			// Code pasted from FolderView.cpp
			D(	printf ("MSG_FolderPane::addNewsgroup()\n");)

			/* We can use folder info in ThreadView to get to Host */
			MSG_Host *host = NULL;
			host = MSG_GetHostForFolder(m_folderInfo);
			
			fe_showSubscribeDialog(getToplevel(), getToplevel()->getBaseWidget(), m_contextData, host);
		}
  else if (IS_CMD(xfeCmdGetNextNNewMsgs))
	  {
		  getNewNews();
	  }
  else if (IS_CMD(xfeCmdMarkMessageByDate))
	  {
		  markReadByDate();
	  }
  else if (IS_CMD(xfeCmdEditPreferences))
	  {
		  fe_showMailNewsPreferences(getToplevel(), m_contextData);
	  }
  else if (IS_CMD(xfeCmdShowAllHeaders)
		   || IS_CMD(xfeCmdPrint)
		   || IS_CMD(xfeCmdShowNormalHeaders)
		   || IS_CMD(xfeCmdShowBriefHeaders)
		   || IS_CMD(xfeCmdWrapLongLines)
		   || IS_CMD(xfeCmdViewAttachmentsInline)
		   || IS_CMD(xfeCmdViewAttachmentsAsLinks)
		   /* pass to msgview
			*/
		   || IS_CMD(xfeCmdBack)
		   || IS_CMD(xfeCmdForward))
	  {
		  m_msgview->doCommand(cmd);
	  }
  else if (IS_CMD(xfeCmdSetPriorityHighest)
		   || IS_CMD(xfeCmdSetPriorityHigh)
		   || IS_CMD(xfeCmdSetPriorityNormal)
		   || IS_CMD(xfeCmdSetPriorityLow)
		   || IS_CMD(xfeCmdSetPriorityLowest)
		   || IS_CMD(xfeCmdSetPriorityNone))
	  {
		  MSG_PRIORITY priority = commandToPriority(cmd);
		  MessageKey key;
		  int i;
		  
		  for (i = 0; i < count; i ++)
			  {
				  key = MSG_GetMessageKey(m_pane, (MSG_ViewIndex)selected[i]);
				  
				  MSG_SetPriority(m_pane, key, priority);
			  }
	  }
  else if (IS_CMD(xfeCmdMoveMessage))
	  {
		  MSG_FolderInfo *info = (MSG_FolderInfo*)calldata;
		  
		  if (info)
		  {
			MSG_FolderLine folderLine;
		  	MSG_GetFolderLineById(XFE_MNView::getMaster(), info, &folderLine);

			if ( folderLine.flags & MSG_FOLDER_FLAG_NEWSGROUP)
			{
			   MSG_CopyMessagesIntoFolder(m_pane, (MSG_ViewIndex*)selected, count, info);
			}
			else
			{
		 	   MSG_MoveMessagesIntoFolder(m_pane, (MSG_ViewIndex*)selected, count, info);
			}
		  }
	  }
  else if (IS_CMD(xfeCmdCopyMessage))
    {
      MSG_FolderInfo *info = (MSG_FolderInfo*)calldata;

      if (info)
	MSG_CopyMessagesIntoFolder(m_pane, (MSG_ViewIndex*)selected, count, info);
    }
  else if (isDisplayingNews()
      && (IS_CMD(xfeCmdDeleteMessage) || IS_CMD(xfeCmdCancelMessages) ||
	IS_CMD(xfeCmdDeleteAny) ))
    {
        // If this is a news article, then Delete Message
        // is really Cancel Message:
        MSG_Command(m_pane, MSG_CancelMessage,
                    (MSG_ViewIndex*)selected, count);
    }
  else if (IS_CMD(xfeCmdExpand))
    { 
        MSG_MessageLine mline;
        MSG_GetThreadLineByIndex(m_pane, (MSG_ViewIndex)selected[0], 1,
                                 &mline);
        if ((mline.numChildren > (uint16)0)
            && ((mline.flags & MSG_FLAG_ELIDED) != (uint32)0))
            MSG_ToggleExpansion(m_pane, (MSG_ViewIndex)selected[0], NULL);
    }
  else if (IS_CMD(xfeCmdCollapse))
    {
        MSG_MessageLine mline;
        MSG_GetThreadLineByIndex(m_pane, (MSG_ViewIndex)selected[0], 1,
                                 &mline);
        if ((mline.numChildren > (uint16)0)
            && ((mline.flags & MSG_FLAG_ELIDED) == (uint32)0)) {
			if (mline.numChildren == 1)
				addCollapsed(selected[0]);
            MSG_ToggleExpansion(m_pane, (MSG_ViewIndex)selected[0], NULL);
		}/* if */
    }
  else if (IS_CMD(xfeCmdExpandAll))
    {
        // Note that in the following loop, getTotalLines()
        // may grow each time we expand, but that's okay.
        int i;
        for (i=0; i < getOutliner()->getTotalLines(); ++i)
        {
            MSG_MessageLine mline;
            MSG_GetThreadLineByIndex(m_pane, i, 1, &mline);
            if ((mline.numChildren != (uint16)0)
                && ((mline.flags & MSG_FLAG_ELIDED) != (uint32)0))
                MSG_ToggleExpansion(m_pane, (MSG_ViewIndex)i, NULL);
        }
    }
  else if (IS_CMD(xfeCmdCollapseAll))
    {
        // Note that in the following loop, getTotalLines()
        // may shrink each time we collapse, but that's okay.
        int i;
        for (i=0; i < getOutliner()->getTotalLines(); ++i)
        {
            MSG_MessageLine mline;
            MSG_GetThreadLineByIndex(m_pane, i, 1, &mline);
            if (mline.numChildren != (uint16)0)
            {
                // Now collapse the parent:
                if ((mline.flags & MSG_FLAG_ELIDED) == (uint32)0) {
					if (mline.numChildren == 1)
						addCollapsed(i);
                    MSG_ToggleExpansion(m_pane, (MSG_ViewIndex)i, NULL);
				}/* if */
            }
        }
    }
  else if (IS_CMD(xfeCmdDeleteMessage) || IS_CMD(xfeCmdDeleteAny) )
	  {
		  if (count == 1)
			  /* stop loading since we are deleting this message:
			   * shall we check if it is stopable?
			   */
			  XP_InterruptContext(m_contextData);

			  {
				  MSG_Command(m_pane, MSG_DeleteMessage, (MSG_ViewIndex*)selected, count);
			  }

		  XFE_MozillaApp::theApp()->
			  notifyInterested(XFE_MNView::folderChromeNeedsUpdating, 
							   getFolderInfo());
	  }
  else if (IS_CMD(xfeCmdEditMailFilterRules))
	  {
		  fe_showMailFilterDlg(getToplevel()->getBaseWidget(), 
							   m_contextData,m_pane);
	  }
  else if (IS_CMD(xfeCmdSearchAddress))
	  {
		  fe_showLdapSearch(XfeAncestorFindApplicationShell(getToplevel()->getBaseWidget()),
							/* Tao: we might need to check if this returns a 
							 * non-NULL frame
							 */
							ViewGlue_getFrame(m_contextData),
							(Chrome*)NULL);
	
	  }
  else if (IS_CMD(xfeCmdSelectThread))
  {
        // Call Select Thread
        selectThread();
  }
  else if (IS_CMD(xfeCmdViewNew)
		   || IS_CMD(xfeCmdViewThreadsWithNew)
		   || IS_CMD(xfeCmdViewWatchedThreadsWithNew)
		   || IS_CMD(xfeCmdViewAllThreads)
		   || IS_CMD(xfeCmdToggleKilledThreads)
		   || IS_CMD(xfeCmdSortBySender)
		   || IS_CMD(xfeCmdSortByDate)
		   || IS_CMD(xfeCmdSortBySubject)
		   || IS_CMD(xfeCmdSortByPriority)
		   || IS_CMD(xfeCmdSortByThread)
		   || IS_CMD(xfeCmdSortByMessageNumber)
		   || IS_CMD(xfeCmdSortBySize)
		   || IS_CMD(xfeCmdSortByFlag)
		   || IS_CMD(xfeCmdSortByStatus)
		   || IS_CMD(xfeCmdSortByUnread))
	  {
		  D(printf ("Changing view.\n");)
		  MSG_CommandType msg_cmd = commandToMsgCmd(cmd);

		  if (msg_cmd != (MSG_CommandType)~0)
			  {
				  MSG_Command(m_pane, msg_cmd, (MSG_ViewIndex*)selected, count);
				  
				  /* make sure one of the selected items is still visible. */
				  m_outliner->getSelection(&selected, &count);
				  if (count > 0)
					  m_outliner->makeVisible(selected[0]);
                  if (IS_CMD(xfeCmdSortByMessageNumber))
                      m_outliner->setSortColumn(-1, m_outliner->getSortDirection());
			  }
	  }
  else if (IS_CMD(xfeCmdOpenSelected))
	  {
		  int i;
		  MessageKey key;

		  for (i = 0; i < count; i ++)
			  {
				  key = MSG_GetMessageKey(m_pane, (MSG_ViewIndex)selected[i]);
				  
				  fe_showMsg(XtParent(getToplevel()->getBaseWidget()),
							 /* Tao: we might need to check if this returns a 
							  * non-NULL frame
							  */
							 ViewGlue_getFrame(m_contextData),
							 NULL, m_folderInfo, key, FALSE);
			  }
	  }
  else if (IS_CMD(xfeCmdSearch))
	  {
		  // We don't need to call XtParent on the base widget because
		  // the base widget is the real toplevel widget already...dora 12/31/96
		  fe_showMNSearch(XfeAncestorFindApplicationShell(getToplevel()->getBaseWidget()),
						  /* Tao: we might need to check if this returns a 
						   * non-NULL frame
						   */
						  ViewGlue_getFrame(m_contextData),
						  NULL, this, m_folderInfo);
	  }
  else if (IS_CMD(xfeCmdSortDescending))
	  {
		  if ( MSG_GetToggleStatus( m_pane, MSG_SortBackward, NULL, 0  )
			   != MSG_Checked )
			  {
				  MSG_Command(m_pane, MSG_SortBackward, NULL, 0);

				  /* make sure one of the selected items is still visible. */
				  m_outliner->getSelection(&selected, &count);
				  if (count > 0)
					  m_outliner->makeVisible(selected[0]);
			  }
	  }
  else if (IS_CMD(xfeCmdSortAscending))
	  {
		  if ( MSG_GetToggleStatus( m_pane, MSG_SortBackward, NULL, 0 )
			   == MSG_Checked )
			  {
				  MSG_Command(m_pane, MSG_SortBackward, NULL, 0);
				  
				  /* make sure one of the selected items is still visible. */
				  m_outliner->getSelection(&selected, &count);
				  if (count > 0)
					  m_outliner->makeVisible(selected[0]);
			  }
	  }
  else 
	  {
		  MSG_CommandType msg_cmd = commandToMsgCmd(cmd);
		  MSG_MotionType nav_cmd = commandToMsgNav(cmd);
		  
		  if (info) {
			  CONTEXT_DATA(m_contextData)->stealth_cmd =
				  ((info->event->type == ButtonRelease) &&
				   (info->event->xkey.state & ShiftMask));
		  }

		  if (nav_cmd == (MSG_MotionType)~0)
			  {
				  if ((msg_cmd == (MSG_CommandType)~0) ||
					  (msg_cmd == MSG_MailNew) ||
					  (msg_cmd == MSG_PostNew) )
					  {
						  XFE_MNListView::doCommand(cmd, calldata, info);
					  }
				  else {
					  MSG_Command(m_pane, msg_cmd, (MSG_ViewIndex*)selected, count);

					  /* single pane; count ==1
					   */
					  if (IS_CMD(xfeCmdUndo) 
						  || IS_CMD(xfeCmdRedo)) {
						  MessageKey key = MSG_MESSAGEKEYNONE;
						  MSG_FolderInfo *folder = NULL;
 						  
						  if (!m_pane || !m_outliner) 
							  return;
 							  
						  UndoStatus undoStatus = 
							  MSG_GetUndoStatus(m_pane); 
						  
						  if ( UndoComplete == undoStatus ) {
							  if (MSG_GetUndoMessageKey(m_pane, 
														&folder, &key) 
								  && folder) {
								  if (m_folderInfo == folder) {
 									  /* same folder
 									   * select the message right away!
 									   */
 									  MSG_ViewIndex index = 
 										  MSG_GetMessageIndexForKey(m_pane, 
 																	key, 
 																	TRUE);
 									  if (index != MSG_VIEWINDEXNONE) {
 										  showMessage(index);
									  }
									  
 								  }/* if */
 								  else {
#if HANDLE_CMD_QUEUE
 									  /* need to load new folder 
 									   */
									  /* XFE: let handlePendingCmd() deal with 
									   * this part
									   */
									  m_commandPending = selectByKey;
									  m_pendingSelectionKey = key;
									  loadFolder(folder);
#endif /* HANDLE_CMD_QUEUE */
 								  }/* if */
 							  }/* if */
 						  }/* if UndoComplete == undoStatus */
 					  }/* if */
 				  }/* else */
				  
			  }
		  else
			  {
				  MSG_ViewIndex index, threadIndex;
				  MessageKey resultId;
				  MSG_FolderInfo *new_finfo;

				  if (count == 1)
					  {
						  m_outliner->setBlockSel(True);
						  MSG_ViewNavigate(m_pane, nav_cmd, selected[0], 
										   &resultId, &index, &threadIndex, 
										   &new_finfo);
						  m_outliner->setBlockSel(False);
						  
						  // ViewNavigate gives a NULL folderinfo if the folder
						  // info won't be changing.
						  if (new_finfo &&
							  new_finfo != m_folderInfo)
							  {
								  /* insert here any navigational commands that can span folders, and
									 add corresponding entry in pendingCommand */
								  switch (nav_cmd)
									  {
									  case MSG_NextUnreadMessage:
										  m_commandPending = selectFirstUnread;
										  break;
									  case MSG_PreviousUnreadMessage:
										  m_commandPending = selectLastUnread;
										  break;
									  default:
										  XP_ASSERT(0);
										  break;
									  }
								  loadFolder(new_finfo);
							  }
						  else
							  {
								  
								  int numLines = MSG_GetNumLines(m_pane),
									  idx = (int) index;
								  if (idx >=0 && idx < numLines)
									  showMessage(idx);
							  }
					  }
			  }

		  XFE_MozillaApp::theApp()->notifyInterested(XFE_MNView::folderChromeNeedsUpdating, 
													 getFolderInfo());
	  }
#undef IS_CMD
}

char *XFE_ThreadView::getColumnTextByMsgLine(MSG_MessageLine *msgLine, int column)
{
  static char buf[1024];	 /* ## Sigh... */
  static char from_buf[1024];	 /* ## Sigh... */
  static char size_buf[100]; /* ## Sigh... */
  char *tmp = NULL;
  
  switch (column)
    {
    case OUTLINER_COLUMN_SUBJECT:
      {
        tmp = IntlDecodeMimePartIIStr(msgLine->subject, fe_LocaleCharSetID, 
                                      FALSE);

        if (msgLine->flags & MSG_FLAG_HAS_RE) 
          {
            XP_SAFE_SPRINTF(buf, sizeof(buf), 
                            "%s", XP_GetString( XFE_RE ) ); 
            XP_STRNCAT_SAFE(buf,
                            (tmp ? tmp : msgLine->subject),
                            sizeof(buf) - XP_STRLEN(buf));
            
            if (tmp) XP_FREE(tmp);
            
            return buf;
          } 
        else 
          {
            if (tmp) 
              {
                XP_STRNCPY_SAFE(buf, tmp, sizeof(buf));
                XP_FREE(tmp);
                return buf;
              } 
            else 
              {
                XP_STRNCPY_SAFE(buf, msgLine->subject, sizeof(buf));
                return buf;
              }
          }
      }
    case OUTLINER_COLUMN_SENDERRECIPIENT:
      {
	tmp = IntlDecodeMimePartIIStr(msgLine->author, fe_LocaleCharSetID, FALSE);
	if (tmp) 
	  {
	    XP_STRNCPY_SAFE(from_buf, tmp, sizeof(from_buf));
	    XP_FREE(tmp);
	    return from_buf;
	  } 
	else 
	  {
	    XP_STRNCPY_SAFE(from_buf, msgLine->author, sizeof(from_buf));
	    return from_buf;
	  }
      }
    case OUTLINER_COLUMN_DATE:
      {
	if (msgLine->date == 0) 
	  {
	    return "";
	  } 
	else 
	  {
	    return (char*)MSG_FormatDate(m_pane, msgLine->date);
	  }
      }
    case OUTLINER_COLUMN_PRIORITY:
      {
	return priorityToString(msgLine->priority);
      }
    case OUTLINER_COLUMN_SIZE:
      {
		  XP_SAFE_SPRINTF(size_buf, sizeof(size_buf),
						  "%ld", msgLine->messageLines);
	return size_buf;
      }
    case OUTLINER_COLUMN_STATUS:
      {
		  if (msgLine->flags & MSG_FLAG_NEW)
			  return XP_GetString(XP_STATUS_NEW);
		  else if ((msgLine->flags & MSG_FLAG_REPLIED)&&
				   (msgLine->flags & MSG_FLAG_FORWARDED))
			  return XP_GetString(XP_STATUS_REPLIED_AND_FORWARDED);
		  else if (msgLine->flags & MSG_FLAG_FORWARDED)
			  return XP_GetString(XP_STATUS_FORWARDED);
		  else if (msgLine->flags & MSG_FLAG_REPLIED)
			  return XP_GetString(XP_STATUS_REPLIED);

		  return NULL; /* ### */
      }
    case OUTLINER_COLUMN_UNREADMSG:
      {
	return NULL; // we don't have a string in this column -- only an icon.
      }
    case OUTLINER_COLUMN_FLAG:
      {
	return NULL; // we don't have a string in this column -- only an icon.
      }
    default:
      XP_ASSERT(0);
      return NULL;
    }

}
/* Outlinable interface methods */
char *XFE_ThreadView::getCellTipString(int row, int column)
{
	/* returned string will be duplicated by outliner
	 */
	char *tmp = 0;
	if (row < 0) {
		/* header
		 */
		tmp = getColumnHeaderText(column);
	}/* if */
	else {
		/* content 
		 */
		MSG_MessageLine* msgLine = 
			(MSG_MessageLine *) XP_CALLOC(1, 
										  sizeof(MSG_MessageLine));
		if (!MSG_GetThreadLineByIndex(m_pane, row, 1, msgLine))
			return NULL;

		/* static array; do not free
		 */
		tmp = getColumnTextByMsgLine(msgLine, column);
		XP_FREEIF(msgLine);
	}/* else */
	if (tmp && 
		(!m_outliner->isColTextFit(tmp, row, column)))
		return tmp;

	return NULL;
}

char *XFE_ThreadView::getCellDocString(int /* row */, int /* column */)
{
	return NULL;
}

void *
XFE_ThreadView::ConvFromIndex(int index)
{
  MessageKey id = MSG_GetMessageKey(m_pane, index);

  return (void*)id;
}

int
XFE_ThreadView::ConvToIndex(void *item)
{
  MessageKey id = (MessageKey)item;
  MSG_ViewIndex index;

  index = MSG_GetMessageIndexForKey(m_pane, id, False);

  if (index == MSG_VIEWINDEXNONE)
	  {
		  MSG_MessageLine line;

		  if (!MSG_GetThreadLineById(m_pane, id, &line))
			  {
				  /* error while looking up line for this message.
					 punt */
				  return -1;
			  }
		  else
			  {
				  MSG_ViewIndex thread_index =
					  MSG_GetMessageIndexForKey(m_pane, line.threadId, False);
				  
				  if (MSG_ExpansionDelta(m_pane, thread_index) > 0)
					  {
						  MSG_ToggleExpansion(m_pane, thread_index, NULL);
						  index = MSG_GetMessageIndexForKey(m_pane, 
															id, False);
					  }
				  
				  return index;
			  }
	  }
  else
	  {
		  return index;
	  }
}

void *
XFE_ThreadView::acquireLineData(int line)
{
  m_ancestorInfo = NULL;

  if (!MSG_GetThreadLineByIndex(m_pane, line, 1, &m_messageLine))
    return NULL;

  m_ancestorInfo = new OutlinerAncestorInfo[ m_messageLine.level + 1 ];

  int i = m_messageLine.level;
  int idx = line + 1;
  while ( i > 0 ) {
    if ( idx < m_outliner->getTotalLines()) {
      int level = MSG_GetThreadLevelByIndex( m_pane, idx );
      if ( level == i ) {
	m_ancestorInfo[i].has_prev = TRUE;
	m_ancestorInfo[i].has_next = TRUE;
	i--;
	idx++;
      } else if ( level < i ) {
	m_ancestorInfo[i].has_prev = FALSE;
	m_ancestorInfo[i].has_next = FALSE;
	i--;
      } else {
	idx++;
      }
    } else {
      m_ancestorInfo[i].has_prev = FALSE;
      m_ancestorInfo[i].has_next = FALSE;
      i--;
    }
  }
  m_ancestorInfo[0].has_prev = FALSE;
  m_ancestorInfo[0].has_next = FALSE;
  
  return &m_messageLine;
}

void
XFE_ThreadView::getTreeInfo(Boolean *expandable,
			      Boolean *is_expanded,
			      int *depth,
			      OutlinerAncestorInfo **ancestor)
{
  XP_Bool is_line_expandable;
  XP_Bool is_line_expanded;

  is_line_expandable = (m_messageLine.numChildren > 0);

  if (is_line_expandable)
    {
      is_line_expanded = (m_messageLine.flags & MSG_FLAG_ELIDED) == 0;
    }
  else
    {
      is_line_expanded = FALSE;
    }

  if ( ancestor )
    *ancestor = m_ancestorInfo;

  if (depth)
    *depth = m_messageLine.level;

  if (expandable)
      *expandable = is_line_expandable;

  if (is_expanded)
    *is_expanded = is_line_expanded;
}

EOutlinerTextStyle
XFE_ThreadView::getColumnStyle(int /*column*/)
{
  if (m_messageLine.flags & MSG_FLAG_EXPIRED)
    return OUTLINER_Default ; /* Don't boldface dummy messages. */
  else if ((m_messageLine.flags & MSG_FLAG_ELIDED) && m_messageLine.numNewChildren > 0)
    return OUTLINER_Bold; /* boldface toplevel thread messages with unread children. */
  else
    return (m_messageLine.flags & MSG_FLAG_READ) ? OUTLINER_Default : OUTLINER_Bold;
}

char *
XFE_ThreadView::getColumnText(int column)
{ 
  static char buf_a[1024];	 /* ## Sigh... */
  static char from_buf_a[1024];	 /* ## Sigh... */
  static char size_buf_a[100]; /* ## Sigh... */
  char *tmp = NULL;
  
  switch (column)
    {
    case OUTLINER_COLUMN_SUBJECT:
      {
	tmp = IntlDecodeMimePartIIStr(m_messageLine.subject, fe_LocaleCharSetID, FALSE);

	if (m_messageLine.flags & MSG_FLAG_HAS_RE) 
	  {
	    XP_STRNCPY_SAFE (buf_a, XP_GetString( XFE_RE ), sizeof(buf_a) ); 
	    XP_STRNCAT_SAFE (buf_a, 
                         (tmp ? tmp : m_messageLine.subject), 
                         sizeof(buf_a) - XP_STRLEN(buf_a));

	    if (tmp) XP_FREE(tmp);

	    return buf_a;
	  } 
	else 
	  {
	    if (tmp) 
	      {
            XP_STRNCPY_SAFE(buf_a, tmp, sizeof(buf_a));
            
            XP_FREE(tmp);
            return buf_a;
	      } 
	    else 
	      {
		return m_messageLine.subject;
	      }
	  }
      }
    case OUTLINER_COLUMN_SENDERRECIPIENT:
      {
	tmp = IntlDecodeMimePartIIStr(m_messageLine.author, fe_LocaleCharSetID, FALSE);
	if (tmp) 
	  {
	    XP_STRNCPY_SAFE(from_buf_a, tmp, sizeof(from_buf_a));
	    XP_FREE(tmp);
	    return from_buf_a;
	  } 
	else 
	  {
	    return m_messageLine.author;
	  }
      }
    case OUTLINER_COLUMN_DATE:
      {
	if (m_messageLine.date == 0) 
	  {
	    return "";
	  } 
	else 
	  {
	    return (char*)MSG_FormatDate(m_pane, m_messageLine.date);
	  }
      }
    case OUTLINER_COLUMN_PRIORITY:
      {
	return priorityToString(m_messageLine.priority);
      }
    case OUTLINER_COLUMN_SIZE:
      {
        XP_SAFE_SPRINTF(size_buf_a, sizeof(size_buf_a),
                        "%ld", m_messageLine.messageLines);
	return size_buf_a;
      }
    case OUTLINER_COLUMN_STATUS:
      {
		  if (m_messageLine.flags & MSG_FLAG_NEW)
			  return XP_GetString(XP_STATUS_NEW);
		  else if ((m_messageLine.flags & MSG_FLAG_REPLIED)&&
				   (m_messageLine.flags & MSG_FLAG_FORWARDED))
			  return XP_GetString(XP_STATUS_REPLIED_AND_FORWARDED);
		  else if (m_messageLine.flags & MSG_FLAG_FORWARDED)
			  return XP_GetString(XP_STATUS_FORWARDED);
		  else if (m_messageLine.flags & MSG_FLAG_REPLIED)
			  return XP_GetString(XP_STATUS_REPLIED);

		  return NULL; /* ### */
      }
    case OUTLINER_COLUMN_UNREADMSG:
      {
	return NULL; // we don't have a string in this column -- only an icon.
      }
    case OUTLINER_COLUMN_FLAG:
      {
	return NULL; // we don't have a string in this column -- only an icon.
      }
    default:
      XP_ASSERT(0);
      return NULL;
    }
}

fe_icon *
XFE_ThreadView::getColumnIcon(int column)
{

  switch (column)
    {
    case OUTLINER_COLUMN_SUBJECT:
		{
			MSG_FolderLine folderLine;
			
			MSG_GetFolderLineById(XFE_MNView::getMaster(), m_folderInfo, &folderLine);
			return flagToIcon(folderLine.flags, m_messageLine.flags);
		}
    case OUTLINER_COLUMN_UNREADMSG:
		return (m_messageLine.flags & MSG_FLAG_READ ? &msgReadIcon : &msgUnreadIcon);
    case OUTLINER_COLUMN_FLAG:
		return (m_messageLine.flags & MSG_FLAG_MARKED ? &msgFlagIcon : &msgReadIcon);
	case -1: /* OUTLINER_DESCENDANT_SELECT_COLUMN */
		{
			if (m_messageLine.flags & MSG_FLAG_IGNORED)
				{
					return (m_messageLine.flags & MSG_FLAG_ELIDED ? &closedSpoolIgnoredIcon : &openSpoolIgnoredIcon);
				}
			else if (m_messageLine.flags & MSG_FLAG_WATCHED)
				{
					return (m_messageLine.flags & MSG_FLAG_ELIDED ? &closedSpoolWatchedIcon : &openSpoolWatchedIcon);
				}
			else if (m_messageLine.numChildren > 0)
				{
					if (m_messageLine.flags & MSG_FLAG_NEW
						|| m_messageLine.numNewChildren > 0)
						return (m_messageLine.flags & MSG_FLAG_ELIDED ? &closedSpoolNewIcon : &openSpoolNewIcon);
					else
						return (m_messageLine.flags & MSG_FLAG_ELIDED ? &closedSpoolIcon : &openSpoolIcon);
				}
			else
				{
					return NULL;
				}
		}
    default:
		return 0;
    }
}

char *
XFE_ThreadView::getColumnName(int column)
{
  switch (column) 
    {
    case OUTLINER_COLUMN_SUBJECT:
      return "Subject";
    case OUTLINER_COLUMN_UNREADMSG:
      return "Unread";
    case OUTLINER_COLUMN_FLAG:
      return "Flag";
    case OUTLINER_COLUMN_DATE:
      return "Date";
    case OUTLINER_COLUMN_PRIORITY:
      return "Priority";
    case OUTLINER_COLUMN_SIZE:
      return "Size";
    case OUTLINER_COLUMN_STATUS:
      return "Status";
    case OUTLINER_COLUMN_SENDERRECIPIENT:
      return "Sender";
    default:
      XP_ASSERT(0);
      return 0;
    }
}

char *
XFE_ThreadView::getColumnHeaderText(int column)
{
	switch (column) 
		{
		case OUTLINER_COLUMN_SUBJECT:
			return XP_GetString(XFE_THREAD_OUTLINER_COLUMN_SUBJECT);
		case OUTLINER_COLUMN_UNREADMSG:
			return 0;
		case OUTLINER_COLUMN_FLAG:
			return 0;
		case OUTLINER_COLUMN_DATE:
			return XP_GetString(XFE_THREAD_OUTLINER_COLUMN_DATE);
		case OUTLINER_COLUMN_PRIORITY:
			return XP_GetString(XFE_THREAD_OUTLINER_COLUMN_PRIORITY);
		case OUTLINER_COLUMN_SIZE:
			{
				if (m_displayingNewsgroup)
					return XP_GetString(XFE_SIZE_IN_LINES);
				else
					return XP_GetString(XFE_SIZE_IN_BYTES);
			}
		case OUTLINER_COLUMN_STATUS:
			return XP_GetString(XFE_THREAD_OUTLINER_COLUMN_STATUS);
		case OUTLINER_COLUMN_SENDERRECIPIENT:
			if (MSG_DisplayingRecipients(m_pane))
				return XP_GetString(XFE_THREAD_OUTLINER_COLUMN_RECIPIENT);
			else
				return XP_GetString(XFE_THREAD_OUTLINER_COLUMN_SENDER);
		default:
			// XP_ASSERT(0);
			return 0;
		}
}

fe_icon *
XFE_ThreadView::getColumnHeaderIcon(int column)
{
  switch (column)
    {
    case OUTLINER_COLUMN_UNREADMSG:
      return &msgUnreadIcon;
    case OUTLINER_COLUMN_FLAG:
      return &msgFlagIcon;
    case OUTLINER_COLUMN_SUBJECT:
      if (MSG_GetToggleStatus(m_pane, MSG_SortByThread, NULL, 0) == MSG_Checked)
		  return &threadonIcon;
      else
		  return &threadoffIcon;
    default:
      return 0;
    }
}

EOutlinerTextStyle
XFE_ThreadView::getColumnHeaderStyle(int column)
{
  MSG_CommandType sort_type = (MSG_CommandType)~0;

  switch (column)
    {
    case OUTLINER_COLUMN_SENDERRECIPIENT:
      sort_type = MSG_SortBySender;
      break;
    case OUTLINER_COLUMN_DATE:
      sort_type = MSG_SortByDate;
      break;
    case OUTLINER_COLUMN_SUBJECT:
      sort_type = MSG_SortBySubject;
      break;
    case OUTLINER_COLUMN_PRIORITY:
      sort_type = MSG_SortByPriority;
      break;
	case OUTLINER_COLUMN_SIZE:
		sort_type = MSG_SortBySize;
		break;
	case OUTLINER_COLUMN_STATUS:
		sort_type = MSG_SortByStatus;
		break;
	case OUTLINER_COLUMN_UNREADMSG:
		sort_type = MSG_SortByUnread;
		break;
	case OUTLINER_COLUMN_FLAG:
		sort_type = MSG_SortByFlagged;
		break;
    }
  
  if (sort_type == (MSG_CommandType)~0)
    {
      return OUTLINER_Default;
    }
  else
    {
		XP_Bool checked = MSG_GetToggleStatus(m_pane, sort_type, NULL, 0) == MSG_Checked;
		XP_Bool backward = MSG_GetToggleStatus(m_pane, MSG_SortBackward, NULL, 0) == MSG_Checked;

		if (checked)
			{
				m_outliner->setSortColumn(column,
										  backward ? OUTLINER_SortDescending : OUTLINER_SortAscending);
				return OUTLINER_Bold;
			}
		else
			{
				return OUTLINER_Default;
			}
    }
}

void
XFE_ThreadView::releaseLineData()
{
  delete [] m_ancestorInfo;
  m_ancestorInfo = NULL;
}

XFE_CALLBACK_DEFN(XFE_ThreadView, allConnectionsComplete)(XFE_NotificationCenter *,
														  void *,
														  void *)
{
	D(printf ("in all connections complete.\n");)

#if HANDLE_CMD_QUEUE

#if defined(DEBUG_tao)
	printf("\n XFE_ThreadView::allConnectionsComplete\n");
#endif

	if (m_deletedCount &&
		m_deleted)
		processCmdQueue();
#endif /* HANDLE_CMD_QUEUE */

	handlePendingCommand();

}

void
XFE_ThreadView::setPendingCmdSelByKey(PendingCommand cmd, MessageKey key)
{
	m_commandPending = cmd;
	m_pendingSelectionKey = key;
}

void
XFE_ThreadView::handlePendingCommand()
{
	switch (m_commandPending)
		{
		case invalidateThreadAndSelection:
			/* for this case, we loop backward to the message with level 1 -- the toplevel
			   message in this thread, find it's num_children, and invalidate all those
			   lines.  This fixes pipe updates and lots of other things. */
			{
				D(printf("handlePendingCommand(invalidateThreadAndSelection)\n");)
				/* tao: Obselete deletion code; assert!
				 */
				XP_ASSERT(0);
				break;
			}
		case getNewMessages:
			{
				D(printf("handlePendingCommand(getNewMessages)\n");)
				m_commandPending = noPendingCommand;
				doCommand(xfeCmdGetNewMessages);
				break;
			}
		case selectByIndex:
			{
				D(printf("handlePendingCommand(selectByIndex)\n");)
				D(printf("  pending selection index is %d\n", m_pendingSelectionIndex);)
				m_commandPending = noPendingCommand;
				if (m_pendingSelectionIndex != MSG_VIEWINDEXNONE)
					showMessage(m_pendingSelectionIndex);
				break;
			}
		case selectByKey:
			{
				D(printf("handlePendingCommand(selectByKey,%d)\n", m_pendingSelectionKey);)

				m_commandPending = noPendingCommand;

				if (m_pendingSelectionKey != MSG_MESSAGEKEYNONE)
					{
						/* 87274: Caldera: "Go to Message Folder" fails when 
						 * messages are threaded.
						 *
						 * Fix: call MSG_xxxForKey() to expand the thread 
						 * when needed.
						 */
						MSG_ViewIndex index = 
							MSG_GetMessageIndexForKey(m_pane, 
													  m_pendingSelectionKey,
													  True);
						D(printf ("  pending selection index is %d\n", index);)
						if (index != MSG_VIEWINDEXNONE)
							showMessage(index);
					}
				break;
			}
		case selectFirstUnread:
			{
				D(printf("handlePendingCommand(selectFirstUnread)\n");)
				MSG_ViewIndex index, threadIndex;
				MessageKey resultId;

				m_commandPending = noPendingCommand;

				m_outliner->setBlockSel(True);
				MSG_ViewNavigate(m_pane, MSG_FirstUnreadMessage, MSG_VIEWINDEXNONE/* ??? */,
								 &resultId, &index, &threadIndex, NULL);
				m_outliner->setBlockSel(False);

				showMessage(index);
				break;
			}
		case selectLastUnread:
			{
				D(printf("handlePendingCommand(selectLastUnread)\n");)
				MSG_ViewIndex index, threadIndex;
				MessageKey resultId;

				m_commandPending = noPendingCommand;

				MSG_ViewNavigate(m_pane, MSG_LastUnreadMessage, MSG_VIEWINDEXNONE/* ??? */,
								 &resultId, &index, &threadIndex, NULL);

				showMessage(index);
				break;
			}
		case scrollToFirstNew:
			{
				D(printf("handlePendingCommand(scrollToFirstNew)\n");)
				MessageKey resultId, resultingThread;
				MSG_ViewIndex resultIndex = 0;

				m_commandPending = noPendingCommand;

				/* note:  We don't expand threads here -- we scroll to the message that
				   starts the thread. */

				/* after we do a get new mail, we end up here.  We try to scroll to the
				   first new one. */
				MSG_DataNavigate(m_pane, MSG_FirstNew, MSG_MESSAGEKEYNONE/*???*/,
								 &resultId, &resultingThread);
				
				/* if there wasn't a new message, we scroll to the first unread message */
				if (resultId == MSG_MESSAGEKEYNONE
					|| resultingThread == MSG_MESSAGEKEYNONE)
					MSG_DataNavigate(m_pane, MSG_FirstUnreadMessage, MSG_MESSAGEKEYNONE/*???*/,
									 &resultId, &resultingThread);
				
				/* if there wasn't an unread message, we scroll to the last item. */
				if (m_outliner->getTotalLines() > 0)
					{
						if (resultId == MSG_MESSAGEKEYNONE
							|| resultingThread == MSG_MESSAGEKEYNONE)
							{
								resultIndex = (MSG_ViewIndex)m_outliner->getTotalLines() - 1;
							}
						else
							{
								resultIndex = 
									MSG_GetMessageIndexForKey(m_pane, 
															  resultingThread,
															  False);
								if (resultIndex == MSG_VIEWINDEXNONE)
									resultIndex = m_outliner->getTotalLines() - 1;
							}
						
						m_outliner->makeVisible((int)resultIndex);
					}
				break;
			}
		case noPendingCommand: // we should be so lucky
			D(printf ("no pending command to execute\n");)
			break;
		default:
			XP_ASSERT(0);
			break;
		}
}

void 
XFE_ThreadView::Buttonfunc(const OutlineButtonFuncData *data)
{
        /* 
	** Broadcast the view is currently in focus because 
	** of the button clicking
	*/
        getToplevel()->notifyInterested(XFE_MNListView::changeFocus, (void*) this);

	if (data->row == -1) // heading row.
		{
			const int *selected;
			int count;
			
			m_outliner->setDescendantSelectAllowed(False);
			
			switch (data->column)
				{
				case OUTLINER_COLUMN_SUBJECT:
					if (data->x < (threadonIcon.width + /* hack -- shadowthickness */ 4))
						{
							if (isCommandEnabled(xfeCmdSortByThread))
								{
									m_outliner->setSortColumn(-1,
															  OUTLINER_SortAscending);
									m_outliner->setDescendantSelectAllowed(True);
									doCommand(xfeCmdSortByThread);
								}
						}
					else
						{
							if (isCommandEnabled(xfeCmdSortBySubject))
								{
									if (m_outliner->getSortColumn() == data->column)
										m_outliner->toggleSortDirection();
									doCommand(xfeCmdSortBySubject);
								}
						}
					break;
				case OUTLINER_COLUMN_SENDERRECIPIENT:
					if (isCommandEnabled(xfeCmdSortBySender))
						{
							if (m_outliner->getSortColumn() == data->column)
								m_outliner->toggleSortDirection();
							doCommand(xfeCmdSortBySender);
						}
					break;
				case OUTLINER_COLUMN_DATE:
					if (isCommandEnabled(xfeCmdSortByDate))
						{
							if (m_outliner->getSortColumn() == data->column)
								m_outliner->toggleSortDirection();
							doCommand(xfeCmdSortByDate);
						}
					break;
				case OUTLINER_COLUMN_PRIORITY:
					if (isCommandEnabled(xfeCmdSortByPriority))
						{
							if (m_outliner->getSortColumn() == data->column)
								m_outliner->toggleSortDirection();
							doCommand(xfeCmdSortByPriority);
						}
					break;
				case OUTLINER_COLUMN_STATUS:
					if (isCommandEnabled(xfeCmdSortByStatus))
						{
							if (m_outliner->getSortColumn() == data->column)
								m_outliner->toggleSortDirection();
							doCommand(xfeCmdSortByStatus);
						}
					break;
				case OUTLINER_COLUMN_SIZE:
					if (isCommandEnabled(xfeCmdSortBySize))
						{
							if (m_outliner->getSortColumn() == data->column)
								m_outliner->toggleSortDirection();
							doCommand(xfeCmdSortBySize);
						}
					break;
				case OUTLINER_COLUMN_UNREADMSG:
					if (isCommandEnabled(xfeCmdSortByUnread))
						{
							if (m_outliner->getSortColumn() == data->column)
								m_outliner->toggleSortDirection();
							doCommand(xfeCmdSortByUnread);
						}
					break;
				case OUTLINER_COLUMN_FLAG:
					if (isCommandEnabled(xfeCmdSortByFlag))
						{
							if (m_outliner->getSortColumn() == data->column)
								m_outliner->toggleSortDirection();
							doCommand(xfeCmdSortByFlag);
						}
					break;
				}

			m_outliner->getSelection(&selected, &count);

			if (count >= 1)
				{
					D(printf ("Making %d visible\n", selected[0]);)
					m_outliner->makeVisible(selected[0]);
				}
		}
	else  // content row.
		{
			if (data->clicks == 2)
				{
					m_outliner->selectItemExclusive(data->row);
					
					if ( m_displayingDraft )
						{
							MSG_OpenDraft(m_pane, m_folderInfo,
										  MSG_GetMessageKey(m_pane, data->row));
						}
					else
						{
							// ok.  we're going to display a message in a message frame.
							// what is left to determine is whether or not we will be
							// popping up another window or reusing an existing one (if
							// there is one.)  This behavior is governed by two things:
							// 1.  fe_globalPrefs.reuse_msg_window and,
							// 2.  data->alt (whether the alt key was down or not...)
							
							fe_showMsg(XtParent(getToplevel()->getBaseWidget()),
									   /* Tao: we might need to check if this returns a 
										* non-NULL frame
										*/
									   ViewGlue_getFrame(m_contextData),
									   (Chrome*)NULL,
									   m_folderInfo,
									   MSG_GetMessageKey(m_pane, data->row),
									   
									   (( fe_globalPrefs.reuse_msg_window
										  && !data->shift)
										|| (!fe_globalPrefs.reuse_msg_window
											&& data->shift))
									   );
						}
				}
			else if (data->clicks == 1)
				{
					const int *selected;
					int count;
							
					m_outliner->getSelection(&selected, &count);
#if defined(DEBUG_tao_)
					printf("\n+++ data->clicks count=%d\n", count);
#endif
					if (data->ctrl)
						{
							m_outliner->toggleSelected(data->row);

							m_outliner->getSelection(&selected, &count);
#if defined(DEBUG_tao_)
							printf("\n+++ data->clicks count=%d\n", count);
#endif
							if (count == 1)
								showMessage(selected[0]);
							else if (count == 0 ||
									 count > 1)
								showMessage(-1);
								
						}
					else if (data->shift)
						{
							// select the range.
							
							if (count == 0) /* there wasn't anything selected yet. */
								{
									m_outliner->selectItemExclusive(data->row);
									
									// clear the message area/label
									showMessage(-1);
								}
							else if (count == 1) /* there was only one, so we select the range from
													that item to the new one. */
								{
									m_outliner->selectRangeByIndices(selected[0], data->row);
									
									// clear the message area/label
									showMessage(-1);
								}
							else /* we had a range of items selected, so let's do something really
									nice with them. */
								{
									m_outliner->trimOrExpandSelection(data->row);
									
									// clear the message area/label
									showMessage(-1);
								}
						}
					else
						{	  
							// handle the columns that don't actually move the selection here
							if (data->column == OUTLINER_COLUMN_UNREADMSG)
								{
									MSG_Command(m_pane, MSG_ToggleMessageRead, (MSG_ViewIndex*)&data->row, 1);
								}
							else if (data->column == OUTLINER_COLUMN_FLAG)
								{
									MSG_MessageLine line;
									
									if (!MSG_GetThreadLineByIndex(m_pane, data->row, 1, &line))
										return;
									
									if (line.flags & MSG_FLAG_MARKED)
										MSG_Command(m_pane, MSG_UnmarkMessages, (MSG_ViewIndex*)&data->row, 1);
									else
										MSG_Command(m_pane, MSG_MarkMessages, (MSG_ViewIndex*)&data->row, 1);
								}
							else
								{
									if ((int) m_targetIndex != data->row) {
										//
										m_targetIndex = MSG_VIEWINDEXNONE;
										if (m_scrollTimer)
											XtRemoveTimeOut(m_scrollTimer);
										m_scrollTimer = 0;
									}/* if */

									// we've selected a message, update the label and load it into
									// our message view.
									
									showMessage(data->row);
								}
						}
					
					XFE_MozillaApp::theApp()->notifyInterested(XFE_MNView::folderChromeNeedsUpdating,
															   getFolderInfo());
				}
		}
}

void
XFE_ThreadView::Flippyfunc(const OutlineFlippyFuncData *data)
{
	int delta = MSG_ExpansionDelta(m_pane, data->row);
	XP_Bool need_to_select_top = FALSE;

	if (data->do_selection) // we do descendant selection
		{
			MSG_MessageLine line;

			if (delta > 0)
				MSG_ToggleExpansion(m_pane, data->row, NULL);
			
			MSG_GetThreadLineByIndex(m_pane, data->row, 1, &line);
			
			m_outliner->deselectAllItems();
			m_outliner->selectRangeByIndices(data->row, data->row + line.numChildren);
			showMessage(-1);
		}
	else if (delta != 0) // actually to flippy stuff.
		{
			if (delta < 0)
				{
					if (delta == -1)
						addCollapsed(data->row);

					int i;
					delta = -delta;

					for (i = data->row + 1; i < data->row + 1 + delta; i ++)
						{
							if (m_outliner->isSelected(i))
								{
									need_to_select_top = TRUE;
									m_outliner->deselectItem(i);
								}
						}
				}

			MSG_ToggleExpansion(m_pane, data->row, NULL);

			if (need_to_select_top)
				m_outliner->selectItem(data->row);
		}
}

XFE_CALLBACK_DEFN(XFE_ThreadView, spaceAtMsgEnd)(XFE_NotificationCenter*,
						 void *, void *)
{
	if (isCommandEnabled(xfeCmdNextUnreadMessage))
		doCommand(xfeCmdNextUnreadMessage);
	else if (isCommandEnabled(xfeCmdNextUnreadCollection))
		doCommand(xfeCmdNextUnreadCollection);
}

static void
click_timer_func(XtPointer closure, XtIntervalId *)
{
    XFE_ThreadView *v = (XFE_ThreadView *) closure;
    if (v)
        v->selectTimer();
}

void
XFE_ThreadView::selectTimer()
{
	if (m_targetIndex != MSG_VIEWINDEXNONE) 
		m_outliner->makeVisible(m_targetIndex);
	//
	m_targetIndex = MSG_VIEWINDEXNONE;
	if (m_scrollTimer)
		XtRemoveTimeOut(m_scrollTimer);
	m_scrollTimer = 0;
}

XFE_CALLBACK_DEFN(XFE_ThreadView, newMessageLoading)(XFE_NotificationCenter*,
													 void *, void*)
{
	if (m_commandPending == noPendingCommand)
		{
			MSG_FolderInfo *info = m_msgview->getFolderInfo();
			MessageKey key = m_msgview->getMessageKey();
#if defined(DEBUG_tao)
			printf("--newMessageLoading, m_pane=0x%x, m_lastLoadedKey=0x%x,key=0x%x", m_pane, m_lastLoadedKey, key);
#endif
			if (info != m_folderInfo)
				{
					m_commandPending = selectByKey;
					m_pendingSelectionKey = key;
					loadFolder(info);
				}
			else
				{
					MSG_ViewIndex index = MSG_GetMessageIndexForKey(m_pane, 
																	key, 
																	False);
					
					int numLines = MSG_GetNumLines(m_pane);
					if (m_outliner->getTotalLines() != numLines)
						m_outliner->change(0, numLines, numLines);
#if 1
					m_targetIndex = index;
					int intvl0 = XtGetMultiClickTime(XtDisplay(m_widget)),
						intvl = intvl0 + intvl0/2;
#if defined(DEBUG_tao)
					printf("--newMessageLoading, intvl0=%d, intvl=%d", 
						   intvl0, intvl);
#endif
					m_scrollTimer = XtAppAddTimeOut(fe_XtAppContext, intvl,
												   click_timer_func, this);
					
#else
					/* 97010: selecting the last mail/news article 
					 *        show the wrong one.
					 * 
					 * Take out to prevent improper auto-selection... -tao
					 */
					m_outliner->makeVisible(index);
#endif
					m_outliner->selectItemExclusive(index);
					updateExpandoFlippyText(index);
				}
      			notifyInterested(XFE_View::chromeNeedsUpdating);
		}
}

void
XFE_ThreadView::toggleMsgExpansion()
{
	int32 folder_prefs = 0;

	if (m_folderInfo)
		{
			folder_prefs = MSG_GetFolderPrefFlags(m_folderInfo);
			folder_prefs |= MSG_FOLDER_PREF_FEVALID;
		}

	if (m_msgExpanded)
		{
			// we are currently expanded
			Dimension message_pane_height;
			
			XtVaGetValues(m_msgview->getBaseWidget(),
						  XmNheight, &message_pane_height,
						  NULL);
			
			D(printf ("Saving message pane height as %d\n", message_pane_height);)
			PREF_SetIntPref(MESSAGEPANE_HEIGHT_PREF, (int32)message_pane_height);

			
			m_msgview->hide();
			
			m_msgExpanded = FALSE;
			// Set maximum heights so that the sash will go away:
			Dimension height;
			XtVaGetValues(m_arrowform,
						  XmNheight, &height,
						  NULL);
			XtVaSetValues(m_arrowform,
						  XmNpaneMinimum, height,
						  XmNpaneMaximum, height,
						  NULL);
			
			XtVaSetValues(m_arrowb, 
						  XmNarrowDirection, XmARROW_UP,
						  NULL);
			
			folder_prefs |= MSG_FOLDER_PREF_ONEPANE;

			notifyInterested(XFE_View::chromeNeedsUpdating);
		}
	else
		{
			const int *selected;
			int count;
			int32 message_pane_desired_height;
			Dimension message_pane_minimum_height;
			Dimension message_pane_resulting_height;
			Dimension panedw_spacing;
			Dimension panedw_marginheight;
			Dimension panedw_height;
			Dimension arrow_form_height;
			Dimension minimum_height_of_outliner;
			int resulting_height_of_outliner;
			XP_Bool need_to_resize_frame = False;
			
			// we are currently unexpanded
			XtVaSetValues(m_arrowb, 
						  XmNarrowDirection, XmARROW_DOWN,
						  NULL);
			
			XtVaGetValues(m_widget,
						  XmNspacing, &panedw_spacing,
						  XmNmarginHeight, &panedw_marginheight,
						  XmNheight, &panedw_height,
						  NULL);
			
			XtVaGetValues(m_arrowform,
						  XmNheight, &arrow_form_height,
						  NULL);
			
			XtVaGetValues(m_outliner->getBaseWidget(),
						  XmNpaneMinimum, &minimum_height_of_outliner,
						  NULL);
			
			XtVaGetValues(m_msgview->getBaseWidget(),
						  XmNpaneMinimum, &message_pane_minimum_height,
						  NULL);
			
			PREF_GetIntPref(MESSAGEPANE_HEIGHT_PREF, &message_pane_desired_height);
			
			D(printf ("Message pane wants to be %d high.\n", message_pane_desired_height);)
			
			resulting_height_of_outliner = (panedw_height
											- (2 * panedw_spacing) /* space between the outliner and arrow
																	  and between the arrow and message */
											- (2 * panedw_marginheight)
											- arrow_form_height
											- message_pane_desired_height);
			
			D(printf ("This will make the outliner %d high.\n", resulting_height_of_outliner);)
			
			if (resulting_height_of_outliner < (int)minimum_height_of_outliner)
				{
					D(printf ("  This is not ok.\n");)
					resulting_height_of_outliner = minimum_height_of_outliner;
					
					message_pane_resulting_height = (message_pane_desired_height
													 - (minimum_height_of_outliner
														- resulting_height_of_outliner));
					
					if (message_pane_resulting_height < message_pane_minimum_height)
						{
							message_pane_resulting_height = message_pane_minimum_height;
							need_to_resize_frame = True;
						}
					
					D(printf ("  The message pane will instead be sized to %d, and the window will%s be resized", message_pane_resulting_height, need_to_resize_frame ? "" : " not");)
				}
			
			if (need_to_resize_frame)
				XtVaSetValues(getToplevel()->getBaseWidget(),
							  XmNallowShellResize, True,
							  NULL);

			XtVaSetValues(m_msgview->getBaseWidget(),
						  XmNskipAdjust, TRUE,
						  NULL);
			
			m_msgview->show();

			XtVaSetValues(m_msgview->getBaseWidget(),
						  XmNskipAdjust, FALSE,
                          XmNpaneMinimum, PANE_MIN,
                          XmNpaneMaximum, PANE_MAX,
						  NULL);

			if (need_to_resize_frame)
				XtVaSetValues(getToplevel()->getBaseWidget(),
							  XmNallowShellResize, False,
							  NULL);
			
			XtVaSetValues(m_outliner->getBaseWidget(),
                          XmNpaneMinimum, PANE_MIN,
                          XmNpaneMaximum, PANE_MAX,
                          NULL);

			m_msgExpanded = TRUE;
			
			folder_prefs &= ~MSG_FOLDER_PREF_ONEPANE;

			m_outliner->getSelection(&selected, &count);
			
			if (count == 1)
				showMessage(selected[0]);
			else
				showMessage(-1);
	
			XFE_MozillaApp::theApp()->notifyInterested(XFE_MNView::folderChromeNeedsUpdating, getFolderInfo());
		}

	if (m_folderInfo)
		MSG_SetFolderPrefFlags(m_folderInfo, folder_prefs);
}

void
XFE_ThreadView::toggleMsgExpansionCallback(Widget, XtPointer clientData, XtPointer)
{
  XFE_ThreadView *obj = (XFE_ThreadView*)clientData;

  obj->toggleMsgExpansion();
}

#if !defined(USE_MOTIF_DND)
void
XFE_ThreadView::sourcedropfunc(fe_dnd_Source */*source*/, fe_dnd_Message msg, void *closure)
{
      MSG_FolderInfo *info = (MSG_FolderInfo*)closure;

      // messages can be copied or moved.  The closure to this function will always
      // be a MSG_FolderInfo*.
      
      // XXX assume that nothing could have changed the selected between the time
      // we started dragging and now...  don't know how smart this is...
      
      const char *name = MSG_GetFolderNameFromID(info);
      const int *indices;
      int count;
      
      m_outliner->getSelection(&indices, &count);
      
      /* New Code to take on the new Drag and Drop APIs with effect for both mail/news. However,
  	 the effect checking on news articles are not completed in BE yet.  Coredump will
	 be encountered when filing news articles. */

      if (msg == FE_DND_MESSAGE_DELETE)
      	{
			  // delete the messages.
      	}
      else 
	{

      		MSG_DragEffect requireEffect = MSG_Default_Drag;

      		MSG_DragEffect effect =
                	MSG_DragMessagesIntoFolderStatus(m_pane, 
                	(MSG_ViewIndex*)indices, count,
                	info, requireEffect);

      		if (effect == MSG_Require_Move ) 
		{
			if (msg == FE_DND_MESSAGE_COPY )
			{
	  			DD(printf("### ThreadView::sourcedropfunc(req MOVE): FE_DND_MESSAGE_COPY To \n");)
			  	MSG_CopyMessagesInto(m_pane, (MSG_ViewIndex*)indices, count, name);
			}
			else
      			{
	  			DD(printf("### ThreadView::sourcedropfunc(req MOVE): FE_DND_MESSAGE_MOVE To \n");)
	  			MSG_MoveMessagesInto(m_pane, (MSG_ViewIndex*)indices, count, name);
      			}
		}
		else if ( effect == MSG_Require_Copy )
		{
	  		DD(printf("### ThreadView::sourcedropfunc(req COPY): FE_DND_MESSAGE_COPY To \n");)
			MSG_CopyMessagesInto(m_pane, (MSG_ViewIndex*)indices, count, name);
		}
		else if ( effect == MSG_Drag_Not_Allowed )
		{
	  		DD(printf("### ThreadView::sourcedropfunc: Not Allowed \n");)
			char tmp[128];
                        XP_SAFE_SPRINTF(tmp, sizeof(tmp),
                                        "%s",
					XP_GetString(XFE_DND_MESSAGE_ERROR));
			XFE_Progress (m_contextData, tmp);

		}
		else /* should not reach here. the effect status should be with in
			the defined range. Please double check if you try to refer
			to any enum outside the effect enum scope */
			XP_ASSERT(0);
	}

  /* now we need to modify the chrome of windows viewing either folder */
  XFE_MozillaApp::theApp()->notifyInterested(XFE_MNView::folderChromeNeedsUpdating, m_folderInfo);
  XFE_MozillaApp::theApp()->notifyInterested(XFE_MNView::folderChromeNeedsUpdating, info);
}

void
XFE_ThreadView::source_drop_func(fe_dnd_Source *src, fe_dnd_Message msg, void *closure)
{
  XFE_ThreadView *obj = (XFE_ThreadView*)src->closure;

  obj->sourcedropfunc(src, msg, closure);
}

void
XFE_ThreadView::dropfunc(Widget /*dropw*/, fe_dnd_Event type,
			 fe_dnd_Source *source, XEvent *event)
{
  m_outliner->handleDragEvent(event, type, source);
}

void
XFE_ThreadView::drop_func(Widget dropw, void *closure, fe_dnd_Event type,
			  fe_dnd_Source *source, XEvent* event)
{
  XFE_ThreadView *obj = (XFE_ThreadView*)closure;

  obj->dropfunc(dropw, type, source, event);
}
#endif /* USE_MOTIF_DND */

void
XFE_ThreadView::openWithKey(MessageKey key )
{
  m_commandPending = selectByKey;
  m_pendingSelectionKey = key;
}

#if defined(USE_MOTIF_DND)
fe_icon_data *
XFE_ThreadView::GetDragIconData(int row, int /*column*/)
{
  D(printf("XFE_ThreadView::GetDragIconData()\n");)
  
  if (row == -1) return &MN_MailRead;
  else
	{
	  MSG_FolderLine folderLine;
	  int flags = m_messageLine.flags;

	  MSG_GetFolderLineById(XFE_MNView::getMaster(), m_folderInfo, &folderLine);

	  acquireLineData(row);
	  flags = m_messageLine.flags;
	  releaseLineData();

	  return flagToIconData(folderLine.flags, flags);
	}
}

fe_icon_data *
XFE_ThreadView::getDragIconData(void *this_ptr,
								int row,
								int column)
{
  D(printf("XFE_ThreadView::getDragIconData()\n");)
  XFE_ThreadView *thread_view = (XFE_ThreadView*)this_ptr;

  return thread_view->GetDragIconData(row, column);
}

void
XFE_ThreadView::GetDragTargets(int row, int column,
							   Atom **targets,
							   int *num_targets)
{
  D(printf("XFE_ThreadView::GetDragTargets()\n");)

  int flags;

  XP_ASSERT(row > -1);
  if (row == -1)
	{
	  *targets = NULL;
	  *num_targets = 0;
	}
  else
	{
	  acquireLineData(row);
	  flags = m_messageLine.flags;
	  releaseLineData();
	  
	  *num_targets = 5;
	  *targets = new Atom[ *num_targets ];

	  (*targets)[1] = XFE_DragBase::_XA_NETSCAPE_URL;
	  (*targets)[2] = XA_STRING;

	  if (isDisplayingNews())
		(*targets)[0] = XFE_OutlinerDrop::_XA_NETSCAPE_NEWS_MESSAGE;
	  else
		(*targets)[0] = XFE_OutlinerDrop::_XA_NETSCAPE_MAIL_MESSAGE;
	}
}

void
XFE_ThreadView::getDragTargets(void *this_ptr,
							   int row,
							   int column,
							   Atom **targets,
							   int *num_targets)
{
  D(printf("XFE_ThreadView::getDragTargets()\n");)
  XFE_ThreadView *thread_view = (XFE_ThreadView*)this_ptr;
 
 thread_view->GetDragTargets(row, column, targets, num_targets);
}

char *
XFE_ThreadView::DragConvert(Atom atom)
{
  if (atom == XFE_DragBase::_XA_NETSCAPE_URL)
	{
	  // translate drag data to NetscapeURL format
	  XFE_URLDesktopType urlData;
	  char *result;
	  const int *selection;
	  int count;
	  int i;

	  m_outliner->getSelection(&selection, &count);

	  urlData.createItemList(count);

	  for (i = 0; i < count; i ++)
		{
		  MessageKey key = MSG_GetMessageKey(m_pane, selection[i]);
		  URL_Struct *url = MSG_ConstructUrlForMessage(m_pane,
													   key);
		  urlData.url(i,url->address);

		  NET_FreeURLStruct(url);
		}

	  result = XtNewString(urlData.getString());
	  
	  return result;
	}
  else if (atom == XFE_OutlinerDrop::_XA_NETSCAPE_MAIL_MESSAGE
		   || atom == XFE_OutlinerDrop::_XA_NETSCAPE_NEWS_MESSAGE)
	{
	  char result[100];
	  const int *selection;
	  int count;

	  m_outliner->getSelection(&selection, &count);

	  if (count < 1) return XtNewString("0"); /* XXX ? */

	  sprintf (result, "%d", selection[0]);
	  return (char*)XtNewString(result); /* XXX fixme */
	}
  else if (atom == XA_STRING)
	{
#if notyet
	  char *result;
	  URL_Struct *url = MSG_ConstructUrlForMessage(m_pane,
												   m_dragmessage);


	  result = XtNewString(url->address);

	  NET_FreeURLStruct(url);

	  return result;
#endif
	}
}

char *
XFE_ThreadView::dragConvert(void *this_ptr,
							Atom atom)
{
  XFE_ThreadView *thread_view = (XFE_ThreadView*)this_ptr;
  
  return thread_view->DragConvert(atom);
}
#endif /* USE_MOTIF_DND */

void
XFE_ThreadView::selectThread()
{
  const int *selected;
  int i,count;
  MessageKey *keys;

  m_outliner->getSelection(&selected, &count);

  if ( count <= 0 ) return;
  keys = new MessageKey[count];

  for ( i = 0; i < count ; i++ )
  {
        keys[i] = MSG_GetMessageKey(m_pane, selected[i]);

  }
  m_outliner->deselectAllItems();

  for ( i = 0; i < count; i++ )
  {
        MSG_ViewIndex index = MSG_ThreadIndexOfMsg( m_pane, keys[ i ] );
        int delta = MSG_ExpansionDelta(m_pane, index);
        if ( delta > 0 )
                MSG_ToggleExpansion(m_pane, index, NULL);
        else
                delta = -delta;
        m_outliner->selectRangeByIndices(index, index+delta);
  }
  delete[] keys;

  showMessage(-1);
}
