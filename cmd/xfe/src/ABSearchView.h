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
   ABSearchView.h -- class definition for XFE_ABSearchView
   Created: Tao Cheng <tao@netscape.com>, 23-oct-97
 */

#ifndef _ABSEARCHVIEW_H_
#define _ABSEARCHVIEW_H_

#include "MNView.h"

#include "dirprefs.h"
#include "msg_srch.h"

typedef enum {AB_SEARCH_NONE = 0,
			  AB_SEARCH_BASIC,
			  AB_SEARCH_ADVANCED
} eABSearchMode;

typedef struct {
	MSG_SearchOperator  m_op;
	MSG_SearchValue     m_attribNval;
	XP_Bool             m_logicOp;

	Widget              m_ruleForm;

	Widget              m_attribOptMenu;
	Widget              m_attribPopup;
	Widget              m_opOptMenu;
	Widget              m_opPopup;

	Widget              m_label;
	Widget              m_textF;
} ABSearchUIParam_t;

typedef struct _ABSearchInfo_t{	
	void              *m_obj;
	void              *m_cbProc;

	DIR_Server        *m_dir;
	eABSearchMode      m_mode;
	uint16             m_nRules;
	ABSearchUIParam_t *m_params;
	XP_Bool            m_logicOp;
} ABSearchInfo_t;

typedef enum {AB_SEARCH_ATTRIB = 0,
			  AB_SEARCH_OP,
			  AB_SEARCH_VAL,
			  AB_SEARCH_CONNECTOR
} eABSearchMenuType;

typedef struct {
	int                 m_th;
	eABSearchMenuType   m_type;
	MSG_SearchMenuItem *m_menuItem;
	void               *m_obj;
} ABSearchUIHint_t;

typedef void (*ABSearchCBProc)(ABSearchInfo_t    *clientData, 
							   void              *callData);

class XFE_ABSearchView : public XFE_MNView
{

public:
	XFE_ABSearchView(XFE_Component  *toplevel_component,
					 Widget          parent,
					 MWContext      *context,
					 ABSearchInfo_t *info);
	virtual ~XFE_ABSearchView();

	void        setParams(ABSearchInfo_t *info);

	/* CB
	 */ 
	static void textFActivateCallback(Widget w, XtPointer clientData, 
								  XtPointer callData);
	static void okCallback(Widget w, XtPointer clientData, 
								  XtPointer callData);
	static void cancelCallback(Widget w, XtPointer clientData, 
								  XtPointer callData);
	static void toggleSearchModeCallback(Widget w, 
										 XtPointer clientData, 
										 XtPointer callData);

	static void moreCallback(Widget w, XtPointer clientData, 
								  XtPointer callData);
	static void fewerCallback(Widget w, XtPointer clientData, 
								  XtPointer callData);
	static void clearCallback(Widget w, XtPointer clientData, 
								  XtPointer callData);
	static void optValChgCallback(Widget w, XtPointer clientData, 
								  XtPointer callData);

	//
	static const char *dlgClose; // cancel btn is hit.

protected:
	void                textFActivateCB(Widget w, XtPointer callData);
	void                okCB(Widget w, XtPointer callData);
	void                cancelCB(Widget w, XtPointer callData);
	void                toggleSearchModeCB(Widget w, 
										   XtPointer callData);

	void                moreCB(Widget w, XtPointer callData);
	void                fewerCB(Widget w, XtPointer callData);
	void                clearCB(Widget w, XtPointer callData);
 
	void                optValChgCB(Widget w, XtPointer callData);
	//
	void                toggleSearchMode(ABSearchInfo_t *info);
	Widget              makeOptMenu(Widget            parent, 
									int               num,
									ABSearchUIHint_t *hint,
									XtCallbackProc    cb,
									Widget           *w);
	Widget              rebuildOptChildren(Widget            parent,
										   int               num, 
										   ABSearchUIHint_t *hint,
										   XtCallbackProc    cb);


	//
	void                getAttribNames();

	void                add1Rule(MSG_SearchAttribute  a = kNumAttributes,
								 MSG_SearchOperator   op = kNumOperators,
								 char                *string = NULL);

	void                remove1Rule();

	void                getParams();
	ABSearchUIHint_t   *getOptMenuHint(uint16 th, uint16 *num);

	char*               getValByAttrib(ABSearchUIParam_t *params, 
									   int nParams, MSG_SearchAttribute attrib);
private:

	DIR_Server         *m_dir;

	eABSearchMode       m_mode;
	ABSearchInfo_t     *m_searchInfo;
	Widget              m_rulesRowCol;
	Widget              m_searchRulesPrompt;
	Widget              m_dirPrompt;
	Widget              m_workForm;

	ABSearchUIParam_t  *m_params;
	uint16              m_nRules;
	uint16              m_nAllocted;

	Widget              m_modeToggle;

	MSG_SearchMenuItem *m_attrItems;
	uint16              m_nAttribItems;

	Widget              m_ruleCmdGroupForm;
	Widget              m_moreBtn;
	Widget              m_fewerBtn;	
	Widget              m_clearBtn;	

	Widget              m_andOrForm;
	Widget              m_andOrOptMenu;
	Widget              m_andOrPopup;

	void cleanupUI(uint16 i, XP_Bool clearRule);
	void setOptMenu(Widget w, int16 attrib);
};

#endif /* _ABSEARCHVIEW_H_ */
