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
   Menu -- class definition for menu bars.
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
 */



#ifndef _xfe_menu_h
#define _xfe_menu_h

#include "Component.h"
#include "Command.h"
#include "Chrome.h"

class XFE_View;
class XFE_Frame;

typedef void (*dynamenuCreateProc)(Widget, void *, XFE_Frame *);

typedef struct MenuSpec {
  /* these are generally initialized in a menu spec. */
  char *menuItemName;
  EChromeTag tag;
  struct MenuSpec *submenu;
  char *radioGroupName;
  Boolean toggleSet;
  void *callData; /* sent in the callData portion of the *Command functions. */

  dynamenuCreateProc generateProc;
  
  /* Paramaterization stuff */
  char*  cmd_name;
  char*  cmd_args[8];

  Boolean do_not_manage; /* set to true if we don't manage this item */

  //  methods
  char* getCmdName() {
    return (cmd_name != NULL)? cmd_name: menuItemName;
  }
  
} MenuSpec;

/* nice shortcuts */
#define MENU_SEPARATOR { "--", SEPARATOR }
#define MENU_END { NULL }
#define MENU_TOGGLE(w) \
{ (w), TOGGLEBUTTON }
#define MENU_TOGGLE_1ARG(w,c,a) \
{ (w), TOGGLEBUTTON, 0, 0, 0, 0, 0, (c), { (a) } }
#define MENU_PUSHBUTTON(w) \
{ (w), PUSHBUTTON }
#define MENU_PUSHBUTTON_NAMED(w, c) \
{ (w), PUSHBUTTON, 0, 0, 0, 0, 0, (c) }
#define MENU_PUSHBUTTON_1ARG(w,c,a) \
{ (w), PUSHBUTTON, 0, 0, 0, 0, 0, (c), { (a) } }
#define MENU_MENU(w, s) \
{ (w), CASCADEBUTTON, (MenuSpec*)(s) }

#define xfeMenuFile "fileMenu"
#define xfeMenuEdit "editMenu"
#define xfeMenuView "viewMenu"
#define xfeMenuInsert "insertMenu"
#define xfeMenuFormat "formatMenu"
#define xfeMenuTools "toolsMenu"
#define xfeMenuGo "goMenu"
#define xfeMenuItem "itemMenu"
#define xfeMenuMessage "messageMenu"
#define xfeMenuBookmark "bookmarkMenu"

#if defined(MOZ_MAIL_NEWS) && defined(EDITOR) && defined(MOZ_TASKBAR)
#define xfeMenuWindow "windowMenu"
#else
#define xfeMenuWindow "windowMenuLite"
#endif

#define xfeMenuHelp "helpMenu"

// this next one is going away soon... don't use it.  Use windowMenu
#define xfeMenuNavigate "navigateMenu"

class XFE_Menu : public XFE_Component
{
public:
  XFE_Menu(XFE_Frame *parent_frame, MenuSpec *menu_spec, Widget baseMenuWidget);
  virtual ~XFE_Menu();

  XFE_Frame *getFrame();

  void setMenuSpec(MenuSpec *menu_spec);

  // interface for view's adding and subtracting menu items
  Widget addMenuItem(char *itemName, EChromeTag tag, 
		     MenuSpec *submenuSpec = NULL, char *radioGroupName = NULL,
		     Boolean toggleSet = False, void *callData = NULL, dynamenuCreateProc generateProc = NULL,
		     int position = XmLAST_POSITION);
  void removeMenuItem(char *itemName, EChromeTag tag);

  void hideMenuItem(char *itemName, EChromeTag tag);
  void showMenuItem(char *itemName, EChromeTag tag);
  Widget findMenuItem(char *itemName, EChromeTag tag);

  void addMenuSpec(MenuSpec *menu_spec);

  void update();
  void updateSubMenu(Widget submenu);
  void updateMenuItem(Widget menuItem);

  void updateCommand(CommandType command);
  void updateCommandInSubMenu(CommandType command, Widget submenu);
  void updateCommandInMenuItem(CommandType command, Widget menuItem);

  // update all the commands in the menu.
  XFE_CALLBACK_DECL(update)
  // update a specific command in the menu.
  XFE_CALLBACK_DECL(updateCommand)

protected:
  void createWidgets();
  void createWidgets(MenuSpec *spec);

  Widget findMenuItemInMenu(char *itemName, EChromeTag tag, Widget menu);

private:
  XFE_Frame *m_parentFrame;
  MenuSpec *m_spec;

  Widget createPulldown(char *cascadeName, MenuSpec *spec, Widget parent_menu, XP_Bool is_fancy);
  Widget createMenuItem(MenuSpec *spec, Widget parent_cascade, Widget parent);

#ifdef DELAYED_MENU_CREATION
  void delayedCreatePulldown(Widget cascade, Widget pulldown, MenuSpec *spec);
  static void delayed_create_pulldown(Widget, XtPointer, XtPointer);
#endif

  void updateToggleGroup(Widget w);

  static void pushb_arm_cb(Widget, XtPointer, XtPointer);
  static void pushb_activate_cb(Widget, XtPointer, XtPointer);
  static void pushb_disarm_cb(Widget, XtPointer, XtPointer);

  static void toggleb_arm_cb(Widget, XtPointer, XtPointer);
  static void toggleb_activate_cb(Widget, XtPointer, XtPointer);
  static void toggleb_disarm_cb(Widget, XtPointer, XtPointer);

  static void radiob_arm_cb(Widget, XtPointer, XtPointer);
  static void radiob_activate_cb(Widget, XtPointer, XtPointer);
  static void radiob_disarm_cb(Widget, XtPointer, XtPointer);

  static void cascade_update_cb(Widget, XtPointer, XtPointer);
};

#endif /* _xfe_menu_h */
