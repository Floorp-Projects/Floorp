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
   MailFilterRulesView.h -- class definition for MailFilterRulesView
   Created: Tao Cheng <tao@netscape.com>, 31-jan-97
 */



#ifndef _MAILFILTERRULESVIEW_H_
#define _MAILFILTERRULESVIEW_H_

#include "msg_filt.h"  
#include "msg_srch.h"  

#include "MNView.h"
#include "FolderDropdown.h"


class XFE_MailFilterRulesView : public XFE_MNView
{
public:
	XFE_MailFilterRulesView(XFE_Component *toplevel_component, 
					   Widget         parent, 
					   XFE_View      *parent_view, 
					   MWContext     *context,
					   XP_Bool        isNew);
	virtual ~XFE_MailFilterRulesView();

	virtual void apply();
	virtual void cancel();

	void setDlgValues(MSG_FilterList *list, MSG_FilterIndex at); 

	static const char *listChanged;

protected:

private:

	XFE_FolderDropdown *m_folderDropDown;

	typedef struct fe_mailfilt_userData {
		int row;
		MSG_SearchAttribute attrib;
	} fe_mailfilt_userData;
 
	Widget m_dialog;
	Widget m_filterName; 
	Widget m_rulesRC;
	Widget m_content[5];	 /* rule content RC */
	Widget m_scopeOpt[5];  /* scope option */
	Widget m_whereOpt[5];  /* where option */
	Widget m_scopeOptPopup[5];
	Widget m_whereOptPopup[5];
	Widget m_scopeText[5]; /* scope text */

	Widget m_priLevel[5];  /* priority level */
	Widget m_priLevelPopup[5];

	Widget m_statusLevel[5];  /* status */
	Widget m_statusLevelPopup[5];

	Widget m_whereLabel[5];
	Widget m_rulesLabel[5];
	Widget m_thenTo;
	Widget m_thenToPopup;
	Widget m_thenClause;
	Widget m_thenClausePopup;
	Widget m_priAction;
	Widget m_priActionPopup;
	Widget m_commandGroup;
	Widget m_despField;
	Widget m_strip[5];
	Widget m_mainDespField;
	Widget m_filterOnBtn;
	Widget m_filterOffBtn;

    /* the boolean and/or option menu and its two children: */
    Widget m_optionAndOr;
    Widget m_optionAnd;
    Widget m_optionOr;

	Widget m_noneBtn[5];
	Widget m_normalBtn[5];
	Widget m_lowBtn[5];
	Widget m_highBtn[5];

    Widget m_newFolderBtn;

	Dimension       m_despwidth;

	MSG_FilterList *m_filterlist;
	MSG_FilterIndex m_at;
	XP_Bool         m_isNew;

	int             m_stripCount;
	XP_Bool         m_curFilterOn;

	XP_Bool         m_booleanAnd;

	// callbacks
	static void newok_cb(Widget w, XtPointer closure, XtPointer call_data);
	static void editok_cb(Widget w, XtPointer closure, XtPointer call_data);
	static void editclose_cb(Widget w, XtPointer closure, XtPointer calldata);
    static void editHdr_cb(Widget w, XtPointer closure, XtPointer calldata);

	static void addRow_cb(Widget w, XtPointer closure, XtPointer call_data);
	static void delRow_cb(Widget w, XtPointer closure, XtPointer call_data);
	static void thenClause_cb(Widget w, XtPointer closure, XtPointer calldata);
	static void turnOnOff(Widget w, XtPointer closure, XtPointer calldata);
	static void whereOpt_cb(Widget w, XtPointer clientData, XtPointer calldata);
	static void scopeOpt_cb(Widget w, XtPointer clientData, XtPointer calldata);
    static void andOr_cb(Widget, XtPointer, XtPointer);
    static void newFolder_cb(Widget, XtPointer, XtPointer);

	// the shadow
	void newokCB(Widget w, XtPointer call_data);
	void editokCB(Widget w, XtPointer call_data);
	void editcloseCB(Widget w, XtPointer calldata);

	void addRowCB(Widget w, XtPointer call_data);
	void delRowCB(Widget w, XtPointer call_data);
	void thenClauseCB(Widget w, XtPointer calldata);
	void turnOnOffCB(Widget w, XtPointer calldata);
	void whereOptCB(Widget w, XtPointer calldata);
	void scopeOptCB(Widget w, XtPointer calldata);
	void newFolderCB(Widget w, XtPointer calldata);

    void editHeaders(Widget);

	void   makeRules(Widget rowcol, int fake_num);

	Widget make_option_menu(Widget parent, 
							char* widgetName, Widget *popup);

	Widget make_actPopup(Widget popupW, 
						 Dimension *width, Dimension *height, int row, 
						 XtCallbackProc cb);

	Widget make_opPopup(Widget popupW, 
						MSG_SearchMenuItem menu[],
						int itemNum, 
						Dimension *width, 
						Dimension *height,	
						int row, XtCallbackProc cb);

	Widget makeopt_priority(Widget popupW, 
							Dimension *width, 
							Dimension *height, 
							int row);
	Widget makeopt_status(Widget popupW, 
						  Dimension *width, 
						  Dimension *height, 
						  int row);

	Widget make_attribPopup(Widget popupW, Dimension *width, 
							Dimension *height,	int row, 
							XtCallbackProc cb,
                            Boolean adding = FALSE);

	void add_folders(Widget popupW, XtCallbackProc cb, MSG_FolderInfo *info);

	void buildWhereOpt(Dimension *width, 
					   Dimension *height, int num, 
					   MSG_SearchAttribute attrib);

	void setRulesParams(int num,
						MSG_SearchAttribute attrib,
						MSG_SearchOperator op,
						MSG_SearchValue value,
                        char* customHdr = NULL);

	void setActionParams(MSG_RuleActionType type, void *value);

	void getParams(int position);

	void getTerm(int num, 
				 MSG_SearchAttribute *attrib, 
				 MSG_SearchOperator *op, 
				 MSG_SearchValue *value,
                 char* customHdr);
 
    void  setAllBooleans(Boolean andP);
    void  setOneBoolean(int i);

	void displayData();
	void resetData();

	void getAction(MSG_RuleActionType *action, void **value);
	void get_option_size(Widget optionMenu, 
						 Widget btn,
						 Dimension *retWidth, 
						 Dimension *retHeight);

};

#endif /* _MAILFILTERRULESVIEW_H_ */
