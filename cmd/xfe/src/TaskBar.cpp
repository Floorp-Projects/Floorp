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
   TaskBar.cpp -- implementation file for XFE_TaskBar
   Created: Stephen Lamm <slamm@netscape.com>, 19-Nov-96.
 */


#include "TaskBar.h"
#include "TaskBarDrop.h"

#include "Button.h"
#include "Command.h"
#include "Frame.h"

#ifdef MOZ_MAIL_NEWS
#include "MNView.h"
#endif

#include "MozillaApp.h"
#include "BrowserFrame.h"
#include "Dashboard.h"
#include "BookmarkFrame.h"
#include "Toolbar.h"
#include "xfe2_extern.h"
#include "prefapi.h"

#include <Xfe/ToolBar.h>
#include <Xm/Form.h>

#ifdef MOZ_MAIL_NEWS
#define SM_BIFF_UNKNOWN_ICONS TaskSm_MailU_group
#define SM_BIFF_NEWMAIL_ICONS TaskSm_MailY_group
#define SM_BIFF_NOMAIL_ICONS  TaskSm_MailN_group

#define LG_BIFF_UNKNOWN_ICONS Task_MailU_group
#define LG_BIFF_NEWMAIL_ICONS Task_MailY_group
#define LG_BIFF_NOMAIL_ICONS  Task_MailN_group
#endif

//////////////////////////////////////////////////////////////////////////
//
// Small icons.
//
//////////////////////////////////////////////////////////////////////////
/*static */ TaskBarSpec 
XFE_TaskBar::m_dockedSpec[] = 
{
    { xfeCmdOpenOrBringUpBrowser,    PUSHBUTTON,  &TaskSm_Browser_group },
#ifdef MOZ_MAIL_NEWS
    { xfeCmdOpenInboxAndGetNewMessages,	PUSHBUTTON,  &SM_BIFF_UNKNOWN_ICONS },
    { xfeCmdOpenNewsgroups, PUSHBUTTON,  &TaskSm_Discussions_group },
#endif
    { xfeCmdOpenEditor,     PUSHBUTTON,  &TaskSm_Composer_group },
    { NULL }
};

//////////////////////////////////////////////////////////////////////////
//
// Large icons.
//
//////////////////////////////////////////////////////////////////////////
/*static */ TaskBarSpec 
XFE_TaskBar::m_floatingSpec[] = 
{
    { xfeCmdOpenOrBringUpBrowser,    PUSHBUTTON,  &Task_Browser_group },
#ifdef MOZ_MAIL_NEWS
    { xfeCmdOpenInboxAndGetNewMessages,	PUSHBUTTON,  &LG_BIFF_UNKNOWN_ICONS },
    { xfeCmdOpenNewsgroups, PUSHBUTTON,  &Task_Discussions_group },
#endif
    { xfeCmdOpenEditor,     PUSHBUTTON,  &Task_Composer_group },
    { NULL }
};

//////////////////////////////////////////////////////////////////////////
//
// XFE_TaskBar Public Methods
//
//////////////////////////////////////////////////////////////////////////
XFE_TaskBar::XFE_TaskBar(Widget			parent,
						 XFE_Frame *	parent_frame,
						 XP_Bool		is_floating) :
	XFE_Component(parent_frame),
    m_isFloating(is_floating),
	m_parentFrame(parent_frame),
	m_toolBar(NULL),
	m_undockButton(NULL)
#ifdef MOZ_MAIL_NEWS
    ,m_biffNoticeInstalled(False)
#endif
{
	XP_ASSERT( parent_frame != NULL );

	// Create the base form
	createBaseForm(parent);

	// Create widgets according to docking state
	if (!m_isFloating)
    {
		createDockedWidgets();
    }
	else
    {
		createFloatingWidgets();

		XFE_MozillaApp::theApp()->registerInterest(
			XFE_MozillaApp::appBusyCallback,
			this,
			updateFloatingBusyState_cb,
			(void*)True);

		XFE_MozillaApp::theApp()->registerInterest(
			XFE_MozillaApp::appNotBusyCallback,
			this,
			updateFloatingBusyState_cb,
			(void*)False);
    }
	
	installDestroyHandler();
}
//////////////////////////////////////////////////////////////////////////
XFE_TaskBar::~XFE_TaskBar()
{
#ifdef MOZ_MAIL_NEWS
	// Unregister the biff notice if needed
	if (m_biffNoticeInstalled)
	{
		XFE_MozillaApp::theApp()->unregisterInterest(
			XFE_MozillaApp::biffStateChanged,
			this,
			(XFE_FunctionNotification)updateBiffStateNotice_cb);
	}
#endif

	if (m_isFloating)
    {
		XFE_MozillaApp::theApp()->unregisterInterest(
			XFE_MozillaApp::appBusyCallback,
			this,
			updateFloatingBusyState_cb,
			(void*)True);

		XFE_MozillaApp::theApp()->unregisterInterest(
			XFE_MozillaApp::appNotBusyCallback,
			this,
			updateFloatingBusyState_cb,
			(void*)False);
    }
}
//////////////////////////////////////////////////////////////////////////
#ifdef MOZ_MAIL_NEWS
XFE_CALLBACK_DEFN(XFE_TaskBar, updateBiffStateNotice)
	(XFE_NotificationCenter*	/* obj */,
	 void *						/* callData */, 
	 void *						clientData)
{
	MSG_BIFF_STATE state = (MSG_BIFF_STATE) clientData;
	IconGroup *icons = 0;
	
	switch (state)
    {
    case MSG_BIFF_NewMail:
		
		if (m_isFloating)
			icons = &LG_BIFF_NEWMAIL_ICONS;
		else
			icons = &SM_BIFF_NEWMAIL_ICONS;
		break;
		
    case MSG_BIFF_NoMail:
		
		if (m_isFloating)
			icons = &LG_BIFF_NOMAIL_ICONS;
		else
			icons = &SM_BIFF_NOMAIL_ICONS;
		break;
		
    case MSG_BIFF_Unknown:
		
		if (m_isFloating)
			icons = &LG_BIFF_UNKNOWN_ICONS;
		else
			icons = &SM_BIFF_UNKNOWN_ICONS;
		break;
		
    default:
		XP_ASSERT(0);
    }
	
	setIconGroupForCommand(xfeCmdOpenInboxAndGetNewMessages, 
						   icons);
}
//////////////////////////////////////////////////////////////////////////
#endif


//////////////////////////////////////////////////////////////////////////
//
// XFE_TaskBar Private Methods
//
//////////////////////////////////////////////////////////////////////////
void
XFE_TaskBar::setIconGroupForCommand(CommandType cmd, IconGroup *icons)
{
	Widget *	children;
	Cardinal	num_children;
	Cardinal	i;
  
	XfeChildrenGet(m_widget,&children,&num_children);

	for (i = 0; i < num_children; i ++)
    {
		if (XfeIsButton(children[i]) && !XfeIsPrivateComponent(children[i]))
		{
			if (Command::intern(XtName(children[i])) == cmd)
			{
				XFE_Button * button = 
					(XFE_Button *) XfeInstancePointer(children[i]);
				
				XP_ASSERT(button);

				if (!button) return;
				
				button->setPixmap(icons);
				
				return;
			}
		}
    }

	XP_ASSERT(0); // command not found in the taskbar...
}
//////////////////////////////////////////////////////////////////////////
static void DropSiteDestroyCb(Widget,XtPointer cd,XtPointer)
{
    if (cd)
        delete (XFE_TaskBarDrop*)cd;
}
//////////////////////////////////////////////////////////////////////////
Widget
XFE_TaskBar::createTaskBarButton(TaskBarSpec *spec)
{
    Widget result = NULL;
    
    XP_ASSERT( XfeIsAlive(m_toolBar) );
    
    if ( (spec->taskBarButtonName == xfeCmdOpenEditor) &&
         fe_IsEditorDisabled() )
	{
		return NULL;
	}

    // Create a XFE_Button.
    XFE_Button *newButton = new XFE_Button(m_parentFrame,
										   m_toolBar,
                                           spec->taskBarButtonName, 
                                           spec->icons);
    
    newButton->registerInterest(XFE_Button::doCommandCallback,
								this,
								(XFE_FunctionNotification)doCommandNotice_cb);

	XtVaSetValues(newButton->getBaseWidget(),
				  XmNtraversalOn,			False,
				  XmNhighlightThickness,	0,
				  NULL);
	
	// Show the new button
	newButton->show();
	
    result = newButton->getBaseWidget();
	
    // register drop site and associated destroy callback
	// for interesting commands
    
	if (spec->taskBarButtonName == xfeCmdOpenOrBringUpBrowser
		|| spec->taskBarButtonName == xfeCmdOpenEditor 
#ifdef MOZ_MAIL_NEWS
		|| spec->taskBarButtonName == xfeCmdOpenInboxAndGetNewMessages 
		|| spec->taskBarButtonName == xfeCmdOpenNewsgroups
#endif
		) 
	{
		XFE_TaskBarDrop * dropSite = 
			new XFE_TaskBarDrop(result,spec->taskBarButtonName);

		dropSite->enable();

        XtAddCallback(result,
					  XmNdestroyCallback,
					  DropSiteDestroyCb,
					  (XtPointer)dropSite);
    }

#ifdef MOZ_MAIL_NEWS
	if (spec->taskBarButtonName == xfeCmdOpenInboxAndGetNewMessages)
	{
		XFE_MozillaApp::theApp()->registerInterest(
			XFE_MozillaApp::biffStateChanged,
			this,
			(XFE_FunctionNotification) updateBiffStateNotice_cb);
		
		// make sure we have the correct biff icon when we start.
		updateBiffStateNotice(NULL, 
							  NULL,
							  (void*) XFE_MNView::getBiffState());

		m_biffNoticeInstalled = True;
	}
#endif
        
    return result;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_TaskBar::createToolBar(String name)
{
	XP_ASSERT( XfeIsAlive(m_widget) );
	XP_ASSERT( name != NULL );

	m_toolBar = XtVaCreateWidget(name,
								 xfeToolBarWidgetClass,
								 m_widget,
								 XmNusePreferredWidth,		True,
								 XmNusePreferredHeight,		True,
								 NULL);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_TaskBar::createBaseForm(Widget parent)
{
	XP_ASSERT( XfeIsAlive(parent) );

	Widget form = XtVaCreateWidget("taskBarForm",
								   xmFormWidgetClass,
								   parent,
								   NULL);

	setBaseWidget(form);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_TaskBar::createUndockButton()
{
	XP_ASSERT( XfeIsAlive(m_widget) );
	
	m_undockButton = XtVaCreateWidget("taskBarUndockButton",
									  xfeButtonWidgetClass,
									  m_widget,
									  XmNusePreferredWidth,		True,
									  XmNusePreferredHeight,	False,
									  NULL);
	// Create the undock pixmap
	if (!XfePixmapGood(TaskSm_Handle_group.pixmap_icon.pixmap))
	{
		IconGroup_createAllIcons(&TaskSm_Handle_group,
								 
								 XfeAncestorFindByClass(m_widget,
														shellWidgetClass,
														XfeFIND_ANY),
								 
								 XfeForeground(m_widget),
								 
								 XfeBackground(m_widget));
	}

	// Set the undock pixmap on the button if valid
	if (XfePixmapGood(TaskSm_Handle_group.pixmap_icon.pixmap))
	{
		XtVaSetValues(m_undockButton,
					  XmNpixmap, TaskSm_Handle_group.pixmap_icon.pixmap,
					  NULL);
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_TaskBar::createDockedWidgets()
{
	XP_ASSERT( XfeIsAlive(m_widget) );

	createUndockButton();

	createToolBar("dockedTaskBar");
	
	XP_ASSERT( XfeIsAlive(m_undockButton) );
	XP_ASSERT( XfeIsAlive(m_toolBar) );
	
	// Place the undock button on the left
	XtVaSetValues(m_undockButton,
				  XmNtopAttachment,			XmATTACH_FORM,
				  XmNbottomAttachment,		XmATTACH_FORM,
				  XmNleftAttachment,		XmATTACH_FORM,
				  XmNrightAttachment,		XmATTACH_NONE,
				  NULL);

	// Place the toolbar on the rigth
	XtVaSetValues(m_toolBar,
				  XmNtopAttachment,			XmATTACH_FORM,
				  XmNbottomAttachment,		XmATTACH_FORM,
				  XmNleftAttachment,		XmATTACH_WIDGET,
				  XmNrightAttachment,		XmATTACH_FORM,
				  XmNleftWidget,			m_undockButton,
				  XmNorientation,			XmHORIZONTAL,
				  NULL);
	
	// Manage the undock button
	XtManageChild(m_undockButton);

	// Manage the toolbar
	XtManageChild(m_toolBar);

	// Create floating buttons
	createButtons(m_dockedSpec);

#if 0
	// If the floating taskbar does not have any enabled buttons, then 
	// we dont need to show the action button.
	XFE_TaskBar * ftb = XFE_Dashboard::getFloatingTaskBar();

	if (!ftb || !ftb->numEnabledButtons())
	{
		XtVaSetValues(m_widget,XmNshowActionButton,False,NULL);
	}
#endif
}
//////////////////////////////////////////////////////////////////////////
void
XFE_TaskBar::createFloatingWidgets()
{
	XP_ASSERT( XfeIsAlive(m_widget) );

	unsigned char	orientation = 
		fe_globalPrefs.task_bar_horizontal ? XmHORIZONTAL : XmVERTICAL;
	
	createToolBar("dockedTaskBar");
	
	XP_ASSERT( XfeIsAlive(m_toolBar) );

	// Configuer the toolbar
	XtVaSetValues(m_toolBar,
				  XmNtopAttachment,			XmATTACH_FORM,
				  XmNbottomAttachment,		XmATTACH_FORM,
				  XmNleftAttachment,		XmATTACH_FORM,
				  XmNrightAttachment,		XmATTACH_FORM,
				  XmNorientation,			orientation,
				  XmNbuttonLayout,			XmBUTTON_LABEL_ON_BOTTOM,
				  NULL);

	// Manage the toolbar
	XtManageChild(m_toolBar);
	
	// Create floating buttons
	createButtons(m_floatingSpec);

	// Update the floating appearance for the first time
	updateFloatingAppearance();
 
	// Update the icons layout when needed
    XFE_MozillaApp::theApp()->registerInterest(
		XFE_MozillaApp::updateToolbarAppearance,
		this,
		(XFE_FunctionNotification)updateIconAppearance_cb);
}
//////////////////////////////////////////////////////////////////////////
void 
XFE_TaskBar::createButtons(TaskBarSpec * spec)
{
	XP_ASSERT( XfeIsAlive(m_widget) );
	
	if (!spec)
	{
		return;
	}

	TaskBarSpec *cur_spec = spec;
	
	while (cur_spec->taskBarButtonName)
	{
		// Create the button only if it is not disabled through resources.
  		if (XfeChildIsEnabled(m_widget,
							  (String) cur_spec->taskBarButtonName,
							  "TaskBarButton",
							  True))
  		{
			createTaskBarButton(cur_spec);
  		}

		cur_spec++;
	}
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_TaskBar,doCommandNotice)
	(XFE_NotificationCenter *		/*obj*/,
	 void *							/*clientData*/,
	 void *							callData)
{
	XP_ASSERT( m_parentFrame != NULL );

	// This code is identical to that in XFE_Toolbar::doCommand_cb()
	XFE_DoCommandArgs *	cmdArgs = (XFE_DoCommandArgs *)callData;

	if (m_parentFrame->handlesCommand(cmdArgs->cmd,
									  cmdArgs->callData,
									  cmdArgs->info)
		&& m_parentFrame->isCommandEnabled(cmdArgs->cmd,
										   cmdArgs->callData,
										   cmdArgs->info))
	{
		// Busy 
		XFE_MozillaApp::theApp()->notifyInterested(XFE_MozillaApp::appBusyCallback);

		xfe_ExecuteCommand(m_parentFrame, cmdArgs->cmd, cmdArgs->callData,
						   cmdArgs->info );
		

		// Not busy
		XFE_MozillaApp::theApp()->notifyInterested(XFE_MozillaApp::appNotBusyCallback);

		m_parentFrame->notifyInterested(Command::commandDispatchedCallback, 
										callData);
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_TaskBar::updateFloatingAppearance() 
{
	XP_ASSERT( XfeIsAlive(m_widget) );
	XP_ASSERT( m_isFloating == True );

	unsigned char	button_layout;

    int32 toolbar_style;
    PREF_GetIntPref("browser.chrome.toolbar_style", &toolbar_style);

	button_layout = XFE_Toolbar::styleToLayout(toolbar_style);

	XtVaSetValues(m_widget,XmNbuttonLayout,button_layout,NULL);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_TaskBar::setFloatingTitle(const char * title)
{
	XP_ASSERT( XfeIsAlive(m_widget) );
	XP_ASSERT( m_isFloating == True );
	XP_ASSERT( m_isFloating == True );

	Widget shell_widget = XfeAncestorFindByClass(m_widget,
												 shellWidgetClass,
												 XfeFIND_ANY);

	XtVaSetValues(shell_widget,XmNtitle,title,NULL);
}
//////////////////////////////////////////////////////////////////////////
Cardinal
XFE_TaskBar::numEnabledButtons()
{
	if (!XfeIsAlive(m_widget))
	{
		return 0;
	}

	Cardinal num_managed = XfeChildrenGetNumManaged(m_widget);

	// Ignore the action button
	if (!m_isFloating && (num_managed > 0))
	{
		num_managed--;
	}

	return num_managed;
}
//////////////////////////////////////////////////////////////////////////
Widget
XFE_TaskBar::getUndockButton()
{
	return m_undockButton;
}
//////////////////////////////////////////////////////////////////////////

XFE_CALLBACK_DEFN(XFE_TaskBar,updateIconAppearance)
	(XFE_NotificationCenter *	/*obj*/, 
	 void *						/*clientData*/, 
	 void *						/*callData*/)
{
	updateFloatingAppearance();
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_TaskBar, updateFloatingBusyState)
	(XFE_NotificationCenter *	/* obj */,
	 void *						clientData,
	 void *						/* callData */)
{
	XP_Bool busy = (XP_Bool) (int) clientData;

	Widget floatingShell = XfeAncestorFindByClass(m_widget,
												  shellWidgetClass,
												  XfeFIND_ANY);

	// Dont update busy state if not realized/alive or undocked
	if (!XfeIsAlive(floatingShell) || 
		!XtIsRealized(floatingShell) ||
		XFE_Dashboard::isTaskBarDocked())
	{
		return;
	}

	if (busy)
	{
		MWContext *	context = m_parentFrame->getContext();
		Cursor		cursor = CONTEXT_DATA(context)->busy_cursor;

		XDefineCursor(XtDisplay(floatingShell),
					  XtWindow(floatingShell),
					  cursor);
	}
	else
	{
		XUndefineCursor(XtDisplay(floatingShell),XtWindow(floatingShell));
	}
}
//////////////////////////////////////////////////////////////////////////
