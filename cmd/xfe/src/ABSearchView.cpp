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
   ABSearchView.cpp -- class definition for XFE_ABSearchView
   Created: Tao Cheng <tao@netscape.com>, 23-oct-97
 */

#include "ABSearchView.h"
#include "ViewDialog.h"

#include <Xm/Form.h>
#include <Xm/LabelG.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/TextF.h>
#include <Xm/Frame.h>
#include <Xm/RowColumn.h>
#include <Xfe/Xfe.h>

#include "xpgetstr.h"
#include "xfe.h"
#include "felocale.h"

extern int XFE_SEARCH_DLG_PROMPT;

extern int XFE_SEARCH_ATTRIB_NAME;
extern int XFE_SEARCH_ATTRIB_EMAIL;
extern int XFE_SEARCH_ATTRIB_ORGAN;
extern int XFE_SEARCH_ATTRIB_ORGANU;
extern int XFE_SEARCH_RESULT_PROMPT;

extern int XFE_AB_SEARCH;
extern int XFE_SEARCH_BASIC;
extern int XFE_SEARCH_ADVANCED;
extern int XFE_SEARCH_MORE;
extern int XFE_SEARCH_FEWER;

extern int XFE_DLG_CANCEL;
extern int XFE_DLG_CLEAR;

extern int XFE_SEARCH_BOOL_PROMPT;
extern int XFE_SEARCH_BOOL_AND_PROMPT;
extern int XFE_SEARCH_BOOL_OR_PROMPT;
extern int XFE_SEARCH_BOOL_WHERE;
extern int XFE_SEARCH_BOOL_THE;
extern int XFE_SEARCH_BOOL_AND_THE;
extern int XFE_SEARCH_BOOL_OR_THE;

extern "C" Widget 
fe_make_option_menu(Widget toplevel, Widget parent, char* widgetName, 
					Widget *popup);

//
const char *XFE_ABSearchView::dlgClose = 
                             "XFE_ABSearchView::dlgClose";

XFE_ABSearchView::XFE_ABSearchView(XFE_Component      *toplevel_component,
								   Widget              parent,
								   MWContext          *context,
								   ABSearchInfo_t *info):
	XFE_MNView(toplevel_component, 
			   NULL, 
			   context,
			   (MSG_Pane *)NULL),
	m_mode(AB_SEARCH_NONE),
	m_searchInfo(info),
	m_params(NULL),
	m_nRules(0),
	m_nAllocted(0),
	m_attrItems(NULL),
	m_nAttribItems(0),
	m_ruleCmdGroupForm(0),
	m_andOrForm(0)
{
	XP_ASSERT(info && info->m_dir);
	m_dir = info->m_dir;

	setBaseWidget(parent);

	Arg av [20];
	int ac = 0;


	/* 2 sub forms
	 */
	Widget subForms[2],
		   workForm;

	/* search rules
	 */
    ac = 0;
    subForms[0] = XmCreateForm(parent, "searchRulesForm", av, ac);
    XtManageChild(subForms[0]);

    /* Frame
     */
    ac = 0;
    XtSetArg (av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av[ac], 
			  XmNchildHorizontalAlignment, XmALIGNMENT_BEGINNING);ac++; 
    Widget frame = XmCreateFrame(subForms[0], "viewFrame", av, ac);
    XtManageChild(frame);

    /* frame title
     */
	Widget title;

	/* to satisfy gcc
	 */
	char tmp[256];
	sprintf(tmp,
			"%s %s", 
			XP_GetString(XFE_SEARCH_DLG_PROMPT),
			m_dir->description?m_dir->description:"");
	
	title = XtVaCreateManagedWidget(tmp,
									xmLabelGadgetClass, frame,
									XmNchildType, XmFRAME_TITLE_CHILD,
									NULL);
	m_dirPrompt = title;

    ac = 0;
	XtSetArg (av[ac], XmNchildType, XmFRAME_WORKAREA_CHILD); ac++;
    workForm = XmCreateForm(frame, "workAreaForm", av, ac);
    XtManageChild(workForm);
	m_workForm = workForm;

	/* row Col
	 */
	ac = 0;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	m_rulesRowCol = XmCreateRowColumn(workForm, "rulesRowCol", av, ac);

	/* result prompt
	 */
	m_searchRulesPrompt =
		XtVaCreateManagedWidget(XP_GetString(XFE_SEARCH_RESULT_PROMPT),
								xmLabelGadgetClass, workForm,
								XmNleftAttachment, XmATTACH_FORM,
								XmNtopAttachment, XmATTACH_WIDGET,
								XmNtopWidget, m_rulesRowCol,
								XmNtopOffset, 5,
								XmNrightAttachment, XmATTACH_FORM,
								XmNbottomAttachment, XmATTACH_FORM,
								NULL);

	/* cmdButtonForm
	 */
    ac = 0;
    subForms[1] = XmCreateForm (parent, "cmdButtonForm", av, ac);
    XtManageChild(subForms[1]);

	/* cmdRowCol1
	 */
    ac = 0;
    XtSetArg(av[ac], XmNorientation, XmVERTICAL); ac++;	
    XtSetArg(av[ac], XmNentryAlignment, XmALIGNMENT_CENTER); ac++;	
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	Widget rowCol1 = XmCreateRowColumn(subForms[1] , "cmdRowCol1", av, ac);
	XtManageChild(rowCol1);

	/* Search button
	 */
	ac = 0;
	Widget dummy = XmCreatePushButton(rowCol1, 
									  XP_GetString(XFE_AB_SEARCH), 
									  av, ac);
	XtVaSetValues(dummy,
				  XmNalignment, XmALIGNMENT_CENTER,
				  NULL);
	fe_SetString(dummy, XmNlabelString, XP_GetString(XFE_AB_SEARCH));
	XtManageChild(dummy);

	XtAddCallback(dummy, XmNactivateCallback, 
				  XFE_ABSearchView::okCallback, 
				  this);

	/* Cancel button
	 */
	ac = 0;
	dummy = XmCreatePushButton(rowCol1, 
							   XP_GetString(XFE_DLG_CANCEL), 
							   av, ac);
	XtVaSetValues(dummy,
				  XmNalignment, XmALIGNMENT_CENTER,
				  NULL);
	fe_SetString(dummy, XmNlabelString, XP_GetString(XFE_DLG_CANCEL));
	XtManageChild(dummy);

	XtAddCallback(dummy, XmNactivateCallback, 
				  XFE_ABSearchView::cancelCallback, 
				  this);

    ac = 0;
    XtSetArg(av[ac], XmNorientation, XmVERTICAL); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_NONE); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	Widget rowCol2 = XmCreateRowColumn(subForms[1] , "cmdRowCol2", av, ac);
	XtManageChild(rowCol2);

	ac = 0;
	XtSetArg(av[ac], XmNalignment, XmALIGNMENT_CENTER);ac++;
	dummy = m_modeToggle = 
		XmCreatePushButton(rowCol2, 
						   ((info->m_mode == AB_SEARCH_BASIC)?
							XP_GetString(XFE_SEARCH_ADVANCED):
							XP_GetString(XFE_SEARCH_BASIC)),
						   av, ac);
	XtManageChild(dummy);

	/* attachment 
	 */
	XtVaSetValues(subForms[1],
				  // XmNheight, 300,
				  XmNleftAttachment, XmATTACH_NONE,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);

	XtVaSetValues(subForms[0],
				  // XmNheight, 300,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_WIDGET,
				  XmNrightWidget, subForms[1],
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);
	
	/* pre-alloc
	 */
	if (!m_params ||
		!m_nAllocted) {
		m_nAllocted = 4;
		m_params = (ABSearchUIParam_t *) XP_CALLOC(m_nAllocted, 
												   sizeof(ABSearchUIParam_t));
	}/* if */

	XtManageChild(m_rulesRowCol);
	toggleSearchMode(info);

	//callbacks
	XtAddCallback(m_modeToggle, XmNactivateCallback, 
				  XFE_ABSearchView::toggleSearchModeCallback, 
				  this);
}

XFE_ABSearchView::~XFE_ABSearchView()
{
}

Widget 
XFE_ABSearchView::rebuildOptChildren(Widget            popupParent, 
									 int               num, 
									 ABSearchUIHint_t *hint,
									 XtCallbackProc    cb)
{
	XP_ASSERT(num && hint);

	WidgetList childrenList;
	int        numChildren;
	
	/* Clean up first 
	 */
	XtVaGetValues(popupParent, 
				  XmNchildren, &childrenList,
				  XmNnumChildren, &numChildren,
				  NULL);
    int i;
	for (i = 0; i < numChildren; i++) {
		/* free user data
		 */

		/* destroy widget
		 */
		XtDestroyWidget(childrenList[i]);
	}/* for i */

	/* Create new ones... 
	 */
	Arg av[20];
	int ac = 0;
	
	XmString xmStr;
	Widget btn, 
		   btn1 = NULL;
	for (i = 0; i < num; i++) {
		xmStr = XmStringCreateSimple(hint[i].m_menuItem->name);
		
		ac = 0;
		XtSetArg(av[ac], XmNlabelString, xmStr); ac++;
		XtSetArg(av[ac], XmNuserData, &(hint[i])); ac++;
		btn = XmCreatePushButtonGadget(popupParent, "optBtn", av, ac);
		if (!btn1)
			btn1 = btn;

		hint[i].m_obj = (void *) this;
		XtAddCallback(btn, XmNactivateCallback, cb, this);

		XtManageChild(btn);
		XmStringFree(xmStr);
	}/* for i */

	return btn1;
}

Widget 
XFE_ABSearchView::makeOptMenu(Widget            parent, 
							  int               num, 
							  ABSearchUIHint_t *hint,
							  XtCallbackProc    cb,
							  Widget           *popUp)
{
	char tmp[32];
	sprintf(tmp, "optMenu_%d", hint[0].m_type);
	Widget popup = NULL, /* to build pull down */
		   optMenu = fe_make_option_menu(getToplevel()->getBaseWidget(),
										 parent, tmp, &popup);
	*popUp = popup;

	/* rebuild popup children
	 */
	Widget btn1 = rebuildOptChildren(popup,
									 num, 
									 hint,
									 cb);
	XtVaSetValues(optMenu, 
				  XmNmenuHistory, btn1, 
				  XmNtraversalOn, False, 
				  NULL);
	XtManageChild(optMenu);
	return optMenu;
}

void XFE_ABSearchView::cleanupUI(uint16 i, XP_Bool clearRule)
{
	if (m_params[i].m_ruleForm) {
		if (clearRule) {
			m_params[i].m_op = opContains;
			m_params[i].m_attribNval.attribute = attribCommonName;
			XP_FREEIF(m_params[i].m_attribNval.u.string);
		}/* if */

		XtUnmanageChild(m_params[i].m_ruleForm);
		
		if (m_params[i].m_attribOptMenu) {
			XtUnmanageChild(m_params[i].m_attribOptMenu);
			
			/* TODO: need to free heap here
			 */
			
			/*
			 */
			XtDestroyWidget(m_params[i].m_attribOptMenu);
			m_params[i].m_attribOptMenu = NULL;
		}/* if */
		
		if (m_params[i].m_opOptMenu) {
			XtUnmanageChild(m_params[i].m_opOptMenu);
			XtDestroyWidget(m_params[i].m_opOptMenu);
			m_params[i].m_opOptMenu = NULL;
		}/* if */
	
		if (m_params[i].m_label) {
			XtUnmanageChild(m_params[i].m_label);
			XtDestroyWidget(m_params[i].m_label);
			m_params[i].m_label = NULL;
		}/* if */
		
		if (m_params[i].m_textF) {
			XtUnmanageChild(m_params[i].m_textF);
			XtDestroyWidget(m_params[i].m_textF);
			m_params[i].m_textF = NULL;
		}/* if */
		XtDestroyWidget(m_params[i].m_ruleForm);
		m_params[i].m_ruleForm = NULL;
	}/* if form */
}

void XFE_ABSearchView::remove1Rule()
{
	XP_ASSERT(m_nRules > 1);
	m_nRules--;
	cleanupUI(m_nRules, True);
}

void XFE_ABSearchView::setOptMenu(Widget w, int16 attrib)
{
	if (!w ||
		!XmIsRowColumn(w))
		XP_ASSERT(0);

	Widget popup = NULL;
	XtVaGetValues(w, 
				  XmNsubMenuId, &popup, 
				  NULL);

	if (!popup ||
		!XmIsRowColumn(popup))
		XP_ASSERT(0);

	int        numChildren = 0;
	WidgetList childrenList;

    XtVaGetValues(popup, 
				  XmNchildren,    &childrenList,
                  XmNnumChildren, &numChildren, 
				  NULL);
	XP_ASSERT(childrenList && numChildren);
	void *userData = NULL;
	for (int i=0; i < numChildren; i++) {
		XtVaGetValues(childrenList[i], XmNuserData, &userData, NULL);
		if (!XfeIsAlive(childrenList[i])) {
			continue;
		}/* if */

		if (attrib == 
			((ABSearchUIHint_t *)userData)->m_menuItem->attrib) {
			XtVaSetValues(w, 
						  XmNmenuHistory, childrenList[i], 
						  NULL);
			break;
		}/* if */
	}/* for i */
}

void XFE_ABSearchView::getAttribNames()
{
	m_attrItems = 
		(MSG_SearchMenuItem *)XP_CALLOC(kNumAttributes, 	
										sizeof(MSG_SearchMenuItem));
	m_nAttribItems = kNumAttributes;
	
	DIR_Server *dirs[1];
	dirs[0]= (DIR_Server*) m_dir;
	
	MSG_SearchError err;
	err = MSG_GetAttributesForSearchScopes(fe_getMNMaster(), 
										   scopeLdapDirectory,
										   (void **) dirs, 
										   1, 
										   m_attrItems,
										   &m_nAttribItems);
}

void XFE_ABSearchView::add1Rule(MSG_SearchAttribute  attrib,
								MSG_SearchOperator   op,
								char                *stringVal)
{
	Arg av [20];
	int ac = 0;

	/* the form 
	 */
	if (m_nAllocted <= m_nRules) {
		m_nAllocted = m_nRules+1;
		m_params = (ABSearchUIParam_t *) 
			XP_REALLOC(m_params,
					   m_nAllocted*sizeof(ABSearchUIParam_t));
		memset(&(m_params[m_nRules]), 0, sizeof(ABSearchUIParam_t));
	}/* else if */

	if (!m_params[m_nRules].m_ruleForm) {
		ac = 0;
		m_params[m_nRules].m_ruleForm = 
			XmCreateForm(m_rulesRowCol, "advancedRuleForm", av, ac);
	}/* if */
	// 0: andOr
	XtVaSetValues(m_params[m_nRules].m_ruleForm, 
				  XmNpositionIndex, m_nRules+1,
				  0);

	/* label: and/or the
	 */
	/* compute
	 */
	Dimension wid = 0, max_with = 0;
	char tmp[256];
	Widget dummy =
		XtVaCreateManagedWidget(tmp,
								xmLabelWidgetClass, 
								m_params[m_nRules].m_ruleForm,
								XmNalignment, XmALIGNMENT_END, 
								XmNleftAttachment, XmATTACH_FORM,
								XmNtopAttachment, XmATTACH_FORM,
								XmNrightAttachment, XmATTACH_NONE,
								XmNbottomAttachment, XmATTACH_FORM,
								NULL);
	for (int j=0; j < 3; j++) {
		sprintf(tmp,
				"%s", 
				XP_GetString(XFE_SEARCH_BOOL_THE+j));
		fe_SetString(dummy, XmNlabelString, tmp);

		XtVaGetValues(dummy, 
					  XmNwidth, &wid,
					  NULL);
		if (wid > max_with)
			max_with = wid;
	}/* for i */
	XtDestroyWidget(dummy);

	/* real
	 */
	sprintf(tmp,
			"%s", 
			(m_nRules==0)?
			(XP_GetString(XFE_SEARCH_BOOL_THE)):
			(m_searchInfo->m_logicOp?
			 XP_GetString(XFE_SEARCH_BOOL_AND_THE):
			 XP_GetString(XFE_SEARCH_BOOL_OR_THE)));

	m_params[m_nRules].m_label = 
		XtVaCreateManagedWidget(tmp,
								xmLabelWidgetClass, 
								m_params[m_nRules].m_ruleForm,
								XmNwidth, max_with,
								XmNrecomputeSize, False,
								XmNalignment, XmALIGNMENT_END, 
								XmNleftAttachment, XmATTACH_FORM,
								XmNtopAttachment, XmATTACH_FORM,
								XmNrightAttachment, XmATTACH_NONE,
								XmNbottomAttachment, XmATTACH_FORM,
								NULL);

	/* Attrib
	 */
	if (!m_nAttribItems ||
		!m_attrItems)
		getAttribNames();

	/* attrib menu
	 */
	ABSearchUIHint_t *hint = 
		(ABSearchUIHint_t *) XP_CALLOC(m_nAttribItems, 
									   sizeof(ABSearchUIHint_t));
	XP_Bool validAttrib = False;
    int i;
	for (i=0; i < m_nAttribItems; i++) {
		hint[i].m_th = m_nRules;
		hint[i].m_type = AB_SEARCH_ATTRIB;
		hint[i].m_menuItem = &(m_attrItems[i]);
		hint[i].m_obj = this;	
		if (attrib != kNumAttributes &&
			!validAttrib)
			if (hint[i].m_menuItem->attrib == (MSG_SearchAttribute) attrib)
				validAttrib = True;
	}/* for i */

	m_params[m_nRules].m_attribOptMenu = 
		makeOptMenu(m_params[m_nRules].m_ruleForm,
					m_nAttribItems, 
					hint,
					&(XFE_ABSearchView::optValChgCallback),
					&m_params[m_nRules].m_attribPopup);

	/* use whatever is there */
	if (!validAttrib) {
		/* correct 
		 */
		attrib = (MSG_SearchAttribute)
			m_attrItems[m_nRules%m_nAttribItems].attrib;
	}/* if */
	m_params[m_nRules].m_attribNval.attribute = attrib;

	setOptMenu(m_params[m_nRules].m_attribOptMenu, 
			   (uint16) attrib);

	XtVaSetValues(m_params[m_nRules].m_attribOptMenu,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, m_params[m_nRules].m_label,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);
	/* op menu
	 */
	uint16 opNum = 0;
	hint = getOptMenuHint(m_nRules, &opNum);
	m_params[m_nRules].m_opOptMenu = 
		makeOptMenu(m_params[m_nRules].m_ruleForm,
					opNum,
					hint,
					&(XFE_ABSearchView::optValChgCallback),
					&m_params[m_nRules].m_opPopup);

	/* compute op
	 */
	XP_Bool opValid = False;
	for (i=0; (op != kNumOperators) && (i < opNum); i++) {
		if (op == (MSG_SearchOperator) hint[i].m_menuItem->attrib) {
			opValid = True;
			break;
		}/* if */
	}/* for i */
	if (!opValid) {
		op = (MSG_SearchOperator) hint[0].m_menuItem->attrib;
	}/* if */
	m_params[m_nRules].m_op = op;
	setOptMenu(m_params[m_nRules].m_opOptMenu, op);

	XtVaSetValues(m_params[m_nRules].m_opOptMenu,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, m_params[m_nRules].m_attribOptMenu,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);
	/* TextF
	 */
	ac = 0;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNleftWidget, m_params[m_nRules].m_opOptMenu); ac++;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	//XtSetArg(av[ac], XmNwidth, 150); ac++;
	m_params[m_nRules].m_textF = fe_CreateTextField(m_params[m_nRules].m_ruleForm,
													"ruleTextVal",
													av, ac);
	XtAddCallback(m_params[m_nRules].m_textF, XmNactivateCallback, 
				  XFE_ABSearchView::textFActivateCallback, 
				  this);
	/* set def
	 */
	fe_SetTextField(m_params[m_nRules].m_textF, 
					stringVal?stringVal:"");
	m_params[i].m_attribNval.u.string = stringVal;

	XtManageChild(m_params[m_nRules].m_textF);
	XtManageChild(m_params[m_nRules].m_ruleForm);
	m_nRules++;

	XtVaSetValues(m_ruleCmdGroupForm, 
				  XmNpositionIndex, XmLAST_POSITION,
				  NULL);

}

char*
XFE_ABSearchView::getValByAttrib(ABSearchUIParam_t *params, 
								 int nParams, MSG_SearchAttribute attrib)
{
	for (int i=0; i < nParams; i++) {
		if (params[i].m_attribNval.attribute == attrib)
			return params[i].m_attribNval.u.string;
	}/* for i */
	return NULL;
}

void XFE_ABSearchView::toggleSearchMode(ABSearchInfo_t *info)
{
	XP_ASSERT(info && info->m_dir && m_rulesRowCol);
	XtUnmanageChild(m_workForm);

	/* pass params
	 */
	m_searchInfo = info;
	if (info->m_nRules &&
		info->m_params) {
		m_nRules = info->m_nRules;
		m_params = info->m_params;
	}/* if */

	m_dir = info->m_dir;

	/* to satisfy gcc
	 */
	char tmp[256];

	/* set title
	 */
	sprintf(tmp,
			"%s %s", 
			XP_GetString(XFE_SEARCH_DLG_PROMPT),
			m_dir->description?m_dir->description:"");
	
	fe_SetString(m_dirPrompt,
				 XmNlabelString, tmp);
	
	Arg av [20];
	int ac = 0;
	int i = 0;

	/* Buffer up
	 */
	int nVal = m_nRules;
	ABSearchUIParam_t *buffer =
		(ABSearchUIParam_t *) XP_CALLOC(m_nRules, 
										sizeof(ABSearchUIParam_t));
	buffer = 
		(ABSearchUIParam_t *) memcpy(buffer, m_params, 
									 m_nRules*sizeof(ABSearchUIParam_t));

	if (info->m_mode == AB_SEARCH_BASIC) {
		/* clean up residue
		 */
		fe_SetString(m_modeToggle, 
					 XmNlabelString, XP_GetString(XFE_SEARCH_ADVANCED));

		if (m_andOrForm)
			XtUnmanageChild(m_andOrForm);
		if (m_ruleCmdGroupForm)
			XtUnmanageChild(m_ruleCmdGroupForm);

		/* un map widgets
		 */
		for (i=0; i < m_nRules; i++)
			cleanupUI(i, False);

		XtVaSetValues(m_rulesRowCol,
					  XmNorientation, XmVERTICAL,
					  XmNnumColumns, 2,
					  XmNpacking, XmPACK_COLUMN,
					  NULL);

		/* get new attribs
		 */
		m_searchInfo->m_logicOp = True; //and
		m_nRules = kNumAttributes;
		MSG_SearchMenuItem *attrItems = 
			(MSG_SearchMenuItem *)XP_CALLOC(m_nRules, 	
											sizeof(MSG_SearchMenuItem));
#if 0
		/* Tao: do not take out this ....
		 */
		MSG_SearchError err = 
			MSG_GetBasicLdapSearchAttributes(m_dir, 
											 attrItems,
											 (int *) &m_nRules);
#endif
		if (!m_nRules ||
			!attrItems[0].name ||
			!XP_STRLEN(attrItems[0].name)) {
			m_nRules = 4;

			attrItems[0].attrib = attribCommonName;
			attrItems[1].attrib = attribOrganization;
			attrItems[2].attrib = attrib822Address;
			attrItems[3].attrib = attribOrgUnit;
			for (i=0; i < m_nRules; i++) {
				DIR_AttributeId id;
				MSG_SearchAttribToDirAttrib((MSG_SearchAttribute)attrItems[i].attrib, 
											&id);
				XP_STRCPY(attrItems[i].name, DIR_GetAttributeName(m_dir, id));
			}/* for i */
		}/* if */

		/* realloc if needed
		 */
		if (m_nRules > m_nAllocted) {
			m_params = (ABSearchUIParam_t *) 
				XP_REALLOC(m_params,
						   m_nRules*sizeof(ABSearchUIParam_t));

			int delta = m_nRules-m_nAllocted;
			memset(&(m_params[m_nAllocted]), 
				   0, 
				   delta*sizeof(ABSearchUIParam_t));
			m_nAllocted = m_nRules;
		}/* if */

		for (i=0; i < m_nRules; i++) {

			// set defaults
			m_params[i].m_op = opContains;
			m_params[i].m_attribNval.attribute = 
				(MSG_SearchAttribute) attrItems[i].attrib;
			m_params[i].m_attribNval.u.string = 
				getValByAttrib(buffer, nVal, 
							   m_params[i].m_attribNval.attribute);
			/* attrib name
			 */
			const char *text = attrItems[i].name;
			ac = 0;
			if (!m_params[i].m_ruleForm)
				m_params[i].m_ruleForm = XmCreateForm(m_rulesRowCol, 
													  "basicRulesForm", av, ac);
			/* Create labels
			 */
			m_params[i].m_label = 
				XtVaCreateManagedWidget(text,
										xmLabelGadgetClass, m_params[i].m_ruleForm,
										XmNalignment, XmALIGNMENT_BEGINNING, 
										XmNleftAttachment, XmATTACH_FORM,
										XmNtopAttachment, XmATTACH_FORM,
										XmNrightAttachment, XmATTACH_NONE,
										XmNbottomAttachment, XmATTACH_NONE,
										NULL);
			/* TextF
			 */
			ac = 0;
			XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
			XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
			XtSetArg(av[ac], XmNtopWidget, m_params[i].m_label); ac++;
			XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
			XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
			//XtSetArg(av[ac], XmNwidth, 180); ac++;
			m_params[i].m_textF = fe_CreateTextField(m_params[i].m_ruleForm, 
													 text,
													 av, ac);
			/* set default
			 */
			if (m_params[i].m_attribNval.u.string)
				fe_SetTextField(m_params[i].m_textF, 
								m_params[i].m_attribNval.u.string);	
			else
				fe_SetTextField(m_params[i].m_textF, 
								"");	

			XtAddCallback(m_params[i].m_textF, XmNactivateCallback, 
						  XFE_ABSearchView::textFActivateCallback, 
						  this);
			XtManageChild(m_params[i].m_textF);
			XtManageChild(m_params[i].m_ruleForm);
		}/* for i */
		XP_FREEIF(attrItems);
	}/* if */
	else {

		/* AB_SEARCH_ADVANCED
		 * clean up residue
		 */
		fe_SetString(m_modeToggle, 
					 XmNlabelString, XP_GetString(XFE_SEARCH_BASIC));
		/* un map
		 */
		for (i=0; i < m_nRules; i++)
			cleanupUI(i, False);

		XtVaSetValues(m_rulesRowCol,
					  XmNorientation, XmVERTICAL,
					  XmNnumColumns, 1,
					  XmNpacking, XmPACK_COLUMN,
					  NULL);

		/* free heap & basic widget .....
		 */


		/* get attribs for this scope
		 */
		if (!m_andOrForm) {
			ac = 0;
			XtSetArg(av[ac], XmNpositionIndex, 0); ac++;
			m_andOrForm =
				XmCreateForm(m_rulesRowCol, "andOrForm", av, ac);

			ABSearchUIHint_t *hint = 
				(ABSearchUIHint_t *) XP_CALLOC(2,
											   sizeof(ABSearchUIHint_t));
			/* and, or
			 */
            int i;
			for (i=0; i < 2; i++) {
				hint[i].m_th = -1;
				hint[i].m_type = AB_SEARCH_CONNECTOR;
				hint[i].m_menuItem = 
					(MSG_SearchMenuItem *) XP_CALLOC(1, sizeof(MSG_SearchMenuItem));
				hint[i].m_menuItem->attrib = (int16) ((i==0)?True:False);
				XP_SAFE_SPRINTF(hint[i].m_menuItem->name,  
								 sizeof(hint[i].m_menuItem->name),
								 "%s",
								XP_GetString(XFE_SEARCH_BOOL_AND_PROMPT+i));
				hint[i].m_menuItem->isEnabled = True;
				hint[i].m_obj = this;		
			}/* for i */

			m_andOrOptMenu =
				makeOptMenu(m_andOrForm,
							2,
							hint,
							&(XFE_ABSearchView::optValChgCallback),
							&m_andOrPopup);
			
			
			XtVaSetValues(m_andOrOptMenu,
						  XmNleftAttachment, XmATTACH_FORM,
						  XmNtopAttachment, XmATTACH_FORM,
						  XmNrightAttachment, XmATTACH_NONE,
						  XmNbottomAttachment, XmATTACH_FORM,
						  NULL);
			/* where
			 */
			Widget label = XtVaCreateManagedWidget(XP_GetString(XFE_SEARCH_BOOL_WHERE),
												   xmLabelGadgetClass, m_andOrForm,
												   XmNleftAttachment, XmATTACH_WIDGET,
												   XmNleftWidget, m_andOrOptMenu,
												   XmNtopAttachment, XmATTACH_FORM,
												   XmNrightAttachment, XmATTACH_NONE,
												   XmNbottomAttachment, XmATTACH_FORM,
												   NULL);


		}/* if */
		/* use whatever is there */
		MSG_SearchAttribute attrib = 
			(MSG_SearchAttribute) info->m_logicOp;			
		setOptMenu(m_andOrOptMenu, (uint16) attrib);
		XtManageChild(m_andOrForm);

		/* get attribs for this scope
		 */
		if (!m_ruleCmdGroupForm) {
			/*  cmd group:form,  more/fewer/clear
			 */
			ac = 0;
			XtSetArg(av[ac], XmNpositionIndex, XmLAST_POSITION); ac++;
			m_ruleCmdGroupForm = 
				XmCreateForm(m_rulesRowCol, "ruleCmdGroupForm", av, ac);

			ac = 0;
			XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
			XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
			XtSetArg(av[ac], XmNrightAttachment, XmATTACH_NONE); ac++;
			XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
			m_moreBtn = XmCreatePushButton(m_ruleCmdGroupForm,
										   XP_GetString(XFE_SEARCH_MORE), 
										   av, ac);
			ac = 0;
			XtSetArg(av[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
			XtSetArg(av[ac], XmNleftWidget, m_moreBtn); ac++;
			XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
			XtSetArg(av[ac], XmNrightAttachment, XmATTACH_NONE); ac++;
			XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
			m_fewerBtn = XmCreatePushButton(m_ruleCmdGroupForm,
											XP_GetString(XFE_SEARCH_FEWER), 
											av, ac);
			ac = 0;
			XtSetArg(av[ac], XmNleftAttachment, XmATTACH_NONE); ac++;
			XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
			XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
			XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
			m_clearBtn = XmCreatePushButton(m_ruleCmdGroupForm,
											XP_GetString(XFE_DLG_CLEAR), 
											av, ac);
			XtManageChild(m_moreBtn);
			XtManageChild(m_fewerBtn);
			XtManageChild(m_clearBtn);
			XtManageChild(m_ruleCmdGroupForm);
			//
			XtAddCallback(m_moreBtn, XmNactivateCallback, 
						  XFE_ABSearchView::moreCallback, 
						  this);
			XtAddCallback(m_fewerBtn, XmNactivateCallback, 
						  XFE_ABSearchView::fewerCallback, 
						  this);
			XtAddCallback(m_clearBtn, XmNactivateCallback, 
						  XFE_ABSearchView::clearCallback, 
						  this);
			

		}/* if !m_ruleCmdGroupForm */		

		uint16 existing = m_nRules;
		m_nRules = 0;
		for (i=0; i < existing; i++) {
			if (!m_params[i].m_attribNval.u.string ||
				!XP_STRLEN(m_params[i].m_attribNval.u.string))
				continue;
			add1Rule(m_params[i].m_attribNval.attribute,
					 m_params[i].m_op,
					 m_params[i].m_attribNval.u.string);
		}/* for i */
		if (!m_nRules)
			add1Rule();

		XtSetSensitive(m_fewerBtn, (m_nRules <= 1)?False:True);

		XtVaSetValues(m_ruleCmdGroupForm, 
					  XmNpositionIndex, XmLAST_POSITION,
					  NULL);
		XtManageChild(m_ruleCmdGroupForm);
			
	}/* else */
	XP_FREEIF(buffer);
	XtManageChild(m_workForm);
}

void XFE_ABSearchView::getParams()
{
	for (int i=0; i < m_nRules; i++) {
		if (!m_params[i].m_textF)
			/* meaningless
			 */
			break;

		m_params[i].m_attribNval.u.string = 
			fe_GetTextField(m_params[i].m_textF);		
	}/* for i */
	m_searchInfo->m_nRules = m_nRules;
	m_searchInfo->m_params = m_params;
}

ABSearchUIHint_t*
XFE_ABSearchView::getOptMenuHint(uint16 th, uint16 *num)
{
	/* op
	 */
	DIR_Server *dirs[1];
	dirs[0] = (DIR_Server*) m_dir;

	MSG_SearchMenuItem *opItems = 
		(MSG_SearchMenuItem *) XP_CALLOC(kNumOperators, 
										 sizeof(MSG_SearchMenuItem));		
	uint16 opNum = kNumOperators;

	MSG_SearchAttribute attrib = 
		m_params[th].m_attribNval.attribute;
	
	MSG_SearchError err;
  	err = MSG_GetOperatorsForSearchScopes(fe_getMNMaster(), 
										  scopeLdapDirectory, 
										  (void**)dirs, 
										  1,
										  attrib,
										  opItems, 
										  &opNum);
	*num = opNum;
	
	/* op menu
	 */
	ABSearchUIHint_t *hint = 
		(ABSearchUIHint_t *) XP_CALLOC(opNum, 
									   sizeof(ABSearchUIHint_t));
	for (int i=0; i < opNum; i++) {
		hint[i].m_th = th;
		hint[i].m_type = AB_SEARCH_OP;
		hint[i].m_menuItem = & opItems[i];
		hint[i].m_obj = this;		
	}/* for i */
	return hint;
}

void XFE_ABSearchView::setParams(ABSearchInfo_t *info)
{
	toggleSearchMode(info);	
}

// CBs
void XFE_ABSearchView::optValChgCallback(Widget w, 
										 XtPointer clientData, 
										 XtPointer callData)
{
	XFE_ABSearchView *obj = (XFE_ABSearchView *) clientData;
	obj->optValChgCB(w, callData);
}

void XFE_ABSearchView::optValChgCB(Widget w, 
								   XtPointer /* callData */)
{
	ABSearchUIHint_t *hint = NULL;
    XtVaGetValues(w, 
				  XmNuserData, &hint,
				  NULL);
	XP_ASSERT(hint);

	if (hint->m_type == AB_SEARCH_ATTRIB) {
		m_params[hint->m_th].m_attribNval.attribute = 
			(MSG_SearchAttribute) hint->m_menuItem->attrib;

		/* rebuild op
		 */
		uint16 opNum = 0;
		ABSearchUIHint_t *Hint = getOptMenuHint(hint->m_th, &opNum);
		Widget btn1 = 
			rebuildOptChildren(m_params[hint->m_th].m_opPopup, 
							   opNum, 
							   Hint,
							   &(XFE_ABSearchView::optValChgCallback));
		XtVaSetValues(m_params[hint->m_th].m_opOptMenu, 
					  XmNmenuHistory, btn1, 
					  NULL);
		m_params[hint->m_th].m_op = 
			(MSG_SearchOperator) Hint[0].m_menuItem->attrib;
	}/* if */
	else if (hint->m_type == AB_SEARCH_OP)
		m_params[hint->m_th].m_op = 
			(MSG_SearchOperator) hint->m_menuItem->attrib;
	else if (hint->m_type == AB_SEARCH_CONNECTOR) {
		m_searchInfo->m_logicOp = (XP_Bool) hint->m_menuItem->attrib;
		if (m_nRules > 1) {
			char tmp[256];
			sprintf(tmp,
					"%s", 
					(m_searchInfo->m_logicOp?
					 XP_GetString(XFE_SEARCH_BOOL_AND_THE):
					 XP_GetString(XFE_SEARCH_BOOL_OR_THE)));
			for (int i=1; i < m_nRules; i++) {
				fe_SetString(m_params[i].m_label, 
							 XmNlabelString, tmp);
			}/* for i */
		}/* if */
	}/* else if */

}

void XFE_ABSearchView::toggleSearchModeCallback(Widget w, 
										 XtPointer clientData, 
										 XtPointer callData)
{
	XFE_ABSearchView *obj = (XFE_ABSearchView *) clientData;
	obj->toggleSearchModeCB(w, callData);
}

void XFE_ABSearchView::toggleSearchModeCB(Widget /* w */, 
										  XtPointer /* callData */)
{
	m_searchInfo->m_mode = 
		(m_searchInfo->m_mode==AB_SEARCH_ADVANCED)
		?AB_SEARCH_BASIC
		:AB_SEARCH_ADVANCED;

	/* get input
	 */
	getParams();
	toggleSearchMode(m_searchInfo); 
}

void XFE_ABSearchView::okCallback(Widget w, 
										 XtPointer clientData, 
										 XtPointer callData)
{
	XFE_ABSearchView *obj = (XFE_ABSearchView *) clientData;
	obj->okCB(w, callData);
}

void XFE_ABSearchView::okCB(Widget /* w */, 
							XtPointer /* callData */)
{
	cancelCB(NULL, NULL);
	((ABSearchCBProc) m_searchInfo->m_cbProc)(m_searchInfo, NULL);	
}

void XFE_ABSearchView::cancelCallback(Widget w, 
										 XtPointer clientData, 
										 XtPointer callData)
{
	XFE_ABSearchView *obj = (XFE_ABSearchView *) clientData;
	obj->cancelCB(w, callData);
}

void XFE_ABSearchView::cancelCB(Widget /* w */, 
								XtPointer /* callData */)
{
	getParams();
	for (int i=0; i < m_nRules; i++) {
		m_params[i].m_ruleForm = NULL;
		m_params[i].m_attribOptMenu = NULL;
		m_params[i].m_attribPopup = NULL;
		m_params[i].m_opOptMenu = NULL;
		m_params[i].m_opPopup = NULL;
		m_params[i].m_label = NULL;
		m_params[i].m_textF = NULL;
	}/* for i */
	notifyInterested(XFE_ABSearchView::dlgClose);
}

void XFE_ABSearchView::moreCallback(Widget w, 
										 XtPointer clientData, 
										 XtPointer callData)
{
	XFE_ABSearchView *obj = (XFE_ABSearchView *) clientData;
	obj->moreCB(w, callData);
}

void XFE_ABSearchView::moreCB(Widget /* w */, 
										  XtPointer /* callData */)
{
	add1Rule();
	XtSetSensitive(m_fewerBtn, (m_nRules <= 1)?False:True);
}

void XFE_ABSearchView::fewerCallback(Widget w, 
										 XtPointer clientData, 
										 XtPointer callData)
{
	XFE_ABSearchView *obj = (XFE_ABSearchView *) clientData;
	obj->fewerCB(w, callData);
}

void XFE_ABSearchView::fewerCB(Widget /* w */, 
										  XtPointer /* callData */)
{
	remove1Rule();
	XtSetSensitive(m_fewerBtn, (m_nRules <= 1)?False:True);
}

void XFE_ABSearchView::clearCallback(Widget w, 
										 XtPointer clientData, 
										 XtPointer callData)
{
	XFE_ABSearchView *obj = (XFE_ABSearchView *) clientData;
	obj->clearCB(w, callData);
}

void XFE_ABSearchView::clearCB(Widget /* w */, 
										  XtPointer /* callData */)
{
	/* simply clear textF
	 */
	for (int i=0; i < m_nRules; i++) {
		XP_FREEIF(m_params[i].m_attribNval.u.string);
		fe_SetTextField(m_params[i].m_textF, 
						"");		
	}/* for i */
}

void XFE_ABSearchView::textFActivateCallback(Widget w, 
										 XtPointer clientData, 
										 XtPointer callData)
{
	XFE_ABSearchView *obj = (XFE_ABSearchView *) clientData;
	obj->textFActivateCB(w, callData);
}

void XFE_ABSearchView::textFActivateCB(Widget /* w */, 
									   XtPointer /* clientData */)
{
	okCB(NULL, NULL);
}

