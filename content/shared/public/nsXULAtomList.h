/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

// OUTPUT_CLASS=nsXULAtoms
// MACRO_NAME=XUL_ATOM

XUL_ATOM(button, "button")
XUL_ATOM(spinner, "spinner")
XUL_ATOM(scrollbar, "scrollbar")
XUL_ATOM(nativescrollbar, "nativescrollbar")
XUL_ATOM(scrollcorner, "scrollcorner")
XUL_ATOM(slider, "slider")
XUL_ATOM(palettename, "palettename")
XUL_ATOM(fontpicker, "fontpicker")
XUL_ATOM(text, "text")
XUL_ATOM(toolbar, "toolbar")
XUL_ATOM(toolbaritem, "toolbaritem")
XUL_ATOM(toolbarbutton, "toolbarbutton")
XUL_ATOM(toolbox, "toolbox")
XUL_ATOM(image, "image")
XUL_ATOM(validate, "validate")
XUL_ATOM(description, "description")
XUL_ATOM(gripper, "gripper")

  // The tree atoms
XUL_ATOM(allowevents, "allowevents") // Lets events be handled on the cell contents or in menus.
XUL_ATOM(sizemode, "sizemode") // when set, measure strings to determine preferred width

XUL_ATOM(open, "open") // Whether or not a menu, tree, etc. is open
XUL_ATOM(closed, "closed")
XUL_ATOM(focus, "focus")

XUL_ATOM(tree, "tree")
XUL_ATOM(treecols, "treecols")
XUL_ATOM(treecol, "treecol")
XUL_ATOM(treechildren, "treechildren")
XUL_ATOM(treeitem, "treeitem")
XUL_ATOM(treerow, "treerow")
XUL_ATOM(treeseparator, "treeseparator")
XUL_ATOM(treecell, "treecell")

XUL_ATOM(primary, "primary")
XUL_ATOM(cycler, "cycler")
XUL_ATOM(editable, "editable")
XUL_ATOM(current, "current")
XUL_ATOM(seltype, "seltype")
XUL_ATOM(sorted, "sorted")
XUL_ATOM(dragSession, "dragSession")
XUL_ATOM(dropOn, "dropOn")
XUL_ATOM(dropBefore, "dropBefore")
XUL_ATOM(dropAfter, "dropAfter")
XUL_ATOM(checked, "checked")
XUL_ATOM(progressNormal, "progressNormal")
XUL_ATOM(progressUndetermined, "progressUndetermined")
XUL_ATOM(odd, "odd")
XUL_ATOM(even, "even")

XUL_ATOM(menubar, "menubar") // An XP menu bar.
XUL_ATOM(menu, "menu") // Represents an XP menu
XUL_ATOM(menuitem, "menuitem") // Represents an XP menu item
XUL_ATOM(menupopup, "menupopup") // The XP menu's children.
XUL_ATOM(menutobedisplayed, "menutobedisplayed") // The menu is about to be displayed at the next sync w/ frame
XUL_ATOM(menuactive, "_moz-menuactive") // Whether or not a menu is active (without necessarily being open)
XUL_ATOM(accesskey, "accesskey") // The shortcut key for a menu or menu item
XUL_ATOM(acceltext, "acceltext") // Text to use for the accelerator
XUL_ATOM(popupgroup, "popupgroup") // Contains popup menus, context menus, and tooltips
XUL_ATOM(popup, "popup") // The popup for a context menu, popup menu, or tooltip
XUL_ATOM(menugenerated, "menugenerated") // Internal
XUL_ATOM(popupanchor, "popupanchor") // Anchor for popups
XUL_ATOM(popupalign, "popupalign") // Alignment for popups
XUL_ATOM(ignorekeys, "ignorekeys") // Alignment for popups
XUL_ATOM(sizetopopup, "sizetopopup") // Whether or not menus size to their popup children (used by menulists)

XUL_ATOM(key, "key") // The key element / attribute
XUL_ATOM(keycode, "keycode") // The keycode attribute
XUL_ATOM(keytext, "keytext") // The keytext attribute
XUL_ATOM(modifiers, "modifiers") // The modifiers attribute
XUL_ATOM(broadcaster, "broadcaster") // A broadcaster
XUL_ATOM(observes, "observes") // The observes element
XUL_ATOM(templateAtom, "template") // A XUL template

// Bogus atoms that people use that are just data containers
XUL_ATOM(broadcasterset, "broadcasterset")
XUL_ATOM(commands, "commands")
XUL_ATOM(commandset, "commandset")

XUL_ATOM(progressmeter, "progressmeter")
XUL_ATOM(crop, "crop")

XUL_ATOM(mode, "mode")
XUL_ATOM(equalsize, "equalsize")
XUL_ATOM(pack, "pack")
XUL_ATOM(box, "box")
XUL_ATOM(hbox, "hbox")
XUL_ATOM(vbox, "vbox")
XUL_ATOM(scrollbox, "scrollbox")
XUL_ATOM(mousethrough, "mousethrough")
XUL_ATOM(flex, "flex")
XUL_ATOM(ordinal, "ordinal")
XUL_ATOM(spring, "spring")
XUL_ATOM(orient, "orient")
XUL_ATOM(minwidth, "minwidth")
XUL_ATOM(minheight, "minheight")
XUL_ATOM(maxwidth, "maxwidth")
XUL_ATOM(maxheight, "maxheight")

XUL_ATOM(autorepeatbutton, "autorepeatbutton")

XUL_ATOM(bulletinboard, "bulletinboard")

XUL_ATOM(stack, "stack")
XUL_ATOM(deck, "deck")
XUL_ATOM(tabbox, "tabbox")
XUL_ATOM(tab, "tab")
XUL_ATOM(tabpanels, "tabpanels")
XUL_ATOM(tabpanel, "tabpanel")
XUL_ATOM(index, "index")
XUL_ATOM(maxpos, "maxpos")
XUL_ATOM(curpos, "curpos")
XUL_ATOM(smooth, "smooth")
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

XUL_ATOM(fixed, "fixed")

// grid
XUL_ATOM(grid, "grid")
XUL_ATOM(rows, "rows")
XUL_ATOM(columns, "columns")
XUL_ATOM(row, "row")
XUL_ATOM(column, "column")

XUL_ATOM(container, "container")
XUL_ATOM(leaf,"leaf")

XUL_ATOM(widget, "widget")
XUL_ATOM(window, "window")
XUL_ATOM(page, "page")
XUL_ATOM(dialog, "dialog")
XUL_ATOM(wizard, "wizard")

XUL_ATOM(iframe, "iframe")
XUL_ATOM(browser, "browser")
XUL_ATOM(editor, "editor")

// 

XUL_ATOM(control, "control")
XUL_ATOM(checkbox, "checkbox")
XUL_ATOM(radio, "radio")
XUL_ATOM(radiogroup, "radiogroup")
XUL_ATOM(menulist, "menulist")
XUL_ATOM(menubutton, "menubutton")
XUL_ATOM(textbox, "textbox")
XUL_ATOM(textarea, "textarea")
XUL_ATOM(listbox, "listbox")
XUL_ATOM(listcols, "listcols")
XUL_ATOM(listcol, "listcol")
XUL_ATOM(listhead, "listhead")
XUL_ATOM(listheader, "listheader")
XUL_ATOM(listrows, "listrows")
XUL_ATOM(listboxbody, "listboxbody")
XUL_ATOM(listitem, "listitem")
XUL_ATOM(listcell, "listcell")
XUL_ATOM(tooltip, "tooltip")
XUL_ATOM(titletip, "titletip")
XUL_ATOM(tooltiptext, "tooltiptext")
XUL_ATOM(context, "context")
XUL_ATOM(contextmenu, "contextmenu")
XUL_ATOM(style, "style")
XUL_ATOM(selected, "selected")
XUL_ATOM(clazz, "class")
XUL_ATOM(id, "id")
XUL_ATOM(persist, "persist")
XUL_ATOM(ref, "ref")
XUL_ATOM(command, "command")
XUL_ATOM(value, "value")
XUL_ATOM(label, "label")
XUL_ATOM(width, "width")
XUL_ATOM(height, "height")
XUL_ATOM(left, "left")
XUL_ATOM(top, "top")
XUL_ATOM(events, "events")
XUL_ATOM(targets, "targets")
XUL_ATOM(uri, "uri")
XUL_ATOM(empty, "empty")
XUL_ATOM(textnode, "textnode")
XUL_ATOM(rule, "rule")
XUL_ATOM(action, "action")
XUL_ATOM(containment, "containment")
XUL_ATOM(flags, "flags")
XUL_ATOM(Template, "template")
XUL_ATOM(member, "member")
XUL_ATOM(conditions, "conditions")
XUL_ATOM(property, "property")
XUL_ATOM(instanceOf, "instanceOf")
XUL_ATOM(xulcontentsgenerated, "xulcontentsgenerated")
XUL_ATOM(parent, "parent")
XUL_ATOM(iscontainer, "iscontainer")
XUL_ATOM(isempty, "isempty")
XUL_ATOM(bindings, "bindings")
XUL_ATOM(binding, "binding")
XUL_ATOM(triple, "triple")
XUL_ATOM(subject, "subject")
XUL_ATOM(predicate, "predicate")
XUL_ATOM(child, "child")
XUL_ATOM(object, "object")
XUL_ATOM(tag, "tag")
XUL_ATOM(content, "content")
XUL_ATOM(coalesceduplicatearcs, "coalesceduplicatearcs")
XUL_ATOM(allownegativeassertions, "allownegativeassertions")
XUL_ATOM(datasources, "datasources")
XUL_ATOM(statedatasource,"statedatasource")
XUL_ATOM(commandupdater, "commandupdater")
XUL_ATOM(keyset, "keyset")
XUL_ATOM(element, "element")
XUL_ATOM(attribute, "attribute")
XUL_ATOM(overlay, "overlay")
XUL_ATOM(insertbefore, "insertbefore")
XUL_ATOM(insertafter, "insertafter")
XUL_ATOM(position, "position")
XUL_ATOM(removeelement, "removeelement")
XUL_ATOM(blankrow, "blankrow")
XUL_ATOM(titlebar, "titlebar")
XUL_ATOM(resizer, "resizer")
XUL_ATOM(dir, "dir")
XUL_ATOM(properties, "properties")
XUL_ATOM(resource, "resource")
XUL_ATOM(sort, "sort")
XUL_ATOM(sortLocked, "sortLocked")
XUL_ATOM(sortDirection, "sortDirection")
XUL_ATOM(sortActive, "sortActive")
XUL_ATOM(sortResource, "sortResource")
XUL_ATOM(sortResource2, "sortResource2")
XUL_ATOM(sortSeparators, "sortSeparators")
XUL_ATOM(sortStaticsLast, "sortStaticsLast")
XUL_ATOM(selectedIndex, "selectedIndex")
XUL_ATOM(staticHint, "staticHint")
XUL_ATOM(_star, "*")
XUL_ATOM(defaultz, "default")
XUL_ATOM(screenX, "screenX")
XUL_ATOM(screenY, "screenY")
XUL_ATOM(type, "type")
XUL_ATOM(hidechrome, "hidechrome")
XUL_ATOM(popupset, "popupset")
XUL_ATOM(parsetype, "parsetype")
