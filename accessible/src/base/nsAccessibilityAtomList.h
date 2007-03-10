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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: Aaron Leventhal (aaronl@netscape.com)
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

  This file contains the list of all accessibility nsIAtoms and their values

  It is designed to be used as inline input to nsAccessibilityAtoms.cpp *only*
  through the magic of C preprocessing.

  All entires must be enclosed in the macro ACCESSIBILITY_ATOM which will have cruel
  and unusual things done to it

  It is recommended (but not strictly necessary) to keep all entries
  in alphabetical order

  The first argument to ACCESSIBILITY_ATOM is the C++ identifier of the atom
  The second argument is the string value of the atom

 ******/


  // Alphabetical list of generic atoms
ACCESSIBILITY_ATOM(_empty, "")
ACCESSIBILITY_ATOM(button, "button")
ACCESSIBILITY_ATOM(_false, "false")
ACCESSIBILITY_ATOM(image, "image")
ACCESSIBILITY_ATOM(password, "password")
ACCESSIBILITY_ATOM(reset, "reset")
ACCESSIBILITY_ATOM(submit, "submit")
ACCESSIBILITY_ATOM(_true, "true")

  // Header info
ACCESSIBILITY_ATOM(headerContentLanguage, "content-language")

  // Alphabetical list of frame types
ACCESSIBILITY_ATOM(areaFrame, "AreaFrame")
ACCESSIBILITY_ATOM(blockFrame, "BlockFrame")
ACCESSIBILITY_ATOM(brFrame, "BRFrame")
ACCESSIBILITY_ATOM(inlineBlockFrame, "InlineBlockFrame")
ACCESSIBILITY_ATOM(inlineFrame, "InlineFrame")
ACCESSIBILITY_ATOM(objectFrame, "ObjectFrame")
ACCESSIBILITY_ATOM(textFrame, "TextFrame")
ACCESSIBILITY_ATOM(tableCellFrame, "TableCellFrame")
ACCESSIBILITY_ATOM(tableOuterFrame, "TableOuterFrame")

  // Alphabetical list of tag names
ACCESSIBILITY_ATOM(a, "a")
ACCESSIBILITY_ATOM(abbr, "abbr")
ACCESSIBILITY_ATOM(acronym, "acronym")
ACCESSIBILITY_ATOM(area, "area")
ACCESSIBILITY_ATOM(blockquote, "blockquote")
ACCESSIBILITY_ATOM(br, "br")
ACCESSIBILITY_ATOM(body, "body")
ACCESSIBILITY_ATOM(caption, "caption")
ACCESSIBILITY_ATOM(choices, "choices") // XForms
ACCESSIBILITY_ATOM(description, "description")    // XUL
ACCESSIBILITY_ATOM(dd, "dd")
ACCESSIBILITY_ATOM(div, "div")
ACCESSIBILITY_ATOM(dl, "dl")
ACCESSIBILITY_ATOM(dt, "dt")
ACCESSIBILITY_ATOM(form, "form")
ACCESSIBILITY_ATOM(h1, "h1")
ACCESSIBILITY_ATOM(h2, "h2")
ACCESSIBILITY_ATOM(h3, "h3")
ACCESSIBILITY_ATOM(h4, "h4")
ACCESSIBILITY_ATOM(h5, "h5")
ACCESSIBILITY_ATOM(h6, "h6")
ACCESSIBILITY_ATOM(item, "item") // XForms
ACCESSIBILITY_ATOM(itemset, "itemset") // XForms
ACCESSIBILITY_ATOM(img, "img")
ACCESSIBILITY_ATOM(input, "input")
ACCESSIBILITY_ATOM(label, "label")
ACCESSIBILITY_ATOM(li, "li")
ACCESSIBILITY_ATOM(link, "link")
ACCESSIBILITY_ATOM(map, "map")
ACCESSIBILITY_ATOM(math, "math")
ACCESSIBILITY_ATOM(menu, "menu")    // XUL
ACCESSIBILITY_ATOM(menupopup, "menupopup")     // XUL
ACCESSIBILITY_ATOM(object, "object")
ACCESSIBILITY_ATOM(ol, "ol")
ACCESSIBILITY_ATOM(optgroup, "optgroup")
ACCESSIBILITY_ATOM(option, "option")
ACCESSIBILITY_ATOM(q, "q")
ACCESSIBILITY_ATOM(select, "select")
ACCESSIBILITY_ATOM(select1, "select1") // XForms
ACCESSIBILITY_ATOM(svg, "svg")
ACCESSIBILITY_ATOM(table, "table")
ACCESSIBILITY_ATOM(tbody, "tbody")
ACCESSIBILITY_ATOM(td, "td")
ACCESSIBILITY_ATOM(th, "th")
ACCESSIBILITY_ATOM(tfoot, "tfoot")
ACCESSIBILITY_ATOM(thead, "thead")
ACCESSIBILITY_ATOM(textarea, "textarea") // XForms
ACCESSIBILITY_ATOM(textbox, "textbox")   // XUL
ACCESSIBILITY_ATOM(toolbaritem, "toolbaritem")   // XUL
ACCESSIBILITY_ATOM(tooltip, "tooltip")   // XUL
ACCESSIBILITY_ATOM(tr, "tr")
ACCESSIBILITY_ATOM(ul, "ul")

  // DHTML accessibility relationship attributes
ACCESSIBILITY_ATOM(controls, "controls")
ACCESSIBILITY_ATOM(describedby, "describedby")
ACCESSIBILITY_ATOM(flowto, "flowto")
ACCESSIBILITY_ATOM(labelledby, "labelledby")

  // Alphabetical list of attributes
ACCESSIBILITY_ATOM(acceltext, "acceltext")
ACCESSIBILITY_ATOM(accesskey, "accesskey")
ACCESSIBILITY_ATOM(alt, "alt")
ACCESSIBILITY_ATOM(control, "control")
ACCESSIBILITY_ATOM(data, "data")
ACCESSIBILITY_ATOM(disabled, "disabled")
ACCESSIBILITY_ATOM(editable, "editable")
ACCESSIBILITY_ATOM(_for, "for")
ACCESSIBILITY_ATOM(href, "href")
ACCESSIBILITY_ATOM(id, "id")
ACCESSIBILITY_ATOM(lang, "lang")
ACCESSIBILITY_ATOM(multiline, "multiline")
ACCESSIBILITY_ATOM(name, "name")
ACCESSIBILITY_ATOM(onclick, "onclick")
ACCESSIBILITY_ATOM(readonly, "readonly")
ACCESSIBILITY_ATOM(src, "src")
ACCESSIBILITY_ATOM(summary, "summary")
ACCESSIBILITY_ATOM(tabindex, "tabindex")
ACCESSIBILITY_ATOM(title, "title")
ACCESSIBILITY_ATOM(tooltiptext, "tooltiptext")
ACCESSIBILITY_ATOM(type, "type")
ACCESSIBILITY_ATOM(value, "value")

  // ARIA (DHTML accessibility) attributes
ACCESSIBILITY_ATOM(activedescendant, "activedescendant")
ACCESSIBILITY_ATOM(checked, "checked")
ACCESSIBILITY_ATOM(droppable, "droppable")
ACCESSIBILITY_ATOM(expanded, "expanded")
ACCESSIBILITY_ATOM(invalid, "invalid")
ACCESSIBILITY_ATOM(level, "level")
ACCESSIBILITY_ATOM(multiselectable, "multiselectable")
ACCESSIBILITY_ATOM(posinset, "posinset")
ACCESSIBILITY_ATOM(required, "required")
ACCESSIBILITY_ATOM(role, "role")
ACCESSIBILITY_ATOM(selected, "selected")
ACCESSIBILITY_ATOM(setsize, "setsize")
ACCESSIBILITY_ATOM(valuenow, "valuenow")    // For DHTML widget values
ACCESSIBILITY_ATOM(valuemin, "valuemin")
ACCESSIBILITY_ATOM(valuemax, "valuemax")
ACCESSIBILITY_ATOM(hidden, "hidden")

  // misc atoms
// a form property used to obtain the default label
// of an HTML button from the button frame
ACCESSIBILITY_ATOM(defaultLabel, "defaultLabel")
