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
   Command.h -- command definitions
   Created: Chris Toshok <toshok@netscape.com>, 13-Aug-96.
 */



#ifndef _xfe_command_h
#define _xfe_command_h

#include <X11/Intrinsic.h> /* for the definition of XrmQuark. */

#undef Bool
#include <xp_core.h>

#include "commands.h"

/* use this to pass the (useful) event info down to doCommand() callees */
typedef enum 
{
	XFE_COMMAND_EVENT_NO_IDEA = 0,
	XFE_COMMAND_EVENT_ACTION,
	XFE_COMMAND_BUTTON_ACTIVATE,
	XFE_COMMAND_REMOTE_ACTION
} XFE_CommandEventType;

typedef struct XFE_CommandInfo
{
public:
	XFE_CommandEventType type;
	Widget    widget;
	XEvent*   event;
	String*   params;
	Cardinal* nparams;
	Cardinal  nparams_store;
	XEvent    event_store;

	XFE_CommandInfo(XFE_CommandEventType t, Widget w, XEvent* e = NULL,
					String* p = NULL, Cardinal n = 0);
} XFE_CommandInfo;

typedef char* CommandType;

class Command {
public:
	static CommandType intern(char* foo);
	static CommandType convertOldRemote(char* foo);

	static char* getString(CommandType command) 
	{
		return (char*)command;
	}

  static const char *commandArmedCallback;
  static const char *commandDispatchedCallback;
  static const char *commandDisarmedCallback;
  static const char *doCommandCallback;
};

struct XFE_CommandParameters
{
	char* name;
	void* data;
};

class XFE_View;
class XFE_Frame;

class XFE_AbstractCommand
{
public:
	virtual CommandType getId() = 0;
	virtual char*       getName() = 0;

	virtual XP_Bool     isDynamic() = 0;
	virtual XP_Bool     isSlow() = 0;

	//    Call these from a View.
	virtual XP_Bool     isEnabled(XFE_View* view, XFE_CommandInfo*) = 0;
	virtual XP_Bool     isSelected(XFE_View*, XFE_CommandInfo*) = 0;
	virtual XP_Bool     isDeterminate(XFE_View*, XFE_CommandInfo*) = 0;

	virtual XFE_CommandParameters* getParameters(XFE_View*) = 0;
	virtual int         getParameterIndex(XFE_View*) = 0;
	virtual void        setParameterIndex(XFE_View*, unsigned) = 0;
	virtual char*       getLabel(XFE_View*, XFE_CommandInfo*) = 0;
	virtual char*       getTipString(XFE_View*, XFE_CommandInfo*) = 0;
	virtual char*       getDocString(XFE_View*, XFE_CommandInfo*) = 0;

	virtual void        doCommand(XFE_View* view, XFE_CommandInfo*) = 0;

	//    Call these from a Frame.
	virtual XP_Bool     isEnabled(XFE_Frame*, XFE_CommandInfo*) = 0;
	virtual XP_Bool     isSelected(XFE_Frame*, XFE_CommandInfo*) = 0;
	virtual XP_Bool     isDeterminate(XFE_Frame*, XFE_CommandInfo*) = 0;

	virtual XFE_CommandParameters* getParameters(XFE_Frame*) = 0;
	virtual int         getParameterIndex(XFE_Frame*) = 0;
	virtual void        setParameterIndex(XFE_Frame*, unsigned) = 0;
	virtual char*       getLabel(XFE_Frame*, XFE_CommandInfo*) = 0;
	virtual char*       getTipString(XFE_Frame*, XFE_CommandInfo*) = 0;
	virtual char*       getDocString(XFE_Frame*, XFE_CommandInfo*) = 0;

	virtual void        doCommand(XFE_Frame* view, XFE_CommandInfo*) = 0;

};

class XFE_Command : public XFE_AbstractCommand
{
public:
	CommandType getId() { return m_id; };
	char*       getName();

	XP_Bool     isDynamic();
	XP_Bool     isSlow();

	//    Call these from a View.
	XP_Bool     isEnabled(XFE_View* view, XFE_CommandInfo*);
	XP_Bool     isSelected(XFE_View*, XFE_CommandInfo*);
	XP_Bool     isDeterminate(XFE_View*, XFE_CommandInfo*);

	XFE_CommandParameters* getParameters(XFE_View*);
	int         getParameterIndex(XFE_View*);
	void        setParameterIndex(XFE_View*, unsigned);
	char*       getLabel(XFE_View*, XFE_CommandInfo*);
	char*       getTipString(XFE_View*, XFE_CommandInfo*);
	char*       getDocString(XFE_View*, XFE_CommandInfo*);

	//    Call these from a Frame.
	XP_Bool     isEnabled(XFE_Frame*, XFE_CommandInfo*);
	XP_Bool     isSelected(XFE_Frame*, XFE_CommandInfo*);
	XP_Bool     isDeterminate(XFE_Frame*, XFE_CommandInfo*);

	XFE_CommandParameters* getParameters(XFE_Frame*);
	int         getParameterIndex(XFE_Frame*);
	void        setParameterIndex(XFE_Frame*, unsigned);
	char*       getLabel(XFE_Frame*, XFE_CommandInfo*);
	char*       getTipString(XFE_Frame*, XFE_CommandInfo*);
	char*       getDocString(XFE_Frame*, XFE_CommandInfo*);

protected:	
	XFE_Command(CommandType id);

private:
	CommandType m_id;
};

class XFE_ViewCommand : public XFE_Command
{
public:
	//    These will forward to the view.
	XP_Bool     isEnabled(XFE_Frame* view, XFE_CommandInfo*);
	XP_Bool     isSelected(XFE_Frame*, XFE_CommandInfo*);
	XP_Bool     isDeterminate(XFE_Frame*, XFE_CommandInfo*);

	XFE_CommandParameters* getParameters(XFE_Frame*);
	int         getParameterIndex(XFE_Frame*);
	void        setParameterIndex(XFE_Frame*, unsigned);
	char*       getLabel(XFE_Frame*, XFE_CommandInfo*);
	char*       getTipString(XFE_Frame*, XFE_CommandInfo*);
	char*       getDocString(XFE_Frame*, XFE_CommandInfo*);

	void        doCommand(XFE_Frame* view, XFE_CommandInfo*);

protected:
	XFE_ViewCommand(CommandType id) : XFE_Command(id) { };

	//
	//    Handle utility for provide command hacker feedback, generates:
	//    "Syntax error in command: setFontFace(moby dick)"
	//
	void        doSyntaxErrorAlert(XFE_View*,  XFE_CommandInfo*);
};

class XFE_FrameCommand : public XFE_Command
{
public:
	//    These will forward to your Frame dual.
	XP_Bool     isEnabled(XFE_View* view, XFE_CommandInfo*);
	XP_Bool     isSelected(XFE_View*, XFE_CommandInfo*);
	XP_Bool     isDeterminate(XFE_View*, XFE_CommandInfo*);

	XFE_CommandParameters* getParameters(XFE_View*);
	int         getParameterIndex(XFE_View*);
	void        setParameterIndex(XFE_View*, unsigned);
	char*       getLabel(XFE_View*, XFE_CommandInfo*);
	char*       getTipString(XFE_View*, XFE_CommandInfo*);
	char*       getDocString(XFE_View*, XFE_CommandInfo*);

	void        doCommand(XFE_View* view, XFE_CommandInfo*);

protected:
	XFE_FrameCommand(CommandType id) : XFE_Command(id) { };

	XFE_Frame*  getFrame(XFE_View*);
	void        doSyntaxErrorAlert(XFE_Frame*,  XFE_CommandInfo*);
};

int
XFE_commandMatchParameters(XFE_CommandParameters*, char*);

class XFE_CommandList {
public:
	friend XFE_Command*     findCommand(XFE_CommandList*, CommandType);
	friend XFE_CommandList* registerCommand(XFE_CommandList*&, XFE_Command*);
private:
	XFE_CommandList(XFE_CommandList*, XFE_Command*);
	XFE_CommandList* m_next;
	XFE_Command*     m_command;
};

//
//    This class will help you impliment context sensitive commands.
//    You must impliment a getObjectType() method. This should return
//    a string token which is the type of context you are in now.
//    Once this is implimented, use your sub-class to give you context
//    sensitive translations, etc.. See uses of objectIs in the editor's
//    resources...djw
//
class XFE_ObjectIsCommand : public XFE_ViewCommand
{
public:
	XFE_ObjectIsCommand();

	        void  doCommand(XFE_View* view, XFE_CommandInfo* info);
	virtual char* getObjectType(XFE_View*) = 0;
};

#endif /* _xfe_command_h */

