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

/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		RDFTreeView.cpp       									*/
/* Description:	XFE_RDFTreeView component header file - A class to     	*/
/*              encapsulate and rdf tree widget.					    */
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#include "RDFTreeView.h"

#include "Command.h"
#include "PopupMenu.h"
#include "RDFImage.h"
#include "RDFUtils.h"

#include "felocale.h"		// fe_ConvertToXmString()
#include "xfe2_extern.h"
#include "xpgetstr.h"



#include <XmL/Tree.h>
#include <Xfe/Xfe.h>

#define TREE_NAME "RdfTree"

extern "C" RDF_NCVocab  gNavCenter;

#if defined(DEBUG_slamm)||defined(DEBUG_mcafee)
#define D(x) x
#else
#define D(x)
#endif

// pixmaps for use in the bookmark window.
fe_icon XFE_RDFTreeView::bookmark = { 0 };
fe_icon XFE_RDFTreeView::closedFolder = { 0 };
fe_icon XFE_RDFTreeView::openedFolder = { 0 };

#define SECONDS_PER_DAY		86400L

extern int XP_BKMKS_HOURS_AGO;
extern int XP_BKMKS_DAYS_AGO;
extern int XP_BKMKS_LESS_THAN_ONE_HOUR_AGO;

extern "C"
{
   extern void treeview_bg_image_cb(XtPointer clientData);
};


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
class XFE_RDFTreeViewCommand : public XFE_ViewCommand
{
public:
	XFE_RDFTreeViewCommand(char* name) : XFE_ViewCommand(name) {};
	
	virtual void    reallyDoCommand(XFE_RDFTreeView*, XFE_CommandInfo*) = 0;
	virtual XP_Bool requiresChromeUpdate() {
		return TRUE;
	};
	void            doCommand(XFE_View* v_view, XFE_CommandInfo* info) {
		XFE_RDFTreeView* view = (XFE_RDFTreeView*)v_view;
		reallyDoCommand(view, info);
		if (requiresChromeUpdate()) {
			//XXX view->updateChrome();
		}
	}; 
};

class XFE_RDFTreeViewAlwaysEnabledCommand : public XFE_RDFTreeViewCommand
{
public:
	XFE_RDFTreeViewAlwaysEnabledCommand(char* name) : XFE_RDFTreeViewCommand(name) {};

	XP_Bool isEnabled(XFE_View*, XFE_CommandInfo*) {
		return True;
	};
};

class RdfPopupCommand : public XFE_RDFTreeViewAlwaysEnabledCommand
{
public:
    RdfPopupCommand() : XFE_RDFTreeViewAlwaysEnabledCommand(xfeCmdShowPopup) {};

    virtual XP_Bool isSlow() {
        return FALSE;
    };

	void reallyDoCommand(XFE_RDFTreeView* view, XFE_CommandInfo* info) {	
        view->doPopup(info->event);
    };
};

//  End of Commands
/////////////////////////////////////////////////////////////////////
//  Start of XFE_RDFTreeView definitions

static XFE_CommandList* my_commands = 0;

XFE_RDFTreeView::XFE_RDFTreeView(XFE_Component *	toplevel, 
								 Widget				parent,
								 XFE_View *			parent_view, 
								 MWContext *		context) :
	XFE_View(toplevel, parent_view, context),
    XFE_RDFBase(),
	_popup(NULL),
	_isStandAlone(False)
{
	if (!my_commands)
	{
		registerCommand(my_commands, new RdfPopupCommand);
	}
	
	// Main form
	Widget rdfMainForm = XtVaCreateWidget("rdfMainForm",
									   xmFormWidgetClass,
									   parent,
									   XmNshadowThickness,		0,
									   NULL);

	setBaseWidget(rdfMainForm);

//     createTree();

//     doAttachments();
	
	//fe_AddTipStringCallback(outline, XFE_Outliner::tip_cb, this);
}
//////////////////////////////////////////////////////////////////////////
XFE_RDFTreeView::~XFE_RDFTreeView()
{
    if (_popup)
	{
        delete _popup;
	}
    // Remove yourself from the RDFImage's listener list
    XFE_RDFImage::removeListener(this);
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFTreeView::createTree()
{
	// Create the tree widget
	_tree = 
		XtVaCreateWidget(TREE_NAME,
						 xmlTreeWidgetClass,
						 getTreeParent(),
						 XmNshadowThickness,		0,
						 XmNhorizontalSizePolicy,	XmRESIZE_IF_POSSIBLE,
						 XmNallowColumnResize,		True,
						 XmNselectionPolicy,		XmSELECT_MULTIPLE_ROW,
						 XmNheadingRows,			1,
						 XmNvisibleRows,			14,
						 XmNhideUnhideButtons,		True,
                         XmNminColumnWidth,         6,
						 NULL);
	
	XtVaSetValues(_tree, XmNcellAlignment, XmALIGNMENT_LEFT, NULL);
	
	XtVaSetValues(_tree,
				  XmNcellDefaults, True,
				  XmNcellAlignment, XmALIGNMENT_LEFT,
				  NULL);

	init_pixmaps();
	
	XtAddCallback(_tree, XmNexpandCallback, expand_row_cb, this);
	XtAddCallback(_tree, XmNcollapseCallback, collapse_row_cb, this);
	XtAddCallback(_tree, XmNdeleteCallback, delete_cb, this);
	XtAddCallback(_tree, XmNactivateCallback, activate_cb, this);
	XtAddCallback(_tree, XmNresizeCallback, resize_cb, this);
	XtAddCallback(_tree, XmNeditCallback, edit_cell_cb, this);
	XtAddCallback(_tree, XmNselectCallback, select_cb, this);
	XtAddCallback(_tree, XmNdeselectCallback, deselect_cb, this);
	XtAddCallback(_tree, XmNpopupCallback, popup_cb, this);
	
    XtManageChild(_tree);
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ Widget
XFE_RDFTreeView::getTreeParent() 
{
	return getBaseWidget();
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFTreeView::doAttachments()
{
	if (XfeIsAlive(_tree))
	{
		XtVaSetValues(_tree,
					  XmNtopAttachment,		XmATTACH_FORM,
					  XmNrightAttachment,	XmATTACH_FORM,
					  XmNleftAttachment,	XmATTACH_FORM,
					  XmNbottomAttachment,	XmATTACH_FORM,
					  NULL);
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFTreeView::setColumnData(int column, void * token, uint32 token_type)
{
    XFE_ColumnData *column_data = getColumnData(column);
    if (column_data) {
        column_data->token        = token;
        column_data->token_type   = token_type;
    } else {
        column_data = new XFE_ColumnData(token, token_type);
    }
    XtVaSetValues(_tree,
                  XmNcolumn, column,
                  XmNcolumnUserData, column_data,
                  NULL);
}
XFE_ColumnData *
XFE_RDFTreeView::getColumnData(int column)
{
    XFE_ColumnData *  column_data;
    XmLGridColumn colp = XmLGridGetColumn(_tree, XmCONTENT, column);
    XtVaGetValues(_tree, 
                  XmNcolumnPtr, colp,
                  XmNcolumnUserData, &column_data,
                  NULL);
    return column_data;
}
/////////////////////////////////////////////////////////////////////
void
XFE_RDFTreeView::init_pixmaps(void)
{
	XP_ASSERT( XfeIsAlive(_tree) );

    Pixel bg_pixel;
    
    XtVaGetValues(_tree, XmNbackground, &bg_pixel, 0);

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
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFTreeView::activate_row(int row)
{
  HT_Resource node = HT_GetNthItem(_ht_view, row);

  XFE_RDFUtils::launchEntry(m_contextData, node);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFTreeView::resize_cb(Widget,
                       XtPointer clientData,
                       XtPointer callData)
{
	XFE_RDFTreeView *obj = (XFE_RDFTreeView*)clientData;

	obj->resize(callData);
}

void
XFE_RDFTreeView::resize(XtPointer callData)
{
	XmLGridCallbackStruct *cbs = (XmLGridCallbackStruct*)callData;

	XP_ASSERT(_tree);
	if(!_tree)
		return;

	if (cbs->reason == XmCR_RESIZE_COLUMN)
    {
        D(printf("Inside XFE_RDFTreeView::resize(COLUMN, %d)\n", cbs->column);); 
        XmLGridColumn column = XmLGridGetColumn(_tree, XmCONTENT, cbs->column);

        XFE_ColumnData *column_data = getColumnData(cbs->column);

        int width = 0;
        XtVaGetValues(_tree,
                      XmNcolumnPtr, column,
                      XmNcolumnWidth, &width,
                      NULL);

        HT_SetColumnWidth(_ht_view, column_data->token, column_data->token_type,
                          width);
    }
}


void
XFE_RDFTreeView::refresh(HT_Resource node)
{
  if (!_ht_view) return;

  XP_ASSERT(HT_IsContainer(node));
  if (!HT_IsContainer(node)) return;

  if (HT_IsContainerOpen(node))
  {
      HT_Resource child;
    
      HT_Cursor child_cursor = HT_NewCursor(node);

      while ( (child = HT_GetNextItem(child_cursor)) )
      {
          add_row(child);

          if (HT_IsContainer(child) && HT_IsContainerOpen(child))
              refresh(child);
      }
      HT_DeleteCursor(child_cursor);
  }
  else
  {
      int row = HT_GetNodeIndex(_ht_view, node);

      XmLTreeDeleteChildren(_tree, row);
  }
}

void
XFE_RDFTreeView::edit_cell_cb(Widget,
                       XtPointer clientData,
                       XtPointer callData)
{
	XFE_RDFTreeView *obj = (XFE_RDFTreeView*)clientData;

	obj->edit_cell(callData);
}

void
XFE_RDFTreeView::edit_cell(XtPointer callData)
{
	XP_ASSERT(_tree);
	if(!_tree)
		return;

	XmLGridCallbackStruct *cbs = (XmLGridCallbackStruct*)callData;
    HT_Resource node = HT_GetNthItem (_ht_view, cbs->row);

	if (node && cbs->reason == XmCR_EDIT_COMPLETE)
    {
        XmLGridColumn column = XmLGridGetColumn(_tree, XmCONTENT,
                                                cbs->column);

        XFE_ColumnData *column_data = getColumnData(cbs->column);

        XmLGridRow row = XmLGridGetRow(_tree, XmCONTENT, cbs->row);
        XmString cell_string;

        XtVaGetValues(_tree,
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
XFE_RDFTreeView::select_cb(Widget,
                       XtPointer clientData,
                       XtPointer callData)
{
	XFE_RDFTreeView *obj = (XFE_RDFTreeView*)clientData;
    XmLGridCallbackStruct *cbs = (XmLGridCallbackStruct *)callData;

    if (cbs->rowType == XmHEADING) {
        obj->sort_column(cbs->column);
    } else {
        obj->select_row(cbs->row);
    }
}

void
XFE_RDFTreeView::sort_column(int column)
{
    int              old_sort_column;
    unsigned char    old_sort_type;

    XmLGridGetSort(_tree, &old_sort_column, &old_sort_type);

    unsigned char    sort_type = XmSORT_ASCENDING;

    if (old_sort_column == column) {
        if (old_sort_type == XmSORT_ASCENDING)
            sort_type = XmSORT_DESCENDING;
        else if (old_sort_type == XmSORT_DESCENDING)
            sort_type = XmSORT_NONE;
    }

    XFE_ColumnData *  column_data = getColumnData(column);

    if (sort_type == XmSORT_NONE) {
        HT_SetSortColumn(_ht_view, NULL, NULL, PR_FALSE);
    } else {
        XP_ASSERT(column_data);
        if (!column_data) return;

        HT_SetSortColumn(_ht_view, 
                         column_data->token,
                         column_data->token_type, 
                         sort_type == XmSORT_DESCENDING ? PR_TRUE : PR_FALSE);
    }

    XmLGridSetSort(_tree, column, sort_type);
}

void
XFE_RDFTreeView::select_row(int row)
{
  HT_Resource node = HT_GetNthItem(_ht_view, row);

  if (!node) return;

  HT_SetSelection(node);
}

void
XFE_RDFTreeView::deselect_cb(Widget,
                         XtPointer clientData,
                         XtPointer callData)
{
	XFE_RDFTreeView *obj = (XFE_RDFTreeView*)clientData;
    XmLGridCallbackStruct *cbs = (XmLGridCallbackStruct *)callData;

    D(fprintf(stderr,"deselect_cb(%d)\n",cbs->row););

	obj->deselect_row(cbs->row);
}

void
XFE_RDFTreeView::deselect_row(int row)
{
  HT_Resource node = HT_GetNthItem(_ht_view, row);

  if (!node) return;

  HT_SetSelectedState(node,False);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFTreeView::notify(HT_Resource n, HT_Event whatHappened)
{
  D(debugEvent(n, whatHappened,"RTV"););
  switch (whatHappened) {
  case HT_EVENT_NODE_ADDED:
    add_row(n);
    break;
  case HT_EVENT_NODE_DELETED_NODATA:
    {
#ifdef UNDEF
       // Is this a container node?
       Boolean expands =    HT_IsContainer(n);
       PRBool isExpanded = False;

       // Is the node expanded?
       if (expands) {
          HT_GetOpenState(n, &isExpanded);   
       }
   
       int row = HT_GetNodeIndex(_ht_view, n);    
       delete_row(row);
#endif  /* UNDEF   */
    break;
    }
  case HT_EVENT_NODE_OPENCLOSE_CHANGED: 
    {
        refresh(n); 

      Boolean expands =    HT_IsContainer(n);

      if (expands)
      {
         PRBool isExpanded = False;

         HT_GetOpenState(n, &isExpanded);          
         int row = HT_GetNodeIndex(_ht_view, n);    

         if (isExpanded)   // The node has been opened
         {
            // Expand the row
            XtVaSetValues(_tree, XmNrow, row, 
                          XmNrowIsExpanded, True, NULL);
         }
         else
         {
            // collapse the row
           XtVaSetValues(_tree, XmNrow, row,
                         XmNrowIsExpanded, False, NULL);           
         }
      }
      break;
    }
  case HT_EVENT_VIEW_SELECTED:
	{
        setHTView(HT_GetView(n));
        break;
    }
  case HT_EVENT_NODE_EDIT:
    {
        int row = HT_GetNodeIndex(_ht_view, n);
        XmLGridEditBegin(_tree, True, row, 0);
        break;
    }
  case HT_EVENT_VIEW_REFRESH:
  case HT_EVENT_VIEW_SORTING_CHANGED:
    {
       int row = HT_GetNodeIndex(_ht_view, n);
       PRBool expands = HT_IsContainer(n);
       PRBool isExpanded = False;
       if (expands) 
          HT_GetOpenState(n, &isExpanded);
       if (expands && isExpanded)
       {
         if (n == HT_TopNode(_ht_view))
           /* It is the top most node. Delete all rows */
           XmLGridDeleteAllRows(_tree, XmCONTENT);
         else
           XmLTreeDeleteChildren(_tree, row);
       }

        refresh(n);

        break;
    }
  default:
      // Fall through and let RDFBase handle the event
    break;
  }
  XFE_RDFBase::notify(n,whatHappened);
}
//////////////////////////////////////////////////////////////////////
void
XFE_RDFTreeView::updateRoot()
{
	fill_tree();
    setHTTreeViewProperties(_ht_view);
}
//////////////////////////////////////////////////////////////////////
void
XFE_RDFTreeView::fill_tree()
{
  XP_ASSERT(_tree);
  if (!_ht_view || !_tree)
      return;
  
  int item_count =  HT_GetItemListCount(_ht_view);
  //void * data=NULL;

  XtVaSetValues(_tree,
                XmNlayoutFrozen, True,
                XmNcolumns, 1,
                NULL);

  XmLGridDeleteAllRows(_tree, XmCONTENT);

  // Set default values for column headings
  //  (Should make so that the grid widget has separate defaults
  //     for headings and content cells)
  XtVaSetValues(_tree,
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
  HT_Cursor column_cursor = HT_NewColumnCursor (_ht_view);
  while (HT_GetNextColumn(column_cursor, &column_name, &column_width,
                          &token, &token_type)) {
    add_column(ii, column_name, column_width, token, token_type);
    ii++;
  }
  HT_DeleteColumnCursor(column_cursor);

  // Set default values for new content cells
  XtVaSetValues(_tree,
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

  XtVaSetValues(_tree,
                XmNlayoutFrozen, False,
                NULL);
}

void
XFE_RDFTreeView::destroy_tree()
{
}

void
XFE_RDFTreeView::add_row(int row)
{
    HT_Resource node = HT_GetNthItem (_ht_view, row);
    add_row(node);
}

void
XFE_RDFTreeView::add_row(HT_Resource node)
{
    int row = HT_GetNodeIndex(_ht_view, node);
    char *name = HT_GetNodeName(node);
    int  depth = HT_GetItemIndentation(node);
    Boolean expands =    HT_IsContainer(node);
    Boolean isExpanded = HT_IsContainerOpen(node);

    Pixmap pixmap, mask;

#if DEBUG_mcafeexxx
    // Using this for debugging. -mcafee
    static PRBool firstRow = PR_TRUE;

    if(firstRow) {
      printf("First row\n");
      firstRow = PR_FALSE;
    } else {
      printf("Not first row\n");
    }
#endif

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

    XmLTreeAddRow(_tree, depth, expands, isExpanded, row, pixmap, mask, NULL);

    int column_count;
    XtVaGetValues(_tree, XmNcolumns, &column_count, NULL);
    for (int ii = 0; ii < column_count; ii++) 
    {
        initCell(node, row, ii);
    }
}

// Format the label of a cell
// Also set whether it is editable.
void
XFE_RDFTreeView::initCell(HT_Resource node, int row, int column)
{
    XFE_ColumnData *column_data = getColumnData(column);

    XP_ASSERT(column_data);
    if (!column_data) return;

    // Set editing behavior
    //
    Boolean is_editable = False;
    if (isStandAlone())
    {
        is_editable = HT_IsNodeDataEditable(node, column_data->token,
                                            column_data->token_type);
    }
        
    // Set the label.
    //
    XmString xmstr = NULL;
    int16 charset = INTL_DefaultWinCharSetID(m_contextData);

    if (column == 0)
    {
        xmstr = XFE_RDFUtils::formatItem(node);
    }
    else
    {
        void *data;
        if (HT_GetNodeData(node, column_data->token, column_data->token_type,
                           &data) && data) 
        {
            char buffer[1024];
            
            switch (column_data->token_type)
            {
            case HT_COLUMN_DATE_INT:
            case HT_COLUMN_INT:
                sprintf(buffer,"%d",(int)data);
                break;
            case HT_COLUMN_DATE_STRING:
            case HT_COLUMN_STRING:
                strcpy(buffer, (char*)data);
                break;
            }
            XmFontList font_list;
            xmstr = fe_ConvertToXmString ((unsigned char *) buffer, charset, 
                                          NULL, XmFONT_IS_FONT, &font_list);
        }
    }
    XtVaSetValues(_tree,
                  XmNrow,            row,
                  XmNcolumn,         column,
                  XmNcellString,     xmstr,
                  XmNcellEditable,   is_editable,
                  NULL);
    if (xmstr)
        XmStringFree(xmstr);
}

void
XFE_RDFTreeView::delete_row(int row)
{
  
   XmLGridDeleteRows(_tree, XmCONTENT, row, 1);

}

void
XFE_RDFTreeView::add_column(int index, char *name, uint32 width,
                        void *token, uint32 token_type)
{
  D( fprintf(stderr,"XFE_RDFTreeView::add_column index(%d) name(%s) width(%d)\n",
             index, name,width););

  if (index > 0) {
      XmLGridAddColumns(_tree, XmCONTENT, index, 1);
  }

  XtVaSetValues(_tree,
                XmNcolumn, index,
                XmNcolumnSizePolicy, XmCONSTANT,
                XmNcolumnWidth, width,
                NULL);

  setColumnData(index, token, token_type);

  XmLGridSetStringsPos(_tree, 
                       XmHEADING, 0,
                       XmCONTENT, index, 
                       name);
}

void
XFE_RDFTreeView::delete_column(HT_Resource /* cursor */)
{

}

void 
XFE_RDFTreeView::expand_row_cb(Widget,
                           XtPointer clientData,
                           XtPointer callData)
{
	XFE_RDFTreeView *obj = (XFE_RDFTreeView*)clientData;
    XmLGridCallbackStruct *cbs = (XmLGridCallbackStruct *)callData;

    D(fprintf(stderr,"expand_row_cb(%d)\n",cbs->row););

	obj->expand_row(cbs->row);
}

void
XFE_RDFTreeView::expand_row(int row)
{
    HT_Resource node = HT_GetNthItem (_ht_view, row);
     
    HT_SetOpenState (node, (PRBool)TRUE);
}

void 
XFE_RDFTreeView::collapse_row_cb(Widget,
                             XtPointer clientData,
                             XtPointer callData)
{
	XFE_RDFTreeView *obj = (XFE_RDFTreeView*)clientData;
    XmLGridCallbackStruct *cbs = (XmLGridCallbackStruct *)callData;
 
    D(fprintf(stderr,"collapse_row_cb(%d)\n",cbs->row););

	obj->collapse_row(cbs->row);
}

void
XFE_RDFTreeView::collapse_row(int row)
{
    HT_Resource node = HT_GetNthItem (_ht_view, row);
     
    HT_SetOpenState (node, (PRBool)FALSE);
}

void 
XFE_RDFTreeView::delete_cb(Widget w,
                       XtPointer clientData,
                       XtPointer callData)
{
	XFE_RDFTreeView *obj = (XFE_RDFTreeView*)clientData;
    XmLGridCallbackStruct *cbs = (XmLGridCallbackStruct *)callData;
 
	cbs = (XmLGridCallbackStruct *)callData;

 	if (cbs->reason != XmCR_DELETE_COLUMN)
		return;

    XFE_ColumnData * column_data = obj->getColumnData(cbs->column);
    if (column_data)
        delete column_data;
}

void 
XFE_RDFTreeView::activate_cb(Widget,
                         XtPointer clientData,
                         XtPointer callData)
{
	XFE_RDFTreeView *obj = (XFE_RDFTreeView*)clientData;
    XmLGridCallbackStruct *cbs = (XmLGridCallbackStruct *)callData;
 
    if (cbs->rowType != XmCONTENT)
        return;

	obj->activate_row(cbs->row);
}

//
// Popup menu stuff
//
void 
XFE_RDFTreeView::popup_cb(Widget,
                      XtPointer clientData,
                      XtPointer callData)
{
	XFE_RDFTreeView *obj = (XFE_RDFTreeView*)clientData;
    XmLGridCallbackStruct *cbs = (XmLGridCallbackStruct *)callData;
 
    if (cbs->rowType != XmCONTENT)
        return;

	obj->doPopup(cbs->event);
}

//////////////////////////////////////////////////////////////////////////
void
XFE_RDFTreeView::doPopup(XEvent * event)
{
	if (_popup)
	{
        delete _popup; //destroy the old one first
    }
    _popup = new XFE_RDFPopupMenu("popup",
                                   //getFrame(),
                                   FE_GetToplevelWidget(),
                                   _ht_view,
                                   FALSE, // not isWorkspace
                                   FALSE); // no background commands for now
		
	_popup->position(event);
	_popup->show();
}
//////////////////////////////////////////////////////////////////////////
//
// Toggle the stand alone state
//
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFTreeView::setStandAlone(XP_Bool stand_alone)
{
	XP_ASSERT( XfeIsAlive(_tree) );

	_isStandAlone = stand_alone;
#ifdef UNDEF
    /* 
     * We need to set lot more properties based on standalone state. 
     * We instead call setHTTreeviewProperties to do all that
     */

	int visibleColumns = (_isStandAlone ? 0 : 1);

	XtVaSetValues(_tree,
				  XmNvisibleColumns,		visibleColumns,
				  NULL);
#endif   /* UNDEF */

}
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
//  Set the HT View properties to the tree
//////////////////////////////////////////////////////////////////////////


void
XFE_RDFTreeView::setHTTreeViewProperties( HT_View  view)
{

   Arg           av[30];
   Cardinal      ac=0;
   void *        data=NULL;
   Pixel         pixel;
   PRBool        gotit=False;


    
//////////////////////////////////////////////////////////////////////////
//  Properties common to all views
//  Should probably go to a base class
//////////////////////////////////////////////////////////////////////////


   /* viewBGColor */
   HT_GetTemplateData(HT_TopNode(view),  gNavCenter->viewBGColor, HT_COLUMN_STRING, &data);
   if (data)
   {
      gotit = fe_GetPixelFromRGBString(getContext(), (char *) data, &pixel);
      if (gotit) {
         XtSetArg(av[ac], XmNbackground, pixel); ac++;
      }
   }


   /* viewFGColor */
  HT_GetTemplateData(HT_TopNode(view),  gNavCenter->viewFGColor, HT_COLUMN_STRING, &data);
   if (data) 
   {
      gotit = fe_GetPixelFromRGBString(getContext(), (char *) data, &pixel);
      if (gotit) {
         XtSetArg(av[ac], XmNforeground, pixel); ac++;
      }
   }

   /* viewBGURL */
  HT_GetTemplateData(HT_TopNode(view),  gNavCenter->viewBGURL, HT_COLUMN_STRING, &data);

   if (data)
   {
      /*   Do the RDFImage thing here */
      char * imageURL = (char *)data;
      XFE_RDFImage * rdfImage=NULL;
      Pixmap         image, mask;


      rdfImage = XFE_RDFImage::isImageAvailable(imageURL);
      if (rdfImage) {
         image = rdfImage->getPixmap();
         mask = rdfImage->getMask();
         XtVaSetValues(_tree, XmNbackgroundPixmap, image, NULL);
      }
      else {

         rdfImage = new XFE_RDFImage(m_toplevel, (void *) this, (char *) data, CONTEXT_DATA(m_contextData)->colormap, _tree);
         rdfImage->setCompleteCallback((completeCallbackPtr)treeview_bg_image_cb, (void *) _tree);
         rdfImage->loadImage();
      }
      
    
   }


//////////////////////////////////////////////////////////////////////////
//  Properties common to all trees
//////////////////////////////////////////////////////////////////////////

   /* selectionFGColor */
   HT_GetTemplateData(HT_TopNode(view),  gNavCenter->selectionFGColor, HT_COLUMN_STRING, &data);
   if (data)
   {
      gotit = fe_GetPixelFromRGBString(getContext(), (char *) data, &pixel);
      if (gotit) {
         XtSetArg(av[ac], XmNselectForeground, pixel); ac++;
      }
   }

   /* selectionBGColor */
   HT_GetTemplateData(HT_TopNode(view),  gNavCenter->selectionBGColor, HT_COLUMN_STRING, &data);
   if (data)
   {
      gotit = fe_GetPixelFromRGBString(getContext(), (char *) data, &pixel);
      if (gotit) {
         XtSetArg(av[ac], XmNselectBackground, pixel); ac++;
      }
   }



   /* treeConnectionColor */
   HT_GetTemplateData(HT_TopNode(view),  gNavCenter->treeConnectionFGColor, HT_COLUMN_STRING, &data);
   if (data)
   {
      gotit = fe_GetPixelFromRGBString(getContext(), (char *) data, &pixel);
      if (gotit) {
         XtSetArg(av[ac], XmNconnectingLineColor, pixel); ac++;
      }
   }


   /*  showColumnHeaders */ 
   /* This doesn't work now. showColumnHeaders returns False in docked as well
    * as in standalone mode. So we'll use the FE method to figure this, until
    * the BE is fixed 
    */
/*
   HT_GetTemplateData(HT_TopNode(view),  gNavCenter->showColumnHeaders, HT_COLUMN_STRING, &data);
   if (data)
   {
       char * answer = (char *) data;
       if ((!XP_STRCASECMP(answer, "yes")) || (!XP_STRCASECMP(answer, "1")))
       {  }
    else if ((!XP_STRCASECMP(answer, "No")) || (!XP_STRCASECMP(answer, "0")))
    {  }
*/
    if (isStandAlone())
    {
         /* Management mode */
         XtSetArg(av[ac], XmNheadingRows, 1);      
         ac ++;
         XtSetArg(av[ac], XmNhideUnhideButtons, True);
         ac++;
         XtSetArg(av[ac], XmNsingleClickActivation, False);
         ac++;

    }
    else
    {
         /* Navigation mode */
         XtSetArg(av[ac], XmNheadingRows, 0);      
         ac ++;
         XtSetArg(av[ac], XmNhideUnhideButtons, False);
         ac++;
         XtSetArg(av[ac], XmNvisibleColumns, 1);
         ac++;
         XtSetArg(av[ac], XmNsingleClickActivation, True);
         ac++;

    }


   /* useInlineEditing. This doesn't work now for teh same reason as above */ 
   HT_GetTemplateData(getRootFolder(),
                      gNavCenter->useInlineEditing,
                      HT_COLUMN_STRING, &data);
   if (data)
   {
       /* Decide if cells are editable */
   }



   XtSetValues(_tree, av, ac);



#ifdef UNDEF   /* Properties that can't be set yet in the tree widget */


   /* viewRolloverColor */
HT_GetTemplateData(HT_TopNode(view),  gNavCenter->viewRolloverColor, HT_COLUMN_STRING, &(viewProperties->viewRolloverColor));

   /* viewPressedColor */
HT_GetTemplateData(HT_TopNode(view),  gNavCenter->viewPressedColor, HT_COLUMN_STRING, &(viewProperties->viewPressedColor));

   /*  showTreeConnections */
 HT_GetTemplateData(HT_TopNode(view),  gNavCenter->showTreeConnections, HT_COLUMN_STRING, &(viewProperties->showTreeConnections));

   /* showDividers */
 HT_GetTemplateData(HT_TopNode(view),  gNavCenter->showDivider, HT_COLUMN_STRING, &(viewProperties->showDivider)); 

   /* dividerColor */
HT_GetTemplateData(HT_TopNode(view),  gNavCenter->dividerColor, HT_COLUMN_STRING, &(viewProperties->dividerColor));

   /* useSingleClick */
HT_GetTemplateData(HT_TopNode(view),  gNavCenter->useSingleClick, HT_COLUMN_STRING, &(viewProperties->useSingleClick));

   /* useSelection */
HT_GetTemplateData(HT_TopNode(view),  gNavCenter->useSelection, HT_COLUMN_STRING, &(viewProperties->useSelection));

   /* selectedColumnHeaderFGColor */
HT_GetTemplateData(HT_TopNode(view),  gNavCenter->selectedColumnHeaderFGColor, HT_COLUMN_STRING, &(viewProperties->selectedColumnHeaderFGColor));

   /* selectedColumnHeaderBGColor */
HT_GetTemplateData(HT_TopNode(view),  gNavCenter->selectedColumnHeaderBGColor, HT_COLUMN_STRING, &(viewProperties->selectedColumnHeaderBGColor));

   /* sortColumnFGColor */
HT_GetTemplateData(HT_TopNode(view),  gNavCenter->sortColumnFGColor, HT_COLUMN_STRING, &(viewProperties->sortColumnFGColor));

   /*  sortColumnBGColor */
HT_GetTemplateData(HT_TopNode(view),  gNavCenter->sortColumnBGColor, HT_COLUMN_STRING, &(viewProperties->sortColumnBGColor));


#endif   /* UNDEF  */


}

extern "C" {

    void
    treeview_bg_image_cb(XtPointer client_data)
    {
   
        callbackClientData * cb = (callbackClientData *) client_data;
        Widget tree = (Widget )cb->widget;
        //Dimension b_width=0, b_height=0;
        
        XtVaSetValues(tree, XmNbackgroundPixmap, cb->image, NULL);
        XP_FREE(cb);

    }

};   /* extern C  */
