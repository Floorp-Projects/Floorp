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
   Dashboard.cpp -- The chrome along the bottom of a frame.
   Created: Chris Toshok <toshok@netscape.com>, 11-Oct-1996
 */


#include "rosetta.h"
#include "Dashboard.h"
#include "Button.h"
#include "View.h"
#include "DisplayFactory.h"
#include "MozillaApp.h"
#include "prefapi.h"
#include "PopupMenu.h"
#include "BookmarkFrame.h"

#ifdef MOZ_TASKBAR
#include "TaskBar.h"
#endif

#include <Xm/Form.h>
#include <Xm/MwmUtil.h>

#include <Xfe/XfeAll.h>

#if 0
#define D(x) x
#else
#define D(x)
#endif

// Widget names
#define DASH_BOARD_NAME				"dashBoard"
#define ICON_TOOL_BAR_NAME			"securityBar"
#define PROGRESS_BAR_NAME			"progressBar"
#define STATUS_BAR_NAME				"statusBar"

#define MIN_DASH_HEIGHT					22

#ifdef MOZ_TASKBAR

#define FLOATING_SHELL_NAME				"mozillaComponentBar"
#define FLOATING_ORIGIN_OFFSET			50
#define MAX_FLOATING_RAISES_PER_SECOND	4

/* static */ MenuSpec 
XFE_Dashboard::m_floatingPopupMenuSpec[] = 
{
	{ xfeCmdFloatingTaskBarAlwaysOnTop,	TOGGLEBUTTON },
	{ xfeCmdFloatingTaskBarHorizontal,	PUSHBUTTON },
	{ xfeCmdFloatingTaskBarClose,		PUSHBUTTON },
	{ NULL }
};

//
// Task Bar members
//

/* static */ XFE_PopupMenu *
XFE_Dashboard::m_floatingPopup = NULL;

/* static */ XFE_TaskBar *
XFE_Dashboard::m_floatingTaskBar = NULL;

/* static */ Widget
XFE_Dashboard::m_floatingShell = NULL;

/* static */ XP_Bool
XFE_Dashboard::m_floatingDocked = False;

/* static */ time_t
XFE_Dashboard::m_floatingLastRaisedTime = 0;

/* static */ int
XFE_Dashboard::m_floatingTimesRaisedPerSecond = 0;

// Needed to determine the colormap used by the floating shell colormap
extern "C" Colormap fe_getColormap(fe_colormap *colormap);

//////////////////////////////////////////////////////////////////////////
//
// XFE_Dashboard notification callbacks names
//
//////////////////////////////////////////////////////////////////////////

/* static */ const char * 
XFE_Dashboard::taskBarDockedCallback = "XFE_Dashboard::taskBarDockedCallback";

/* static */ const char * 
XFE_Dashboard::taskBarUndockedCallback = "XFE_Dashboard::taskBarUndockedCallback";

#endif

// Needed to truncate strings 
extern "C" XmString fe_StringChopCreate(char* message, char* tag,
										XmFontList font_list,int maxwidth);


//////////////////////////////////////////////////////////////////////////
XFE_Dashboard::XFE_Dashboard(XFE_Component *	toplevel_component,
							 Widget				parent,
                             XFE_Frame *        frame,
                             XP_Bool			have_task_bar)
	: XFE_Component(toplevel_component)
{
    m_parentFrame   = frame;
	m_statusBar     = NULL;
	m_progressBar	= NULL;
	m_securityIcon	= NULL;
	m_signedIcon	= NULL;
	m_securityBar	= NULL;

#ifdef MOZ_TASKBAR
	m_dockedTaskBar	= NULL;
#endif

#ifndef MOZ_TASKBAR
	have_task_bar = False;
#endif

	m_securityRegistered	= False;

	m_widget = XtVaCreateWidget(DASH_BOARD_NAME,
								xfeDashBoardWidgetClass,
								parent,
								XmNusePreferredWidth,	False,
								XmNusePreferredHeight,	True,
								XmNshowDockedTaskBar,	have_task_bar,
								XmNminHeight,			MIN_DASH_HEIGHT,
								NULL);

#ifdef MOZ_TASKBAR
	// Add floating shell map/unmap callbacks to dashboard
	XtAddCallback(m_widget,
				  XmNfloatingMapCallback,
				  XFE_Dashboard::floatingMappingCB,
				  (XtPointer) this);

	XtAddCallback(m_widget,
				  XmNfloatingUnmapCallback,
				  XFE_Dashboard::floatingMappingCB,
				  (XtPointer) this);
#endif

#ifdef MOZ_TASKBAR
	// Start the floating shell only if a parent frame is given
	if (m_parentFrame)
	{
		// The task bar parent frame will always be the bookmark frame since
		// it is the only frame that sticks around for the duration of the 
		// application.
		XFE_Frame * taskBarParentFrame = NULL;

		// If our parent frame is the bookmark frame, use that.
		if (m_parentFrame->getType() == FRAME_BOOKMARK)
		{
			taskBarParentFrame = m_parentFrame;
		}
		// Otherwise get the bookmark frame
		else
		{
			taskBarParentFrame = XFE_BookmarkFrame::getBookmarkFrame();
		}

		// Make sure the floating task bar is started
		XFE_Dashboard::startFloatingTaskBar(taskBarParentFrame);
	}

	// Create the docked task bar if needed
	if (have_task_bar)
	{
		createDockedTaskBar();
	}
#endif
  
	installDestroyHandler();
}
//////////////////////////////////////////////////////////////////////////
XFE_Dashboard::~XFE_Dashboard()
{
	D( printf ("In XFE_Dashboard::~XFE_Dashboard\n");)
	
	// Unregister status notifications with top level component
	if (m_statusBar)
	{
		m_toplevel->unregisterInterest(
			Command::commandArmedCallback,
			this,
			(XFE_FunctionNotification)showCommandStringNotice_cb);
		
		m_toplevel->unregisterInterest(
			Command::commandDisarmedCallback,
			this,
			(XFE_FunctionNotification)eraseCommandStringNotice_cb);
		
		m_toplevel->unregisterInterest(
			XFE_View::statusNeedsUpdatingMidTruncated,
			this,
			(XFE_FunctionNotification)setStatusTextNotice_cb);
		
		m_toplevel->unregisterInterest(
			XFE_View::statusNeedsUpdating,
			this,
			(XFE_FunctionNotification)setStatusTextNotice_cb);
	}

	// Unregister progress notifications with parent
	if (m_parentFrame && m_progressBar)
	{
#if defined(GLUE_COMPO_CONTEXT)
		m_parentFrame->unregisterInterest(
			XFE_Component::progressBarCylonStart,
			this,
			startCylonNotice_cb);
		
		m_parentFrame->unregisterInterest(
			XFE_Component::progressBarCylonStop,
			this,
			stopCylonNotice_cb);
		
		m_parentFrame->unregisterInterest(
			XFE_Component::progressBarCylonTick,
			this,
			tickCylonNotice_cb);
		
		m_parentFrame->unregisterInterest(
			XFE_Component::progressBarUpdatePercent,
			this,
			progressBarUpdatePercentNotice_cb);

		m_parentFrame->unregisterInterest(
			XFE_Component::progressBarUpdateText,
			this,
			progressBarUpdateTextNotice_cb);
#else
		m_parentFrame->unregisterInterest(
			XFE_Frame::progressBarCylonStart,
			this,
			startCylonNotice_cb);
		
		m_parentFrame->unregisterInterest(
			XFE_Frame::progressBarCylonStop,
			this,
			stopCylonNotice_cb);
		
		m_parentFrame->unregisterInterest(
			XFE_Frame::progressBarCylonTick,
			this,
			tickCylonNotice_cb);
		
		m_parentFrame->unregisterInterest(
			XFE_Frame::progressBarUpdatePercent,
			this,
			progressBarUpdatePercentNotice_cb);

		m_parentFrame->unregisterInterest(
			XFE_Frame::progressBarUpdateText,
			this,
			progressBarUpdateTextNotice_cb);
#endif /* GLUE_COMPO_CONTEXT */
	}

	// Unregister parent frame update chrome notifications if needed
	if (m_parentFrame && m_securityRegistered)
	{
		m_parentFrame->unregisterInterest(
			XFE_View::chromeNeedsUpdating,
			this,
			(XFE_FunctionNotification)update_cb);
	}

	D(	printf ("Leaving XFE_Dashboard::~XFE_Dashboard\n");)
}

#if defined(GLUE_COMPO_CONTEXT)
void XFE_Dashboard::connect2Dashboard(XFE_Component *compo)
{
	if (compo) {
		// register dashboard events
		compo->registerInterest(XFE_View::statusNeedsUpdating,
								this,
								(XFE_FunctionNotification)setStatusTextNotice_cb);
		
		// Register progress notifications with parent
		compo->registerInterest(XFE_Component::progressBarCylonStart,
								this,
								startCylonNotice_cb);
			
		compo->registerInterest(XFE_Component::progressBarCylonStop,
								this,
								stopCylonNotice_cb);
			
		compo->registerInterest(XFE_Component::progressBarCylonTick,
								this,
								tickCylonNotice_cb);
		
		compo->registerInterest(XFE_Component::progressBarUpdatePercent,
								this,
								progressBarUpdatePercentNotice_cb);
		
		compo->registerInterest(XFE_Component::progressBarUpdateText,
								this,
								progressBarUpdateTextNotice_cb);
	}/* compo */
}

void XFE_Dashboard::disconnectFromDashboard(XFE_Component *compo)
{
	if (compo) {
		// register dashboard events
		compo->unregisterInterest(XFE_View::statusNeedsUpdating,
								  this,
								  (XFE_FunctionNotification)setStatusTextNotice_cb);
		
		// Register progress notifications with parent
		compo->unregisterInterest(XFE_Component::progressBarCylonStart,
								  this,
								  startCylonNotice_cb);
		
		compo->unregisterInterest(XFE_Component::progressBarCylonStop,
								  this,
								  stopCylonNotice_cb);
		
		compo->unregisterInterest(XFE_Component::progressBarCylonTick,
								  this,
								  tickCylonNotice_cb);
		
		compo->unregisterInterest(XFE_Component::progressBarUpdatePercent,
								  this,
								  progressBarUpdatePercentNotice_cb);
			
		compo->unregisterInterest(XFE_Component::progressBarUpdateText,
								  this,
								  progressBarUpdateTextNotice_cb);
	}/* compo */
}
#endif /* GLUE_COMPO_CONTEXT */		

//////////////////////////////////////////////////////////////////////////
void
XFE_Dashboard::createStatusBar()
{
	XP_ASSERT( XfeIsAlive(m_widget) );

	m_statusBar = 
		XtVaCreateWidget(STATUS_BAR_NAME,
						 xfeLabelWidgetClass,
						 m_widget,

						 XmNusePreferredHeight,	True,
						 XmNusePreferredWidth,	False,

						 XmNtruncateProc,		fe_StringChopCreate,

						 XmNtraversalOn,		True,
						 XmNhighlightThickness,	0,
						 NULL);

	// Clear the status bar to begin with
	setStatusText("");

	// Register status notifications with top level component
	m_toplevel->registerInterest(
		Command::commandArmedCallback,
		this,
		(XFE_FunctionNotification)showCommandStringNotice_cb);
	
	m_toplevel->registerInterest(
		Command::commandDisarmedCallback,
		this,
		(XFE_FunctionNotification)eraseCommandStringNotice_cb);
	
	m_toplevel->registerInterest(
		XFE_View::statusNeedsUpdatingMidTruncated,
		this,
		(XFE_FunctionNotification)setStatusTextNotice_cb);
	
	m_toplevel->registerInterest(
		XFE_View::statusNeedsUpdating,
		this,
		(XFE_FunctionNotification)setStatusTextNotice_cb);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Dashboard::createProgressBar()
{
	XP_ASSERT( XfeIsAlive(m_widget) );
	XP_ASSERT( m_progressBar == NULL );

	m_progressBar = 
		XtVaCreateManagedWidget(PROGRESS_BAR_NAME,
								xfeProgressBarWidgetClass,
								m_widget,

								XmNusePreferredHeight,		True,
								XmNusePreferredWidth,		False,

								XmNtraversalOn,				True,
								XmNhighlightThickness,		0,
								NULL);

	// Clear the progress bar to begin with
	setProgressBarText("");
	setProgressBarPercent(0);

	// Register progress notifications with parent
	if (m_parentFrame)
	{
#if defined(GLUE_COMPO_CONTEXT)
		m_parentFrame->registerInterest(
			XFE_Component::progressBarCylonStart,
			this,
			startCylonNotice_cb);
		
		m_parentFrame->registerInterest(
			XFE_Component::progressBarCylonStop,
			this,
			stopCylonNotice_cb);
		
		m_parentFrame->registerInterest(
			XFE_Component::progressBarCylonTick,
			this,
			tickCylonNotice_cb);
		
		m_parentFrame->registerInterest(
			XFE_Component::progressBarUpdatePercent,
			this,
			progressBarUpdatePercentNotice_cb);

		m_parentFrame->registerInterest(
			XFE_Component::progressBarUpdateText,
			this,
			progressBarUpdateTextNotice_cb);
#else
		m_parentFrame->registerInterest(
			XFE_Frame::progressBarCylonStart,
			this,
			startCylonNotice_cb);
		
		m_parentFrame->registerInterest(
			XFE_Frame::progressBarCylonStop,
			this,
			stopCylonNotice_cb);
		
		m_parentFrame->registerInterest(
			XFE_Frame::progressBarCylonTick,
			this,
			tickCylonNotice_cb);
		
		m_parentFrame->registerInterest(
			XFE_Frame::progressBarUpdatePercent,
			this,
			progressBarUpdatePercentNotice_cb);

		m_parentFrame->registerInterest(
			XFE_Frame::progressBarUpdateText,
			this,
			progressBarUpdateTextNotice_cb);
#endif /* GLUE_COMPO_CONTEXT */		
	}
}
//////////////////////////////////////////////////////////////////////////
#ifdef MOZ_TASKBAR
void
XFE_Dashboard::createDockedTaskBar()
{
	XP_ASSERT( XfeIsAlive(m_widget) );
	XP_ASSERT( XFE_Dashboard::floatingTaskBarIsAlive() );
	XP_ASSERT( m_parentFrame != NULL );
	
	// Create the docked taskbar here
	m_dockedTaskBar = new XFE_TaskBar(m_widget,m_parentFrame,False);
	
	// Add action cb to dashboard
	XtAddCallback(m_dockedTaskBar->getBaseWidget(),
				  XmNactionCallback,
				  XFE_Dashboard::floatingActionCB,
				  (XtPointer) this);
	
	// Install the global floating task bar on the dash board
	XtVaSetValues(m_widget,XmNfloatingShell,m_floatingShell,NULL);
}
#endif
//////////////////////////////////////////////////////////////////////////
void
XFE_Dashboard::createSecurityBar()
{
	XP_ASSERT( m_parentFrame != NULL );
	XP_ASSERT( XfeIsAlive(m_widget) );
	/* XP_ASSERT( m_securityBar == NULL ); */

	if (!m_parentFrame)
		return;

	HG12928
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Dashboard::addSecurityIcon()
{
	XP_ASSERT( m_parentFrame != NULL );
	XP_ASSERT( XfeIsAlive(m_widget) );
	/* XP_ASSERT( XfeIsAlive(m_securityBar) ); */
	/* XP_ASSERT( m_securityIcon == NULL ); */

	if (!m_parentFrame)
		return;

	HG00298
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Dashboard::addSignedIcon()
{
	XP_ASSERT( m_parentFrame != NULL );
	XP_ASSERT( XfeIsAlive(m_widget) );
	/* XP_ASSERT( XfeIsAlive(m_securityBar) ); */
	/* XP_ASSERT( m_signedIcon == NULL ); */

	if (!m_parentFrame)
		return;

	HG02920
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Dashboard::configureSecurityBar()
{
	HG02092
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Dashboard::setStatusText(const char * text)
{
	D(
		Widget top = m_toplevel->getBaseWidget();
		printf("%s: setStatusText: %s\n", XtName(top), text);
	)
	XP_ASSERT( XfeIsAlive(m_statusBar) );
#if defined(DEBUG_tao_)
	printf("\nXFE_Dashboard::setStatusText=%s\n", text?text:"");
#endif

	XfeLabelSetStringPSZ(m_statusBar,(String) text);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Dashboard::setProgressBarText(const char * text)
{
	XP_ASSERT( XfeIsAlive(m_progressBar) );

#if defined(DEBUG_tao_)
	printf("\nXFE_Dashboard::setProgressBarText=%s\n", text?text:"");
#endif
	XfeLabelSetStringPSZ(m_progressBar,(String) text);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Dashboard::setProgressBarPercent(int percent)
{
	XP_ASSERT( XfeIsAlive(m_progressBar) );

#if defined(DEBUG_tao_)
	printf("\nXFE_Dashboard::setProgressBarPercent=%d\n", percent);
#endif
	if (percent < 0)
	{
		percent = 0;
	}

	if (percent > 100)
	{
		percent = 100;
	}
	XfeProgressBarSetPercentages(m_progressBar,0,percent);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Dashboard::startCylon()
{
	XP_ASSERT( XfeIsAlive(m_progressBar) );
#if defined(DEBUG_tao_)
	printf("\nXFE_Dashboard::startCylon\n");
#endif
	XfeProgressBarCylonStart(m_progressBar);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Dashboard::stopCylon()
{
	XP_ASSERT( XfeIsAlive(m_progressBar) );
#if defined(DEBUG_tao_)
	printf("\nXFE_Dashboard::stopCylon\n");
#endif

	XfeProgressBarCylonStop(m_progressBar);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Dashboard::tickCylon()
{
	XP_ASSERT( XfeIsAlive(m_progressBar) );

#if defined(DEBUG_tao_)
	printf("\nXFE_Dashboard::tickCylon\n");
#endif
	XfeProgressBarCylonTick(m_progressBar);
}
//////////////////////////////////////////////////////////////////////////
#ifdef MOZ_TASKBAR
/* static */ void
XFE_Dashboard::dockTaskBar()
{
	if (!XFE_Dashboard::isTaskBarDocked() && 
		XFE_Dashboard::floatingTaskBarIsAlive())
	{
		XtUnmanageChild(XFE_Dashboard::m_floatingTaskBar->getBaseWidget());
	}
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_Dashboard::unDockTaskBar()
{
	if (XFE_Dashboard::isTaskBarDocked() && 
		XFE_Dashboard::floatingTaskBarIsAlive())
	{
		if (m_floatingTaskBar->numEnabledButtons())
		{
			XtManageChild(XFE_Dashboard::m_floatingTaskBar->getBaseWidget());
		}
	}
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_Dashboard::toggleTaskBar()
{
	if (XFE_Dashboard::isTaskBarDocked())
	{
		XFE_Dashboard::unDockTaskBar();
	}
	else
	{
		XFE_Dashboard::dockTaskBar();
	}
}
//////////////////////////////////////////////////////////////////////////
/* static */ XP_Bool
XFE_Dashboard::isTaskBarDocked()
{
	return (XFE_Dashboard::floatingTaskBarIsAlive() &&
			!XtIsManaged(XFE_Dashboard::m_floatingTaskBar->getBaseWidget()));
}
//////////////////////////////////////////////////////////////////////////
/* static */ XP_Bool
XFE_Dashboard::floatingTaskBarIsAlive()
{
	return (XfeIsAlive(XFE_Dashboard::m_floatingShell) && 
			XfeIsAlive(XFE_Dashboard::m_floatingTaskBar->getBaseWidget()));
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_Dashboard::startFloatingTaskBar(XFE_Frame * parentFrame)
{
	// Make sure the floating task bar has not been started yet
	if (XFE_Dashboard::floatingTaskBarIsAlive())
	{
		return;
	}

	XP_ASSERT( parentFrame != NULL );

	Widget			app_shell;
	int				decor;
	int				funcs;
	fe_colormap *	floating_fe_colormap;
	Colormap		floating_colormap;
	Visual *		floating_visual;
	int				floating_depth;

	// Access the application shell (confusingly known as top_level)
	app_shell = FE_GetToplevelWidget();
	
	// Make sure the display factory is running
	fe_startDisplayFactory(app_shell);
	
	// Obtain the colormap / depth / and visual to use for the
	// floating shell.  These cannot be gotten from the parent
	// frame, since it might go away.
	floating_fe_colormap = 
		XFE_DisplayFactory::theFactory()->getSharedColormap();

	floating_colormap = fe_getColormap(floating_fe_colormap);
	
	floating_depth = 
		XFE_DisplayFactory::theFactory()->getVisualDepth();
	
	floating_visual = 
		XFE_DisplayFactory::theFactory()->getVisual();

	// Set the window manager decorations and functions
	decor = MWM_DECOR_BORDER | MWM_DECOR_TITLE | MWM_DECOR_MENU;
	funcs = MWM_FUNC_CLOSE | MWM_FUNC_MOVE;

	// Create the floating shell
	XFE_Dashboard::m_floatingShell = 
		XtVaCreatePopupShell(FLOATING_SHELL_NAME,
							 xmDialogShellWidgetClass,
							 app_shell,
							 XmNvisual,				floating_visual,
							 XmNdepth,				floating_depth,
							 XmNcolormap,		    floating_colormap,
							 XmNallowShellResize,	True,
							 XmNmwmDecorations,		decor,
							 XmNmwmFunctions,		funcs,
							 XmNdeleteResponse,		XmDO_NOTHING,
							 NULL);

	// Create the floating task bar
	XFE_Dashboard::m_floatingTaskBar = new XFE_TaskBar(m_floatingShell,
													   parentFrame,True);

	// Create the floating taskbar popup menu
	XFE_Dashboard::m_floatingPopup = new XFE_PopupMenu("taskBarContextMenu",
													   parentFrame,
													   FE_GetToplevelWidget());

	XFE_Dashboard::m_floatingPopup->setMenuSpec(XFE_Dashboard::m_floatingPopupMenuSpec);

	// Add popup menu post event handler to floating task bar items
	XfeChildrenAddEventHandler(
		XFE_Dashboard::m_floatingTaskBar->getBaseWidget(),
		ButtonPressMask,
		True,
		&XFE_Dashboard::floatingButtonEH,
		NULL);

	// Add floating shell configure event handler
	XtAddEventHandler(XFE_Dashboard::m_floatingShell,
					  StructureNotifyMask | VisibilityChangeMask,
					  True,
					  &XFE_Dashboard::floatingConfigureEH,
					  NULL);

	// Look for the defaults, in which case we use a decent default
	// Why are the defaults (-1,-1) ???
	if (fe_globalPrefs.task_bar_x == -1)
	{
		fe_globalPrefs.task_bar_x = FLOATING_ORIGIN_OFFSET;
	}

	if (fe_globalPrefs.task_bar_y == -1)
	{
		fe_globalPrefs.task_bar_y = FLOATING_ORIGIN_OFFSET;
	}

	XFE_Dashboard::setFloatingTaskBarPosition(fe_globalPrefs.task_bar_x,
											  fe_globalPrefs.task_bar_y);

	XFE_Dashboard::setFloatingTaskBarHorizontal(fe_globalPrefs.task_bar_horizontal);
	XFE_Dashboard::setFloatingTaskBarOnTop(fe_globalPrefs.task_bar_ontop);

	// Set the title for the floating dialog
	XFE_Dashboard::m_floatingTaskBar->setFloatingTitle(" ");

	// Set the initial state of the taskbar
	XFE_Dashboard::m_floatingDocked = True;

	// Undock the taskbar if needed
	if (fe_globalPrefs.task_bar_floating)
	{
		unDockTaskBar();
	}
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_Dashboard::stopFloatingTaskBar()
{
	// Return right away if the task bar is docked
	if (XFE_Dashboard::isTaskBarDocked())
	{
		return;
	}

	// Make sure the floating task bar is still alive
	if (!XFE_Dashboard::floatingTaskBarIsAlive())
	{
		return;
	}

	// Simpy unmap the taskbar shell.  The widgets (and objects) will be
	// destroyed later.
	XUnmapWindow(XtDisplay(XFE_Dashboard::m_floatingShell),
				 XtWindow(XFE_Dashboard::m_floatingShell));
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_Dashboard::setFloatingTaskBarPosition(int32 x,int32 y)
{
	XP_ASSERT( XFE_Dashboard::floatingTaskBarIsAlive() );

//	printf("setFloatingTaskBarPosition(%d,%d)\n",x,y);

	// Update the prefs
	fe_globalPrefs.task_bar_x = x;
	fe_globalPrefs.task_bar_y = y;

	// Update the floating shell's position
	XtVaSetValues(XFE_Dashboard::m_floatingShell,
				  XmNx,x,
				  XmNy,y,
				  NULL);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_Dashboard::setFloatingTaskBarHorizontal(XP_Bool horizontal)
{
	XP_ASSERT( XFE_Dashboard::floatingTaskBarIsAlive() );

	// Update the prefs
	fe_globalPrefs.task_bar_horizontal = horizontal;

	// Update the floating task bar's orientation
	XtVaSetValues(m_floatingTaskBar->getBaseWidget(),
				  XmNorientation,horizontal ? XmHORIZONTAL : XmVERTICAL,
				  NULL);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_Dashboard::setFloatingTaskBarOnTop(XP_Bool ontop)
{
	XP_ASSERT( XFE_Dashboard::floatingTaskBarIsAlive() );

	// Update the prefs
	fe_globalPrefs.task_bar_ontop = ontop;

	// Raise the shell if needed
	if (fe_globalPrefs.task_bar_ontop)
	{
		XFE_Dashboard::raiseFloatingShell();
	}
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_Dashboard::showFloatingTaskBar(XFE_Frame * parentFrame)
{
	// Make sure the floating task bar is started
	XFE_Dashboard::startFloatingTaskBar(parentFrame);

	// Manage the floating task bar
	XtManageChild(XFE_Dashboard::m_floatingTaskBar->getBaseWidget());
}
#endif
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_Dashboard, showCommandStringNotice)
	(XFE_NotificationCenter *	/* obj */,
	 void *						/* clientData */,
	 void *						callData)
{
	CommandType cmd = (CommandType)callData;
	XFE_Component*  component = (XFE_Component*)m_toplevel;
	char*       doc_string = NULL;

	if (!XfeIsAlive(m_widget))
		return;

	if (component!= NULL)
		doc_string = component->getDocString(cmd);

#ifdef DEBUG
	if (doc_string == NULL)
		doc_string = "No Documentation String for this command!!!!";
#endif

	setStatusText(doc_string);
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_Dashboard, eraseCommandStringNotice)
	(XFE_NotificationCenter *	/* obj */,
	 void *						/* clientData */,
	 void *						/* callData */)
{
	if (!XfeIsAlive(m_widget))
		return;

	setStatusText("");
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_Dashboard, setStatusTextNotice)
	(XFE_NotificationCenter *	/* obj */,
	 void *						/* clientData */,
	 void *						callData)
{
	if (!XfeIsAlive(m_widget))
		return;

	setStatusText((char *) callData);
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_Dashboard, update)
	(XFE_NotificationCenter *	/* obj */, 
	 void *						/* clientData */, 
	 void *						/* callData */)
{
	if (!XfeIsAlive(m_widget))
		return;

  HG28999
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_Dashboard, doSecurityCommand)
	(XFE_NotificationCenter *		/* obj */,
	 void *							/* clientData */,
	 void *							callData)
{
  XFE_DoCommandArgs *cmdArgs = (XFE_DoCommandArgs *)callData;

	if (!XfeIsAlive(m_widget))
		return;

  HG09219
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_Dashboard, startCylonNotice)
	(XFE_NotificationCenter *	/* obj */,
	 void *						/* clientData */,
	 void *						/* callData */)
{
	if (!XfeIsAlive(m_widget))
		return;

	startCylon();
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_Dashboard, stopCylonNotice)
	(XFE_NotificationCenter *	/* obj */,
	 void *						/* clientData */,
	 void *						/* callData */)
{
	if (!XfeIsAlive(m_widget))
		return;

	stopCylon();
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_Dashboard, tickCylonNotice)
	(XFE_NotificationCenter *	/* obj */,
	 void *						/* clientData */,
	 void *						/* callData */)
{
	if (!XfeIsAlive(m_widget))
		return;

	tickCylon();
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_Dashboard, progressBarUpdatePercentNotice)
	(XFE_NotificationCenter *	/* obj */,
	 void *						/* clientData */,
	 void *						callData)
{
	if (!XfeIsAlive(m_widget))
		return;

	setProgressBarPercent((int) callData);
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_Dashboard, progressBarUpdateTextNotice)
	(XFE_NotificationCenter *	/* obj */,
	 void *						/* clientData */,
	 void *						callData)
{
	setProgressBarText((char *) callData);
}
//////////////////////////////////////////////////////////////////////////

#ifdef MOZ_TASKBAR
/* static */ void
XFE_Dashboard::floatingMappingCB(Widget		/* w */,
								 XtPointer	clientData,
								 XtPointer	callData)
{
	XFE_Dashboard *			db = (XFE_Dashboard *) clientData;
	XmAnyCallbackStruct *	cbs = (XmAnyCallbackStruct *) callData;

	XP_ASSERT( db != NULL );
	XP_ASSERT( cbs != NULL );

	if (!db || !cbs)
	{
		return;
	}

	switch(cbs->reason)
	{
	case XmCR_FLOATING_MAP:

		if (XFE_Dashboard::m_floatingDocked)
		{
			// Floating task bar is now docked
			XFE_Dashboard::m_floatingDocked = False;

			// Increase the top level window count for the floating taskbar
			fe_WindowCount++;
			
			// Notify all frames about the change in top level window count
			XFE_MozillaApp::theApp()->notifyInterested(
				XFE_MozillaApp::changeInToplevelFrames,
				(void *) fe_WindowCount);
			
			D( printf("  map = undock (%s,%s) count = %d\n",
					  XtName(db->m_parentFrame->getBaseWidget()),
					  XtName(XtParent(XtParent(w))),
					  fe_WindowCount); );

			// Update the prefs
			fe_globalPrefs.task_bar_floating = True;

			// Make sure the floating shell is raised
			XFE_Dashboard::raiseFloatingShell();
		}

		break;

	case XmCR_FLOATING_UNMAP:

		if (!XFE_Dashboard::m_floatingDocked)
		{
			// Floating task bar is now un docked
			XFE_Dashboard::m_floatingDocked = True;

			// Decrease the top level window count for the floating taskbar
			fe_WindowCount--;
			
			// Notify all frames about the change in top level window count
			XFE_MozillaApp::theApp()->notifyInterested(
				XFE_MozillaApp::changeInToplevelFrames,
				(void *) fe_WindowCount);
			
			D( printf("unmap =   dock (%s,%s) count = %d\n",
					  XtName(db->m_parentFrame->getBaseWidget()),
					  XtName(XtParent(XtParent(w))),
					  fe_WindowCount); );

			// If no windows are left after closing the floating task bar, exit
			if (XFE_MozillaApp::theApp()->toplevelWindowCount() == 0)
			{
				// If the app is closed from the taskbar, then the 
				// instead of changing the pref to not floating, we make 
				// sure it remains floating.
				fe_globalPrefs.task_bar_floating = True;

				XFE_MozillaApp::theApp()->exit(0);
			}
			else
			{
				// Update the prefs
				fe_globalPrefs.task_bar_floating = False;
			}
		}

		break;
	}
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_Dashboard::floatingActionCB(Widget		/* w */,
								XtPointer	clientData,
								XtPointer	/* callData */)
{
	XFE_Dashboard *			db = (XFE_Dashboard *) clientData;

	XP_ASSERT( db != NULL );

	if (!db)
	{
		return;
	}
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_Dashboard::floatingButtonEH(Widget		/* w */,
								XtPointer	/* clientData */,
								XEvent *	event,
								Boolean *	cont)
{
	// Make sure Button3 was pressed
	if ((event->type == ButtonPress) && (event->xbutton.button == Button3))
	{
 		XFE_Dashboard::m_floatingPopup->position(event);
 		XFE_Dashboard::m_floatingPopup->show();
 		XFE_Dashboard::m_floatingPopup->raise();
	}
	
	*cont = True;
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_Dashboard::floatingConfigureEH(Widget		/* w */,
								   XtPointer	/* clientData */,
								   XEvent *		event,
								   Boolean *	cont)
{
	// Configure
	if (event->type == ConfigureNotify)
	{
		Position x = XfeRootX(XFE_Dashboard::m_floatingShell);
		Position y = XfeRootY(XFE_Dashboard::m_floatingShell);
		Position offset_x;
		Position offset_y;

		// Try and obtain the offset occupied by the wm decorations
		if (!XfeShellGetDecorationOffset(XFE_Dashboard::m_floatingShell,
										 &offset_x,&offset_y))
		{
			offset_x = 0;
			offset_y = 0;
		}

 		x -= offset_x;
 		y -= offset_y;

		// Update the new position in the prefs
		fe_globalPrefs.task_bar_x = x;
		fe_globalPrefs.task_bar_y = y;
	}
	// Visibility
	else if (event->type == VisibilityNotify)
	{
		// If it is, raise the floating shell
		XFE_Dashboard::raiseFloatingShell();
	}
	
	*cont = True;
}
#endif
//////////////////////////////////////////////////////////////////////////
void
XFE_Dashboard::setShowSecurityIcon(XP_Bool state)
{
	XP_ASSERT( XfeIsAlive(m_widget) );

	// Create the security icon only if it is not disabled through resources.
	if (!XfeChildIsEnabled(m_widget,
						   xfeCmdViewSecurity,
						   "ViewSecurity",
						   True))
	{
		return;
	}

	HG89219
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Dashboard::setShowStatusBar(XP_Bool state)
{
	XP_ASSERT( XfeIsAlive(m_widget) );

	// Create the status bar if needed
	if (!m_statusBar)
	{
		createStatusBar();
	}

	XfeSetManagedState(m_statusBar,state);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Dashboard::setShowProgressBar(XP_Bool state)
{
	XP_ASSERT( XfeIsAlive(m_widget) );

	// Create the progress bar if needed
	if (!m_progressBar)
	{
		createProgressBar();
	}

	XfeSetManagedState(m_progressBar,state);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Dashboard::setShowSignedIcon(XP_Bool state)
{
	XP_ASSERT( XfeIsAlive(m_widget) );

	// Create the signed icon only if it is not disabled through resources.
	if (!XfeChildIsEnabled(m_widget,
						   xfeCmdViewSecurity,
						   "ViewSecurity",
						   True))
	{
		return;
	}


	HG82198
}
//////////////////////////////////////////////////////////////////////////
XP_Bool
XFE_Dashboard::isSecurityIconShown()
{
	return (HG20303);
}
//////////////////////////////////////////////////////////////////////////
XP_Bool
XFE_Dashboard::isSignedIconShown()
{
	return (HG02023);
}
//////////////////////////////////////////////////////////////////////////
XP_Bool
XFE_Dashboard::processTraversal(XmTraversalDirection direction)
{
	HG93649

	// Try the progress bar
	if (XfeIsAlive(m_progressBar) && 
		XmProcessTraversal(m_progressBar,direction))
	{
		return True;
	}

	// Try the status bar
	if (XfeIsAlive(m_statusBar) && 
		XmProcessTraversal(m_statusBar,direction))
	{
		return True;
	}

#ifdef MOZ_TASKBAR
	// Try the task bar
	if ((m_dockedTaskBar && m_dockedTaskBar->isAlive()) && 
		XmProcessTraversal(m_dockedTaskBar->getBaseWidget(),direction))
	{
		return True;
	}
#endif

	// Try the base widget
	if (XfeIsAlive(m_widget) && 
		XmProcessTraversal(m_widget,direction))
	{
		return True;
	}

	return False;
}
//////////////////////////////////////////////////////////////////////////
#ifdef MOZ_TASKBAR
/* static */ XFE_TaskBar *
XFE_Dashboard::getTaskBar()
{
	return m_dockedTaskBar;
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_Dashboard::raiseFloatingShell()
{
	XP_ASSERT( XFE_Dashboard::floatingTaskBarIsAlive() );

	// Dont raise the window if the XmDisplay is grabbed or a tooltip is
	// showing
	if (XfeDisplayIsUserGrabbed(XFE_Dashboard::m_floatingShell) ||
		fe_ToolTipIsShowing())
	{
		return;
	}

	// If the taskbar needs to be ontop, raise it
	if (fe_globalPrefs.task_bar_ontop)
	{
		// The time 'now'
		time_t time_now = time(NULL);

		// The time difference between 'now' and the last time we were called
		time_t time_diff = time_now - m_floatingLastRaisedTime;

		// If the difference is 0, then we are being called more than once
		// in one second.
		if (time_diff == 0)
		{
			m_floatingTimesRaisedPerSecond++;
		}
		// Otherwise we are being called after a 'long' interval and we 
		// reset the time/sec counter.
		else
		{
			m_floatingTimesRaisedPerSecond = 0;
		}

		// If the times/sec counter is more than a magic number, then we
		// are being called too often and we are most likely in an infinite
		// loop fighting with another window that also wants to be always
		// on top.
		if (m_floatingTimesRaisedPerSecond <= MAX_FLOATING_RAISES_PER_SECOND)
		{
			Widget shell = XFE_Dashboard::m_floatingShell;
			
			// Place window on top
			XWindowChanges changes;
			
			changes.stack_mode = Above;
			
			XReconfigureWMWindow(XtDisplay(shell),
								 XtWindow(shell), 
								 XScreenNumberOfScreen(XtScreen(shell)),
								 CWStackMode,&changes);

			m_floatingLastRaisedTime = time_now;
		}
	}
}
//////////////////////////////////////////////////////////////////////////
/* static */ XFE_TaskBar *
XFE_Dashboard::getFloatingTaskBar()
{
	XP_ASSERT( XFE_Dashboard::floatingTaskBarIsAlive() );
	
	return XFE_Dashboard::m_floatingTaskBar;
}
//////////////////////////////////////////////////////////////////////////
extern "C" void
fe_showTaskBar(Widget /* toplevel */)
{
	XFE_BookmarkFrame * parentFrame = XFE_BookmarkFrame::getBookmarkFrame();

	XP_ASSERT( parentFrame != NULL );

	XFE_Dashboard::showFloatingTaskBar(parentFrame);
}
//////////////////////////////////////////////////////////////////////////
#endif
