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
   RDFView.h -- class definition for XFE_RDFView
   Created: Stephen Lamm <slamm@netscape.com>, 5-Nov-97.
 */



#ifndef _xfe_rdfview_h
#define _xfe_rdfview_h

#include "View.h"
#include "IconGroup.h"
#include "htrdf.h"
#include "NavCenterView.h"
#include "RDFTreeView.h"

class XFE_RDFView : public XFE_View
{
public:

  XFE_RDFView(XFE_Component *toplevel, Widget parent,
              XFE_View *parent_view, MWContext *context);

  ~XFE_RDFView();

  // Get tooltipString & docString; 
  // returned string shall be freed by the callee
  // row < 0 indicates heading row; otherwise it is a content row
  // (starting from 0)
  //
  virtual char *getCellTipString(int /* row */, int /* column */) {return NULL;}
  virtual char *getCellDocString(int /* row */, int /* column */) {return NULL;}

  // Refresh the outliner
  void refresh(HT_Resource node);

  // Open properties dialog
  //void openPropertiesWindow();
  //void closePropertiesWindow();

  // RDF Specific calls
  void notify(HT_Notification ns, HT_Resource n, HT_Event whatHappened);

	// HT view set/get methods
	void setHTView(HT_View view);
    void setHTTitlebarProperties(HT_View view, Widget titleBar);

	// Stand alone set/get methods
	void setStandAloneState(XP_Bool state);
	XP_Bool getStandAloneState();

private:

	// The view as registered in the hypertree
	HT_View				_ht_rdfView;

	// The label that displays the currently open pane
	Widget				_viewLabel;     

	// Parent of the label and the button on top
	Widget				_controlToolBar; 

	// Toggle tree operating mode
	Widget				_addBookmarkControl;

	// Close the view
	Widget				_closeControl;

	// Toggle tree operating mode
	Widget				_modeControl;

	// The rdf tree view component
	XFE_RDFTreeView *	_rdfTreeView;

	// Is this a stand alone view ?
	XP_Bool				_standAloneState;

	static void closeRdfView_cb(Widget, XtPointer, XtPointer);
};

#endif /* _xfe_rdfview_h */
