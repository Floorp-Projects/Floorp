/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _role_h_
#define _role_h_

/**
 * @note Make sure to update the localized role names when changing the list.
 * @note When adding a new role, be sure to also add it to base/RoleMap.h and
 *       update nsIAccessibleRole.
 */

namespace mozilla {
namespace a11y {
namespace roles {

enum Role {
  /**
   * Used when the accessible has no strongly-defined role.
   */
  NOTHING = 0,

  /**
   * Represents the menu bar (positioned beneath the title bar of a window)
   * from which menus are selected by the user. The role is used by
   * xul:menubar or role="menubar".
   */
  MENUBAR = 1,

  /**
   * Represents a vertical or horizontal scroll bar, which is part of the client
   * area or used in a control.
   */
  SCROLLBAR = 2,

  /**
   * Represents an alert or a condition that a user should be notified about.
   * Assistive Technologies typically respond to the role by reading the entire
   * onscreen contents of containers advertising this role. Should be used for
   * warning dialogs, etc. The role is used by xul:browsermessage,
   * role="alert".
   */
  ALERT = 3,

  /**
   * A sub-document (<frame> or <iframe>)
   */
  INTERNAL_FRAME = 4,

  /**
   * Represents a menu, which presents a list of options from which the user can
   * make a selection to perform an action. It is used for role="menu".
   */
  MENUPOPUP = 5,

  /**
   * Represents a menu item, which is an entry in a menu that a user can choose
   * to carry out a command, select an option. It is used for xul:menuitem,
   * role="menuitem".
   */
  MENUITEM = 6,

  /**
   * Represents a ToolTip that provides helpful hints.
   */
  TOOLTIP = 7,

  /**
   * Represents a main window for an application. It is used for
   * role="application". Also refer to APP_ROOT
   */
  APPLICATION = 8,

  /**
   * Represents a document window. A document window is always contained within
   * an application window. For role="document", see NON_NATIVE_DOCUMENT.
   */
  DOCUMENT = 9,

  /**
   * Represents a pane within a frame or document window. Users can navigate
   * between panes and within the contents of the current pane, but cannot
   * navigate between items in different panes. Thus, panes represent a level
   * of grouping lower than frame windows or documents, but above individual
   * controls. It is used for the first child of a <frame> or <iframe>.
   */
  PANE = 10,

  /**
   * Represents a dialog box or message box. It is used for xul:dialog,
   * role="dialog".
   */
  DIALOG = 11,

  /**
   * Logically groups other objects. There is not always a parent-child
   * relationship between the grouping object and the objects it contains. It
   * is used for html:textfield, xul:groupbox, role="group".
   */
  GROUPING = 12,

  /**
   * Used to visually divide a space into two regions, such as a separator menu
   * item or a bar that divides split panes within a window. It is used for
   * xul:separator, html:hr, role="separator".
   */
  SEPARATOR = 13,

  /**
   * Represents a toolbar, which is a grouping of controls (push buttons or
   * toggle buttons) that provides easy access to frequently used features. It
   * is used for xul:toolbar, role="toolbar".
   */
  TOOLBAR = 14,

  /**
   * Represents a status bar, which is an area at the bottom of a window that
   * displays information about the current operation, state of the application,
   * or selected object. The status bar has multiple fields, which display
   * different kinds of information. It is used for xul:statusbar.
   */
  STATUSBAR = 15,

  /**
   * Represents a table that contains rows and columns of cells, and optionally,
   * row headers and column headers. It is used for html:table,
   * role="grid". Also refer to the following role: COLUMNHEADER,
   * ROWHEADER, ROW, CELL.
   */
  TABLE = 16,

  /**
   * Represents a column header, providing a visual label for a column in
   * a table. It is used for XUL tree column headers, html:th,
   * role="colheader". Also refer to TABLE.
   */
  COLUMNHEADER = 17,

  /**
   * Represents a row header, which provides a visual label for a table row.
   * It is used for role="rowheader". Also, see TABLE.
   */
  ROWHEADER = 18,

  /**
   * Represents a row of cells within a table. Also, see TABLE.
   */
  ROW = 19,

  /**
   * Represents a cell within a table. It is used for html:td and xul:tree cell.
   * Also, see TABLE.
   */
  CELL = 20,

  /**
   * Represents a link to something else. This object might look like text or
   * a graphic, but it acts like a button. It is used for
   * xul:label@class="text-link", html:a, html:area.
   */
  LINK = 21,

  /**
   * Represents a list box, allowing the user to select one or more items. It
   * is used for xul:listbox, html:select@size, role="list". See also
   * LIST_ITEM.
   */
  LIST = 22,

  /**
   * Represents an item in a list. See also LIST.
   */
  LISTITEM = 23,

  /**
   * Represents an outline or tree structure, such as a tree view control,
   * that displays a hierarchical list and allows the user to expand and
   * collapse branches. Is is used for role="tree".
   */
  OUTLINE = 24,

  /**
   * Represents an item in an outline or tree structure. It is used for
   * role="treeitem".
   */
  OUTLINEITEM = 25,

  /**
   * Represents a page tab, it is a child of a page tab list. It is used for
   * xul:tab, role="treeitem". Also refer to PAGETABLIST.
   */
  PAGETAB = 26,

  /**
   * Represents a property sheet. It is used for xul:tabpanel,
   * role="tabpanel".
   */
  PROPERTYPAGE = 27,

  /**
   * Represents a picture. Is is used for xul:image, html:img.
   */
  GRAPHIC = 28,

  /**
   * Represents read-only text, such as labels for other controls or
   * instructions in a dialog box. Static text cannot be modified or selected.
   * Is is used for xul:label, xul:description, html:label, role="label".
   */
  STATICTEXT = 29,

  /**
   * Represents selectable text that allows edits or is designated read-only.
   */
  TEXT_LEAF = 30,

  /**
   * Represents a push button control. It is used for xul:button, html:button,
   * role="button".
   */
  PUSHBUTTON = 31,

  /**
   * Represents a check box control. It is used for xul:checkbox,
   * html:input@type="checkbox", role="checkbox".
   */
  CHECKBUTTON = 32,

  /**
   * Represents an option button, also called a radio button. It is one of a
   * group of mutually exclusive options. All objects sharing a single parent
   * that have this attribute are assumed to be part of single mutually
   * exclusive group. It is used for xul:radio, html:input@type="radio",
   * role="radio".
   */
  RADIOBUTTON = 33,

  /**
   * Represents a combo box; a popup button with an associated list box that
   * provides a set of predefined choices. It is used for html:select with a
   * size of 1 and xul:menulist. See also ROLE_EDITCOMBOBOX.
   */
  COMBOBOX = 34,

  /**
   * Represents a progress bar, dynamically showing the user the percent
   * complete of an operation in progress. It is used for html:progress,
   * role="progressbar".
   */
  PROGRESSBAR = 35,

  /**
   * Represents a slider, which allows the user to adjust a setting in given
   * increments between minimum and maximum values. It is used by xul:scale,
   * role="slider".
   */
  SLIDER = 36,

  /**
   * Represents a spin box, which is a control that allows the user to increment
   * or decrement the value displayed in a separate "buddy" control associated
   * with the spin box. It is used for input[type=number] spin buttons.
   */
  SPINBUTTON = 37,

  /**
   * Represents a graphical image used to diagram data. It is used for svg:svg.
   */
  DIAGRAM = 38,

  /**
   * Represents an animation control, which contains content that changes over
   * time, such as a control that displays a series of bitmap frames.
   */
  ANIMATION = 39,

  /**
   * Represents a button that drops down a list of items.
   */
  BUTTONDROPDOWN = 40,

  /**
   * Represents a button that drops down a menu.
   */
  BUTTONMENU = 41,

  /**
   * Represents blank space between other objects.
   */
  WHITESPACE = 42,

  /**
   * Represents a container of page tab controls. Is it used for xul:tabs,
   * DHTML: role="tabs". Also refer to PAGETAB.
   */
  PAGETABLIST = 43,

  /**
   * Represents a control that can be drawn into and is used to trap events.
   * It is used for html:canvas.
   */
  CANVAS = 44,

  /**
   * Represents a menu item with a check box.
   */
  CHECK_MENU_ITEM = 45,

  /**
   * Represents control whose purpose is to allow a user to edit a date.
   */
  DATE_EDITOR = 46,

  /**
   * Frame role. A top level window with a title bar, border, menu bar, etc.
   * It is often used as the primary window for an application.
   */
  CHROME_WINDOW = 47,

  /**
   * Presents an icon or short string in an interface.
   */
  LABEL = 48,

  /**
   * A text object uses for passwords, or other places where the text content
   * is not shown visibly to the user.
   */
  PASSWORD_TEXT = 49,

  /**
   * A radio button that is a menu item.
   */
  RADIO_MENU_ITEM = 50,

  /**
   * Collection of objects that constitute a logical text entity.
   */
  TEXT_CONTAINER = 51,

  /**
   * A toggle button. A specialized push button that can be checked or
   * unchecked, but does not provide a separate indicator for the current state.
   */
  TOGGLE_BUTTON = 52,

  /**
   * Represent a control that is capable of expanding and collapsing rows as
   * well as showing multiple columns of data.
   */
  TREE_TABLE = 53,

  /**
   * A paragraph of text.
   */
  PARAGRAPH = 54,

  /**
   * An control whose textual content may be entered or modified by the user.
   */
  ENTRY = 55,

  /**
   * A caption describing another object.
   */
  CAPTION = 56,

  /**
   * An element containing content that assistive technology users may want to
   * browse in a reading mode, rather than a focus/interactive/application mode.
   * This role is used for role="document". For the container which holds the
   * content of a web page, see DOCUMENT.
   */
  NON_NATIVE_DOCUMENT = 57,

  /**
   * Heading.
   */
  HEADING = 58,

  /**
   * A container of document content.  An example of the use of this role is to
   * represent an html:div.
   */
  SECTION = 59,

  /**
   * A container of form controls. An example of the use of this role is to
   * represent an html:form.
   */
  FORM = 60,

  /**
   * XXX: document this.
   */
  APP_ROOT = 61,

  /**
   * Represents a menu item, which is an entry in a menu that a user can choose
   * to display another menu.
   */
  PARENT_MENUITEM = 62,

  /**
   * A list of items that is shown by combobox.
   */
  COMBOBOX_LIST = 63,

  /**
   * A item of list that is shown by combobox.
   */
  COMBOBOX_OPTION = 64,

  /**
   * An image map -- has child links representing the areas
   */
  IMAGE_MAP = 65,

  /**
   * An option in a listbox
   */
  OPTION = 66,

  /**
   * A rich option in a listbox, it can have other widgets as children
   */
  RICH_OPTION = 67,

  /**
   * A list of options
   */
  LISTBOX = 68,

  /**
   * Represents a mathematical equation in the accessible name
   */
  FLAT_EQUATION = 69,

  /**
   * Represents a cell within a grid. It is used for role="gridcell". Unlike
   * CELL, it allows the calculation of the accessible name from subtree.
   * Also, see TABLE.
   */
  GRID_CELL = 70,

  /**
   * A note. Originally intended to be hidden until activated, but now also used
   * for things like html 'aside'.
   */
  NOTE = 71,

  /**
   * A figure. Used for things like HTML5 figure element.
   */
  FIGURE = 72,

  /**
   * Represents a rich item with a check box.
   */
  CHECK_RICH_OPTION = 73,

  /**
   * Represent a definition list (dl in HTML).
   */
  DEFINITION_LIST = 74,

  /**
   * Represent a term in a definition list (dt in HTML).
   */
  TERM = 75,

  /**
   * Represent a definition in a definition list (dd in HTML)
   */
  DEFINITION = 76,

  /**
   * Represent a keyboard or keypad key (ARIA role "key").
   */
  KEY = 77,

  /**
   * Represent a switch control widget (ARIA role "switch").
   */
  SWITCH = 78,

  /**
   * A block of MathML code (math).
   */
  MATHML_MATH = 79,

  /**
   * A MathML identifier (mi in MathML).
   */
  MATHML_IDENTIFIER = 80,

  /**
   * A MathML number (mn in MathML).
   */
  MATHML_NUMBER = 81,

  /**
   * A MathML operator (mo in MathML).
   */
  MATHML_OPERATOR = 82,

  /**
   * A MathML text (mtext in MathML).
   */
  MATHML_TEXT = 83,

  /**
   * A MathML string literal (ms in MathML).
   */
  MATHML_STRING_LITERAL = 84,

  /**
   * A MathML glyph (mglyph in MathML).
   */
  MATHML_GLYPH = 85,

  /**
   * A MathML row (mrow in MathML).
   */
  MATHML_ROW = 86,

  /**
   * A MathML fraction (mfrac in MathML).
   */
  MATHML_FRACTION = 87,

  /**
   * A MathML square root (msqrt in MathML).
   */
  MATHML_SQUARE_ROOT = 88,

  /**
   * A MathML root (mroot in MathML).
   */
  MATHML_ROOT = 89,

  /**
   * A MathML enclosed element (menclose in MathML).
   */
  MATHML_ENCLOSED = 90,

  /**
   * A MathML styling element (mstyle in MathML).
   */
  MATHML_STYLE = 91,

  /**
   * A MathML subscript (msub in MathML).
   */
  MATHML_SUB = 92,

  /**
   * A MathML superscript (msup in MathML).
   */
  MATHML_SUP = 93,

  /**
   * A MathML subscript and superscript (msubsup in MathML).
   */
  MATHML_SUB_SUP = 94,

  /**
   * A MathML underscript (munder in MathML).
   */
  MATHML_UNDER = 95,

  /**
   * A MathML overscript (mover in MathML).
   */
  MATHML_OVER = 96,

  /**
   * A MathML underscript and overscript (munderover in MathML).
   */
  MATHML_UNDER_OVER = 97,

  /**
   * A MathML multiple subscript and superscript element (mmultiscripts in
   * MathML).
   */
  MATHML_MULTISCRIPTS = 98,

  /**
   * A MathML table (mtable in MathML).
   */
  MATHML_TABLE = 99,

  /**
   * A MathML labelled table row (mlabeledtr in MathML).
   */
  MATHML_LABELED_ROW = 100,

  /**
   * A MathML table row (mtr in MathML).
   */
  MATHML_TABLE_ROW = 101,

  /**
   * A MathML table entry or cell (mtd in MathML).
   */
  MATHML_CELL = 102,

  /**
   * A MathML interactive element (maction in MathML).
   */
  MATHML_ACTION = 103,

  /**
   * A MathML error message (merror in MathML).
   */
  MATHML_ERROR = 104,

  /**
   * A MathML stacked (rows of numbers) element (mstack in MathML).
   */
  MATHML_STACK = 105,

  /**
   * A MathML long division element (mlongdiv in MathML).
   */
  MATHML_LONG_DIVISION = 106,

  /**
   * A MathML stack group (msgroup in MathML).
   */
  MATHML_STACK_GROUP = 107,

  /**
   * A MathML stack row (msrow in MathML).
   */
  MATHML_STACK_ROW = 108,

  /**
   * MathML carries, borrows, or crossouts for a row (mscarries in MathML).
   */
  MATHML_STACK_CARRIES = 109,

  /**
   * A MathML carry, borrow, or crossout for a column (mscarry in MathML).
   */
  MATHML_STACK_CARRY = 110,

  /**
   * A MathML line in a stack (msline in MathML).
   */
  MATHML_STACK_LINE = 111,

  /**
   * A group containing radio buttons
   */
  RADIO_GROUP = 112,

  /**
   * A text container exposing brief amount of information. See related
   * TEXT_CONTAINER role.
   */
  TEXT = 113,

  /**
   * The html:details element.
   */
  DETAILS = 114,

  /**
   * The html:summary element.
   */
  SUMMARY = 115,

  /**
   * An ARIA landmark. See related NAVIGATION role.
   */
  LANDMARK = 116,

  /**
   * A specific type of ARIA landmark. The ability to distinguish navigation
   * landmarks from other types of landmarks is, for example, needed on macOS
   * where specific AXSubrole and AXRoleDescription for navigation landmarks
   * are used.
   */
  NAVIGATION = 117,

  /**
   * An object that contains the text of a footnote.
   */
  FOOTNOTE = 118,

  /**
   * A complete or self-contained composition in a document, page, application,
   * or site and that is, in principle, independently distributable or reusable,
   * e.g. in syndication.
   */
  ARTICLE = 119,

  /**
   * A perceivable section containing content that is relevant to a specific,
   * author-specified purpose and sufficiently important that users will likely
   * want to be able to navigate to the section easily and to have it listed in
   * a summary of the page.
   */
  REGION = 120,

  /**
   * Represents a control with a text input and a popup with a set of predefined
   * choices. It is used for ARIA's combobox role. See also COMBOBOX.
   */
  EDITCOMBOBOX = 121,

  /**
   * A section of content that is quoted from another source.
   */
  BLOCKQUOTE = 122,

  /**
   * Content previously deleted or proposed for deletion, e.g. in revision
   * history or a content view providing suggestions from reviewers.
   */
  CONTENT_DELETION = 123,

  /**
   * Content previously inserted or proposed for insertion, e.g. in revision
   * history or a content view providing suggestions from reviewers.
   */
  CONTENT_INSERTION = 124,

  /**
   * An html:form element with a label provided by WAI-ARIA.
   * This may also be used if role="form" with a label should be exposed
   * differently in the future.
   */
  FORM_LANDMARK = 125,

  /**
   * The html:mark element.
   * This is also used for the equivalent WAI-ARIA role.
   */
  MARK = 126,

  /**
   * The WAI-ARIA suggestion role.
   */
  SUGGESTION = 127,

  /**
   * The WAI-ARIA comment role.
   */
  COMMENT = 128,

  /**
   * A snippet of program code. ATs might want to treat this differently.
   */
  CODE = 129,

  /**
   * Represents control whose purpose is to allow a user to edit a time.
   */
  TIME_EDITOR = 130,

  /**
   * Represents the marker associated with a list item. In unordered lists,
   * this is a bullet, while in ordered lists this is a number.
   */
  LISTITEM_MARKER = 131,

  /**
   * Essentially, this is a progress bar with a contextually defined
   * scale, ex. the strength of a password entered in an input.
   */
  METER = 132,

  /**
   * Represents phrasing content that is presented with vertical alignment
   * lower than the baseline and a smaller font size. For example, the "2" in
   * the chemical formula H2O.
   */
  SUBSCRIPT = 133,

  /**
   * Represents phrasing content that is presented with vertical alignment
   * higher than the baseline and a smaller font size. For example, the
   * exponent in a math expression.
   */
  SUPERSCRIPT = 134,

  LAST_ROLE = SUPERSCRIPT
};

}  // namespace roles

typedef enum mozilla::a11y::roles::Role role;

}  // namespace a11y
}  // namespace mozilla

#endif
