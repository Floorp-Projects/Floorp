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
   Button.h -- class definition for XFE_Button.
   Created: Chris McAfee <mcafee@netscape.com>, Wed Nov  6 11:11:42 PST 1996
 */



#ifndef _xfe_button_h
#define _xfe_button_h

#include "Component.h"
#include "IconGroup.h"
#include "Menu.h"
#include "Frame.h"

#define MAX_ICON_GROUPS 4

typedef struct XFE_DoCommandArgs
{
public:
  CommandType       cmd;
  void*             callData;
  XFE_CommandInfo*  info;

  XFE_DoCommandArgs(CommandType command, void* cd = NULL, 
                    XFE_CommandInfo* i = NULL)
    : cmd(command), callData(cd), info(i) {}

} XFE_doCommandArgs;

class XFE_Button : public XFE_Component
{
public:

    XFE_Button(XFE_Frame *			frame,
			   Widget				parent,
               const char *			name,
               IconGroup *			iconGroup,
               IconGroup *			iconGroup2 = NULL,
               IconGroup *			iconGroup3 = NULL,
               IconGroup *			iconGroup4 = NULL);

    XFE_Button(XFE_Frame *			frame,
               Widget				parent,
               const char *			name,
			   MenuSpec *			menu_spec,
               IconGroup *			iconGroup,
               IconGroup *			iconGroup2 = NULL,
               IconGroup *			iconGroup3 = NULL,
               IconGroup *			iconGroup4 = NULL);

    XFE_Button(XFE_Frame *			frame,
               Widget				parent,
               const char *			name,
			   dynamenuCreateProc	generateProc,
			   void *				generateArg,
               IconGroup *			iconGroup,
               IconGroup *			iconGroup2 = NULL,
               IconGroup *			iconGroup3 = NULL,
               IconGroup *			iconGroup4 = NULL);

    ~XFE_Button();

	const char *	getName()		{ return m_name; }
	CommandType		getCmd()		{ return m_cmd; }
	void *			getCallData()	{ return m_callData; }

	void			setCmd		(CommandType cmd) { m_cmd = cmd; }
	void			setCallData	(void * callData) { m_callData = callData; }

	static const char *doCommandCallback;
	
  void setLabel	(char *label);

  void setPixmap (IconGroup * icons);

  int getIconGroupIndex();
  
  // Give a complete list of replacements with each call
  void setIconGroups (IconGroup *iconGroup,
                      IconGroup *iconGroup2 = NULL,
                      IconGroup *iconGroup3 = NULL,
                      IconGroup *iconGroup4 = NULL);

  void useIconGroup (int index);

  void setMenuSpec(MenuSpec *menu_spec);

  void setPretendSensitive(Boolean sensitive);

  // tooltip
  void setToplevel(XFE_Component *top) { m_toplevel = top;}

  Boolean isPretendSensitive();

  void setPopupDelay(int delay);

	void				configureEnabled();

protected:

	// The button name
	const char * m_name;
	
	// For now allow up to four possible icons groups
	// where each icon group can have up to four states
	IconGroup  *m_icons[MAX_ICON_GROUPS];

	int m_currentIconGroup;

	// The command executed by this button
	CommandType		m_cmd;
	void *			m_callData;

	static void	activate_cb			(Widget,XtPointer,XtPointer);
	static void	sub_menu_cb			(Widget,XtPointer,XtPointer);
	Widget		createButton		(Widget,WidgetClass);
	
	static  void tip_cb(Widget, XtPointer, XtPointer cb_data);
	virtual void tipCB(Widget, XtPointer cb_data);
};

extern void fe_buttonSetPixmaps(Widget button,IconGroup * group);

#endif /* _xfe_button_h */
