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
#include "Command.h"
#include "xfe2_extern.h"
#include "xpgetstr.h"
#include <Xm/Xm.h>
#include <XmL/Tree.h>
#include <Xfe/Xfe.h>

#define TREE_NAME "RdfTree"

#ifdef DEBUG_slamm
#define D(x) x
#else
#define D(x)
#endif

// pixmaps for use in the bookmark window.
fe_icon XFE_RDFView::bookmark = { 0 };
fe_icon XFE_RDFView::closedFolder = { 0 };
fe_icon XFE_RDFView::openedFolder = { 0 };

#define SECONDS_PER_DAY		86400L

extern int XP_BKMKS_HOURS_AGO;
extern int XP_BKMKS_DAYS_AGO;
extern int XP_BKMKS_LESS_THAN_ONE_HOUR_AGO;

struct RDFColumnData
{
  RDFColumnData(void) : token(NULL), token_type(0) {}
  RDFColumnData(void *t, uint32 tt) : token(t), token_type(tt) {}

  void* token;
  uint32 token_type;
};


void HTFE_MakePrettyDate(char* buffer, time_t lastVisited)
{
    buffer[0] = 0;
    time_t today = XP_TIME();
    int elapsed = today - lastVisited;

    if (elapsed < SECONDS_PER_DAY) 
    {
        int32 hours = (elapsed + 1800L) / 3600L;
        if (hours < 1) 
        {
            XP_STRCPY(buffer, XP_GetString(XP_BKMKS_LESS_THAN_ONE_HOUR_AGO));
        }
        sprintf(buffer, XP_GetString(XP_BKMKS_HOURS_AGO), hours);
    } 
    else if (elapsed < (SECONDS_PER_DAY * 31)) 
    {
        sprintf(buffer, XP_GetString(XP_BKMKS_DAYS_AGO),
                (elapsed + (SECONDS_PER_DAY / 2)) / SECONDS_PER_DAY);
    } 
    else 
    {
	  struct tm* tmp;
	  tmp = localtime(&lastVisited);

	  sprintf(buffer, asctime(tmp));
    }
}


//
//  Command Handling
//

//
//    This acts as an encapsulator for the doCommand() method.
//    Sub-classes impliment a reallyDoCommand(), and leave the
//    boring maintainence work to this class. This approach
//    saves every sub-class from calling super::doCommand(),
//    which would really be a drag, now wouldn't it.
//
class XFE_RDFViewCommand : public XFE_ViewCommand
{
public:
	XFE_RDFViewCommand(char* name) : XFE_ViewCommand(name) {};
	
	virtual void    reallyDoCommand(XFE_RDFView*, XFE_CommandInfo*) = 0;
	virtual XP_Bool requiresChromeUpdate() {
		return TRUE;
	};
	void            doCommand(XFE_View* v_view, XFE_CommandInfo* info) {
		XFE_RDFView* view = (XFE_RDFView*)v_view;
		reallyDoCommand(view, info);
		if (requiresChromeUpdate()) {
			//XXX view->updateChrome();
		}
	}; 
};

class XFE_RDFViewAlwaysEnabledCommand : public XFE_RDFViewCommand
{
public:
	XFE_RDFViewAlwaysEnabledCommand(char* name) : XFE_RDFViewCommand(name) {};

	XP_Bool isEnabled(XFE_View*, XFE_CommandInfo*) {
		return True;
	};
};

class RdfPopupCommand : public XFE_RDFViewAlwaysEnabledCommand
{
public:
    RdfPopupCommand() : XFE_RDFViewAlwaysEnabledCommand(xfeCmdShowPopup) {};

    virtual XP_Bool isSlow() {
        return FALSE;
    };

	void reallyDoCommand(XFE_RDFView* view, XFE_CommandInfo* info) {	
        view->doPopup(info->event);
    };
};

//    END OF COMMAND DEFINES

static XFE_CommandList* my_commands = 0;

XFE_RDFView::XFE_RDFView(XFE_Component *toplevel, Widget parent,
                         XFE_View *parent_view, MWContext *context,
                         XP_Bool isStandalone)
  : XFE_View(toplevel, parent_view, context)
{
  m_rdfview = NULL;
  m_popup = NULL;
  

  if (!my_commands)
      registerCommand(my_commands, new RdfPopupCommand);

  Widget tree = 
      XtVaCreateManagedWidget(TREE_NAME,
                              xmlTreeWidgetClass,
                              parent,
							  XmNshadowThickness,       0,
                              XmNhorizontalSizePolicy,  XmRESIZE_IF_POSSIBLE,
                              XmNallowColumnResize,     True,
							  XmNselectionPolicy,		XmSELECT_MULTIPLE_ROW,
                              XmNheadingRows,           1,
                              XmNvisibleRows,           14,
                              XmNhideUnhideButtons,     True,
                              XmNvisibleColumns,        (isStandalone ? 0 : 1),
                              //XmNdebugLevel, 1,
                              /* Form resources */
                              XmNtopAttachment,    XmATTACH_FORM,
                              XmNbottomAttachment, XmATTACH_FORM,
                              XmNleftAttachment,   XmATTACH_FORM,
                              XmNrightAttachment,  XmATTACH_FORM,
                              XmNtopOffset,        2,
                              XmNbottomOffset,     2,
                              XmNleftOffset,       2,
                              XmNrightOffset,      2,
                              NULL);

  setBaseWidget(tree);

  XtVaSetValues(m_widget, XmNcellAlignment, XmALIGNMENT_LEFT, NULL);
  XtVaSetValues(m_widget,
                XmNcellDefaults, True,
                XmNcellAlignment, XmALIGNMENT_LEFT,
                NULL);

  init_pixmaps();
  
  XtAddCallback(m_widget, XmNexpandCallback, expand_row_cb, this);
  XtAddCallback(m_widget, XmNcollapseCallback, collapse_row_cb, this);
  XtAddCallback(m_widget, XmNdeleteCallback, delete_cb, NULL);
  XtAddCallback(m_widget, XmNactivateCallback, activate_cb, this);
  XtAddCallback(m_widget, XmNresizeCallback, resize_cb, this);
  XtAddCallback(m_widget, XmNeditCallback, edit_cell_cb, this);
  XtAddCallback(m_widget, XmNselectCallback, select_cb, this);
  XtAddCallback(m_widget, XmNdeselectCallback, deselect_cb, this);
  //XtInsertEventHandler(m_widget, ButtonPressMask, False, button_eh, this, XtListTail);
  XtAddCallback(m_widget, XmNpopupCallback, popup_cb, this);

  //fe_AddTipStringCallback(outline, XFE_Outliner::tip_cb, this);

}

XFE_RDFView::~XFE_RDFView()
{
    if (m_popup)
        delete m_popup;
}


void
XFE_RDFView::init_pixmaps(void)
{
    Pixel bg_pixel;
    
    XtVaGetValues(m_widget, XmNbackground, &bg_pixel, 0);

    Widget  shell = XfeAncestorFindByClass(getToplevel()->getBaseWidget(),
                                           shellWidgetClass,
                                           XfeFIND_ANY);
    if (!bookmark.pixmap)
      fe_NewMakeIcon(shell,
		     BlackPixelOfScreen(XtScreen(m_widget)),
		     bg_pixel,
		     &bookmark,
		     NULL,
		     BM_Bookmark.width, BM_Bookmark.height,
		     BM_Bookmark.mono_bits, BM_Bookmark.color_bits,
             BM_Bookmark.mask_bits, FALSE);
    if (!closedFolder.pixmap)
      fe_NewMakeIcon(shell,
		     BlackPixelOfScreen(XtScreen(m_widget)),
		     bg_pixel,
		     &closedFolder,
		     NULL,
		     BM_Folder.width, BM_Folder.height,
		     BM_Folder.mono_bits, BM_Folder.color_bits,
             BM_Folder.mask_bits, FALSE);

    if (!openedFolder.pixmap)
      fe_NewMakeIcon(shell,
		     BlackPixelOfScreen(XtScreen(m_widget)),
		     bg_pixel,
		     &openedFolder,
		     NULL,
		     BM_FolderO.width, BM_FolderO.height,
		     BM_FolderO.mono_bits, BM_FolderO.color_bits,
             BM_FolderO.mask_bits, FALSE);
}

void
XFE_RDFView::activate_row(int row)
{
  HT_Resource node = HT_GetNthItem(m_rdfview, row);

  if (!node) return;

#if 0
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

        }
  getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
#endif /*0*/
                
  if (!HT_IsContainer(node)) {
      // Dispatch in new window
      char *s = HT_GetNodeURL (node);
      URL_Struct *url = NET_CreateURLStruct (s, NET_DONT_RELOAD);
      //url->window_target = XP_STRDUP("_rdf_target");
      fe_reuseBrowser (m_contextData, url);
  }
}

void
XFE_RDFView::resize_cb(Widget,
                       XtPointer clientData,
                       XtPointer callData)
{
	XFE_RDFView *obj = (XFE_RDFView*)clientData;

	obj->resize(callData);
}

void
XFE_RDFView::resize(XtPointer callData)
{
	XmLGridCallbackStruct *cbs = (XmLGridCallbackStruct*)callData;

	XP_ASSERT(m_widget);
	if(!m_widget)
		return;

	if (cbs->reason == XmCR_RESIZE_COLUMN)
    {
        D( printf("Inside XFE_RDFView::resize(COLUMN, %d)\n", cbs->column);); 
    }
}


void
XFE_RDFView::refresh(HT_Resource node)
{
  if (!m_rdfview) return;

  XP_ASSERT(HT_IsContainer(node));
  if (!HT_IsContainer(node)) return;

  if (HT_IsContainerOpen(node))
  {
      HT_Resource child;
    
      HT_Cursor child_cursor = HT_NewCursor(node);
      while (child = HT_GetNextItem(child_cursor))
      {
          add_row(child);

          if (HT_IsContainer(child) && HT_IsContainerOpen(child))
              refresh(child);
      }
      HT_DeleteCursor(child_cursor);
  }
  else
  {
      int row = HT_GetNodeIndex(m_rdfview, node);

      XmLTreeDeleteChildren(m_widget, row);
  }
}

void
XFE_RDFView::edit_cell_cb(Widget,
                       XtPointer clientData,
                       XtPointer callData)
{
	XFE_RDFView *obj = (XFE_RDFView*)clientData;

	obj->edit_cell(callData);
}

void
XFE_RDFView::edit_cell(XtPointer callData)
{
	XP_ASSERT(m_widget);
	if(!m_widget)
		return;

	XmLGridCallbackStruct *cbs = (XmLGridCallbackStruct*)callData;
    HT_Resource node = HT_GetNthItem (m_rdfview, cbs->row);

	if (node && cbs->reason == XmCR_EDIT_COMPLETE)
    {
        XmLGridColumn column = XmLGridGetColumn(m_widget, XmCONTENT,
                                                cbs->column);
        RDFColumnData *column_data = NULL;

        XtVaGetValues(m_widget, 
                      XmNcolumnPtr, column,
                      XmNcolumnUserData, &column_data,
                      NULL);

        XmLGridRow row = XmLGridGetRow(m_widget, XmCONTENT, cbs->row);
        XmString cell_string;

        XtVaGetValues(m_widget,
                      XmNcolumnPtr, column,
                      XmNrowPtr, row,
                      XmNcellString, &cell_string,
                      NULL);
        char *text;
        XmStringGetLtoR(cell_string, XmSTRING_DEFAULT_CHARSET, &text);
        HT_SetNodeData (node, column_data->token, column_data->token_type,
                        text);
    }
}

void
XFE_RDFView::select_cb(Widget,
                       XtPointer clientData,
                       XtPointer callData)
{
	XFE_RDFView *obj = (XFE_RDFView*)clientData;
    XmLGridCallbackStruct *cbs = (XmLGridCallbackStruct *)callData;

    D(fprintf(stderr,"select_cb(%d)\n",cbs->row););

	obj->select_row(cbs->row);
}

void
XFE_RDFView::select_row(int row)
{
  HT_Resource node = HT_GetNthItem(m_rdfview, row);

  if (!node) return;

  HT_SetSelection(node);
}

void
XFE_RDFView::deselect_cb(Widget,
                         XtPointer clientData,
                         XtPointer callData)
{
	XFE_RDFView *obj = (XFE_RDFView*)clientData;
    XmLGridCallbackStruct *cbs = (XmLGridCallbackStruct *)callData;

    D(fprintf(stderr,"deselect_cb(%d)\n",cbs->row););

	obj->deselect_row(cbs->row);
}

void
XFE_RDFView::deselect_row(int row)
{
  HT_Resource node = HT_GetNthItem(m_rdfview, row);

  if (!node) return;

  HT_SetSelectedState(node,False);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFView::notify(HT_Notification ns, HT_Resource n, 
                    HT_Event whatHappened)
{
  switch (whatHappened) {
  case HT_EVENT_NODE_ADDED:
    D(printf("RDFView::HT_Event: %s on %s\n","HT_EVENT_NODE_ADDED",
             HT_GetNodeName(n)););
    add_row(n);
    break;
  case HT_EVENT_NODE_DELETED_DATA:
    D(printf("RDFView::HT_Event: %s on %s\n","HT_EVENT_NODE_DELETED_DATA",
             HT_GetNodeName(n)););
    break;
  case HT_EVENT_NODE_DELETED_NODATA:
    D(printf("RDFView::HT_Event: %s on %s\n","HT_EVENT_NODE_DELETED_NODATA",
             HT_GetNodeName(n)););
    break;
  case HT_EVENT_NODE_VPROP_CHANGED:
    D(printf("RDFView::HT_Event: %s on %s\n","HT_EVENT_NODE_VPROP_CHANGED",
             HT_GetNodeName(n)););
    break;
  case HT_EVENT_NODE_SELECTION_CHANGED:
    D(printf("RDFView::HT_Event: %s on %s\n","HT_EVENT_NODE_SELECTION_CHANGED",
             HT_GetNodeName(n)););
    break;
  case HT_EVENT_NODE_OPENCLOSE_CHANGED: 
    {
      D(printf("RDFView::HT_Event: %s on %s\n","HT_EVENT_NODE_OPENCLOSE_CHANGED",
               HT_GetNodeName(n)););
      
      refresh(n);

      break;
    }
  case HT_EVENT_VIEW_CLOSED:
    D(printf("RDFView::HT_Event: %s on %s\n","HT_EVENT_VIEW_CLOSED",
             HT_GetNodeName(n)););
    break;
  case HT_EVENT_VIEW_SELECTED:
    {
      D(printf("RDFView::HT_Event: %s on %s\n","HT_EVENT_VIEW_SELECTED",
               HT_GetNodeName(n)););
      
      HT_View view = HT_GetView(n);
      
      if (m_rdfview != view)
        setRDFView(view);
    }
    break;
  case HT_EVENT_NODE_OPENCLOSE_CHANGING:
    D(printf("RDFView::HT_Event: %s on %s\n","HT_EVENT_NODE_OPENCLOSE_CHANGING",
             HT_GetNodeName(n)););
    break;
  default:
    D(printf("RDFView::HT_Event: Unknown type on %s\n",HT_GetNodeName(n)););
    break;
  }
}
//////////////////////////////////////////////////////////////////////
void
XFE_RDFView::setRDFView(HT_View htview)
{
  if (htview == m_rdfview) return;

  m_rdfview = htview;

  if (!m_rdfview) return;

  fill_tree();
}
//////////////////////////////////////////////////////////////////////
void
XFE_RDFView::fill_tree()
{
  XP_ASSERT(m_widget);
  if (!m_rdfview || !m_widget)
      return;
  
  int item_count =  HT_GetItemListCount(m_rdfview);

  XtVaSetValues(m_widget,
                XmNlayoutFrozen, True,
                XmNcolumns, 1,
                NULL);

  XmLGridDeleteAllRows(m_widget, XmCONTENT);

  // Set default values for column headings
  //  (Should make so that the grid widget has separate defaults
  //     for headings and content cells)
  XtVaSetValues(m_widget,
                XmNcellDefaults, True,
                XmNcellLeftBorderType, XmBORDER_LINE,
                XmNcellRightBorderType, XmBORDER_LINE,
                XmNcellTopBorderType, XmBORDER_LINE,
                XmNcellBottomBorderType, XmBORDER_LINE,
                NULL);
  
  // add columns
  char *column_name;
  uint32 column_width;
  void *token;
  uint32 token_type;
  int ii = 0;
  HT_Cursor column_cursor = HT_NewColumnCursor (m_rdfview);
  while (HT_GetNextColumn(column_cursor, &column_name, &column_width,
                          &token, &token_type)) {
    add_column(ii, column_name, column_width, token, token_type);
    ii++;
  }
  HT_DeleteColumnCursor(column_cursor);

  // Set default values for new content cells
  XtVaSetValues(m_widget,
                XmNcellDefaults, True,
                XmNcellMarginLeft, 1,
                XmNcellMarginRight, 1,
                XmNcellLeftBorderType, XmBORDER_NONE,
                XmNcellRightBorderType, XmBORDER_NONE,
                XmNcellTopBorderType, XmBORDER_NONE,
                XmNcellBottomBorderType, XmBORDER_NONE,
                NULL);
  

  // add rows
  for (ii=0; ii < item_count; ii++) {
    add_row(ii);
  }

  XtVaSetValues(m_widget,
                XmNlayoutFrozen, False,
                NULL);
}

void
XFE_RDFView::destroy_tree()
{
}

void
XFE_RDFView::add_row(int row)
{
    HT_Resource node = HT_GetNthItem (m_rdfview, row);
    add_row(node);
}

void
XFE_RDFView::add_row(HT_Resource node)
{
  //HT_Resource node = GetNthItem (m_rdfview, row);
    int row = HT_GetNodeIndex(m_rdfview, node);
    char *name = HT_GetNodeName(node);
    int  depth = HT_GetItemIndentation(node);
    Boolean expands =    HT_IsContainer(node);
    Boolean isExpanded = HT_IsContainerOpen(node);

    /*D( fprintf(stderr,"XFE_RDFView::add_row(0x%x %d) name(%s) depth(%d)\n",
             node,row, name, depth);)*/

    Pixmap pixmap, mask;

	//pixmap = XmUNSPECIFIED_PIXMAP;
	//pixmask = XmUNSPECIFIED_PIXMAP;
    if (expands && isExpanded) {
      pixmap = openedFolder.pixmap;
      mask   = openedFolder.mask;
    } else if (expands && !isExpanded) {
      pixmap = closedFolder.pixmap;
      mask   = closedFolder.mask;
    } else {
      pixmap = bookmark.pixmap;
      mask   = bookmark.mask;
    }

  	XmString xmstr = XmStringCreateSimple(name);

    XmLTreeAddRow(m_widget,
				  depth,
				  expands,
				  isExpanded,
				  row,
				  pixmap,
				  mask,
				  xmstr);

	XmStringFree(xmstr);

    int column_count;
    // Should only need to do this for visible columns
    XtVaGetValues(m_widget, XmNcolumns, &column_count, NULL);
    RDFColumnData *column_data;
    void *data;
    for (int ii = 1; ii < column_count; ii++) 
    {
        XmLGridColumn column = XmLGridGetColumn(m_widget, XmCONTENT, ii);
        XtVaGetValues(m_widget, 
                      XmNcolumnPtr, column,
                      XmNcolumnUserData, &column_data,
                      NULL);

        Boolean is_editable = HT_IsNodeDataEditable(node,
                                                    column_data->token,
                                                    column_data->token_type);
        XtVaSetValues(m_widget,
                      XmNrow,          row,
                      XmNcolumn,       ii,
                      XmNcellEditable, is_editable,
                      NULL);

        if (HT_GetNodeData (node, column_data->token,
                            column_data->token_type, &data)
            && data) 
       {
            time_t dateVal;
            struct tm* time;
            char buffer[200];
            
            switch (column_data->token_type)
            {
			case HT_COLUMN_DATE_STRING:
				if ((dateVal = (time_t)atol((char *)data)) == 0) break;
				if ((time = localtime(&dateVal)) == NULL) break;
				HTFE_MakePrettyDate(buffer, dateVal);
				break;
			case HT_COLUMN_DATE_INT:
				if ((time = localtime((time_t *) &data)) == NULL) break;
				HTFE_MakePrettyDate(buffer, (time_t)data);
				break;
			case HT_COLUMN_INT:
				sprintf(buffer,"%d",(int)data);
				break;
			case HT_COLUMN_STRING:
				strcpy(buffer, (char*)data);
				break;
            }
            /*D(fprintf(stderr,"Node data (%d, %d) = '%s'\n",row,ii,buffer););*/
            XmLGridSetStringsPos(m_widget, 
                                 XmCONTENT, row,
                                 XmCONTENT, ii, 
                                 buffer);
        }
        else
        {
            /*D(fprintf(stderr,"No column data r(%d) c(%d) type(%d)\n",row,ii,
                      column_data->token_type););*/
        }
    }
}

void
XFE_RDFView::delete_row(int row)
{
  XmLGridDeleteRows(m_widget, XmCONTENT, row, 1);
}

void
XFE_RDFView::add_column(int index, char *name, uint32 width,
                        void *token, uint32 token_type)
{
  D( fprintf(stderr,"XFE_RDFView::add_column index(%d) name(%s) width(%d)\n",
             index, name,width););

  if (index > 0) {
      XmLGridAddColumns(m_widget, XmCONTENT, index, 1);
  }

  RDFColumnData *column_data = new RDFColumnData(token, token_type);
  XtVaSetValues(m_widget,
                XmNcolumn, index,
                XmNcolumnSizePolicy, XmCONSTANT,
                XmNcolumnWidth, width,
                XmNcolumnUserData, column_data,
                NULL);

  XmLGridSetStringsPos(m_widget, 
                       XmHEADING, 0,
                       XmCONTENT, index, 
                       name);
}

void
XFE_RDFView::delete_column(HT_Resource cursor)
{

}

void 
XFE_RDFView::expand_row_cb(Widget,
                           XtPointer clientData,
                           XtPointer callData)
{
	XFE_RDFView *obj = (XFE_RDFView*)clientData;
    XmLGridCallbackStruct *cbs = (XmLGridCallbackStruct *)callData;

    D(fprintf(stderr,"expand_row_cb(%d)\n",cbs->row););

	obj->expand_row(cbs->row);
}

void
XFE_RDFView::expand_row(int row)
{
    HT_Resource node = HT_GetNthItem (m_rdfview, row);
     
    HT_SetOpenState (node, (PRBool)TRUE);
}

void 
XFE_RDFView::collapse_row_cb(Widget,
                             XtPointer clientData,
                             XtPointer callData)
{
	XFE_RDFView *obj = (XFE_RDFView*)clientData;
    XmLGridCallbackStruct *cbs = (XmLGridCallbackStruct *)callData;
 
    D(fprintf(stderr,"collapse_row_cb(%d)\n",cbs->row););

	obj->collapse_row(cbs->row);
}

void
XFE_RDFView::collapse_row(int row)
{
    HT_Resource node = HT_GetNthItem (m_rdfview, row);
     
    HT_SetOpenState (node, (PRBool)FALSE);
}

void 
XFE_RDFView::delete_cb(Widget w,
                       XtPointer /*clientData*/,
                       XtPointer callData)
{
    XmLGridCallbackStruct *cbs = (XmLGridCallbackStruct *)callData;
 
	XmLGridColumn column;
    RDFColumnData *column_data = NULL;

	cbs = (XmLGridCallbackStruct *)callData;

 	if (cbs->reason != XmCR_DELETE_COLUMN)
		return;

	column = XmLGridGetColumn(w, XmCONTENT, cbs->column);

	XtVaGetValues(w,
		XmNcolumnPtr, column,
		XmNcolumnUserData, &column_data,
		NULL);

    delete column_data;
}

void 
XFE_RDFView::activate_cb(Widget,
                         XtPointer clientData,
                         XtPointer callData)
{
	XFE_RDFView *obj = (XFE_RDFView*)clientData;
    XmLGridCallbackStruct *cbs = (XmLGridCallbackStruct *)callData;
 
    if (cbs->rowType != XmCONTENT)
        return;

	obj->activate_row(cbs->row);
}

//
// Popup menu stuff
//
void 
XFE_RDFView::popup_cb(Widget,
                      XtPointer clientData,
                      XtPointer callData)
{
	XFE_RDFView *obj = (XFE_RDFView*)clientData;
    XmLGridCallbackStruct *cbs = (XmLGridCallbackStruct *)callData;
 
    if (cbs->rowType != XmCONTENT)
        return;

	obj->doPopup(cbs->event);
}

//////////////////////////////////////////////////////////////////////////
void
XFE_RDFView::doPopup(XEvent * event)
{
	if (m_popup)
	{
        delete m_popup; //destroy the old one first
    }
    m_popup = new XFE_RDFPopupMenu("popup",
                                   //getFrame(),
                                   FE_GetToplevelWidget(),
                                   m_rdfview,
                                   FALSE, // not isWorkspace
                                   FALSE); // no background commands for now
		
	m_popup->position(event);
	m_popup->show();
}


//////////////////////////////////////////////////////////////////////////

XFE_RDFPopupMenu::XFE_RDFPopupMenu(String name, Widget parent,
                                   HT_View view, 
                                   Boolean isWorkspace, Boolean isBackground)
    : XFE_SimplePopupMenu(name, parent)
{
    m_pane = HT_GetPane(view);

    HT_Cursor cursor = HT_NewContextualMenuCursor(view, isWorkspace, isBackground);
    HT_MenuCmd command;
    while(HT_NextContextMenuItem(cursor, &command))
    {
        if (command == HT_CMD_SEPARATOR)
            addSeparator();
        else
            addPushButton(HT_GetMenuCmdName(command), (XtPointer)command,
                          HT_IsMenuCmdEnabled(m_pane, command));
    }
}

void
XFE_RDFPopupMenu::PushButtonActivate(Widget w, XtPointer userData)
{
    HT_DoMenuCmd(m_pane, (HT_MenuCmd)userData);
}
