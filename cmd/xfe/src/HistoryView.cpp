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
   HistoryView.h -- view of user's history
   Created: Stephen Lamm <slamm@netscape.com>, 24-Feb-97.
 */



#include "MozillaApp.h"
#include "HistoryView.h"

#if defined(USE_MOTIF_DND)
#include "OutlinerDrop.h"
#endif /* USE_MOTIF_DND */

#include "BookmarkFrame.h"  // for fe_getBookmarkContext
#include "BrowserFrame.h"  // for fe_reuseBrowser
#include "PersonalToolbar.h" // for XFE_PersonalToolbar::getToolbarFolder
#include "xfe.h"
#include "bkmks.h"    // for BM_NewUrl
#include "felocale.h" /* for fe_ConvertToLocalEncoding */
#include "prefs.h"    // for fe_showPreferences
#include "xplocale.h" // for FE_StrfTime
#include "xp_mem.h"
#include "prefapi.h"
#include "undo.h"
#include "xpgetstr.h"
extern int XFE_HISTORY_OUTLINER_COLUMN_TITLE;
extern int XFE_HISTORY_OUTLINER_COLUMN_LOCATION;
extern int XFE_HISTORY_OUTLINER_COLUMN_FIRSTVISITED;
extern int XFE_HISTORY_OUTLINER_COLUMN_LASTVISITED;
extern int XFE_HISTORY_OUTLINER_COLUMN_EXPIRES;
extern int XFE_HISTORY_OUTLINER_COLUMN_VISITCOUNT;

#if DEBUG_username
#define DDD(x) x
#else
#define DDD(x)
#endif

#define HISTORY_OUTLINER_GEOMETRY_PREF "history.outliner_geometry"
// date format is like "19:21 Thu 27 Feb 97"
//#define DATE_FORMAT "%H:%M %a %e %b %y"
//
// I liked my date format better, but in the name of consistency, 
//   we will do this instead...
#define DATE_FORMAT  XP_DATE_TIME_FORMAT

const int XFE_HistoryView::OUTLINER_COLUMN_TITLE        = 0;
const int XFE_HistoryView::OUTLINER_COLUMN_LOCATION     = 1;
const int XFE_HistoryView::OUTLINER_COLUMN_FIRSTVISITED = 2;
const int XFE_HistoryView::OUTLINER_COLUMN_LASTVISITED  = 3;
const int XFE_HistoryView::OUTLINER_COLUMN_EXPIRES      = 4;
const int XFE_HistoryView::OUTLINER_COLUMN_VISITCOUNT   = 5;

/* pixmaps for use in the history window. */
fe_icon XFE_HistoryView::historyIcon = { 0 };

XFE_HistoryView::XFE_HistoryView(XFE_Component *toplevel_component, 
				   Widget parent,
				   XFE_View *parent_view, MWContext *context) 
  : XFE_View(toplevel_component, parent_view, context)
{
  int num_columns = 6;
  static int column_widths[] = {35, 40, 25, 25, 25, 15};

  m_outliner = new XFE_Outliner("historyList",
								this, 
								getToplevel(),
								parent,
								False, // constantSize 
								True,  // hasHeadings
								num_columns,  // Number of columns.
								num_columns,  // Number of visible columns.
								column_widths, 
								HISTORY_OUTLINER_GEOMETRY_PREF);

  m_outliner->setHideColumnsAllowed( TRUE );

  // is this the hierarchical stuff??
  //  m_outliner->setPipeColumn( OUTLINER_COLUMN_TITLE );

  setBaseWidget(m_outliner->getBaseWidget());

  // Initialize clip stuff.
  /*
  clip.block = NULL;
  clip.length = 0;
  */
  {
    Pixel bg_pixel;
    
    XtVaGetValues(m_widget, XmNbackground, &bg_pixel, 0);

    if (!historyIcon.pixmap)
      {
        fe_NewMakeIcon(getToplevel()->getBaseWidget(),
                       BlackPixelOfScreen(XtScreen(m_widget)),
                       bg_pixel,
                       &historyIcon,
                       NULL,
                       HGroup.width, HGroup.height,
                       HGroup.mono_bits, HGroup.color_bits,
                       HGroup.mask_bits, FALSE);
      }
  }

#if defined(USE_MOTIF_DND)
  m_outliner->enableDragDrop(this,
							 NULL, /* no dropping -- of anything but outliner columns. */
							 XFE_HistoryView::getDragTargets,
							 XFE_HistoryView::getDragIconData,
							 XFE_HistoryView::dragConvert);

  m_outliner->setMultiSelectAllowed(True);
#else
  m_outliner->setDragType(FE_DND_HISTORY, &historyIcon,
						  this);
  m_outliner->setMultiSelectAllowed(True);

  fe_dnd_CreateDrop(m_outliner->getBaseWidget(), drop_func, this);


#endif /* USE_MOTIF_DND */

  m_filter = NULL;
  m_sortBy = eHSC_LASTVISIT;
  m_sortDescending = False;
  m_selectList = XP_ListNew();

  m_outliner->setSortColumn(m_sortBy, OUTLINER_SortAscending );

  // Save vertical scroller for keyboard accelerators
  CONTEXT_DATA(context)->vscroll = m_outliner->getScroller();

  refresh();

  // Select the first row
  if (m_outliner->getTotalLines() > 0)
    {
      m_outliner->selectItemExclusive(0);
    }
}

XFE_HistoryView::~XFE_HistoryView()
{
  char *pszKey;
  while( (pszKey = (char *)XP_ListRemoveTopObject( m_selectList )) )
    {
      delete [] pszKey;
    }
  XP_ListDestroy(m_selectList);
}

Boolean
XFE_HistoryView::isCommandEnabled(CommandType cmd, void *calldata,
                                   XFE_CommandInfo* info)
{
#define IS_CMD(command) cmd == (command)
  const int *selected;
  int count;
  
  m_outliner->getSelection(&selected, &count);
  
  if (IS_CMD(xfeCmdOpenSelected)
      || cmd == xfeCmdAddToToolbar
      || cmd == xfeCmdAddBookmark
      )
    {
      return count == 1;
    } 
  else if (
           IS_CMD(xfeCmdSaveAs)
           || IS_CMD(xfeCmdOpenSelected)

           || IS_CMD(xfeCmdSelectAll)

           || IS_CMD(xfeCmdSortByTitle)
           || IS_CMD(xfeCmdSortByLocation)
           || IS_CMD(xfeCmdSortByDateFirstVisited)
           || IS_CMD(xfeCmdSortByDateLastVisited)
           || IS_CMD(xfeCmdSortByExpirationDate)
           || IS_CMD(xfeCmdSortByVisitCount)
           || IS_CMD(xfeCmdSortAscending)
           || IS_CMD(xfeCmdSortDescending)
           //           || IS_CMD(xfeCmdMoreOptions)
           //           || IS_CMD(xfeCmdFewerOptions)
           || IS_CMD(xfeCmdEditPreferences)
           )
    {
	  return True;
    } 
  else if (
           IS_CMD(xfeCmdPrintSetup)
           || IS_CMD(xfeCmdPrint)

           || IS_CMD(xfeCmdUndo)
           || IS_CMD(xfeCmdRedo)
           || IS_CMD(xfeCmdCut)
           || IS_CMD(xfeCmdCopy)
           || IS_CMD(xfeCmdDelete)
           || IS_CMD(xfeCmdSearch)
           )
    {
      return False;
    }
  else
    {
      return XFE_View::isCommandEnabled(cmd, calldata, info);
    }
#undef IS_CMD
}

void
XFE_HistoryView::doCommand(CommandType cmd, void *calldata, XFE_CommandInfo*info)
{
#define IS_CMD(command) cmd == (command)

  if (
      IS_CMD(xfeCmdPrintSetup)
      || IS_CMD(xfeCmdPrint)
      
      || IS_CMD(xfeCmdUndo)
      || IS_CMD(xfeCmdRedo)
      || IS_CMD(xfeCmdCut)
      || IS_CMD(xfeCmdCopy)
      || IS_CMD(xfeCmdDelete)
      || IS_CMD(xfeCmdSearch)
      
      //      || IS_CMD(xfeCmdMoreOptions)
      //      || IS_CMD(xfeCmdFewerOptions)
      )
    {
      DDD( printf("Unimplemented command: %s\n", Command::getString(cmd)); )
      return;  //XXX do nothing for now
    }
  else if (IS_CMD(xfeCmdOpenSelected))
    {
      openSelected();
      return;
    }
  else if (IS_CMD(xfeCmdSaveAs))
    {
      GH_FileSaveAsHTML( m_histCursor, getContext());
      return;
    }
  else if (IS_CMD(xfeCmdSelectAll))
    {
      m_outliner->selectAllItems();
    }
  else if (IS_CMD(xfeCmdSortByTitle)
           || IS_CMD(xfeCmdSortByLocation)
           || IS_CMD(xfeCmdSortByDateFirstVisited)
           || IS_CMD(xfeCmdSortByDateLastVisited)
           || IS_CMD(xfeCmdSortByExpirationDate)
           )
    {
      resort((enHistSortCol)calldata, False);
      return;
    }
  else if (IS_CMD(xfeCmdSortByVisitCount))
    {
      resort((enHistSortCol)calldata, True);
      return;
    }
  else if (IS_CMD(xfeCmdSortAscending))
    {
      resort(m_sortBy, False);
      return;
    }
  else if (IS_CMD(xfeCmdSortDescending))
    {
      resort(m_sortBy, True);
      return;
    }
  else if (IS_CMD(xfeCmdEditPreferences))
    {
	  fe_showPreferences(getToplevel(), getContext());

      getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
  else if (cmd == xfeCmdAddToToolbar)
    {
      const int *	selected;
      int			count;
		
      m_outliner->getSelection(&selected, &count);
      
      if (count == 1 && selected[0] >= 0
          && selected[0] < m_outliner->getTotalLines() - 1)
        {
          BM_Entry * personalTB =
            XFE_PersonalToolbar::getToolbarFolder();
          gh_HistEntry* gh_entry =
            GH_GetRecord(m_histCursor, selected[0]);

          if (personalTB && gh_entry)
            {
              BM_Entry* entry = 
                BM_NewUrl(gh_entry->pszName,
                          gh_entry->address,
                          NULL,
                          gh_entry->last_accessed);
              
              BM_AppendToHeader(fe_getBookmarkContext(),
                                personalTB, entry);
            }
        }
    }
  else if (cmd == xfeCmdAddBookmark)
    {
      const int *	selected;
      int			count;
		
      m_outliner->getSelection(&selected, &count);
      
      if (count == 1 && selected[0] >= 0
          && selected[0] < m_outliner->getTotalLines() - 1)
        {
          gh_HistEntry* gh_entry =
            GH_GetRecord(m_histCursor, selected[0]);

          if (!gh_entry) return;

          BM_Entry* entry = 
            BM_NewUrl(gh_entry->pszName,
                      gh_entry->address,
                      NULL,
                      gh_entry->last_accessed);
          
          BM_AppendToHeader (XFE_BookmarkFrame::main_bm_context,
                             BM_GetAddHeader(XFE_BookmarkFrame::main_bm_context), entry);
        }
    }
  else
    {
      XFE_View::doCommand(cmd,calldata,info);
    }
#undef IS_CMD
}

Boolean
XFE_HistoryView::handlesCommand(CommandType cmd, void *calldata,
                                 XFE_CommandInfo* info)
{
#define IS_CMD(command) cmd == (command)

  if (
      IS_CMD(xfeCmdSaveAs)
      || IS_CMD(xfeCmdOpenSelected)
      || IS_CMD(xfeCmdAddToToolbar)
      || IS_CMD(xfeCmdAddBookmark)
      || IS_CMD(xfeCmdPrintSetup)
      || IS_CMD(xfeCmdPrint)
      
      || IS_CMD(xfeCmdUndo)
      || IS_CMD(xfeCmdRedo)
      || IS_CMD(xfeCmdCut)
      || IS_CMD(xfeCmdCopy)
      || IS_CMD(xfeCmdDelete)
      || IS_CMD(xfeCmdSelectAll)
      || IS_CMD(xfeCmdSearch)

      || IS_CMD(xfeCmdSortByTitle)
      || IS_CMD(xfeCmdSortByLocation)
      || IS_CMD(xfeCmdSortByDateFirstVisited)
      || IS_CMD(xfeCmdSortByDateLastVisited)
      || IS_CMD(xfeCmdSortByExpirationDate)
      || IS_CMD(xfeCmdSortByVisitCount)
      || IS_CMD(xfeCmdSortAscending)
      || IS_CMD(xfeCmdSortDescending)
      //      || IS_CMD(xfeCmdMoreOptions)
      //      || IS_CMD(xfeCmdFewerOptions)
      || IS_CMD(xfeCmdEditPreferences)
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

char *
XFE_HistoryView::commandToString(CommandType, void*, XFE_CommandInfo*)
{
    return NULL;
}

XP_Bool
XFE_HistoryView::isCommandSelected(CommandType cmd,
                                   void *calldata, XFE_CommandInfo* info)
{
#define IS_CMD(command) cmd == (command)

  // This method is designed for toggle button.
  // We want to keep the toggle button to have the same state
  //   as its matched view.

  if (IS_CMD(xfeCmdSortByTitle)
      || IS_CMD(xfeCmdSortByLocation)
      || IS_CMD(xfeCmdSortByDateFirstVisited)
      || IS_CMD(xfeCmdSortByDateLastVisited)
      || IS_CMD(xfeCmdSortByExpirationDate)
      || IS_CMD(xfeCmdSortByVisitCount)
      )
    {
      return m_sortBy == (enHistSortCol)calldata;
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

// Methods from the outlinable interface.
void *
XFE_HistoryView::ConvFromIndex(int index)
{
  return acquireLineData(index);
}

int
XFE_HistoryView::ConvToIndex(void */*item*/)
{
  // do we need this function?
  XP_ASSERT(0);
  return 0;
}


void *
XFE_HistoryView::acquireLineData(int line)
{
  if( m_sortDescending )
    {
      m_entry = GH_GetRecord(m_histCursor, m_totalLines - 1 - line);
    }
  else
    {
      m_entry = GH_GetRecord(m_histCursor, line);
    }

  return m_entry;
}

void
XFE_HistoryView::getTreeInfo(XP_Bool */*expandable*/,
			      XP_Bool */*is_expanded*/,
			      int *depth,
			      OutlinerAncestorInfo **/*ancestor*/)
{
  depth = 0;
}

EOutlinerTextStyle
XFE_HistoryView::getColumnStyle(int /*column*/)
{
  return OUTLINER_Default;
}

char *
XFE_HistoryView::getColumnText(int column)
{
  static char title_buf[1024];
  static char location_buf[1024];
  static char first_visited_buf[1024];
  static char last_visited_buf[1024];
  static char expiration_date_buf[1024];
  static char visit_count_buf[1024];

  XP_ASSERT(m_entry);

  switch (column)
    {
    case OUTLINER_COLUMN_TITLE:
      {
        char *title;
        unsigned char *loc;
	  
        title = m_entry->pszName;
        loc = fe_ConvertToLocaleEncoding(INTL_DefaultWinCharSetID(NULL),
                                         (unsigned char*)title);
	  
        strcpy(title_buf, (const char *)loc);
        title_buf[1023] = 0;
	  
        if ((char *) loc != title)
          {
            XP_FREE(loc);
          }
	  
        return title_buf;
      }
    case OUTLINER_COLUMN_LOCATION:
      {
        if (m_entry->address)
          {
            strcpy(location_buf, m_entry->address);
            location_buf[1023] = 0;
	  
            return location_buf;
          }
      }
    case OUTLINER_COLUMN_FIRSTVISITED:
      {
        struct tm* tmp;
        
        XP_ASSERT(m_entry);
        
        if (m_entry && m_entry->first_accessed != 0)
          {
            tmp = localtime(&(m_entry->first_accessed));
            FE_StrfTime(getContext(), first_visited_buf, 1024,
                        DATE_FORMAT, tmp);
            return first_visited_buf;
          }
        return NULL;
      }
    case OUTLINER_COLUMN_LASTVISITED:
      {
        struct tm* tmp;
        
        XP_ASSERT(m_entry);
        
        if (m_entry && m_entry->last_accessed != 0)
          {
            tmp = localtime(&(m_entry->last_accessed));
            FE_StrfTime(getContext(), last_visited_buf, 1024,
                        DATE_FORMAT, tmp);
            return last_visited_buf;
          }
        return NULL;
      }
    case OUTLINER_COLUMN_EXPIRES:
      {
        
        int32 expireDays;
        PREF_GetIntPref("browser.link_expiration",&expireDays);
        
        struct tm* tmp;
        
        XP_ASSERT(m_entry);
        
        if (m_entry && m_entry->last_accessed != 0)
          {
            time_t expireSecs = m_entry->last_accessed;
            expireSecs += expireDays * 60 * 60 * 24;

            tmp = localtime(&expireSecs);
            FE_StrfTime(getContext(), expiration_date_buf, 1024,
                        DATE_FORMAT, tmp);
            return expiration_date_buf;
          }
        return NULL;
      }
    case OUTLINER_COLUMN_VISITCOUNT:
      {
        XP_ASSERT(m_entry);
        
        if (m_entry && m_entry->iCount != 0)
          {
            sprintf(visit_count_buf, "%d", m_entry->iCount);
            visit_count_buf[1023] = 0;
            return visit_count_buf;
          }
        return NULL;
      }
    default:
      XP_ASSERT(0);
      return NULL;
    }
}

fe_icon *
XFE_HistoryView::getColumnIcon(int column)
{
  XP_ASSERT(m_entry);
  
  if (column != OUTLINER_COLUMN_TITLE)
    return NULL;
  
  return &historyIcon;
}

char *
XFE_HistoryView::getColumnName(int column)
{
	switch (column)
		{
		case OUTLINER_COLUMN_TITLE:
			return "title";
		case OUTLINER_COLUMN_LOCATION:
			return "location";
		case OUTLINER_COLUMN_FIRSTVISITED:
			return "firstVisited";
		case OUTLINER_COLUMN_LASTVISITED:
			return "lastVisited";
		case OUTLINER_COLUMN_EXPIRES:
			return "expires";
		case OUTLINER_COLUMN_VISITCOUNT:
			return "visitCount";
		default:
			XP_ASSERT(0);
			return 0;
		}
}

char *
XFE_HistoryView::getColumnHeaderText(int column)
{
	switch (column)
		{
		case OUTLINER_COLUMN_TITLE:
			return XP_GetString(XFE_HISTORY_OUTLINER_COLUMN_TITLE);
		case OUTLINER_COLUMN_LOCATION:
			return XP_GetString(XFE_HISTORY_OUTLINER_COLUMN_LOCATION);
		case OUTLINER_COLUMN_FIRSTVISITED:
			return XP_GetString(XFE_HISTORY_OUTLINER_COLUMN_FIRSTVISITED);
		case OUTLINER_COLUMN_LASTVISITED:
			return XP_GetString(XFE_HISTORY_OUTLINER_COLUMN_LASTVISITED);
		case OUTLINER_COLUMN_EXPIRES:
			return XP_GetString(XFE_HISTORY_OUTLINER_COLUMN_EXPIRES);
		case OUTLINER_COLUMN_VISITCOUNT:
			return XP_GetString(XFE_HISTORY_OUTLINER_COLUMN_VISITCOUNT);
		default:
			XP_ASSERT(0);
			return 0;
		}
}

fe_icon *
XFE_HistoryView::getColumnHeaderIcon(int /*column*/)
{
  return 0;
}

EOutlinerTextStyle
XFE_HistoryView::getColumnHeaderStyle(int /*column*/)
{
  return OUTLINER_Default;
}

void
XFE_HistoryView::releaseLineData()
{
  // nothing to do
  return;
}

void
XFE_HistoryView::Buttonfunc(const OutlineButtonFuncData *data)
{
  if (data->row == -1) 
    {
      // sort based on a column

      enHistSortCol sortBy = (enHistSortCol)data->column;

      if (sortBy == m_sortBy)
        {
          resort(sortBy, !m_sortDescending);
        }
      else if (sortBy != eHSC_VISITCOUNT)
        {
          resort((enHistSortCol)data->column, False);
        }
      else
        {
          resort((enHistSortCol)data->column, True);
        }
      return;
    }

  gh_HistEntry* entry = GH_GetRecord(m_histCursor, data->row);

  if (!entry) return;

  if (data->clicks == 1)
    {
      if (data->ctrl) 
        {
          if (m_outliner->isSelected(data->row))
            m_outliner->deselectItem(data->row);
          else
            m_outliner->selectItem(data->row);
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
              m_outliner->selectItemExclusive(data->row);
            }
          else if (count == 1)
            {
              // There was only one, so we select the range from
              //    that item to the new one.
              m_outliner->selectRangeByIndices(selected[0], data->row);
            }
          else
            {
              // we had a range of items selected, so let's do
              // something really nice with them.
              m_outliner->trimOrExpandSelection(data->row);
            }
        } 
      else 
        {
          m_outliner->selectItemExclusive(data->row);
        }
    }
  else if (data->clicks == 2) 
    {
      m_outliner->selectItemExclusive(data->row);
      
      openSelected();
    }
  
  //  getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
}

void
XFE_HistoryView::Flippyfunc(const OutlineFlippyFuncData *data)
{
/*  gh_HistEntry* entry = GH_GetRecord(m_histCursor, data->row);
  if (entry && BM_IsHeader(entry))
    {
      if (!BM_IsFolded(entry))
	{
	  BM_ClearAllChildSelection((MWContext*)m_bmdata, entry, TRUE);
	}

      BM_FoldHeader((MWContext*)m_bmdata, entry, !BM_IsFolded(entry), TRUE, FALSE);
    }
    */
  DDD( printf("XFE_HistoryView::Flippyfunc(row = %d)\n", data->row+1); )
}

XFE_Outliner *
XFE_HistoryView::getOutliner()
{
  return m_outliner;
}

void
XFE_HistoryView::refreshCells(int first, int last, XP_Bool /*now*/)
{
  m_outliner->change(first - 1, last - first + 1, m_totalLines);

  //    fe_OutlineSetMaxDepth(BM_DATA(bm_context)->outline,
  //			  BM_GetMaxDepth(bm_context));

  DDD( printf("refreshCells %d to %d with %d total\n", first, last, 
              m_totalLines); )
}

void
XFE_HistoryView::scrollIntoView(gh_HistEntry *entry)
{
  int num = GH_GetRecordNum(m_histCursor, entry->pszName);

  if (num > 0) 
      m_outliner->makeVisible(num - 1);
}

void
XFE_HistoryView::setClipContents(void */*block*/, int32 /*length*/)
{
  freeClipContents();
  
  /*clip.block = XP_ALLOC(length);
  XP_MEMCPY(clip.block, block, length);
  clip.length = length;
*/
}

void *
XFE_HistoryView::getClipContents(int32 */*length*/)
{
  /*
  if (length)
    *length = clip.length;

  return (clip.block);
*/
  return NULL;
}

void
XFE_HistoryView::freeClipContents()
{
  /*
if (clip.block) XP_FREE(clip.block);

  clip.block = NULL;
  clip.length = 0;
  */
}
/*
void
XFE_HistoryView::openHistoryWindow()
{
#if 0
  if (m_editshell)
    {
      makeEditItemDialog();
      m_editentry = NULL;
    }

  XtManageChild(m_editshell);
  XMapRaised(XtDisplay(m_editshell), XtWindow(m_editshell));
#endif
}
*/
#if !defined(USE_MOTIF_DND)
void
XFE_HistoryView::dropfunc(Widget /*dropw*/, fe_dnd_Event type,
						   fe_dnd_Source *source, XEvent *event)
{
  int row = -1;
  int x, y;
  
  /* we only understand these targets -- well, we only understand
     the first two. The outliner understands the COLUMN one. */
  if (source->type != FE_DND_COLUMN &&
      source->type != FE_DND_URL &&
      source->type != FE_DND_HISTORY)
    {
      return;
    }
  
  if (source->type == FE_DND_COLUMN)
    {
      m_outliner->handleDragEvent(event, type, source);
      
      return;
    }
  
  m_outliner->translateFromRootCoords(event->xbutton.x_root, event->xbutton.y_root, &x, &y);
  
  row = m_outliner->XYToRow(x, y);
  
  switch (type)
    {
    case FE_DND_START:
      break;
    case FE_DND_DRAG:
      m_outliner->outlineLine(row);
      break;
    case FE_DND_DROP:
      if (row >= 0)
        {
          if (source->type == FE_DND_HISTORY)
            {
              //BM_DoDrop(m_contextData, row + 1, FALSE);
            }
          else if (source->type == FE_DND_URL)
            {
            }
          
          // to clear the drag feedback.
          m_outliner->outlineLine(-1);
        }
      break;
    case FE_DND_END:
      break;
    }
}

void
XFE_HistoryView::drop_func(Widget dropw, void *closure, fe_dnd_Event type,
							fe_dnd_Source *source, XEvent* event)
{
	XFE_HistoryView *obj = (XFE_HistoryView*)closure;
	
	obj->dropfunc(dropw, type, source, event);
}
#endif /* USE_MOTIF_DND */

int
XFE_HistoryView::notify(gh_NotifyMsg *msg)
{
  // Force a refresh now
  // Eventually we should set a dirty flag and do this in the idle loop

  if (!m_dirty) 
    {
      m_dirty = TRUE;
      XtAppAddTimeOut(XtWidgetToApplicationContext(getBaseWidget()),
                      0,
                      &XFE_HistoryView::refresh_cb,
                      this);
    }


  if (msg->iNotifyMsg == GH_NOTIFY_UPDATE && msg->pszKey)
    {
      DDD( printf("notify: update %s\n",msg->pszKey); )
    }
  return TRUE;
}

int
XFE_HistoryView::notify_cb(gh_NotifyMsg *msg)
{
  XFE_HistoryView *obj = (XFE_HistoryView *)msg->pUserData;
  
  return obj->notify(msg);
}

void 
XFE_HistoryView::resort(enHistSortCol sortBy, XP_Bool sortDescending)
{
  if (m_sortBy != sortBy)
    {
      saveSelected();

      m_sortBy = sortBy;
      m_sortDescending = sortDescending;
      refresh();

      m_outliner->setSortColumn(m_sortBy, (m_sortDescending ? 
                                           OUTLINER_SortDescending :
                                           OUTLINER_SortAscending));

      restoreSelected();
    }
  else if (m_sortDescending != sortDescending)
    {
      // Just changing the order
      m_sortDescending = sortDescending;

      m_outliner->toggleSortDirection();

      refreshCells(1, m_totalLines, TRUE);
      reflectSelected();
    }
}

void
XFE_HistoryView::refresh()
{

  gh_SortColumn sortCol;

  if (m_sortBy < eHSC_EXPIRES)
    {
      sortCol = (gh_SortColumn)m_sortBy;
    }
  else if (m_sortBy == eHSC_EXPIRES)
    {
          sortCol = eGH_LastDateSort;
    }
  else
    {
      sortCol = eGH_VisitCountSort;
    }
  
  m_histCursor = GH_GetContext(sortCol, m_filter, 
                               &XFE_HistoryView::notify_cb,
                               NULL, this );
  
  XP_ASSERT( m_histCursor );

  // Initialize undo/redo support
  GH_SupportUndoRedo( m_histCursor );

  m_totalLines = GH_GetNumRecords( m_histCursor );

  refreshCells(1, m_totalLines, TRUE);

  m_dirty = FALSE;

  DDD( printf("Refresh history\n"); )
}

void
XFE_HistoryView::refresh_cb(XtPointer closure, XtIntervalId */*id*/)
{
  XFE_HistoryView *obj = (XFE_HistoryView *)closure;

  obj->saveSelected();
  obj->refresh();
  obj->restoreSelected();
}

gh_HistEntry *
XFE_HistoryView::getEntry(int index)
{
  if (index >= 0)
    {
      if( m_sortDescending )
        {
          return GH_GetRecord(m_histCursor, m_totalLines - 1 - index);
        }
      else
        {
          return GH_GetRecord(m_histCursor, index);
        }
    }
  else
    return NULL;
}

gh_HistEntry *
XFE_HistoryView::getSelection()
{
  const int *selected;
  int count;
  
  m_outliner->getSelection(&selected, &count);
  
  if (count >= 1)
    {
      if( m_sortDescending )
        {
          return GH_GetRecord(m_histCursor, m_totalLines - 1 - selected[0]);
        }
      else
        {
          return GH_GetRecord(m_histCursor, selected[0]);
        }
    }
  else
    return NULL;
}

void
XFE_HistoryView::openSelected()
{
  gh_HistEntry *entry = getSelection();
      
  if (entry)
    {
      fe_reuseBrowser(getContext(),
                      NET_CreateURLStruct(entry->address,
                                          NET_DONT_RELOAD));
    }
}

void 
XFE_HistoryView::reflectSelected()
{
  const int *selected;
  int count;
  
  m_outliner->getSelection(&selected, &count);
  m_outliner->deselectAllItems();

  if (selected && count > 0)
    for(int ii = 0; ii < count; ii++ )
      m_outliner->selectItem(m_totalLines - 1 - selected[ii]);
}

void
XFE_HistoryView::saveSelected()
{
  const int *selected;
  int count;
  
  m_outliner->getSelection(&selected, &count);

  if (selected && count > 0)
    {
      for(int ii = 0; ii < count; ii++ )
        {
          int itemNum = selected[ii];
          
          if( m_sortDescending )
            {
              itemNum = (m_totalLines - 1) - itemNum;
            }
          
          gh_HistEntry *record = GH_GetRecord( m_histCursor, itemNum );
          
          if( !record )
            {
              continue;
            }
          
          char *pszKey = new char[XP_STRLEN( record->address )+1];
          XP_STRCPY ( pszKey, record->address );
          XP_ListAddObject (m_selectList, pszKey);
        }
    }
}

void
XFE_HistoryView::restoreSelected()
{
  m_outliner->deselectAllItems();

  char *pszKey;
  while( (pszKey = (char *)XP_ListRemoveTopObject( m_selectList )) )
    {
      int itemNum =  GH_GetRecordNum( m_histCursor, pszKey );

      if( itemNum > -1 )
        {
          if( m_sortDescending )
            {
              itemNum = (m_totalLines - 1) - itemNum;
            }
          m_outliner->selectItem(itemNum);
        }
      delete [] pszKey;
    }
}

#if defined(USE_MOTIF_DND)

fe_icon_data *
XFE_HistoryView::GetDragIconData(int /*row*/, int /*column*/)
{
  DDD(printf("XFE_HistoryView::GetDragIconData()\n");)
  
  return &HGroup;
}

fe_icon_data *
XFE_HistoryView::getDragIconData(void *this_ptr,
								int row,
								int column)
{
  DDD(printf("XFE_HistoryView::getDragIconData()\n");)
  XFE_HistoryView *history_view = (XFE_HistoryView*)this_ptr;

  return history_view->GetDragIconData(row, column);
}

void
XFE_HistoryView::GetDragTargets(int row, int column,
								Atom **targets,
								int *num_targets)
{
  DDD(printf("XFE_HistoryView::GetDragTargets()\n");)

  int flags;

  XP_ASSERT(row > -1);
  if (row == -1)
	{
	  *targets = NULL;
	  *num_targets = 0;
	}
  else
	{
	  *num_targets = 1;
	  *targets = new Atom[ *num_targets ];
	  
	  (*targets)[0] = XFE_DragBase::_XA_NETSCAPE_URL;
	  
	  m_dragentry = GH_GetRecord(m_histCursor, row);
	}
}

void
XFE_HistoryView::getDragTargets(void *this_ptr,
								int row,
								int column,
								Atom **targets,
								int *num_targets)
{
  DDD(printf("XFE_HistoryView::getDragTargets()\n");)
  XFE_HistoryView *history_view = (XFE_HistoryView*)this_ptr;
 
  history_view->GetDragTargets(row, column, targets, num_targets);
}

char *
XFE_HistoryView::DragConvert(Atom atom)
{
  if (atom == XFE_DragBase::_XA_NETSCAPE_URL)
	{
	  char *result;

	  // translate drag data to NetscapeURL format
	  XFE_URLDesktopType urlData;
	  
	  urlData.createItemList(1);
	  urlData.url(0,m_dragentry->address);
	  result = XtNewString(urlData.getString());
	  
	  return result;
	}
  else if (atom == XA_STRING)
	{
	  return XtNewString(m_dragentry->address);
	}
}

char *
XFE_HistoryView::dragConvert(void *this_ptr,
							Atom atom)
{
  XFE_HistoryView *history_view = (XFE_HistoryView*)this_ptr;
  
  return history_view->DragConvert(atom);
}

#endif /* USE_MOTIF_DND */
