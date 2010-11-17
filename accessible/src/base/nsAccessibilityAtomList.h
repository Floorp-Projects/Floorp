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
ACCESSIBILITY_ATOM(button, "button")
ACCESSIBILITY_ATOM(checkbox, "checkbox")
ACCESSIBILITY_ATOM(col, "col")
ACCESSIBILITY_ATOM(_empty, "")
ACCESSIBILITY_ATOM(_false, "false")
ACCESSIBILITY_ATOM(image, "image")
ACCESSIBILITY_ATOM(menu, "menu")
ACCESSIBILITY_ATOM(menuButton, "menu-button")
ACCESSIBILITY_ATOM(menugenerated, "menugenerated")
ACCESSIBILITY_ATOM(multiple, "multiple")
ACCESSIBILITY_ATOM(open, "open")
ACCESSIBILITY_ATOM(password, "password")
ACCESSIBILITY_ATOM(radio, "radio")
ACCESSIBILITY_ATOM(reset, "reset")
ACCESSIBILITY_ATOM(row, "row")
ACCESSIBILITY_ATOM(submit, "submit")
ACCESSIBILITY_ATOM(_true, "true")
ACCESSIBILITY_ATOM(_undefined, "undefined")

  // Header info
ACCESSIBILITY_ATOM(headerContentLanguage, "content-language")

  // Alphabetical list of frame types
ACCESSIBILITY_ATOM(areaFrame, "AreaFrame")
ACCESSIBILITY_ATOM(blockFrame, "BlockFrame")
ACCESSIBILITY_ATOM(boxFrame, "BoxFrame")
ACCESSIBILITY_ATOM(brFrame, "BRFrame")
ACCESSIBILITY_ATOM(deckFrame, "DeckFrame")
ACCESSIBILITY_ATOM(inlineBlockFrame, "InlineBlockFrame")
ACCESSIBILITY_ATOM(inlineFrame, "InlineFrame")
ACCESSIBILITY_ATOM(objectFrame, "ObjectFrame")
ACCESSIBILITY_ATOM(scrollFrame, "ScrollFrame")
ACCESSIBILITY_ATOM(textFrame, "TextFrame")
ACCESSIBILITY_ATOM(tableCaptionFrame, "TableCaptionFrame")
ACCESSIBILITY_ATOM(tableCellFrame, "TableCellFrame")
ACCESSIBILITY_ATOM(tableOuterFrame, "TableOuterFrame")
ACCESSIBILITY_ATOM(tableRowGroupFrame, "TableRowGroupFrame")
ACCESSIBILITY_ATOM(tableRowFrame, "TableRowFrame")

  // Alphabetical list of tag names
ACCESSIBILITY_ATOM(a, "a")
ACCESSIBILITY_ATOM(abbr, "abbr")
ACCESSIBILITY_ATOM(acronym, "acronym")
ACCESSIBILITY_ATOM(area, "area")
ACCESSIBILITY_ATOM(article, "article") // HTML landmark
ACCESSIBILITY_ATOM(aside, "aside") // HTML landmark
ACCESSIBILITY_ATOM(autocomplete, "autocomplete")
ACCESSIBILITY_ATOM(blockquote, "blockquote")
ACCESSIBILITY_ATOM(br, "br")
ACCESSIBILITY_ATOM(body, "body")
ACCESSIBILITY_ATOM(caption, "caption") // XUL
ACCESSIBILITY_ATOM(choices, "choices") // XForms
ACCESSIBILITY_ATOM(description, "description")    // XUL
ACCESSIBILITY_ATOM(dd, "dd")
ACCESSIBILITY_ATOM(div, "div")
ACCESSIBILITY_ATOM(dl, "dl")
ACCESSIBILITY_ATOM(dt, "dt")
ACCESSIBILITY_ATOM(footer, "footer") // HTML landmark
ACCESSIBILITY_ATOM(form, "form")
ACCESSIBILITY_ATOM(header, "header") // HTML landmark
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
ACCESSIBILITY_ATOM(legend, "legend")
ACCESSIBILITY_ATOM(li, "li")
ACCESSIBILITY_ATOM(link, "link")
ACCESSIBILITY_ATOM(listcell, "listcell") // XUL
ACCESSIBILITY_ATOM(listcols, "listcols") // XUL
ACCESSIBILITY_ATOM(listcol, "listcol") // XUL
ACCESSIBILITY_ATOM(listhead, "listhead") // XUL
ACCESSIBILITY_ATOM(listheader, "listheader") // XUL
ACCESSIBILITY_ATOM(map, "map")
ACCESSIBILITY_ATOM(math, "math")
ACCESSIBILITY_ATOM(menupopup, "menupopup")     // XUL
ACCESSIBILITY_ATOM(object, "object")
ACCESSIBILITY_ATOM(nav, "nav") // HTML landmark
ACCESSIBILITY_ATOM(ol, "ol")
ACCESSIBILITY_ATOM(optgroup, "optgroup")
ACCESSIBILITY_ATOM(option, "option")
ACCESSIBILITY_ATOM(output, "output")
ACCESSIBILITY_ATOM(panel, "panel") // XUL
ACCESSIBILITY_ATOM(q, "q")
ACCESSIBILITY_ATOM(select, "select")
ACCESSIBILITY_ATOM(select1, "select1") // XForms
ACCESSIBILITY_ATOM(svg, "svg")
ACCESSIBILITY_ATOM(table, "table")
ACCESSIBILITY_ATOM(tabpanels, "tabpanels") // XUL
ACCESSIBILITY_ATOM(tbody, "tbody")
ACCESSIBILITY_ATOM(td, "td")
ACCESSIBILITY_ATOM(th, "th")
ACCESSIBILITY_ATOM(tfoot, "tfoot")
ACCESSIBILITY_ATOM(thead, "thead")
ACCESSIBILITY_ATOM(textarea, "textarea") // XForms
ACCESSIBILITY_ATOM(textbox, "textbox")   // XUL
ACCESSIBILITY_ATOM(toolbaritem, "toolbaritem")   // XUL
ACCESSIBILITY_ATOM(toolbarseparator, "toolbarseparator")   // XUL
ACCESSIBILITY_ATOM(toolbarspring, "toolbarspring")   // XUL
ACCESSIBILITY_ATOM(toolbarspacer, "toolbarspacer")   // XUL
ACCESSIBILITY_ATOM(tooltip, "tooltip")   // XUL
ACCESSIBILITY_ATOM(tr, "tr")
ACCESSIBILITY_ATOM(tree, "tree")
ACCESSIBILITY_ATOM(ul, "ul")

  // Alphabetical list of attributes (DOM)
ACCESSIBILITY_ATOM(acceltext, "acceltext")
ACCESSIBILITY_ATOM(accesskey, "accesskey")
ACCESSIBILITY_ATOM(alt, "alt")
ACCESSIBILITY_ATOM(anonid, "anonid") // Used for ID's in XBL
ACCESSIBILITY_ATOM(checked, "checked")
ACCESSIBILITY_ATOM(contenteditable, "contenteditable")
ACCESSIBILITY_ATOM(control, "control")
ACCESSIBILITY_ATOM(disabled, "disabled")
ACCESSIBILITY_ATOM(_class, "class")
ACCESSIBILITY_ATOM(cycles, "cycles") // used for XUL cycler attribute
ACCESSIBILITY_ATOM(curpos, "curpos") // XUL
ACCESSIBILITY_ATOM(data, "data")
ACCESSIBILITY_ATOM(_default, "default") // XUL button
ACCESSIBILITY_ATOM(draggable, "draggable")
ACCESSIBILITY_ATOM(droppable, "droppable")   // XUL combo box
ACCESSIBILITY_ATOM(editable, "editable")
ACCESSIBILITY_ATOM(_for, "for")
ACCESSIBILITY_ATOM(headers, "headers")   // HTML table
ACCESSIBILITY_ATOM(hidden, "hidden")   // XUL tree columns
ACCESSIBILITY_ATOM(hover, "hover") // XUL color picker
ACCESSIBILITY_ATOM(href, "href") // XUL, XLink
ACCESSIBILITY_ATOM(increment, "increment") // XUL
ACCESSIBILITY_ATOM(lang, "lang")
ACCESSIBILITY_ATOM(linkedPanel, "linkedpanel") // XUL
ACCESSIBILITY_ATOM(longDesc, "longdesc")
ACCESSIBILITY_ATOM(max, "max") // XUL
ACCESSIBILITY_ATOM(maxpos, "maxpos") // XUL
ACCESSIBILITY_ATOM(minpos, "minpos") // XUL
ACCESSIBILITY_ATOM(_moz_menuactive, "_moz-menuactive") // XUL
ACCESSIBILITY_ATOM(multiline, "multiline") // XUL
ACCESSIBILITY_ATOM(name, "name")
ACCESSIBILITY_ATOM(onclick, "onclick")
ACCESSIBILITY_ATOM(popup, "popup")
ACCESSIBILITY_ATOM(placeholder, "placeholder")
ACCESSIBILITY_ATOM(readonly, "readonly")
ACCESSIBILITY_ATOM(scope, "scope") // HTML table
ACCESSIBILITY_ATOM(seltype, "seltype") // XUL listbox
ACCESSIBILITY_ATOM(simple, "simple") // XLink
ACCESSIBILITY_ATOM(src, "src")
ACCESSIBILITY_ATOM(selected, "selected")
ACCESSIBILITY_ATOM(summary, "summary")
ACCESSIBILITY_ATOM(tabindex, "tabindex")
ACCESSIBILITY_ATOM(title, "title")
ACCESSIBILITY_ATOM(toolbarname, "toolbarname")
ACCESSIBILITY_ATOM(tooltiptext, "tooltiptext")
ACCESSIBILITY_ATOM(type, "type")
ACCESSIBILITY_ATOM(usemap, "usemap")
ACCESSIBILITY_ATOM(value, "value")

  // Alphabetical list of object attributes
ACCESSIBILITY_ATOM(checkable, "checkable")
ACCESSIBILITY_ATOM(display, "display")
ACCESSIBILITY_ATOM(eventFromInput, "event-from-input")
ACCESSIBILITY_ATOM(textAlign, "text-align")
ACCESSIBILITY_ATOM(textIndent, "text-indent")

  // Alphabetical list of text attributes (AT API)
ACCESSIBILITY_ATOM(color, "color")
ACCESSIBILITY_ATOM(backgroundColor, "background-color")
ACCESSIBILITY_ATOM(fontFamily, "font-family")
ACCESSIBILITY_ATOM(fontStyle, "font-style")
ACCESSIBILITY_ATOM(fontWeight, "font-weight")
ACCESSIBILITY_ATOM(fontSize, "font-size")
ACCESSIBILITY_ATOM(invalid, "invalid")
ACCESSIBILITY_ATOM(language, "language")
ACCESSIBILITY_ATOM(textLineThroughStyle, "text-line-through-style")
ACCESSIBILITY_ATOM(textUnderlineStyle, "text-underline-style")
ACCESSIBILITY_ATOM(textPosition, "text-position")

  // ARIA (DHTML accessibility) attributes
  // Also add to nsARIAMap.cpp and nsARIAMap.h
  // ARIA role attribute
ACCESSIBILITY_ATOM(role, "role")
ACCESSIBILITY_ATOM(aria_activedescendant, "aria-activedescendant")
ACCESSIBILITY_ATOM(aria_atomic, "aria-atomic")
ACCESSIBILITY_ATOM(aria_autocomplete, "aria-autocomplete")
ACCESSIBILITY_ATOM(aria_busy, "aria-busy")
ACCESSIBILITY_ATOM(aria_checked, "aria-checked")
ACCESSIBILITY_ATOM(aria_controls, "aria-controls")
ACCESSIBILITY_ATOM(aria_describedby, "aria-describedby")
ACCESSIBILITY_ATOM(aria_disabled, "aria-disabled")
ACCESSIBILITY_ATOM(aria_dropeffect, "aria-dropeffect")
ACCESSIBILITY_ATOM(aria_expanded, "aria-expanded")
ACCESSIBILITY_ATOM(aria_flowto, "aria-flowto")
ACCESSIBILITY_ATOM(aria_grabbed, "aria-grabbed")
ACCESSIBILITY_ATOM(aria_haspopup, "aria-haspopup")
ACCESSIBILITY_ATOM(aria_invalid, "aria-invalid")
ACCESSIBILITY_ATOM(aria_label, "aria-label")
ACCESSIBILITY_ATOM(aria_labelledby, "aria-labelledby")
ACCESSIBILITY_ATOM(aria_level, "aria-level")
ACCESSIBILITY_ATOM(aria_live, "aria-live")
ACCESSIBILITY_ATOM(aria_multiline, "aria-multiline")
ACCESSIBILITY_ATOM(aria_multiselectable, "aria-multiselectable")
ACCESSIBILITY_ATOM(aria_orientation, "aria-orientation")
ACCESSIBILITY_ATOM(aria_owns, "aria-owns")
ACCESSIBILITY_ATOM(aria_posinset, "aria-posinset")
ACCESSIBILITY_ATOM(aria_pressed, "aria-pressed")
ACCESSIBILITY_ATOM(aria_readonly, "aria-readonly")
ACCESSIBILITY_ATOM(aria_relevant, "aria-relevant")
ACCESSIBILITY_ATOM(aria_required, "aria-required")
ACCESSIBILITY_ATOM(aria_selected, "aria-selected")
ACCESSIBILITY_ATOM(aria_setsize, "aria-setsize")
ACCESSIBILITY_ATOM(aria_sort, "aria-sort")
ACCESSIBILITY_ATOM(aria_valuenow, "aria-valuenow")
ACCESSIBILITY_ATOM(aria_valuemin, "aria-valuemin")
ACCESSIBILITY_ATOM(aria_valuemax, "aria-valuemax")
ACCESSIBILITY_ATOM(aria_valuetext, "aria-valuetext")

  // misc atoms
// a form property used to obtain the default label
// of an HTML button from the button frame
ACCESSIBILITY_ATOM(defaultLabel, "defaultLabel")
// the attribute specifying the editor's bogus br node
ACCESSIBILITY_ATOM(mozeditorbogusnode, "_moz_editor_bogus_node")

// Object attributes
ACCESSIBILITY_ATOM(tableCellIndex, "table-cell-index")
ACCESSIBILITY_ATOM(containerAtomic, "container-atomic")
ACCESSIBILITY_ATOM(containerBusy, "container-busy")
ACCESSIBILITY_ATOM(containerLive, "container-live")
ACCESSIBILITY_ATOM(containerLiveRole, "container-live-role")
ACCESSIBILITY_ATOM(containerRelevant, "container-relevant")
ACCESSIBILITY_ATOM(level, "level")
ACCESSIBILITY_ATOM(live, "live")
ACCESSIBILITY_ATOM(lineNumber, "line-number")
ACCESSIBILITY_ATOM(posinset, "posinset") 
ACCESSIBILITY_ATOM(setsize, "setsize")
ACCESSIBILITY_ATOM(xmlroles, "xml-roles")
