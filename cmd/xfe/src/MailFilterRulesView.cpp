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
   MailFilterRulesView.h -- view of user's mailfilters.
   Created: Chris Toshok <toshok@netscape.com>, 29-Aug-96.
 */



#include "MailFilterRulesView.h"

#include "EditHdrDialog.h"
#include "FolderPromptDialog.h"
#include "ViewGlue.h"

#include <Xm/ArrowBG.h>
#include <Xm/ToggleB.h>
#include <Xfe/Xfe.h>

#include "xpgetstr.h"
#include "felocale.h"
#include "xp_time.h"
#include "xplocale.h"

const char *XFE_MailFilterRulesView::listChanged = "XFE_MailFilterRulesView::listChanged";
extern "C"
MSG_SearchError MSG_GetValuesForAttribute( MSG_ScopeAttribute scope, 
										   MSG_SearchAttribute attrib, 
										   MSG_SearchMenuItem *items, 
										   uint16 *maxItems);

/* C++ code
 */
XFE_MailFilterRulesView::XFE_MailFilterRulesView(XFE_Component *toplevel_component,
												 /* the dialog form */
												 Widget         parent, 
												 XFE_View      *parent_view,
												 MWContext     *context,
												 XP_Bool        isNew) 
	: XFE_MNView(toplevel_component, parent_view, context, (MSG_Pane *) NULL),
	  m_folderDropDown(0),
	  m_isNew(isNew)
{
	/* set base widget
	 */
	setBaseWidget(parent);

    m_booleanAnd = TRUE;

    m_booleanAnd = TRUE;

    /* Create main filter list dialog
     */
	m_filterlist = 0;
	m_dialog = XtParent(parent);
#ifdef DEBUG_tao_
	if (XmIsDialogShell(m_dialog)) {
		printf("\n XFE_MailFilterRulesView: parent is not dialog shell\n");
	}/* if */
#endif

	Widget    dummy;
	Arg	      av[30];
	Cardinal  ac;
	Dimension width, height;
  
	Widget content = parent;
	Widget frame, frameForm, frameTitle;
	Widget popupW, thenLabel;
	Widget lessbtn, morebtn;
	Widget filterLabel, despLabel, statusRadioBox;
    
    /* Filter name, textF
     */
    ac = 0;
    XtSetArg (av [ac], XmNalignment, XmALIGNMENT_END); ac++;
    XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNrightAttachment, XmATTACH_NONE); ac++;
    filterLabel = XmCreateLabelGadget(content, "filterLabel", av, ac);
    
    ac = 0;
    XtSetArg (av [ac], XmNcolumns, 15); ac++;
    XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg (av [ac], XmNleftWidget, filterLabel); ac++;
    m_filterName = fe_CreateTextField(content,"filterName",av,ac);

    /* Description goes underneath filter name: */
    ac = 0;
    XtSetArg(av[ac], XmNalignment, XmALIGNMENT_END); ac++;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(av[ac], XmNtopWidget, m_filterName); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    despLabel = XmCreateLabelGadget(content, "despLabel", av, ac);

    ac = 0;
    XtSetArg(av[ac], XmNrows, 4); ac++;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(av[ac], XmNtopWidget, despLabel); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	m_despField = fe_CreateText(content, "despField", av, ac);
    
	XP_ASSERT(m_despField && despLabel && m_filterName && filterLabel);
    XtManageChild(m_filterName);
    XtManageChild(filterLabel);
    XtManageChild(m_despField);
    XtManageChild(despLabel);

    ac=0;
    XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg (av [ac], XmNtopWidget, m_despField); ac++;
    XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    frame = XmCreateFrame(content, "rulesFrame", av, ac);

    ac=0;
    XtSetArg(av[ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
    frameTitle = XmCreateLabelGadget(frame, "frameTitle", av, ac);
    XtManageChild(frameTitle);
    
    ac=0;
    frameForm = XmCreateForm(frame, "frameForm", av, ac);

    Widget popup;
	m_optionAndOr = make_option_menu(frameForm, "optionAndOr", &popup);
    XtVaSetValues(m_optionAndOr,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_FORM,
                  0);

    m_optionAnd = XmCreatePushButtonGadget(popup, "matchAll", 0, 0);
    XtAddCallback(m_optionAnd, XmNactivateCallback, andOr_cb, this);
    XtManageChild(m_optionAnd);
    m_optionOr = XmCreatePushButtonGadget(popup, "matchAny", 0, 0);
    XtAddCallback(m_optionOr, XmNactivateCallback, andOr_cb, this);
    XtManageChild(m_optionOr);

    XtManageChild(m_optionAndOr);

    ac = 0;
    XtSetArg (av [ac], XmNorientation, XmVERTICAL); ac++;
    XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg (av [ac], XmNtopWidget, m_optionAndOr); ac++;
    XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    m_rulesRC = XmCreateRowColumn(frameForm, "rulesRC", av, ac);

    //
    // The command group holds the "more" and "less" buttons, nothing else
    //
    ac = 0;
    XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg (av [ac], XmNtopWidget, m_rulesRC); ac++;
    XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    m_commandGroup = XmCreateForm(frameForm, "commandGrp", av, ac );
  	
    /* More button
     */
    ac = 0;
    XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    morebtn = XmCreatePushButton(m_commandGroup, "more", av, ac);
    XtAddCallback(morebtn, 
				  XmNactivateCallback, XFE_MailFilterRulesView::addRow_cb,
				  this);
  	
    /* Less button
     */
    XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg (av [ac], XmNleftWidget, morebtn); ac++;
    lessbtn = XmCreatePushButton(m_commandGroup, "fewer", av, ac);
    XtAddCallback(lessbtn, 
				  XmNactivateCallback, XFE_MailFilterRulesView::delRow_cb, 
				  this);

  	XP_ASSERT(morebtn && lessbtn);
    XtManageChild(morebtn);
    XtManageChild(lessbtn);
  	
    /* If structure; make one rule row
     */
    m_content[0] = 0;
	makeRules(m_rulesRC, 1);
    m_stripCount = 1;

    m_content[1] = 0;
    m_content[2] = 0;
    m_content[3] = 0;
    m_content[4] = 0;
    
  	XP_ASSERT(m_rulesRC && m_commandGroup);
    XtManageChild(m_rulesRC);
    XtManageChild(m_commandGroup);

    /* Then structure
     */    
    ac = 0;
    XtSetArg (av [ac], XmNalignment, XmALIGNMENT_END); ac++;
    XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg (av [ac], XmNtopWidget, m_commandGroup); ac++;
    XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    thenLabel = XmCreateLabelGadget(frameForm, "thenLabel", av, ac);

    /* create "then" clause option menu */  	
    ac = 0;
    popupW=0;
	m_thenClause = make_option_menu(frameForm, "thenClause", &popupW);
    XtVaSetValues(m_thenClause,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, m_commandGroup,
                  XmNleftAttachment, XmATTACH_FORM,
                  0);
    dummy = make_actPopup(popupW,
						  &width, &height, 
						  0, XFE_MailFilterRulesView::thenClause_cb);
    m_thenClausePopup = popupW;    

    /* 
	 * create the "Move to folder.." option menu 
	 * which is a list of folders 
	 */
	/* hook up drop down 
	 */
	m_folderDropDown = new	XFE_FolderDropdown(toplevel_component,
											   frameForm,
											   FALSE, /* allowServerSelection */
											   FALSE /* showNewsgroups */);
	m_folderDropDown->setPopupServer(False);;

	/* we need to select the inbox initially, so if we later try to select
	   by name and the named folder isn't present, we aren't left in a state
	   where we're selecting the mail server -- since this can cause a crash
	   in the mail filter code. */
	{
		MSG_FolderInfo *inbox_info = NULL;

		if (MSG_GetFoldersWithFlag(fe_getMNMaster(),
								   MSG_FOLDER_FLAG_INBOX,
								   &inbox_info, 1) > 0
			&& inbox_info)
			{
				m_folderDropDown->selectFolder(inbox_info);
			}
	}

	m_folderDropDown->show();
	m_thenTo = m_folderDropDown->getBaseWidget();

    XtVaSetValues(m_thenTo,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, m_commandGroup,
				  XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNbottomWidget, m_thenClause,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, m_thenClause,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNtopOffset, 3,
				  XmNleftOffset, 3,
				  XmNbottomOffset, 3,
				  0);

    // Create a New Folder button, only visible when action is Move to Folder
    XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg (av [ac], XmNtopWidget, m_commandGroup); ac++;
    XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
    XtSetArg (av [ac], XmNbottomWidget, m_thenClause); ac++;
    XtSetArg (av [ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg (av [ac], XmNleftWidget, m_thenTo); ac++;
    m_newFolderBtn = XmCreatePushButton(frameForm, "newFolder", av, ac);
    XtAddCallback(m_newFolderBtn,
				  XmNactivateCallback, XFE_MailFilterRulesView::newFolder_cb, 
				  this);
  	
    /* this is the priority action menu 
     */
    /* list of priority destinations 
     */
    ac = 0;
    popupW=0;
    m_priAction = make_option_menu(frameForm, 
								   "priAction", &popupW);
    dummy = makeopt_priority(popupW, &width, &height, 1);
    m_priActionPopup = popupW;
  	
	XP_ASSERT(thenLabel && m_thenClause && m_thenTo);
    XtManageChild(thenLabel);
    XtManageChild(m_thenClause);
    XtManageChild(m_thenTo);

    XtManageChild(frameForm);
    XtManageChild(frame);

	/* Filter is...
     */
    ac = 0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg (av [ac], XmNleftWidget, m_filterName); ac++;
    XtSetArg (av [ac], XmNleftOffset, 50); ac++;
    Widget statusLabel = XmCreateLabelGadget(content, "statusLabel", av, ac);
 	
    ac = 0;
    XtSetArg (av [ac], XmNorientation, XmHORIZONTAL); ac++;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg (av [ac], XmNleftWidget, statusLabel); ac++;
    statusRadioBox = XmCreateRadioBox(content, "radioBox", av, ac);
  	
    /* On/Off
     */
    m_filterOnBtn = XtVaCreateManagedWidget("on", xmToggleButtonGadgetClass, 
											statusRadioBox, 
											NULL);
    XtAddCallback(m_filterOnBtn, 
				  XmNvalueChangedCallback, XFE_MailFilterRulesView::turnOnOff,
				  this);

    m_filterOffBtn = XtVaCreateManagedWidget("off", xmToggleButtonGadgetClass, 
											 statusRadioBox, 
											 NULL);
	XtAddCallback(m_filterOffBtn, 
				  XmNvalueChangedCallback, XFE_MailFilterRulesView::turnOnOff,
				  this);


	XP_ASSERT(statusLabel && statusRadioBox);
    XtManageChild(statusLabel);
    XtManageChild(statusRadioBox);

    XtVaGetValues(despLabel, XmNwidth, &width, 0);
    XtVaSetValues(thenLabel, XmNwidth, width, 0 );
    XtVaSetValues(m_commandGroup, XmNleftOffset, width, 0);
    XtVaSetValues(m_thenClause, XmNleftOffset, width, 0);

    m_despwidth = width;
	XP_ASSERT(content);
    XtManageChild(content);

	/* here we determine if the entry selected is java script */
	XP_ASSERT(m_rulesRC && thenLabel && m_thenTo && m_thenClause);
	XtManageChild(m_rulesRC);

#if defined(DEBUG_tao_)
	height = XfeHeight(thenLabel);
	printf("\n thenLabel, HEI=%d\n", height);
	height = XfeHeight(m_thenClause);
	printf("\n m_thenClause, HEI=%d\n", height);
	height = XfeHeight(m_thenTo);
	printf("\n m_thenTo, HEI=%d\n", height);
#endif
    XtVaSetValues(m_thenTo,
				  XmNheight, height,
 				  0);

}

// callbacks
XFE_MailFilterRulesView::~XFE_MailFilterRulesView()
{
}

void XFE_MailFilterRulesView::setDlgValues(MSG_FilterList *list,
										   MSG_FilterIndex at)
{
	m_filterlist = list; 
	m_at = at;

	if (m_isNew) {
		resetData();
	}/* if */
	else {
		displayData();
	}/* else */

}/* XFE_MailFilterRulesView::setDlgValues() */


void XFE_MailFilterRulesView::apply()
{
	if (m_isNew) {
		newokCB(NULL, NULL);

		/* the rules dlg 
		 */
		getToplevel()->notifyInterested(XFE_MailFilterRulesView::listChanged,
										(void *) -1);
	}/* if */
	else {
		editokCB(NULL, NULL);

		/* the rules dlg 
		 */
		getToplevel()->notifyInterested(XFE_MailFilterRulesView::listChanged,
										(void *) m_at);
	}/* else */
}

void XFE_MailFilterRulesView::cancel()
{
}


/* this function get the rule data specified by the user 
 */
void
XFE_MailFilterRulesView::getTerm(int num, 
								 MSG_SearchAttribute *attrib, 
								 MSG_SearchOperator *op, 
								 MSG_SearchValue *value,
                                 char* customHdr)
{
	fe_mailfilt_userData *userData;
	Widget w;

	w=0;
	XtVaGetValues(m_scopeOpt[num], XmNmenuHistory, &w, NULL);
	XtVaGetValues(w, XmNuserData, &userData, 0);
	*attrib = (MSG_SearchAttribute) userData->attrib;

	w=0;
	XtVaGetValues(m_whereOpt[num], XmNmenuHistory, &w, NULL);
	XtVaGetValues(w, XmNuserData, &userData, 0);
	*op = (MSG_SearchOperator) userData->attrib;


	if (*attrib == attribPriority) {                      /* priority */
		value->attribute = *attrib;
		w=0;
		XtVaGetValues(m_priLevel[num], XmNmenuHistory, &w, NULL);
		XtVaGetValues(w, XmNuserData, &userData, 0);
		value->u.priority = (MSG_PRIORITY) userData->attrib;
	} 
	else if (*attrib == attribMsgStatus) {                      /* priority */
		value->attribute = *attrib;
		w=0;
		XtVaGetValues(m_statusLevel[num], XmNmenuHistory, &w, NULL);
		XtVaGetValues(w, XmNuserData, &userData, 0);
		value->u.msgStatus = (MSG_PRIORITY) userData->attrib;
	} 
	else if (*attrib == attribDate) {                   /* Date */
		char *string = fe_GetTextField(m_scopeText[num]);
		value->attribute = *attrib;
		value->u.date = XP_ParseTimeString(string, False);
#if defined(DEBUG_tao)
		// verify
		char dateStr[40];
		time_t entered = value->u.date;
		XP_StrfTime(m_contextData, 
					dateStr, sizeof(dateStr), 
					XP_DATE_TIME_FORMAT,
					localtime(&entered));

		printf("\n***User enter, XP_ParseTimeString, date=%s=%ld=%s", 
			   string, value->u.date, dateStr);

#endif
		XtFree(string);
	} 
	else if (*attrib == attribOtherHeader) {            /* Custom Header */
		value->attribute = *attrib;
        value->u.string = fe_GetTextField(m_scopeText[num]);

        // and get the value of the custom header (ugh!)
        Widget curW;
        XtVaGetValues(m_scopeOptPopup[num],
                      XmNmenuHistory, &curW,
                      0);
        XmString xmstr;
        XtVaGetValues(curW, XmNlabelString, &xmstr, 0);
        char* labl;
        XmStringGetLtoR(xmstr, XmSTRING_DEFAULT_CHARSET, &labl);
        XP_STRCPY(customHdr, labl);
        XtFree(labl);
#ifdef DEBUG_akkana
        printf("Line %d had custom header of %s, value %s\n",
               num, customHdr, value->u.string);
#endif
    }
	else if (*attrib == attribOtherHeader) {            /* Custom Header */
		value->attribute = *attrib;
        value->u.string = fe_GetTextField(m_scopeText[num]);

        // and get the value of the custom header (ugh!)
        Widget curW;
        XtVaGetValues(m_scopeOptPopup[num],
                      XmNmenuHistory, &curW,
                      0);
        XmString xmstr;
        XtVaGetValues(curW, XmNlabelString, &xmstr, 0);
        char* labl;
        XmStringGetLtoR(xmstr, XmSTRING_DEFAULT_CHARSET, &labl);
        XP_STRCPY(customHdr, labl);
        XtFree(labl);
#ifdef DEBUG_akkana
        printf("Line %d had custom header of %s, value %s\n",
               num, customHdr, value->u.string);
#endif
    }
	else {                                              /* everything else */
		value->attribute = *attrib;
        value->u.string = fe_GetTextField(m_scopeText[num]);
	}
}


void
XFE_MailFilterRulesView::getAction(MSG_RuleActionType *action, 
								   void **value) 
{
	Widget w;
	fe_mailfilt_userData *userData;
	const char *this_folder_name;
	MSG_FolderInfo *info;

	w=0;
	XtVaGetValues(m_thenClause, XmNmenuHistory, &w, NULL);
	XtVaGetValues(w, XmNuserData, &userData, 0);
	*action = (MSG_RuleActionType) userData->attrib;
	switch (*action) {
	case acMoveToFolder:
		info = m_folderDropDown->getSelectedFolder();
		this_folder_name = MSG_GetFolderNameFromID(info);
		*value = (void *) this_folder_name;
		break;

	case acChangePriority:
		w=0;
		XtVaGetValues(m_priAction, XmNmenuHistory, &w, NULL);
		XtVaGetValues(w, XmNuserData, &userData, 0);
		*value = (void *) userData->attrib;
		break;
		
	default:
		*value = NULL;
		break;
	}/* switch*/
}

void
XFE_MailFilterRulesView::get_option_size(Widget optionMenu, 
										 Widget btn,
										 Dimension *retWidth, 
										 Dimension *retHeight )
{
  Dimension width, height;
  Dimension mh, mw, ml, mr, mt, mb ; /*margin*/
  Dimension st, bw, ht;
  Dimension space;
  
  XtVaGetValues(btn, XmNwidth, &width,
		XmNheight, &height,
		XmNmarginHeight, &mh,
		XmNmarginWidth, &mw,
		XmNmarginLeft, &ml,
		XmNmarginRight, &mr,
		XmNmarginBottom, &mb,
		XmNmarginTop, &mt,
		XmNhighlightThickness, &ht,
		XmNshadowThickness, &st,
		XmNborderWidth, &bw,
		0);
  
  XtVaGetValues(optionMenu, XmNspacing, &space, 0);
  
  *retHeight = height + (mh+mt+mb+bw+st+ht + space ) * 2;
  *retWidth  = width + (mw+ml+mr+bw+st+ht + space) * 2;
}


Widget
XFE_MailFilterRulesView::make_opPopup(Widget popupW, 
									  MSG_SearchMenuItem menu[],
									  int itemNum, 
									  Dimension *width, 
									  Dimension *height,	
									  int row, XtCallbackProc cb) 
{
  XmString xmStr;
  char *buttonLabel;
  Widget btn, returnBtn = 0;
  Arg	av[30];
  Cardinal ac;
  int j = 0;
  fe_mailfilt_userData *userData;  

 
  while(j < itemNum) {
    j++;	
    buttonLabel = (char *) malloc(strlen(menu[j-1].name)+1);
    strcpy(buttonLabel, menu[j-1].name);
    xmStr = XmStringCreateLtoR(buttonLabel, XmSTRING_DEFAULT_CHARSET);

    userData = XP_NEW_ZAP(fe_mailfilt_userData);
    userData->row = row;
    userData->attrib = (MSG_SearchAttribute) menu[j-1].attrib;
    ac = 0;
    XtSetArg(av[ac], XmNuserData, userData); ac++;
    XtSetArg(av[ac], XmNlabelString, xmStr); ac++;
    btn = XmCreatePushButtonGadget(popupW, "operatorBtn", av, ac);
    XtAddCallback(btn, XmNactivateCallback, cb, this);
	
    get_option_size(popupW, btn, width, height);
	XP_ASSERT(btn);
    XtManageChild(btn);
    free(buttonLabel);
    XmStringFree(xmStr);
    if (j==1)
      returnBtn = btn;
  }
  return returnBtn;
}

Widget
XFE_MailFilterRulesView::make_option_menu(Widget parent, char* widgetName,
										  Widget *popup)
{
  Cardinal ac;
  Arg      av[10];
  Visual   *v = 0;
  Colormap cmap = 0;
  Cardinal depth =0;
  Widget option_menu;

  XtVaGetValues(CONTEXT_WIDGET(m_contextData),
		XtNvisual, &v, XtNcolormap, &cmap, XtNdepth, &depth, 0);
  ac = 0;
  XtSetArg(av[ac], XmNvisual, v); ac++;
  XtSetArg(av[ac], XmNdepth, depth); ac++;
  XtSetArg(av[ac], XmNcolormap, cmap); ac ++;
  *popup= XmCreatePulldownMenu(parent, widgetName, av, ac);
  
  ac = 0;
  XtSetArg(av[ac], XmNsubMenuId, *popup); ac++;
  XtSetArg(av[ac], XmNmarginWidth, 0); ac++;
  option_menu = XmCreateOptionMenu(parent, widgetName, av, ac);
  XtUnmanageChild(XmOptionLabelGadget(option_menu));
  
  return option_menu;
}

void
XFE_MailFilterRulesView::editclose_cb(Widget w, 
									  XtPointer clientData, 
									  XtPointer calldata)
{
  XFE_MailFilterRulesView *obj = (XFE_MailFilterRulesView *) clientData;
  obj->editcloseCB(w, calldata);
}

void
XFE_MailFilterRulesView::editcloseCB(Widget /* w */, XtPointer /* calldata */)
{
  XtUnmanageChild(m_dialog);
}

void
XFE_MailFilterRulesView::editok_cb(Widget w, XtPointer clientData, XtPointer calldata)
{
  XFE_MailFilterRulesView *obj = (XFE_MailFilterRulesView *) clientData;
  obj->editokCB(w, calldata);
}

void
XFE_MailFilterRulesView::editokCB(Widget /* w */, XtPointer /* calldata */)
{
  MSG_Filter *f;

  MSG_GetFilterAt(m_filterlist, m_at, &f);
  MSG_DestroyFilter(f);
  getParams(m_at);
}


void
XFE_MailFilterRulesView::newok_cb(Widget w, 
								  XtPointer clientData, 
								  XtPointer calldata)
{
  XFE_MailFilterRulesView *obj = (XFE_MailFilterRulesView *) clientData;
  obj->newokCB(w, calldata);
}

void
XFE_MailFilterRulesView::newokCB(Widget /* w */, XtPointer /* calldata */)
{
  getParams(-1);
}

void
XFE_MailFilterRulesView::whereOpt_cb(Widget w, 
									 XtPointer clientData, 
									 XtPointer calldata)
{
  XFE_MailFilterRulesView *obj = (XFE_MailFilterRulesView *) clientData;
  obj->whereOptCB(w, calldata);
}

void
XFE_MailFilterRulesView::whereOptCB(Widget w, XtPointer /* calldata */)
{
	int num;
	fe_mailfilt_userData *userData;
	MSG_SearchAttribute attrib;
	MSG_SearchOperator op;
	XtVaGetValues(w, XmNuserData, &userData, 0);
	num = userData->row - 1;
#if defined(DEBUG_tao)
	printf("\n XFE_MailFilterRulesView::whereOptCB:userData->row=%d, num=%d\n",
		   userData->row, num);
#endif
	op = (MSG_SearchOperator) userData->attrib;

	/* shall we get from menuHistory instead of xxxSelected ?
	 */
	/* WATCH OUT !!!
	 */
	Widget ww = 0;
	XtVaGetValues(m_scopeOpt[num], XmNmenuHistory, &ww, NULL);
	XtVaGetValues(ww, XmNuserData, &userData, 0);

	attrib = (MSG_SearchAttribute) userData->attrib;
	if (op == opIsEmpty ||
		attrib == attribPriority ||
		attrib == attribMsgStatus) {
		XtUnmanageChild(m_scopeText[num]);
		XtVaSetValues(m_whereOpt[num],
					  XmNtopAttachment, XmATTACH_FORM,
					  XmNbottomAttachment, XmATTACH_FORM,
					  XmNleftAttachment, XmATTACH_WIDGET,
					  XmNleftWidget, m_whereLabel[num],
					  XmNrightAttachment, XmATTACH_NONE,
					  0);

		if (attrib == attribPriority) {
			XtVaSetValues(m_priLevel[num],
						  XmNtopAttachment, XmATTACH_FORM,
						  XmNbottomAttachment, XmATTACH_NONE,
						  XmNleftAttachment, XmATTACH_WIDGET,
						  XmNleftWidget, m_whereOpt[num],
						  XmNrightAttachment, XmATTACH_FORM,
						  0);

			if (op == opIsHigherThan) {
#if defined(DEBUG_tao_)
				printf("\n op == opIsHigherThan \n");
#endif
				if (m_lowBtn[num])
					XtManageChild(m_lowBtn[num]);
				if (m_highBtn[num])
					XtUnmanageChild(m_highBtn[num]);
				if (m_noneBtn[num])
					XtUnmanageChild(m_noneBtn[num]);

				XtVaSetValues(m_priLevel[num], XmNmenuHistory, m_normalBtn[num], 
							  NULL);
			}/* if */ 
			else if (op == opIsLowerThan) {
#if defined(DEBUG_tao_)
				printf("\n op == opIsLowerThan \n");
#endif
 				if (m_highBtn[num])
 					XtManageChild(m_highBtn[num]);
 				if (m_lowBtn[num])
 					XtUnmanageChild(m_lowBtn[num]);
 				if (m_noneBtn[num])
 					XtUnmanageChild(m_noneBtn[num]);	
				XtVaSetValues(m_priLevel[num], XmNmenuHistory, m_normalBtn[num], 
							  NULL);
			}/* else if */
			else {
				/* Assert */
 				if (m_highBtn[num])
 					XtManageChild(m_highBtn[num]);
 				if (m_lowBtn[num])
 					XtManageChild(m_lowBtn[num]);
				if (m_noneBtn[num]) {
 					XtManageChild(m_noneBtn[num]);	
					XtVaSetValues(m_priLevel[num], XmNmenuHistory, m_noneBtn[num], 
								  NULL);
				}/* if */
			}/* else */
		}/* if */
		else if (attrib == attribMsgStatus) {
			XtVaSetValues(m_statusLevel[num],
						  XmNtopAttachment, XmATTACH_FORM,
						  XmNbottomAttachment, XmATTACH_NONE,
						  XmNleftAttachment, XmATTACH_WIDGET,
						  XmNleftWidget, m_whereOpt[num],
						  XmNrightAttachment, XmATTACH_FORM,
						  0);
		}/* if */
	} else if (!XtIsManaged(m_scopeText[num])) {
		XtVaSetValues(m_whereOpt[num],
					  XmNtopAttachment, XmATTACH_FORM,
					  XmNbottomAttachment, XmATTACH_FORM,
					  XmNleftAttachment, XmATTACH_WIDGET,
					  XmNleftWidget, m_whereLabel[num],
					  XmNrightAttachment, XmATTACH_NONE,
					  0);
		XtVaSetValues(m_scopeText[num],
					  XmNtopAttachment, XmATTACH_FORM,
					  XmNbottomAttachment, XmATTACH_NONE,
					  XmNleftAttachment, XmATTACH_WIDGET,
					  XmNleftWidget, m_whereOpt[num],
					  0);

		XP_ASSERT(m_scopeText[num]);	
		XtManageChild(m_scopeText[num]);
	}
}

void
XFE_MailFilterRulesView::scopeOpt_cb(Widget w, 
									 XtPointer clientData, 
									 XtPointer calldata)
{
  XFE_MailFilterRulesView *obj = (XFE_MailFilterRulesView *) clientData;
  obj->scopeOptCB(w, calldata);
}

void
XFE_MailFilterRulesView::scopeOptCB(Widget w, XtPointer /* calldata */)
{
  fe_mailfilt_userData *userData;
  int num;
  MSG_SearchAttribute attrib;
  Dimension width, height;
  
  XtVaGetValues(w, XmNuserData, &userData, 0);
  num = userData->row - 1;
  attrib = userData->attrib;
#if defined(DEBUG_tao)
  printf("\n XFE_MailFilterRulesView::scopeOptCB: num=%d, attrib=%d\n",
		 num, attrib);
#endif

  if (attrib == attribPriority ||
	  attrib == attribMsgStatus) {

    XtUnmanageChild(m_whereOpt[num]);

    /* This piece of code destroys all WhereOpt's children;
     * and recreate them based on scopeOpt's value...
     * It may disable OptionMenu's capability of setting current choice
     * menuHistory
     */
    XtUnmanageChild(m_scopeText[num]);
    buildWhereOpt(&width, &height, num, attrib);
    XtVaSetValues(m_whereOpt[num],
		  XmNwidth, width,
		  XmNtopAttachment, XmATTACH_FORM,
		  XmNbottomAttachment, XmATTACH_FORM,
		  XmNleftAttachment, XmATTACH_WIDGET,
		  XmNleftWidget, m_whereLabel[num],
		  XmNrightAttachment, XmATTACH_NONE,
		  0);

	if (attrib == attribPriority)
		XtVaSetValues(m_priLevel[num],
					  XmNtopAttachment, XmATTACH_FORM,
					  XmNbottomAttachment, XmATTACH_FORM,
					  XmNleftAttachment, XmATTACH_WIDGET,
					  XmNleftWidget, m_whereOpt[num],
					  XmNrightAttachment, XmATTACH_FORM,
					  0);
	else if (attrib == attribMsgStatus)
		XtVaSetValues(m_statusLevel[num],
					  XmNtopAttachment, XmATTACH_FORM,
					  XmNbottomAttachment, XmATTACH_FORM,
					  XmNleftAttachment, XmATTACH_WIDGET,
					  XmNleftWidget, m_whereOpt[num],
					  XmNrightAttachment, XmATTACH_FORM,
					  0);


	XP_ASSERT(m_whereOpt[num]);
    XtManageChild(m_whereOpt[num]);

	if (attrib == attribPriority) {
		if (m_statusLevel[num] && XtIsManaged(m_statusLevel[num]))
			XtUnmanageChild(m_statusLevel[num]);
		XP_ASSERT(m_priLevel[num]);
		XtManageChild(m_priLevel[num]);
	}/* if */
	else if (attrib == attribMsgStatus) {
		XP_ASSERT(m_statusLevel[num]);
		if (m_priLevel[num] && XtIsManaged(m_priLevel[num]))
			XtUnmanageChild(m_priLevel[num]);
		XtManageChild(m_statusLevel[num]);
	}/* else if */
  } else {
    XtUnmanageChild(m_whereOpt[num]);
	if (XtIsManaged(m_priLevel[num]))
		XtUnmanageChild(m_priLevel[num]);
	if (XtIsManaged(m_statusLevel[num]))
		XtUnmanageChild(m_statusLevel[num]);
        
    buildWhereOpt(&width, &height, num, attrib);

    XtVaSetValues(m_whereOpt[num],
		  XmNwidth, width,
		  XmNtopAttachment, XmATTACH_FORM,
		  XmNbottomAttachment, XmATTACH_FORM,
		  XmNleftAttachment, XmATTACH_WIDGET,
		  XmNleftWidget, m_whereLabel[num],
		  XmNrightAttachment, XmATTACH_NONE,
		  0);
    XtVaSetValues(m_scopeText[num],
		  XmNtopAttachment, XmATTACH_FORM,
		  XmNbottomAttachment, XmATTACH_FORM,
		  XmNleftAttachment, XmATTACH_WIDGET,
		  XmNleftWidget, m_whereOpt[num],
		  XmNrightAttachment, XmATTACH_FORM,
		  0);
	if (attrib == attribDate) {
		char dateStr[40], tmp[16];
		time_t now = XP_TIME();
		XP_StrfTime(m_contextData, 
					dateStr, sizeof(dateStr), 
					XP_DATE_TIME_FORMAT,
					localtime(&now));
#if defined(DEBUG_tao)
		printf("\n***Default local time, XP_StrfTime: date=%ld=%s\n", 
			   now, dateStr);
#endif
		XP_STRNCPY_SAFE(tmp, dateStr, 9);
		fe_SetTextField(m_scopeText[num], tmp);
	}/* if */
	else {
		char tmp[16];
		tmp[0] = '\0';
		fe_SetTextField(m_scopeText[num], "");
	}/* else */
	XP_ASSERT(m_whereOpt[num] && m_scopeText[num]);
    XtManageChild(m_whereOpt[num]);
    XtManageChild(m_scopeText[num]);
  }
}

void
XFE_MailFilterRulesView::thenClause_cb(Widget w, 
									   XtPointer clientData, 
									   XtPointer calldata)
{
  XFE_MailFilterRulesView *obj = (XFE_MailFilterRulesView *) clientData;
  obj->thenClauseCB(w, calldata);
}

void
XFE_MailFilterRulesView::thenClauseCB(Widget w, XtPointer /* calldata */)
{
  fe_mailfilt_userData *userData;
  int num; 
  MSG_SearchAttribute attrib;
  XtVaGetValues(w, XmNuserData, &userData, 0);
  num = userData->row - 1;
  attrib = userData->attrib;
  if (attrib==acChangePriority) {
	  XtUnmanageChild(m_thenTo);
	  XtUnmanageChild(m_newFolderBtn);
	  XtVaSetValues(m_priAction,
					XmNtopAttachment, XmATTACH_WIDGET,
					XmNtopWidget, m_commandGroup,
					XmNbottomAttachment, XmATTACH_NONE,
					XmNleftAttachment, XmATTACH_WIDGET,
					XmNleftWidget, m_thenClause,
					XmNrightAttachment, XmATTACH_NONE,
					0);
	  XP_ASSERT(m_priAction);
	  XtManageChild(m_priAction);
  } else if (attrib==acMoveToFolder) {
	  XtUnmanageChild(m_priAction);
	  XP_ASSERT(m_thenTo);
	  Dimension hei = XfeHeight(m_thenClause);
	  XtVaSetValues(m_thenTo,
					XmNheight, hei-4,
					XmNtopAttachment, XmATTACH_WIDGET,
					XmNtopWidget, m_commandGroup,
					XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
					XmNbottomWidget, m_thenClause,
					XmNleftAttachment, XmATTACH_WIDGET,
					XmNleftWidget, m_thenClause,
					XmNrightAttachment, XmATTACH_NONE,
					XmNtopOffset, 3,
					XmNbottomOffset, 3,
					0);
	  XtVaSetValues(m_newFolderBtn,
					XmNheight, hei-4,
					XmNtopAttachment, XmATTACH_WIDGET,
					XmNtopWidget, m_commandGroup,
					XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
					XmNbottomWidget, m_thenClause,
					XmNleftAttachment, XmATTACH_WIDGET,
					XmNleftWidget, m_thenTo,
					XmNrightAttachment, XmATTACH_NONE,
					XmNtopOffset, 3,
					XmNbottomOffset, 3,
					XmNleftOffset, 3,
					0);
      XtManageChild(m_thenTo);
      XtManageChild(m_newFolderBtn);
#if defined(DEBUG_tao_)
	  int topOffset, bottomOffset;
	  XtVaGetValues(m_thenTo, 
					XmNtopOffset, &topOffset,
					XmNbottomOffset, &bottomOffset,
					0);
	  Dimension height = XfeHeight(m_thenClause);
	  printf("\n m_thenClause, HEI=%d\n", height);
	  height = XfeHeight(m_thenTo);
	  printf("\n m_thenTo,     HEI=%d\n", height);
	  printf("\n topOffset=%d, bottomOffset=%d\n", topOffset, bottomOffset);
#endif

  } else {
	  XtUnmanageChild(m_priAction);
	  XtUnmanageChild(m_thenTo);
	  XtUnmanageChild(m_newFolderBtn);
	  XtVaSetValues(m_thenClause, XmNrightAttachment, XmATTACH_NONE, 0);
  }
}

void 
XFE_MailFilterRulesView::buildWhereOpt(Dimension *width, 
									   Dimension *height, int num, 
									   MSG_SearchAttribute attrib)
{
  Widget dummy;
  Widget popupW;
  MSG_SearchMenuItem menu[20];
  uint16 maxItems = 20;
  int i, numChildren;
  WidgetList childrenList;

  /* This piece of code destroies all WhereOpt's children;
   * and recreate them based on scopeOpt's value...
   * It may disable OptionMenu's capability of setting current choice
   * menuHistory
   */
  popupW = m_whereOptPopup[num];
  if (popupW) {
    XtVaGetValues(popupW, XmNchildren, &childrenList,
                  XmNnumChildren, &numChildren, 0);
    for ( i = 0; i < numChildren; i++ ) {
      XtDestroyWidget(childrenList[i]);
    }
  }/* if */

  /* make the option menu for where */
  /* get what's needed in the option menu from backend */
  MSG_FolderInfo *selArray[1];
  MSG_FolderInfo *folderInfo;
  MSG_Master *master = fe_getMNMaster();
  int num_inboxes = MSG_GetFoldersWithFlag(master,
										   MSG_FOLDER_FLAG_INBOX,
										   &folderInfo, 1);
  if (num_inboxes >= 1) {
	  selArray[0] = folderInfo;
	  MSG_GetOperatorsForFilterScopes(master, scopeMailFolder, 
									  (void**)selArray, 1,
									  attrib, menu, &maxItems);
  }/* if */
  else
	  maxItems = 0;

  /* we save the first widget returned always */
  dummy = make_opPopup(popupW, menu, maxItems, width,
					   height, num+1, 
					   whereOpt_cb);
  XtVaSetValues(m_whereOpt[num], XmNmenuHistory, dummy, NULL);
  m_whereOptPopup[num] = popupW;
}

void
XFE_MailFilterRulesView::getParams(int position)
{
  int num;
  char *desptext;
  char *nametext;
  void *value2;
  MSG_Filter *filter;
  MSG_Rule *rule;
  MSG_RuleActionType action;
  MSG_SearchAttribute attrib;
  MSG_SearchOperator op;
  MSG_SearchValue value;
  int32 filterCount;
  MSG_FilterError err;

  nametext = fe_GetTextField(m_filterName);
  MSG_CreateFilter(filterInboxRule, nametext, &filter);
  desptext = fe_GetTextField(m_despField);
  MSG_SetFilterDesc(filter, desptext);

  free(nametext);
  free(desptext);

  if (m_curFilterOn) {
    MSG_EnableFilter(filter, m_curFilterOn);
  }

  if (err = MSG_GetFilterRule(filter, &rule))
	  {
		  return;
	  }

  for (num = 0; num < m_stripCount; num++) {
    char customHdr[160];
    getTerm(num, &attrib, &op, &value, customHdr);
#ifdef DEBUG_akkana
    if (attrib == attribOtherHeader && customHdr)
        printf("Adding custom header term %s with value %s\n",
               customHdr, value.u.string);
#endif
    if (err = MSG_RuleAddTerm(rule, attrib, op, &value,
                              m_booleanAnd, customHdr)) {
	  XP_ASSERT(err == FilterError_Success);
	  if (err != FilterError_Success) return;
	}/* if */
  }
  XP_ASSERT(num);

  getAction(&action, &value2);
  MSG_RuleSetAction(rule, action, value2);

  if (position < 0) {
    MSG_GetFilterCount(m_filterlist, &filterCount);
    MSG_SetFilterAt(m_filterlist, filterCount, filter);
  } else {
    MSG_SetFilterAt(m_filterlist, (MSG_FilterIndex) position, filter);
  }
}

void
XFE_MailFilterRulesView::setRulesParams(int num,
										MSG_SearchAttribute attrib,
										MSG_SearchOperator op,
										MSG_SearchValue value,
                                        char* customHdr)
{
  fe_mailfilt_userData *userData;
  int numChildren, i;
  WidgetList childrenList;

  /* scope
   */
  XtVaGetValues(m_scopeOptPopup[num], XmNchildren, &childrenList,
                XmNnumChildren, &numChildren, 0);
  for ( i = 0; i < numChildren; i++ ) {
    XtVaGetValues(childrenList[i], XmNuserData, &userData, 0);
    if (!XfeIsAlive(childrenList[i])) {
#if defined(DEBUG_tao_)
      printf("\n *** dying widget found in setting op Opt!\n ");
#endif
      continue;
    }/* if */

    if (userData && (attrib == (MSG_SearchAttribute) userData->attrib))
    {
        if ((MSG_SearchOperator)userData->attrib == attribOtherHeader
            && customHdr && customHdr[0] != '\0')
        {
            // compare labelString of childrenList[i] to customHdr
            XmString xmstr;
            XtVaGetValues(childrenList[i], XmNlabelString, &xmstr, 0);
            char* labl;
            XmStringGetLtoR(xmstr, XmSTRING_DEFAULT_CHARSET, &labl);
            if (labl && !XP_STRCMP(labl, customHdr)) {
                XtVaSetValues(m_scopeOpt[num],
                              XmNmenuHistory, childrenList[i],
                              NULL);
                XtFree(labl);
                break;
            }
            XtFree(labl);
            continue;
        }
        else {
            XtVaSetValues(m_scopeOpt[num], XmNmenuHistory, childrenList[i], 
                          NULL);
        }
        break;
    }/* if */
  }/* for i */

  // If we've gotten through the list without matching,
  // punt and set to the first button in the list:
  if (i >= numChildren)
  {
#ifdef DEBUG_akkana
      printf("Unrecognized header (%s); punting\n", customHdr);
#endif
      i = 0;
      XtVaSetValues(m_scopeOpt[num], XmNmenuHistory, childrenList[0], 0);
  }

  scopeOpt_cb(childrenList[i], (XtPointer) this, NULL);

  /* op
   */
  XtVaGetValues(m_whereOptPopup[num], XmNchildren, &childrenList,
                XmNnumChildren, &numChildren, 0);
  for ( i = 0; i < numChildren; i++ ) {
    if (!XfeIsAlive(childrenList[i])) {
#if defined(DEBUG_tao_)
      printf("\n *** dying widget found in setting op Opt!\n ");
#endif
      continue;
    }/* if */

    XtVaGetValues(childrenList[i], XmNuserData, &userData, 0);
    if (userData && op == (MSG_SearchOperator) userData->attrib) {
      XtVaSetValues(m_whereOpt[num], XmNmenuHistory, childrenList[i], 
                    NULL);
      break;
    }/* match */
  }/* for */
  whereOpt_cb(childrenList[i], (XtPointer) this, NULL);
  
  /* value
   */
  switch(attrib) {
  case attribDate:
	  {
		  char dateStr[40], tmp[16];
		  XP_StrfTime(m_contextData, 
					  dateStr, sizeof(dateStr), XP_DATE_TIME_FORMAT,
					  localtime(&(value.u.date)));
#if defined(DEBUG_tao)
		  printf("\n*** Date from db: date=%ld=%s\n", value.u.date, dateStr);
#endif
		  XP_STRNCPY_SAFE(tmp, dateStr, 9);
		  fe_SetTextField(m_scopeText[num], tmp);
		  break;
	  }
  case attribMsgStatus:
    XtVaGetValues(m_statusLevelPopup[num], XmNchildren, &childrenList,
				  XmNnumChildren, &numChildren, 0);
    for ( i = 0; i < numChildren; i++ ) {
		if (!XfeIsAlive(childrenList[i])) {
#if defined(DEBUG_tao_)
			printf("\n *** dying widget found in setting op Opt!\n ");
#endif
			continue;
		}/* if */
		XtVaGetValues(childrenList[i], XmNuserData, &userData, 0);
		if (value.u.msgStatus == (uint32) userData->attrib) {
			XtVaSetValues(m_statusLevel[num], XmNmenuHistory, childrenList[i], 
						  NULL);
			break;
		}/* if */
    }/* for i */
    break;

  case attribPriority:
    XtVaGetValues(m_priLevelPopup[num], XmNchildren, &childrenList,
				  XmNnumChildren, &numChildren, 0);
    for ( i = 0; i < numChildren; i++ ) {
		if (!XfeIsAlive(childrenList[i])) {
#if defined(DEBUG_tao_)
			printf("\n *** dying widget found in setting op Opt!\n ");
#endif
			continue;
		}/* if */
		XtVaGetValues(childrenList[i], XmNuserData, &userData, 0);
		if (value.u.priority == (MSG_PRIORITY) userData->attrib) {
			XtVaSetValues(m_priLevel[num], XmNmenuHistory, childrenList[i], 
						  NULL);
			break;
		}/* if */
    }/* for i */
    break;

  default:
    fe_SetTextField(m_scopeText[num], value.u.string);
    break;
  }
}


void
XFE_MailFilterRulesView::setActionParams(MSG_RuleActionType type,
										 void *value)
{
  fe_mailfilt_userData *userData;
  int numChildren, i;
  WidgetList childrenList;

  XtVaGetValues(m_thenClausePopup, XmNchildren, &childrenList,
                XmNnumChildren, &numChildren, 0);
  for ( i = 0; i < numChildren; i++ ) {
    if (!XfeIsAlive(childrenList[i])) {
#if defined(DEBUG_tao_)
      printf("\n *** dying widget found in setting op Opt!\n ");
#endif
      continue;
    }/* if */

    XtVaGetValues(childrenList[i], XmNuserData, &userData, 0);
    if (type == (MSG_RuleActionType) userData->attrib) {
      XtVaSetValues(m_thenClause, XmNmenuHistory, childrenList[i], 
					NULL);
      break;
    }/* if */
  }/* for i */

  thenClause_cb(childrenList[i], (XtPointer) this, NULL);

  if (type == acMoveToFolder) {
	  m_folderDropDown->selectFolder((char *) value);
  } else if (type == acChangePriority) {
    XtVaGetValues(m_priActionPopup, XmNchildren, &childrenList,
                  XmNnumChildren, &numChildren, 0);
    for ( i = 0; i < numChildren; i++ ) {
		if (!XfeIsAlive(childrenList[i])) {
#if defined(DEBUG_tao_)
			printf("\n *** dying widget found in setting chg pri Opt!\n ");
#endif
			continue;
		}/* if */
		XtVaGetValues(childrenList[i], XmNuserData, &userData, 0);
		if ((MSG_PRIORITY) value == (MSG_PRIORITY) userData->attrib) {
			XtVaSetValues(m_priAction, 
						  XmNmenuHistory, childrenList[i], NULL);
			break;
		}
    }
  }/* else if */
  
}

Widget
XFE_MailFilterRulesView::make_actPopup(Widget popupW, 
									   Dimension *width, Dimension *height,	
									   int row, XtCallbackProc cb) 
{
  XmString xmStr;
  char *buttonLabel;
  Widget btn, returnBtn = 0;
  Arg	av[30];
  Cardinal ac;
  int j = 0;
  uint16 maxItems;
  fe_mailfilt_userData *userData;  
  MSG_RuleMenuItem menu[20];

  maxItems = 20;
  MSG_GetRuleActionMenuItems(filterInbox, menu, &maxItems);
 
  while(j < (int)maxItems) {
    j++;	
	buttonLabel = (char *) malloc(strlen(menu[j-1].name)+1);
	strcpy(buttonLabel, menu[j-1].name);
    xmStr = XmStringCreateLtoR(buttonLabel, XmSTRING_DEFAULT_CHARSET);

	userData = XP_NEW_ZAP(fe_mailfilt_userData);
    userData->row = row;
    userData->attrib = (MSG_SearchAttribute) menu[j-1].attrib;
    ac = 0;
    XtSetArg(av[ac], XmNuserData, userData); ac++;
    XtSetArg(av[ac], XmNlabelString, xmStr); ac++;
    btn = XmCreatePushButtonGadget(popupW, "operatorBtn", av, ac);
    XtAddCallback(btn, XmNactivateCallback, cb, this);
	
    get_option_size(popupW, btn, width, height);
	XP_ASSERT(btn);
    XtManageChild(btn);
    free(buttonLabel);
    XmStringFree(xmStr);
	if ( j == 1) 
		returnBtn = btn;
  }
  /* set to the first button
   */
  return returnBtn;
}

void
XFE_MailFilterRulesView::add_folders(Widget popupW, 
									 XtCallbackProc cb, 
									 MSG_FolderInfo *info)
{
  int num_sub_folders, i;
  MSG_FolderInfo **children = 0;
  const char *this_folder_name = MSG_GetFolderNameFromID(info);
  Widget button;

  button = XtVaCreateManagedWidget(this_folder_name,
								   xmPushButtonWidgetClass,
								   popupW,
								   NULL);
  
  XtVaSetValues(button, XmNuserData, info, NULL);
  XtAddCallback(button, XmNactivateCallback, cb, this);

  num_sub_folders = MSG_GetFolderChildren(fe_getMNMaster(), info, NULL, 0);

  if (num_sub_folders > 0)
	{
	  children = (MSG_FolderInfo **) XP_CALLOC(num_sub_folders, 
											   sizeof(MSG_FolderInfo*));
	  num_sub_folders = MSG_GetFolderChildren(fe_getMNMaster(), 
											  info, 
											  children, 
											  num_sub_folders);
	}

  for (i = 0; i < num_sub_folders; i ++)
	add_folders(popupW, cb, children[i]);

  if (num_sub_folders > 0)
	XP_FREE(children);
}

/* this function returns the first button that is created which
 * by default shows on the option menu */

Widget
XFE_MailFilterRulesView::make_attribPopup(Widget popupW, 
										  Dimension *width, 
										  Dimension *height,
										  int row, 
										  XtCallbackProc cb,
                                          Boolean adding)
{
  XmString xmStr;
  char *buttonLabel;
  Widget btn, returnBtn = 0;
  Arg	av[30];
  Cardinal ac;
  int j;
  uint16 numattrs = 0;
  MSG_SearchMenuItem *menu;
  fe_mailfilt_userData *userData;  

  MSG_FolderInfo *selArray[1];
  MSG_FolderInfo *folderInfo;
  MSG_Master *master = fe_getMNMaster();

  MSG_GetFoldersWithFlag(master,
						 MSG_FOLDER_FLAG_INBOX,
						 &folderInfo, 1);
  selArray[0] = folderInfo;
  MSG_GetNumAttributesForFilterScopes (master, scopeMailFolder,
                                       (void**)selArray, 1, &numattrs);
  menu = new MSG_SearchMenuItem[numattrs];
  MSG_GetAttributesForFilterScopes(master, scopeMailFolder, 
                                   (void**)selArray, 1, menu, &numattrs);
  int firstCustomHdr = -1;
  int numChildren;
  WidgetList childList;
  if (adding)
  {
      XtVaGetValues(popupW,
                    XmNchildren, &childList,
                    XmNnumChildren, &numChildren,
                    0);
  }

  for (j = 0; j < (int)numattrs; ++j)
  {
    if (firstCustomHdr < 0 && menu[j].attrib == attribOtherHeader)
    {
        firstCustomHdr = j;
        if (!adding)
            XtVaCreateManagedWidget("_sep", xmSeparatorGadgetClass, popupW, 0);
    }

    buttonLabel = strdup(menu[j].name);

    // Special case:
    // If we're adding and we've increased the number of custom headers,
    // then we'll hit the separator when we want to be adding the first
    // new header.  In that case, delete the separator and Custom...
    // button and switch off adding mode.
    if (adding && j >= numChildren-3)
    {
        adding = FALSE;
        XtDestroyWidget(childList[numChildren-1]);
        XtDestroyWidget(childList[numChildren-2]);
    }

    if (adding && firstCustomHdr > 0 && j > firstCustomHdr)
    {
        XtVaGetValues(childList[j+1],
                      XmNlabelString, &xmStr,
                      XmNuserData, &userData,
                      0);
        // j is index in menu; corresponding widget is @ j+1
        // because of the separator.
        char* curlabl;
        XmStringGetLtoR(xmStr, XmSTRING_DEFAULT_CHARSET, &curlabl);
        if (!XP_STRCMP(curlabl, buttonLabel))
        {
            XtFree(curlabl);
            continue;
        }
        XtFree(curlabl);
        XP_ASSERT(userData);
        if (userData)
            userData->attrib = (MSG_SearchAttribute) menu[j].attrib;
        else {      // this shouldn't happen!
            userData = XP_NEW_ZAP(fe_mailfilt_userData);
            userData->row = row;
            userData->attrib = (MSG_SearchAttribute) menu[j].attrib;
        }
        xmStr = XmStringCreateLtoR(buttonLabel, XmSTRING_DEFAULT_CHARSET);
        free(buttonLabel);
        XtVaSetValues(childList[j+1],
                      XmNlabelString, xmStr,
                      XmNuserData, userData,
                      0);
        XmStringFree(xmStr);
        continue;
    }

    if (adding)
        continue;

    xmStr = XmStringCreateLtoR(buttonLabel, XmSTRING_DEFAULT_CHARSET);

    userData = XP_NEW_ZAP(fe_mailfilt_userData);
    userData->row = row;
    userData->attrib = (MSG_SearchAttribute) menu[j].attrib;
    ac = 0;
    XtSetArg(av[ac], XmNuserData, userData); ac++;
    XtSetArg(av[ac], XmNlabelString, xmStr); ac++;
    btn = XmCreatePushButtonGadget(popupW, "operatorBtn", av, ac);
    XtAddCallback(btn, XmNactivateCallback, cb, this);

    if (width && height)
        get_option_size(popupW, btn, width, height);
	XP_ASSERT(btn);
    XtManageChild(btn);
    free(buttonLabel);
    XmStringFree(xmStr);
    if (j == 0)
		returnBtn = btn;
  }
  delete[] menu;

  if (!adding)
  {
      // Add the "Edit Custom Hdr" button:
      XtVaCreateManagedWidget("sep", xmSeparatorGadgetClass, popupW, 0);
      btn = XtVaCreateManagedWidget("editHdrBtn",
                                    xmPushButtonGadgetClass, popupW, 0);
      XtAddCallback(btn, XmNactivateCallback, editHdr_cb, this);
  }

  return returnBtn;
}

/* this function returns the first button that is created which
 * by default shows on the option menu */

Widget
XFE_MailFilterRulesView::makeopt_priority(Widget popupW, 
										  Dimension *width, 
										  Dimension *height,	
										  int row) 
{
  int num = row-1;
  XmString xmStr;
  Widget btn;
  Arg	av[30];
  Cardinal ac;
  int j = 0;
  uint16 maxItems = 20;
  Widget returnBtn = 0;
  fe_mailfilt_userData *userData;  

  /* Good citizen initializes those widget
   */
  XP_ASSERT(num >= 0);
  m_noneBtn[num] = NULL;
  m_normalBtn[num] = NULL;
  m_lowBtn[num] = NULL;
  m_highBtn[num] = NULL;

  MSG_SearchMenuItem *items = 
	  (MSG_SearchMenuItem*) XP_CALLOC(maxItems, sizeof(MSG_SearchMenuItem));

  MSG_GetValuesForAttribute(scopeMailFolder, attribPriority, items, &maxItems );
  while(j < (int)maxItems) {
    j++;
    xmStr = XmStringCreateLtoR(items[j-1].name, XmSTRING_DEFAULT_CHARSET);

	
    userData = XP_NEW_ZAP(fe_mailfilt_userData);
    userData->row = row;
    userData->attrib = (MSG_SearchAttribute) items[j-1].attrib;
    ac = 0;
    XtSetArg(av[ac], XmNuserData, userData); ac++;
    XtSetArg(av[ac], XmNlabelString, xmStr); ac++;
    btn = XmCreatePushButtonGadget(popupW, "operatorBtn", av, ac);

    if (items[j-1].attrib == MSG_HighestPriority) 
      m_highBtn[num] = btn;
    else if (items[j-1].attrib == MSG_LowestPriority) 
      m_lowBtn[num] = btn;
    else if (items[j-1].attrib == MSG_NormalPriority) 
      m_normalBtn[num] = btn;
    else if (items[j-1].attrib == MSG_NoPriority) 
      m_noneBtn[num] = btn;
	
    get_option_size(popupW, btn, width, height);
	XP_ASSERT(btn);
    XtManageChild(btn);
    XmStringFree(xmStr);
    if (j == 1) 
      returnBtn = btn;
  }
  XP_FREEIF(items);
  return returnBtn;
}


Widget
XFE_MailFilterRulesView::makeopt_status(Widget popupW, 
										Dimension *width, 
										Dimension *height,	
										int row) 
{
  XmString xmStr;
  Widget btn;
  Arg	av[30];
  Cardinal ac;
  int j = 0;
  uint16 maxItems = 20;
  Widget returnBtn = 0;
  fe_mailfilt_userData *userData;  

  MSG_SearchMenuItem *items = 
	  (MSG_SearchMenuItem*) XP_CALLOC(maxItems, sizeof(MSG_SearchMenuItem));

  MSG_GetValuesForAttribute(scopeMailFolder,
							attribMsgStatus, items, &maxItems );
  while(j < (int)maxItems) {
    j++;
    xmStr = XmStringCreateLtoR(items[j-1].name, XmSTRING_DEFAULT_CHARSET);

	
    userData = XP_NEW_ZAP(fe_mailfilt_userData);
    userData->row = row;
    userData->attrib = (MSG_SearchAttribute) items[j-1].attrib;
    ac = 0;
    XtSetArg(av[ac], XmNuserData, userData); ac++;
    XtSetArg(av[ac], XmNlabelString, xmStr); ac++;
    btn = XmCreatePushButtonGadget(popupW, "operatorBtn", av, ac);
	
    get_option_size(popupW, btn, width, height);
	XP_ASSERT(btn);
    XtManageChild(btn);
    XmStringFree(xmStr);
    if (j == 1) 
      returnBtn = btn;
  }
  XP_FREEIF(items);
  return returnBtn;
}


void
XFE_MailFilterRulesView::makeRules(Widget rowcol, int fake_num) 
{
  int num = fake_num-1;
  Arg	av[30];
  Cardinal ac;
  Widget popupW;
  Widget dummy;
  char name[128];
  Dimension width, height;
  fe_mailfilt_userData *userData;
  MSG_SearchAttribute attrib;

  if (!m_content[num]) {
    sprintf(name,"formContainer%d",num);
    ac = 0;
    m_content[num] = XmCreateForm(rowcol, name, av, ac);
    ac = 0;
    m_strip[num] = XmCreateForm(m_content[num], "rulesForm", av, ac );
    
    /* build the label */
    ac = 0;
    if (num == 0)
      m_rulesLabel[num] = XmCreateLabelGadget(m_content[num], 
					       "rulesLabel1", av, ac);
    else {
      m_rulesLabel[num] = XmCreateLabelGadget(m_content[num], 
					       "rulesLabel", av, ac);
      setOneBoolean(num);
    }
    
    /* make the option menu for scope */ 
    m_scopeOpt[num] = make_option_menu(m_strip[num],
									   "rulesScopeOpt", &popupW);
    dummy = make_attribPopup(popupW, &width, &height, num+1, 
							 scopeOpt_cb);
    m_scopeOptPopup[num] = popupW;
	XtVaSetValues(m_scopeOpt[num], XmNmenuHistory, dummy, NULL);

    /* we save the first widget returned always */
    /* later we use this widget to get the correct attribute */
    
    /* another label */ 
    ac = 0;
    m_whereLabel[num] = XmCreateLabelGadget(m_strip[num], 
											"whereLabel", 
											av, ac);
    
    /* get what's needed in the option menu from backend 
	 */
	Widget w = 0;
	XtVaGetValues(m_scopeOpt[num], XmNmenuHistory, &w, NULL);
	XtVaGetValues(w, XmNuserData, &userData, 0);
    attrib = userData->attrib;
    popupW = 0;
    m_whereOpt[num] = make_option_menu(m_strip[num],
									   "rulesWhereOpt", &popupW);
    m_whereOptPopup[num] = popupW;
    buildWhereOpt(&width, &height, num, attrib);
	
    ac = 0;
    XtSetArg (av[ac], XmNwidth, 120); ac++;
    m_scopeText[num] = fe_CreateTextField(m_strip[num], "rulesText", av, ac);
	
    /* here is the priority level option menu */  
    popupW=0;
    m_priLevel[num] = make_option_menu(m_strip[num],
									   "priorityLevel", &popupW);
    dummy = makeopt_priority(popupW, &width, &height, num+1);
    m_priLevelPopup[num] = popupW;
    

	/* status level
	 */    
	popupW=0;
    m_statusLevel[num] = make_option_menu(m_strip[num],
									   "statusLevel", &popupW);
    dummy = makeopt_status(popupW, &width, &height, num+1);
    m_statusLevelPopup[num] = popupW;


    /* geometry */  
    XtVaSetValues(m_rulesLabel[num],
		  XmNalignment, XmALIGNMENT_END,
		  XmNtopAttachment, XmATTACH_FORM,
		  XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		  XmNbottomWidget, m_strip[num],
		  XmNleftAttachment, XmATTACH_FORM,
		  XmNrightAttachment, XmATTACH_WIDGET,
		  XmNrightWidget, m_strip[num],
		  0);
  	
    XtVaSetValues(m_scopeOpt[num],
		  XmNtopAttachment, XmATTACH_FORM,
		  XmNbottomAttachment, XmATTACH_FORM,
		  XmNleftAttachment, XmATTACH_FORM,
		  0);
  	
    XtVaSetValues(m_whereLabel[num],
		  XmNalignment, XmALIGNMENT_BEGINNING,
		  XmNtopAttachment, XmATTACH_FORM,
		  XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		  XmNbottomWidget, m_scopeOpt[num],
		  XmNleftAttachment, XmATTACH_WIDGET,
		  XmNleftWidget, m_scopeOpt[num],
		  XmNheight, XfeHeight(m_scopeOpt[num]),
		  0);
	
    XtVaSetValues(m_whereOpt[num],
		  XmNtopAttachment, XmATTACH_FORM,
		  XmNbottomAttachment, XmATTACH_FORM,
		  XmNleftAttachment, XmATTACH_WIDGET,
		  XmNleftWidget, m_whereLabel[num],
		  0);
	
    XtVaSetValues(m_scopeText[num],
		  XmNtopAttachment, XmATTACH_FORM,
		  XmNbottomAttachment, XmATTACH_NONE,
		  XmNleftAttachment, XmATTACH_WIDGET,
		  XmNleftWidget, m_whereOpt[num],
		  XmNrightAttachment, XmATTACH_FORM,
		  0);
  	
    XtVaSetValues(m_strip[num],
		  XmNtopAttachment, XmATTACH_FORM,
		  XmNbottomAttachment, XmATTACH_FORM,
          XmNleftAttachment, XmATTACH_NONE,
		  XmNrightAttachment, XmATTACH_FORM,
		  0);
	XP_ASSERT(m_rulesLabel[num] && m_scopeOpt[num] && 
			  m_whereLabel[num] && m_whereOpt[num] &&
			  m_scopeText[num] && m_strip[num] &&
			  m_content[num]);
    XtManageChild(m_rulesLabel[num]);
    XtManageChild(m_scopeOpt[num]);
    XtManageChild(m_whereLabel[num]);
    XtManageChild(m_whereOpt[num]);
    XtManageChild(m_scopeText[num]);
    XtManageChild(m_strip[num]);
    XtManageChild(m_content[num]);
  }

  // Doesn't userData also need to be deleted?
}

void
XFE_MailFilterRulesView::editHeaders(Widget w)
{
#undef DEFEAT_RACE_CONDITION    // BE not implemented yet
#ifdef DEFEAT_RACE_CONDITION
    // Defeat race condition between this dialog and the search view
    MSG_Master* master = fe_getMNMaster();
    if (!master->AcquireEditHeadersSemaphore(this))
    {
#ifdef DEBUG_akkana
        printf("MailFilterRulesView: Couldn't get semaphore!  Punting ...\n");
#endif
        return;
    }
#endif /* DEFEAT_RACE_CONDITION */

    XFE_EditHdrDialog* d
        = new XFE_EditHdrDialog(getToplevel()->getBaseWidget(),
                                "editHdrDialog", getContext());
    char* newhdr = d->post();

#ifdef DEFEAT_RACE_CONDITION
    if (!master->ReleaseEditHeadersSemaphore(this))
    {
#ifdef DEBUG_akkana
        printf("MailFilterRulesView: Couldn't release semaphore!\n");
#endif
        if (newhdr)
            XtFree(newhdr);
        return;
    }
#endif /* DEFEAT_RACE_CONDITION */

    //
    // The rest of this fcn: try to reset the scope to something reasonable,
    // since it can't stay on "Customize...", and update all the opt menus.
    //
    char* thisbtn = 0;
    Widget callerPopup = XtParent(w);

    //
    // Refresh all the option menus in case new headers have been added:
    //
    int num;
    int calledFromNum = -1;
    for (num = 0; num < m_stripCount; ++num)
    {
        // Figure out if this is the rules line we were called from,
        // based on the popup widget:
        if (callerPopup == m_scopeOptPopup[num])
            calledFromNum = num;

        (void)make_attribPopup(m_scopeOptPopup[num], 0, 0,
                               num+1, scopeOpt_cb, TRUE);
    }

    if (calledFromNum < 0)
    {
#ifdef DEBUG_akkana
        printf("Couldn't find which line we were called from\n");
#endif
        goto free_ret;
    }

    // Good, we know which popup we have to change. Get the new menu items:
    int numChildren;
    WidgetList childrenList;
    XtVaGetValues(m_scopeOptPopup[calledFromNum],
                  XmNchildren, &childrenList,
                  XmNnumChildren, &numChildren,
                  0);

    if (newhdr == 0)
    {
        XtVaSetValues(m_scopeOpt[calledFromNum],
                      XmNmenuHistory, childrenList[0],
                      0);
        scopeOpt_cb(childrenList[0], (XtPointer)this, NULL);
        goto free_ret;
    }

    // Find the widget matching the string returned from the dialog:
    for (num = 0; num < numChildren; ++num)
    {
        XmString xmstr;
        XtVaGetValues(childrenList[num], XmNlabelString, &xmstr, 0);
        XmStringGetLtoR(xmstr, XmFONTLIST_DEFAULT_TAG, &thisbtn);
        if (thisbtn && !XP_STRCMP(thisbtn, newhdr))
        {
            // Set the option menu to show that string
            XtVaSetValues(m_scopeOpt[calledFromNum],
                          XmNmenuHistory, childrenList[num],
                          0);
            scopeOpt_cb(childrenList[num], (XtPointer)this, NULL);
            break;
        }
    }

free_ret:
    if (newhdr) XtFree(newhdr);
    if (thisbtn) XtFree(thisbtn);
}

void
XFE_MailFilterRulesView::newFolder_cb(Widget w, 
									  XtPointer clientData, 
									  XtPointer calldata)
{
  XFE_MailFilterRulesView *obj = (XFE_MailFilterRulesView *) clientData;
  obj->newFolderCB(w, calldata);
}

void
XFE_MailFilterRulesView::newFolderCB(Widget , XtPointer)
{
    XFE_FolderPromptDialog* fpd
        = new XFE_FolderPromptDialog(getToplevel()->getBaseWidget(),
                                     "newFolderDialog",
                                     ViewGlue_getFrame(getContext()),
                                     getToplevel());

    // Would be nice: get the parent of the currently selected folder,
    // so by default the new folder will be created as a peer.

    MSG_FolderInfo* finfo = m_folderDropDown->getSelectedFolder();
    MSG_FolderInfo* newFolder = fpd->prompt(finfo);

    // This unfortunately doesn't work because we have to get the
    // folder dropdown to refresh its folder list!
    if (newFolder != 0)
    {
#ifdef DEBUG_akkana
        printf("Trying to sync folder list ...\n");
#endif
        m_folderDropDown->resyncDropdown();
        m_folderDropDown->selectFolder(newFolder);
    }
}

void
XFE_MailFilterRulesView::addRow_cb(Widget w, 
								   XtPointer clientData, 
								   XtPointer calldata)
{
  XFE_MailFilterRulesView *obj = (XFE_MailFilterRulesView *) clientData;
  obj->addRowCB(w, calldata);
}

void
XFE_MailFilterRulesView::editHdr_cb(Widget w,
                                    XtPointer clientData,
                                    XtPointer /*calldata*/ )
{
    XFE_MailFilterRulesView* mfrv = (XFE_MailFilterRulesView *) clientData;
    if (mfrv)
        mfrv->editHeaders(w);
}

void
XFE_MailFilterRulesView::addRowCB(Widget , XtPointer)
{
  if (m_stripCount < 5) {
	m_stripCount++;
	makeRules(m_rulesRC, m_stripCount);
  }/* if */
}


void
XFE_MailFilterRulesView::delRow_cb(Widget w, 
								   XtPointer clientData, 
								   XtPointer calldata)
{
  XFE_MailFilterRulesView *obj = (XFE_MailFilterRulesView *) clientData;
  obj->delRowCB(w, calldata);
}

void
XFE_MailFilterRulesView::delRowCB(Widget, XtPointer)
{
  if (m_stripCount>1) {
    int num = m_stripCount-1;
    XtUnmanageChild(m_content[num]);
    XtDestroyWidget(m_content[num]);
    XtDestroyWidget(m_strip[num]);
    XtDestroyWidget(m_scopeText[num]);
    XtDestroyWidget(m_rulesLabel[num]);
    XtDestroyWidget(m_scopeOpt[num]);
    XtDestroyWidget(m_whereOpt[num]);
    XtDestroyWidget(m_whereLabel[num]);
    m_content[num] = 0;
    m_stripCount--;
  }

  // Doesn't userData also need to be deleted?
}

void
XFE_MailFilterRulesView::resetData()
{
  /* No real filter is involved
   */
  MSG_SearchValue value;
  MSG_FolderInfo *info;
  const char *this_folder_name;

  /* Name 
   */
  fe_SetTextField(m_filterName, "");

  /* Rules
   */
  while (m_stripCount > 1)
    delRow_cb(NULL, (XtPointer) this, (XtPointer) NULL);

  value.attribute = attribSender ;
  value.u.string = XP_STRDUP("");
  setRulesParams(0, 
			     attribSender,
			     opContains, value);
  XP_FREE(value.u.string);

  /* Then
   */
  int num_inboxes;

  num_inboxes = MSG_GetFoldersWithFlag(XFE_MNView::getMaster(),
									   MSG_FOLDER_FLAG_INBOX,
									   NULL, 0);
  if (num_inboxes == 1) {
	  MSG_GetFoldersWithFlag(XFE_MNView::getMaster(),
							 MSG_FOLDER_FLAG_INBOX,
							 &info, 1);

	  this_folder_name = MSG_GetFolderNameFromID(info);
  }/* if */
  else
	  this_folder_name = NULL;

  setActionParams(acMoveToFolder,
				  (void *) this_folder_name);
  /* Desc
   */
  fe_SetTextField(m_despField, "");

  /* Filter: on
   */
  /* UI
   */
  m_curFilterOn = True;
  XtVaSetValues(m_filterOnBtn, XmNset, True, NULL);
  XtVaSetValues(m_filterOffBtn, XmNset, False, NULL);
}

void
XFE_MailFilterRulesView::displayData()
{
  MSG_Filter *f;
  char *text;
  int32 numTerms;
  int i;
  XP_Bool enabled;
  MSG_SearchAttribute attrib;
  MSG_SearchOperator op;
  MSG_SearchValue value;
  MSG_RuleActionType type;
  MSG_Rule *rule;
  MSG_FilterError err;

  if (err = MSG_GetFilterAt(m_filterlist, 
							(MSG_FilterIndex) m_at, &f))
	  {
#ifdef DEBUG_tao_
		  printf("\n  MSG_GetFilterAt: err=%d", err);
#endif
		  return;
	  }
  
  /* Set name
   */
  MSG_GetFilterName(f, &text); 
  fe_SetTextField(m_filterName, text);
  
  /* Set Description
   */
  MSG_GetFilterDesc(f, &text); 
  fe_SetTextField(m_despField, text);

  /* Rule
   */
  if (err = MSG_GetFilterRule(f, &rule))
	  {
#ifdef DEBUG_tao_
		  printf("\n  MSG_GetFilterAt:err=%d", err);
#endif
		  return;
	  }

  /* Terms
   */
  if (err = MSG_RuleGetNumTerms(rule, &numTerms))
	  {
#ifdef DEBUG_tao_
		  printf("\n  MSG_RuleGetNumTerms:err=%d", err);
#endif
		  return;
	  }

  XP_ASSERT(numTerms > 0 && numTerms < 6);
  
  for (i = 0; i < numTerms; i++) {
    /* create more strips if necessary
     */
    if (m_stripCount <= i) {
      m_stripCount++;
      makeRules(m_rulesRC, m_stripCount);
    }/* if */

    /* display term
     */
    char* customHdrs;
    MSG_RuleGetTerm(rule, i, &attrib, &op, &value,
                    &m_booleanAnd, &customHdrs);
#ifdef DEBUG_akkana
    if (attrib == attribOtherHeader)
        printf("attribOtherHeaders: custom header was '%s'\n", customHdrs);
#endif
    setRulesParams(i, attrib, op, value, customHdrs);
    if (i > 0)
        setOneBoolean(i);
  }/* for i */

  /* Now that we've gotten at least one m_booleanAnd,
   * make sure the option menu reads right:
   */
  Widget booloption = m_booleanAnd ? m_optionAnd : m_optionOr;
  if (booloption)
      XtVaSetValues(m_optionAndOr,
                    XmNmenuHistory, m_booleanAnd ? m_optionAnd : m_optionOr,
                    0);

  /* delete redundant strips
   */
  for(; i < m_stripCount; i++) {
      delRow_cb(NULL, (XtPointer) this, (XtPointer)NULL);
      m_stripCount--;
  }/* for i */

  void *value2;  
  MSG_RuleGetAction(rule, &type, &value2);
  setActionParams(type, value2);

  /* Filter On/Off
   */
  MSG_IsFilterEnabled(f, &enabled);  
  if (enabled) {
    m_curFilterOn = True;
    XtVaSetValues(m_filterOnBtn, XmNset, True, NULL);
    XtVaSetValues(m_filterOffBtn, XmNset, False, NULL);
  }/* if */
  else {
    m_curFilterOn = False;
    XtVaSetValues(m_filterOffBtn, XmNset, True, NULL);
    XtVaSetValues(m_filterOnBtn, XmNset, False, NULL);
  }/* else */
}

void
XFE_MailFilterRulesView::turnOnOff(Widget w, XtPointer clientData, XtPointer calldata)
{
  XFE_MailFilterRulesView *obj = (XFE_MailFilterRulesView *) clientData;
  obj->turnOnOffCB(w, calldata);
}

void
XFE_MailFilterRulesView::turnOnOffCB(Widget w, XtPointer /* calldata */)
{
  if (!w)
    return;

  if (w == m_filterOnBtn)
	  m_curFilterOn = True;
  else
	  m_curFilterOn = False;

}

void
XFE_MailFilterRulesView::andOr_cb(Widget w, XtPointer clientData, XtPointer)
{
    XFE_MailFilterRulesView* view = (XFE_MailFilterRulesView*)clientData;
    view->setAllBooleans((strcmp("matchAny", XtName(w)) != 0));
}

// change all the the2 "ands" to "ors" or vice versa
void
XFE_MailFilterRulesView::setAllBooleans(Boolean andP)
{
    m_booleanAnd = andP;
    if (m_stripCount <= 1)
        return;
    for (int i=1; i < m_stripCount; ++i)
        setOneBoolean(i);
}

void
XFE_MailFilterRulesView::setOneBoolean(int which)
{
    extern int XFE_SEARCH_BOOL_AND_THE, XFE_SEARCH_BOOL_OR_THE;
    char* newlabel;
    if (m_booleanAnd)
        newlabel = XP_GetString(XFE_SEARCH_BOOL_AND_THE);
    else
        newlabel = XP_GetString(XFE_SEARCH_BOOL_OR_THE);
    XmString xmnewlabel = XmStringCreateSimple(newlabel);
    XtVaSetValues(m_rulesLabel[which],
                  XmNlabelString, xmnewlabel,
                  0);
    XmStringFree(xmnewlabel);
}

