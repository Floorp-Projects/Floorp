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
   Menu.cpp -- implementation file for Menu
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
 */



#if DEBUG_toshok_
#define D(x) x
#else
#define D(x)
#endif

#include "xp_core.h"
#include "prefapi.h"

#include "Menu.h"
#include "Frame.h"
#include "View.h"
#include "Command.h"
#include "RadioGroup.h"
#include <Xm/XmP.h>
#include <Xm/RowColumn.h>
#include <Xm/CascadeBG.h>
#include <Xm/PushBG.h>
#include <Xm/LabelG.h>
#include <Xm/ToggleB.h>
#include <Xm/SeparatoG.h>

#include <Xfe/Xfe.h>
#include <Xfe/BmButton.h>
#include <Xfe/BmCascade.h>


// Check the class of a menu item
#define IS_LABEL(w)		(XmIsLabel(w) || XmIsLabelGadget(w))
#define IS_SEP(w)		(XmIsSeparator(w) || XmIsSeparatorGadget(w))
#define IS_CASCADE(w)	(XmIsCascadeButton(w) || XmIsCascadeButtonGadget(w))
#define IS_PUSH(w)		(XmIsPushButton(w) || XmIsPushButtonGadget(w))
#define IS_TOGGLE(w)	(XmIsToggleButton(w) || XmIsToggleButtonGadget(w))

// Possible menu item classes
#define ITEM_CASCADE_CLASS			xmCascadeButtonWidgetClass
#define ITEM_FANCY_CASCADE_CLASS	xfeBmCascadeWidgetClass
#define ITEM_LABEL_CLASS			xmLabelGadgetClass
#define ITEM_PUSH_CLASS				xmPushButtonWidgetClass
#define ITEM_SEP_CLASS				xmSeparatorGadgetClass
#define ITEM_TOGGLE_CLASS			xmToggleButtonWidgetClass

XFE_Menu::XFE_Menu(XFE_Frame *parent_frame, MenuSpec *spec, Widget baseMenuWidget) 
  : XFE_Component(parent_frame)
{
D(	printf ("in XFE_Menu::XFE_Menu()\n");)

  m_parentFrame = parent_frame;

  m_spec = NULL;

  if (baseMenuWidget)
  {
	  setBaseWidget(baseMenuWidget);
  }

  if (spec)
  {
	  setMenuSpec(spec);
  }

  m_parentFrame->registerInterest(XFE_View::chromeNeedsUpdating,
				  this,
				  (XFE_FunctionNotification)update_cb);

  m_parentFrame->registerInterest(XFE_View::commandNeedsUpdating,
				  this,
				  (XFE_FunctionNotification)updateCommand_cb);

D(	printf ("leaving XFE_Menu::XFE_Menu()\n");)
}

XFE_Menu::~XFE_Menu()
{
  m_parentFrame->unregisterInterest(XFE_View::chromeNeedsUpdating,
				    this,
				    (XFE_FunctionNotification)update_cb);

  m_parentFrame->unregisterInterest(XFE_View::commandNeedsUpdating,
				    this,
				    (XFE_FunctionNotification)updateCommand_cb);
}

XFE_CALLBACK_DEFN(XFE_Menu, update)(XFE_NotificationCenter */*obj*/, 
				       void */*clientData*/, 
				       void */*callData*/)
{
  update();
}

void
XFE_Menu::update()
{
D(	printf ("in XFE_Menu::update()\n");)
  int num_children, i;
  Widget *children;
  
  XtVaGetValues(m_widget,
		XmNchildren, &children,
		XmNnumChildren, &num_children,
		NULL);
  
  for (i = 0; i < num_children; i ++)
    updateMenuItem(children[i]);    

D(	printf ("leaving XFE_Menu::update()\n");)
}

void
XFE_Menu::updateSubMenu(Widget submenu)
{
D(	printf ("in XFE_Menu::updateSubMenu()\n");)
  int num_children, i;
  Widget *children;
  
  XtVaGetValues(submenu,
		XmNchildren, &children,
		XmNnumChildren, &num_children,
		NULL);
  
  for (i = 0; i < num_children; i ++)
    updateMenuItem(children[i]);

D(	printf ("leaving XFE_Menu::updateSubMenu()\n");)
}

static char*
menuItemGetCmdName(Widget w)
{
	XtPointer userData;

	XtVaGetValues(w,
				  XmNuserData, &userData,
				  NULL);
	MenuSpec* spec = (MenuSpec*)userData;
	
	return spec ? spec->getCmdName() : "";
}

void
XFE_Menu::updateMenuItem(Widget menuItem)
{
  char *command_string;
  XtPointer userData;
  CommandType cmd;

  XtVaGetValues(menuItem,
		XmNuserData, &userData,
		NULL);

  MenuSpec* spec = (MenuSpec*)userData;
  char**    params = (spec != NULL)? spec->cmd_args: (char**)NULL;
  XFE_CommandInfo info(XFE_COMMAND_BUTTON_ACTIVATE,
					   menuItem, NULL, params);


  if (spec)
	  {
		  info.params = spec->cmd_args;

		  cmd = spec->getCmdName();
		  
		  D(	printf ("in XFE_Menu::updateMenuItem(%s)\n", Command::getString(cmd));)
			  
			  command_string = m_parentFrame->commandToString(cmd,
															  spec->callData,
															  &info);
			  
			  if ( command_string )
				  {
					  XmString str;
					  
					  D(	printf ("    command string is %s\n", command_string);)
						  
						  str = XmStringCreate(command_string, XmFONTLIST_DEFAULT_TAG);
						  
						  XtVaSetValues(menuItem, XmNlabelString, str, NULL);
						  
						  XmStringFree(str);
				  }
	  }
  else
	  {
		  cmd = Command::intern(XtName(menuItem));
	  }

  /* if it's a cascade button, sensitize it's submenu. */
  if (IS_CASCADE(menuItem))
	  {
		  Widget submenu = NULL;
		  
		  XtVaGetValues(menuItem,
						XmNsubMenuId, &submenu,
						NULL);
		  
		  if (submenu)
			  {
	  			  if (XfeIsViewable(submenu)) 
					  updateSubMenu(submenu);
			  }
	  }
  else if (XmIsToggleButton(menuItem) )
	  {
		  Boolean turnOn;

		  if (spec)
			  {
				  turnOn = m_parentFrame->isCommandSelected(cmd, spec->callData,
															&info);
			  }
		  else
			  {
				  turnOn = m_parentFrame->isCommandSelected(cmd);
			  }
		  
		  XmToggleButtonSetState(menuItem, turnOn, False);
	  }
  else if (XmIsToggleButtonGadget(menuItem))
	  {
		  Boolean turnOn;

		  if (spec)
			  {
				  turnOn = m_parentFrame->isCommandSelected(cmd, spec->callData,
															&info);
			  }
		  else
			  {
				  turnOn = m_parentFrame->isCommandSelected(cmd);
			  }

		  XmToggleButtonGadgetSetState(menuItem, turnOn, False);
	  }
  
  /* we don't sensitize labels or cascadebuttons. */
  if (IS_PUSH(menuItem) || IS_TOGGLE(menuItem))
	  {
		  Boolean isSensitive;

		  if (spec)
			  {
				  isSensitive = (m_parentFrame->handlesCommand(cmd, spec->callData, &info)
								 && m_parentFrame->isCommandEnabled(cmd, spec->callData, &info));
			  }
		  else
			  {
				  isSensitive = (m_parentFrame->handlesCommand(cmd)
								 && m_parentFrame->isCommandEnabled(cmd,NULL,&info));
			  }

		  XtVaSetValues(menuItem,
						XmNsensitive, isSensitive,
						NULL);
	  }
  
D(	printf ("leaving XFE_Menu::updateMenuItem()\n");)
}

XFE_CALLBACK_DEFN(XFE_Menu, updateCommand)(XFE_NotificationCenter */*obj*/, 
					      void */*clientData*/, 
					      void *callData)
{
D(	printf ("in XFE_Menu::updateCommand()\n");)
	int num_children, i;
	Widget *children;
	CommandType command = (CommandType)callData;
	
	XtVaGetValues(m_widget,
				  XmNchildren, &children,
				  XmNnumChildren, &num_children,
				  NULL);
	
	for (i = 0; i < num_children; i ++)
		{
			if (IS_CASCADE(children[i]))
				{
					Widget submenu = NULL;
					
					XtVaGetValues(children[i],
								  XmNsubMenuId, &submenu,
								  NULL);
					
					if (submenu)
						{
							// Instead of checking if the pulldown menu shell is popped or not,
							// we should be checking if the submenu rowcolumn is mapped or not.
							
							// The reason is because all of the pulldown submenus sharing the
							// same popup shell; therefore, checking if the shell is managed
							// or not does not work. That's why our pulldown menu are generated
							// everytinge EnableClicking happens. - Thanks ramiro giving us
							// this wonderful API to test on our submenu.
							
							// This fix will prevent unnecessary submenu being created at the wrong
							// time.
							if (XfeIsViewable(submenu)) 
								updateCommandInSubMenu(command, submenu);
						}
				}
		}

D(	printf ("leaving XFE_Menu::updateCommand()\n");)
}

void
XFE_Menu::updateCommandInSubMenu(CommandType command,
				    Widget submenu)
{
D(	printf ("in XFE_Menu::updateCommandInSubMenu()\n");)
  int num_children, i;
  Widget *children;
  
  XtVaGetValues(submenu,
		XmNchildren, &children,
		XmNnumChildren, &num_children,
		NULL);
  
  for (i = 0; i < num_children; i ++)
    updateCommandInMenuItem(command, children[i]);

D(	printf ("leaving XFE_Menu::updateCommandInSubMenu()\n");)
}

void
XFE_Menu::updateCommandInMenuItem(CommandType command, Widget menuItem)
{
  XtPointer userData;

D(	printf ("in XFE_Menu::updateCommandInMenuItem()\n");)
  
  XtVaGetValues(menuItem,
		XmNuserData, &userData,
		NULL);

  MenuSpec* spec = (MenuSpec*)userData;
  CommandType cmd;

  XFE_CommandInfo info(XFE_COMMAND_BUTTON_ACTIVATE,
					   menuItem, NULL);


  if (spec)
	  {
		  info.params = spec->cmd_args;

		  cmd = spec->getCmdName();
	  }
  else
	  {
		  cmd = Command::intern(XtName(menuItem));
	  }

  if (command == cmd) 
	  {
		  Boolean isSensitive;

		  if (spec)
			  {
				  isSensitive = m_parentFrame->isCommandEnabled(cmd, spec->callData, &info);
			  }
		  else
			  {
				  isSensitive = m_parentFrame->isCommandEnabled(cmd,NULL,&info);
			  }

		  XtVaSetValues(menuItem,
						XmNsensitive, isSensitive,
						0);
	  }

  /* if it's a cascade button, also sensitize it's submenu. */
  if (IS_CASCADE(menuItem))
	  {
		  Widget submenu = NULL;
		  
		  XtVaGetValues(menuItem,
						XmNsubMenuId, &submenu,
						NULL);
		  
		  if (submenu)
			  {
				  if (XfeIsViewable(submenu)) 
					  updateCommandInSubMenu(command, submenu);
			  }
	  }
D(	printf ("leaving XFE_Menu::updateCommandInMenuItem()\n");)
}

Widget
XFE_Menu::createPulldown(char *cascadeName, MenuSpec *, Widget parent_menu, 
						 XP_Bool is_fancy)
{
	Widget		cascade;
	Widget		pulldown;
	WidgetClass wc = is_fancy ? ITEM_FANCY_CASCADE_CLASS : ITEM_CASCADE_CLASS;

	// Create a pulldown pane (cascade + pulldown)
	XfeMenuCreatePulldownPane(parent_menu,
							  m_parentFrame->getBaseWidget(),
							  cascadeName,
							  "pulldown",
							  wc,
							  False,
							  NULL,
							  0,
							  &cascade,
							  &pulldown);
	
#if DELAYED_MENU_CREATION
	XtAddCallback(cascade, XmNcascadingCallback, delayed_create_pulldown, this);
#else
	if (spec)
		{
			MenuSpec *cur_spec;
			Widget items[150];
			int num_items = 0;

			cur_spec = spec;
			while (cur_spec->menuItemName)
				{
					items[num_items] = createMenuItem(cur_spec, cascade, pulldown);

					if (items[num_items] != NULL) // we return NULL for DYNA_MENUITEMS
						num_items++;
					
					cur_spec ++;
				}

			if (num_items)
				XtManageChildren(items, num_items);
		}
#endif
	return cascade;
}

Widget
XFE_Menu::createMenuItem(MenuSpec *spec, Widget parent_cascade, Widget parent)
{
	Widget result = 0;
	
	switch(spec->tag)
		{
		case LABEL:
			{
				result = XtCreateWidget(spec->menuItemName,
										ITEM_LABEL_CLASS,
										parent,
										NULL, 0);
				break;
			};
		case SEPARATOR:
			{
				result = XtCreateWidget(spec->menuItemName,
										ITEM_SEP_CLASS,
										parent,
										NULL, 0);
				break;
			}
		case PUSHBUTTON:
			{
				result = XtCreateWidget(spec->menuItemName,
										ITEM_PUSH_CLASS,
										parent,
										NULL, 0);
				
				XtAddCallback(result, XmNarmCallback, pushb_arm_cb, this);
				XtAddCallback(result, XmNactivateCallback, pushb_activate_cb, this);
				XtAddCallback(result, XmNdisarmCallback, pushb_disarm_cb, this);
				
				break;
			}
		case CUSTOMBUTTON:
			{
				char* str;
				XmString xm_str;
				KeySym mnemonic;

				if ( !PREF_GetLabelAndMnemonic((char*) spec->callData, &str, 
						&xm_str, &mnemonic) ) break;

				if ( !strcmp(str, "-") ) {
					result = XtCreateWidget(spec->menuItemName,
											ITEM_SEP_CLASS,
											parent,
											NULL, 0);
				} else {
				
					result = XtCreateWidget(spec->menuItemName,
											ITEM_PUSH_CLASS,
											parent,
											NULL, 0);
				
					XtVaSetValues(result, XmNlabelString, xm_str, NULL);
					if ( mnemonic ) {
						XtVaSetValues(result, XmNmnemonic, mnemonic, NULL);
					}

					XtAddCallback(result, XmNarmCallback, pushb_arm_cb, this);
					XtAddCallback(result, XmNactivateCallback, pushb_activate_cb, this);
					XtAddCallback(result, XmNdisarmCallback, pushb_disarm_cb, this);
				}
				
				XmStringFree(xm_str);

				break;
			}
		case CASCADEBUTTON:
		case FANCY_CASCADEBUTTON:
			{
				result = createPulldown(spec->menuItemName,
										spec->submenu,
										parent,
										spec->tag == FANCY_CASCADEBUTTON);
				
#ifndef DELAYED_MENU_CREATION
				XtAddCallback(result, XmNcascadingCallback, cascade_update_cb, this);
#endif
				
				break;
			}
		case TOGGLEBUTTON:
			result = XtCreateWidget(spec->menuItemName,
									ITEM_TOGGLE_CLASS,
									parent,
									NULL, 0);
			
			if (spec->radioGroupName) {
				XtAddCallback(result, XmNarmCallback, radiob_arm_cb, this);
				XtAddCallback(result, XmNvalueChangedCallback, radiob_activate_cb, this);
				XtAddCallback(result, XmNdisarmCallback, radiob_disarm_cb, this);
				
				XFE_RadioGroup *rg;
				rg = XFE_RadioGroup::getByName(spec->radioGroupName, m_parentFrame);
				rg->addChild(result);
			} else {
				XtAddCallback(result, XmNarmCallback, toggleb_arm_cb, this);
				XtAddCallback(result, XmNvalueChangedCallback, toggleb_activate_cb,this);
				XtAddCallback(result, XmNdisarmCallback, toggleb_disarm_cb, this);
			}
			
			break;
		case DYNA_CASCADEBUTTON:
		case DYNA_FANCY_CASCADEBUTTON:
			XP_ASSERT(spec->submenu == NULL); // doesn't make sense if you have a submenu here...
			
			result = createPulldown(spec->menuItemName,
									spec->submenu,
									parent,
									spec->tag == DYNA_FANCY_CASCADEBUTTON);
			
			(*spec->generateProc)(result, spec->callData, m_parentFrame);
			break;
			
		case DYNA_MENUITEMS:
			XP_ASSERT(XmIsRowColumn(parent));
			
			(*spec->generateProc)(parent_cascade, spec->callData, m_parentFrame);
			break;
		default:
			XP_ASSERT(0);
			break;
		}
	
	if (result) {
		XtVaSetValues(result,
					  XmNuserData, spec,
					  NULL);
	}
	
	return result;
}

void
XFE_Menu::addMenuSpec(MenuSpec *spec)
{
  if (!m_spec)
    setMenuSpec(spec);
  else
    createWidgets(spec);
}

void
XFE_Menu::createWidgets()
{
  createWidgets(m_spec);
}

void
XFE_Menu::createWidgets(MenuSpec *spec)
{
	MenuSpec *cur_spec;
	Widget items[150];
	int num_items = 0;

D(	printf ("in XFE_Menu::createWidgets()\n");)
	if (!m_spec)
		return;

	cur_spec = spec;
	
	while (cur_spec->menuItemName)
		{
			items[num_items] = createMenuItem(cur_spec, NULL, m_widget);
			
			if (items[num_items] != NULL) // we return NULL for DYNA_MENUITEMS
				{
					if (!strcmp(cur_spec->menuItemName, xfeMenuHelp)
						&& IS_CASCADE(items[num_items]))
						XtVaSetValues(m_widget,
									  XmNmenuHelpWidget, items[num_items],
									  NULL);
					
					num_items++;
				}
			cur_spec++;
		}

	if (num_items)
		XtManageChildren(items, num_items);

D(	printf ("leaving XFE_Menu::createWidgets()\n");)
}

Widget
XFE_Menu::addMenuItem(char *itemName,
		      EChromeTag tag,
		      MenuSpec* submenuspec,
		      char* radioGroupName,
		      Boolean toggleSet,
		      void *calldata,
		      dynamenuCreateProc generateProc,
		      int position)
{
  // This code sucks now.
  // Using a temp spec doesn't work, because we try to reference
  // it from the menu item's user data.
  MenuSpec tmp_spec;
  Widget item;

  tmp_spec.menuItemName = itemName;
  tmp_spec.tag = tag;
  tmp_spec.submenu = submenuspec;
  tmp_spec.radioGroupName = radioGroupName;
  tmp_spec.toggleSet = toggleSet;
  tmp_spec.callData = calldata;
  tmp_spec.generateProc = generateProc;

  item = createMenuItem(&tmp_spec, NULL, m_widget);

  XtVaSetValues(item,
		XmNpositionIndex, position,
		NULL);

  return item;
}

void
XFE_Menu::removeMenuItem(char */*menuName*/,
			    EChromeTag /*tag*/)
{
}

Widget
XFE_Menu::findMenuItemInMenu(char *menuName,
			     EChromeTag tag,
			     Widget menu)
{
  int num_children;
  Widget *children;
  int i;

  XtVaGetValues(menu,
		XmNnumChildren, &num_children,
		XmNchildren, &children,
		NULL);

  for (i = 0; i < num_children; i ++)
    {
      Widget child = children[i];

      if (!strcmp(menuName, menuItemGetCmdName(child)))
	{
	  switch (tag)
	    {
	    case LABEL:
	      if (IS_LABEL(child))
		return child;
	      break;
	    case TOGGLEBUTTON:
	      if (IS_TOGGLE(child))
		return child;
	      break;
	    case CASCADEBUTTON:
	      if (IS_CASCADE(child))
		return child;
	      break;
	    case PUSHBUTTON:
	      if (IS_PUSH(child))
		return child;
	      break;
	    case SEPARATOR:
	      if (IS_SEP(child))
		return child;
	      break;
	    default:
	      XP_ASSERT(0);
	      break;
	    }
	}
      else
	{
	  if (IS_CASCADE(child))
	    {
	      Widget submenu = NULL;
	      Widget submenu_child;

	      XtVaGetValues(child,
			    XmNsubMenuId, &submenu,
			    NULL);

	      submenu_child = findMenuItemInMenu(menuName, tag, submenu);

	      if (submenu_child)
		return submenu_child;
	    }
	}
    }

  return 0;
}

Widget
XFE_Menu::findMenuItem(char *menuName,
		       EChromeTag tag)
{
  return findMenuItemInMenu(menuName, tag, m_widget);
}

void
XFE_Menu::hideMenuItem(char *menuName,
		       EChromeTag tag)
{
  Widget item = findMenuItem(menuName,
			     tag);

  if (item)
    XtUnmanageChild(item);
}

void
XFE_Menu::showMenuItem(char *menuName,
		       EChromeTag tag)
{
  Widget item = findMenuItem(menuName,
			     tag);

  if (item)
    XtManageChild(item);
}

void
XFE_Menu::setMenuSpec(MenuSpec *menu_spec)
{
D(	printf ("in XFE_Menu::setMenuSpec()\n");)
  XP_ASSERT(m_spec == NULL);

  m_spec = menu_spec;

  createWidgets();

D(	printf ("leaving XFE_Menu::setMenuSpec()\n");)
}

void
XFE_Menu::cascade_update_cb(Widget w,
			       XtPointer clientData,
			       XtPointer /*callData*/)
{
  Widget submenu = NULL;
  XFE_Menu *mb = (XFE_Menu*)clientData;

  XP_ASSERT(mb);

  XtVaGetValues(w,
		XmNsubMenuId, &submenu,
		NULL);

  mb->updateSubMenu(submenu);
}

static CommandType
get_cmd_type(Widget w)
{
	XtPointer   userData;
	MenuSpec*   spec;
	CommandType cmd;

	XtVaGetValues(w,
				  XmNuserData, &userData,
				  NULL);

	spec = (MenuSpec*)userData;
	cmd = spec->getCmdName();

	return cmd;
}

void
XFE_Menu::pushb_arm_cb(Widget w, XtPointer clientData, XtPointer)
{
  XFE_Menu*   obj = (XFE_Menu*)clientData;
  CommandType cmd = get_cmd_type(w);

  obj->m_parentFrame->notifyInterested(Command::commandArmedCallback,
									   (void*)cmd);
}

void
XFE_Menu::pushb_activate_cb(Widget w, XtPointer clientData, XtPointer callData)
{
  XFE_Menu*                   obj = (XFE_Menu*)clientData;
  XmPushButtonCallbackStruct* cd = (XmPushButtonCallbackStruct *)callData;

  XtPointer userData;
  XtVaGetValues(w,
		XmNuserData, &userData,
		NULL);

  MenuSpec*   spec = (MenuSpec*)userData;
  CommandType cmd = spec->getCmdName();
  
  XFE_CommandInfo e_info(XFE_COMMAND_BUTTON_ACTIVATE,
						 w,
						 cd->event,
						 spec->cmd_args, 0);
  
  xfe_ExecuteCommand(obj->m_parentFrame, cmd, spec->callData, &e_info);

  obj->m_parentFrame->notifyInterested(Command::commandDispatchedCallback,
									   (void*)cmd);
}

void
XFE_Menu::pushb_disarm_cb(Widget w, XtPointer clientData, XtPointer)
{
  XFE_Menu*   obj = (XFE_Menu*)clientData;
  CommandType cmd = get_cmd_type(w);

  obj->m_parentFrame->notifyInterested(Command::commandDisarmedCallback,
									   (void*)cmd);
}

void
XFE_Menu::toggleb_arm_cb(Widget w, XtPointer clientData, XtPointer)
{
  XFE_Menu*   obj = (XFE_Menu*)clientData;
  CommandType cmd = get_cmd_type(w);

  obj->m_parentFrame->notifyInterested(Command::commandArmedCallback,
									   (void*)cmd);
}

void
XFE_Menu::toggleb_activate_cb(Widget w,
							  XtPointer clientData, XtPointer callData)
{
  XmToggleButtonCallbackStruct* cbs = (XmToggleButtonCallbackStruct*)callData;
  XFE_Menu* obj = (XFE_Menu*)clientData;

  XtPointer userData;
  XtVaGetValues(w,
		XmNuserData, &userData,
		NULL);

  MenuSpec*   spec = (MenuSpec*)userData;
  CommandType cmd = spec->getCmdName();
  
  XFE_CommandInfo e_info(XFE_COMMAND_BUTTON_ACTIVATE,
						 w,
						 cbs->event,
						 spec->cmd_args, 0);
  
  xfe_ExecuteCommand(obj->m_parentFrame, cmd, spec->callData, &e_info);

  obj->m_parentFrame->notifyInterested(Command::commandDispatchedCallback,
									   (void*)cmd);
}

void
XFE_Menu::toggleb_disarm_cb(Widget w, XtPointer clientData, XtPointer)
{
  XFE_Menu*   obj = (XFE_Menu*)clientData;
  CommandType cmd = get_cmd_type(w);

  obj->notifyInterested(Command::commandDisarmedCallback, (void*)cmd);
}

void
XFE_Menu::radiob_arm_cb(Widget w, XtPointer clientData, XtPointer)
{
  XFE_Menu*   obj = (XFE_Menu*)clientData;
  CommandType cmd = get_cmd_type(w);

  obj->m_parentFrame->notifyInterested(Command::commandArmedCallback,
									   (void*)cmd);
}

void
XFE_Menu::radiob_activate_cb(Widget w,
							  XtPointer clientData, XtPointer callData)
{
  XmToggleButtonCallbackStruct* cbs = (XmToggleButtonCallbackStruct*)callData;
  XFE_Menu* obj = (XFE_Menu*)clientData;

  XtPointer userData;
  XtVaGetValues(w,
		XmNuserData, &userData,
		NULL);

  MenuSpec*   spec = (MenuSpec*)userData;

  //
  //    Only do stuff, if we are being turned on. This is not right,
  //    but we have code which depends on it.
  //
  if (!cbs->set)
	return;

  CommandType cmd = spec->getCmdName();
  
  XFE_CommandInfo e_info(XFE_COMMAND_BUTTON_ACTIVATE,
						 w,
						 cbs->event,
						 spec->cmd_args, 0);
  
  xfe_ExecuteCommand(obj->m_parentFrame, cmd, spec->callData, &e_info);

  obj->m_parentFrame->notifyInterested(Command::commandDispatchedCallback,
									   (void*)cmd);
}

void
XFE_Menu::radiob_disarm_cb(Widget w, XtPointer clientData, XtPointer)
{
  XFE_Menu*   obj = (XFE_Menu*)clientData;
  CommandType cmd = get_cmd_type(w);

  obj->notifyInterested(Command::commandDisarmedCallback, (void*)cmd);
}

#ifdef DELAYED_MENU_CREATION
void
XFE_Menu::delayedCreatePulldown(Widget cascade, Widget pulldown, MenuSpec *spec)
{
	if (spec)
		{
			MenuSpec *cur_spec;
			Widget items[150];
			int num_items = 0;
			
			cur_spec = spec;
			while (cur_spec->menuItemName)
				{
					items[num_items] = createMenuItem(cur_spec, cascade, pulldown);
					
					if ((items[num_items] != NULL) && // we return NULL for DYNA_MENUITEMS
						(! cur_spec->do_not_manage))  // we set this field for non-Communicator menu items
						num_items++;
					
					cur_spec ++;
				}
			
			if (num_items)
				XtManageChildren(items, num_items);
		}
}

void
XFE_Menu::delayed_create_pulldown(Widget w, XtPointer cd, XtPointer)
{
	Widget submenu;
	MenuSpec *spec;
	XFE_Menu *mb = (XFE_Menu*)cd;

	XP_ASSERT(mb);
	
	XtVaGetValues(w,
				  XmNsubMenuId, &submenu,
				  XmNuserData, &spec,
				  NULL);

D(	printf ("Creating pulldown menu\n");)

	mb->delayedCreatePulldown(w, submenu, spec->submenu);

	XtRemoveCallback(w, XmNcascadingCallback, delayed_create_pulldown, mb);

	XtAddCallback(w, XmNcascadingCallback, cascade_update_cb, mb);

	/* this is needed because the above delayedCreatePulldown possibly attached
	   dynamic menus to the pulldown, and we need to trigger them to create their
	   menu items.  It also sensitizes the menu because of the above line.
	   */
	XtCallCallbacks(w, XmNcascadingCallback, NULL);
}

#endif
