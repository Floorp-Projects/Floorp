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
   SearchRuleView.h -- class definitions for search rule view.
   Created: Dora Hsu <dora@netscape.com>, 15-Dec-96.
*/



#ifndef _xfe_searchruleview_h
#define _xfe_searchruleview_h

#include "MNView.h"
#include "msg_srch.h"
#include "xp_time.h"
#include <Xm/Xm.h>

class XFE_SearchRuleView: public XFE_MNView
{

 public:
	XFE_SearchRuleView(XFE_Component *toplevel_component,
                Widget parent,
                XFE_View *parent_view,
		MWContext *context,
                MSG_Pane *p,
		char *title,
		void *folderInfo,
		MSG_ScopeAttribute curScope,
		uint16 curAttrib);

	virtual ~XFE_SearchRuleView();
  	void setLabelWidth(Dimension width);
	XP_Bool getSearchTerm( MSG_SearchAttribute *attr, 
		MSG_SearchOperator *op, MSG_SearchValue *value,
                           char* customHdr);
	void resetSearchValue();
	void changeScope(MSG_FolderInfo *folderInfo);
	void adjustLeadingWidth (Dimension width);

	Dimension getLeadingWidth();
	Dimension getHeight();
	uint16 getAttributeIndex();

    void changeLabel(char* newlabel);

        static const char *activateSearch;
    XFE_CALLBACK_DECL(afterRealizeWidget)
	
 protected:
  void buildRules(Widget parent,
	 	MSG_ScopeAttribute curScope,
  		uint16 newAttr,   
		MSG_SearchMenuItem attrItems[], uint16 attrNum,
		MSG_SearchMenuItem opItems[], uint16 opNum,
		MSG_SearchValueWidget valueType );
  char* getAttribLabel();   // used for custom headers -- free with XtFree()!

 private:

  Widget  makeOptionMenu(Widget parent, char* widgetName, Widget* popup);
  Boolean needNewMenu(Widget pulldownParent,
                        MSG_SearchMenuItem *opItems,
                        uint16 opNum );

  void	 initializeDataMember();
  void	 calculateSize();
  void 	 createWidgets(Widget parent);
  void   buildPulldownMenu(Widget pulldownParent,
                        MSG_SearchMenuItem items[], uint16 itemNum,
                        XtCallbackProc activeCB, XtPointer clientData);
  void   resetHeaderOptions(char*);

  Widget   replaceValueField ( 
		Widget valueField, 
		Widget parent,
                MSG_SearchValueWidget valueType,
                XtCallbackProc callback, XtPointer clientData);

  void   setAttribOption(int16 attrib);
  void   createValueWidgetSet(Widget parent);
  void   editHeaders();
  static void 	attribOptionCallback(Widget w, 
			XtPointer clientData, XtPointer callData);
  static void   operatorOptionCallback(Widget w, 
			XtPointer clientData, XtPointer callData);
  static void 	valueOptionCallback(Widget w, 
			XtPointer clientData, XtPointer callData);
  static void   typeActivateCallback(Widget, XtPointer, XtPointer);
  static void editHdrCallback(Widget, XtPointer, XtPointer);

  //--------- Private Data member ----------------
  
  Visual*  m_visual;
  Colormap m_cmap;
  Cardinal m_depth;
  char*	   m_ruleTitle;
  
  Widget m_label;	     	/* label */
  Widget m_attr_opt;   		/* attribute option */
  Widget m_attr_pop;   		/* attribute option */
  Widget m_op_opt;   		/* operator option */
  Widget m_op_pop;   		/* operator option */
  Widget m_value;      		/* value option */

  Dimension m_width;   		/* width  of the row */
  Dimension m_height;  		/* height of the row */

  MSG_ScopeAttribute m_curScope;
  MSG_SearchAttribute m_attr;
  MSG_SearchOperator  m_op;
  MSG_SearchValueWidget m_type;
  int32 m_value_option;  /* for the MSG value */
  uint16 m_curAttribIndex;
  char *m_textValue;
  Widget m_opt_valueField;
  Widget m_date_valueField;
  Widget m_text_valueField;
  Widget m_date_textField;
  MSG_FolderInfo *m_folderInfo;
  DIR_Server *m_dir; /* Index of the current selected ldap dir in the option list */
  XFE_CALLBACK_DECL(scopeChanged)
};

#endif
