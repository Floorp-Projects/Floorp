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
   BookmarkView.h -- view of user's bookmark's.
   Created: Chris Toshok <toshok@netscape.com>, 9-Aug-96.
 */



#include "MozillaApp.h"
#include "BookmarkView.h"

#if defined(USE_MOTIF_DND)

#include "OutlinerDrop.h"

#endif /* USE_MOTIF_DND */

#include "BrowserFrame.h"  // for fe_reuseBrowser
#include "BookmarkFindDialog.h"
#include "BookmarkWhatsNewDialog.h"
#include "ViewGlue.h"
#include "xfe.h"
#include "xfe2_extern.h"
#include "bkmks.h"
#include "felocale.h" /* for fe_ConvertToLocalEncoding */
#include "xp_mem.h"
#include "prefapi.h"

#include "PersonalToolbar.h"

#include <Xfe/Xfe.h>

#include "xpgetstr.h"
extern int XFE_BM_OUTLINER_COLUMN_NAME;
extern int XFE_BM_OUTLINER_COLUMN_LOCATION;
extern int XFE_BM_OUTLINER_COLUMN_LASTVISITED;
extern int XFE_BM_OUTLINER_COLUMN_CREATEDON;

#define BOOKMARK_OUTLINER_GEOMETRY_PREF "bookmarks.outliner_geometry"
#define READ_BUFFER_SIZE 2048

#if defined(DEBUG_toshok)||defined(DEBUG_tao)
#define D(x) printf x
#else
#define D(x)
#endif

const int XFE_BookmarkView::OUTLINER_COLUMN_NAME = 0;
const int XFE_BookmarkView::OUTLINER_COLUMN_LOCATION = 1;
const int XFE_BookmarkView::OUTLINER_COLUMN_LASTVISITED = 2;
const int XFE_BookmarkView::OUTLINER_COLUMN_CREATEDON = 3;

/* pixmaps for use in the bookmark window. */
fe_icon XFE_BookmarkView::bookmark = { 0 };
fe_icon XFE_BookmarkView::mailBookmark = { 0 };
fe_icon XFE_BookmarkView::newsBookmark = { 0 };
fe_icon XFE_BookmarkView::changedBookmark = { 0 };
fe_icon XFE_BookmarkView::closedFolder = { 0 };
fe_icon XFE_BookmarkView::openedFolder = { 0 };
fe_icon XFE_BookmarkView::closedPersonalFolder = { 0 };
fe_icon XFE_BookmarkView::openedPersonalFolder = { 0 };
fe_icon XFE_BookmarkView::closedFolderDest = { 0 };
fe_icon XFE_BookmarkView::openedFolderDest = { 0 };
fe_icon XFE_BookmarkView::closedFolderMenu = { 0 };
fe_icon XFE_BookmarkView::openedFolderMenu = { 0 };
fe_icon XFE_BookmarkView::closedFolderMenuDest = { 0 };
fe_icon XFE_BookmarkView::openedFolderMenuDest = { 0 };

MenuSpec XFE_BookmarkView::open_popup_spec[] = {
  { xfeCmdOpenLinkNew, PUSHBUTTON },
#ifdef EDITOR 
  { xfeCmdOpenLinkEdit, PUSHBUTTON },
#endif
  MENU_SEPARATOR,
  { NULL },
};
MenuSpec XFE_BookmarkView::new_popup_spec[] = {
  { xfeCmdNewBookmark, PUSHBUTTON },
  { xfeCmdNewFolder, PUSHBUTTON },
  { xfeCmdNewSeparator, PUSHBUTTON },
  MENU_SEPARATOR,
  { NULL },
};
MenuSpec XFE_BookmarkView::set_popup_spec[] = {
  { xfeCmdSetToolbarFolder, PUSHBUTTON },
  { xfeCmdSetNewBookmarkFolder, PUSHBUTTON },
  { xfeCmdSetBookmarkMenuFolder, PUSHBUTTON },
  MENU_SEPARATOR,
  { NULL },
};
MenuSpec XFE_BookmarkView::saveas_popup_spec[] = {
  { xfeCmdSaveLink, PUSHBUTTON },
  MENU_SEPARATOR,
  { NULL },
};
MenuSpec XFE_BookmarkView::cutcopy_popup_spec[] = {
  { xfeCmdCut, PUSHBUTTON },
  { xfeCmdCopy, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_BookmarkView::copylink_popup_spec[] = {
  { xfeCmdCopyLink, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_BookmarkView::paste_popup_spec[] = {
  { xfeCmdPaste, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_BookmarkView::delete_popup_spec[] = {
  { xfeCmdDelete, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_BookmarkView::makealias_popup_spec[] = {
  { xfeCmdMakeAlias, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_BookmarkView::properties_popup_spec[] = {
  MENU_SEPARATOR,
  { xfeCmdBookmarkProperties, PUSHBUTTON },
  { NULL },
};

XFE_BookmarkView::XFE_BookmarkView(XFE_Component *toplevel_component, 
				   Widget parent,
				   XFE_View *parent_view, MWContext *context) 
  : XFE_View(toplevel_component, parent_view, context)
{
  int num_columns = 4;
  static int column_widths[] = {40, 40, 15, 30};

  m_propertiesDialog = NULL;
  m_whatsNewDialog = NULL;
  m_popup = NULL;
  m_batchDepth = 0;
  m_menuIsInvalid = FALSE;

  m_outliner = new XFE_Outliner("bookmarkList",
								this, 
								getToplevel(),
								parent,
								False, // constantSize 
								True,  // hasHeadings
								num_columns,  // Number of columns.
								num_columns,  // Number of visible columns.
								column_widths, 
								BOOKMARK_OUTLINER_GEOMETRY_PREF);

  m_outliner->setHideColumnsAllowed( TRUE );
  m_outliner->setPipeColumn( OUTLINER_COLUMN_NAME );
  m_outliner->setMultiSelectAllowed(True);

  m_sortDescending = True;
  m_lastSort = xfeCmdSortByTitle;

  m_outliner->setSortColumn( OUTLINER_COLUMN_NAME, 
                             OUTLINER_SortAscending );

  setBaseWidget(m_outliner->getBaseWidget());

  // Initialize clip stuff.
  clip.block = NULL;
  clip.length = 0;

  {
    Pixel bg_pixel;
    
    XtVaGetValues(m_widget, XmNbackground, &bg_pixel, 0);

    if (!bookmark.pixmap)
      fe_NewMakeIcon(getToplevel()->getBaseWidget(),
		     BlackPixelOfScreen(XtScreen(m_widget)),
		     bg_pixel,
		     &bookmark,
		     NULL,
		     BM_Bookmark.width, BM_Bookmark.height,
		     BM_Bookmark.mono_bits, BM_Bookmark.color_bits, BM_Bookmark.mask_bits, FALSE);

	if (!mailBookmark.pixmap)
      fe_NewMakeIcon(getToplevel()->getBaseWidget(),
					 BlackPixelOfScreen(XtScreen(m_widget)),
					 bg_pixel,
					 &mailBookmark,
					 NULL,
					 BM_MailBookmark.width, BM_MailBookmark.height,
					 BM_MailBookmark.mono_bits, BM_MailBookmark.color_bits,
					 BM_MailBookmark.mask_bits, FALSE);

	if (!newsBookmark.pixmap)
      fe_NewMakeIcon(getToplevel()->getBaseWidget(),
					 BlackPixelOfScreen(XtScreen(m_widget)),
					 bg_pixel,
					 &newsBookmark,
					 NULL,
					 BM_NewsBookmark.width, BM_NewsBookmark.height,
					 BM_NewsBookmark.mono_bits, BM_NewsBookmark.color_bits,
					 BM_NewsBookmark.mask_bits, FALSE);

	if (!changedBookmark.pixmap)
      fe_NewMakeIcon(getToplevel()->getBaseWidget(),
					 BlackPixelOfScreen(XtScreen(m_widget)),
					 bg_pixel,
					 &changedBookmark,
					 NULL,
					 BM_Change.width, BM_Change.height,
					 BM_Change.mono_bits, BM_Change.color_bits,
					 BM_Change.mask_bits, FALSE);

    if (!closedFolder.pixmap)
      fe_NewMakeIcon(getToplevel()->getBaseWidget(),
		     BlackPixelOfScreen(XtScreen(m_widget)),
		     bg_pixel,
		     &closedFolder,
		     NULL,
		     BM_Folder.width, BM_Folder.height,
		     BM_Folder.mono_bits, BM_Folder.color_bits, BM_Folder.mask_bits, FALSE);

    if (!openedFolder.pixmap)
      fe_NewMakeIcon(getToplevel()->getBaseWidget(),
		     BlackPixelOfScreen(XtScreen(m_widget)),
		     bg_pixel,
		     &openedFolder,
		     NULL,
		     BM_FolderO.width, BM_FolderO.height,
		     BM_FolderO.mono_bits, BM_FolderO.color_bits, BM_FolderO.mask_bits, FALSE);

    if (!closedPersonalFolder.pixmap)
      fe_NewMakeIcon(getToplevel()->getBaseWidget(),
		     BlackPixelOfScreen(XtScreen(m_widget)),
		     bg_pixel,
		     &closedPersonalFolder,
		     NULL,
		     BM_PersonalFolder.width, BM_PersonalFolder.height,
		     BM_PersonalFolder.mono_bits, BM_PersonalFolder.color_bits, BM_PersonalFolder.mask_bits, FALSE);

    if (!openedPersonalFolder.pixmap)
      fe_NewMakeIcon(getToplevel()->getBaseWidget(),
		     BlackPixelOfScreen(XtScreen(m_widget)),
		     bg_pixel,
		     &openedPersonalFolder,
		     NULL,
		     BM_PersonalFolderO.width, BM_PersonalFolderO.height,
		     BM_PersonalFolderO.mono_bits, BM_PersonalFolderO.color_bits, BM_PersonalFolderO.mask_bits, FALSE);


    if (!openedFolderDest.pixmap)
      fe_NewMakeIcon(getToplevel()->getBaseWidget(),
		     BlackPixelOfScreen(XtScreen(m_widget)),
		     bg_pixel,
		     &openedFolderDest,
		     NULL,
		     BM_NewFolderO.width, BM_NewFolderO.height,
		     BM_NewFolderO.mono_bits, BM_NewFolderO.color_bits, BM_NewFolderO.mask_bits, FALSE);

    if (!closedFolderDest.pixmap)
      fe_NewMakeIcon(getToplevel()->getBaseWidget(),
		     BlackPixelOfScreen(XtScreen(m_widget)),
		     bg_pixel,
		     &closedFolderDest,
		     NULL,
		     BM_NewFolder.width, BM_NewFolder.height,
		     BM_NewFolder.mono_bits, BM_NewFolder.color_bits, BM_NewFolder.mask_bits, FALSE);

    if (!openedFolderMenu.pixmap)
      fe_NewMakeIcon(getToplevel()->getBaseWidget(),
		     BlackPixelOfScreen(XtScreen(m_widget)),
		     bg_pixel,
		     &openedFolderMenu,
		     NULL,
		     BM_MenuFolderO.width, BM_MenuFolderO.height,
		     BM_MenuFolderO.mono_bits, BM_MenuFolderO.color_bits, BM_MenuFolderO.mask_bits, FALSE);

    if (!closedFolderMenu.pixmap)
      fe_NewMakeIcon(getToplevel()->getBaseWidget(),
		     BlackPixelOfScreen(XtScreen(m_widget)),
		     bg_pixel,
		     &closedFolderMenu,
		     NULL,
		     BM_MenuFolder.width, BM_MenuFolder.height,
		     BM_MenuFolder.mono_bits, BM_MenuFolder.color_bits, BM_MenuFolder.mask_bits, FALSE);

    if (!openedFolderMenuDest.pixmap)
      fe_NewMakeIcon(getToplevel()->getBaseWidget(),
		     BlackPixelOfScreen(XtScreen(m_widget)),
		     bg_pixel,
		     &openedFolderMenuDest,
		     NULL,
		     BM_NewAndMenuFolderO.width, BM_NewAndMenuFolderO.height,
		     BM_NewAndMenuFolderO.mono_bits, BM_NewAndMenuFolderO.color_bits, BM_NewAndMenuFolderO.mask_bits, FALSE);

    if (!closedFolderMenuDest.pixmap)
      fe_NewMakeIcon(getToplevel()->getBaseWidget(),
		     BlackPixelOfScreen(XtScreen(m_widget)),
		     bg_pixel,
		     &closedFolderMenuDest,
		     NULL,
		     BM_NewAndMenuFolder.width, BM_NewAndMenuFolder.height,
		     BM_NewAndMenuFolder.mono_bits, BM_NewAndMenuFolder.color_bits, BM_NewAndMenuFolder.mask_bits, FALSE);
  }


#if defined(USE_MOTIF_DND)

  m_outliner->enableDragDrop(this,
							 XFE_BookmarkView::getDropTargets,
							 XFE_BookmarkView::getDragTargets,
							 XFE_BookmarkView::getDragIconData,
							 XFE_BookmarkView::dragConvert,
							 XFE_BookmarkView::processTargets);
#else
  m_outliner->setDragType(FE_DND_BOOKMARK, &bookmark,
						  this);
  fe_dnd_CreateDrop(m_outliner->getBaseWidget(), drop_func, this);
#endif  /* USE_MOTIF_DND */

  // Save vertical scroller for keyboard accelerators
  CONTEXT_DATA(context)->vscroll = m_outliner->getScroller();

  // jump start the bookmarkview's display
  BM_InitializeBookmarksContext(context);

  BM_SetFEData(context, this);

  loadBookmarks(NULL);

  m_ancestorInfo = NULL;

  // Select the first row
  if (m_outliner->getTotalLines() > 0)
    {
      BM_SelectItem(m_contextData, 
                    BM_AtIndex(m_contextData, 1),
                    TRUE, FALSE, TRUE);
    }

  // Set visible rows here.  Should check to see if pref geometry is in effect.
  // m_outliner->showAllRowsWithRange(10, 40);
}

XFE_BookmarkView::~XFE_BookmarkView()
{
  if (m_propertiesDialog != NULL)
    {
      delete m_propertiesDialog;
    }
  if (m_whatsNewDialog != NULL)
    {
      delete m_whatsNewDialog;
    }
}

Boolean
XFE_BookmarkView::isCommandEnabled(CommandType cmd, void *calldata,
                                   XFE_CommandInfo* info)
{
#define IS_CMD(command) cmd == (command)
  const int *selected;
  int count;
  
  BM_CommandType bm_cmd = commandToBMCmd(cmd);

  m_outliner->getSelection(&selected, &count);
  
  if (IS_CMD(xfeCmdOpenSelected))
    {
      return count == 1 && BM_FindCommandStatus(m_contextData, bm_cmd);
    } 
  else if (IS_CMD(xfeCmdBookmarkProperties))
    {
      return count == 1 && m_propertiesDialog == 0 && 
        BM_FindCommandStatus(m_contextData, bm_cmd);
    }
  else if (IS_CMD(xfeCmdAddToToolbar))
    {
	  return count == 1;
    } 
  else if (IS_CMD(xfeCmdMoveBookmarkUp))
    {
	  return count == 1 && selected[0] > 1;
    } 
  else if (IS_CMD(xfeCmdMoveBookmarkDown))
    {
	  return count == 1 && selected[0] != 0
        && selected[0] < m_outliner->getTotalLines() - 1;
    } 
  else if (IS_CMD(xfeCmdBookmarksWhatsNew))
    {
      return m_whatsNewDialog == 0;
    }
  else if (IS_CMD(xfeCmdSortAscending) 
           || IS_CMD(xfeCmdSortDescending))
    {
      return m_lastSort != xfeCmdSortByTitle;
    }
  else if (IS_CMD(xfeCmdSortByTitle)
           || IS_CMD(xfeCmdSortByLocation)
           || IS_CMD(xfeCmdSortByDateLastVisited)
           || IS_CMD(xfeCmdSortByDateCreated)
           )
    {
      return TRUE;
    }
  else if (IS_CMD(xfeCmdOpenLinkNew)
#ifdef EDITOR
           || IS_CMD(xfeCmdOpenLinkEdit)
#endif
           || IS_CMD(xfeCmdSaveLink)
           || IS_CMD(xfeCmdCopyLink)
           )
    {
      return count == 1;
    }
  else if (bm_cmd != BM_Cmd_Invalid)
    {
      return BM_FindCommandStatus(m_contextData, bm_cmd);
    }
  else
    {
      return XFE_View::isCommandEnabled(cmd, calldata, info);
    }
#undef IS_CMD
}

BM_CommandType
XFE_BookmarkView::commandToBMCmd(CommandType cmd)
{
	// mcafee: what is this?   (BM_CommandType)~0;
	BM_CommandType bm_cmd = BM_Cmd_Invalid;  
	
#define BEGIN_BM_MAP() if (0) ;
#define BM_CMDMAP(the_cmd, the_bm_cmd) else if (cmd == (the_cmd)) bm_cmd = (the_bm_cmd);
	
	// Map bookmark commands onto xfeCmd's.
	// By default, these commands will just be shoved into
	// BM_ObeyCommand() in XFE_BookmarkView::doCommand().
	// Commands that need special attention will be intercepted
	// and called directly.
	BEGIN_BM_MAP()
	
	BM_CMDMAP(xfeCmdNewBookmark, BM_Cmd_InsertBookmark) // New Bookmark...
	BM_CMDMAP(xfeCmdNewFolder,          BM_Cmd_InsertHeader)   // New Folder...
	BM_CMDMAP(xfeCmdNewSeparator,       BM_Cmd_InsertSeparator)// New Separator
	BM_CMDMAP(xfeCmdOpenSelected,       BM_Cmd_GotoBookmark)   // Open Selected
	BM_CMDMAP(xfeCmdOpenBookmarkFile,   BM_Cmd_Open)           // Open Bookmark File...
	BM_CMDMAP(xfeCmdImport, BM_Cmd_ImportBookmarks) // Import...
	BM_CMDMAP(xfeCmdSaveAs, BM_Cmd_SaveAs)          // Save As...
	BM_CMDMAP(xfeCmdMakeAlias, BM_Cmd_MakeAlias)       // Make Alias
	BM_CMDMAP(xfeCmdClose,  BM_Cmd_Close)           // Close

	BM_CMDMAP(xfeCmdUndo,  BM_Cmd_Undo)
	BM_CMDMAP(xfeCmdRedo,  BM_Cmd_Redo)
	BM_CMDMAP(xfeCmdCut,   BM_Cmd_Cut)
	BM_CMDMAP(xfeCmdCopy,  BM_Cmd_Copy)
	BM_CMDMAP(xfeCmdPaste, BM_Cmd_Paste)
	BM_CMDMAP(xfeCmdDelete,             BM_Cmd_Delete)
	BM_CMDMAP(xfeCmdSelectAll,          BM_Cmd_SelectAllBookmarks)
	BM_CMDMAP(xfeCmdFindInObject,       BM_Cmd_Find)
	BM_CMDMAP(xfeCmdFindAgain,          BM_Cmd_FindAgain)
	BM_CMDMAP(xfeCmdBookmarkProperties, BM_Cmd_BookmarkProps)
	
    BM_CMDMAP(xfeCmdSortByTitle,             BM_Cmd_Sort_Natural)
    BM_CMDMAP(xfeCmdSortByLocation,         (m_sortDescending
                                             ? BM_Cmd_Sort_Address
                                             : BM_Cmd_Sort_Address_Asc))
    BM_CMDMAP(xfeCmdSortByDateLastVisited,  (m_sortDescending
                                             ? BM_Cmd_Sort_LastVisit
                                             : BM_Cmd_Sort_LastVisit_Asc))
    BM_CMDMAP(xfeCmdSortByDateCreated,      (m_sortDescending
                                             ? BM_Cmd_Sort_AddDate
                                             : BM_Cmd_Sort_AddDate_Asc))
  
	BM_CMDMAP(xfeCmdSortBookmarks,      BM_Cmd_SortBookmarks) // Sort Bookmarks
	BM_CMDMAP(xfeCmdSetNewBookmarkFolder, BM_Cmd_SetAddHeader)
	BM_CMDMAP(xfeCmdSetBookmarkMenuFolder,  BM_Cmd_SetMenuHeader)
	BM_CMDMAP(xfeCmdSetToolbarFolder,  BM_Cmd_SetMenuHeader)

	return bm_cmd;
	
#undef BEGIN_BM_MAP
#undef BM_CMDMAP
}

void
XFE_BookmarkView::doCommand(CommandType cmd, void *calldata, XFE_CommandInfo*info)
{
  const int *	selected;
  int			count;
  
  m_outliner->getSelection(&selected, &count);
		
  if (cmd == xfeCmdSortAscending 
      || cmd == xfeCmdSortDescending)
    {
      m_sortDescending = !m_sortDescending;
      
      cmd = m_lastSort;
    }
  
  BM_CommandType bm_cmd = commandToBMCmd(cmd);
    
  if (cmd == xfeCmdOpenSelected)
    {
      if (count == 1)
        {
          BM_Entry* entry = BM_AtIndex(m_contextData, selected[0] + 1);
          
          if (entry) {
            BM_GotoBookmark(m_contextData, entry);
          }
        }
    }
  else if (cmd == xfeCmdSetToolbarFolder)
	{
      if (count == 1)
		{
          BM_Entry * entry = BM_AtIndex(m_contextData,selected[0] + 1);
          
          if (entry && BM_IsHeader(entry))
			{
              XFE_PersonalToolbar::setToolbarFolder(entry,True);
              refreshCells();
			}
		}
	}
  else if (cmd == xfeCmdMoveBookmarkUp)
    {
      if (count == 1 && selected[0] > 0)
		{
          startBatch();
          BM_DoDrop(m_contextData, selected[0] - 1, True);
          endBatch();

          m_outliner->makeVisible(selected[0]);
        }
    }
  else if (cmd == xfeCmdMoveBookmarkDown)
    {
      if (count == 1 && selected[0] != 0
          && selected[0] < m_outliner->getTotalLines() - 1)
		{
          BM_Entry *entry = BM_AtIndex(m_contextData, selected[0] + 1);


          startBatch();

          // Close a folder when moving it down in the list
          if (entry && BM_IsHeader(entry) && !BM_IsFolded(entry))
            {
              BM_FoldHeader(m_contextData, entry, TRUE, FALSE, FALSE);
            }

          BM_DoDrop(m_contextData, selected[0] + 2, True);
          endBatch();

          m_outliner->makeVisible(selected[0]);
        }
    }
  else if (cmd == xfeCmdBookmarksWhatsNew)
    {
      startWhatsChanged();
    }
  else if (cmd == xfeCmdAddToToolbar)
    {
      if (count == 1 && selected[0] >= 0
          && selected[0] < m_outliner->getTotalLines() - 1)
        {
          BM_Entry * personalTB =
            XFE_PersonalToolbar::getToolbarFolder();
          BM_Entry *entry = 
            BM_AtIndex(m_contextData, selected[0] + 1);
          
          if (personalTB && entry)
            {
              BM_Entry* new_entry = 
                BM_CopyBookmark(m_contextData, entry);
              
              BM_AppendToHeader(m_contextData, personalTB, new_entry);
            }
        }
    }
  else if (cmd == xfeCmdShowPopup)
    {
      doPopup(info->event);
    }
  else if (cmd == xfeCmdOpenLinkNew)
    {
      if (count == 1)
        {
          BM_Entry* entry = BM_AtIndex(m_contextData, selected[0] + 1);
          char *url = BM_GetAddress(entry);

          if (url) 
            {
              fe_showBrowser(XfeAncestorFindApplicationShell(CONTEXT_WIDGET (m_contextData)),
                             NULL, NULL, 
                             NET_CreateURLStruct (url, NET_DONT_RELOAD));
            }
        }
    }
#ifdef EDITOR
  else if (cmd == xfeCmdOpenLinkEdit)
    {
      if (count == 1)
        {
          BM_Entry* entry = BM_AtIndex(m_contextData, selected[0] + 1);
          char *url = BM_GetAddress(entry);

          if (url) 
            {
#ifdef MOZ_MAIL_NEWS
              if (MSG_RequiresComposeWindow(url))
                fe_reuseBrowser(m_contextData,
                                NET_CreateURLStruct(url, NET_DONT_RELOAD));
              else
#endif
                fe_EditorNew(m_contextData, (XFE_Frame *)m_toplevel,
                             NULL, url);
            }
        }
    }
#endif  // EDITOR
  else if (cmd == xfeCmdSaveLink)
    {
      if (count == 1)
        {
          BM_Entry* entry = BM_AtIndex(m_contextData, selected[0] + 1);
          char *url = BM_GetAddress(entry);

          if (url) 
            {
              fe_SaveURL(m_contextData,
                         NET_CreateURLStruct (url, NET_DONT_RELOAD));
            }
        }
    }
  else if (cmd == xfeCmdCopyLink)
    {
      if (count == 1)
        {
          BM_Entry* entry = BM_AtIndex(m_contextData, selected[0] + 1);
          char *url = BM_GetAddress(entry);

          if (url) 
            {
              fe_clipboard_link_cb (CONTEXT_WIDGET (m_contextData),
                                    (XtPointer) m_contextData, NULL,
                                    NET_CreateURLStruct (url, NET_DONT_RELOAD));
            }
        }
    }
  else if (cmd == xfeCmdSortByTitle
           || cmd == xfeCmdSortByLocation
           || cmd == xfeCmdSortByDateLastVisited
           || cmd == xfeCmdSortByDateCreated
           )
    {
      EOutlinerSortDirection sortDir = (m_sortDescending ?
                                        OUTLINER_SortAscending :
                                        OUTLINER_SortDescending );

      if (cmd == xfeCmdSortByTitle)
        {
          if (m_lastSort == xfeCmdSortByTitle) {
            XBell (XtDisplay(m_widget), 0);
            return;
          }
          m_outliner->setSortColumn(OUTLINER_COLUMN_NAME, sortDir);
        }
      else if (cmd == xfeCmdSortByLocation)
        m_outliner->setSortColumn(OUTLINER_COLUMN_LOCATION, sortDir);
      else if (cmd == xfeCmdSortByDateLastVisited)
        m_outliner->setSortColumn(OUTLINER_COLUMN_LASTVISITED, sortDir);
      else if (cmd == xfeCmdSortByDateCreated)
        m_outliner->setSortColumn(OUTLINER_COLUMN_CREATEDON, sortDir);
      
      m_lastSort = cmd;
          
      BM_SelectAll( m_contextData, FALSE );

      startBatch();
      BM_ObeyCommand(m_contextData, bm_cmd);
      endBatch();

      BM_ClearAllSelection( m_contextData, TRUE );

      // Select first item
      BM_SelectItem(m_contextData, 
                    BM_AtIndex(m_contextData, 1),
                    TRUE, FALSE, TRUE);
    }
  else if (bm_cmd != BM_Cmd_Invalid)
    {
      startBatch();
      BM_ObeyCommand(m_contextData, bm_cmd);
      endBatch();
    }
  else
    {
      XFE_View::doCommand(cmd,calldata,info);
    }
}

Boolean
XFE_BookmarkView::handlesCommand(CommandType cmd, void *calldata,
                                 XFE_CommandInfo* info)
{
#define IS_CMD(command) cmd == (command)

  if (IS_CMD(xfeCmdOpenSelected)
      || IS_CMD(xfeCmdNewBookmark)
      || IS_CMD(xfeCmdNewFolder)
      || IS_CMD(xfeCmdNewSeparator)
      || IS_CMD(xfeCmdOpenSelected)
      || IS_CMD(xfeCmdOpenBookmarkFile)
      || IS_CMD(xfeCmdImport)
      || IS_CMD(xfeCmdSaveAs)
      || IS_CMD(xfeCmdAddToToolbar)
	  || IS_CMD(xfeCmdMakeAlias)
	  || IS_CMD(xfeCmdClose)

      || IS_CMD(xfeCmdUndo)
      || IS_CMD(xfeCmdRedo)
      || IS_CMD(xfeCmdCut)
      || IS_CMD(xfeCmdCopy)
      || IS_CMD(xfeCmdPaste)
      || IS_CMD(xfeCmdDelete)
      || IS_CMD(xfeCmdSelectAll)
      || IS_CMD(xfeCmdFindInObject)
      || IS_CMD(xfeCmdFindAgain)
      || IS_CMD(xfeCmdBookmarkProperties)

	  || IS_CMD(xfeCmdMoveBookmarkUp)
	  || IS_CMD(xfeCmdMoveBookmarkDown)
	  || IS_CMD(xfeCmdSetNewBookmarkFolder)
	  || IS_CMD(xfeCmdSetBookmarkMenuFolder)
	  || IS_CMD(xfeCmdSetToolbarFolder)

      || IS_CMD(xfeCmdSortBookmarks)
      || IS_CMD(xfeCmdSortByTitle)
      || IS_CMD(xfeCmdSortByLocation)
      || IS_CMD(xfeCmdSortByDateLastVisited)
      || IS_CMD(xfeCmdSortByDateCreated)
      || IS_CMD(xfeCmdSortAscending)
      || IS_CMD(xfeCmdSortDescending)

      || IS_CMD(xfeCmdBookmarksWhatsNew)

      // Popup commands
      || IS_CMD(xfeCmdShowPopup)
      || IS_CMD(xfeCmdOpenLinkNew)
#ifdef EDITOR
      || IS_CMD(xfeCmdOpenLinkEdit)
#endif
      || IS_CMD(xfeCmdSaveLink)
      || IS_CMD(xfeCmdCopyLink)
	)
    {
      return True;
    }
  else
    {
      return XFE_View::handlesCommand(cmd, calldata, info);
    }
#undef IS_CMD
}

XP_Bool
XFE_BookmarkView::isCommandSelected(CommandType cmd,
                                    void *calldata, XFE_CommandInfo* info)
{
#define IS_CMD(command) cmd == (command)

  // This method is designed for toggle button.
  // We want to keep the toggle button to have the same state
  //   as its matched view.

  if (IS_CMD(xfeCmdSortByTitle)
      || IS_CMD(xfeCmdSortByLocation)
      || IS_CMD(xfeCmdSortByDateLastVisited)
      || IS_CMD(xfeCmdSortByDateCreated)
      )
    {
      return m_lastSort == cmd;
    }
  else if (IS_CMD(xfeCmdSortAscending))
    {
      return !m_sortDescending;
    }
  else if (IS_CMD(xfeCmdSortDescending))
    {
      return m_sortDescending;
    }
  else
    {
      XFE_View *view = getParent();
      return (view && view->isCommandSelected(cmd, calldata, info));
    }

#undef IS_CMD
}

char *
XFE_BookmarkView::commandToString(CommandType, void*, XFE_CommandInfo*)
{
    return NULL;
}


Boolean
XFE_BookmarkView::loadBookmarks(char *filename)
{
  char url[512];
  XP_StatStruct st;
  int result;

  if (!filename)
    filename = fe_globalPrefs.bookmark_file;

  result = XP_Stat( filename, &st, xpBookmarks);

  if (result == -1 && errno == ENOENT 
      && filename && *filename) {
    // Since the bookmarks file does not exist,
    //   try to copy the default one.
    char *defaultBM = fe_getDefaultBookmarks();
    
    if (defaultBM) {
      copyBookmarksFile(filename, defaultBM);
      XP_FREE(defaultBM);
    }
  }
  PR_snprintf(url, sizeof(url), "file:%s", filename);
  
  BM_ReadBookmarksFromDisk(m_contextData, filename, url);

  return True;
}

Boolean
XFE_BookmarkView::copyBookmarksFile(char *dst, char *src)
{
  XP_File src_fp, dst_fp;
  char buffer[READ_BUFFER_SIZE];

  src_fp = XP_FileOpen(src, xpBookmarks, XP_FILE_READ);
  if (!src_fp) return False;

  dst_fp = XP_FileOpen(dst, xpBookmarks, XP_FILE_WRITE);
  if (!dst_fp) return False;

  Boolean writeOk = True;
  int length = 0;
  while (XP_FileReadLine(buffer, READ_BUFFER_SIZE, src_fp) && writeOk) {
    length = XP_STRLEN(buffer);
    if (XP_FileWrite(buffer, length, dst_fp) < length) {
      writeOk = False;
    }
  }
  XP_FileClose(src_fp);
  XP_FileClose(dst_fp);

  return writeOk;
}

// Methods from the outlinable interface.
void *
XFE_BookmarkView::ConvFromIndex(int /*index*/)
{
  XP_ASSERT(0);
  return 0;
}

int
XFE_BookmarkView::ConvToIndex(void */*item*/)
{
  XP_ASSERT(0);
  return 0;
}


void *
XFE_BookmarkView::acquireLineData(int line)
{
  BM_Entry *tmp;

  m_entryDepth = 0;

  m_entry = BM_AtIndex(m_contextData, line + 1);

  if (!m_entry)
    return NULL;

  tmp = m_entry;
  
  while ( (tmp = BM_GetParent(tmp) ) != NULL )
	  m_entryDepth ++;

  m_ancestorInfo = new OutlinerAncestorInfo[ m_entryDepth + 1];
  
  if (m_entryDepth)
	  {
		  BM_Entry *tmp;
		  int i;
		  
		  for (tmp = m_entry, i = m_entryDepth;
			   tmp != NULL;
			   tmp = BM_GetParent(tmp), i --)
			  {
				  m_ancestorInfo[i].has_next = BM_HasNext(tmp);
				  m_ancestorInfo[i].has_prev = BM_HasPrev(tmp);
			  }
	  }
  else
	  {
		  m_ancestorInfo[0].has_next = BM_HasNext (m_entry);
		  m_ancestorInfo[0].has_prev = BM_HasPrev (m_entry);
	  }
  
  return m_entry;
}

void
XFE_BookmarkView::getTreeInfo(XP_Bool *expandable,
			      XP_Bool *is_expanded,
			      int *depth,
			      OutlinerAncestorInfo **ancestor)
{
  XP_Bool is_entry_expandable = False;
  XP_Bool is_entry_expanded = False;

  XP_ASSERT(m_entry);

  if (!m_entry)
  {
	  return;
  }

  is_entry_expandable = BM_IsHeader(m_entry) && BM_GetChildren(m_entry);
  if (is_entry_expandable)
    is_entry_expanded = ! BM_IsFolded(m_entry);

  if (expandable)
    *expandable = is_entry_expandable;

  if (is_expanded)
    *is_expanded = is_entry_expanded;

  if (depth)
    *depth = m_entryDepth;

  if (ancestor)
    *ancestor = m_ancestorInfo;
}

EOutlinerTextStyle
XFE_BookmarkView::getColumnStyle(int /*column*/)
{
  XP_ASSERT(m_entry);

  return BM_IsAlias(m_entry) ? OUTLINER_Italic : OUTLINER_Default;
}

char *
XFE_BookmarkView::getColumnText(int column)
{
  static char *separator_string = "--------------------";
  static char name_buf[1024];
  static char location_buf[1024];
  static char visited_buf[1024];
  static char added_buf[1024];

  XP_ASSERT(m_entry);

  switch (column)
    {
    case OUTLINER_COLUMN_NAME:
      if (BM_IsSeparator(m_entry))
	return separator_string;
      else
	{
	  char *name;
	  unsigned char *loc;
	  
	  name = BM_GetName(m_entry);
	  loc = fe_ConvertToLocaleEncoding(INTL_DefaultWinCharSetID(NULL), (unsigned char*)name);
	  
	  XP_STRNCPY_SAFE(name_buf, (const char *)loc, sizeof(name_buf));
	  
	  if ((char *) loc != name)
	    {
	      XP_FREE(loc);
	    }
	  
	  return name_buf;
	}
    case OUTLINER_COLUMN_LOCATION:
      {
	char *address = BM_GetAddress(m_entry);
	
	if (address && !BM_IsHeader(m_entry))
	  {
	    XP_STRNCPY_SAFE(location_buf, address, sizeof(location_buf));
	  
	    return location_buf;
	  }
	else
	  {
	    return "---";
	  }
      }
    case OUTLINER_COLUMN_LASTVISITED:
      {
	char *visited = BM_PrettyLastVisitedDate(m_entry);
	
	if (visited)
	  {
	    XP_STRNCPY_SAFE(visited_buf, visited, sizeof(visited_buf));

	    return visited_buf;
	  }
	else
	  {
	    return "---";
	  }
      }	
    case OUTLINER_COLUMN_CREATEDON:
      {
        char *added = BM_PrettyAddedOnDate(m_entry);

        if (added)
	  {
	    XP_STRNCPY_SAFE(added_buf, added, sizeof(added_buf));
	  
	    return added_buf;
	  }
	else
	  {
	    return "-";
	  }
      }
    default:
      XP_ASSERT(0);
      return NULL;
    }
}

fe_icon *
XFE_BookmarkView::getColumnIcon(int column)
{
	XP_ASSERT(m_entry);

	if (column != OUTLINER_COLUMN_NAME)
		return NULL;
	
	/* do the icon for this bookmark */
	if (m_entry && (m_entry == XFE_PersonalToolbar::getToolbarFolder()))
		{
			if (BM_IsFolded(m_entry))
				{
					return &closedPersonalFolder;
				}
			else
				{
					return &openedPersonalFolder;
				}
		}
	else if (BM_IsHeader(m_entry)) 
		{
			static fe_icon *icon_list[] = {
				&openedFolder,
				&closedFolder,
				&openedFolderDest,
				&closedFolderDest,
				&openedFolderMenu,
				&closedFolderMenu,
				&openedFolderMenuDest,
				&closedFolderMenuDest
			};
			int key = 0;
			
			if (BM_IsFolded(m_entry)) key++;
			if (m_entry == BM_GetAddHeader(m_contextData)) key += 2;
			if (m_entry == BM_GetMenuHeader(m_contextData)) key += 4;
			return icon_list[key];
		} 
	else 
		{
			if (BM_IsAlias(m_entry) && BM_IsHeader(BM_GetAliasOriginal(m_entry))) 
				{
					return &closedFolder;
				} 
			else 
				{
					int url_type = NET_URL_Type(BM_GetAddress(m_entry));

					switch (url_type)
						{
						case IMAP_TYPE_URL:
						case MAILBOX_TYPE_URL:
							return &mailBookmark;
						case NEWS_TYPE_URL:
							return &newsBookmark;
						default:
                          {
                            if( BM_GetChangedState(m_entry) == BM_CHANGED_YES )
                              return &changedBookmark;
                            else
                              return &bookmark;
                          }
						}
				}
		}
}

char *
XFE_BookmarkView::getColumnName(int column)
{
	switch (column)
		{
		case OUTLINER_COLUMN_NAME:
			return "Name";
		case OUTLINER_COLUMN_LOCATION:
			return "Location";
		case OUTLINER_COLUMN_LASTVISITED:
			return "LastVisited";
		case OUTLINER_COLUMN_CREATEDON:
			return "CreatedOn";
		default:
			XP_ASSERT(0);
			return 0;
		}
}

char *
XFE_BookmarkView::getColumnHeaderText(int column)
{
	switch (column)
		{
		case OUTLINER_COLUMN_NAME:
			return XP_GetString(XFE_BM_OUTLINER_COLUMN_NAME);
		case OUTLINER_COLUMN_LOCATION:
			return XP_GetString(XFE_BM_OUTLINER_COLUMN_LOCATION);
		case OUTLINER_COLUMN_LASTVISITED:
			return XP_GetString(XFE_BM_OUTLINER_COLUMN_LASTVISITED);
		case OUTLINER_COLUMN_CREATEDON:
			return XP_GetString(XFE_BM_OUTLINER_COLUMN_CREATEDON);
		default:
			XP_ASSERT(0);
			return 0;
		}
}

fe_icon *
XFE_BookmarkView::getColumnHeaderIcon(int /*column*/)
{
  return 0;
}

EOutlinerTextStyle
XFE_BookmarkView::getColumnHeaderStyle(int /*column*/)
{
  return OUTLINER_Default;
}

void
XFE_BookmarkView::releaseLineData()
{
  delete [] m_ancestorInfo;
  m_ancestorInfo = NULL;
}

void
XFE_BookmarkView::Buttonfunc(const OutlineButtonFuncData *data)
{
  if (data->row == -1) 
    {
      CommandType sortCmd = m_lastSort;

      switch(data->column)
        {
        case OUTLINER_COLUMN_NAME:
          sortCmd = xfeCmdSortByTitle;
          break;
        case OUTLINER_COLUMN_LOCATION:
          sortCmd = xfeCmdSortByLocation;
          break;
        case OUTLINER_COLUMN_LASTVISITED:
          sortCmd = xfeCmdSortByDateLastVisited;
          break;
        case OUTLINER_COLUMN_CREATEDON:
          sortCmd = xfeCmdSortByDateCreated;
          break;
        default:
          sortCmd = 0;
        }
      if (sortCmd && isCommandEnabled(sortCmd))
          {
            if (sortCmd == m_lastSort && sortCmd != xfeCmdSortByTitle)
              m_sortDescending = !m_sortDescending;
            else
              m_sortDescending = True;

            xfe_ExecuteCommand((XFE_Frame *)m_toplevel, sortCmd);
          }
      return;
    }

  BM_Entry* entry = BM_AtIndex(m_contextData, data->row + 1);

  if (!entry) return;

  if (data->clicks == 1)
    {
      if (data->ctrl) 
        {
          m_outliner->toggleSelected(data->row);
          
          BM_ToggleItem(m_contextData, entry, TRUE, TRUE);
        } 
      else if (data->shift) 
        {
          // select the range.
          const int *selected;
          int count;
	  
          m_outliner->getSelection(&selected, &count);
	  
          if (count == 0) 
            {
              // there wasn't anything selected yet.
              BM_SelectItem(m_contextData, entry, TRUE, FALSE, TRUE);
            }
          else if (count == 1)
            {
              // There was only one, so we select the range from
              //    that item to the new one.
              BM_SelectRangeTo(m_contextData, entry);
            }
          else
            {
              // we had a range of items selected, so let's do
              // something really nice with them.
              BM_SelectRangeTo(m_contextData, entry);
            }
        } 
      else 
        {
          BM_SelectItem(m_contextData, entry, TRUE, FALSE, TRUE);

          if (data->button == Button2) 
            {
              xfe_ExecuteCommand((XFE_Frame *)m_toplevel, 
                                 xfeCmdOpenLinkNew);
            }
        }
    }
  else if (data->clicks == 2) 
    {
      BM_SelectItem(m_contextData, entry, TRUE, FALSE, TRUE);
      
      if (BM_IsAlias(entry) || BM_IsUrl(entry)) 
        {
          BM_GotoBookmark(m_contextData, entry);
        }
      else if (BM_IsHeader(entry))
        {
          if (!BM_IsFolded(entry))
            {
              BM_ClearAllChildSelection(m_contextData, entry, TRUE);
            }
          
          BM_FoldHeader(m_contextData, entry, !BM_IsFolded(entry), TRUE, FALSE);
        }
    }
  
  getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
}

void
XFE_BookmarkView::Flippyfunc(const OutlineFlippyFuncData *data)
{
	BM_Entry *entry = BM_AtIndex(m_contextData, data->row + 1);
	if (entry && BM_IsHeader(entry))
		{
			if (!BM_IsFolded(entry))
				{
					BM_ClearAllChildSelection(m_contextData, entry, TRUE);
				}
			
			BM_FoldHeader(m_contextData, entry, !BM_IsFolded(entry), TRUE, FALSE);
		}
}

XFE_Outliner *
XFE_BookmarkView::getOutliner()
{
  return m_outliner;
}

void
XFE_BookmarkView::refreshCells(int first, int last, XP_Bool /*now*/)
{
	int i;
	
	if (last == BM_LAST_CELL)
		last = BM_GetVisibleCount(m_contextData);

	m_outliner->change(first - 1, last - first + 1,
                       BM_GetVisibleCount(m_contextData));
	
	for (i = first-1; i < last; i ++)
		{
			BM_Entry *entry = BM_AtIndex(m_contextData, i + 1);
			XP_Bool bm_selected = BM_IsSelected(entry);
			XP_Bool outliner_selected = m_outliner->isSelected(i);

			if (bm_selected && !outliner_selected)
				m_outliner->selectItem(i);
			else if (!bm_selected && outliner_selected)
				m_outliner->deselectItem(i);
		}
	
  //    fe_OutlineSetMaxDepth(BM_DATA(bm_context)->outline,
  //			  BM_GetMaxDepth(bm_context));
}

void
XFE_BookmarkView::gotoBookmark(const char *url)
{
	// Goto the bookmark reusing a browser (or possibly creating a new one)
	MWContext * context = 
	  fe_reuseBrowser(m_contextData,NET_CreateURLStruct(url, NET_DONT_RELOAD));

	// If everything looks good, popup the browser in case its lost in space
	if (context && (context->type == MWContextBrowser))
	{
		XFE_BrowserFrame * frame = (XFE_BrowserFrame *)
			ViewGlue_getFrame(XP_GetNonGridContext(context));
		
		if (frame && frame->isAlive())
		{
			Widget w = frame->getBaseWidget();

			XMapRaised(XtDisplay(w),XtWindow(w));

			XtPopup(w,XtGrabNone);
		}
	}
}

void
XFE_BookmarkView::scrollIntoView(BM_Entry *entry)
{
  int num = BM_GetIndex(m_contextData, entry);

  if (num > 0) 
      m_outliner->makeVisible(num - 1);
}

void
XFE_BookmarkView::setClipContents(void *block, int32 length)
{
  freeClipContents();
  
  clip.block = XP_ALLOC(length);
  XP_MEMCPY(clip.block, block, length);
  clip.length = length;
}

void *
XFE_BookmarkView::getClipContents(int32 *length)
{
  if (length)
    *length = clip.length;

  return (clip.block);
}

void
XFE_BookmarkView::freeClipContents()
{
  if (clip.block) XP_FREE(clip.block);

  clip.block = NULL;
  clip.length = 0;
}

void
XFE_BookmarkView::openBookmarksWindow()
{
  if (m_propertiesDialog == NULL)
    {
      m_propertiesDialog =
        new XFE_BookmarkPropDialog(m_contextData, getToplevel()->getBaseWidget());
      
      XtAddCallback(m_propertiesDialog->getBaseWidget(),
                    XmNdestroyCallback, properties_destroy_cb, this);
    }

  m_propertiesDialog->show();

  getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
}

void
XFE_BookmarkView::closeBookmarksWindow()
{
  if (m_propertiesDialog)
    {
      m_propertiesDialog->close();
      m_propertiesDialog = 0;
    }

  getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
}

void
XFE_BookmarkView::startWhatsChanged()
{
  if (m_whatsNewDialog == NULL)
    {
      m_whatsNewDialog =
        new XFE_BookmarkWhatsNewDialog(m_contextData, getToplevel()->getBaseWidget());
      
      XtAddCallback(m_whatsNewDialog->getBaseWidget(),
                    XmNdestroyCallback, whats_new_destroy_cb, this);
    }

  m_whatsNewDialog->show();

  getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
}

void
XFE_BookmarkView::updateWhatsChanged(const char *url, int32 done, int32 total,
                                     const char *totaltime)
{
  m_whatsNewDialog->updateWhatsChanged(url, done, total, totaltime);
}

void
XFE_BookmarkView::finishedWhatsChanged(int32 totalchecked, int32 numreached, int32 numchanged)
{
  m_whatsNewDialog->finishedWhatsChanged(totalchecked, numreached, numchanged);
}

/* The dialog is deleted on destroy */
void 
XFE_BookmarkView::properties_destroy_cb(Widget /* widget */, XtPointer closure,
                                        XtPointer /* call_data */)
{
  XFE_BookmarkView* obj = (XFE_BookmarkView *)closure;

  obj->m_propertiesDialog = NULL;
}

/* The dialog is deleted on destroy */
void 
XFE_BookmarkView::whats_new_destroy_cb(Widget /* widget */, XtPointer closure,
                                        XtPointer /* call_data */)
{
  XFE_BookmarkView* obj = (XFE_BookmarkView *)closure;

  obj->m_whatsNewDialog = NULL;
}

void
XFE_BookmarkView::editItem(BM_Entry *entry)
{
  if (m_propertiesDialog && entry)
    {
      m_propertiesDialog->editItem(entry);
    }
}

void
XFE_BookmarkView::entryGoingAway(BM_Entry *entry)
{
  if (m_propertiesDialog != NULL &&
      m_propertiesDialog->isShown())
    {
      m_propertiesDialog->entryGoingAway(entry);
    }
}

void 
XFE_BookmarkView::setName(BM_Entry *entry, char *name)
{
  char * toolbarName = XFE_PersonalToolbar::getToolbarFolderName();
  XP_Bool isPersonalToolbar = FALSE;

  startBatch();

  if (toolbarName && XP_STRCMP(toolbarName,BM_GetName(entry)) == 0) {
    isPersonalToolbar = TRUE;
  }

  BM_SetName(m_contextData, entry, name);

  if (isPersonalToolbar) {
    XFE_PersonalToolbar::setToolbarFolder(entry, FALSE);  
  }
  
  endBatch();
}

void
XFE_BookmarkView::bookmarkMenuInvalid()
{
  if (m_batchDepth == 0)
    {
      XFE_MozillaApp::theApp()->notifyInterested(XFE_MozillaApp::bookmarksHaveChanged);

      BMFE_RefreshCells(m_contextData, 1, BM_LAST_CELL, TRUE);
      
      m_menuIsInvalid = FALSE;
    }
  else
    {
      // Since we are in a BE call, delay the update
      // until after the call is finished
      m_menuIsInvalid = TRUE;
    }
}

extern "C" void
BMFE_BookmarkMenuInvalid (MWContext* bm_context)
{
  XFE_BookmarkView *view = (XFE_BookmarkView*)BM_GetFEData(bm_context);

  if (view)
    view->bookmarkMenuInvalid();
}

extern "C" void
BMFE_UpdateWhatsChanged(MWContext* bm_context, const char* url,
						int32 done, int32 total,
						const char* totaltime)
{
  XFE_BookmarkView *view = (XFE_BookmarkView*)BM_GetFEData(bm_context);

  if (view)
    view->updateWhatsChanged(url, done, total, totaltime);
}

extern "C" void 
BMFE_FinishedWhatsChanged(MWContext* bm_context, int32 totalchecked,
						  int32 numreached, int32 numchanged)
{
  XFE_BookmarkView *view = (XFE_BookmarkView*)BM_GetFEData(bm_context);

  if (view)
    view->finishedWhatsChanged(totalchecked, numreached, numchanged);
}

void
BMFE_RefreshCells(MWContext* bm_context, int32 first, int32 last,
		  XP_Bool now )
{
  XFE_BookmarkView *view = (XFE_BookmarkView*)BM_GetFEData(bm_context);

  // Need this if because this call might (and does) happen before
  // we've call BM_SetFEData on the bookmark context.
  if (view)
    view->refreshCells(first, last, now);
}

void
BMFE_SyncDisplay(MWContext* bm_context)
{
  BMFE_RefreshCells(bm_context, 1, BM_LAST_CELL, TRUE);
}

void
BMFE_MeasureEntry(MWContext* /*bm_context*/, BM_Entry* /*entry*/,
		  uint32* width, uint32* height )
{
  *width = 1;			/* ### */
  *height = 1;
}

void
BMFE_FreeClipContents(MWContext* bm_context)
{
  XFE_BookmarkView *view = (XFE_BookmarkView*)BM_GetFEData(bm_context);
  
  if (view)
    view->freeClipContents();
}

void *
BMFE_GetClipContents(MWContext* bm_context, int32* length)
{
  XFE_BookmarkView *view = (XFE_BookmarkView*)BM_GetFEData(bm_context);

  if (view)
    return view->getClipContents(length);
  else
    return NULL;
}

void
BMFE_SetClipContents(MWContext* bm_context, void *block, int32 length )
{
  XFE_BookmarkView *view = (XFE_BookmarkView*)BM_GetFEData(bm_context);

  if (view)
    view->setClipContents(block, length);
}

void
BMFE_OpenBookmarksWindow(MWContext* bm_context)
{
  XFE_BookmarkView *view = (XFE_BookmarkView*)BM_GetFEData(bm_context);

  if (view)
    view->openBookmarksWindow();
}

void
BMFE_EditItem(MWContext* bm_context, BM_Entry* entry )
{
  XFE_BookmarkView *view = (XFE_BookmarkView*)BM_GetFEData(bm_context);
  
  if (view)
    view->editItem(entry);
}

void
BMFE_EntryGoingAway(MWContext* bm_context, BM_Entry* entry)
{
  XFE_BookmarkView *view = (XFE_BookmarkView*)BM_GetFEData(bm_context);

  if (view)
    view->entryGoingAway(entry);

#if 0
  if (entry)
  {
	  char * name = XFE_PersonalToolbar::getToolbarFolderName();
	  
	  if (name && XP_STRCMP(name,BM_GetName(entry)))
	  {
		  XFE_PersonalToolbar::setToolbarFolder(NULL,True);  
	  }
  }
#endif
}


void
BMFE_CloseBookmarksWindow(MWContext* bm_context )
{
  XFE_BookmarkView *view = (XFE_BookmarkView*)BM_GetFEData(bm_context);

  if (view)
    view->closeBookmarksWindow();
}


void
BMFE_GotoBookmark(MWContext* bm_context, const char* url, const char * /* target */ )
{
  XFE_BookmarkView *view = (XFE_BookmarkView*)BM_GetFEData(bm_context);

  if (view)
    view->gotoBookmark(url);
}


void*
BMFE_OpenFindWindow(MWContext* bm_context, BM_FindInfo* findInfo )
{
	XFE_Frame *frame = ViewGlue_getFrame(bm_context);

	fe_showBMFindDialog(frame->getBaseWidget(),
						findInfo);

	return 0;
}


void
BMFE_ScrollIntoView(MWContext* bm_context, BM_Entry* entry)
{
  XFE_BookmarkView *view = (XFE_BookmarkView*)BM_GetFEData(bm_context);

  if (view)
    view->scrollIntoView(entry);
}

void
BMFE_StartBatch(MWContext* bm_context)
{
  XFE_BookmarkView *view = (XFE_BookmarkView*)BM_GetFEData(bm_context);

  if (view)
    view->startBatch();
}

void
BMFE_EndBatch(MWContext* bm_context)
{
  XFE_BookmarkView *view = (XFE_BookmarkView*)BM_GetFEData(bm_context);

  if (view)
    view->endBatch();
}

void
XFE_BookmarkView::startBatch()
{
  m_batchDepth += 1;
}

void
XFE_BookmarkView::endBatch()
{
  m_batchDepth -= 1;

  XP_ASSERT(m_batchDepth >= 0);

  if (m_batchDepth == 0 && m_menuIsInvalid)
    bookmarkMenuInvalid();
}

#if !defined(USE_MOTIF_DND)
void
XFE_BookmarkView::dropfunc(Widget /*dropw*/, fe_dnd_Event type,
						   fe_dnd_Source *source, XEvent *event)
{
	int row = -1;
	int x, y;

	/* we only understand these targets -- well, we only understand
	   the first two. The outliner understands the COLUMN one. */
	if (source->type != FE_DND_COLUMN &&
		source->type != FE_DND_URL &&
		source->type != FE_DND_BOOKMARK)
		{
			return;
		}

	if (source->type == FE_DND_COLUMN)
		{
			m_outliner->handleDragEvent(event, type, source);

			return;
		}

	m_outliner->translateFromRootCoords(event->xbutton.x_root, event->xbutton.y_root, &x, &y);
	
    XP_Bool isBelow = FALSE;

	row = m_outliner->XYToRow(x, y, &isBelow);

	switch (type)
		{
		case FE_DND_START:
			break;
		case FE_DND_DRAG:
			if (BM_IsDragEffectBox(m_contextData, row + 1, isBelow))
				m_outliner->outlineLine(row);
			else
				m_outliner->underlineLine(row);
			break;
		case FE_DND_DROP:
			if (row >= 0)
				{
					if (source->type == FE_DND_BOOKMARK)
						{
                          startBatch();
                          BM_DoDrop(m_contextData, row + 1, isBelow);
                          endBatch();
						}
					else if (source->type == FE_DND_URL)
						{
						}

					// to clear the drag feedback.
					if (BM_IsDragEffectBox(m_contextData, row + 1, isBelow))
						m_outliner->outlineLine(-1);
					else
						m_outliner->underlineLine(-1);
				}
            break;
        case FE_DND_END:
            break;
		}
}

void
XFE_BookmarkView::drop_func(Widget dropw, void *closure, fe_dnd_Event type,
							fe_dnd_Source *source, XEvent* event)
{
	XFE_BookmarkView *obj = (XFE_BookmarkView*)closure;
	
	obj->dropfunc(dropw, type, source, event);
}
#endif /* */

void
XFE_BookmarkView::doPopup(XEvent *event)
{
  int x, y, clickrow;
  BM_Entry *entryUnderMouse;

  Widget widget = XtWindowToWidget(event->xany.display, event->xany.window);
  if (widget == NULL)
    widget = m_widget;
		
  if (m_popup)
    delete m_popup;

  m_outliner->translateFromRootCoords(event->xbutton.x_root,
                                      event->xbutton.y_root,
                                      &x, &y);

  clickrow = m_outliner->XYToRow(x, y);

  if (clickrow > -1) 
      entryUnderMouse = BM_AtIndex(m_contextData, clickrow + 1);
  else
      entryUnderMouse = NULL;
		

  m_popup = new XFE_PopupMenu("popup",
							  (XFE_Frame*)m_toplevel, // XXXXXXX
                              widget,
                              NULL);

  XP_Bool isUrl = BM_IsUrl(entryUnderMouse) || BM_IsAlias(entryUnderMouse);
  XP_Bool isFolder = BM_IsHeader(entryUnderMouse);
  
  // possible types - url, alias, address, separator, header (i.e. folder)

  if (isUrl)             m_popup->addMenuSpec(open_popup_spec);
  if (1)                 m_popup->addMenuSpec(new_popup_spec);  // always add
  if (isFolder)          m_popup->addMenuSpec(set_popup_spec);
  if (isUrl)             m_popup->addMenuSpec(saveas_popup_spec);
  if (entryUnderMouse)   m_popup->addMenuSpec(cutcopy_popup_spec);
  if (isUrl)             m_popup->addMenuSpec(copylink_popup_spec);
  if (BM_FindCommandStatus(m_contextData, BM_Cmd_Paste))
                         m_popup->addMenuSpec(paste_popup_spec);
  if (entryUnderMouse)   m_popup->addMenuSpec(delete_popup_spec);
  if (isUrl)             m_popup->addMenuSpec(makealias_popup_spec);
  if (isUrl || isFolder) m_popup->addMenuSpec(properties_popup_spec);

  // Make sure we select what is under the mouse
  if (entryUnderMouse)
    {
      BM_SelectItem(m_contextData, entryUnderMouse,
                    TRUE, FALSE, TRUE);
    }

  m_popup->position(event);
  
  m_popup->show();
}

#if defined(USE_MOTIF_DND)
fe_icon_data *
XFE_BookmarkView::GetDragIconData(int row, int column)
{
  D(("XFE_BookmarkView::GetDragIconData()\n"));
  
  return &BM_Bookmark;
}

fe_icon_data *
XFE_BookmarkView::getDragIconData(void *this_ptr,
								int row,
								int column)
{
  D(("XFE_Bookmarkview::getDragIconData()\n"));
  XFE_BookmarkView *bookmark_view = (XFE_BookmarkView*)this_ptr;

  return bookmark_view->GetDragIconData(row, column);
}

void
XFE_BookmarkView::GetDragTargets(int row, int column,
								 Atom **targets,
								 int *num_targets)
{
  D(("XFE_FolderView::GetDragTargets()\n"));
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

	  *num_targets = 2;

	  *targets = new Atom[ *num_targets ];

	  (*targets)[0] = XFE_DropBase::_XA_NETSCAPE_URL;
	  (*targets)[1] = XA_STRING;
	}
}

void
XFE_BookmarkView::getDragTargets(void *this_ptr,
							   int row,
							   int column,
							   Atom **targets,
							   int *num_targets)
{
  D(("XFE_BookmarkView::getDragTargets()\n"));
  XFE_BookmarkView *bookmark_view = (XFE_BookmarkView*)this_ptr;

  bookmark_view->GetDragTargets(row, column, targets, num_targets);
}

void 
XFE_BookmarkView::getDropTargets(void */*this_ptr*/,
							   Atom **targets,
							   int *num_targets)
{
  D(("XFE_BookmarkView::getDropTargets()\n"));
  *num_targets = 2;
  *targets = new Atom[ *num_targets ];

  (*targets)[0] = XFE_DragBase::_XA_NETSCAPE_URL;
  (*targets)[1] = XA_STRING;
}

char *
XFE_BookmarkView::DragConvert(Atom atom)
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
		  BM_Entry *entry = BM_AtIndex(m_contextData, selection[i] + 1);

		  urlData.url(i,BM_GetAddress(entry));
		}

	  result = XtNewString(urlData.getString());
	  
	  return result;
	}
  else if (atom == XFE_OutlinerDrop::_XA_NETSCAPE_BOOKMARK)
	{
	  return (char*)XtNewString("");
	}
  else if (atom == XA_STRING)
	{
#if notyet
	  return (char*)XtNewString(BM_GetAddress(m_dragentry));
#endif
	}
}

char *
XFE_BookmarkView::dragConvert(void *this_ptr,
							Atom atom)
{
  XFE_BookmarkView *bookmark_view = (XFE_BookmarkView*)this_ptr;
  
  return bookmark_view->DragConvert(atom);
}

int
XFE_BookmarkView::ProcessTargets(int row, int col,
								 Atom *targets,
								 const char **data,
								 int numItems)
{
  int i;

  D(("XFE_BookmarkView::ProcessTargets(row=%d, col=%d, numItems=%d)\n",row, col, numItems));

  for (i = 0; i<numItems; i ++)
	{
	  if (targets[i]==None || data[i]==NULL || strlen(data[i])==0)
		continue;

	  D(("  [%d] %s: \"%s\"\n",i,XmGetAtomName(XtDisplay(m_widget),targets[i]),data[i]));

	  if (targets[i] == XFE_DropBase::_XA_NETSCAPE_URL)
		{		  
		  BM_Entry *drop_entry;
		  XFE_URLDesktopType urlData(data[i]);
		  int i;

		  drop_entry = BM_AtIndex(m_contextData, row + 1);
		  if (!drop_entry) return FALSE;

		  for (i = urlData.numItems() - 1; i >= 0; i --)
			{
			  const char *title;
			  BM_Entry* new_entry;

			  title = urlData.title(i);
			  if (!title) title = urlData.url(i);

			  new_entry = BM_NewUrl(title,
									urlData.url(i),
									NULL, 0);

			  BM_InsertItemInHeaderOrAfterItem(m_contextData,
											   drop_entry,
											   new_entry);
			}

		  return TRUE;
		}
	  else if (targets[i] == XFE_OutlinerDrop::_XA_NETSCAPE_BOOKMARK)
		{
		  startBatch();
		  BM_DoDrop(m_contextData, row + 1, FALSE);
		  endBatch();
		  return TRUE;
		}
	}

  return FALSE;
}

int
XFE_BookmarkView::processTargets(void *this_ptr,
							   int row, int col,
							   Atom *targets,
							   const char **data,
							   int numItems)
{
  XFE_BookmarkView *bookmark_view = (XFE_BookmarkView*)this_ptr;
  
  return bookmark_view->ProcessTargets(row, col, targets, data, numItems);
}
#endif /* USE_MOTIF_DND */
