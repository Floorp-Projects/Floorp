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
   Dashboard.h -- The chrome along the bottom of a frame.
   Created: Chris Toshok <toshok@netscape.com>, 11-Oct-1996
 */



#ifndef _xfe_dashboard_h
#define _xfe_dashboard_h

#include "xp_core.h"
#include "Frame.h"
#include "Component.h"

#ifdef MOZ_TASKBAR
class XFE_TaskBar;
class XFE_PopupMenu;
#endif


class XFE_Dashboard : public XFE_Component 
{
public:
	
	XFE_Dashboard(XFE_Component *	toplevel_component,
				  Widget			parent,
                  XFE_Frame *       frame,
				  XP_Bool			have_taskbar = True);
	
	virtual ~XFE_Dashboard();

#if defined(GLUE_COMPO_CONTEXT)
	// connect the compo to the dashboard
	void connect2Dashboard(XFE_Component *compo);
	void disconnectFromDashboard(XFE_Component *compo);
#endif /* GLUE_COMPO_CONTEXT */		

	// Status methods
	void setStatusText			(const char * text);

	// Progress methods
	void setProgressBarText		(const char * text);
	void setProgressBarPercent	(int percent);

	void startCylon			();
	void stopCylon			();
	void tickCylon			();
	
#ifdef MOZ_TASKBAR
	// Task Bar dockingness functions
	static void		dockTaskBar				();
	static void		toggleTaskBar			();
	static void		unDockTaskBar			();
	static XP_Bool	isTaskBarDocked			();
	static XP_Bool	floatingTaskBarIsAlive	();
	static void		startFloatingTaskBar	(XFE_Frame * parentFrame);
	static void		stopFloatingTaskBar		();
	static void		showFloatingTaskBar		(XFE_Frame * parentFrame);

	// Access floating taskbar
	static XFE_TaskBar *		getFloatingTaskBar	();

	// Access taskbar
	XFE_TaskBar *				getTaskBar			();

	// Update state and prefs
	static void		setFloatingTaskBarPosition		(int32 x,int32 y);
	static void		setFloatingTaskBarHorizontal	(XP_Bool horizpntal);
	static void		setFloatingTaskBarOnTop			(XP_Bool ontop);

	// Invoked when the taskbar is docked
	static const char *taskBarDockedCallback;

	// Invoked when the taskbar is undocked
	static const char *taskBarUndockedCallback;
#endif

	// Security icon methods
	void		setShowSecurityIcon		(XP_Bool state);
	void		setShowSignedIcon		(XP_Bool state);
	void		setShowStatusBar		(XP_Bool state);
	void		setShowProgressBar		(XP_Bool state);
	XP_Bool		isSecurityIconShown		();
	XP_Bool		isSignedIconShown		();

	XP_Bool		processTraversal		(XmTraversalDirection direction);

private:

    XFE_Frame   *			m_parentFrame;

	Widget					m_statusBar;
	Widget					m_progressBar;
	Widget					m_securityBar;

    XFE_Button *            m_securityIcon;
	XFE_Button *            m_signedIcon;

	XP_Bool					m_securityRegistered;


#ifdef MOZ_TASKBAR

	static XFE_PopupMenu *	m_floatingPopup;
	static MenuSpec			m_floatingPopupMenuSpec[];

	XFE_TaskBar *			m_dockedTaskBar;
	
	static Widget			m_floatingShell;
	static XFE_TaskBar *	m_floatingTaskBar;
	static XP_Bool			m_floatingDocked;

	static time_t			m_floatingLastRaisedTime;
	static int				m_floatingTimesRaisedPerSecond;

	// Callbacks and event handlers

	static void floatingButtonEH		(Widget,XtPointer,XEvent *,Boolean *);
	static void floatingConfigureEH		(Widget,XtPointer,XEvent *,Boolean *);
	static void floatingMappingCB		(Widget,XtPointer,XtPointer);
	static void floatingActionCB		(Widget,XtPointer,XtPointer);

	void			createDockedTaskBar	();
#endif

	void			addSecurityIcon		();
	void			addSignedIcon		();
	void			configureSecurityBar();

	void			createProgressBar	();
	void			createStatusBar		();
	void			createSecurityBar	();

	// callback handler for the commandArmedCallback in XFE_MenuBar.
	// -- command is in calldata
	XFE_CALLBACK_DECL(showCommandStringNotice)
		
	// callback handler for the commandDisarmedCallback in XFE_MenuBar.
	// -- command is in calldata
	XFE_CALLBACK_DECL(eraseCommandStringNotice)
		
	// update the status bar in this frame.
	XFE_CALLBACK_DECL(setStatusTextNotice)

	// Start the cylon
	XFE_CALLBACK_DECL(startCylonNotice)

	// Stop the cylon
	XFE_CALLBACK_DECL(stopCylonNotice)

	// Tick the cylon
	XFE_CALLBACK_DECL(tickCylonNotice)

	// Progress bar notices
	XFE_CALLBACK_DECL(progressBarUpdatePercentNotice)
	XFE_CALLBACK_DECL(progressBarUpdateTextNotice)

	XFE_CALLBACK_DECL(doSecurityCommand)

	// update a specific command in the toolbar.
	XFE_CALLBACK_DECL(update)

#ifdef MOZ_TASKBAR
	static void		raiseFloatingShell	();
#endif
};
#endif /* _xfe_dashboard_h */
