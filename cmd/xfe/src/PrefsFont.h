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
/* */
/*
   PrefsFont.h -- Fontuage preference dialog 
   Created: Linda Wei <lwei@netscape.com>, 25-Feb-97.
 */


#ifndef _xfe_prefsfont_h
#define _xfe_prefsfont_h

#include "Dialog.h"
#include "PrefsDialog.h"
#include "Outliner.h"
#include "Outlinable.h"

struct PrefsDataFont
{
	MWContext *context;

	XFE_Outliner  *font_outliner;
	Widget         font_list;
	Widget         up_button;
	Widget         down_button;

	int            m_rowIndex;
	int            font_displayers_count;
	char         **font_displayers;
};

class XFE_PrefsFontDialog : public XFE_Dialog, public XFE_Outlinable
{
public:

	// Constructors, Destructors

	XFE_PrefsFontDialog(XFE_PrefsDialog *prefsDialog,
						Widget           parent,    
						char            *name,  
						Boolean          modal = TRUE);

	virtual ~XFE_PrefsFontDialog();

	virtual void show();                // pop up dialog
	void initPage();                    // initialize dialog
	Boolean verifyPage();               // verify page
	MWContext *getContext();            // return the context
    XFE_PrefsDialog *getPrefsDialog();  // return the pref dialog											   
	void setSelectionPos(int pos);
	void deselectPos(int pos);

	// Outlinable interface methods

	virtual void *ConvFromIndex(int index);
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
	virtual void releaseLineData();

	virtual void Buttonfunc(const OutlineButtonFuncData *data);
	virtual void Flippyfunc(const OutlineFlippyFuncData *data);

	virtual XFE_Outliner *getOutliner();
	// Get tooltipString & docString; 
	// returned string shall be freed by the callee
	// row < 0 indicates heading row; otherwise it is a content row
	// (starting from 0)
	//
	virtual char *getCellTipString(int /* row */, int /* column */) {return NULL;}
	virtual char *getCellDocString(int /* row */, int /* column */) {return NULL;}


	// Callbacks

	static void cb_ok(Widget, XtPointer, XtPointer);
	static void cb_cancel(Widget, XtPointer, XtPointer);
	static void cb_promote(Widget, XtPointer, XtPointer);
	static void cb_demote(Widget, XtPointer, XtPointer);

private:

	static const int OUTLINER_COLUMN_ORDER;
	static const int OUTLINER_COLUMN_DISPLAYER;
	static const int OUTLINER_COLUMN_MAX_LENGTH;
	static const int OUTLINER_INIT_POS;

	// User data

	XFE_PrefsDialog          *m_prefsDialog;
	PrefsDataFont            *m_prefsDataFont;
};

#endif /* _xfe_prefsfont_h */

