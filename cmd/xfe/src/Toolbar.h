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
   Toolbar.h -- class definition for the toolbar object.
   Created: Chris Toshok <toshok@netscape.com>, 13-Aug-96.
 */



#ifndef _xfe_toolbar_h
#define _xfe_toolbar_h

#include "ToolboxItem.h"

//#include "xp_core.h"

#include "Command.h"
#include "icons.h"

class XFE_View;
class XFE_Frame;
class XFE_Button;
class XFE_Logo;
class XFE_Toolbox;

//
// The toolbar is used by many (almost all) of the Frame classes.
//
class XFE_Toolbar : public XFE_ToolboxItem
{
	XFE_Frame *getFrame();

	void addButton(const char *name, EChromeTag tag, CommandType command, fe_icon *icon);

	virtual Widget findButton(const char *name, EChromeTag tag);

	void updateButton(Widget button);

	void updateCommand(CommandType command);

    void updateAppearance();

	XFE_CALLBACK_DECL(doCommand)

	// update all the commands in the toolbar.
	XFE_CALLBACK_DECL(update)
	// update a specific command in the toolbar.
	XFE_CALLBACK_DECL(updateCommand)
    // update the toolbar appearance
    XFE_CALLBACK_DECL(updateToolbarAppearance)

public:  

	XFE_Toolbar(XFE_Frame *			parent_frame,
				XFE_Toolbox *		parent_toolbox,
				ToolbarSpec *		spec);

	virtual ~XFE_Toolbar();
	void hideButton(const char *name, EChromeTag tag);
	void showButton(const char *name, EChromeTag tag);
	// method version of the callback below.  Used by the frame to force the toolbar
	// to update itself
	void update();
	Widget getToolBar();
	Widget getToolBarForm();

	virtual void setToolbarSpec(ToolbarSpec *menu_spec);

	// Convert from icon style (from prefs) to button layout enum
	static unsigned char styleToLayout(int32 style);

protected:

    XFE_Frame *		m_parentFrame;
	ToolbarSpec *	m_spec;
	Widget			m_toolBar;


    XFE_Button *m_security;

	void	createMain				(Widget parent);
	void	createItems				();

	Widget	createItem				(ToolbarSpec *	spec);
	Widget	createPush				(ToolbarSpec *	spec);
	Widget	createSeparator			(ToolbarSpec *	spec);
	Widget	createDynamicCascade	(ToolbarSpec *	spec);
	Widget	createCascade			(ToolbarSpec *	spec);

	int		verifyPopupDelay		(int delay);

	//////////////////////////////////////////////////////////////////
	//																//
	// User commands support										//
	//																//
	//////////////////////////////////////////////////////////////////
	static XtResource	m_user_commandResources[];
	String				m_user_commandName;
	String				m_user_commandData;
	String				m_user_commandIcon;
	String				m_user_commandSubMenu;

	void		user_createItems			();
	
	CommandType	user_verifyCommand	(String					commandName,
									 dynamenuCreateProc &	generateProcOut,
									 void * &				generateArgOut);

	void *		user_verifyCommandData	(CommandType		cmd,
										 String				commandData,
										 XP_Bool &			needToFreeOut);
	
	IconGroup *	user_verifyCommandIcon	(CommandType		cmd,
										 String				commandIcon);
	
	MenuSpec *	user_verifySubMenu		(String				commandSubMenu);
	
	void		user_addItem			(String				widgetName,
										 CommandType		cmd,
										 EChromeTag			tag,
										 void *				data,
										 XP_Bool			needToFreeData,
										 IconGroup *		ig,
										 MenuSpec *			subMenuSpec,
										 XP_Bool			needToFreeSpec,
										 dynamenuCreateProc	generateProc,
										 void *				generateArg,
										 EToolbarPopupDelay	popupDelay);
	
	static void user_commandDestroyCB(Widget,XtPointer,XtPointer);
	static void user_freeCallDataCB(Widget,XtPointer,XtPointer);
};

#endif /* _xfe_toolbar_h */
