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
   SearchRuleView.h -- creates each rule statement.
   Created: Dora Hsu <dora@netscape.com>, 15-Dec-96 
 */



#include "MNSearchView.h"
#include "SearchRuleView.h"
#include "EditHdrDialog.h"

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/LabelG.h>
#include <Xm/RowColumn.h>
#include <Xm/TextF.h>
#include <Xm/Frame.h>
#include <Xm/PushBG.h>
#include <time.h> // For search date
#include <xpgetstr.h> /* for XP_GetString() */

#include <Xfe/Xfe.h>

#include <felocale.h>
#include "xp_time.h"
#include "xplocale.h"


extern int XFE_SEARCH_INVALID_DATE;
extern int XFE_SEARCH_INVALID_MONTH;
extern int XFE_SEARCH_INVALID_DAY;
extern int XFE_SEARCH_INVALID_YEAR;


//--- notification ---
const char *XFE_SearchRuleView::activateSearch = "XFE_SearchRuleView::activateSearch";
//--- C utilities function ---

extern "C" Widget 
fe_make_option_menu(Widget toplevel, Widget parent, char* widgetName, Widget *popup)
{
  Cardinal ac;
  Arg      av[10];
  Widget option_menu;
  Visual*  v = 0;
  Colormap cmap = 0;
  Cardinal depth = 0;
 
  XtVaGetValues(toplevel, XtNvisual, &v, XtNcolormap, &cmap,
        XtNdepth, &depth, 0);
 
  ac = 0;
  XtSetArg(av[ac], XmNvisual, v); ac++;
  XtSetArg(av[ac], XmNdepth, depth); ac++;
  XtSetArg(av[ac], XmNcolormap, cmap); ac++;
  *popup= XmCreatePulldownMenu(parent, widgetName, av, ac);

  ac = 0;
  XtSetArg(av[ac], XmNsubMenuId, *popup); ac++;
  XtSetArg(av[ac], XmNmarginWidth, 0); ac++;
  option_menu = XmCreateOptionMenu(parent, widgetName, av, ac);
  XtUnmanageChild(XmOptionLabelGadget(option_menu));

  return option_menu;
}

extern "C" void
fe_get_option_size ( Widget optionMenu,
                        Dimension *retWidth, Dimension *retHeight )
{
   Dimension width = 0, height = 0;
   Dimension mh=0, mw=0, ml=0, mr=0, mt=0, mb=0 ; /*margin*/
   Dimension st=0, bw=0, ht=0;
   Dimension space=0;
   WidgetList children;
   Cardinal numChildren;
   Widget popupW;
   Widget btn;
   
   *retWidth = 0;
   *retHeight = 0;

   XtVaGetValues(optionMenu, 
                XmNsubMenuId, &popupW, 0);

   XtVaGetValues(popupW,
		XmNchildren, &children, 
		XmNnumChildren, &numChildren, 0);

    if ( numChildren > 0 )
    {
    btn = children[0];
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
   *retWidth= width + (mw+ml+mr+bw+st+ht + space) * 2;
  }

}

extern "C" Boolean
fe_set_current_attribute_option(Widget parent, int btnPos )
{
   unsigned char type;
   WidgetList children;
   Cardinal     numChildren;
   Widget       pulldownW;

   if ( XmIsRowColumn(parent) )
   {
        XtVaGetValues(parent, XmNrowColumnType, &type,
                        XmNsubMenuId, &pulldownW,
                        0);

        if ( type == XmMENU_OPTION && pulldownW)
        {
                XtVaGetValues(pulldownW,
                        XmNchildren, &children,
                        XmNnumChildren, &numChildren,0 );

                if ( (int)numChildren > btnPos && btnPos >= 0)
                {
                        XtVaSetValues(parent, XmNmenuHistory,
                                children[btnPos], 0);
                        return True;
                }
        }
   }
   return False;
}

/*---------------------------------------------------------------
 This part should be belonging to libmsg. This part is
 copied from winfe
----------------------------------------------------------------*/
extern "C"
MSG_SearchError MSG_GetValuesForAttribute( MSG_ScopeAttribute /*scope*/, 
   MSG_SearchAttribute attrib, 
   MSG_SearchMenuItem *items, 
   uint16 *maxItems)
{
   int32 status;
        const int32 aiStatiValues[] = { MSG_FLAG_READ,
                                        MSG_FLAG_REPLIED,
										MSG_FLAG_FORWARDED};

        const MSG_PRIORITY aiPriorityValues[] = { 
                                                MSG_LowestPriority,
                                                MSG_LowPriority,
                                                MSG_NormalPriority,
                                                MSG_HighPriority,
                                                MSG_HighestPriority };

        uint16 nStati = sizeof(aiStatiValues) / sizeof(int32);
        uint16 nPriorities = sizeof(aiPriorityValues) / sizeof(MSG_PRIORITY);
        uint16 i;

        switch (attrib) {
        case attribPriority:
                for ( i = 0; i < nPriorities && i < *maxItems; i++ ) {
                        items[i].attrib = (int16) aiPriorityValues[i];
                        MSG_GetPriorityName( (MSG_PRIORITY) items[i].attrib, 
                                 items[i].name, 
				 sizeof( items[i].name) / sizeof(char) );
                        items[i].isEnabled = TRUE;
                }
                *maxItems = i;
                if ( i == nPriorities ) {
                        return SearchError_Success;
                } else {
                        return SearchError_ListTooSmall;
                }

        case attribMsgStatus:
                for ( i = 0; i < nStati &&  i< *maxItems; i++ ) {

                        items[i].attrib = (int16) aiStatiValues[i];
			status = (int32) items[i].attrib;
                        MSG_GetStatusName( status, 
                                           items[i].name, 
					 sizeof( items[i].name) / sizeof(char) );
                        items[i].isEnabled = TRUE;
                }
                *maxItems = i;
                if ( i == nStati ) {
                        return SearchError_Success;
                } else {
                        return SearchError_ListTooSmall;
                }
        
        default:
                *maxItems = 0;
                return SearchError_InvalidAttribute;
        }
}

// --------- Constructor 

XFE_SearchRuleView::XFE_SearchRuleView(XFE_Component *toplevel_component,
                                 Widget parent,
                                 XFE_View *parent_view,
				 MWContext *context,
                                 MSG_Pane *p,
			 	 char *title,
				 void *folderInfo,
				 MSG_ScopeAttribute curScope,
			         uint16 curAttrib)
  : XFE_MNView(toplevel_component, parent_view, context, p)
{
  XP_ASSERT(p!= NULL);
  {

	initializeDataMember();
	m_curScope = curScope;
	m_curAttribIndex = curAttrib;
    m_ruleTitle = (char*)XP_STRDUP(title);
  }

  {
  /* Get Visual, Colormap, and Depth */
  XtVaGetValues(getToplevel()->getBaseWidget(),
                 XmNvisual, &m_visual,
                 XmNcolormap, &m_cmap,
                 XmNdepth, &m_depth,
                 NULL);
  }
  m_opt_valueField = NULL;
  m_date_valueField = NULL;
  m_date_textField = NULL;
  m_text_valueField = NULL;
  if ( curScope == scopeLdapDirectory )
    m_dir = (DIR_Server*)folderInfo;
  else 
    m_folderInfo = (MSG_FolderInfo*)folderInfo; 
  createWidgets(parent);
  getToplevel()->registerInterest(
                XFE_MNSearchView::scopeChanged, this,
                (XFE_FunctionNotification)scopeChanged_cb);

  getToplevel()->registerInterest(XFE_Component::afterRealizeCallback,
             this,
             (XFE_FunctionNotification)afterRealizeWidget_cb); 

}

XFE_SearchRuleView::~XFE_SearchRuleView()
{
  getToplevel()->unregisterInterest(
                XFE_MNSearchView::scopeChanged, this,
                (XFE_FunctionNotification)scopeChanged_cb);

  getToplevel()->unregisterInterest(XFE_Component::afterRealizeCallback,
             this,
             (XFE_FunctionNotification)afterRealizeWidget_cb);
}

void
XFE_SearchRuleView:: initializeDataMember()
{
	// Init data here
  m_height = 0;
}

void
XFE_SearchRuleView::createWidgets(Widget parent)
{
  MSG_SearchMenuItem *attrItems;
  MSG_SearchMenuItem *opItems;
  MSG_SearchValueWidget valueType;
  MSG_SearchError err;
  uint16 attrNum;
  uint16 opNum;
  DIR_Server *dirs[1];
  MSG_FolderInfo *folderInfos[1];

  opItems = (MSG_SearchMenuItem*)
			XP_CALLOC(kNumOperators, sizeof(MSG_SearchMenuItem));		
  opNum = kNumOperators;


  if ( m_curScope != scopeLdapDirectory )
  {
  	folderInfos[0]= (MSG_FolderInfo *)m_folderInfo;
    MSG_GetNumAttributesForSearchScopes(fe_getMNMaster(),
                                        m_curScope,
                                        (void**)folderInfos, 1,
                                        &attrNum); 
    attrItems = (MSG_SearchMenuItem *)XP_CALLOC(attrNum,
                                                sizeof(MSG_SearchMenuItem));

  	err = MSG_GetAttributesForSearchScopes(fe_getMNMaster(), m_curScope, 
				(void**)folderInfos, 1, attrItems, &attrNum);

  }
  else
  {
	if ( m_dir ) 
        { 
  		dirs[0]= (DIR_Server*) m_dir;
        MSG_GetNumAttributesForSearchScopes(fe_getMNMaster(),
                                            m_curScope,
                                            (void**)dirs, 1,
                                            &attrNum); 
        attrItems = (MSG_SearchMenuItem *)XP_CALLOC(attrNum,
                                                   sizeof(MSG_SearchMenuItem));
  		err = MSG_GetAttributesForSearchScopes(fe_getMNMaster(), m_curScope, 
				(void**)dirs, 1, attrItems, &attrNum);
	}
	else return;

  }
  if ( err != SearchError_Success ) return;

  // water fall display, display the next attribute 

  // Caller needs to garuntee that m_curAttrib is less than attrNum

  if ((uint16)m_curAttribIndex >= attrNum ) m_curAttribIndex =0 ; 

  if (m_curScope != scopeLdapDirectory )
  {

  	err = MSG_GetOperatorsForSearchScopes(fe_getMNMaster(), m_curScope, 
			(void**)folderInfos, 1,
			(MSG_SearchAttribute)(attrItems[m_curAttribIndex].attrib),
                        opItems, &opNum);
  }
  else
  {
  	err = MSG_GetOperatorsForSearchScopes(fe_getMNMaster(), m_curScope, 
			(void**)dirs, 1,
			(MSG_SearchAttribute)(attrItems[m_curAttribIndex].attrib),
                        opItems, &opNum);
  }

  if ( err == SearchError_Success ) 
  {

  err = MSG_GetSearchWidgetForAttribute(
						(MSG_SearchAttribute)(attrItems[m_curAttribIndex].attrib),
						&valueType);

  if ( err == SearchError_Success ) 
  {

  buildRules(parent, m_curScope,  m_curAttribIndex, attrItems, attrNum,
             opItems, opNum, valueType );


  fe_set_current_attribute_option( m_attr_opt, m_curAttribIndex);
  fe_set_current_attribute_option( m_op_opt, 0);
  }
  }

  /* FREE CALLOC data */
  XP_FREE(attrItems);

  XP_FREE(opItems);
}


void
XFE_SearchRuleView::setLabelWidth(Dimension width)
{
 // This method should be called after view
 // is created so that the layout would be fine

 if (m_label)
  XtVaSetValues(m_label, XmNwidth, width,
          XmNalignment, XmALIGNMENT_END, 
          XmNrecomputeSize, False,
          0 );

  if ( m_attr_opt )
  XtVaSetValues( m_attr_opt, XmNleftOffset, width, 0);
}

void
XFE_SearchRuleView::buildRules(Widget parent,
	 	MSG_ScopeAttribute curScope,
  		uint16 newAttr,   
		MSG_SearchMenuItem attrItems[], uint16 attrNum,
		MSG_SearchMenuItem opItems[], uint16 opNum,
		MSG_SearchValueWidget valueType )
{
  Arg      av[20];
  Widget formW;

  m_curScope = curScope;
  m_attr = (MSG_SearchAttribute)(attrItems[newAttr].attrib);
  m_op = (MSG_SearchOperator)(opItems[0].attrib);
  m_type = valueType;

  if ( !getBaseWidget() )
  {
    formW = XmCreateForm(parent, "searchRulesForm", av, 0);
    setBaseWidget(formW);
  }
  else formW = getBaseWidget();

  m_label = XtVaCreateManagedWidget( 
			m_ruleTitle, 
			xmLabelGadgetClass, 
			formW,
			NULL ); 

  /* Attrib */
  m_attr_opt = fe_make_option_menu(getToplevel()->getBaseWidget(),
			formW, "searchAttrOpt", &m_attr_pop);
  buildPulldownMenu(m_attr_pop,
                 attrItems, attrNum,
                 attribOptionCallback, 
				 (XtPointer)this);

  /* Operator */
  m_op_opt = fe_make_option_menu(getToplevel()->getBaseWidget(),
				formW, "searchOpOpt", &m_op_pop);

  buildPulldownMenu(m_op_pop,
		opItems, opNum,
		operatorOptionCallback, 
	    (XtPointer)this);


  /* Calculate form size */
  calculateSize(); 

  /* Values */
  m_value = replaceValueField(NULL,
				formW,
				valueType,
				NULL, NULL);
  //XtManageChild(m_value);
  m_type = valueType;

  /* Doing layout */
  if ( m_label )
  XtVaSetValues( m_label,
	XmNheight, m_height,
        XmNtopAttachment, XmATTACH_FORM,
        XmNbottomAttachment, XmATTACH_NONE,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_NONE, 0);

		
  if ( m_attr_opt )
  XtVaSetValues( m_attr_opt,
        XmNtopAttachment, XmATTACH_FORM,
        XmNbottomAttachment, XmATTACH_NONE,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_NONE,
        0);

  if ( m_op_opt )
  XtVaSetValues( m_op_opt,
        XmNtopAttachment, XmATTACH_FORM,
        XmNbottomAttachment, 
	  (m_attr_opt? XmATTACH_OPPOSITE_WIDGET: XmATTACH_NONE),
        XmNbottomWidget, m_attr_opt,
        XmNleftAttachment, 
	  (m_attr_opt? XmATTACH_WIDGET: XmATTACH_NONE),
        XmNleftWidget, m_attr_opt,
        XmNrightAttachment, XmATTACH_NONE, 0);

  if ( m_value )
  {
  XtVaSetValues( m_value,
        XmNtopAttachment, 
	  (m_op_opt? XmATTACH_OPPOSITE_WIDGET: XmATTACH_NONE),
        XmNtopWidget, m_op_opt,
        XmNbottomAttachment, 
	  (m_op_opt? XmATTACH_OPPOSITE_WIDGET: XmATTACH_NONE),
        XmNbottomWidget, m_op_opt,
        XmNleftAttachment, 
	  (m_op_opt? XmATTACH_WIDGET: XmATTACH_NONE),
        XmNleftWidget, m_op_opt,
        XmNrightAttachment, XmATTACH_FORM, 0);

  }
  XtVaSetValues(formW, XmNheight, m_height, 
			    XmNtopAttachment, XmATTACH_NONE,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
				XmNrightAttachment, XmATTACH_FORM, 
				0); 

  XtManageChild(m_attr_opt);
  XtManageChild(m_op_opt);
  XtManageChild(formW);
}

void XFE_SearchRuleView::changeLabel(char* newlabel)
{
    XmString xmlabl = XmStringCreateSimple(newlabel);
    XtVaSetValues(m_label, XmNlabelString, xmlabl, 0);
    XmStringFree(xmlabl);
}

void
XFE_SearchRuleView::buildPulldownMenu(Widget pulldownParent,
                        MSG_SearchMenuItem items[], uint16 itemNum,
                        XtCallbackProc activeCB, XtPointer clientData)
{
  int i;
  XmString xmStr;
  Arg av[20];
  int ac;
  Widget btn;
  WidgetList childrenList;
  int numChildren;
  int attribData;
  int iitemNum = (int)itemNum;

  /* Clean up first */

  XtVaGetValues( pulldownParent, XmNchildren, &childrenList,
                XmNnumChildren, &numChildren, 0);

  for ( i = 0; i < numChildren; i++ )
  {
        XtDestroyWidget(childrenList[i]);
  }

  /* Create new ones... */

  Boolean otherHdrs = FALSE;
  for ( i = 0; i < iitemNum; i++ )
  {
    attribData = (int)(items[i].attrib);

    if (!otherHdrs && attribData == attribOtherHeader)
    {
        XtVaCreateManagedWidget("sep", xmSeparatorGadgetClass, pulldownParent,
                                0);
        otherHdrs = TRUE;
    }

    xmStr = XmStringCreateSimple(items[i].name);

    ac = 0;
    XtSetArg(av[ac], XmNuserData, attribData); ac++;
    XtSetArg(av[ac], XmNlabelString, xmStr); ac++;
    btn = XmCreatePushButtonGadget(pulldownParent, "ruleBtn", av, ac);
    XtAddCallback(btn, XmNactivateCallback, activeCB, clientData);
    XtManageChild(btn);

    XmStringFree(xmStr);
  }

  if (pulldownParent == m_attr_pop)
  {
      // Add the "Edit Custom Hdr" button:
      XtVaCreateManagedWidget("sep", xmSeparatorGadgetClass, pulldownParent,
                              0);
      btn = XtVaCreateManagedWidget("editHdrBtn", xmPushButtonGadgetClass,
                                    pulldownParent,
                                    0);
      XtAddCallback(btn, XmNactivateCallback, editHdrCallback, this);
  }
}

void
XFE_SearchRuleView::attribOptionCallback(Widget w, 
			XtPointer clientData, XtPointer /*callData*/)
{
   XFE_SearchRuleView *obj = (XFE_SearchRuleView*)clientData;
   XtPointer attribData;
   int16 n;

   XtVaGetValues(w, XmNuserData, &attribData, 0);
   int iattribData = (int)attribData;
   n = (int16)iattribData;
   obj->setAttribOption(n);
}

void
XFE_SearchRuleView::createValueWidgetSet(Widget /*parent*/)
{
}

void
XFE_SearchRuleView::setAttribOption(int16 attrib)
{
   MSG_SearchError err;
   MSG_SearchValueWidget valueType;
   MSG_SearchMenuItem *opItems;
   uint16 opNum = kNumOperators; /* XP relies on this value to know array size */


  XtUnmanageChild(getBaseWidget());
  opItems = (MSG_SearchMenuItem*)
			XP_CALLOC(kNumOperators, sizeof(MSG_SearchMenuItem));		
   if ( m_attr != attrib)
   {
     XtUnmanageChild(m_attr_opt);
     XtUnmanageChild(m_op_opt);

     m_attr = (MSG_SearchAttribute)attrib;
     m_curAttribIndex = (uint16) m_attr;

     if (m_curScope != scopeLdapDirectory )
     {
     	MSG_FolderInfo *folderInfos[1];
     	folderInfos[0]= (MSG_FolderInfo *)m_folderInfo;
     	err = MSG_GetOperatorsForSearchScopes(fe_getMNMaster(), 
			m_curScope, (void**)folderInfos, 1,
			(MSG_SearchAttribute)m_attr,
                        opItems, &opNum);
     }
     else
     {
  	DIR_Server *dirs[1];

  	dirs[0]= (DIR_Server*) m_dir;
     	err = MSG_GetOperatorsForSearchScopes(fe_getMNMaster(), 
			m_curScope, (void**)dirs, 1,
			(MSG_SearchAttribute)m_attr,
                        opItems, &opNum);

     }

     if (err == SearchError_Success ) 
     {

     err = MSG_GetSearchWidgetForAttribute(m_attr, &valueType);
     if ( err == SearchError_Success  )
     {

     if ( needNewMenu(m_op_pop, opItems, opNum ))
     {
	XtUnmanageChild(m_op_opt);
	XtDestroyWidget(m_op_opt);
        m_op_opt = fe_make_option_menu(getToplevel()->getBaseWidget(),
				getBaseWidget(), "searchOpOpt", &m_op_pop);

        buildPulldownMenu(m_op_pop, opItems, opNum,
                    operatorOptionCallback, 
					(XtPointer)this);
        fe_set_current_attribute_option( m_op_opt, 0);
        /* Operator may have been changed */
        m_op = (MSG_SearchOperator)(opItems[0].attrib); /* Use the new one */
  XtVaSetValues( m_op_opt,
        XmNtopAttachment, XmATTACH_FORM,
        XmNbottomAttachment, 
	  (m_attr_opt? XmATTACH_OPPOSITE_WIDGET: XmATTACH_NONE),
        XmNbottomWidget, m_attr_opt,
        XmNleftAttachment, 
	  (m_attr_opt? XmATTACH_WIDGET: XmATTACH_NONE),
        XmNleftWidget, m_attr_opt,
        XmNrightAttachment, XmATTACH_NONE, 0);
     }

    XtManageChild(m_attr_opt);
    XtManageChild(m_op_opt);
     /* Build new value */
     m_value = replaceValueField(m_value, getBaseWidget(),
                                //XtParent(m_value),
                                valueType, NULL, (XtPointer)NULL);
     } /* success */
     } /* success*/
   }

    XtManageChild(getBaseWidget());
    if (valueType == widgetText)
        XmProcessTraversal(m_text_valueField, XmTRAVERSE_CURRENT);
    else if (valueType == widgetDate)
        XmProcessTraversal(m_date_textField, XmTRAVERSE_CURRENT);

  /* FREE CALLOC data */
  XP_FREE(opItems);
}

Boolean 
XFE_SearchRuleView::needNewMenu(Widget pulldownParent,
                        MSG_SearchMenuItem *opItems,
                        uint16 opNum )
{
  int numChildren;
  WidgetList childrenList;
  XtPointer data;
  int idata;
  uint16 data16;
  int i, j;
  Boolean found;
  int iopNum = (int)opNum;

  XtVaGetValues( pulldownParent, XmNchildren, &childrenList,
                XmNnumChildren, &numChildren, 0);

//  if ( numChildren != opNum ) return True;

  for ( i = 0; i < numChildren; i++ )
  {
        XtVaGetValues( childrenList[i], XmNuserData, &data, 0 );
	idata = (int)data;
	data16= (uint16)idata;

        found = True;
        for ( j = 0; j < iopNum; j++ )
        {
          if ( data16 != opItems[j].attrib )
          {
                found = False;
                break;
          }
        }
        if ( !found ) return True;
  }
  return False;
}

void
XFE_SearchRuleView::operatorOptionCallback(Widget w, 
			XtPointer clientData, XtPointer /*callData*/)
{
   XFE_SearchRuleView *obj = (XFE_SearchRuleView*)clientData;
   XtPointer opData;
   int iopData;
   int16 n;

   XtVaGetValues(w, XmNuserData, &opData, 0);
   iopData = (int)opData;

   n = (int16)iopData;
   obj->m_op= (MSG_SearchOperator)n;
}

void
XFE_SearchRuleView::valueOptionCallback(Widget w, 
			XtPointer clientData, XtPointer /*callData*/)
{
   XFE_SearchRuleView *obj = (XFE_SearchRuleView*)clientData;
   XtPointer valueData;
   int ivalueData;
   uint16 n;

   XtVaGetValues(w, XmNuserData, &valueData, 0);
   ivalueData = (int)valueData;
   n = (int16)ivalueData;
   obj->m_value_option = n;
}

void
XFE_SearchRuleView::calculateSize()
{
  Dimension width, height;
  WidgetList childrenList;
  int numChildren;

  XtVaGetValues( m_attr_pop,
                XmNchildren, &childrenList,
                XmNnumChildren, &numChildren, 0 );

  if ( numChildren > 0 )
  {
  	fe_get_option_size(m_attr_opt, 
                        &width, &height );

  	if (width > m_width) m_width = width;
  	if (height > m_height) m_height = height;
  }

  XtVaGetValues( m_op_pop,
                XmNchildren, &childrenList,
                XmNnumChildren, &numChildren, 0 );

  if ( numChildren > 0 )
  {
  	fe_get_option_size(m_op_opt,
                        &width, &height );

  	if (width > m_width) m_width = width+20;
  	if (height > m_height) m_height = height+20;
  }
}



Widget
XFE_SearchRuleView::replaceValueField ( Widget valueField, 
		Widget parent,
                MSG_SearchValueWidget valueType,
                XtCallbackProc callback, XtPointer clientData)
{
  Arg av[20];
  int ac;
  Widget newField = NULL;
  MSG_SearchMenuItem *items;
  uint16 maxItems = 16;
  Widget lblField;
  Visual*  v = 0;
  Colormap cmap = 0;
  Cardinal depth = 0;
  if (!valueField) 
  {
   // Create everything for the first time...

   // Create Menu Widget
   Widget popup;

   m_opt_valueField = fe_make_option_menu(getToplevel()->getBaseWidget(),
				parent,
                                "op_valueField", &popup);
   
   // Create Date Widget
   ac = 0;
   m_date_valueField = XmCreateForm(parent, "date_valueField", av, ac );
   m_date_textField= fe_CreateTextField(m_date_valueField, "textValueField", av, ac );
   XtAddCallback(m_date_textField, XmNactivateCallback, typeActivateCallback, this);
   lblField = XmCreateLabelGadget(m_date_valueField, "labelValueField", av, ac);
   XtVaSetValues(m_date_textField,
       XmNtopAttachment, XmATTACH_FORM,
       XmNbottomAttachment, XmATTACH_FORM,
       XmNleftAttachment, XmATTACH_FORM,
       XmNrightAttachment, XmATTACH_FORM,
       XmNrightOffset, XfeWidth(lblField) + 6,
       0 );
   XtVaSetValues(lblField,
       XmNtopAttachment, XmATTACH_FORM,
       XmNbottomAttachment, XmATTACH_FORM,
       XmNleftAttachment, XmATTACH_NONE,
       XmNrightAttachment, XmATTACH_FORM,
       0 );
  XtManageChild(m_date_textField);
  XtManageChild(lblField);

   // Widget Text
   ac = 0;
   m_text_valueField= fe_CreateTextField(parent, "text_valueField", av, ac );

   XtAddCallback(m_text_valueField, XmNactivateCallback, typeActivateCallback, this);
   // set up attachment
#if 0
    XtVaSetValues( m_opt_valueField,
        XmNleftWidget, m_op_opt,
        XmNleftAttachment, XmATTACH_WIDGET,
        XmNrightAttachment, XmATTACH_FORM,
        XmNbottomWidget, m_op_opt,
        XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
        XmNtopWidget, m_op_opt, 
        XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
        0);
    XtVaSetValues( m_date_valueField,
        XmNleftWidget, m_op_opt,
        XmNleftAttachment, XmATTACH_WIDGET,
        XmNrightAttachment, XmATTACH_FORM,
        XmNbottomWidget, m_op_opt,
        XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
        XmNtopWidget, m_op_opt, 
        XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
        0);
    XtVaSetValues( m_text_valueField,
        XmNleftWidget, m_op_opt,
        XmNleftAttachment, XmATTACH_WIDGET,
        XmNrightAttachment, XmATTACH_FORM,
        XmNbottomWidget, m_op_opt,
        XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
        XmNtopWidget, m_op_opt, 
        XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
        0);
#endif
  }
  else 
  {
#if 0
 	ac = 0;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_NONE); ac++;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_NONE); ac++;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_NONE); ac++;
  	XtSetValues(valueField, av, ac);
#endif
	XtUnmanageChild(valueField);
  }

  m_type = valueType;
  switch(valueType )
  {
        case widgetMenu:
	{
	if ( m_opt_valueField)
    	{
	// Get the popup widget, and destroy them for creating new ones

#if 0
 	ac = 0;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_NONE); ac++;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_NONE); ac++;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_NONE); ac++;
        XtSetValues(m_opt_valueField, av,ac);
#endif
        Widget popup;

	XtUnmanageChild(m_opt_valueField);
	XtDestroyWidget(m_opt_valueField);
        m_opt_valueField = fe_make_option_menu(getToplevel()->getBaseWidget(),
				parent,
                                "op_valueField", &popup);

	popup = NULL;
	XtVaGetValues(m_opt_valueField, XmNsubMenuId, &popup, 0 );
	if ( popup ) XtDestroyWidget(popup);
	XtVaSetValues(m_opt_valueField, XmNsubMenuId, NULL, 0 );
	XtVaGetValues(getToplevel()->getBaseWidget(), 
	XtNvisual, &v, XtNcolormap, &cmap,
        XtNdepth, &depth, 0);

  ac = 0;
  XtSetArg(av[ac], XmNvisual, v); ac++;
  XtSetArg(av[ac], XmNdepth, depth); ac++;
  XtSetArg(av[ac], XmNcolormap, cmap); ac++;
  popup= XmCreatePulldownMenu(parent, "Pulldown", av, ac);

  items = (MSG_SearchMenuItem*) XP_CALLOC(maxItems, sizeof(MSG_SearchMenuItem));		
  MSG_GetValuesForAttribute(m_curScope,
                                m_attr, items, &maxItems );

  buildPulldownMenu(popup, items, maxItems,
        valueOptionCallback, 
	(XtPointer)this);

  XtVaSetValues(m_opt_valueField, XmNsubMenuId, popup, 0);
  /* FREE CALLOC data */
  XP_FREE(items);
                }
	}

	newField = m_opt_valueField;
        break;
        case widgetDate:
        {
/* Create default date time */
		char dateStr[40], tmp[16];
                time_t now = XP_TIME();
                XP_StrfTime(m_contextData,
                                        dateStr, sizeof(dateStr),
                                        XP_DATE_TIME_FORMAT,
                                        localtime(&now));
		XP_STRNCPY_SAFE(tmp, dateStr, 9);

                fe_SetTextField(m_date_textField, tmp);
	  
	  if ( callback )
                 XtAddCallback(m_date_textField, XmNvalueChangedCallback, callback, clientData);
                }
	newField = m_date_valueField;
            break;
        case widgetText:
		if ( callback )
                XtAddCallback(m_text_valueField, XmNvalueChangedCallback,
                        callback, clientData);
		newField = m_text_valueField;
        	break;

      case widgetInt:
		if ( callback )
                XtAddCallback(m_text_valueField, XmNvalueChangedCallback,
                        callback, clientData);
		newField = m_text_valueField;
        	break;
	default:
        XP_ASSERT (0);
		return 0;
  }

 	ac = 0;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
	XtSetArg(av[ac], XmNbottomWidget, m_op_opt); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, m_op_opt); ac++;
	XtSetValues(newField,av,ac);
  XtSetMappedWhenManaged(newField, TRUE);
	ac = 0;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNleftWidget, m_op_opt); ac++;
	XtSetArg(av[ac], XmNleftOffset, 0);ac++;
	XtSetValues(newField,av,ac);
  XtSetMappedWhenManaged(newField, TRUE);
  XtManageChild(newField);

  return newField;
}

XFE_CALLBACK_DEFN(XFE_SearchRuleView,afterRealizeWidget)(XFE_NotificationCenter *,
                                 void *, void*)
{
    // put focus in text field:
    XmProcessTraversal(m_text_valueField, XmTRAVERSE_CURRENT);
}

void
XFE_SearchRuleView::resetSearchValue()
{
   
   if ( m_date_textField )
   { /* Reset date */
      char dateStr[40], tmp[16];
      time_t now = XP_TIME();
      XP_StrfTime(m_contextData,
                  dateStr, sizeof(dateStr),
                  XP_DATE_TIME_FORMAT,
                  localtime(&now));
      XP_STRNCPY_SAFE(tmp, dateStr, 9);

      fe_SetTextField(m_date_textField, tmp);

   }
   
   if ( m_text_valueField )
   { /* Reset text value */
	fe_SetTextFieldAndCallBack(m_text_valueField, "");

   }

#if 0
	/* Reset all the search rules */
	if ( m_type == widgetText )
    {
	  if (m_value && XmIsTextField(m_value))
	     fe_SetTextFieldAndCallBack(m_value, "");
    }
    else if ( m_type == widgetDate)
	{   Widget textW;

        textW = XtNameToWidget(m_value, "textValueField");
	    fe_SetTextFieldAndCallBack(textW, "");
    }
/* Don't change the option back at all ...
   Need to check with HI about this...
         else
	   fe_search_set_current_attribute_option(curlist->value, 0);
*/
#endif
}
	

XP_Bool
fe_getDate (char *string, int* mm, int* dd, int* yyyy)
{
  char *cp;

  cp = strtok(string, "/");
  if (cp && strspn(cp, "0123456789") == strlen(cp))
  { 
	*mm = atoi(cp);
	if (*mm > 12 || *mm < 1) *mm = -1;
  }
  else
  {
    *mm = -1;
#ifdef DEBUG_dora
    printf("error in month\n");
#endif
  }

  cp = strtok(NULL, "/");
  if (cp && strspn(cp, "0123456789") == strlen(cp))
  {
        *dd = atoi(cp);
	if (*dd > 31 || *dd < 1) *dd = -1;
  }
  else
  {
    *dd = -1;
#ifdef DEBUG_dora
    printf ("error in day\n");
#endif
  }

  cp = strtok(NULL, "\0");
  if (cp && strspn(cp, "0123456789") == strlen(cp))
  {
    	*yyyy = atoi(cp);
	if (*yyyy < 1900) *yyyy = -1;
  }
  else
  {
    *yyyy = -1;
#ifdef DEBUG_dora
    printf("error in year\n");
#endif
  }

#ifdef DEBUG_dora
  printf("mon = %d, day = %d, year = %d\n", *mm, *dd, *yyyy);
#endif

  if ( *mm == -1 || *dd == -1 || *yyyy == -1)
     return False;
  return True;

}

time_t 
fe_mkTime(int mon, int day, int year)
{
  time_t t;
  struct tm time_str;

          time_str.tm_year    = year - 1900;
          time_str.tm_mon     = mon-1;
          time_str.tm_mday    = day;
          time_str.tm_hour    = 0;
          time_str.tm_min     = 0;
          time_str.tm_sec     = 1;
          time_str.tm_isdst   = -1; // ignore daylight saving 

  t = mktime(&time_str);
#ifdef DEBUG_dora
  if (t == -1)
   printf("mktime error\n");
  else
   printf("time = %d\n", t);
#endif
  return t;
}

XP_Bool
XFE_SearchRuleView::getSearchTerm( MSG_SearchAttribute *attr, 
				MSG_SearchOperator *op, MSG_SearchValue *value,
                                   char* customHdr)
{
  Widget textW;
  char *string = 0;
  char *label;
  XP_Bool no_err = True;


	   if ( m_type == widgetText || m_type == widgetInt )
  	      string = fe_GetTextField(m_value);
	   else if ( m_type == widgetDate)
	   {
		string = NULL;
		textW = XtNameToWidget(m_value, "textValueField");
  	    	string = fe_GetTextField(textW);
		
		
       	    }

	   value->attribute = m_attr;
	   switch (m_attr)
	   {
		case attribDate:
		{
                  value->u.date = XP_ParseTimeString(string, False);
		  XtFree(string);
		}
		break;
		case attribPriority:
			value->u.priority = (MSG_PRIORITY)(m_value_option);
		break;
		case attribMsgStatus:
			value->u.msgStatus = m_value_option;
		break;
		case attribAgeInDays:
			value->u.age = XP_ATOI(string);
		break;
		case attribOtherHeader:
			value->u.string = string;
            label = getAttribLabel();
            strcpy(customHdr, label);
            XtFree(label);
		break;
		default:
 	   		value->u.string = XP_STRDUP(string);
	   }

       if (string)
           XtFree(string);

	*attr = m_attr;
	*op = m_op;

   return no_err;
}

// Result of getAttribLabel should be XtFree()ed
char*
XFE_SearchRuleView::getAttribLabel()
{
    Widget lablW;
    XtVaGetValues(m_attr_opt, XmNmenuHistory, &lablW, 0);
    XmString xmstr;
    XtVaGetValues(lablW, XmNlabelString, &xmstr, 0);
    char* lablstr;
    XmStringGetLtoR(xmstr, XmSTRING_DEFAULT_CHARSET, &lablstr);
    return lablstr;
}

void
XFE_SearchRuleView::editHeaders()
{
#undef DEFEAT_RACE_CONDITION        // BE API not ready yet
#ifdef DEFEAT_RACE_CONDITION
    // Defeat race condition between this dialog and the search view
    MSG_Master* master = fe_getMNMaster();
    if (!master->AcquireEditHeadersSemaphore(this))
    {
#ifdef DEBUG_akkana
        printf("SearchRuleView: Couldn't get semaphore!  Punting ...\n");
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
        printf("SearchRuleView: Couldn't release semaphore!\n");
#endif
        return;
    }
#endif /* DEFEAT_RACE_CONDITION */

    //
    // Set our option menu to something reasonable,
    // since it can't stay on "Customize..."
    // Future work: notify our parent so it updates the menus of
    // all the other SearchRuleViews as well (like the Filter UI does).

    resetHeaderOptions(newhdr);
    if (newhdr) free(newhdr);
}

void
XFE_SearchRuleView::resetHeaderOptions(char* newhdr)
{
    if ( m_curScope == scopeLdapDirectory )
        return;

    // Get the list of attributes again from the backend:
    MSG_FolderInfo *folderInfos[1];
  	folderInfos[0]= (MSG_FolderInfo *)m_folderInfo;
    uint16 attrNum = 0;
    MSG_GetNumAttributesForSearchScopes(fe_getMNMaster(),
                                        m_curScope,
                                        (void**)folderInfos, 1,
                                        &attrNum); 
    MSG_SearchMenuItem* attrItems
        = (MSG_SearchMenuItem *)XP_CALLOC(attrNum,
                                          sizeof(MSG_SearchMenuItem));

  	int err = MSG_GetAttributesForSearchScopes(fe_getMNMaster(), m_curScope, 
                                               (void**)folderInfos, 1,
                                               attrItems, &attrNum);
    if (err != SearchError_Success)
        return;

    buildPulldownMenu(m_attr_pop,
                      attrItems, attrNum,
                      attribOptionCallback, 
                      (XtPointer)this);

    // Get the new menu items:
    int numChildren;
    WidgetList childrenList;
    XtVaGetValues(m_attr_pop,
                  XmNchildren, &childrenList,
                  XmNnumChildren, &numChildren,
                  0);

    if (newhdr == 0)
    {
        XtVaSetValues(m_attr_opt,
                      XmNmenuHistory, childrenList[0],
                      0);
        attribOptionCallback(childrenList[0], (XtPointer)this, NULL);
        return;
    }

    // Loop over the option menu's options and make sure they match
    // the current set of arbitrary headers:
    char* thisbtn = 0;
    for (int num = 0; num < numChildren; ++num)
    {
        XmString xmstr;
        XtVaGetValues(childrenList[num], XmNlabelString, &xmstr, 0);
        XmStringGetLtoR(xmstr, XmFONTLIST_DEFAULT_TAG, &thisbtn);
        if (thisbtn && !XP_STRCMP(thisbtn, newhdr))
        {
            // Set the option menu to show that string
            XtVaSetValues(m_attr_opt,
                          XmNmenuHistory, childrenList[num],
                          0);
            attribOptionCallback(childrenList[num], (XtPointer)this, NULL);
            break;
        }
    }
    if (thisbtn) XtFree(thisbtn);
}

void
XFE_SearchRuleView::changeScope(MSG_FolderInfo *folderInfo)
{
  /* Find out what scope this folder is ++ */

  if ( m_curScope != scopeLdapDirectory)
  {
  	m_folderInfo = folderInfo;

  	MSG_FolderLine folderLine;
  	MSG_GetFolderLineById(XFE_MNView::getMaster(), folderInfo, &folderLine);

  	if (folderLine.flags &
          (MSG_FOLDER_FLAG_NEWSGROUP | MSG_FOLDER_FLAG_NEWS_HOST))
  	{ /* News */
		m_curScope = scopeNewsgroup;
  	}
  	else m_curScope = scopeMailFolder;
  }

  /* Find out what scope this folder is  -- */

  /* destroyRules */
  Widget form = getBaseWidget();
  Widget parent = XtParent(form);
  int i, numChildren;
  WidgetList childrenList;
  Dimension leadingWidth;

  XtVaGetValues(form, XmNnumChildren, &numChildren,
		XmNchildren, &childrenList, 0);

  leadingWidth = getLeadingWidth();
  for ( i = 0; i < numChildren; i++ )
  {
	XtDestroyWidget(childrenList[i]);
  }
  createWidgets(parent);
  adjustLeadingWidth(leadingWidth);
  
}

XFE_CALLBACK_DEFN(XFE_SearchRuleView, scopeChanged)(XFE_NotificationCenter*,

                           void *, void *calldata)
{

   if ( m_curScope == scopeLdapDirectory )
   {
	m_dir = (DIR_Server *)calldata;
        changeScope(NULL);
   }
   else
   {
        MSG_FolderInfo* folder = (MSG_FolderInfo*)calldata;

        changeScope(folder);
   }
}

uint16
XFE_SearchRuleView::getAttributeIndex()
{
	return m_curAttribIndex;
}

Dimension 
XFE_SearchRuleView::getLeadingWidth()
{
	Dimension width;

	XtVaGetValues(m_label, XmNwidth, &width, NULL);
	return width;
}

void
XFE_SearchRuleView::adjustLeadingWidth(Dimension width)
{
   XtVaSetValues(m_attr_opt, XmNleftOffset, width, 0);

   XtVaSetValues(m_label,
			XmNwidth, width, XmNrecomputeSize, False, 0 );

}


Dimension
XFE_SearchRuleView::getHeight()
{
	return m_height;
}


void
XFE_SearchRuleView::typeActivateCallback(Widget /* w */,
                                   XtPointer clientData,
                                   XtPointer /* callData */)
{
  XFE_SearchRuleView *obj = (XFE_SearchRuleView*) clientData;
   
   obj->getToplevel()->notifyInterested(
	XFE_SearchRuleView::activateSearch, (void*) NULL);
}

void
XFE_SearchRuleView::editHdrCallback(Widget,
                                    XtPointer clientData,
                                    XtPointer /*calldata*/ )
{
    XFE_SearchRuleView* srv = (XFE_SearchRuleView *) clientData;
    srv->editHeaders();
}

