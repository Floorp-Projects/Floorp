/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/* 
   TextEditorView.h - anything that has a plain text editable area in it. 
   Created: Dora Hsu <dora@netscape.com>, Sept-30-96.
 */



#ifndef _xfe_texteditorview_h
#define _xfe_texteditorview_h

#include "MNView.h"
#include "Editable.h"
#include "PopupMenu.h"
extern "C" {
#include "msgcom.h"
}

class XFE_TextEditorView: public XFE_MNView, public XFE_Editable
{
public:
  XFE_TextEditorView(XFE_Component *toplevel_component, 
			XFE_View *parent_view, MSG_Pane *pane,
		     MWContext *context);

  virtual Boolean isCommandEnabled(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual Boolean handlesCommand(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual void doCommand(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual void 	createWidgets(Widget parent, XP_Bool wrap_p);

  // virtual methods on Editable....
  virtual char *getPlainText();
  virtual void insertMessageCompositionText(
                const char* text, XP_Bool leaveCursorBeginning, 
				XP_Bool isHTML = False );

  virtual void getMessageBody(
		char **pBody, uint32 *body_size, 
		MSG_FontCode **font_changes);

  virtual void doneWithMessageBody(char* pBody);

  virtual Boolean isModified();

  void setComposeWrapState(XP_Bool wrap_p);
  void setFont(XmFontList fontList);

  void textFocus();

  static const char *textFocusIn;

protected:
private:
  static MenuSpec popupmenu_spec[];
  XFE_PopupMenu *m_popup;

  static void textFocusCallback(Widget, XtPointer clientData, XtPointer);
};

#endif //_xfe_texteditorview_h
