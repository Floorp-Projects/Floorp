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
/* */
/*
 *----------------------------------------------------------------------------
 *
 *  EditorToolbar.cpp --- Toolbar for Editor and HTML Mail Compose.
 *
 *  Created: David Williams <djw@netscape.com>, Feb-7-1997
 *  RCSID: "$Id: EditorToolbar.cpp,v 3.6 1998/09/29 07:11:22 mcafee%netscape.com Exp $"
 *
 *----------------------------------------------------------------------------
 */


#include "Frame.h"
#include "View.h"
#include "EditorView.h"
#include "Logo.h"
#include "Button.h" // for fe_buttonSetPixmaps()

#include <DtWidgets/ComboBoxP.h>
#include <Xm/MenuUtilP.h>   /* for _XmGrabPointer(), etc */
#include <Xm/VendorSP.h>    /* for _XmAddGrab(), _XmRemoveGrab(), etc */
#include <Xm/Label.h>
#include <Xm/ArrowB.h>
#include <Xm/RowColumnP.h>
#include <Xfe/Xfe.h>
#include <Xfe/Primitive.h>
#include <Xfe/Cascade.h>
#include <Xfe/Button.h>
#include <pa_tags.h>        /* for P_UNUM_LIST, P_NUM_LIST, etc */


#include "EditorToolbar.h"
#include "xeditor.h"

extern struct _WidgetClassRec xfePrimitiveClassRec;

static void
install_children_from_toolspec(Widget w_parent,
							   ToolbarSpec* tb_spec,
							   XFE_Component* cp_parent);

#define SAVE_ME(w) XtVaSetValues((w), XmNuserData, (XtPointer)this, 0)

class XFE_ComponentList
{
public:
	XFE_AbstractMenuItem* item() {
		return m_item;
	}
	XFE_ComponentList*   next() {
		return m_next;
	}
	XFE_ComponentList* push(XFE_AbstractMenuItem* item) {
		XFE_ComponentList* foo = new XFE_ComponentList;
		foo->m_next = this;
		foo->m_item = item;
		return foo;
	}
private:
	XFE_AbstractMenuItem* m_item;
	XFE_ComponentList*    m_next;
};

static void
getIcons(Widget widget, ToolbarSpec* spec,
		 Pixmap* r_pixmap,
		 Pixmap* r_ipixmap,
		 Pixmap* r_rpixmap,
		 Pixmap* r_apixmap)
{
	Pixmap  pixmap = 0;;
	Pixmap ipixmap = 0;
	Pixmap rpixmap = 0;
	Pixmap apixmap = 0;

	Widget parent = XtParent(widget);

	if (spec->iconGroup != NULL) {
		Widget toplevel = XfeAncestorFindTopLevelShell(parent);
		Pixel  fg = XfeForeground(parent);
		Pixel  bg = XfeBackground(parent);
		
		IconGroup_createAllIcons(spec->iconGroup, toplevel,	fg, bg);
		
		pixmap = spec->iconGroup->pixmap_icon.pixmap;
		ipixmap = spec->iconGroup->pixmap_i_icon.pixmap;
		rpixmap = spec->iconGroup->pixmap_mo_icon.pixmap;
		apixmap = spec->iconGroup->pixmap_md_icon.pixmap;
	}

	if (r_pixmap != NULL)
		*r_pixmap = pixmap;
	if (r_ipixmap != NULL)
		*r_ipixmap = ipixmap;
	if (r_rpixmap != NULL)
		*r_rpixmap = rpixmap;
	if (r_apixmap != NULL)
		*r_apixmap = apixmap;
}

static void
fe_setLabelTypeFromSpec(Widget widget, ToolbarSpec* spec)
{
	if (XfeIsButton(widget))
	{
 		if (spec && spec->iconGroup)
		{
			fe_buttonSetPixmaps(widget,spec->iconGroup);
		}
	}
	else
	{
		if (spec && spec->iconGroup)		
		{
			Pixmap		pixmap;
			Pixmap		ipixmap;
			Arg			args[4];
			Cardinal	n = 0;
			
			getIcons(widget, spec, &pixmap, &ipixmap, NULL, NULL);
			
			XtSetArg(args[n], XmNlabelType, XmPIXMAP); n++;
			XtSetArg(args[n], XmNlabelPixmap, pixmap); n++;
			
			if (ipixmap != 0 && ipixmap != pixmap) 
			{
				XtSetArg(args[n], XmNlabelInsensitivePixmap, ipixmap); n++;
			}
			
			if (n)
			{
				XtSetValues(widget, args, n);
			}
		}
		else
		{
			XtVaSetValues(widget,
						  XmNlabelType, XmSTRING,
						  NULL);
		}
	}
}

XP_Bool
XFE_ActionMenuItem::showsUpdate()
{
	if (m_cmd_handler != NULL)
		return m_cmd_handler->isDynamic();
	else
		return TRUE;
}

static void
pb_arm_cb(Widget /*widget*/, XtPointer clientData, XtPointer)
{
	XFE_ActionMenuItem*  menu_item = (XFE_ActionMenuItem*)clientData;
	XFE_Component*       frame = menu_item->getToplevel();
	CommandType          cmd = menu_item->getCmdId();

	frame->notifyInterested(Command::commandArmedCallback, (void*)cmd);
}

static void
pb_activate_cb(Widget widget, XtPointer clientData, XtPointer callData)
{
	XmPushButtonCallbackStruct* cbs
		= (XmPushButtonCallbackStruct*)callData;
	XFE_ActionMenuItem*  menu_item = (XFE_ActionMenuItem*)clientData;
	XFE_Frame*           frame = menu_item->getParentFrame();
	CommandType          cmd = menu_item->getCmdId();

	XFE_CommandInfo e_info(XFE_COMMAND_BUTTON_ACTIVATE,
						   widget,
						   cbs->event);

	menu_item->doCommand(&e_info);

	frame->notifyInterested(Command::commandDispatchedCallback, (void*)cmd);
}

static void
pb_disarm_cb(Widget /*widget*/, XtPointer clientData, XtPointer)
{
	XFE_ActionMenuItem*  menu_item = (XFE_ActionMenuItem*)clientData;
	XFE_Frame*           frame = menu_item->getParentFrame();
	CommandType          cmd = menu_item->getCmdId();

	frame->notifyInterested(Command::commandDisarmedCallback, (void*)cmd);
}

XFE_EditorToolbarPushButton::XFE_EditorToolbarPushButton(Widget,
														 ToolbarSpec* spec,
														 XFE_Component* tb)
	: XFE_EditorToolbarItem(tb, spec)
{
}

XFE_Command*
XFE_ActionMenuItem::getCommand(CommandType id)
{
	XFE_Frame* frame = getParentFrame();
    XFE_View*  view = 0;
    XFE_Command* cmd = 0;

    if (id == 0)
      id = m_cmd_id;

    // get the dispatcher from the XFE_EditorToolbar m_parent
    if (m_parent->isClassOf("EditorToolbar"))
    {
      XFE_EditorToolbar* tb = (XFE_EditorToolbar*)m_parent;
      if (tb)
      {
        XFE_Component* cmdDispatcher = tb->getCommandDispatcher();
        /* See comments in Menu.cpp */
        if (cmdDispatcher)
        {
          if (cmdDispatcher->isClassOf("View"))
			view = (XFE_View *)cmdDispatcher;
          else if (cmdDispatcher->isClassOf("Frame"))
			frame = (XFE_Frame *)cmdDispatcher;
        }
      }
    }

	if (view)
      cmd = view->getCommand(id);
	else if (frame)
      cmd = frame->getCommand(id);

    return cmd;
}

XFE_Component *
XFE_ActionMenuItem::getCommandDispatcher()
{
    // get the dispatcher from the XFE_EditorToolbar m_parent
    if (m_parent->isClassOf("EditorToolbar"))
    {
      XFE_EditorToolbar* tb = (XFE_EditorToolbar*)m_parent;
      if (tb)
      {
        XFE_Component* cmdDispatcher = tb->getCommandDispatcher();
        if (cmdDispatcher)
          return cmdDispatcher;
      }
    }
    return getParentFrame();
}

void XFE_ActionMenuItem::doCommand(XFE_CommandInfo* info)
{
  if (m_cmd_handler != NULL)
  {
    if (m_cmd_handler->isViewCommand())
    {
      XFE_Component* dispatcher = getCommandDispatcher();
      if (dispatcher && dispatcher->isClassOf("View"))
        ((XFE_ViewCommand*)m_cmd_handler)->setView((XFE_View*)dispatcher);
    }
    m_cmd_handler->doCommand(getParentFrame(), info);
  }
  else
  {
    XFE_Component* dispatcher = getCommandDispatcher();
    if (dispatcher && dispatcher->isClassOf("View"))
      ((XFE_View*)dispatcher)->doCommand(m_cmd_id, NULL, info);
    else if (dispatcher && dispatcher->isClassOf("Frame"))
      ((XFE_Frame*)dispatcher)->doCommand(m_cmd_id, NULL, info);
    else
      getParentFrame()->doCommand(m_cmd_id, NULL, info);
  }
}

void
XFE_EditorToolbarPushButton::update(XFE_Component* dispatcher)
{
	XFE_CommandInfo e_info(XFE_COMMAND_BUTTON_ACTIVATE, m_widget);
	Boolean         sensitive = False;

	//
	//    Do we have a command dispatcher?
	//
    if (dispatcher == 0)
      dispatcher = getParentFrame();

	//
	//    Do we have a command handler?
	//
	if (!m_cmd_handler)
		m_cmd_handler = getCommand(m_cmd_id);

	if (m_cmd_handler != NULL)
    {
      if (dispatcher->isClassOf("Frame"))
		sensitive = m_cmd_handler->isEnabled((XFE_Frame*)dispatcher, &e_info);
      else if (dispatcher->isClassOf("View"))
		sensitive = m_cmd_handler->isEnabled((XFE_View*)dispatcher, &e_info);
    }

	XtVaSetValues(m_widget, XtNsensitive, sensitive, 0);
}

class XFE_XfePushButton : public XFE_EditorToolbarPushButton
{
public:
	XFE_XfePushButton(Widget parent, ToolbarSpec* spec, XFE_Component* tb);
};

XFE_XfePushButton::XFE_XfePushButton(Widget parent,
									 ToolbarSpec* spec,
									 XFE_Component* tb)
	: XFE_EditorToolbarPushButton(parent, spec, tb)
{
	Arg      args[8];
	Cardinal n = 0;

	m_widget = XtCreateWidget((char*)spec->toolbarButtonName,
							  xfeButtonWidgetClass,
							  parent,
							  args,
							  n);
	fe_setLabelTypeFromSpec(m_widget, spec);

	XtAddCallback(m_widget, XmNarmCallback, pb_arm_cb, this);
	XtAddCallback(m_widget, XmNactivateCallback, pb_activate_cb, this);
	XtAddCallback(m_widget, XmNdisarmCallback, pb_disarm_cb, this);

	fe_WidgetAddToolTips(m_widget);

	SAVE_ME(m_widget);

	installDestroyHandler();
}

class XFE_XmPushButton : public XFE_EditorToolbarPushButton
{
public:
	XFE_XmPushButton(Widget parent,
					 ToolbarSpec* spec,
					 XFE_Component* tb);
};

XFE_XmPushButton::XFE_XmPushButton(Widget parent,
								   ToolbarSpec* spec,
								   XFE_Component* tb)
	: XFE_EditorToolbarPushButton(parent, spec, tb)
{
	Arg      args[8];
	Cardinal n = 0;

	m_widget = XmCreatePushButtonGadget(parent,
										(char*)spec->toolbarButtonName,
										args,
										n);
	fe_setLabelTypeFromSpec(m_widget, spec);

	XtAddCallback(m_widget, XmNarmCallback, pb_arm_cb, this);
	XtAddCallback(m_widget, XmNactivateCallback, pb_activate_cb, this);
	XtAddCallback(m_widget, XmNdisarmCallback, pb_disarm_cb, this);


	SAVE_ME(m_widget);

	installDestroyHandler();
}

static void
tb_valuechanged_cb(Widget widget, XtPointer closure, XtPointer cb_d)
{
	XfeButtonCallbackStruct*  cbs = (XfeButtonCallbackStruct*)cb_d;
	XFE_EditorToolbarToggleButton* button =
		(XFE_EditorToolbarToggleButton*)closure;

	XFE_Frame*  frame = button->getParentFrame();
	CommandType cmd = button->getCmdId();

	XFE_CommandInfo e_info(XFE_COMMAND_BUTTON_ACTIVATE,
						   widget,
						   cbs->event);
  
	button->doCommand(&e_info);

	frame->notifyInterested(Command::commandDispatchedCallback, (void*)cmd);
}

void
XFE_EditorToolbarToggleButton::update(XFE_Component* dispatcher)
{
	XFE_CommandInfo e_info(XFE_COMMAND_BUTTON_ACTIVATE, m_widget);
	Boolean         sensitive = False;
	Boolean         selected = False;
	Boolean         determinate = True;

	//
	//    Do we have a command dispatcher?
	//
    if (dispatcher == 0)
      dispatcher = getParentFrame();

	//
	//    Do we have a command handler?
	//
	if (!m_cmd_handler)
		m_cmd_handler = getCommand(m_cmd_id);

	if (m_cmd_handler != NULL)
    {
      if (dispatcher->isClassOf("Frame"))
      {
		sensitive = m_cmd_handler->isEnabled((XFE_Frame*)dispatcher, &e_info);
		selected = m_cmd_handler->isSelected((XFE_Frame*)dispatcher, &e_info);
		determinate = m_cmd_handler->isDeterminate((XFE_Frame*)dispatcher,
                                                   &e_info);
      }
      else if (dispatcher->isClassOf("View"))
      {
		sensitive = m_cmd_handler->isEnabled((XFE_View*)dispatcher, &e_info);
		selected = m_cmd_handler->isSelected((XFE_View*)dispatcher, &e_info);
		determinate = m_cmd_handler->isDeterminate((XFE_View*)dispatcher,
                                                   &e_info);
      }
    }

	XtVaSetValues(m_widget,
				  XtNsensitive, sensitive,
				  XmNdeterminate, determinate,
				  XmNarmed, (selected || !determinate),
				  0);
}

XFE_EditorToolbarToggleButton::XFE_EditorToolbarToggleButton(Widget parent,
															 ToolbarSpec* spec,
															 XFE_Component* tb)
	: XFE_EditorToolbarItem(tb, spec)
{
	Arg args[16];
	Cardinal n = 0;

 	XtSetArg(args[n], XmNbuttonType, XmBUTTON_TOGGLE); n++;
 	XtSetArg(args[n], XmNlabelString, NULL); n++;
	m_widget = XfeCreateButton(parent,
							   (char*)spec->toolbarButtonName,
							   args,
							   n);

	fe_setLabelTypeFromSpec(m_widget, spec);

	XtAddCallback(m_widget, XmNarmCallback, pb_arm_cb, this);
	XtAddCallback(m_widget, XmNactivateCallback, tb_valuechanged_cb, this);
	XtAddCallback(m_widget, XmNdisarmCallback, pb_disarm_cb, this);

	installDestroyHandler();

	fe_WidgetAddToolTips(m_widget);

	SAVE_ME(m_widget);
}

XFE_EditorToolbarSpacer::XFE_EditorToolbarSpacer(Widget parent, 
												 XFE_Component* tb) :
	XFE_EditorToolbarItem(tb, (ToolbarSpec*)NULL)
{
	m_widget = XmCreateLabelGadget(parent,
								   (char*)"spacer",
								   NULL, 0);
	XmString xms = XmStringCreateLtoR ("", XmSTRING_DEFAULT_CHARSET);
	XtVaSetValues(m_widget,
				  XmNlabelType, XmSTRING,
				  XmNlabelString, xms,
				  NULL);
	XmStringFree(xms);

	SAVE_ME(m_widget);

	installDestroyHandler();
}

class XFE_ComboList : public XFE_EditorToolbarItem
{
public:
	XFE_ComboList(Widget parent,
				  ToolbarSpec* spec,
				  XFE_Component* tb);
	virtual void update(XFE_Component* dispatcher = 0) = 0;
	virtual void itemSelected(unsigned index) = 0;
	void    selectItem(int index);
	void    addItems(XFE_CommandParameters* list);
protected:
	Widget m_combo;
};

static void
cl_selection_cb(Widget /*widget*/, XtPointer client_data, XtPointer cb_data)
{
	DtComboBoxCallbackStruct* info = (DtComboBoxCallbackStruct*)cb_data;
	XFE_ComboList*            list = (XFE_ComboList*)client_data;

	list->itemSelected(info->item_position);
}

XFE_ComboList::XFE_ComboList(Widget parent, ToolbarSpec* spec,
							 XFE_Component* tb)
	: XFE_EditorToolbarItem(tb, spec)
{
	Arg args[16];
	Cardinal n;
	char buf[256];

	sprintf(buf, "%sFrame", (char*)spec->toolbarButtonName);

	n = 0;
	XtSetArg(args[n], XmNshadowType, XmSHADOW_IN); n++;
	XtSetArg(args[n], XmNshadowThickness, 1); n++;
	XtSetArg(args[n], XmNmarginWidth, 0); n++;
	XtSetArg(args[n], XmNmarginHeight, 0); n++;
	m_widget = XmCreateFrame(parent, buf, args, n);

	Visual*  v = 0;
	Colormap cmap = 0;
	Cardinal depth = 0;
	XtVaGetValues(XfeAncestorFindTopLevelShell(parent),
				  XtNvisual, &v,
				  XtNcolormap, &cmap,
				  XtNdepth, &depth,
				  0);

	n = 0;
	XtSetArg(args[n], XmNvisual, v); n++;
	XtSetArg(args[n], XmNdepth, depth); n++;
	XtSetArg(args[n], XmNcolormap, cmap); n++;
	XtSetArg(args[n], XmNtype, XmDROP_DOWN_LIST_BOX); n++;
	XtSetArg(args[n], XmNshadowThickness, 1); n++;
	XtSetArg(args[n], XmNmarginWidth, 0); n++;
	XtSetArg(args[n], XmNmarginHeight, 0); n++;
	XtSetArg(args[n], XmNarrowType, XmMOTIF); n++;
	m_combo = DtCreateComboBox(m_widget,
                                (char*)spec->toolbarButtonName,
								args, n);
	XtManageChild(m_combo);

	XtAddCallback(m_combo, XmNselectionCallback, cl_selection_cb, this);

	Widget arrow = XtNameToWidget(m_combo, "ComboBoxArrow");
	Widget label = XtNameToWidget(m_combo, "Label");
	if (arrow != NULL)
		fe_WidgetAddToolTips(arrow);
	if (label != NULL)
		fe_WidgetAddToolTips(label);

	SAVE_ME(m_widget);

	installDestroyHandler();
}

void
XFE_ComboList::addItems(XFE_CommandParameters* list)
{
	unsigned i;
	char* string;
	XmString xm_string;

	if (!list)
		return;

	for (i = 0; list[i].name != NULL; i++) {

		string = XfeSubResourceGetStringValue(m_combo,
											  list[i].name, 
											  "SetParagraphStyle",
											  XmNlabelString, 
											  XmCXmString,
											  NULL);

		if (!string)
			string = list[i].name;
		xm_string = XmStringCreateLocalized(string);
		DtComboBoxAddItem(m_combo, xm_string, i + 1, FALSE);
		XmStringFree(xm_string);
	}
	XtVaSetValues(m_combo, XmNvisibleItemCount, (XtPointer)i, 0);
}

void
XFE_ComboList::selectItem(int index)
{
   if (index < 0) {
	   XmString blank = XmStringCreateLocalized("");
	   XtVaSetValues(m_combo,
					 XmNupdateLabel, False,
					 XmNlabelString, blank,
					 0);
	   XmStringFree(blank);
   } else {
	   XtVaSetValues(m_combo,
					 XmNupdateLabel, True,
					 XmNselectedPosition, index,
					 0);
   }
}

class XFE_SmartComboList : public XFE_ComboList
{
public:
	XFE_SmartComboList(Widget parent,
				  ToolbarSpec* spec,
				  XFE_Component* tb);
	void itemSelected(unsigned index);
	void update(XFE_Component* dispatcher = 0);
	
protected:
	XFE_CommandParameters* m_params;
};

XFE_SmartComboList::XFE_SmartComboList(Widget parent, ToolbarSpec* spec,
							 XFE_Component* tb)
	: XFE_ComboList(parent, spec, tb)
{
	//
	//    ask the command for list of parameters.
	//
	m_params = NULL;
}

void
XFE_SmartComboList::itemSelected(unsigned index)
{
	String params[2];
	unsigned i;

	if (m_params == NULL)
		return;

	for (i = 0; i < index; i++) {
		if (m_params[i].name == NULL)
			return;
	}
	
	params[0] = m_params[index].name;
	params[1] = NULL;

	XFE_CommandInfo e_info(XFE_COMMAND_BUTTON_ACTIVATE,
						   m_widget,
						   (XEvent*) NULL,
						   params, 1);
	doCommand(&e_info);
}

void
XFE_SmartComboList::update(XFE_Component* dispatcher)
{
	int        index = -1;

	//
	//    Do we have a command dispatcher?
	//
    if (dispatcher == 0)
      dispatcher = getParentFrame();

	//
	//    Do we have a command handler?
	//
    if (!m_cmd_handler)
      m_cmd_handler = getCommand();

	if (m_cmd_handler != NULL) {
		if (!m_params) {
            if (dispatcher->isClassOf("Frame"))
              m_params = m_cmd_handler->getParameters((XFE_Frame*)dispatcher);
            else if (dispatcher->isClassOf("View"))
			  m_params = m_cmd_handler->getParameters((XFE_View*)dispatcher);
            addItems(m_params);
		}

        if (dispatcher->isClassOf("Frame"))
          index = m_cmd_handler->getParameterIndex((XFE_Frame*)dispatcher);
        else if (dispatcher->isClassOf("View"))
          index = m_cmd_handler->getParameterIndex((XFE_View*)dispatcher);
	}

	selectItem(index);
}

class XFE_RamiroCascadeMenu : public XFE_EditorToolbarItem
{
public:
	XFE_RamiroCascadeMenu(Widget         parent,
						  ToolbarSpec*   spec,
						  XFE_Component* tb);
	virtual XP_Bool showsUpdate() { return False; };
protected:
	Widget       getMenuWidget();
	Widget       addChildPushButton(ToolbarSpec* spec);
};

XFE_RamiroCascadeMenu::XFE_RamiroCascadeMenu(Widget         parent,
											 ToolbarSpec*   spec,
											 XFE_Component* tb)
	: XFE_EditorToolbarItem(tb, spec)
{
	Arg args[16];
	Cardinal n;

	n = 0;
	XtSetArg(args[n], XmNshowArrows, True); n++;
	XtSetArg(args[n], XmNcascadeArrowWidth, 9); n++;
	XtSetArg(args[n], XmNcascadeArrowHeight, 9); n++;
	XtSetArg(args[n], XmNcascadeArrowLocation, XmLOCATION_SOUTH_WEST); n++;
	XtSetArg(args[n], XmNdrawCascadeArrow, True); n++;

	m_widget = XtCreateWidget((char*)spec->toolbarButtonName,
							  xfeCascadeWidgetClass,
							  parent,
							  args,
							  n);
	fe_setLabelTypeFromSpec(m_widget, spec);

	installDestroyHandler();

	fe_WidgetAddToolTips(m_widget);

	SAVE_ME(m_widget);
}

Widget
XFE_RamiroCascadeMenu::getMenuWidget()
{
	Widget sub_menu;
	XtVaGetValues(m_widget, XmNsubMenuId, &sub_menu, 0);
	return sub_menu;
}

Widget
XFE_RamiroCascadeMenu::addChildPushButton(ToolbarSpec* spec)
{
	Widget sub_menu = getMenuWidget();
	Arg args[16];
	Cardinal n = 0;

	Widget baby = XmCreatePushButton(sub_menu,
									 (char*)spec->toolbarButtonName,
									 args,
									 n);
	fe_setLabelTypeFromSpec(baby, spec);

	XtManageChild(baby);

	return baby;
}

class XFE_AlignmentMenu : public XFE_RamiroCascadeMenu
{
public:
	XFE_AlignmentMenu(Widget         parent,
					  ToolbarSpec*   spec,
					  XFE_Component* tb);
	void doIndex(unsigned index);
	void updateDisplayed(int index);
	void update(XFE_Component* dispatcher = 0);
	XP_Bool showsUpdate() { return TRUE; };
};

void
XFE_AlignmentMenu::update(XFE_Component* dispatcher)
{
	int        index = -1;

	//
	//    Do we have a command dispatcher?
	//
    if (dispatcher == 0)
      dispatcher = getParentFrame();

	//
	//    Do we have a command handler?
	//
	if (!m_cmd_handler)
    {
      if (dispatcher->isClassOf("Frame"))
		m_cmd_handler = getParentFrame()->getCommand(m_cmd_id);
      else if (dispatcher->isClassOf("View"))
		m_cmd_handler = ((XFE_View *)dispatcher)->getCommand(m_cmd_id);
    }

	if (m_cmd_handler != NULL)
    {
      if (dispatcher->isClassOf("Frame"))
		index = m_cmd_handler->getParameterIndex((XFE_Frame*)dispatcher);
      else if (dispatcher->isClassOf("View"))
		index = m_cmd_handler->getParameterIndex((XFE_View*)dispatcher);
    }

	updateDisplayed(index);
}

void
XFE_AlignmentMenu::doIndex(unsigned index)
{
	XFE_Frame* frame = getParentFrame();
	
	m_cmd_handler->setParameterIndex(frame, index);
}

void
XFE_AlignmentMenu::updateDisplayed(int index)
{
	ToolbarSpec* c_specs;

	if (index < 0) { // in-determinate
		c_specs = m_spec;
	} else {
		c_specs = (ToolbarSpec*)m_spec->submenu;
	}

	fe_buttonSetPixmaps(m_widget,c_specs[index].iconGroup);
	
}

static void
am_activate_cb(Widget widget, XtPointer closure, XtPointer /*cb_data*/)
{
	XFE_AlignmentMenu* menu = (XFE_AlignmentMenu*)closure;
	Widget             menu_widget = XtParent(widget);
	Widget*            children;
	Cardinal           nchildren;
	
	XtVaGetValues(menu_widget,
				  XmNchildren, &children,
				  XmNnumChildren, &nchildren,
				  0);
	
	unsigned n;
	for (n = 0; n < nchildren; n++) {
		if (children[n] == widget) {
			menu->doIndex(n);
			//
			//    I'm in two minds as to whether this should update
			//    or let the update() method take care of it. I'm
			//    doing it here, or there is consistancy betwen Motif
			//    option menus, drop boxes, and hand made selecting menus.
			//
			menu->updateDisplayed(n);
		}
	}
}

XFE_AlignmentMenu::	XFE_AlignmentMenu(Widget         parent,
									  ToolbarSpec*   spec,
									  XFE_Component* tb)
	: XFE_RamiroCascadeMenu(parent, spec, tb)
{
	ToolbarSpec* children_spec = (ToolbarSpec*)m_spec->submenu;

	for (; children_spec->toolbarButtonName != NULL; children_spec++) {
		Widget baby = addChildPushButton(children_spec);
		XtAddCallback(baby, XmNactivateCallback, am_activate_cb, this);
	}

	updateDisplayed(-1);
}

class XFE_PixmapMenu : public XFE_RamiroCascadeMenu
{
public:
	XFE_PixmapMenu(Widget         parent,
				   ToolbarSpec*   spec,
				   XFE_Component* tb);
};

XFE_PixmapMenu::XFE_PixmapMenu(Widget         parent,
							   ToolbarSpec*   spec,
							   XFE_Component* tb)
	: XFE_RamiroCascadeMenu(parent, spec, tb)
{
	ToolbarSpec* child_spec = (ToolbarSpec*)m_spec->submenu;
	Widget       menu = getMenuWidget();

	for (; child_spec->toolbarButtonName != NULL; child_spec++) {
		XFE_XmPushButton* baby = new XFE_XmPushButton(menu, child_spec, this);
		baby->show();
	}
}

class XFE_ColorMenu : public XFE_EditorToolbarItem
{
public:
	XFE_ColorMenu(Widget         parent,
					 ToolbarSpec*   spec,
					 XFE_Component* tb);
	void setValue(LO_Color* color);
	void update(XFE_Component* dispatcher = 0);
protected:
	void instanciateMenu(XFE_Component* dispatcher = 0);
	Widget m_arrow;
	Widget m_label;
	Widget m_popup;
	Widget m_rowcol;
};

void
XFE_ColorMenu::setValue(LO_Color* color)
{
	XFE_Frame* frame = getParentFrame();
	LO_Color   default_color;
	String params[2];
	char buf[32];

	if (m_cmd_handler == NULL)
		return;

	if (color == NULL) {
		MWContext* context = frame->getContext();

		//    Get the default color.
		fe_EditorDocumentGetColors(context,
								   NULL, /* bg image */
								   NULL, /* leave image */
								   NULL, /* bg color */
								   &default_color,/* normal color */
								   NULL, /* link color */
								   NULL, /* active color */
								   NULL); /* followed color */

		color = &default_color;
	}

	sprintf(buf, "#%02x%02x%02x", color->red, color->green, color->blue);

	params[0] = buf;
	params[1] = NULL;

	XFE_CommandInfo e_info(XFE_COMMAND_BUTTON_ACTIVATE,
						   m_widget,
						   (XEvent*) NULL,
						   params, 1);
	doCommand(&e_info);
}

static void
cm_popupdown_cb(Widget /*widget*/, XtPointer closure, XtPointer /*cb_data*/)
{
	Widget frame = (Widget)closure;
	Widget shell = XtParent(frame);

    XtPopdown(shell);
    XtUngrabPointer(shell, CurrentTime);
    XtUngrabKeyboard(shell, CurrentTime);
    XtRemoveGrab(shell);
}

static XtPointer
fe_find_widget_mappee(Widget widget, XtPointer data)
{
	Window window = (Window)data;

	if (XtWindow(widget) == window)
		return widget;
	else
		return 0;
}

static void
cm_shell_eh(Widget widget, XtPointer closure, XEvent* event, Boolean* dispatch)
{
	Widget frame = (Widget)closure;
	Widget shell = XtParent(frame);
    Window window = event->xbutton.window;

    if ((((ShellWidget)(shell))->shell.popped_up)
		&&
		!fe_WidgetTreeWalk(frame, fe_find_widget_mappee, (XtPointer)window)) {

		cm_popupdown_cb(widget, frame, NULL);
		*dispatch = False;
    }
}

static void
cm_activate_cb(Widget widget, XtPointer closure, XtPointer cb_data)
{
	XmArrowButtonCallbackStruct* cb = (XmArrowButtonCallbackStruct*)cb_data;

	if ((cb->event->type != ButtonPress && cb->event->type != ButtonRelease)
		||
		cb->event->xbutton.button != Button1)
		return;

	Widget    location_widget = XtParent(XtParent(widget));
	Widget    frame = (Widget)closure;
	Widget    shell = XtParent(frame);
    Display*  display = XtDisplayOfObject(widget);
    Screen*   screen = XtScreen(widget);
    Dimension border_width;
    Dimension width;
	Dimension height;
    Position  x;
	Position  y;
    Position  root_x;
	Position  root_y;
	char      buf[128];

    XtVaGetValues(location_widget,
				  XmNx, &x,
				  XmNy, &y,
				  XmNwidth, &width,
				  XmNheight, &height,
				  0);
    XtTranslateCoords(XtParent(location_widget), x, y, &root_x, &root_y);
	root_y += height;

    /*
     *    Make sure it fits on screen.
     */
	if (!XtIsRealized(shell))
		XtRealizeWidget(shell);
    XtVaGetValues(shell, XmNborderWidth, &border_width, 0);
    XtVaGetValues(frame, XmNwidth, &width, XmNheight, &height, 0);

    height += (2*border_width);
    width += (2*border_width);

    if (root_x + width > WidthOfScreen(screen))
		root_x = WidthOfScreen(screen) - width;
    else if (root_x < 0)
		root_x = 0;

    if (root_y + height > HeightOfScreen(screen))
		/*
		 *    Position at bottom of screen. Maybe it would be better to
		 *    position above arrow row.
		 */
		root_y = HeightOfScreen(screen) - height;
    else if (root_y < 0)
		root_y = 0;

    /*
     *    Make sure the user cannot shoot themselves with a random
     *    geometry spec.
     */
	sprintf(buf, "%dx%d", width, height);
	XtVaSetValues(shell,
				  XmNx, root_x, XmNy, root_y,
				  XmNwidth, width, XmNheight, height,
				  XmNgeometry, buf, 0);

    XtPopup(shell, XtGrabNone);

    XtGrabPointer(shell, True, ButtonPressMask,
				  GrabModeAsync, GrabModeAsync, None, 
				  XmGetMenuCursor(display), CurrentTime);
    XtGrabKeyboard(shell, False, GrabModeAsync, GrabModeAsync, 
				   CurrentTime);
    XtAddGrab(shell, True, True);
}

static void
cm_matrix_cb(Widget widget, XtPointer closure, XtPointer cbd)
{
	XFE_ColorMenu* color_menu = (XFE_ColorMenu*)closure;
	XmDrawnButtonCallbackStruct* cbs = (XmDrawnButtonCallbackStruct*)cbd;
	int    colorn;
	XColor color;
	XButtonEvent* event = (XButtonEvent*)cbs->event;

	colorn = fe_SwatchMatrixGetColor(widget, event->x, event->y, &color);

	if (colorn == -1)
		return;

	LO_Color lo_color;
	lo_color.red = color.red;
	lo_color.green = color.green;
	lo_color.blue =	color.blue;

	color_menu->setValue(&lo_color);
}

static void
cm_default_cb(Widget /*widget*/, XtPointer closure, XtPointer /*cbd*/)
{
	XFE_ColorMenu* color_menu = (XFE_ColorMenu*)closure;

	color_menu->setValue(NULL);
}

static void
cm_picker_cb(Widget /*widget*/, XtPointer closure, XtPointer /*cbd*/)
{
	XFE_ColorMenu* color_menu = (XFE_ColorMenu*)closure;
	XFE_Frame*     frame = color_menu->getParentFrame();

	if (frame->handlesCommand(xfeCmdSetCharacterColor)
		&& frame->isCommandEnabled(xfeCmdSetCharacterColor)) {
		frame->doCommand(xfeCmdSetCharacterColor);
	}
}

void
XFE_ColorMenu::instanciateMenu(XFE_Component *dispatcher)
{
	Widget     parent = XtParent(m_widget);

	if (!dispatcher)
		dispatcher = getParentFrame();

	//
	//    Do we have a command handler?
	//
    if (dispatcher->isClassOf("Frame"))
		m_cmd_handler = ((XFE_Frame *)dispatcher)->getCommand(m_cmd_id);
    else if (dispatcher->isClassOf("View"))
		m_cmd_handler = ((XFE_View *)dispatcher)->getCommand(m_cmd_id);

	if (!m_cmd_handler)
		return;

	Cardinal n = 0;
	Arg      args[16];
	XtSetArg(args[n], XmNcolormap, XfeColormap(parent));	n++;
	XtSetArg(args[n], XmNdepth, XfeDepth(parent)); n++;
	XtSetArg(args[n], XmNvisual,	XfeVisual(parent));	n++;
    XtSetArg(args[n], XtNoverrideRedirect, TRUE); n++;
    XtSetArg(args[n], XtNallowShellResize, TRUE); n++;
    XtSetArg(args[n], XtNsaveUnder, TRUE); n++;
    XtSetArg(args[n], XtNtransientFor, getParentFrame()->getBaseWidget()); n++;

	Widget shell = XtCreatePopupShell("setColorsPopup", 
									  transientShellWidgetClass,
									  parent,
									  args, n);

	n = 0;
	XtSetArg(args[n], XmNshadowType, XmSHADOW_ETCHED_OUT); n++;
	XtSetArg(args[n], XmNshadowThickness, 2); n++;
	XtSetArg(args[n], XmNmarginWidth, 0); n++;
	XtSetArg(args[n], XmNmarginHeight, 0); n++;
	Widget f_widget = XmCreateFrame(shell, "frame", args, n);
	XtManageChild(f_widget);
	
	n = 0;
	XtSetArg(args[n], XmNmarginHeight, 0); n++;
	XtSetArg(args[n], XmNmarginWidth, 0); n++;
	XtSetArg(args[n], XmNrowColumnType, XmWORK_AREA); n++;
	XtSetArg(args[n], XmNorientation, XmVERTICAL); n++;
	XtSetArg(args[n], XmNpacking, XmPACK_TIGHT); n++;
	XtSetArg(args[n], XmNspacing, 0); n++;
	m_rowcol = XmCreateRowColumn(f_widget, "colorPicker", args, n);
	XtManageChild(m_rowcol);

	m_popup = f_widget;

    XtInsertEventHandler(shell, ButtonPressMask, True,
						 (XtEventHandler)cm_shell_eh, 
						 (XtPointer)m_popup, XtListHead);

	n = 0;
	Widget matrix = fe_CreateSwatchMatrix(m_rowcol, "colorMatrix", args, n);

	fe_AddSwatchMatrixCallback(matrix,
							   XmNactivateCallback, cm_matrix_cb, this);
	fe_AddSwatchMatrixCallback(matrix,
							   XmNactivateCallback, cm_popupdown_cb, m_popup);

	XtManageChild(matrix);

	n = 0;
	Widget default_b = XmCreatePushButtonGadget(m_rowcol, "defaultColor",
												args, n);
	XtManageChild(default_b);
	XtAddCallback(default_b,
				  XmNactivateCallback, cm_popupdown_cb, m_popup);
	XtAddCallback(default_b,
				  XmNactivateCallback, cm_default_cb, this);

	n = 0;
	Widget picker_b = XmCreatePushButtonGadget(m_rowcol, "otherColor",
											   args, n);
	XtManageChild(picker_b);
	XtAddCallback(picker_b,
				  XmNactivateCallback, cm_popupdown_cb, m_popup);
	XtAddCallback(picker_b,
				  XmNactivateCallback, cm_picker_cb, this);

	XtAddCallback(m_arrow, XmNactivateCallback, cm_activate_cb, m_popup);
	XtAddCallback(m_label, XmNactivateCallback, cm_activate_cb, m_popup);
	//FIXME: add tooltips.
}

XFE_ColorMenu::XFE_ColorMenu(Widget         parent,
							  ToolbarSpec*   spec,
							  XFE_Component* tb)
	: XFE_EditorToolbarItem(tb, spec)
{
	Arg args[16];
	Cardinal n = 0;
	Widget rowcol;
	Widget frame_widget;

	n = 0;
	XtSetArg(args[n], XmNshadowType, XmSHADOW_ETCHED_IN); n++;
	XtSetArg(args[n], XmNshadowThickness, 2); n++;
	XtSetArg(args[n], XmNmarginWidth, 0); n++;
	XtSetArg(args[n], XmNmarginHeight, 0); n++;
	frame_widget = XmCreateFrame(parent, "frame", args, n);

	n = 0;
	XtSetArg(args[n], XmNmarginHeight, 0); n++;
	XtSetArg(args[n], XmNmarginWidth, 0); n++;
	XtSetArg(args[n], XmNrowColumnType, XmWORK_AREA); n++;
	XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg(args[n], XmNpacking, XmPACK_TIGHT); n++;
	XtSetArg(args[n], XmNspacing, 0); n++;
	rowcol = XmCreateRowColumn(frame_widget,
							   (char*)spec->toolbarButtonName,
							   args,
							   n);
	XtManageChild(rowcol);

	XmString xm_string = XmStringCreateLocalized("");
	n = 0;
	XtSetArg(args[n], XmNlabelType, XmSTRING); n++;
	XtSetArg(args[n], XmNrecomputeSize, FALSE); n++;
	XtSetArg(args[n], XmNshadowThickness, 1); n++;
	XtSetArg(args[n], XmNlabelString, xm_string); n++;
	XtSetArg(args[n], XmNmarginLeft, LABEL_PADDING); n++;
	XtSetArg(args[n], XmNmarginRight, LABEL_PADDING); n++;
	XtSetArg(args[n], XmNmarginWidth, 0); n++;
	XtSetArg(args[n], XmNmarginHeight, 0); n++;
	XtSetArg(args[n], XmNfillOnArm, False); n++;

	m_label = XmCreatePushButton(rowcol, "label", args, n);
	XtManageChild(m_label);
	XmStringFree(xm_string);

	n = 0;
    XtSetArg(args[n], XmNheight, 14); n++;
    XtSetArg(args[n], XmNtraversalOn, FALSE); n++;
    XtSetArg(args[n], XmNhighlightThickness, 0); n++;
	XtSetArg(args[n], XmNshadowThickness, 1); n++;
	XtSetArg(args[n], XmNarrowDirection, XmARROW_DOWN); n++;
	XtSetArg(args[n], XmNforeground, XfeBackground(parent)); n++;
	XtSetArg(args[n], XmNmarginHeight, 0); n++;
	XtSetArg(args[n], XmNmarginHeight, 0); n++;
	m_arrow = XmCreateArrowButton(rowcol, "arrow", args, n);
	XtManageChild(m_arrow);

	fe_WidgetAddToolTips(m_arrow);
	fe_WidgetAddToolTips(m_label);

	/*
	 *   Make the label symetrical with the arrow.
	 */
	Dimension width;
	Dimension height;
	XtVaGetValues(m_arrow, XmNwidth, &width, XmNheight, &height, 0);
	XtVaSetValues(m_label, XmNwidth, width, XmNheight, height, 0);

	m_widget = frame_widget;
	SAVE_ME(m_widget);

	installDestroyHandler();

	//    Don't instanciate menu until later - it requires us
	//    binding to cmd_handler which would suck.
	m_rowcol = rowcol;
	m_popup = NULL;
}

void
XFE_ColorMenu::update(XFE_Component* dispatcher)
{
	//
	//    Do we have a command dispatcher?
	//
    if (dispatcher == 0)
      dispatcher = getParentFrame();

	MWContext* context;
    if (dispatcher->isClassOf("Frame"))
        context = ((XFE_Frame*)dispatcher)->getContext();
    else if (dispatcher->isClassOf("View"))
        context = ((XFE_View*)dispatcher)->getContext();

	//
	//    Do we have a command handler?
	//
	if (!m_cmd_handler)
    {
      if (dispatcher->isClassOf("Frame"))
		m_cmd_handler = getParentFrame()->getCommand(m_cmd_id);
      else if (dispatcher->isClassOf("View"))
		m_cmd_handler = ((XFE_View *)dispatcher)->getCommand(m_cmd_id);
    }

	if (!m_popup)
		instanciateMenu(dispatcher);

	Pixel pixel;

    Boolean is_determinate = FALSE;
    if (dispatcher->isClassOf("Frame"))
      is_determinate = m_cmd_handler->isDeterminate((XFE_Frame*)dispatcher, 0);
    else if (dispatcher->isClassOf("View"))
      is_determinate = m_cmd_handler->isDeterminate((XFE_View*)dispatcher, 0);

	LO_Color   color;
	if (is_determinate) { /* is it defined */
		fe_EditorColorGet(context, &color);
		pixel = fe_GetPixel(context, color.red, color.green, color.blue);
	} else {
		XtVaGetValues(m_rowcol, XmNbackground, &pixel, NULL);
	}

	XtVaSetValues(m_label, XmNbackground, pixel, NULL);
}

static void
chrome_update_cb(XFE_NotificationCenter*, XFE_NotificationCenter* obj,
				 void*, void*)
{
	XFE_EditorToolbar* tb = (XFE_EditorToolbar*)obj;
	tb->update();
}

static void
command_update_cb(XFE_NotificationCenter*, XFE_NotificationCenter* obj,
				 void*, void* call_data)
{
	XFE_EditorToolbar* tb = (XFE_EditorToolbar*)obj;
	tb->updateCommand((CommandType)call_data);
}

static Widget
make_toolbar(Widget parent, char* name,
			 ToolbarSpec* spec, XFE_EditorToolbar* owner, Boolean do_frame)
{
    Arg      args[20];
    Cardinal n;

	if (do_frame) {
		char buf[256];

		strcpy(buf, name);
		strcat(buf, "Frame");

		n = 0;
		XtSetArg(args[n], XmNshadowType, XmSHADOW_OUT); n++;
		XtSetArg(args[n], XmNshadowThickness, 1); n++;
		Widget frame = XmCreateFrame(parent, buf, args, n);
		parent = frame;
	}

	n = 0;
	XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg(args[n], XmNpacking, XmPACK_TIGHT); n++;
	XtSetArg(args[n], XmNadjustMargin, False); n++;
	Widget toolbar = XmCreateRowColumn(parent, name, args, n);

	if (do_frame)
		XtManageChild(toolbar);

	install_children_from_toolspec(toolbar, spec, owner);

	return toolbar;
}

XFE_EditorToolbar::XFE_EditorToolbar(XFE_Component* top_level,
									 XFE_Toolbox *	parent_toolbox,
									 char*        name,
									 ToolbarSpec* spec,
									 Boolean      do_frame) :
	XFE_ToolboxItem(top_level,parent_toolbox)
{
	m_toplevel = top_level;
	m_update_list = NULL;

    m_cmdDispatcher = 0;

	m_rowcol = make_toolbar(parent_toolbox->getBaseWidget(),
							name, spec, this, do_frame);

	if (do_frame)
		m_widget = XtParent(m_rowcol);
	else
		m_widget = m_rowcol;

	// This is needed so that the toolbox frame can have access this
	// object without having to maintain a list.
	XtVaSetValues(m_widget,XmNuserData,this,NULL);

	// Add drag widgets for the editor toolbar
	addDragWidget(getBaseWidget());
	addDragWidget(getChildrenManager());

	top_level->registerInterest(XFE_View::chromeNeedsUpdating,
								   this,
								   chrome_update_cb);

	top_level->registerInterest(XFE_View::commandNeedsUpdating,
								   this,
								   command_update_cb);
	installDestroyHandler();
}

XFE_EditorToolbar::~XFE_EditorToolbar()
{
	m_toplevel->unregisterInterest(XFE_View::chromeNeedsUpdating,
								   this,
								   chrome_update_cb);

	m_toplevel->unregisterInterest(XFE_View::commandNeedsUpdating,
								   this,
								   command_update_cb);
}

const char* XFE_EditorToolbar::getClassName()
{
	return "XFE_EditorToolbar::className";
}

Widget
XFE_EditorToolbar::findButton(const char* /*name*/,
							  EChromeTag /*tag*/)
{
	return NULL;
}

static void
update_children(Widget parent, CommandType cmd, XFE_Component* dispatcher)
{
	Widget* children;
	int     nchildren;
	int     i;

	XtVaGetValues(parent,
				  XmNchildren, &children,
				  XmNnumChildren, &nchildren,
				  NULL);

	for (i = 0; i < nchildren; i++) {
		XFE_MenuItem* menu_item = (XFE_MenuItem*)XfeUserData(children[i]);
		
		if (!menu_item)
			continue;

		if ((cmd == 0 || menu_item->getCmdId() == cmd)
			&&
			menu_item->showsUpdate()) {

			menu_item->update(dispatcher);
		}
	}
}

void
XFE_EditorToolbar::updateCommand(CommandType cmd)
{
	if (!XtIsManaged(m_widget))
		return;
  
	update_children(m_rowcol, cmd, m_cmdDispatcher);
}

void
XFE_EditorToolbar::update()
{
	updateCommand(0);
}

void
XFE_EditorToolbar::show()
{
	update_children(m_rowcol, 0, m_cmdDispatcher);
        // skip the updateCommand() managed check.
	XFE_ToolboxItem::show();
}

static void
install_children_from_toolspec(Widget w_parent,
							   ToolbarSpec* tb_spec,
							   XFE_Component* cp_parent)
{
	ToolbarSpec* spec;
	
	for (spec = tb_spec; spec->toolbarButtonName != NULL; spec++) {
		
		XFE_EditorToolbarItem* item;
		
		if (spec->tag == PUSHBUTTON) {
			
			item = new XFE_XfePushButton(w_parent, spec, cp_parent);

		} else if (spec->tag == TOGGLEBUTTON) {
			
			item = new XFE_EditorToolbarToggleButton(w_parent,spec, cp_parent);

		} else if (spec->tag == CASCADEBUTTON) {
			
			if (spec->toolbarButtonName == xfeCmdSetAlignmentStyle)
				item = new XFE_AlignmentMenu(w_parent, spec, cp_parent);
			else
				item = new XFE_PixmapMenu(w_parent, spec, cp_parent);

		} else if (spec->tag == COMBOBOX) {

			if (spec->toolbarButtonName == xfeCmdSetFontColor)
				item = new XFE_ColorMenu(w_parent, spec, cp_parent);
			else
				item = new XFE_SmartComboList(w_parent, spec, cp_parent);

		} else if (spec->tag == SEPARATOR) {
			
			item = new XFE_EditorToolbarSpacer(w_parent, cp_parent);

		} else {
			/*error*/
			continue;
		}
		
		item->show(); /* manage */
	}
}


