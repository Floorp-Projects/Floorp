/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL") you may not use this file except in
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/******

  This file contains the list of all XUL nsIAtoms and their values
  
  It is designed to be used as inline input to nsXULAtoms.cpp *only*
  through the magic of C preprocessing.

  All entires must be enclosed in the macro XUL_ATOM which will have cruel
  and unusual things done to it

  It is recommended (but not strictly necessary) to keep all entries
  in alphabetical order

  The first argument to XUL_ATOM is the C++ identifier of the atom
  The second argument is the string value of the atom

 ******/


XUL_ATOM(button, "button")
XUL_ATOM(spinner, "spinner")
XUL_ATOM(scrollbar, "scrollbar")
XUL_ATOM(slider, "slider")
XUL_ATOM(colorpicker, "colorpicker")
XUL_ATOM(palettename, "palettename")
XUL_ATOM(fontpicker, "fontpicker")
XUL_ATOM(radio, "radio")
XUL_ATOM(text, "text")
XUL_ATOM(toolbar, "toolbar")
XUL_ATOM(toolbaritem, "toolbaritem")
XUL_ATOM(toolbox, "toolbox")

  // The tree atoms
XUL_ATOM(tree, "tree") // The start of a tree view
XUL_ATOM(treecaption, "treecaption") // The caption of a tree view
XUL_ATOM(treehead, "treehead") // The header of the tree view
XUL_ATOM(treerow, "treerow") // A row in the tree view
XUL_ATOM(treecell, "treecell") // An item in the tree view
XUL_ATOM(treeitem, "treeitem") // A cell in the tree view
XUL_ATOM(treechildren, "treechildren") // The children of an item in the tree view
XUL_ATOM(treeindentation, "treeindentation") // Specifies that the indentation for the level should occur here.
XUL_ATOM(treeallowevents, "treeallowevents") // Lets events be handled on the cell contents.
XUL_ATOM(treecol, "treecol") // A column in the tree view
XUL_ATOM(treecolgroup, "treecolgroup") // A column group in the tree view
XUL_ATOM(treefoot, "treefoot") // The footer of the tree view
XUL_ATOM(treepusher, "treepusher") // A column pusher (left or right) for the tree view

XUL_ATOM(open, "open") // Whether or not a menu, tree, etc. is open

XUL_ATOM(menubar, "menubar") // An XP menu bar.
XUL_ATOM(menu, "menu") // Represents an XP menu
XUL_ATOM(menuitem, "menuitem") // Represents an XP menu item
XUL_ATOM(menupopup, "menupopup") // The XP menu's children.
XUL_ATOM(menuactive, "menuactive") // Whether or not a menu is active (without necessarily being open)
XUL_ATOM(accesskey, "accesskey") // The shortcut key for a menu or menu item
XUL_ATOM(acceltext, "acceltext") // Text to use for the accelerator
XUL_ATOM(menupopupset, "menupopupset") // Contains popup menus, context menus, and tooltips

XUL_ATOM(key, "key") // A key element
XUL_ATOM(broadcaster, "broadcaster") // A broadcaster
XUL_ATOM(observes, "observes") // The observes element
XUL_ATOM(templateAtom, "template") // A XUL template

XUL_ATOM(progressmeter, "progressmeter")
XUL_ATOM(titledbutton, "titledbutton")
XUL_ATOM(crop, "crop")

XUL_ATOM(mode, "mode")
XUL_ATOM(box, "box")
XUL_ATOM(flex, "flex")
XUL_ATOM(spring, "spring")

XUL_ATOM(deck, "deck")
XUL_ATOM(tabcontrol, "tabcontrol")
XUL_ATOM(tab, "tab")
XUL_ATOM(tabpanel, "tabpanel")
XUL_ATOM(tabpage, "tabpage")
XUL_ATOM(tabbox, "tabbox")
XUL_ATOM(maxpos, "maxpos")
XUL_ATOM(curpos, "curpos")
XUL_ATOM(scrollbarbutton, "scrollbarbutton")
XUL_ATOM(increment, "increment")
XUL_ATOM(pageincrement, "pageincrement")
XUL_ATOM(thumb, "thumb")
XUL_ATOM(toggled, "toggled")
XUL_ATOM(grippy, "grippy")
XUL_ATOM(splitter, "splitter")
XUL_ATOM(collapse, "collapse")
XUL_ATOM(resizebefore, "resizebefore")
XUL_ATOM(resizeafter, "resizeafter")
XUL_ATOM(state, "state")


XUL_ATOM(widget, "widget")
XUL_ATOM(window, "window")
