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
   FolderView.cpp -- presents view of mail folders and newsgroups/hosts.
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
   */


#include "rosetta.h"
#include "FolderView.h"
#include "SubscribeDialog.h" /* for fe_showSubscribeDialog */
#include "ThreadFrame.h" /* for fe_showMessages */
#if defined(USE_MOTIF_DND)
#include "ThreadView.h"
#endif /* USE_MOTIF_DND */

#include "ViewGlue.h"
#include "Outliner.h"
#if defined(USE_MOTIF_DND)
#include "OutlinerDrop.h"
#endif /* USE_MOTIF_DND */
#include "MozillaApp.h"
#include "NewsServerDialog.h"
#include "xfe.h"
#include "icondata.h"
#include "msgcom.h"
#include "felocale.h" /* for fe_ConvertToLocalEncoding */
#include "xp_mem.h"
#include "Xfe/Xfe.h"

#include "NewsServerPropDialog.h"
#include "NewsgroupPropDialog.h"
#include "MailFolderPropDialog.h"

#include "MNSearchFrame.h"
#include "LdapSearchFrame.h"
#include "MailFilterDlg.h"

#include "prefs.h"


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

#define FOLDER_OUTLINER_GEOMETRY_PREF "mail.folderpane.outliner_geometry"

// bloody entrails dragging behind.... weighing me down...
extern "C" CL_Compositor *fe_create_compositor(MWContext *context);
extern "C" void fe_load_default_font (MWContext *context);
extern "C" void fe_get_final_context_resources (MWContext *context);
extern "C" void fe_sec_logo_cb (Widget, XtPointer, XtPointer);

const char *XFE_FolderView::folderSelected = "XFE_FolderView::folderSelected";
const char *XFE_FolderView::folderDblClicked = "XFE_FolderView::folderDblClicked";
const char *XFE_FolderView::folderAltDblClicked = "XFE_FolderView::folderAltDblClicked";

const int XFE_FolderView::OUTLINER_COLUMN_NAME = 0;
const int XFE_FolderView::OUTLINER_COLUMN_UNREAD = 1;
const int XFE_FolderView::OUTLINER_COLUMN_TOTAL = 2;

#include "xpgetstr.h"
extern int XFE_FOLDER_OUTLINER_COLUMN_NAME;
extern int XFE_FOLDER_OUTLINER_COLUMN_TOTAL;
extern int XFE_FOLDER_OUTLINER_COLUMN_UNREAD;
extern int XFE_DND_MESSAGE_ERROR;

MenuSpec XFE_FolderView::mailserver_popup_menu[] = {
#if 0
  { xfeCmdUpdateMessageCount,	PUSHBUTTON },
#endif
  { xfeCmdNewFolder,		PUSHBUTTON },
  { xfeCmdViewProperties,		PUSHBUTTON },
  { NULL }
};

MenuSpec XFE_FolderView::newshost_popup_menu[] = {
// From the spec:
// "Open Discussion Group Server" -- is that xfeCmdGetNewGroups?
  { xfeCmdAddNewsgroup,			PUSHBUTTON },
#if 0
  { xfeCmdUpdateMessageCount,	PUSHBUTTON },
#endif
  { xfeCmdDeleteFolder,			PUSHBUTTON },
  { xfeCmdViewProperties,		PUSHBUTTON },
  { NULL }
};

MenuSpec XFE_FolderView::mailfolder_popup_menu[] = {
  { xfeCmdOpenFolder,	PUSHBUTTON },
//  { xfeCmdOpenFolderInNewWindow,	PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdNewFolder,		PUSHBUTTON },
  { xfeCmdDeleteFolder,		PUSHBUTTON },
  { xfeCmdRenameFolder,		PUSHBUTTON },
  { xfeCmdCompressFolders,	PUSHBUTTON },
  { xfeCmdCompressAllFolders,	PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdComposeMessage,	PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdSearch,		PUSHBUTTON },
  { xfeCmdViewProperties,	PUSHBUTTON },
  { NULL }
};

MenuSpec XFE_FolderView::newsgroup_popup_menu[] = {
  { xfeCmdOpenSelected,	PUSHBUTTON },
//  open newsgroups in new window should go here
  MENU_SEPARATOR,
  { xfeCmdComposeMessage,	PUSHBUTTON },
  { xfeCmdMarkAllMessagesRead,	PUSHBUTTON },
  { xfeCmdDeleteFolder,		PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdSearch,		PUSHBUTTON },
  { xfeCmdViewProperties,	PUSHBUTTON },
  { NULL }
};

XFE_FolderView::XFE_FolderView(XFE_Component *toplevel_component,
			       Widget parent,
			       XFE_View *parent_view, MWContext *context,
			       MSG_Pane *p)
  : XFE_MNListView(toplevel_component, parent_view, context, p)
{
  // initialize it
  m_popup = NULL;
#ifdef USE_3PANE
  m_clickTimer = 0;
  m_clickData = NULL;
  m_cur_selected = NULL;
  m_cur_count = 0;
#endif

  int num_columns = 3;
  static int default_column_widths[] = {30, 10, 10};

  toplevel_component->registerInterest(XFE_Component::afterRealizeCallback,
				       this,
				       (XFE_FunctionNotification)beforeToplevelShow_cb);

  if (!p)
    setPane(MSG_CreateFolderPane(m_contextData,
				 XFE_MNView::m_master));

  // create our outliner
  m_outliner = new XFE_Outliner("folderList",
								this,
								getToplevel(),
								parent,
								False,  // constantSize
								True,   // hasHeadings
								num_columns, 
								3,
								default_column_widths,
								FOLDER_OUTLINER_GEOMETRY_PREF);

  m_outliner->setPipeColumn( OUTLINER_COLUMN_NAME );
  m_outliner->setMultiSelectAllowed( True );
  m_outliner->setHideColumnsAllowed( True );
#if defined(USE_MOTIF_DND)
  m_outliner->enableDragDrop(this,
							 XFE_FolderView::getDropTargets,
							 XFE_FolderView::getDragTargets,
							 XFE_FolderView::getDragIconData,
							 XFE_FolderView::dragConvert,
							 XFE_FolderView::processTargets);
#else
  m_outliner->setDragType( FE_DND_MAIL_FOLDER, &folderIcon, this, 
			   (fe_dnd_SourceDropFunc)&XFE_FolderView::source_drop_func);
  fe_dnd_CreateDrop(m_outliner->getBaseWidget(), drop_func, this);
#endif /* USE_MOTIF_DND */
  setBaseWidget(m_outliner->getBaseWidget());

  // initialize the icons if they haven't already been
  {
    Pixel bg_pixel;
    
    XtVaGetValues(m_widget, XmNbackground, &bg_pixel, 0);

	initFolderIcons(getToplevel()->getBaseWidget(),
					bg_pixel, getToplevel()->getFGPixel());
  }

  // jump start the folderview's display
  FE_ListChangeStarting(m_pane,
			FALSE, MSG_NotifyAll, 0, 0);
  FE_ListChangeFinished(m_pane,
			FALSE, MSG_NotifyAll, 0, 0);

}

XFE_FolderView::~XFE_FolderView()
{
  D( printf ("~XFE_FolderView()\n");)

  delete m_outliner;

  destroyPane();

  // destroy popup here
  if (m_popup)
    delete m_popup;
}

void
XFE_FolderView::initFolderIcons(Widget widget, Pixel bg_pixel, Pixel fg_pixel)
{
	static Boolean icons_init_done = False;

	if (icons_init_done)
		return;

	icons_init_done = True;

    if (!mailLocalIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &mailLocalIcon,
					   NULL,
					   MN_MailLocal.width, MN_MailLocal.height,
					   MN_MailLocal.mono_bits, MN_MailLocal.color_bits, MN_MailLocal.mask_bits, FALSE);

    if (!mailServerIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &mailServerIcon,
					   NULL,
					   MN_MailLocal.width, MN_MailLocal.height,
					   MN_MailLocal.mono_bits, MN_MailLocal.color_bits, MN_MailLocal.mask_bits, FALSE);

    if (!inboxIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &inboxIcon,
					   NULL, 
					   MN_Inbox.width, MN_Inbox.height,
					   MN_Inbox.mono_bits, MN_Inbox.color_bits, MN_Inbox.mask_bits, FALSE);
    
    if (!inboxOpenIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &inboxOpenIcon,
					   NULL, 
					   MN_InboxO.width, MN_InboxO.height,
					   MN_InboxO.mono_bits, MN_InboxO.color_bits, MN_InboxO.mask_bits, FALSE);
	
    if (!draftsIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &draftsIcon,
					   NULL, 
					   MN_Draft.width, MN_Draft.height,
					   MN_Draft.mono_bits, MN_Draft.color_bits, MN_Draft.mask_bits, FALSE);
	
    if (!draftsOpenIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &draftsOpenIcon,
					   NULL, 
					   MN_DraftO.width, MN_DraftO.height,
					   MN_DraftO.mono_bits, MN_DraftO.color_bits, MN_DraftO.mask_bits, FALSE);
	
    if (!templatesIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &templatesIcon,
					   NULL, 
					   MN_Template.width, MN_Template.height,
					   MN_Template.mono_bits, MN_Template.color_bits, MN_Template.mask_bits, FALSE);
	
    if (!templatesOpenIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &templatesOpenIcon,
					   NULL, 
					   MN_TemplateO.width, MN_TemplateO.height,
					   MN_TemplateO.mono_bits, MN_TemplateO.color_bits, MN_TemplateO.mask_bits, FALSE);
	
    if (!outboxIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &outboxIcon,
					   NULL, 
					   MN_Outbox.width, MN_Outbox.height,
					   MN_Outbox.mono_bits, MN_Outbox.color_bits, MN_Outbox.mask_bits, FALSE);
	
    if (!outboxOpenIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &outboxOpenIcon,
					   NULL, 
					   MN_OutboxO.width, MN_OutboxO.height,
					   MN_OutboxO.mono_bits, MN_OutboxO.color_bits, MN_OutboxO.mask_bits, FALSE);

    if (!folderIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &folderIcon,
					   NULL, 
					   MN_Folder.width, MN_Folder.height,
					   MN_Folder.mono_bits, MN_Folder.color_bits, MN_Folder.mask_bits, FALSE);
	
    if (!folderOpenIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &folderOpenIcon,
					   NULL, 
					   MN_FolderO.width, MN_FolderO.height,
					   MN_FolderO.mono_bits, MN_FolderO.color_bits, MN_FolderO.mask_bits, FALSE);
	
    if (!folderServerIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &folderServerIcon,
					   NULL, 
					   MN_FolderServer.width, MN_FolderServer.height,
					   MN_FolderServer.mono_bits, MN_FolderServer.color_bits, MN_FolderServer.mask_bits, FALSE);
	
    if (!folderServerOpenIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &folderServerOpenIcon,
					   NULL, 
					   MN_FolderServerO.width, MN_FolderServerO.height,
					   MN_FolderServerO.mono_bits, MN_FolderServerO.color_bits, MN_FolderServerO.mask_bits, FALSE);
	
    if (!newsServerIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &newsServerIcon,
					   NULL, 
					   MN_NewsServer.width, MN_NewsServer.height,
					   MN_NewsServer.mono_bits, MN_NewsServer.color_bits, MN_NewsServer.mask_bits, FALSE);

    HG39875	
    if (!newsgroupIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &newsgroupIcon,
					   NULL, 
					   MN_Newsgroup.width, MN_Newsgroup.height,
					   MN_Newsgroup.mono_bits, MN_Newsgroup.color_bits, MN_Newsgroup.mask_bits, FALSE);
	
    if (!trashIcon.pixmap)
		fe_NewMakeIcon(widget,
					   fg_pixel,
					   bg_pixel,
					   &trashIcon,
					   NULL, 
					   MN_TrashSmall.width, MN_TrashSmall.height,
					   MN_TrashSmall.mono_bits, MN_TrashSmall.color_bits, MN_TrashSmall.mask_bits, FALSE);
}

void
XFE_FolderView::paneChanged(XP_Bool asynchronous,
							MSG_PANE_CHANGED_NOTIFY_CODE code,
							int32 value)
{
	switch (code)
		{
		case MSG_PaneNotifySelectNewFolder:
			{
				/* check value
				 */
				if (value >=0) {
					m_outliner->makeVisible(value);
					m_outliner->selectItemExclusive(value);
				}/* if */
#if defined(DEBUG_tao)
				else {
				  printf("\nXFE_FolderView::paneChanged, value=%d", value);
				  XP_ASSERT(value >=0);
				}/* else */
#endif
				break;
			}

		case MSG_PaneNotifyFolderDeleted:
			XFE_MozillaApp::theApp()->notifyInterested(XFE_MNView::folderDeleted,
													   (void *)value);

			break;

		default:
			break;
		}

  XFE_MNView::paneChanged(asynchronous, code, value);
}

XFE_CALLBACK_DEFN(XFE_FolderView, beforeToplevelShow)(XFE_NotificationCenter *,
						    void *, void*)
{
}

void
XFE_FolderView::selectFolder(MSG_FolderInfo *info)
{
  MSG_ViewIndex index;
  
  index = MSG_GetFolderIndexForInfo(m_pane, info, TRUE);
  
  if (index != MSG_VIEWINDEXNONE)
    {
      m_outliner->makeVisible(index);
      m_outliner->selectItemExclusive(index);

	  getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
    }
}

Boolean
XFE_FolderView::isCommandEnabled(CommandType cmd, void *call_data, XFE_CommandInfo* info)
{
  const int *selected;
  int count;

  m_outliner->getSelection(&selected, &count);

#define IS_CMD(command) cmd == (command)
  if (// from the FolderFrame edit menu.
      IS_CMD(xfeCmdFindInObject)
      || IS_CMD(xfeCmdFindAgain))
    {
      return !XP_IsContextBusy(m_contextData) && count > 0;
    }
#ifdef DEBUG_akkana
  else if ( IS_CMD(xfeCmdExpand)
      || IS_CMD(xfeCmdExpandAll)
      || IS_CMD(xfeCmdCollapse)
      || IS_CMD(xfeCmdCollapseAll)
            )
      return True;
#endif
  HG07326
  else if (IS_CMD(xfeCmdViewProperties))
	  {
		  return count == 1;
	  }
  else if (IS_CMD(xfeCmdUpdateMessageCount))
	  {
		  return !XP_IsContextBusy(m_contextData);
	  }
  else if (IS_CMD(xfeCmdSearch) )
  {
     // Find current selected folder, if none, disable this button
			
     if (XP_IsContextBusy(m_contextData) ) return False;

     const int *selected;
     int count;

     m_outliner->getSelection(&selected, &count);
			
     if ( count > 0 ) 
       return True;
     else 
       return False;
  }
  else if ( IS_CMD(xfeCmdSearchAddress))
	  {
		  return !XP_IsContextBusy(m_contextData);
	  }
  else if (IS_CMD(xfeCmdDeleteAny))
    {
      return XFE_MNListView::isCommandEnabled(xfeCmdDeleteFolder);
    }
  else if (IS_CMD(xfeCmdEditPreferences) || IS_CMD(xfeCmdNewNewsHost))
    {
      return True;
    }
  else if (IS_CMD(xfeCmdEditConfiguration) )
  {
        MSG_FolderLine line;
        if (!MSG_GetFolderLineByIndex(m_pane, selected[0], 1, &line))
            return False;
	return (line.flags & MSG_FOLDER_FLAG_MAIL);
  }
  else if (IS_CMD(xfeCmdModerateDiscussion) )
  {
        MSG_FolderLine line;
        if (!MSG_GetFolderLineByIndex(m_pane, selected[0], 1, &line))
            return False;
	return (line.flags & MSG_FOLDER_FLAG_NEWSGROUP);
  }
  else if (IS_CMD(xfeCmdOpenSelected))
    {
        MSG_FolderLine line;
        if (!MSG_GetFolderLineByIndex(m_pane, selected[0], 1, &line))
            return False;
        return ((line.flags & MSG_FOLDER_FLAG_NEWSGROUP)
                || ((line.flags & MSG_FOLDER_FLAG_MAIL)
                    && (line.level > 1)));
    }
  else if (IS_CMD(xfeCmdNewFolder) && count == 1)
    {
        MSG_FolderLine line;
        if (!MSG_GetFolderLineByIndex(m_pane, selected[0], 1, &line))
            return False;
        XP_Bool selectable;
					
        if (line.flags & MSG_FOLDER_FLAG_NEWSGROUP
            || line.flags & MSG_FOLDER_FLAG_NEWS_HOST)
            MSG_CommandStatus(m_pane, MSG_NewNewsgroup,
                              (MSG_ViewIndex*)selected, count, 
                              &selectable, NULL, NULL, NULL);
        else
            MSG_CommandStatus(m_pane, MSG_NewFolder,
                              (MSG_ViewIndex*)selected, count, 
                              &selectable, NULL, NULL, NULL);
        return selectable;
    }
  else if (IS_CMD(xfeCmdRenameFolder) && count == 1)
    {
        MSG_FolderLine line;
        if (!MSG_GetFolderLineByIndex(m_pane, selected[0], 1, &line))
            return False;
        XP_Bool selectable;
					
        if (line.flags & MSG_FOLDER_FLAG_MAIL)
            MSG_CommandStatus(m_pane, MSG_NewFolder,
                              (MSG_ViewIndex*)selected, count, 
                              &selectable, NULL, NULL, NULL);
        return selectable;
    }
  else
    {
      return XFE_MNListView::isCommandEnabled(cmd, call_data, info);
    }
#undef IS_CMD
}

Boolean
XFE_FolderView::handlesCommand(CommandType cmd, void *, XFE_CommandInfo*)
{
#define IS_CMD(command) cmd == (command)

  if (IS_CMD(xfeCmdOpenSelected)
      || IS_CMD(xfeCmdEditPreferences)
      || IS_CMD(xfeCmdOpenPage)
      || IS_CMD(xfeCmdSaveAs)
      || IS_CMD(xfeCmdNewFolder)
      || IS_CMD(xfeCmdRenameFolder)
      || IS_CMD(xfeCmdEmptyTrash)
      || IS_CMD(xfeCmdCompressFolders)
      || IS_CMD(xfeCmdCompressAllFolders)
      || IS_CMD(xfeCmdAddNewsgroup)
      || IS_CMD(xfeCmdNewNewsHost)
      || IS_CMD(xfeCmdInviteToNewsgroup)
      || IS_CMD(xfeCmdPrintSetup)
      || IS_CMD(xfeCmdPrintPreview)
      || IS_CMD(xfeCmdPrint)

      || IS_CMD(xfeCmdSearch)
      || IS_CMD(xfeCmdSearchAddress)

      || IS_CMD(xfeCmdUndo)
      || IS_CMD(xfeCmdRedo)
      || IS_CMD(xfeCmdCut)
      || IS_CMD(xfeCmdCopy)
      || IS_CMD(xfeCmdPaste)

      || IS_CMD(xfeCmdGetNewMessages)

      || IS_CMD(xfeCmdExpand)
      || IS_CMD(xfeCmdExpandAll)
      || IS_CMD(xfeCmdCollapse)
      || IS_CMD(xfeCmdCollapseAll)

#ifdef DEBUG_akkana
      || IS_CMD(xfeCmdMoveFoldersInto)
#endif

      || IS_CMD(xfeCmdMarkAllMessagesRead)
      || IS_CMD(xfeCmdSendMessagesInOutbox)
      || IS_CMD(xfeCmdDeleteAny)
      || IS_CMD(xfeCmdDeleteFolder)
      HG82883
	  || IS_CMD(xfeCmdViewProperties)
      || IS_CMD(xfeCmdUpdateMessageCount)
      || IS_CMD(xfeCmdShowPopup))
    {
      return True;
    }
  else
    {
      return XFE_MNListView::handlesCommand(cmd);
    }

#undef IS_CMD
}

void
XFE_FolderView::doCommand(CommandType cmd,
						  void * calldata, 
						  XFE_CommandInfo* info)
{
#define IS_CMD(command) cmd == (command)  
	const int *selected;
	int count;
	
	m_outliner->getSelection(&selected, &count);

D(	printf ("in XFE_FolderView::doCommand()\n");)

	if (IS_CMD(xfeCmdShowPopup))
		{
			XP_ASSERT(info);
			XEvent *event = (XEvent *)info->event;
			int x, y, clickrow;

			Widget w = XtWindowToWidget(event->xany.display, event->xany.window);
			MSG_FolderLine clickline;
			MenuSpec *spec;
			int depth;
			
			if (w == NULL) w = m_widget;
			
			/* we create a new popup each time. */
			if (m_popup)
				delete m_popup;

			/* Need to switch focus for the popup menu so that menu items can be enabled in 
			   the three pane world */

			getToplevel()->notifyInterested(XFE_MNListView::changeFocus, (void*)this);
			
			m_popup = new XFE_PopupMenu("popup",
										(XFE_Frame*)m_toplevel, // XXXXXXX
										w,
										NULL);
			
			m_outliner->translateFromRootCoords(event->xbutton.x_root,
												event->xbutton.y_root,
												&x, &y);
			
			// figure out which row they clicked in, and choose the
			// right popup menu.
			clickrow = m_outliner->XYToRow(x, y);
			
			D( printf ("(x,y) = (%d,%d). row is %d\n", x, y, clickrow);)
			
			if (clickrow == -1)
				return;
			
			if (! m_outliner->isSelected(clickrow))
				m_outliner->selectItemExclusive(clickrow);
			
			acquireLineData(clickrow);
			getTreeInfo(NULL, NULL, &depth, NULL);
			releaseLineData();
			
			if (!MSG_GetFolderLineByIndex(m_pane, clickrow, 1, &clickline))
				return;
			
			if (depth < 1)
				{
					if (clickline.flags & MSG_FOLDER_FLAG_NEWS_HOST)
						spec = newshost_popup_menu;
					else
						spec = mailserver_popup_menu;
				}
			else
				{
					if (clickline.flags & MSG_FOLDER_FLAG_NEWSGROUP)
						spec = newsgroup_popup_menu;
					else
						spec = mailfolder_popup_menu;
				}
			
			m_popup->addMenuSpec(spec);
			
			m_popup->position(event);
			
			m_popup->show();
		}
	else if (IS_CMD(xfeCmdEditPreferences))
		{
			fe_showMailNewsPreferences(getToplevel(), m_contextData);
		}
	else if (IS_CMD(xfeCmdOpenSelected))
		{
			int i;
			
			for (i = 0; i < count; i ++)
				{
					MSG_FolderLine line;
					
					if (!MSG_GetFolderLineByIndex(m_pane, selected[i], 1, &line)) continue;
					
					if (!(line.level == 1 && line.flags & MSG_FOLDER_FLAG_DIRECTORY))
						fe_showMessages(XtParent(getToplevel()->getBaseWidget()), 
										/* Tao: we might need to check if this returns a 
										 * non-NULL frame
										 */
										ViewGlue_getFrame(m_contextData),
										NULL,
										MSG_GetFolderInfo(m_pane, selected[i]),
										FALSE, FALSE, MSG_MESSAGEKEYNONE);
				}
		}
	else if (IS_CMD(xfeCmdGetNewMessages))
		{
			getNewMail();
		}
	else if (IS_CMD(xfeCmdSendMessagesInOutbox))
		{
			D(	printf ("MSG_FolderPane::sendMessagesInOutbox()\n");)
				
			MSG_Command(m_pane, MSG_DeliverQueuedMessages, (MSG_ViewIndex*)selected, count);
		}
	else if (IS_CMD(xfeCmdAddNewsgroup))
		{
			D(	printf ("MSG_FolderPane::addNewsgroup()\n");)

			MSG_FolderLine line;
			MSG_Host *host = NULL;

			if (MSG_GetFolderLineByIndex(m_pane, selected[0], 1, &line))
				{
					if (line.flags & MSG_FOLDER_FLAG_NEWS_HOST
                        || line.flags & MSG_FOLDER_FLAG_IMAP_SERVER)
						{
							host = MSG_GetHostFromIndex(m_pane, selected[0]);
#ifdef DEBUG_akkana
                            if (!host)
                                printf("Couldn't get host from index\n");
#endif
						}
					else if (line.flags & MSG_FOLDER_FLAG_NEWSGROUP
                             || line.flags & MSG_FOLDER_FLAG_IMAPBOX)
						{
							MSG_FolderInfo *info = MSG_GetFolderInfo(m_pane, selected[0]);
							
							if (info)
								host = MSG_GetHostForFolder(info);
#ifdef DEBUG_akkana
                            else
                                printf("Couldn't get folder info\n");
                            if (info && !host)
                                printf("Couldn't get host for folder\n");
#endif
						}
				}
			
			fe_showSubscribeDialog(getToplevel(), getToplevel()->getBaseWidget(), m_contextData, host);
		}
	else if (IS_CMD(xfeCmdNewFolder))
		{
			D(	printf ("MSG_FolderPane::newFolder()\n");)

			MSG_FolderLine line;
			
			if (MSG_GetFolderLineByIndex(m_pane, selected[0], 1, &line))
				{
					if (line.flags & MSG_FOLDER_FLAG_NEWSGROUP
						|| line.flags & MSG_FOLDER_FLAG_NEWS_HOST)
                        MSG_Command(m_pane, MSG_NewNewsgroup, (MSG_ViewIndex*)selected, count);
//						fe_showSubscribeDialog(getToplevel(), getToplevel()->getBaseWidget(), m_contextData);
					else
						{
							MSG_Command(m_pane, MSG_NewFolder, (MSG_ViewIndex*)selected, count);
							XFE_MozillaApp::theApp()->notifyInterested(XFE_MNView::foldersHaveChanged);
						}
				}
		}
	else if (IS_CMD(xfeCmdRenameFolder))
		{
		D(	printf ("MSG_FolderPane::renameFolder()\n");)
			
			MSG_Command(m_pane, MSG_DoRenameFolder, (MSG_ViewIndex*)selected, count);
			XFE_MozillaApp::theApp()->notifyInterested(XFE_MNView::foldersHaveChanged);
		}
	else if (IS_CMD(xfeCmdEmptyTrash))
		{
		D(	printf ("MSG_FolderPane::emptyTrash()\n");)
			
			MSG_Command(m_pane, MSG_EmptyTrash, (MSG_ViewIndex*)selected, count);
		}
	else if (IS_CMD(xfeCmdCompressFolders))
		{
		D(	printf ("MSG_FolderPane::compressFolders()\n");)
			
			MSG_Command(m_pane, MSG_CompressFolder, (MSG_ViewIndex*)selected, count);
		}
	else if (IS_CMD(xfeCmdCompressAllFolders))
		{
		D(	printf ("MSG_FolderPane::compressAllFolders()\n");)
			
			MSG_Command(m_pane, MSG_CompressAllFolders, (MSG_ViewIndex*)selected, count);
		}
	else if (IS_CMD(xfeCmdDeleteFolder) || IS_CMD(xfeCmdDeleteAny) )
		{
		D(	printf ("MSG_FolderPane::deleteFolder()\n");)
			MSG_Command(m_pane, MSG_DeleteFolder, (MSG_ViewIndex*)selected, count);
			XFE_MozillaApp::theApp()->notifyInterested(XFE_MNView::foldersHaveChanged);
		}
	HG20388
	else if (IS_CMD(xfeCmdEditMailFilterRules))
		{
			fe_showMailFilterDlg(getToplevel()->getBaseWidget(), 
								 m_contextData,m_pane);
			
		}
	else if (IS_CMD(xfeCmdSearchAddress))
		{
		DD(printf ("searchAddress\n");)
			fe_showLdapSearch(XfeAncestorFindApplicationShell(getToplevel()->getBaseWidget()), 
							  /* Tao: we might need to check if this returns a 
							   * non-NULL frame
							   */
							  ViewGlue_getFrame(m_contextData),
							  (Chrome*)NULL);
			
		}
	else if (IS_CMD(xfeCmdViewProperties))
		{
			MSG_FolderInfo *info = MSG_GetFolderInfo(m_pane, selected[0]);
			MSG_FolderLine line;

			D(printf("viewProperties\n");)
			if (MSG_GetFolderLineById(XFE_MNView::m_master, info, &line))
				{
					if (line.flags & MSG_FOLDER_FLAG_NEWS_HOST)
						{
							MSG_NewsHost *host = MSG_GetNewsHostFromIndex(m_pane, selected[0]);

							fe_showNewsServerProperties(getToplevel()->getBaseWidget(),
														m_contextData,
														host);
						}
					else if (line.flags & MSG_FOLDER_FLAG_NEWSGROUP)
						{
							fe_showNewsgroupProperties(getToplevel()->getBaseWidget(),
													   m_contextData,
													   info);
						}
					else if (line.flags & MSG_FOLDER_FLAG_MAIL)
						{
							if (line.level == 1)
								fe_showMailServerPreferences(getToplevel(), m_contextData);
							else
								fe_showMailFolderProperties(getToplevel()->getBaseWidget(),
															m_contextData,
															m_pane,
															info);
						}
				}
		}
	else if (IS_CMD(xfeCmdSearch))
		{
		DD(printf ("searchMailNews\n");)
			
			// Find current selected folder
			
			const int *selected;
			int count;
			MSG_FolderInfo *folderInfo;
			
			m_outliner->getSelection(&selected, &count);
			
			if ( count > 0 )
				{
				DD(printf("### Search Selected Folder %d\n", selected[0]);)
					folderInfo = MSG_GetFolderInfo(m_pane, (MSG_ViewIndex)(selected[0]));
				}
			else
				{
				DD(printf("### Search All Folder\n");)
					folderInfo = NULL;
				}
			// We have to use the parent of the base widget to get
			// to the real top level widget. By doing so, when dispatching
			// notification, SearchFrame will only get to its registerred
			// event this way. in fe_WidgetToContext, there is an intent
			// to get to the wrong frame if we don't use the real toplevel
			// as the base for the toplevel frames ... dora 12/31/96
			fe_showMNSearch(XfeAncestorFindApplicationShell(getToplevel()->getBaseWidget()), 
							/* Tao: we might need to check if this returns a 
							 * non-NULL frame
							 */
							ViewGlue_getFrame(m_contextData),
							NULL, this,
							folderInfo);
		}
#ifdef DEBUG_akkana
    else if (IS_CMD(xfeCmdMoveFoldersInto))
		{
            MSG_MoveFoldersInto(m_pane, (MSG_ViewIndex*)selected, count,
                                (MSG_FolderInfo*)info);
            XFE_MozillaApp::theApp()->notifyInterested(XFE_MNView::foldersHaveChanged);
		}
    else if (IS_CMD(xfeCmdExpand) || IS_CMD(xfeCmdExpandAll)
             || IS_CMD(xfeCmdCollapse) || IS_CMD(xfeCmdCollapseAll))
		{
            // Nope, these are the cmds for the thread win, not folder win. :-(
            //MSG_CommandType msg_cmd = commandToMsgCmd(cmd);
            //MSG_Command(m_pane, msg_cmd, (MSG_ViewIndex*)selected, count);
		}
#endif
    else if (IS_CMD(xfeCmdNewNewsHost) /* || IS_CMD(xfeCmdAddNewsServer) */)
         {
            // This code taken out of SubAllView::doCommand():
             XFE_NewsServerDialog *d = new XFE_NewsServerDialog(getToplevel()->getBaseWidget(),
                                                                "addServer",
										   /* Tao: we might need to check if this returns a 
											* non-NULL frame
											*/
                                                                ViewGlue_getFrame(m_contextData));
             XP_Bool ok_pressed = d->post();

             if (ok_pressed)
             {
                 const char *serverName = d->getServer();
                 int serverPort = d->getPort();
                 XP_Bool isxxx = HG72988;
                 MSG_NewsHost *host;

                 host = MSG_CreateNewsHost(XFE_MNView::getMaster(),
                                           serverName,
                                           isxxx,
                                           serverPort);
					
                 XFE_MozillaApp::theApp()->notifyInterested(XFE_MNView::foldersHaveChanged);
             }

             delete d;
		}
    else if (IS_CMD(xfeCmdMarkAllMessagesRead))
		{
			MSG_Command(m_pane, MSG_MarkAllRead,
                        (MSG_ViewIndex*)selected, count);
//			XFE_MozillaApp::theApp()->notifyInterested(XFE_MNView::foldersHaveChanged);
		}
    else if (IS_CMD(xfeCmdEditConfiguration) ||
		 IS_CMD(xfeCmdModerateDiscussion) )
    		{
            		MSG_CommandType msg_cmd = commandToMsgCmd(cmd);
            		MSG_Command(m_pane, msg_cmd, (MSG_ViewIndex*)selected, count);
			return;
    		}
	/* Do it now
	 */
	else if ( (cmd == xfeCmdComposeMessage) 
			  || (cmd == xfeCmdComposeArticle) ) {
        if (info) {
			CONTEXT_DATA(m_contextData)->stealth_cmd =
				((info->event->type == ButtonRelease) &&
				 (info->event->xkey.state & ShiftMask));
		}

		MSG_Command(m_pane, commandToMsgCmd(cmd), 
					(MSG_ViewIndex*)selected, count);
	}
	else if ( (cmd == xfeCmdComposeMessageHTML) 
			  || (cmd == xfeCmdComposeArticleHTML) ) {
		CONTEXT_DATA(m_contextData)->stealth_cmd = 
			(fe_globalPrefs.send_html_msg == False);
		MSG_Command(m_pane, commandToMsgCmd(cmd), 
					(MSG_ViewIndex*)selected, count);
	}
	else if ( (cmd == xfeCmdComposeMessagePlain)
			  || (cmd == xfeCmdComposeArticlePlain) ) {
		CONTEXT_DATA(m_contextData)->stealth_cmd = 
			(fe_globalPrefs.send_html_msg == True) ;
		MSG_Command(m_pane, commandToMsgCmd(cmd), 
					(MSG_ViewIndex*)selected, count);
	}
	/* End of New message
	 */
	else
		{
			XFE_MNListView::doCommand(cmd, calldata, info);
			return;
		}
	
	getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);

D(	printf ("leaving XFE_FolderView::doCommand()\n");)
#undef IS_CMD
}

char *
XFE_FolderView::commandToString(CommandType cmd, void *calldata, XFE_CommandInfo* info)
{
#define IS_CMD(command) cmd == (command)  
	const int *selected;
	int count;
	MSG_FolderLine line;
	
	m_outliner->getSelection(&selected, &count);
	
	// this doesn't handle selections with more than one element in them.
	
	if (IS_CMD(xfeCmdNewFolder)) 
		{
			if (count != 1)
				return stringFromResource("newFolderCmdString");
			
			MSG_GetFolderLineByIndex(m_pane, selected[0], 1, &line);
			if (line.flags & MSG_FOLDER_FLAG_NEWSGROUP
				|| line.flags & MSG_FOLDER_FLAG_NEWS_HOST)
				return stringFromResource("newNewsgroupCmdString");
			else /* (line.flags & MSG_FOLDER_FLAG_MAIL) */
				if (line.level == 1)
					return stringFromResource("newFolderCmdString");
				else
					return stringFromResource("newSubFolderCmdString");
		}
	else if (IS_CMD(xfeCmdOpenSelected))
		{
			if (count != 1)
				return stringFromResource("openFolderCmdString");
			
			MSG_GetFolderLineByIndex(m_pane, selected[0], 1, &line);
			
			if ((line.flags & MSG_FOLDER_FLAG_NEWSGROUP)
                || (line.flags & MSG_FOLDER_FLAG_NEWS_HOST))
				return stringFromResource("openNewsgroupCmdString");
			else
				return stringFromResource("openFolderCmdString");
		}
	else if (IS_CMD(xfeCmdDeleteFolder) || IS_CMD(xfeCmdDeleteAny))
		{
			if (count != 1)
				return stringFromResource("deleteFolderCmdString");
			
			MSG_GetFolderLineByIndex(m_pane, selected[0], 1, &line);
#if defined(DEBUG_tao)
			printf("\n  line.flags=%d", line.flags);
#endif
			if (line.flags & MSG_FOLDER_FLAG_NEWS_HOST)
				return stringFromResource("deleteNewsHostCmdString");
			else if (line.flags & MSG_FOLDER_FLAG_NEWSGROUP)
				return stringFromResource("unsubscribeNewsgroupCmdString");
			else /* (line.flags & MSG_FOLDER_FLAG_MAIL) */
				if (line.level == 1)
					return stringFromResource("deleteMailHostCmdString");
				else
					return stringFromResource("deleteFolderCmdString");
		}
	else if (IS_CMD(xfeCmdViewProperties))
		{
			if (count != 1)
				return stringFromResource("folderPropsCmdString");
			
			MSG_GetFolderLineByIndex(m_pane, selected[0], 1, &line);
			
			if (line.flags & MSG_FOLDER_FLAG_NEWSGROUP)
				return stringFromResource("newsgroupPropsCmdString");
			else if (line.flags & MSG_FOLDER_FLAG_NEWS_HOST)
				return stringFromResource("newsServerPropsCmdString");
			else /* mail */
				if (line.level == 1)
					return stringFromResource("mailServerPropsCmdString");
				else
					return stringFromResource("folderPropsCmdString");
		}
	else
		{
			return XFE_MNListView::commandToString(cmd, calldata, info);
		}
	
#undef IS_CMD
}

fe_icon*
XFE_FolderView::treeInfoToIcon(int depth, int flags, XP_Bool expanded, XP_Bool xxxe)
{
	if (depth < 1)
		{
			if (flags & MSG_FOLDER_FLAG_NEWS_HOST)
			  return HG17272 &newsServerIcon;
			else if (flags & MSG_FOLDER_FLAG_IMAPBOX)
				return &mailServerIcon;
			else
				return &mailLocalIcon;
		}

	if (flags & MSG_FOLDER_FLAG_NEWSGROUP)
		return HG00288 &newsgroupIcon;
	else if (flags & MSG_FOLDER_FLAG_INBOX)
		return expanded ? &inboxOpenIcon : &inboxIcon;
	else if (flags & MSG_FOLDER_FLAG_DRAFTS)
		return expanded ? &draftsOpenIcon : &draftsIcon;
	else if (flags & MSG_FOLDER_FLAG_TEMPLATES)
		return expanded ? &templatesOpenIcon : &templatesIcon;
	else if (flags & MSG_FOLDER_FLAG_TRASH)
		return &trashIcon;
	else if (flags & MSG_FOLDER_FLAG_QUEUE)
		return expanded ? &outboxOpenIcon : &outboxIcon;
	else if (flags & MSG_FOLDER_FLAG_IMAPBOX)
		return expanded ? &folderServerOpenIcon : &folderServerIcon;
	else
		return expanded ? &folderOpenIcon : &folderIcon;
}

#if defined(USE_MOTIF_DND)
fe_icon_data *
XFE_FolderView::treeInfoToIconData(int depth, int flags, XP_Bool expanded)
{
	if (depth < 1)
		{
			if (flags & MSG_FOLDER_FLAG_NEWS_HOST)
				return &MN_NewsServer;
			else if (flags & MSG_FOLDER_FLAG_IMAPBOX)
				return &MN_MailLocal; /* XXX ? */
			else
				return &MN_MailLocal;
		}

	if (flags & MSG_FOLDER_FLAG_NEWSGROUP)
		return &MN_Newsgroup;
	else if (flags & MSG_FOLDER_FLAG_INBOX)
		return expanded ? &MN_InboxO : &MN_Inbox;
	else if (flags & MSG_FOLDER_FLAG_DRAFTS)
		return expanded ? &MN_DraftO : &MN_Draft;
	else if (flags & MSG_FOLDER_FLAG_TEMPLATES)
		return expanded ? &MN_TemplateO : &MN_Template;
	else if (flags & MSG_FOLDER_FLAG_TRASH)
		return &MN_TrashSmall;
	else if (flags & MSG_FOLDER_FLAG_QUEUE)
		return expanded ? &MN_OutboxO : &MN_Outbox;
	else if (flags & MSG_FOLDER_FLAG_IMAPBOX)
		return expanded ? &MN_FolderServerO : &MN_FolderServer;
	else
		return expanded ? &MN_FolderO : &MN_Folder;
}
#endif /* USE_MOTIF_DND */

/* Outlinable interface methods */
char *
XFE_FolderView::getColumnTextByFolderLine(MSG_FolderLine* folderLine, 
										  int column)
{
	static char unseen[10];
	static char total[10];
	
	switch (column)
		{
		case OUTLINER_COLUMN_NAME:
			{
				if (folderLine->prettyName)
					return (char*)folderLine->prettyName;
				else
					return (char*)folderLine->name;
			}
		
		case OUTLINER_COLUMN_UNREAD:
			{
				if (folderLine->level == 1
					&& folderLine->flags & MSG_FOLDER_FLAG_DIRECTORY)
					{
						return "---";
					}
				else
					{
						/* if we know how many unseen messages there are, 
						   display the number. otherwise, we put ???'s */
						if (folderLine->unseen >= 0)
							{
								PR_snprintf(unseen, sizeof (unseen), "%d", folderLine->unseen);
								
								return unseen;
							}
						else 
							{
								return "???";
							}
					}
			}
		case OUTLINER_COLUMN_TOTAL:
			{
				/* if the folder is a directory, just put dashes
				   for the total messages. */
				if (folderLine->level == 1
					&& folderLine->flags & MSG_FOLDER_FLAG_DIRECTORY)
					{
						return "---";
					}
				else
					{
						/* if we know how many unseen messages there are, 
						   we also know how many total messages there are.
						   otherwise, we put ???'s */
						if (folderLine->total >= 0)
							{
								PR_snprintf(total, sizeof (total), "%d", folderLine->total);
								
								return total;
							}
						else 
							{
								return "???";
							}
					}
			}
		default:
			XP_ASSERT(0);
			return NULL;
		}
}

char *XFE_FolderView::getCellTipString(int row, int column)
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
		m_ancestorInfo = NULL;

		MSG_FolderLine* folderLine = 
			(MSG_FolderLine *) XP_CALLOC(1, sizeof(MSG_FolderLine));

		if (!MSG_GetFolderLineByIndex(m_pane, row, 1, folderLine))
			return NULL;

		/* static array; do not free
		 */
		tmp = getColumnTextByFolderLine(folderLine, column);
		XP_FREEIF(folderLine);
	}/* else */
	if (tmp && 
		(!m_outliner->isColTextFit(tmp, row, column)))
		return tmp;

	return NULL;
}

char *XFE_FolderView::getCellDocString(int /* row */, int /* column */)
{
	return NULL;
}

void *
XFE_FolderView::ConvFromIndex(int index)
{
  return MSG_GetFolderInfo(m_pane, index);
}

int
XFE_FolderView::ConvToIndex(void *item)
{
  MSG_ViewIndex index = MSG_GetFolderIndex(m_pane, (MSG_FolderInfo*)item);

  if (index == MSG_VIEWINDEXNONE)
    return -1;
  else
    return index;
}

void *
XFE_FolderView::acquireLineData(int line)
{
  m_ancestorInfo = NULL;

  if (!MSG_GetFolderLineByIndex(m_pane, line, 1, &m_folderLine))
    return NULL;

  if ( m_folderLine.level > 0 ) 
    {
      m_ancestorInfo = new OutlinerAncestorInfo[ m_folderLine.level ];
    }
  else
    {
      m_ancestorInfo = new OutlinerAncestorInfo[ 1 ];
    }
      
  // ripped straight from the winfe
  int i = m_folderLine.level - 1;
  int idx = line + 1;
  int total_lines = m_outliner->getTotalLines();
  while ( i >  0 ) {
    if ( idx < total_lines ) {
		int level = MSG_GetFolderLevelByIndex(m_pane, idx);
		if ( (level - 1) == i ) {
			m_ancestorInfo[i].has_prev = TRUE;
			m_ancestorInfo[i].has_next = TRUE;
			i--;
			idx++;
		} else if ( (level - 1) < i ) {
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
  
  return &m_folderLine;
}

void
XFE_FolderView::getTreeInfo(XP_Bool *expandable,
							XP_Bool *is_expanded,
							int *depth,
							OutlinerAncestorInfo **ancestor)
{
  XP_Bool is_line_expandable;
  XP_Bool is_line_expanded;

  is_line_expandable = (m_folderLine.numChildren > 0);
  if (is_line_expandable)
    {
      is_line_expanded = !(m_folderLine.flags & MSG_FOLDER_FLAG_ELIDED);
    }
  else
    {
      is_line_expanded = FALSE;
    }

  if (ancestor)
    *ancestor = m_ancestorInfo;

  if (depth)
    *depth = m_folderLine.level - 1;

  if (expandable)
    *expandable =  is_line_expandable;

  if (is_expanded)
    *is_expanded = is_line_expanded;
}

EOutlinerTextStyle 
XFE_FolderView::getColumnStyle(int /*column*/)
{
  return (m_folderLine.unseen > 0) ? OUTLINER_Bold : OUTLINER_Default;
}

#define A_BIG_BUFFER_SIZE 1024 /* I have no idea what the right size is */

char *
XFE_FolderView::getColumnText(int column)
{
	static char unseen_a[10]; /* uniquify name for IRIX5.3 */
	static char total_a[10];
	static char a_line[A_BIG_BUFFER_SIZE];
	char *loc;
	
	switch (column)
		{
		case OUTLINER_COLUMN_NAME:
			{
				if (m_folderLine.prettyName) {
					XP_STRNCPY_SAFE(a_line, m_folderLine.prettyName, sizeof(a_line));
				}
				else {
					XP_STRNCPY_SAFE(a_line, m_folderLine.name, sizeof(a_line));
				}
				INTL_CONVERT_BUF_TO_LOCALE(a_line, loc);
				return a_line;
			}
		
		case OUTLINER_COLUMN_UNREAD:
			{
				if (m_folderLine.level == 1
					&& m_folderLine.flags & MSG_FOLDER_FLAG_DIRECTORY)
					{
						return "---";
					}
				else
					{
						/* if we know how many unseen messages there are, 
						   display the number. otherwise, we put ???'s */
#if defined(DEBUG_tao)
						if (m_folderLine.unseen < 0)
							printf("\n**OUTLINER_COLUMN_UNREAD=%d\n", 
								   m_folderLine.unseen);
#endif
						if (m_folderLine.unseen >= 0)
							{
								PR_snprintf(unseen_a, sizeof (unseen_a), "%d", m_folderLine.unseen);
								
								return unseen_a;
							}
						else 
							{
								return "???";
							}
					}
			}
		case OUTLINER_COLUMN_TOTAL:
			{
				/* if the folder is a directory, just put dashes
				   for the total messages. */
				if (m_folderLine.level == 1
					&& m_folderLine.flags & MSG_FOLDER_FLAG_DIRECTORY)
					{
						return "---";
					}
				else
					{
						/* if we know how many unseen messages there are, 
						   we also know how many total messages there are.
						   otherwise, we put ???'s */
#if defined(DEBUG_tao)
						if (m_folderLine.total < 0)
							printf("\n**OUTLINER_COLUMN_TOTAL=%d\n", m_folderLine.total);
#endif
						if (m_folderLine.total >= 0)
							{
								PR_snprintf(total_a, sizeof (total_a), "%d", m_folderLine.total);
								
								return total_a;
							}
						else 
							{
								return "???";
							}
					}
			}
		default:
			XP_ASSERT(0);
			return NULL;
		}
}

fe_icon *
XFE_FolderView::getColumnIcon(int column)
{
  XP_Bool expanded;
  int depth;

  getTreeInfo(NULL, &expanded, &depth, NULL);

  if (column == OUTLINER_COLUMN_NAME)
    {
	  XP_Bool xxxe = False;
	  /* we need to check newshosts */
	  if (m_folderLine.flags & MSG_FOLDER_FLAG_NEWS_HOST)
		{
		  MSG_NewsHost *host = MSG_GetNewsHostForFolder(m_folderLine.id);
		  HG82992
		}

		return treeInfoToIcon(depth, m_folderLine.flags, expanded, xxxe);
    }
  else
    {
      return 0;
    }
}

char *
XFE_FolderView::getColumnName(int column)
{
  switch (column)
    {
    case OUTLINER_COLUMN_NAME:
      return "Name";
    case OUTLINER_COLUMN_TOTAL:
      return "Total";
    case OUTLINER_COLUMN_UNREAD:
      return "Unread";
    default:
      XP_ASSERT(0);
      return 0;
    }
}

char *
XFE_FolderView::getColumnHeaderText(int column)
{
  switch (column)
    {
    case OUTLINER_COLUMN_NAME:
		return XP_GetString(XFE_FOLDER_OUTLINER_COLUMN_NAME);
    case OUTLINER_COLUMN_TOTAL:
		return XP_GetString(XFE_FOLDER_OUTLINER_COLUMN_TOTAL);
    case OUTLINER_COLUMN_UNREAD:
		return XP_GetString(XFE_FOLDER_OUTLINER_COLUMN_UNREAD);
    default:
      XP_ASSERT(0);
      return 0;
    }
}

fe_icon *
XFE_FolderView::getColumnHeaderIcon(int /*column*/)
{
  return 0;
}

EOutlinerTextStyle
XFE_FolderView::getColumnHeaderStyle(int /*column*/)
{
  return OUTLINER_Default;
}

void
XFE_FolderView::releaseLineData()
{
  delete [] m_ancestorInfo;
  m_ancestorInfo = NULL;
}

#ifdef USE_3PANE
static void
click_timer_func(XtPointer closure, XtIntervalId *)
{
    XFE_FolderView* fv = (XFE_FolderView*)closure;
    DD(printf("---::: Process timer :::---\n");)
    if (fv)
        fv->processClick();
}
#endif

void 
XFE_FolderView::Buttonfunc(const OutlineButtonFuncData *data)
{
  MSG_FolderLine line;

  // Broadcast to be in focus
  getToplevel()->notifyInterested(XFE_MNListView::changeFocus, (void*) this);

  if (!MSG_GetFolderLineByIndex(m_pane, data->row, 1, &line)) return;
  

  DD(printf("###  times folder clicked %d\n", data->clicks);)
  if (data->clicks == 2) 
	  {
#ifdef USE_3PANE
		if (m_clickTimer)
			XtRemoveTimeOut(m_clickTimer);
		m_clickTimer = 0;
		DD(printf("---::: Remove timer :::---\n");)

		for (int i = 0; i < m_cur_count; i++ )
		{
		  if ( i == 0 )
		     m_outliner->selectItemExclusive(m_cur_selected[i]);
		  else
		     m_outliner->selectItem(m_cur_selected[i]);
		}
#else
		  m_outliner->selectItemExclusive(data->row);
#endif
		  
		  if (line.level == 1
			  && line.flags & MSG_FOLDER_FLAG_DIRECTORY)
			  {
				  // they double clicked on a newshost or mail server.
				  toggleExpansion(data->row);
			  }
		  else
			  {
				  // ok.  we're going to display a folder in a thread frame.  what
				  // is left to do now is determine whether we are going to reuse
				  // an existing one or pop up another.  This is governed by the
				  // preference reuse_thread_window and whether or not
				  // the alt button was held during the double click.
				  
				  fe_showMessages(XtParent(getToplevel()->getBaseWidget()),
								  /* Tao: we might need to check if this return a 
								   * non-NULL frame
								   */
								  ViewGlue_getFrame(m_contextData),
								  NULL,
								  MSG_GetFolderInfo(m_pane, data->row),
								  (( fe_globalPrefs.reuse_thread_window
									 && !data->shift)
								   || (!fe_globalPrefs.reuse_thread_window
									   && data->shift)),
								  False,
								  MSG_MESSAGEKEYNONE);
			  }
	  }
  else
	  {
		  if (data->ctrl)
			  {
				  m_outliner->toggleSelected(data->row);
			  }
		  else if (data->shift)
			  {
				  // select the range.
				  const int *selected;
				  int count;
				  
				  m_outliner->getSelection(&selected, &count);
				  
				  if (count == 0) /* there wasn't anything selected yet. */
					  {
						  m_outliner->selectItemExclusive(data->row);
					  }
				  else if (count == 1) /* there was only one, so we select the range from
										  that item to the new one. */
					  {
						  m_outliner->selectRangeByIndices(selected[0], data->row);
					  }
				  else /* we had a range of items selected, so let's do something really
						  nice with them. */
					  {
						  m_outliner->trimOrExpandSelection(data->row);
					  }
			  }
		  else
			  {

#ifdef USE_3PANE
				const int *selected;
				int count;
  				m_outliner->getSelection(&selected, &count);

				m_cur_selected= (int *)XP_CALLOC(count, sizeof(int));
				for ( int i =0; i < count; i++ )
					m_cur_selected[i] = selected[i];
				m_cur_count = count;

		  		m_outliner->selectItemExclusive(data->row);
				m_clickData = (XtPointer)data;
				if (m_clickTimer)
					XtRemoveTimeOut(m_clickTimer);
                		m_clickTimer = XtAppAddTimeOut(fe_XtAppContext, 650,
                                              click_timer_func, this);
				DD(printf("### ---::: Install timer :::--- ###\n");)
#else

				  m_outliner->selectItemExclusive(data->row);
				  
				  notifyInterested(folderSelected, MSG_GetFolderInfo(m_pane, data->row));
#endif
			  }
		  
		  getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
	  }
}


#ifdef USE_3PANE
// This is the routine that will select a folder for single click.
void
XFE_FolderView::processClick()
{
    OutlineButtonFuncData *data = (OutlineButtonFuncData *)m_clickData;

    MSG_FolderInfo *info = MSG_GetFolderInfo(m_pane, data->row);
    XFE_ThreadFrame *theFrame = XFE_ThreadFrame::frameForInfo(info);

    if ( theFrame)
    {
	// if the frame exists, then, single click perform the
	// same way as the double click mode by bringing the existing
	// frame to the foreground
#if 0
	for (int i = 0; i < m_cur_count; i++ )
		  m_outliner->selectItemExclusive(m_cur_selected[i]);
#endif
		  fe_showMessages(XtParent(getToplevel()->getBaseWidget()),
						  /* Tao: we might need to check if this return a 
						   * non-NULL frame
						   */
				  ViewGlue_getFrame(m_contextData),
				  NULL,
				  MSG_GetFolderInfo(m_pane, data->row),
				  (( fe_globalPrefs.reuse_thread_window
						 && !data->shift)
				   || (!fe_globalPrefs.reuse_thread_window
					   && data->shift)),
				  False,
				  MSG_MESSAGEKEYNONE);
    }
    else
    {
     m_outliner->selectItemExclusive(data->row);
     notifyInterested(folderSelected, info);
    }
    m_clickTimer = 0;
    XP_FREEIF(m_cur_selected);
    m_cur_selected = NULL;
    m_cur_count = 0 ;
				  
}
#endif

void
XFE_FolderView::toggleExpansion(int row)
{
	int delta;
	XP_Bool selection_needs_bubbling = False;

	delta = MSG_ExpansionDelta(m_pane, row);

	if (delta == 0)
		return;

	/* we're not selected and we're being collapsed.
	   check if any of our children are selected, and if
	   so we select this row. */
	if (!m_outliner->isSelected(row)
		&& delta < 0)
		{
			int num_children = -1 * delta;
			int i;
			
			for (i = row + 1; i <= row + num_children; i ++)
				if (m_outliner->isSelected(i))
					{
						selection_needs_bubbling = True;
					}
		}

	MSG_ToggleExpansion(m_pane, row, NULL);

	if (selection_needs_bubbling)
		m_outliner->selectItem(row);

	getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
}

void
XFE_FolderView::Flippyfunc(const OutlineFlippyFuncData *data)
{
	toggleExpansion(data->row);
}

#if !defined(USE_MOTIF_DND)

void
XFE_FolderView::dropfunc(Widget /*dropw*/, fe_dnd_Event type,
						 fe_dnd_Source *source, XEvent *event)
{
	int row = -1;
	int x, y;

	/* we only understand these targets -- well, we only understand the last 3,
	   and the outliner understands the first one. */
	if (source->type != FE_DND_COLUMN &&
		source->type != FE_DND_MAIL_MESSAGE &&
		source->type != FE_DND_NEWS_MESSAGE &&
		source->type != FE_DND_MAIL_FOLDER) 
		{
			return;
		}
	
	if (source->type == FE_DND_COLUMN)
		{
			m_outliner->handleDragEvent(event, type, source);
			
			return;
		}
	
	/* we've got to have a sourcedropfunc to drop things in a folder view,
	   since the work must really be done by the source. */
	if (source->func == NULL)
		{
			return;
		}
	
	m_outliner->translateFromRootCoords(event->xbutton.x_root, event->xbutton.y_root, &x, &y);
	
	row = m_outliner->XYToRow(x, y);
	
	if (row >= 0) 
		{
			MSG_FolderLine folder;
			if (!MSG_GetFolderLineByIndex(m_pane,
										  row, 1, &folder))
				row = -1;
		}
	
	switch (type) 
		{
		case FE_DND_START:
			DD(printf("### DRAG> button press = %d\n", ((event->xany.type == ButtonPress)? 1: 0 ) );)
			DD(printf("### DRAG> button release = %d\n", ((event->xany.type == ButtonRelease)? 1: 0 ) );)
			DD(printf("### DRAG> alt key state = %d\n", ((event->xkey.state &Mod1Mask)? 1: 0 ) );)
			/* ### In outliner, we set ignoreevent to true when the event
			   type is ButtonPress; therefore, we won't be getting
			   button press event here. We cannot perform stealth command
			   at button press time when drag take place. We can only
			   perform stealth command at button release time. Need
			   to find out why we set "ignoreevnt" to true in outliner. ### */
#if 0
			CONTEXT_DATA(m_contextData)->stealth_cmd =
                    		((event->xany.type == ButtonPress) &&
                                  (event->xkey.state & Mod1Mask));
#endif
			break;
		case FE_DND_DRAG:
			m_outliner->outlineLine(row);
			break;
		case FE_DND_DROP:
			DD(printf("### DROP> button press = %d\n", ((event->xany.type == ButtonPress)? 1: 0 ) );)
			DD(printf("### DROP> button release = %d\n", ((event->xany.type == ButtonRelease)? 1: 0 ) );)
			DD(printf("### DROP> alt key state = %d\n", ((event->xkey.state &Mod1Mask)? 1: 0 ) );)
			CONTEXT_DATA(m_contextData)->stealth_cmd =
                    		((event->xany.type == ButtonRelease) &&
                                  (event->xkey.state & Mod1Mask));
			if (row >= 0)
			{
			   MSG_FolderInfo *drop_info = MSG_GetFolderInfo(m_pane, row);
				
			   if (drop_info)
			   {
				fe_dnd_Message  msg;
				if ( source->type == FE_DND_NEWS_MESSAGE )
				{
					DD(printf("### FolderView::dropfunc:  NEWS MESSAGE_COPY ******\n");)
					msg = FE_DND_MESSAGE_COPY;
				}
				else if (( source->type == FE_DND_MAIL_MESSAGE) && 
					(CONTEXT_DATA(m_contextData)->stealth_cmd ))
				{
					DD(printf("### FolderView::dropfunc:  MAIL MESSAGE_COPY ******\n");)
					msg = FE_DND_MESSAGE_COPY;
				}
				else 
				{
					DD(printf("### FolderView::dropfunc: MAIL MESSAGE_MOVE ******\n");)
					msg = FE_DND_MESSAGE_MOVE;
				}
				(*source->func)(source, msg, (void*)drop_info);
			   }
					
			   D(printf ("Outlining line -1\n");)
					
			   m_outliner->outlineLine(-1); // to clear the drag feedback
			}
		}
}

void
XFE_FolderView::drop_func(Widget dropw, void *closure, fe_dnd_Event type,
			  fe_dnd_Source *source, XEvent* event)
{
  XFE_FolderView *obj = (XFE_FolderView*)closure;

  obj->dropfunc(dropw, type, source, event);
}

void
XFE_FolderView::sourcedropfunc(fe_dnd_Source *, fe_dnd_Message msg, void *closure)
{
  // folders can be moved or deleted.  The closure to this function will always
  // be a MSG_FolderInfo*.

  // XXX assume that nothing could have changed the selected between the time
  // we started dragging and now...  don't know how smart this is...

  MSG_FolderInfo *info = (MSG_FolderInfo*)closure;
  const int *indices;
  int count;

  DD(printf("### Enter XFE_FolderView::sourcedropfunc()\n");)
  m_outliner->getSelection(&indices, &count);

  MSG_DragEffect requireEffect = MSG_Default_Drag;

  MSG_DragEffect effect = MSG_DragFoldersIntoStatus(m_pane,
                        (MSG_ViewIndex*)indices, count,
                        info, requireEffect);



  if (msg == FE_DND_MESSAGE_MOVE )
    {
      DD(printf("### XFE_FolderView::sourcedropfunc:: FE_DND_MESSAGE_MOVE ##\n");)
      if (effect == MSG_Require_Move)
      {
  	DD(printf("## XFE_FolderView::sourcedropfunc:: Move FOLDER REQUIRED ##\n");)
        MSG_MoveFoldersInto(m_pane, (MSG_ViewIndex*)indices, count, info);
	XFE_MozillaApp::theApp()->notifyInterested(XFE_MNView::foldersHaveChanged);
      }
      else if ( effect == MSG_Drag_Not_Allowed )
      {
        DD(printf("### Drag NOT allowed : DROP SITE \n");)
        char tmp[128];
        XP_SAFE_SPRINTF(tmp, sizeof(tmp),
                        "%s",
                        XP_GetString(XFE_DND_MESSAGE_ERROR));
        XFE_Progress (m_contextData, tmp);
      }
      else 
	XP_ASSERT(0);

    }
  else if (msg == FE_DND_MESSAGE_DELETE)
    {
      // do something spiffy here, like unsubscribe if it's a newsgroup,
      // and move to the trash if it's a mail folder.
    }
}

void
XFE_FolderView::source_drop_func(fe_dnd_Source *src, fe_dnd_Message msg, void *closure)
{
  XFE_FolderView *obj = (XFE_FolderView*)src->closure;

  obj->sourcedropfunc(src, msg,closure);
}

#else

fe_icon_data *
XFE_FolderView::GetDragIconData(int row, int column)
{
  D(printf("XFE_FolderView::GetDragIconData()\n");)
  XP_Bool expanded;
  int depth;
  
  if (row == -1) return &MN_Folder;
  else
	{
	  acquireLineData(row);
	  getTreeInfo(NULL, &expanded, &depth, NULL);
	  releaseLineData();
	  return treeInfoToIconData(depth, m_folderLine.flags, expanded);
	}
}

fe_icon_data *
XFE_FolderView::getDragIconData(void *this_ptr,
								int row,
								int column)
{
  D(printf("XFE_FolderView::getDragIconData()\n");)
  XFE_FolderView *folder_view = (XFE_FolderView*)this_ptr;

  return folder_view->GetDragIconData(row, column);
}

void
XFE_FolderView::GetDragTargets(int row, int column,
							   Atom **targets,
							   int *num_targets)
{
  D(printf("XFE_FolderView::GetDragTargets()\n");)
  int depth;
  int flags;

  XP_ASSERT(row > -1);
  if (row == -1)
	{
	  *targets = NULL;
	  *num_targets = 0;
	}
  else
	{
	  if (!m_outliner->isSelected(row))
		m_outliner->selectItemExclusive(row);

	  acquireLineData(row);
	  getTreeInfo(NULL, NULL, &depth, NULL);
	  flags = m_folderLine.flags;
	  releaseLineData();
	  
	  *num_targets = 3;
	  *targets = new Atom[ *num_targets ];

	  (*targets)[1] = XFE_DragBase::_XA_NETSCAPE_URL;
	  (*targets)[2] = XA_STRING;

	  if (depth < 1)
		{
		  if (flags & MSG_FOLDER_FLAG_NEWS_HOST)
			(*targets)[0] = XFE_OutlinerDrop::_XA_NETSCAPE_NEWS_SERVER;
		  else
			(*targets)[0] = XFE_OutlinerDrop::_XA_NETSCAPE_MAIL_SERVER;
		}
	  else if (flags & MSG_FOLDER_FLAG_NEWSGROUP)
		(*targets)[0] = XFE_OutlinerDrop::_XA_NETSCAPE_NEWSGROUP;
	  else
		(*targets)[0] = XFE_OutlinerDrop::_XA_NETSCAPE_MAIL_FOLDER;
	}
}

void
XFE_FolderView::getDragTargets(void *this_ptr,
							   int row,
							   int column,
							   Atom **targets,
							   int *num_targets)
{
  D(printf("XFE_FolderView::getDragTargets()\n");)
  XFE_FolderView *folder_view = (XFE_FolderView*)this_ptr;

  folder_view->GetDragTargets(row, column, targets, num_targets);
}

void 
XFE_FolderView::getDropTargets(void */*this_ptr*/,
							   Atom **targets,
							   int *num_targets)
{
  D(printf("XFE_FolderView::getDropTargets()\n");)
  *num_targets = 7;
  *targets = new Atom[ *num_targets ];

  (*targets)[0] = XFE_OutlinerDrop::_XA_NETSCAPE_MAIL_FOLDER;
  (*targets)[1] = XFE_OutlinerDrop::_XA_NETSCAPE_MAIL_MESSAGE;
  (*targets)[2] = XFE_OutlinerDrop::_XA_NETSCAPE_MAIL_SERVER;
  (*targets)[3] = XFE_OutlinerDrop::_XA_NETSCAPE_NEWSGROUP;
  (*targets)[4] = XFE_OutlinerDrop::_XA_NETSCAPE_NEWS_MESSAGE;
  (*targets)[5] = XFE_OutlinerDrop::_XA_NETSCAPE_NEWS_SERVER;
  (*targets)[6] = XFE_OutlinerDrop::_XA_NETSCAPE_URL; /* a specialized class of urls are allowed. */
}

char *
XFE_FolderView::DragConvert(Atom atom)
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

	  urlData.createItemList(1);

	  for (i = 0; i < count; i ++)
		{
		  MSG_FolderInfo* info = MSG_GetFolderInfo(m_pane, selection[i]);
		  URL_Struct *url = MSG_ConstructUrlForFolder(m_pane,
													  info);
		  urlData.url(i,url->address);
		  
		  NET_FreeURLStruct(url);
		}

	  result = XtNewString(urlData.getString());
	  
	  return result;
	}
  else if (atom == XFE_OutlinerDrop::_XA_NETSCAPE_MAIL_FOLDER
		   || atom == XFE_OutlinerDrop::_XA_NETSCAPE_NEWSGROUP
		   || atom == XFE_OutlinerDrop::_XA_NETSCAPE_MAIL_SERVER
		   || atom == XFE_OutlinerDrop::_XA_NETSCAPE_NEWS_SERVER)
	{
	  /* for now we use a hack -- only transfer the index of the dragged
		 folder. */
	  char buf[100];
	  const int *selection;
	  int count;

	  m_outliner->getSelection(&selection, &count);

	  PR_snprintf(buf, 100, "%d",
				  selection[0]);

	  return (char*)XtNewString(buf);
	}
  else
	{
	  return (char*)XtNewString("");
	}
}

char *
XFE_FolderView::dragConvert(void *this_ptr,
							Atom atom)
{
  XFE_FolderView *folder_view = (XFE_FolderView*)this_ptr;
  
  return folder_view->DragConvert(atom);
}

int
XFE_FolderView::ProcessTargets(int row, int col,
							   Atom *targets,
							   const char **data,
							   int numItems)
{
  int i;

  D(printf("XFE_FolderView::ProcessTargets()\n");)

  for (i = 0; i<numItems; i ++)
	{
	  if (targets[i]==None || data[i]==NULL || strlen(data[i])==0)
		continue;

	  D(("  [%d] %s: \"%s\"\n",i,XmGetAtomName(XtDisplay(m_widget),targets[i]),data[i]));

	  if (targets[i] == XFE_OutlinerDrop::_XA_NETSCAPE_MAIL_FOLDER
		  || targets[i] == XFE_OutlinerDrop::_XA_NETSCAPE_NEWSGROUP)
		{
		  MSG_ViewIndex index = (MSG_ViewIndex)atoi(data[i]);
		  MSG_FolderInfo *info = MSG_GetFolderInfo(m_pane, row);

		  if (info)
			{
			  MSG_MoveFoldersInto(m_pane, &index, 1, info);
			  return TRUE;
			}
		  else
			return FALSE;
		}
	  else if (targets[i] == XFE_OutlinerDrop::_XA_NETSCAPE_MAIL_MESSAGE
			   || targets[i] == XFE_OutlinerDrop::_XA_NETSCAPE_NEWS_MESSAGE)
		{
		  return TRUE;
		}
	  else if (targets[i] == XFE_OutlinerDrop::_XA_NETSCAPE_URL)
		{
		  MSG_FolderInfo *dest_info = MSG_GetFolderInfo(m_pane, row);
		  XFE_URLDesktopType urlData(data[i]);
		  MSG_ViewIndex *indices;
		  int num_indices = 0;

		  if (dest_info == NULL) return FALSE;

		  indices = new MSG_ViewIndex[ urlData.numItems() ];
		  for (i = 0; i < urlData.numItems(); i ++)
			{
			  const char *url = urlData.url(i);

			  if (MSG_PaneTypeForURL(url) != MSG_FOLDERPANE)
				continue;
			  else
				{
				  MSG_FolderInfo *info = MSG_GetFolderInfoFromURL(m_master,
																  url,FALSE);
				  MSG_ViewIndex new_index;

				  if (!info)
					continue;

				  new_index = MSG_GetFolderIndexForInfo(m_pane,
														info, TRUE);

				  if (new_index != MSG_VIEWINDEXNONE)
					indices[num_indices ++] = new_index;
				}
			}

		  if (num_indices > 0)
			MSG_MoveFoldersInto(m_pane, indices, num_indices, dest_info);

		  
		  delete [] indices;
		  return num_indices > 0;
		}
	}

  return FALSE;
}

int
XFE_FolderView::processTargets(void *this_ptr,
							   int row, int col,
							   Atom *targets,
							   const char **data,
							   int numItems)
{
  XFE_FolderView *folder_view = (XFE_FolderView*)this_ptr;
  
  return folder_view->ProcessTargets(row, col, targets, data, numItems);
}
#endif /* USE_MOTIF_DND */
