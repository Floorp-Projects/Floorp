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
   TreeView.h -- class definition for TreeView
   Created: spence murray <spence@netscape.com>, 3-Nov-97.
 */



#ifndef _xfe_treeview_h
#define _xfe_treeview_h

#include "NotificationCenter.h"
#include "Outlinable.h"
#include "XmL/Tree.h"

extern "C"
{
  #include "gridpopup.h"
  #include "htrdf.h"
  #include "rdf.h"
} 

class XFE_TreeView : public XFE_Outlinable
{
public:
	XFE_TreeView(const char *name,
				 Widget parent,
				 int tree_type,
				 int width,
				 int height);

	XFE_TreeView();
	virtual ~XFE_TreeView();

	// The Outlinable interface.
	void *ConvFromIndex(int index);
	virtual int ConvToIndex(void *item);
	virtual char *getColumnName(int column);
	virtual char *getColumnHeaderText(int column);
	virtual fe_icon *getColumnHeaderIcon(int column);
	virtual EOutlinerTextStyle getColumnHeaderStyle(int column);
	virtual void *acquireLineData(int line);
	virtual void getTreeInfo(XP_Bool *expandable, XP_Bool *is_expanded, int *depth, 
							 OutlinerAncestorInfo **ancestor);
	virtual EOutlinerTextStyle getColumnStyle(int column);
	virtual char *getColumnText(int column);
	virtual fe_icon *getColumnIcon(int column);
	
	// Tells the Outlinable object that the line data is no
	// longer needed, and it can free any storage associated with
	// it.
	virtual void releaseLineData();
	
	virtual void Buttonfunc(const OutlineButtonFuncData *data);
	virtual void Flippyfunc(const OutlineFlippyFuncData *data);
	
	// return the outliner associated with this outlinable.
	virtual XFE_Outliner *getOutliner();
	
	// Get tooltipString & docString; 
	// returned string shall be freed by the callee
	// row < 0 indicates heading row; otherwise it is a content row
	// (starting from 0)
	//
	virtual char *getCellTipString(int row, int column);
	virtual char *getCellDocString(int row, int column);

	// the notifyProc
	void handleRDFNotify (HT_Notification ns, 
						  HT_Resource node,
						  HT_Event event);
private:
	// load the RDF database
	void getRDFDB ();

	// fill the tree with the current RDF database
	void drawTree ();

	// draw a node
	void drawNode (HT_Resource node);

protected:
	XFE_Outliner *m_outliner;
	Widget m_tree;

	// rdf handles
	RDF_Resources m_rdfResources;
	HT_Pane m_htPane;
	HT_View m_htView;
};

extern "C" void
fe_showTreeView (Widget parent, int tree_type, int width, int height);

	// the notifyProc
extern "C" void
xfe_handleRDFNotify (HT_Notification ns, 
				 HT_Resource node,
				 HT_Event event);

#endif /* _xfe_treeview_h */

