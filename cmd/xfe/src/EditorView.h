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
   EditorView.h -- class definition for the Editor view class
   Created: Richard Hess <rhess@netscape.com>, 11-Nov-96.
 */



#ifndef _xfe_editorview_h
#define _xfe_editorview_h

#include "structs.h"
#include "mozilla.h"
#include "xfe.h"

#include "HTMLView.h"
#include "Editable.h"
#include "Command.h"
#include "EditorDrop.h"

#define RGB_TO_WORD(r,g,b) (((r) << 16) + ((g) << 8) + (b))
#define WORD_TO_R(w)       (((w) >> 16) & 0xff)
#define WORD_TO_G(w)       (((w) >> 8) & 0xff)
#define WORD_TO_B(w)       ((w) & 0xff)

class XFE_EditorView : public XFE_HTMLView, public XFE_Editable
{
public:
  XFE_EditorView(XFE_Component *toplevel_component, Widget parent, XFE_View *parent_view, MWContext *context);
  virtual ~XFE_EditorView();
#if 0
  virtual Boolean isCommandEnabled(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual void    doCommand(CommandType, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual Boolean handlesCommand(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual char*   commandToString(CommandType cmd, void *calldata = NULL,
								  XFE_CommandInfo* i = NULL);
  virtual Boolean isCommandSelected(CommandType cmd, void* calldata,
								   XFE_CommandInfo* i = NULL);
#endif
  virtual XFE_Command* getCommand(CommandType);
  virtual XFE_View*    getCommandView(XFE_Command*);

  virtual char *getPlainText();
  virtual void insertMessageCompositionText(const char* text, 
					    XP_Bool leaveCursorBeginning, XP_Bool isHTML = False);
  virtual void getMessageBody(char **pBody, uint32 *body_size, 
			      MSG_FontCode **font_changes);
  virtual void doneWithMessageBody(char* pBody);

  virtual Boolean isModified();

  void    updateChrome();

private:
  XFE_EditorDrop *_dropSite;
  XtIntervalId m_update_timer;
  XFE_CommandList* m_commands;
    
  virtual void DocEncoding(XFE_NotificationCenter *, void *, void *);
  static  void updateChromeTimeout(XtPointer closure, XtIntervalId* id);
};

// Command classes which need to be public
// (needed by BrowserFrame for embedded editor)

//    This acts as an encapsulator for the doCommand() method.
//    Sub-classes impliment a reallyDoCommand(), and leave the
//    boring maintainence work to this class. This approach
//    saves every sub-class from calling super::doCommand(),
//    which would really be a drag, now wouldn't it.
//
class XFE_EditorViewCommand : public XFE_ViewCommand
{
public:
	XFE_EditorViewCommand(char* name, XFE_EditorView *v = NULL)
      : XFE_ViewCommand(name, v) {};
	
	virtual void    reallyDoCommand(XFE_View*, XFE_CommandInfo*) = 0;
	virtual XP_Bool requiresChromeUpdate() {
		return TRUE;
	};
	void            doCommand(XFE_View* v_view, XFE_CommandInfo* info) {
		XFE_EditorView* view = (XFE_EditorView*)v_view;

		if (m_view)
			view = (XFE_EditorView *)m_view;

		reallyDoCommand(view, info);
		if (requiresChromeUpdate()) {
			view->updateChrome();
		}
	}; 
};

class ReallyAlwaysEnabledCommand : public XFE_EditorViewCommand
{
public:
	ReallyAlwaysEnabledCommand(char* name, XFE_EditorView *v)
      : XFE_EditorViewCommand(name, v) {};

	XP_Bool isEnabled(XFE_View*, XFE_CommandInfo*) {
		return True;
	};
};

// AlwaysEnabledCommand isn't really always enabled;
// it's turned off if we're blocked, e.g. if a plugin is running.
// If you really want something on all the time,
// use ReallyAlwaysEnabledCommand.
class AlwaysEnabledCommand : public ReallyAlwaysEnabledCommand
{
public:
	AlwaysEnabledCommand(char* name, XFE_EditorView *v)
      : ReallyAlwaysEnabledCommand(name, v) {};

	XP_Bool isEnabled(XFE_View* v, XFE_CommandInfo*);
};

class SetFontColorCommand : public AlwaysEnabledCommand
{
public:
	SetFontColorCommand(XFE_EditorView *v)
      : AlwaysEnabledCommand(xfeCmdSetFontColor, v) {
		m_params = NULL;
	};

	virtual XP_Bool isDynamic() { return TRUE; };
	XP_Bool isDeterminate(XFE_View* view, XFE_CommandInfo*);
	XFE_CommandParameters* getParameters(XFE_View* view);
	int getParameterIndex(XFE_View* view);
	void setParameterIndex(XFE_View* view, unsigned index);
	void reallyDoCommand(XFE_View* view, XFE_CommandInfo* info);
private:
	XFE_CommandParameters* m_params;
};

#endif /* _xfe_editorview_h */

