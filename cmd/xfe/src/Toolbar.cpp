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
   Toolbar.cpp -- implementation file for toolbars
   Created: Chris Toshok <toshok@netscape.com>, 13-Aug-96.
 */


#include "rosetta.h"
#include "Frame.h"
#include "HTMLView.h"
#include "MozillaApp.h"
#include "Toolbar.h"
#include "Command.h"
#include "Button.h"
#include "Logo.h"
#include "Toolbox.h"
#include "BackForwardMenu.h"
#include "prefapi.h"

#include <Xfe/ToolBar.h>
#include <Xfe/ToolBox.h>
#include <Xfe/ToolItem.h>
#include <Xfe/Cascade.h>

#if 0
#define D(x) x
#else
#define D(x)
#endif

#define MAX_DELAY 5000
#define MIN_DELAY 0

#define LOGO_NAME				"logo"
#define TOOL_BAR_ITEM_NAME		"toolBarItem"
#define TOOL_BAR_NAME			"toolBar"

//////////////////////////////////////////////////////////////////////////
//
// User command resources
//
//////////////////////////////////////////////////////////////////////////
/* static */ XtResource
XFE_Toolbar::m_user_commandResources[] = 
{
	{
		"commandName",
		"CommandName",
		XmRString,
		sizeof(String),
		XtOffset(XFE_Toolbar *, m_user_commandName),
		XmRString,
		"Error"
	},
	{
		"commandData",
		"CommandData",
		XmRString,
		sizeof(String),
		XtOffset(XFE_Toolbar *, m_user_commandData),
		XmRImmediate,
		(XtPointer) NULL
	},
	{
		"commandIcon",
		"CommandIcon",
		XmRString,
		sizeof(String),
		XtOffset(XFE_Toolbar *, m_user_commandIcon),
		XmRImmediate,
		(XtPointer) NULL
	},
	{
		"commandSubMenu",
		"CommandSubMenu",
		XmRString,
		sizeof(String),
		XtOffset(XFE_Toolbar *, m_user_commandSubMenu),
		XmRImmediate,
		(XtPointer) NULL
	},
};
//////////////////////////////////////////////////////////////////////////

//
// XFE_Toolbar class
//
//////////////////////////////////////////////////////////////////////////
XFE_Toolbar::XFE_Toolbar(XFE_Frame *		parent_frame,
						 XFE_Toolbox *		parent_toolbox,
						 ToolbarSpec *		spec) :
	XFE_ToolboxItem(parent_frame,parent_toolbox)
{
	m_parentFrame = parent_frame;

	m_spec = spec;

    m_security = NULL;

	createMain(m_parentToolbox->getBaseWidget());

	createItems();

	// add drag widgets for toolbar
	addDragWidget(getBaseWidget());
	addDragWidget(getToolBar());
	addDragWidget(getToolBarForm());

	m_parentFrame->registerInterest(XFE_View::chromeNeedsUpdating,
									this,
									(XFE_FunctionNotification)update_cb);

	m_parentFrame->registerInterest(XFE_View::commandNeedsUpdating,
									this,
									(XFE_FunctionNotification)updateCommand_cb);

    XFE_MozillaApp::theApp()->registerInterest(XFE_View::commandNeedsUpdating,
											   this,
											   (XFE_FunctionNotification)updateCommand_cb);

    XFE_MozillaApp::theApp()->registerInterest(XFE_MozillaApp::updateToolbarAppearance,
                                               this,
                                               (XFE_FunctionNotification)updateToolbarAppearance_cb);
}
//////////////////////////////////////////////////////////////////////////
XFE_Toolbar::~XFE_Toolbar()
{
	m_parentFrame->unregisterInterest(XFE_View::chromeNeedsUpdating,
									  this,
									  (XFE_FunctionNotification)update_cb);

	m_parentFrame->unregisterInterest(XFE_View::commandNeedsUpdating,
									  this,
									  (XFE_FunctionNotification)updateCommand_cb);

    XFE_MozillaApp::theApp()->unregisterInterest(XFE_View::commandNeedsUpdating,
												 this,
												 (XFE_FunctionNotification)updateCommand_cb);

    XFE_MozillaApp::theApp()->unregisterInterest(XFE_MozillaApp::updateToolbarAppearance,
                                                 this,
                                                 (XFE_FunctionNotification)updateToolbarAppearance_cb);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Toolbar::updateButton(Widget w)
{
	XP_ASSERT( XfeIsAlive(w) );
	XP_ASSERT( XfeIsButton(w) );

	// Make very sure we deal with a valid button/cascade
	if (!w || !XfeIsButton(w) || !XfeIsAlive(w) || XfeIsPrivateComponent(w))
	{
		return;
	}

	XFE_Button * button = (XFE_Button *) XfeInstancePointer(w);

	XP_ASSERT(button);

	if (!button)
	{
		return;
	}

	// Ontain the button's command
	CommandType cmd = button->getCmd();

	button->setPretendSensitive(m_parentFrame->isCommandEnabled(cmd));

	// The submenu items of the back/forward buttons are dynamic and
	// always active, so leave them alone.
	if ((cmd == xfeCmdBack) || (cmd == xfeCmdForward))
	{
		return;
	}		

    // check for Cascade button with pulldown attached
	if (XfeIsCascade(w)) 
	{
		Widget *	children;
		Cardinal	num_children;
		Cardinal	j;

		// Obtain the children
		XfeCascadeGetChildren(w,&children,&num_children);

		for (j = 0; j < num_children; j++)
		{
			Widget child = children[j];

			if (XfeIsAlive(child) && XtName(child))
			{
				// Only gadgets are used by XFE_Button for submenus

				if (XmIsPushButtonGadget(child))
				{
					CommandType cmd = Command::intern(XtName(child));
					Boolean enabled = m_parentFrame->isCommandEnabled(cmd);

					XtSetSensitive(child,enabled);

					// toolbar button must be enabled if any of the 
					// subcommands are.
					if (enabled)
						button->setPretendSensitive(True);

					char *command_string;

					command_string = m_parentFrame->commandToString(cmd);
					
					if ( command_string )
					{
						XmString str;
								
						str = XmStringCreate(command_string, 
											 XmFONTLIST_DEFAULT_TAG);
								
						XtVaSetValues(child, XmNlabelString, str, NULL);
								
						XmStringFree(str);
					}
				}
				else if (XmIsToggleButtonGadget(child))
				{
					CommandType cmd = Command::intern(XtName(child));
					Boolean turnOn = m_parentFrame->isCommandSelected(cmd);

					XmToggleButtonGadgetSetState(child,turnOn,False);

					char *command_string;

					command_string = m_parentFrame->commandToString(cmd);
					
					if ( command_string )
					{
						XmString str;
								
						str = XmStringCreate(command_string, 
											 XmFONTLIST_DEFAULT_TAG);
								
						XtVaSetValues(child, XmNlabelString, str, NULL);
								
						XmStringFree(str);
					}
				}
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_Toolbar, updateCommand)(XFE_NotificationCenter */*obj*/, 
					      void */*clientData*/, 
					      void *callData)
{
	D(printf("In XFE_Toolbar::updateCommand(%s)\n",Command::getString(cmd));)  

	// Make sure the toolbar is alive
    if (!XfeIsAlive(m_toolBar))
    {
        return;
    }
	
	Widget *		children;
	Cardinal		num_children;
	Cardinal		i;
	CommandType		cmd = (CommandType) callData;

	XfeChildrenGet(m_toolBar,&children,&num_children);	
	
	for (i = 0; i < num_children; i ++)
	{
		if (XfeIsButton(children[i]) && !XfeIsPrivateComponent(children[i]))
		{
			XFE_Button * button = 
				(XFE_Button *) XfeInstancePointer(children[i]);
			
			if (button)
			{
				if (cmd == button->getCmd())
					button->setPretendSensitive(m_parentFrame->isCommandEnabled(cmd));
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_Toolbar, update)(XFE_NotificationCenter */*obj*/, 
									   void */*clientData*/, 
									   void */*callData*/)
{
  update();
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Toolbar::update()
{
	D(	printf ("In XFE_Toolbar::update()\n");)

	// Make sure the toolbar is alive
    if (!XfeIsAlive(m_toolBar))
    {
        return;
    }
  
	Widget *		children;
	Cardinal		num_children;
	Cardinal		i;

	XfeChildrenGet(m_toolBar,&children,&num_children);

	for (i = 0; i < num_children; i ++)
	{
		if (XfeIsButton(children[i]))
        {
			HG10291
			updateButton(children[i]);
        }
    }
	
	D(	printf ("Leaving XFE_Toolbar::update()\n");)
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_Toolbar, updateToolbarAppearance)(XFE_NotificationCenter */*obj*/, 
									   void */*clientData*/, 
									   void */*callData*/)
{
	updateAppearance();
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Toolbar::updateAppearance() 
{
	unsigned char	button_layout;

	button_layout = XFE_Toolbar::styleToLayout(fe_globalPrefs.toolbar_style);

	XtVaSetValues(m_toolBar,XmNbuttonLayout,button_layout,NULL);

	if (button_layout == XmBUTTON_LABEL_ON_BOTTOM)
	{
		m_logo->setSize(XFE_ANIMATION_LARGE);
	}
	else
	{
		m_logo->setSize(XFE_ANIMATION_SMALL);
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Toolbar::setToolbarSpec(ToolbarSpec *spec)
{
	XP_ASSERT( spec != NULL );
	XP_ASSERT( m_spec == NULL );

	m_spec = spec;

    m_security = NULL;

	createItems();
}
//////////////////////////////////////////////////////////////////////////
XFE_Frame *
XFE_Toolbar::getFrame()
{
	return m_parentFrame;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Toolbar::addButton(const char */*name*/,
					   EChromeTag /*tag*/,
					   CommandType /*command*/,
					   fe_icon */*icon*/)
{
}
//////////////////////////////////////////////////////////////////////////
Widget
XFE_Toolbar::findButton(const char *name,
						EChromeTag /*tag*/)
{
	WidgetList	children;
	Cardinal	num_children;
	Cardinal	i;
	CommandType cmd = Command::intern((char *) name);
	
	XfeChildrenGet(m_toolBar,&children,&num_children);

	for (i = 0; i < num_children; i ++)
	{
		if (XfeIsButton(children[i]) && !XfeIsPrivateComponent(children[i]))
		{
			XFE_Button * button = 
				(XFE_Button *) XfeInstancePointer(children[i]);

			// First try to match command
			if (button->getCmd() == cmd)
			{
				return children[i];
			}
			// Then, try to match name
			else if (XP_STRCMP(button->getName(),name) == 0)
			{
				return children[i];
			}
		}
	}

	return NULL;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Toolbar::hideButton(const char *name,
						EChromeTag tag)
{
	Widget kid = findButton(name, tag);

	if (kid)
		XtUnmanageChild(kid);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Toolbar::showButton(const char *name,
						EChromeTag tag)
{
	Widget kid = findButton(name, tag);

	if (kid)
		XtManageChild(kid);
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_Toolbar, doCommand)
	(XFE_NotificationCenter *	/* obj */,
	 void *						/* clientData */,
	 void *						callData)
{
	XFE_DoCommandArgs * cmdArgs = (XFE_DoCommandArgs *) callData;

	if (m_parentFrame->handlesCommand(cmdArgs->cmd,
									  cmdArgs->callData,
									  cmdArgs->info)
		&& m_parentFrame->isCommandEnabled(cmdArgs->cmd,
										   cmdArgs->callData,
										   cmdArgs->info))
	{
		xfe_ExecuteCommand(m_parentFrame, cmdArgs->cmd, cmdArgs->callData,
						   cmdArgs->info );
		
		m_parentFrame->notifyInterested(Command::commandDispatchedCallback, 
										callData);
	  }
}
//////////////////////////////////////////////////////////////////////////
Widget
XFE_Toolbar::getToolBar()
{
    return m_toolBar;
}
//////////////////////////////////////////////////////////////////////////
Widget
XFE_Toolbar::getToolBarForm()
{
	XP_ASSERT( XfeIsAlive(m_toolBar) );

    return XtParent(m_toolBar);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Toolbar::createMain(Widget parent)
{
	XP_ASSERT( XfeIsAlive(parent) );

	// Create the base widget - a tool item
	m_widget = XtVaCreateWidget(TOOL_BAR_ITEM_NAME,
								xfeToolItemWidgetClass,
								parent,
								XmNuserData,			this,
								NULL);

	// Create the tool bar
	m_toolBar = 
		XtVaCreateManagedWidget(TOOL_BAR_NAME,
								xfeToolBarWidgetClass,
								m_widget,
								XmNorientation,			XmHORIZONTAL,
								XmNusePreferredHeight,	True,
								XmNusePreferredWidth,	False,
								NULL);

	// Create the logo
	m_logo = new XFE_Logo(m_parentFrame,m_widget,LOGO_NAME);

	m_logo->show();

    // Update the appearance for the first time
    updateAppearance();

	// Configure the logo for the first time
	configureLogo();

	installDestroyHandler();
}
//////////////////////////////////////////////////////////////////////////
Widget
XFE_Toolbar::createItem(ToolbarSpec *	spec)
{
	XP_ASSERT( spec != NULL );
	
	switch(spec->tag)
	{
	case PUSHBUTTON:

		return createPush(spec);

		break;
		
	case DYNA_CASCADEBUTTON:

		return createDynamicCascade(spec);

		break;
		
	case CASCADEBUTTON:

		return createCascade(spec);

		break;
		
	case SEPARATOR:

		return createSeparator(spec);

		break;
		
	default:
		
		XP_ASSERT(0);
	}

	return NULL;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Toolbar::createItems()
{
	ToolbarSpec *	cur_spec;

	XP_ASSERT( XfeIsAlive(m_toolBar) );

	if (!m_spec)
	{
		return;
	}

	cur_spec = m_spec;

	// Create toolbar buttons
	while (cur_spec->toolbarButtonName)
    {
		// Create the item only if it is not disabled through resources.
		if (XfeChildIsEnabled(m_toolBar,
							  (String) cur_spec->toolbarButtonName,
							  "UserCommand",
							  True))
		{
			createItem(cur_spec);
		}

		cur_spec ++;
    }

	// Create user buttons
	user_createItems();
}
//////////////////////////////////////////////////////////////////////////
Widget
XFE_Toolbar::createPush(ToolbarSpec * spec)
{
	XP_ASSERT( spec != NULL );
	XP_ASSERT( spec->tag == PUSHBUTTON );
	XP_ASSERT( XfeIsAlive(m_toolBar) );

	// Create a XFE_Button.
	XFE_Button * button = new XFE_Button(m_parentFrame,
										 m_toolBar,
										 spec->toolbarButtonName, 
										 spec->iconGroup,
										 spec->iconGroup2,
										 spec->iconGroup3,
										 spec->iconGroup4);
	// 
	button->setToplevel(m_parentFrame);

    Widget result = button->getBaseWidget();

	// Show the new push
	button->show();
		
	// [button] --DoCommandCallback--> [toolbar]
	button->registerInterest(XFE_Button::doCommandCallback,
							 this,
							 (XFE_FunctionNotification)doCommand_cb);
	
    HG01922
	
    return result;
}
//////////////////////////////////////////////////////////////////////////
Widget
XFE_Toolbar::createDynamicCascade(ToolbarSpec * spec)
{
	XP_ASSERT( spec != NULL );
	XP_ASSERT( spec->tag == DYNA_CASCADEBUTTON );
	XP_ASSERT( XfeIsAlive(m_toolBar) );

	// Create a XFE_Button. (with popups)
	XFE_Button * button = new XFE_Button(m_parentFrame,
										 m_toolBar,
										 spec->toolbarButtonName, 
										 spec->generateProc,
										 spec->generateArg,
										 spec->iconGroup);
	
	// Show the new cascade
	button->show();
		
	// [button] --DoCommandCallback--> [toolbar]
	button->registerInterest(XFE_Button::doCommandCallback,
							 this,
							 (XFE_FunctionNotification)doCommand_cb);

	// Set the popup delay
	button->setPopupDelay(verifyPopupDelay(spec->popupDelay));
	
    return button->getBaseWidget();
}
//////////////////////////////////////////////////////////////////////////
Widget
XFE_Toolbar::createCascade(ToolbarSpec * spec)
{
	XP_ASSERT( spec != NULL );
	XP_ASSERT( spec->tag == CASCADEBUTTON );
	XP_ASSERT( XfeIsAlive(m_toolBar) );

	// Create a XFE_Button. (with popups)
	XFE_Button * button = new XFE_Button(m_parentFrame,
										 m_toolBar,
										 spec->toolbarButtonName, 
										 spec->submenu,
										 spec->iconGroup);
	
	// Show the new cascade
	button->show();
		
	// [button] --DoCommandCallback--> [toolbar]
	button->registerInterest(XFE_Button::doCommandCallback,
							 this,
							 (XFE_FunctionNotification)doCommand_cb);

	// Set the popup delay
	button->setPopupDelay(verifyPopupDelay(spec->popupDelay));
	
    return button->getBaseWidget();
}
//////////////////////////////////////////////////////////////////////////
Widget
XFE_Toolbar::createSeparator(ToolbarSpec * spec)
{
	XP_ASSERT( spec != NULL );
	XP_ASSERT( spec->tag == SEPARATOR );
	XP_ASSERT( XfeIsAlive(m_toolBar) );

	Widget separator = XmCreateSeparator(m_toolBar,
										 (char *) spec->toolbarButtonName,
										 NULL,0);

	XtManageChild(separator);
	
	return separator;
}
//////////////////////////////////////////////////////////////////////////
int
XFE_Toolbar::verifyPopupDelay(int delay)
{
	int result = delay;

 	if (result < MIN_DELAY)
 	{
		result = MIN_DELAY;
 	}
 	else if (result > MAX_DELAY)
 	{
 		result = MAX_DELAY;
 	}

	return result;
}
//////////////////////////////////////////////////////////////////////////
/* static */ unsigned char
XFE_Toolbar::styleToLayout(int32 style)
{
	unsigned char button_layout = XmBUTTON_LABEL_ON_BOTTOM;

	switch(style)
	{
	case BROWSER_TOOLBAR_ICONS_AND_TEXT:

		button_layout = XmBUTTON_LABEL_ON_BOTTOM;

		break;

	case BROWSER_TOOLBAR_ICONS_ONLY:

		button_layout = XmBUTTON_PIXMAP_ONLY;

		break;

	case BROWSER_TOOLBAR_TEXT_ONLY:

		button_layout = XmBUTTON_LABEL_ONLY;

		break;

	default:
		
		XP_ASSERT( 0 );

		break;
	}
	
	return button_layout;
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// User commands support
//
//////////////////////////////////////////////////////////////////////////
void
XFE_Toolbar::user_createItems()
{
	XP_ASSERT( XfeIsAlive(m_toolBar) );

	Cardinal	i = 1;
	XP_Bool		done = False;

	// Iterate
	while(!done)
	{
		char userCommandName[32];

		// Commands are name userCommand1, userCommand2, userCommand3, etc
		XP_SPRINTF(userCommandName,"userCommand%d",i);

		// Attempt to get sub resources for the next user command
		XtGetSubresources(m_toolBar,
						  (XtPointer) this,
						  userCommandName,
						  "UserCommand",
						  m_user_commandResources,
						  XtNumber(m_user_commandResources),
						  NULL,
						  0);

		// If no sub resource are found, then we are done adding commands
		done = (!m_user_commandName || 
				(XP_STRCMP(m_user_commandName,"Error") == 0));

		// Verify that the command is valid
		if (!done)
		{
			// Separator
			if ((XP_STRNCASECMP(m_user_commandName,"sep",3) == 0) ||
				(m_user_commandName[0] == '-') )
			{
				user_addItem("--",						// widgetName
							 NULL,						// cmd
							 SEPARATOR,					// tag
							 NULL,						// data
							 False,						// needToFreeData
							 NULL,						// ig
							 NULL,						// subMenuSpec
							 False,						// needToFreeSpec
							 NULL,						// generateProc
							 NULL,						// generateArg
							 XFE_TOOLBAR_DELAY_SHORT);	// popupDelay
			}
			else 
			{
				MenuSpec *			subMenuSpec = NULL;
				dynamenuCreateProc	generateProc = NULL;
				void *				generateArg = NULL;
				CommandType			cmd;

				// Verify the sub menu spec
				subMenuSpec = user_verifySubMenu(m_user_commandSubMenu);

				// Verify the command
				cmd = user_verifyCommand(m_user_commandName,
										 generateProc,
										 generateArg);
				
				// Add the command if needed
				if (cmd || subMenuSpec)
				{
					XP_Bool				needToFreeData = False;
					XP_Bool				needToFreeSpec = (subMenuSpec != NULL);
					void *				data = NULL;
					IconGroup *			ig = NULL;
					EChromeTag			tag;
					EToolbarPopupDelay	popupDelay = XFE_TOOLBAR_DELAY_SHORT;
					
					// Adjust the popup delay if the command is valid
					if (cmd)
					{
						popupDelay = XFE_TOOLBAR_DELAY_LONG;
					}
					
					// Verify the command data if needed
					if (cmd)
					{
						data = user_verifyCommandData(cmd,
													  m_user_commandData,
													  needToFreeData);
					}
					
					// Verify the icon group
					ig = user_verifyCommandIcon(cmd,m_user_commandIcon);

					// Determine what chrome tag to use
					if (subMenuSpec != NULL)
					{
						tag = CASCADEBUTTON;
					}
					else if (generateProc)
					{
						tag = DYNA_CASCADEBUTTON;
					}
					else
					{
						tag = PUSHBUTTON;
					}
					
#ifdef DEBUG_ramiro
					printf("Adding UserCommand(%d,%s,%s,%s,submenu = %s)\n",
						   i,
						   cmd,
						   data ? m_user_commandData : "NULL",
						   m_user_commandIcon ? m_user_commandIcon : cmd,
						   m_user_commandSubMenu ? m_user_commandSubMenu : "NULL");
#endif
					
					// Add the command
					user_addItem(userCommandName,		// widgetName
								 cmd,					// cmd
								 tag,					// tag
								 data,					// data
								 needToFreeData,		// needToFreeData
								 ig,					// ig
								 subMenuSpec,			// subMenuSpec
								 needToFreeSpec,		// needToFreeSpec
								 generateProc,			// generateProc
								 generateArg,			// generateArg
								 popupDelay);			// popupDelay

				} // if (cmd || submenu)
			} // else
		} // if (!done)
		
		i++;
		
	} // while(!done)
}
//////////////////////////////////////////////////////////////////////////
CommandType
XFE_Toolbar::user_verifyCommand(String					commandName,
								dynamenuCreateProc &	generateProcOut,
								void * &				generateArgOut)
{
	generateProcOut = NULL;
	generateArgOut = NULL;

	// Make sure a command name string is given
	if (commandName == NULL)
	{
		return NULL;
	}

	// Try to find the command
	CommandType cmd = Command::intern(commandName);

	// Replace, swap or massage commands that need extra care
	if (cmd == xfeCmdOpenUrl)
	{
		cmd = xfeCmdOpenTargetUrl;
	}

	// Determine if the parent frame handles the command
	if (!m_parentFrame->handlesCommand(cmd))
	{
		return NULL;
	}

	// Assign the generation proc and arg for commands that have them
	if (cmd == xfeCmdBack)
	{
		generateProcOut = &XFE_BackForwardMenu::generate;
		generateArgOut = (void *) False;
	}
	else if (cmd == xfeCmdForward)
	{
		generateProcOut = &XFE_BackForwardMenu::generate;
		generateArgOut = (void *) True;
	}

	return cmd;
}
//////////////////////////////////////////////////////////////////////////
void *		
XFE_Toolbar::user_verifyCommandData(CommandType 	cmd,
									String			commandData,
									XP_Bool &		needToFreeOut)
{
	// Initialize the return value
	needToFreeOut = False;

	// Make sure a command data string is given
	if (commandData == NULL)
	{
		return False;
	}
	
	// xfeCmdOpenTargetUrl
	if (cmd == xfeCmdOpenTargetUrl)
	{
		LO_AnchorData * anchorData = 
			(LO_AnchorData *) XtMalloc(sizeof(LO_AnchorData));
		
		anchorData->anchor = (PA_Block) commandData;
		anchorData->target = (PA_Block) NULL;

		// Assign the return values
		needToFreeOut = True;

		return (void *) anchorData;
	}
	
	return NULL;
}
//////////////////////////////////////////////////////////////////////////
IconGroup *
XFE_Toolbar::user_verifyCommandIcon(CommandType cmd,String commandIcon)
{
	String iconName = commandIcon;

	// Use the command if no icon name is given
	if (iconName == NULL)
	{
		iconName = cmd;
	}

	if (iconName == NULL)
	{
		return &TB_Home_group;
	}

	return IconGroup_findGroupForName(iconName);
}
//////////////////////////////////////////////////////////////////////////
MenuSpec *	
XFE_Toolbar::user_verifySubMenu(String commandSubMenu)
{
	if (commandSubMenu == NULL)
	{
		return NULL;
	}

	// Make a local copy of the submenu string for strtok
	String		sub_menu_copy = (String) XtNewString(commandSubMenu);
	String		strptr;
	String		tokptr;
	Cardinal	numItems = 0;

	// First, count the number of valid submenu commands
	strptr = sub_menu_copy;

	// Tokenize
	while( (tokptr = XP_STRTOK(strptr,",")) != NULL )
	{
		// Check for separators
		if ( (XP_STRNCASECMP(tokptr,"sep",3) == 0) || 
			 (tokptr[0] == '-') )
		{
			numItems++;
		}
		else
		{
			CommandType subcmd = Command::intern(tokptr);
			
			// Check whether the parent frame handles the command
			if (m_parentFrame->handlesCommand(subcmd))
			{
				numItems++;
			}
		}

		// Advance to next token
		strptr = NULL;
	} // while

	// Make sure at least 1 valid command is given
	if (numItems == 0)
	{
		XtFree(sub_menu_copy);

		return NULL;
	}

	// Allocate enough space for n MenuSpecs plus 1 NULL terminator
	MenuSpec *	spec = (MenuSpec *) XtMalloc(sizeof(MenuSpec) * (numItems+1));
	Cardinal	i = 0;

	// Replace the local copy since strtok screws it up
	XP_STRCPY(sub_menu_copy,commandSubMenu);
	
	// Add the commands
	strptr = sub_menu_copy;
	
	// Tokenize
	while( (tokptr = XP_STRTOK(strptr,",")) != NULL )
	{
		// Check for separators
		if ( (XP_STRNCASECMP(tokptr,"sep",3) == 0) ||
			 (tokptr[0] == '-') )
		{
			static String static_sep_name = "--";

			spec[i].menuItemName	= static_sep_name;
			spec[i].tag				= SEPARATOR;

			i++;
		}
		else
		{
			CommandType subcmd = Command::intern(tokptr);
			
			// Check whether the parent frame handles the command
			if (m_parentFrame->handlesCommand(subcmd))
			{
				spec[i].menuItemName	= subcmd;
				spec[i].tag				= PUSHBUTTON;
				
				i++;
			}
		}

		// Advance to next token
		strptr = NULL;
	}	

	// Assign the terminating spec
	spec[i].menuItemName = NULL;

	// Free the local copy
	XtFree(sub_menu_copy);

	return spec;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Toolbar::user_addItem(String				widgetName,
						  CommandType			cmd,
						  EChromeTag			tag,
						  void *				data,
						  XP_Bool				needToFreeData,
						  IconGroup *			ig,
						  MenuSpec *			subMenuSpec,
						  XP_Bool				needToFreeSpec,
						  dynamenuCreateProc	generateProc,
						  void *				generateArg,
						  EToolbarPopupDelay	popupDelay)
{
	XP_ASSERT( XfeIsAlive(m_toolBar) );
	XP_ASSERT( widgetName != NULL );

	XFE_Button *	button = NULL;
	Widget			item = NULL;
	ToolbarSpec *	spec = (ToolbarSpec *)XtMalloc(sizeof(ToolbarSpec));

	// Initialize the toolbar spec
	spec->toolbarButtonName = (char *) XtNewString(widgetName);
	spec->tag				= tag;
	spec->iconGroup			= ig;
	spec->iconGroup2		= NULL;
	spec->iconGroup3		= NULL;
	spec->iconGroup4		= NULL;
	spec->submenu			= subMenuSpec;
	spec->generateProc		= generateProc;
	spec->generateArg		= generateArg;
	spec->popupDelay		= popupDelay;
			
	// Create the new user command item
	item = createItem(spec);

	// Configure items that are not separators
	if (tag != SEPARATOR)
	{
		// Acces the instance pointer
		button = (XFE_Button *) XfeInstancePointer(item);
		
		// Install the new cmd on the button if needed
		if (cmd)
		{
			button->setCmd(cmd);
			
			// Add the command data if needed
			if (data != NULL)
			{
				button->setCallData(data);
				
				// Add destroy callback to free data if needed
				if (needToFreeData)
				{
					XtAddCallback(item,
								  XmNdestroyCallback,
								  user_freeCallDataCB,
								  (XtPointer) data);
				}
			}
		}

		// Add destroy callback to free menu spec if needed
		if (needToFreeSpec)
		{
			XtAddCallback(item,
						  XmNdestroyCallback,
						  user_freeCallDataCB,
						  (XtPointer) subMenuSpec);
		}
	}
	
	// Add destroy callback to free all the ToolbarSpec we allocated
	XtAddCallback(item,
				  XmNdestroyCallback,
				  user_commandDestroyCB,
				  (XtPointer) spec);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void 
XFE_Toolbar::user_commandDestroyCB(Widget		/* w */,
								   XtPointer	clientData,
								   XtPointer	/* callData */)
{
	ToolbarSpec * spec = (ToolbarSpec *) clientData;

	XP_ASSERT( spec != NULL );

	XP_ASSERT( spec->toolbarButtonName != NULL );

	XtFree((char *) spec->toolbarButtonName);

	XtFree((char *) spec);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void 
XFE_Toolbar::user_freeCallDataCB(Widget			/* w */,
								 XtPointer		clientData,
								 XtPointer		/* callData */)
{
	if (clientData)
	{
		XtFree((char *) clientData);
	}
}
//////////////////////////////////////////////////////////////////////////
