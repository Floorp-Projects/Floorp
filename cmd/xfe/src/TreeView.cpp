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
   TreeView.cpp -- class definition for TreeView
   Created: spence murray <spence@netscape.com>, 3-Nov-97.
 */



#include "TreeView.h"
#include "Outlinable.h"
#include "BookmarkView.h"
#include "AddrBookView.h"
#include "HistoryView.h"
#include "ViewGlue.h"
#include "Frame.h"
#include "mozilla.h"

#define TREE_OUTLINER_GEOMETRY_PREF "tree.outliner_geometry"

extern "C" MWContext *fe_WidgetToMWContext (Widget);

// Sanity check on the context.  Throughout the progress stuff below,
// the context (and fe_data) needs to be checked for validity before
// dereferencing the members.
#define CHECK_CONTEXT_AND_DATA(c) \
((c) && CONTEXT_DATA(c) && !CONTEXT_DATA(context)->being_destroyed)

static XFE_Frame * 
fe_frameFromMWContext(MWContext *context)
{
    XFE_Frame * frame = NULL;

    // Sanity check for possible invocation of this function from a frame
    // that was just destroyed (or is being destroyed)
    if (!CHECK_CONTEXT_AND_DATA(context))
    {
		return NULL;
    }

    // Try to use context's frame
    frame = ViewGlue_getFrame(XP_GetNonGridContext(context));
    
	// Try to use the active frame
// 	if (!frame)
// 	{
//  		frame = XFE_Frame::getActiveFrame();
// 	}

	// Make sure the frame is alive
	if (frame && !frame->isAlive())
	{
		frame = NULL;
	}

    return frame;
}

XFE_TreeView::XFE_TreeView (const char *name,
							Widget parent, 
							int tree_type,
							int width,
							int height)
{
#ifdef DEBUG_spence
	printf ("XFE_TreeView: %s\n", name);
#endif

	// get the toplevel
	MWContext *context = fe_WidgetToMWContext (parent);
	XFE_Component *toplevel = fe_frameFromMWContext (context);
		
	/* empty tree */
	if (tree_type == 0)
	{
		int i, n, size;
		XmLTreeRowDefinition *rows;
		static struct
		{
			Boolean expands;
			int level;
			char *string;
		} data[] =
		  {
			  { True,  0, "Root" },
			  { True,  1, "Level 1 Parent" },
			  { False, 2, "1st Child of Level 1 Parent" },
			  { False, 2, "2nd Child of Level 1 Parent" },
			  { True,  2, "Level 2 Parent" },
			  { False, 3, "Child of Level 2 Parent" },
			  { True,  1, "Level 1 Parent" },
			  { False, 2, "Child of Level 1 Parent" },
	};

#ifdef DEBUG_spence
		printf ("empty tree\n");
#endif

		// create the tree
	m_tree = XtVaCreateManagedWidget("tree",
		xmlTreeWidgetClass, parent,
		XmNvisibleRows, 10,
		NULL);
	XtVaSetValues(m_tree,
		XmNcellDefaults, True,
		NULL);

	/* Create a TreeRowDefinition array from the data array */
	/* and add rows to the Tree */
	n = 8;
	size = sizeof(XmLTreeRowDefinition) * n;
	rows = (XmLTreeRowDefinition *)malloc(size);
	for (i = 0; i < n; i++)
	{
		rows[i].level = data[i].level;
		rows[i].expands = data[i].expands;
		rows[i].isExpanded = True;
		rows[i].pixmap = XmUNSPECIFIED_PIXMAP;
		rows[i].pixmask = XmUNSPECIFIED_PIXMAP;
		rows[i].string = XmStringCreateSimple(data[i].string);
	}
	XmLTreeAddRows(m_tree, rows, n, -1);

	/* Free the TreeRowDefintion array (and XmStrings) we created above */
	for (i = 0; i < n; i++)
		XmStringFree(rows[i].string);
	free((char *)rows);

#if 0
	// Save vertical scroller for keyboard accelerators
	CONTEXT_DATA(context)->vscroll = m_outliner->getScroller();
	m_outliner->show();
#endif

#ifdef DEBUG_spence
	printf ("width = %d, height = %d\n", width, height);
#endif

	XtVaSetValues (m_tree, XmNwidth, width, XmNheight, height, NULL);

	// load rdf database
	getRDFDB();

	} else if (tree_type == 1) {
#ifdef DEBUG_spence
		printf ("bookmark tree\n");
#endif
		// bookmark tree
		// create the bookmark view
		XFE_BookmarkView *view = new XFE_BookmarkView(toplevel, 
													  parent, 
													  NULL, 
													  context);

		XtVaSetValues(view->getBaseWidget(),
  		XmNleftAttachment, XmATTACH_FORM,
		XmNtopAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		NULL);

		view->show ();
		m_outliner = view->getOutliner ();
	} else if (tree_type == 2) {
#ifdef DEBUG_spence
		printf ("addressbook tree\n");
#endif
		// create the addressbook view; 
		// import from XFE_BookmarkFrame::XFE_BookmarkFrame
		XFE_AddrBookView *view = new XFE_AddrBookView(toplevel, 
													  parent,
													  NULL,
													  context);
	} else if (tree_type == 3) {
#ifdef DEBUG_spence
		printf ("history tree\n");
#endif
		// create the History view
		XFE_HistoryView *view = new XFE_HistoryView(toplevel, parent, NULL, context);
	}

	if (m_tree == NULL && m_outliner == NULL)
	{
#ifdef DEBUG_spence
		printf ("XFE_TreeView: outliner create failed!\n");
#endif
		return;
	}
}

XFE_TreeView::~XFE_TreeView()
{
	/* nothing to do here */
}

void
XFE_TreeView::getRDFDB()
{
	HT_Notification ns;
	RDF_Resources std;

#ifdef DEBUG_spence
	printf ("getRDFDB:\n");
#endif

#if 0
	ns = (HT_Notification) XP_ALLOC (sizeof (HT_NotificationStruct));
	if (ns == NULL)
	{
		printf ("couldn't allocate NotificationStruct\n");
		return;
	}
#endif

	ns = new HT_NotificationStruct;
	ns->notifyProc = (HT_NotificationProc) xfe_handleRDFNotify;
	ns->data = this;

#if 0
    std = RDF_StdVocab();
#endif

	// now create the HT_View - the backend representation of our tree
	m_htPane = HT_NewPane (ns);
	m_htView = HT_GetNthView (m_htPane, 0);
	// m_htView = HT_NewView (std->RDF_Top, m_htPane);

	// now populate the tree
	drawTree ();
}

void 
XFE_TreeView::handleRDFNotify (HT_Notification ns, 
							   HT_Resource node,
							   HT_Event event)
{
#ifdef DEBUG_spence
	printf ("handleRDFNotify\n");
#endif
}


void
XFE_TreeView::drawTree ()
{
	HT_Pane      pane;
	// fe_Data      *feData = (fe_Data *) HT_GetViewFEData (m_htView);
	int          count = HT_GetItemListCount (m_htView);
	int          n = 0;

#ifdef DEBUG_spence
	printf ("drawTree\n");
#endif

	while (n < count) {
		HT_Resource node = HT_GetNthItem (m_htView, n);
		// add element to tree widget
		drawNode (node);
		++n;
	}
}

void
XFE_TreeView::drawNode (HT_Resource node)
{
	HT_Cursor cursor;
	PRBool isContainer, isOpen, isSelected;
	uint16			colNum, depth;
	uint32			colWidth, colTokenType;
	void *data, *colToken;
	struct tm *time;
	time_t dateVal;
	char buffer [128];

	if (isContainer = HT_IsContainer(node))
	{
		isOpen = HT_IsContainerOpen(node);
		// draw the node
#ifdef DEBUG_spence
		printf ("node draw\n");
#endif
	}

	if ((cursor = HT_NewColumnCursor(HT_GetView(node))) == NULL) {
		return;
	}

	isSelected = HT_IsSelected(node);

	colNum = 0;
	while (HT_GetNextColumn(cursor, NULL, &colWidth, &colToken, &colTokenType) == TRUE)
	{
		if (HT_GetNodeData(node, colToken, colTokenType, &data) == TRUE)
		{
			switch(colTokenType)
			{
			case	HT_COLUMN_DATE_STRING:
				if (data == NULL)	break;
				if ((dateVal = (time_t)atol((char *)data)) == 0)	break;
				if ((time = localtime(&dateVal)) == NULL)	break;

				strftime(buffer,sizeof(buffer),"%#m/%#d/%Y %#I:%M %p",time);
#ifdef DEBUG_spence
				printf ("node: %s\n", buffer);
#endif
			break;

			case	HT_COLUMN_DATE_INT:
				if (data == 0L)	break;
				if ((time = localtime((time_t *) &data)) == NULL)	break;

				strftime(buffer,sizeof(buffer),"%#m/%#d/%Y %#I:%M %p",time);
#ifdef DEBUG_spence
				printf ("node: %s\n", buffer);
#endif
			break;

			case	HT_COLUMN_INT:
				sprintf(buffer,"%d",(int)data);
#ifdef DEBUG_spence
				printf ("node: %s\n", buffer);
#endif
			break;

			case	HT_COLUMN_STRING:
				if (data == NULL)	break;
				if (colNum==0)
				{
					depth = HT_GetItemIndentation(node) - 1;
#ifdef DEBUG_spence
					printf ("node: %s\n", (char *)data);
#endif
				}
				else
				{
#ifdef DEBUG_spence
					printf ("node: %s\n", (char *)data);
#endif
				}
			break;
			}
		}
		if (isContainer)	break;
	}
	HT_DeleteColumnCursor(cursor);
}


// Outlinable interface methods
void *
XFE_TreeView::ConvFromIndex (int /* index */)
{
	return NULL;
}

int
XFE_TreeView::ConvToIndex (void * /* item */)
{
	return 0;
}

char *
XFE_TreeView::getColumnName (int /* column */)
{
	return NULL;
}

char *
XFE_TreeView::getColumnHeaderText (int /* column */)
{
	return NULL;
}

fe_icon *
XFE_TreeView::getColumnHeaderIcon (int /* column */)
{
	return NULL;
}

EOutlinerTextStyle
XFE_TreeView::getColumnHeaderStyle (int /* column */)
{
	return (EOutlinerTextStyle) NULL;
}

void *
XFE_TreeView::acquireLineData (int /* line */)
{
	return NULL;
}

void
XFE_TreeView::getTreeInfo (XP_Bool * /* expandable */, XP_Bool * /* is_expanded */,
			 int * /* depth */, OutlinerAncestorInfo ** /* ancestor */)
{
}

EOutlinerTextStyle
XFE_TreeView::getColumnStyle (int /* column */)
{
	return (EOutlinerTextStyle) NULL;
}

char *
XFE_TreeView::getColumnText (int /* column */)
{
	return NULL;
}

fe_icon *
XFE_TreeView::getColumnIcon (int /* column */)
{
	return NULL;
}

void
XFE_TreeView::releaseLineData()
{
}

void
XFE_TreeView::Buttonfunc (const OutlineButtonFuncData * /* data */)
{
}

void
XFE_TreeView::Flippyfunc (const OutlineFlippyFuncData * /* data */)
{
}

XFE_Outliner *
XFE_TreeView::getOutliner()
{
	return m_outliner;
}

char *
XFE_TreeView::getCellTipString (int /* row */, int /* column */)
{
	return NULL;
}

char *
XFE_TreeView::getCellDocString (int /* row */, int /* column */)
{
	return NULL;
}


extern "C" void
fe_showTreeView (Widget parent, int tree_type, int width, int height)
{
	static XP_Bool been_here = FALSE;

#ifdef DEBUG_spence
	printf ("XFE_TreeView: showTreeView()\n");
#endif

	if (!been_here) {
#ifdef DEBUG_spence
		printf ("RDF_Init()\n");
#endif
		RDF_Init("file://./"); // assume it hasn't been initialized yet
		been_here = TRUE;
	}

	XFE_TreeView *tree = new XFE_TreeView ("tree", 
										   parent,
										   tree_type,
										   width,
										   height);

	XtRealizeWidget (parent);

#ifdef DEBUG_spence
	if (tree == NULL)
	{
		printf ("fe_showTreeView: create failed!\n");
		return;
	}
#endif

}

extern "C" void 
xfe_handleRDFNotify (HT_Notification ns, 
					 HT_Resource node,
					 HT_Event event)
{
#ifdef DEBUG_spence
	printf ("handleRDFNotify\n");
#endif

	XFE_TreeView *tree = (XFE_TreeView *) ns->data;
	tree->handleRDFNotify (ns, node, event);
}

