/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
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
XUL_ATOM(palettename, "palettename")
XUL_ATOM(fontpicker, "fontpicker")
XUL_ATOM(text, "text")
XUL_ATOM(toolbar, "toolbar")
XUL_ATOM(toolbaritem, "toolbaritem")
XUL_ATOM(toolbox, "toolbox")
XUL_ATOM(image, "image")

  // The tree atoms
XUL_ATOM(tree, "tree") // The start of a tree view
XUL_ATOM(treecaption, "treecaption") // The caption of a tree view
XUL_ATOM(treehead, "treehead") // The header of the tree view
XUL_ATOM(treerow, "treerow") // A row in the tree view
XUL_ATOM(treerows, "treerows") // A row in the tree view
XUL_ATOM(treecell, "treecell") // An item in the tree view
XUL_ATOM(treeitem, "treeitem") // A cell in the tree view
XUL_ATOM(treechildren, "treechildren") // The children of an item in the tree view
XUL_ATOM(treeindentation, "treeindentation") // Specifies that the indentation for the level should occur here.
XUL_ATOM(allowevents, "allowevents") // Lets events be handled on the cell contents or in menus.
XUL_ATOM(treecol, "treecol") // A column in the tree view
XUL_ATOM(treecolgroup, "treecolgroup") // A column group in the tree view
XUL_ATOM(treecols, "treecols") // A column group in the tree view
XUL_ATOM(treefoot, "treefoot") // The footer of the tree view
XUL_ATOM(scrollbarlist, "scrollbarlist") // An atom for internal use by the tree view
XUL_ATOM(indent, "indent") // indicates that a cell should be indented

XUL_ATOM(open, "open") // Whether or not a menu, tree, etc. is open

XUL_ATOM(menubar, "menubar") // An XP menu bar.
XUL_ATOM(menu, "menu") // Represents an XP menu
XUL_ATOM(menuitem, "menuitem") // Represents an XP menu item
XUL_ATOM(menupopup, "menupopup") // The XP menu's children.
XUL_ATOM(menutobedisplayed, "menutobedisplayed") // The menu is about to be displayed at the next sync w/ frame
XUL_ATOM(menuactive, "menuactive") // Whether or not a menu is active (without necessarily being open)
XUL_ATOM(accesskey, "accesskey") // The shortcut key for a menu or menu item
XUL_ATOM(acceltext, "acceltext") // Text to use for the accelerator
XUL_ATOM(popupset, "popupset") // Contains popup menus, context menus, and tooltips
XUL_ATOM(popup, "popup") // The popup for a context menu, popup menu, or tooltip
XUL_ATOM(menugenerated, "menugenerated") // Internal
XUL_ATOM(popupanchor, "popupanchor") // Anchor for popups
XUL_ATOM(popupalign, "popupalign") // Alignment for popups
XUL_ATOM(ignorekeys, "ignorekeys") // Alignment for popups

XUL_ATOM(key, "key") // A key element
XUL_ATOM(broadcaster, "broadcaster") // A broadcaster
XUL_ATOM(observes, "observes") // The observes element
XUL_ATOM(templateAtom, "template") // A XUL template

XUL_ATOM(progressbar, "progressbar")
XUL_ATOM(titledbutton, "titledbutton")
XUL_ATOM(crop, "crop")

XUL_ATOM(mode, "mode")
XUL_ATOM(equalsize, "equalsize")
XUL_ATOM(box, "box")
XUL_ATOM(hbox, "hbox")
XUL_ATOM(vbox, "vbox")
XUL_ATOM(scrollbox, "scrollbox")
XUL_ATOM(mousethrough, "mousethrough")
XUL_ATOM(flex, "flex")
XUL_ATOM(spring, "spring")
XUL_ATOM(orient, "orient")
XUL_ATOM(autostretch, "autostretch")

XUL_ATOM(autorepeatbutton, "autorepeatbutton")

XUL_ATOM(bulletinboard, "bulletinboard")

XUL_ATOM(titledbox, "titledbox")
XUL_ATOM(title, "title")
XUL_ATOM(titledboxContentPseudo, ":titledbox-content")

XUL_ATOM(stack, "stack")
XUL_ATOM(deck, "deck")
XUL_ATOM(tabcontrol, "tabcontrol")
XUL_ATOM(tab, "tab")
XUL_ATOM(tabpanel, "tabpanel")
XUL_ATOM(tabpage, "tabpage")
XUL_ATOM(tabbox, "tabbox")
XUL_ATOM(index, "index")
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
XUL_ATOM(collapsed, "collapsed")
XUL_ATOM(resizebefore, "resizebefore")
XUL_ATOM(resizeafter, "resizeafter")
XUL_ATOM(state, "state")
XUL_ATOM(debug, "debug")

// grid
XUL_ATOM(grid, "grid")
XUL_ATOM(rows, "rows")
XUL_ATOM(columns, "columns")
XUL_ATOM(row, "row")
XUL_ATOM(column, "column")

// toolbar & toolbar d&d atoms
XUL_ATOM(ddDropLocation, "dd-droplocation")
XUL_ATOM(ddDropLocationCoord, "dd-droplocationcoord")
XUL_ATOM(ddDropOn, "dd-dropon")
XUL_ATOM(ddTriggerRepaintSorted, "dd-triggerrepaintsorted")
XUL_ATOM(ddTriggerRepaintRestore, "dd-triggerrepaintrestore")
XUL_ATOM(ddTriggerRepaint, "dd-triggerrepaint")
XUL_ATOM(ddNoDropBetweenRows, "dd-nodropbetweenrows")
XUL_ATOM(container, "container")
XUL_ATOM(ddDragDropArea, "dragdroparea")
XUL_ATOM(ddDropMarker, ":-moz-drop-marker")

XUL_ATOM(widget, "widget")
XUL_ATOM(window, "window")

XUL_ATOM(iframe, "iframe")
XUL_ATOM(browser, "browser")
XUL_ATOM(editor, "editor")

// 

XUL_ATOM(checkbox, "checkbox")
XUL_ATOM(radio, "radio")
XUL_ATOM(radiogroup, "radiogroup")
XUL_ATOM(menulist, "menulist")
XUL_ATOM(menubutton, "menubutton")
XUL_ATOM(textfield, "textfield")
XUL_ATOM(textarea, "textarea")
XUL_ATOM(listbox, "listbox")

XUL_ATOM(blankrow, "blankrow")
XUL_ATOM(titlebar, "titlebar")
XUL_ATOM(resizer, "resizer")
XUL_ATOM(dir, "dir")

