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
   RDFView.h -- view of rdf data
   Created: Stephen Lamm <slamm@netscape.com>, 5-Nov-97.
 */



#include "RDFView.h"
#include "Frame.h" // for xfe_ExecuteCommand
#include "xfe2_extern.h"
#ifdef DEBUG_slamm
#define D(x) x
#else
#define D(x)
#endif

// pixmaps for use in the bookmark window.
fe_icon XFE_RDFView::bookmark = { 0 };
fe_icon XFE_RDFView::closedFolder = { 0 };
fe_icon XFE_RDFView::openedFolder = { 0 };

XFE_RDFView::XFE_RDFView(XFE_Component *toplevel, Widget parent,
                         XFE_View *parent_view, MWContext *context,
                         HT_View htview)
  : XFE_View(toplevel, parent_view, context)
{
  // xxx Pull this info out of rdf
  int num_columns = 1;
  static int column_widths[] = {40};

  m_toplevel = getToplevel();
  m_widget = getBaseWidget();
#ifdef DEBUG_spence
  if (m_toplevel != toplevel) {
    printf ("m_toplevel != toplevel\n");
  }
#endif

  m_outliner = new XFE_Outliner("rdfList",
								this, 
								getToplevel(),
								parent,
								False, // constantSize 
								True,  // hasHeadings
								1, //num_columns, // Number of columns.
								1, //num_columns, // Number of visible columns.
								column_widths, 
								NULL /*BOOKMARK_OUTLINER_GEOMETRY_PREF*/);

  m_outliner->setHideColumnsAllowed( TRUE );
  m_outliner->setPipeColumn( 0 /*OUTLINER_COLUMN_NAME*/ );
  m_outliner->setMultiSelectAllowed( TRUE );
  setBaseWidget(m_outliner->getBaseWidget());

  {
    Pixel bg_pixel;
    
    XtVaGetValues(m_widget, XmNbackground, &bg_pixel, 0);

    if (!bookmark.pixmap)
      fe_NewMakeIcon(m_widget,
		     BlackPixelOfScreen(XtScreen(m_widget)),
		     bg_pixel,
		     &bookmark,
		     NULL,
		     BM_Bookmark.width, BM_Bookmark.height,
		     BM_Bookmark.mono_bits, BM_Bookmark.color_bits, BM_Bookmark.mask_bits, FALSE);
    if (!closedFolder.pixmap)
      fe_NewMakeIcon(m_widget,
		     BlackPixelOfScreen(XtScreen(m_widget)),
		     bg_pixel,
		     &closedFolder,
		     NULL,
		     BM_Folder.width, BM_Folder.height,
		     BM_Folder.mono_bits, BM_Folder.color_bits, BM_Folder.mask_bits, FALSE);

    if (!openedFolder.pixmap)
      fe_NewMakeIcon(m_widget,
		     BlackPixelOfScreen(XtScreen(m_widget)),
		     bg_pixel,
		     &openedFolder,
		     NULL,
		     BM_FolderO.width, BM_FolderO.height,
		     BM_FolderO.mono_bits, BM_FolderO.color_bits, BM_FolderO.mask_bits, FALSE);
  }

  setRDFView(htview);
}

XFE_RDFView::~XFE_RDFView()
{
  //xxx what to delete?
}

Boolean
XFE_RDFView::isCommandEnabled(CommandType cmd, void *calldata,
                              XFE_CommandInfo* info)
{
    {
      return XFE_View::isCommandEnabled(cmd, calldata, info);
    }
}

void
XFE_RDFView::doCommand(CommandType cmd, void *calldata, XFE_CommandInfo*info)
{
  const int *	selected;
  int			count;
  
  m_outliner->getSelection(&selected, &count);
		
    {
      XFE_View::doCommand(cmd,calldata,info);
    }
}

Boolean
XFE_RDFView::handlesCommand(CommandType cmd, void *calldata,
                            XFE_CommandInfo* info)
{
    {
      return XFE_View::handlesCommand(cmd, calldata, info);
    }
}

XP_Bool
XFE_RDFView::isCommandSelected(CommandType cmd,
                                    void *calldata, XFE_CommandInfo* info)
{
    {
      XFE_View *view = getParent();
      return (view && view->isCommandSelected(cmd, calldata, info));
    }
}

char *
XFE_RDFView::commandToString(CommandType cmd, void*, XFE_CommandInfo*)
{
    return NULL;
}


// Methods from the outlinable interface.
void *
XFE_RDFView::ConvFromIndex(int /*index*/)
{
  XP_ASSERT(0);
  return 0;
}

int
XFE_RDFView::ConvToIndex(void */*item*/)
{
  XP_ASSERT(0);
  return 0;
}

void *
XFE_RDFView::acquireLineData(int line)
{
  m_node = HT_GetNthItem(m_rdfview, line /*+ 1*/);

  if (!m_node)
    return NULL;

  m_nodeDepth = HT_GetItemIndentation(m_node);
  m_ancestorInfo = new OutlinerAncestorInfo[ m_nodeDepth + 1];

  if (m_nodeDepth)
    {
      HT_Resource tmp;
      int i;
		  
      for (tmp = m_node, i = m_nodeDepth;
           tmp != NULL;
           tmp = HT_GetParent(tmp), i --)
        {
          m_ancestorInfo[i].has_prev = HT_ItemHasBackwardSibling(tmp);
          m_ancestorInfo[i].has_next = HT_ItemHasForwardSibling(tmp);
        }
    }
  else
    {
      m_ancestorInfo[0].has_prev = HT_ItemHasBackwardSibling(m_node);
      m_ancestorInfo[0].has_next = HT_ItemHasForwardSibling(m_node);
    }
  
  return m_node;
}

void
XFE_RDFView::getTreeInfo(XP_Bool *expandable,
			      XP_Bool *is_expanded,
			      int *depth,
			      OutlinerAncestorInfo **ancestor)
{
  XP_Bool is_node_expandable = False;
  XP_Bool is_node_expanded = False;

  XP_ASSERT(m_node);

  if (!m_node)
  {
	  return;
  }

  is_node_expandable = HT_IsContainer(m_node);
  if (is_node_expandable)
    is_node_expanded = HT_IsContainerOpen(m_node);

  if (expandable)
    *expandable = is_node_expandable;

  if (is_expanded)
    *is_expanded = is_node_expanded;

  if (depth)
    *depth = m_nodeDepth;

  if (ancestor)
    *ancestor = m_ancestorInfo;
}

EOutlinerTextStyle
XFE_RDFView::getColumnStyle(int /*column*/)
{
  XP_ASSERT(m_node);

  return OUTLINER_Default;
  //return BM_IsAlias(m_node) ? OUTLINER_Italic : OUTLINER_Default;
}

char *
XFE_RDFView::getColumnText(int /*column*/)
{
  static char node_buf[2048]; // does this need to be thread safe???
  void *data;

  XP_ASSERT(m_node);

  // xxx Need error handling and column switching
  HT_NodeDisplayString(m_node, node_buf, sizeof(node_buf));
  return node_buf;
}

fe_icon *
XFE_RDFView::getColumnIcon(int column)
{
	XP_ASSERT(m_node);

    if (column != 0) {
      return NULL;
    }
    if (HT_IsContainer(m_node)) {
      if (HT_IsContainerOpen(m_node)) {
        return &openedFolder;
      } else {
        return &closedFolder;
      }
    } else {
      return &bookmark;
    }
}

char *
XFE_RDFView::getColumnName(int /*column*/)
{
  // xxx Widget name
  return "Name";
}

char *
XFE_RDFView::getColumnHeaderText(int /*column*/)
{
  // xxx Column Label
  return "Name";
}

fe_icon *
XFE_RDFView::getColumnHeaderIcon(int /*column*/)
{
  return 0;
}

EOutlinerTextStyle
XFE_RDFView::getColumnHeaderStyle(int /*column*/)
{
  return OUTLINER_Default;
}

void
XFE_RDFView::releaseLineData()
{
  delete [] m_ancestorInfo;
  m_ancestorInfo = NULL;
}

void
XFE_RDFView::Buttonfunc(const OutlineButtonFuncData *data)
{
  if (data->row == -1) 
    {
      // Click on Column header

      // data->column has the column
      return;
    }

  HT_Resource node = HT_GetNthItem(m_rdfview, data->row);

  if (!node) return;

  if (data->clicks == 1)
    {
      if (data->ctrl) 
        {
          HT_ToggleSelection(node);	
          m_outliner->toggleSelected(data->row);
        } 
      else if (data->shift) 
        {
          HT_SetSelectionRange(node, m_node);
          m_outliner->trimOrExpandSelection(data->row);
        } 
      else 
        {
          HT_SetSelection(node);
          m_outliner->selectItemExclusive(data->row);

          if (data->button == Button2) 
            {
              // Dispatch in new window (same as double click)
	      char *s = HT_GetNodeURL (node);
	      URL_Struct *url = NET_CreateURLStruct (s, NET_DONT_RELOAD);
	      url->window_target = "_rdf_target";
	      fe_reuseBrowser (m_contextData, url);
            }
        }
    }
  else if (data->clicks == 2) 
    {
      HT_SetSelection(node);
      m_outliner->selectItemExclusive(data->row);
      
      if (HT_IsContainer(node))
        {
          PRBool isOpen;

          HT_GetOpenState(node, &isOpen);
          if (isOpen)
            {
              //BM_ClearAllChildSelection(m_contextData, node, TRUE);
            }
          
          HT_SetOpenState(m_node, (PRBool)!isOpen);
        }
      else
        {
          // Dispatch in new window (same as button2 above)
	  char *s = HT_GetNodeURL (node);
	  URL_Struct *url = NET_CreateURLStruct (s, NET_DONT_RELOAD);
	  url->window_target = "_rdf_target";
	  fe_reuseBrowser (m_contextData, url);
        }
    }
  
  getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
}

void
XFE_RDFView::Flippyfunc(const OutlineFlippyFuncData *data)
{
  HT_Resource node = HT_GetNthItem(m_rdfview, data->row);
  if (node && HT_IsContainer(node)) 
    {
      PRBool isOpen;
      
      HT_GetOpenState(node, &isOpen);
      if (isOpen)
        {
          //BM_ClearAllChildSelection(m_contextData, node, TRUE);
        }
      
      HT_SetOpenState(m_node, (PRBool)!isOpen);
    }
}

XFE_Outliner *
XFE_RDFView::getOutliner()
{
  return m_outliner;
}

void
XFE_RDFView::setRDFView(HT_View htview)
{
  m_rdfview = htview;

  if (!m_rdfview) return;
  
  int itemCount =  HT_GetItemListCount(htview);

  m_outliner->change(0, itemCount,
                     (itemCount > 0 ? itemCount : 0));
}





