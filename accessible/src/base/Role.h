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
   * Used when accessible hans't strong defined role.
   */
  NOTHING = 0,

  /**
   * Represents a title or caption bar for a window. It is used by MSAA only,
   * supported automatically by MS Windows.
   */
  TITLEBAR = 1,

  /**
   * Represents the menu bar (positioned beneath the title bar of a window)
   * from which menus are selected by the user. The role is used by
   * xul:menubar or role="menubar".
   */
  MENUBAR = 2,

  /**
   * Represents a vertical or horizontal scroll bar, which is part of the client
   * area or used in a control.
   */
  SCROLLBAR = 3,

  /**
   * Represents a special mouse pointer, which allows a user to manipulate user
   * interface elements such as windows. For example, a user clicks and drags
   * a sizing grip in the lower-right corner of a window to resize it.
   */
  GRIP = 4,

  /**
   * Represents a system sound, which is associated with various system events.
   */
  SOUND = 5,

  /**
   * Represents the system mouse pointer.
   */
  CURSOR = 6,

  /**
   * Represents the system caret. The role is supported for caret.
   */
  CARET = 7,

  /**
   * Represents an alert or a condition that a user should be notified about.
   * Assistive Technologies typically respond to the role by reading the entire
   * onscreen contents of containers advertising this role. Should be used for
   * warning dialogs, etc. The role is used by xul:browsermessage,
   * role="alert", xforms:message.
   */
  ALERT = 8,

  /**
   * Represents the window frame, which contains child objects such as
   * a title bar, client, and other objects contained in a window. The role
   * is supported automatically by MS Windows.
   */
  WINDOW = 9,

  /**
   * A sub-document (<frame> or <iframe>)
   */
  INTERNAL_FRAME = 10,

  /**
   * Represents a menu, which presents a list of options from which the user can
   * make a selection to perform an action. It is used for role="menu".
   */
  MENUPOPUP = 11,

  /**
   * Represents a menu item, which is an entry in a menu that a user can choose
   * to carry out a command, select an option. It is used for xul:menuitem,
   * role="menuitem".
   */
  MENUITEM = 12,

  /**
   * Represents a ToolTip that provides helpful hints.
   */
  TOOLTIP = 13,

  /**
   * Represents a main window for an application. It is used for
   * role="application". Also refer to APP_ROOT
   */
  APPLICATION = 14,

  /**
   * Represents a document window. A document window is always contained within
   * an application window. It is used for role="document".
   */
  DOCUMENT = 15,

  /**
   * Represents a pane within a frame or document window. Users can navigate
   * between panes and within the contents of the current pane, but cannot
   * navigate between items in different panes. Thus, panes represent a level
   * of grouping lower than frame windows or documents, but above individual
   * controls. It is used for the first child of a <frame> or <iframe>.
   */
  PANE = 16,

  /**
   * Represents a graphical image used to represent data.
   */
  CHART = 17,

  /**
   * Represents a dialog box or message box. It is used for xul:dialog, 
   * role="dialog".
   */
  DIALOG = 18,

  /**
   * Represents a window border.
   */
  BORDER = 19,

  /**
   * Logically groups other objects. There is not always a parent-child
   * relationship between the grouping object and the objects it contains. It
   * is used for html:textfield, xul:groupbox, role="group".
   */
  GROUPING = 20,

  /**
   * Used to visually divide a space into two regions, such as a separator menu
   * item or a bar that divides split panes within a window. It is used for
   * xul:separator, html:hr, role="separator".
   */
  SEPARATOR = 21,

  /**
   * Represents a toolbar, which is a grouping of controls (push buttons or
   * toggle buttons) that provides easy access to frequently used features. It
   * is used for xul:toolbar, role="toolbar".
   */
  TOOLBAR = 22,

  /**
   * Represents a status bar, which is an area at the bottom of a window that
   * displays information about the current operation, state of the application,
   * or selected object. The status bar has multiple fields, which display
   * different kinds of information. It is used for xul:statusbar.
   */
  STATUSBAR = 23,

  /**
   * Represents a table that contains rows and columns of cells, and optionally,
   * row headers and column headers. It is used for html:table,
   * role="grid". Also refer to the following role: COLUMNHEADER,
   * ROWHEADER, COLUMN, ROW, CELL.
   */
  TABLE = 24,

  /**
   * Represents a column header, providing a visual label for a column in
   * a table. It is used for XUL tree column headers, html:th,
   * role="colheader". Also refer to TABLE.
   */
  COLUMNHEADER = 25,

  /**
   * Represents a row header, which provides a visual label for a table row.
   * It is used for role="rowheader". Also, see TABLE.
   */
  ROWHEADER = 26,

  /**
   * Represents a column of cells within a table. Also, see TABLE.
   */
  COLUMN = 27,

  /**
   * Represents a row of cells within a table. Also, see TABLE.
   */
  ROW = 28,

  /**
   * Represents a cell within a table. It is used for html:td,
   * xul:tree cell and xul:listcell. Also, see TABLE.
   */
  CELL = 29,

  /**
   * Represents a link to something else. This object might look like text or
   * a graphic, but it acts like a button. It is used for
   * xul:label@class="text-link", html:a, html:area,
   * xforms:trigger@appearance="minimal".
   */
  LINK = 30,

  /**
   * Displays a Help topic in the form of a ToolTip or Help balloon.
   */
  HELPBALLOON = 31,

  /**
   * Represents a cartoon-like graphic object, such as Microsoft Office
   * Assistant, which is displayed to provide help to users of an application.
   */
  CHARACTER = 32,

  /**
   * Represents a list box, allowing the user to select one or more items. It
   * is used for xul:listbox, html:select@size, role="list". See also
   * LIST_ITEM.
   */
  LIST = 33,

  /**
   * Represents an item in a list. See also LIST.
   */
  LISTITEM = 34,

  /**
   * Represents an outline or tree structure, such as a tree view control,
   * that displays a hierarchical list and allows the user to expand and
   * collapse branches. Is is used for role="tree".
   */
  OUTLINE = 35,

  /**
   * Represents an item in an outline or tree structure. It is used for
   * role="treeitem".
   */
  OUTLINEITEM = 36,

  /**
   * Represents a page tab, it is a child of a page tab list. It is used for
   * xul:tab, role="treeitem". Also refer to PAGETABLIST.
   */
  PAGETAB = 37,

  /**
   * Represents a property sheet. It is used for xul:tabpanel,
   * role="tabpanel".
   */
  PROPERTYPAGE = 38,

  /**
   * Represents an indicator, such as a pointer graphic, that points to the
   * current item.
   */
  INDICATOR = 39,

  /**
   * Represents a picture. Is is used for xul:image, html:img.
   */
  GRAPHIC = 40,

  /**
   * Represents read-only text, such as labels for other controls or
   * instructions in a dialog box. Static text cannot be modified or selected.
   * Is is used for xul:label, xul:description, html:label, role="label",
   * or xforms:output.
   */
  STATICTEXT = 41,

  /**
   * Represents selectable text that allows edits or is designated read-only.
   */
  TEXT_LEAF = 42,

  /**
   * Represents a push button control. It is used for xul:button, html:button,
   * role="button", xforms:trigger, xforms:submit.
   */
  PUSHBUTTON = 43,

  /**
   * Represents a check box control. It is used for xul:checkbox,
   * html:input@type="checkbox", role="checkbox", boolean xforms:input.
   */
  CHECKBUTTON = 44,
  
  /**
   * Represents an option button, also called a radio button. It is one of a
   * group of mutually exclusive options. All objects sharing a single parent
   * that have this attribute are assumed to be part of single mutually
   * exclusive group. It is used for xul:radio, html:input@type="radio",
   * role="radio".
   */
  RADIOBUTTON = 45,
  
  /**
   * Represents a combo box; an edit control with an associated list box that
   * provides a set of predefined choices. It is used for html:select,
   * xul:menulist, role="combobox".
   */
  COMBOBOX = 46,

  /**
   * Represents the calendar control. It is used for date xforms:input.
   */
  DROPLIST = 47,

  /**
   * Represents a progress bar, dynamically showing the user the percent
   * complete of an operation in progress. It is used for xul:progressmeter,
   * role="progressbar".
   */
  PROGRESSBAR = 48,

  /**
   * Represents a dial or knob whose purpose is to allow a user to set a value.
   */
  DIAL = 49,

  /**
   * Represents a hot-key field that allows the user to enter a combination or
   * sequence of keystrokes.
   */
  HOTKEYFIELD = 50,

  /**
   * Represents a slider, which allows the user to adjust a setting in given
   * increments between minimum and maximum values. It is used by xul:scale,
   * role="slider", xforms:range.
   */
  SLIDER = 51,

  /**
   * Represents a spin box, which is a control that allows the user to increment
   * or decrement the value displayed in a separate "buddy" control associated
   * with the spin box. It is used for xul:spinbuttons.
   */
  SPINBUTTON = 52,

  /**
   * Represents a graphical image used to diagram data. It is used for svg:svg.
   */
  DIAGRAM = 53,
  
  /**
   * Represents an animation control, which contains content that changes over
   * time, such as a control that displays a series of bitmap frames.
   */
  ANIMATION = 54,

  /**
   * Represents a mathematical equation. It is used by MATHML, where there is a
   * rich DOM subtree for an equation. Use FLAT_EQUATION for <img role="math" alt="[TeX]"/>
   */
  EQUATION = 55,

  /**
   * Represents a button that drops down a list of items.
   */
  BUTTONDROPDOWN = 56,

  /**
   * Represents a button that drops down a menu.
   */
  BUTTONMENU = 57,

  /**
   * Represents a button that drops down a grid. It is used for xul:colorpicker.
   */
  BUTTONDROPDOWNGRID = 58,

  /**
   * Represents blank space between other objects.
   */
  WHITESPACE = 59,

  /**
   * Represents a container of page tab controls. Is it used for xul:tabs,
   * DHTML: role="tabs". Also refer to PAGETAB.
   */
  PAGETABLIST = 60,

  /**
   * Represents a control that displays time.
   */
  CLOCK = 61,

  /**
   * Represents a button on a toolbar that has a drop-down list icon directly
   * adjacent to the button.
   */
  SPLITBUTTON = 62,

  /**
   * Represents an edit control designed for an Internet Protocol (IP) address.
   * The edit control is divided into sections for the different parts of the
   * IP address.
   */
  IPADDRESS = 63,

  /**
   * Represents a label control that has an accelerator.
   */
  ACCEL_LABEL = 64,

  /**
   * Represents an arrow in one of the four cardinal directions.
   */
  ARROW  = 65,

  /**
   * Represents a control that can be drawn into and is used to trap events.
   * It is used for html:canvas.
   */
  CANVAS = 66,

  /**
   * Represents a menu item with a check box.
   */
  CHECK_MENU_ITEM = 67,

  /**
   * Represents a specialized dialog that lets the user choose a color.
   */
  COLOR_CHOOSER  = 68,

  /**
   * Represents control whose purpose is to allow a user to edit a date.
   */
  DATE_EDITOR = 69,

  /**
   * An iconified internal frame in an DESKTOP_PANE. Also refer to
   * INTERNAL_FRAME.
   */
  DESKTOP_ICON = 70,

  /**
   * A desktop pane. A pane that supports internal frames and iconified
   * versions of those internal frames.
   */
  DESKTOP_FRAME = 71,

  /**
   * A directory pane. A pane that allows the user to navigate through
   * and select the contents of a directory. May be used by a file chooser.
   * Also refer to FILE_CHOOSER.
   */
  DIRECTORY_PANE = 72,

  /**
   * A file chooser. A specialized dialog that displays the files in the
   * directory and lets the user select a file, browse a different directory,
   * or specify a filename. May use the directory pane to show the contents of
   * a directory. Also refer to DIRECTORY_PANE.
   */
  FILE_CHOOSER = 73,

  /**
   * A font chooser. A font chooser is a component that lets the user pick
   * various attributes for fonts.
   */
  FONT_CHOOSER = 74,

  /**
   * Frame role. A top level window with a title bar, border, menu bar, etc.
   * It is often used as the primary window for an application.
   */
  CHROME_WINDOW = 75,

  /**
   *  A glass pane. A pane that is guaranteed to be painted on top of all
   * panes beneath it. Also refer to ROOT_PANE.
   */
  GLASS_PANE = 76,

  /**
   * A document container for HTML, whose children represent the document
   * content.
   */
  HTML_CONTAINER = 77,

  /**
   * A small fixed size picture, typically used to decorate components.
   */
  ICON = 78,

  /**
   * Presents an icon or short string in an interface.
   */
  LABEL = 79,

  /**
   * A layered pane. A specialized pane that allows its children to be drawn
   * in layers, providing a form of stacking order. This is usually the pane
   * that holds the menu bar as  well as the pane that contains most of the
   * visual components in a window. Also refer to GLASS_PANE and
   * ROOT_PANE.
   */
  LAYERED_PANE = 80,

  /**
   * A specialized pane whose primary use is inside a dialog.
   */
  OPTION_PANE = 81,

  /**
   * A text object uses for passwords, or other places where the text content
   * is not shown visibly to the user.
   */
  PASSWORD_TEXT = 82,

  /**
   * A temporary window that is usually used to offer the user a list of
   * choices, and then hides when the user selects one of those choices.
   */
  POPUP_MENU = 83,

  /**
   * A radio button that is a menu item.
   */
  RADIO_MENU_ITEM = 84,

  /**
   * A root pane. A specialized pane that has a glass pane and a layered pane
   * as its children. Also refer to GLASS_PANE and LAYERED_PANE.
   */
  ROOT_PANE = 85,

  /**
   * A scroll pane. An object that allows a user to incrementally view a large
   * amount of information.  Its children can include scroll bars and a
   * viewport. Also refer to VIEW_PORT.
   */
  SCROLL_PANE = 86,

  /**
   * A split pane. A specialized panel that presents two other panels at the
   * same time. Between the two panels is a divider the user can manipulate to
   * make one panel larger and the other panel smaller.
   */
  SPLIT_PANE = 87,

  /**
   * The header for a column of a table.
   * XXX: it looks this role is dupe of COLUMNHEADER.
   */
  TABLE_COLUMN_HEADER = 88,

  /**
   * The header for a row of a table.
   * XXX: it looks this role is dupe of ROWHEADER
   */
  TABLE_ROW_HEADER = 89,

  /**
   * A menu item used to tear off and reattach its menu.
   */
  TEAR_OFF_MENU_ITEM = 90,

  /**
   * Represents an accessible terminal.
   */
  TERMINAL = 91,

  /**
   * Collection of objects that constitute a logical text entity.
   */
  TEXT_CONTAINER = 92,

  /**
   * A toggle button. A specialized push button that can be checked or
   * unchecked, but does not provide a separate indicator for the current state.
   */
  TOGGLE_BUTTON = 93,

  /**
   * Representas a control that is capable of expanding and collapsing rows as
   * well as showing multiple columns of data.
   * XXX: it looks like this role is dupe of OUTLINE.
   */
  TREE_TABLE = 94,

  /**
   * A viewport. An object usually used in a scroll pane. It represents the
   * portion of the entire data that the user can see. As the user manipulates
   * the scroll bars, the contents of the viewport can change. Also refer to
   * SCROLL_PANE.
   */
  VIEWPORT = 95,

  /**
   * Header of a document page. Also refer to FOOTER.
   */
  HEADER = 96,

  /**
   * Footer of a document page. Also refer to HEADER.
   */
  FOOTER = 97,

  /**
   * A paragraph of text.
   */
  PARAGRAPH = 98,

  /**
   * A ruler such as those used in word processors.
   */
  RULER = 99,

  /**
   * A text entry having dialog or list containing items for insertion into
   * an entry widget, for instance a list of words for completion of a
   * text entry. It is used for xul:textbox@autocomplete
   */
  AUTOCOMPLETE = 100,

  /**
   *  An editable text object in a toolbar.
   */
  EDITBAR = 101,

  /**
   * An control whose textual content may be entered or modified by the user.
   */
  ENTRY = 102,

  /**
   * A caption describing another object.
   */
  CAPTION = 103,

  /**
   * A visual frame or container which contains a view of document content.
   * Document frames may occur within another Document instance, in which case
   * the second document may be said to be embedded in the containing instance.
   * HTML frames are often DOCUMENT_FRAME. Either this object, or a
   * singleton descendant, should implement the Document interface.
   */
  DOCUMENT_FRAME = 104,

  /**
   * Heading.
   */
  HEADING = 105,

  /**
   * An object representing a page of document content.  It is used in documents
   * which are accessed by the user on a page by page basis.
   */
  PAGE = 106,

  /**
   * A container of document content.  An example of the use of this role is to
   * represent an html:div.
   */
  SECTION = 107,

  /**
   * An object which is redundant with another object in the accessible
   * hierarchy. ATs typically ignore objects with this role.
   */
  REDUNDANT_OBJECT = 108,

  /**
   * A container of form controls. An example of the use of this role is to
   * represent an html:form.
   */
  FORM = 109,

  /**
   * An object which is used to allow input of characters not found on a
   * keyboard, such as the input of Chinese characters on a Western keyboard.
   */
  IME = 110,

  /**
   * XXX: document this.
   */
  APP_ROOT = 111,

  /**
   * Represents a menu item, which is an entry in a menu that a user can choose
   * to display another menu.
   */
  PARENT_MENUITEM = 112,

  /**
   * A calendar that allows the user to select a date.
   */
  CALENDAR = 113,

  /**
   * A list of items that is shown by combobox.
   */
  COMBOBOX_LIST = 114,

  /**
   * A item of list that is shown by combobox.
   */
  COMBOBOX_OPTION = 115,

  /**
   * An image map -- has child links representing the areas
   */
  IMAGE_MAP = 116,
  
  /**
   * An option in a listbox
   */
  OPTION = 117,
  
  /**
   * A rich option in a listbox, it can have other widgets as children
   */
  RICH_OPTION = 118,
  
  /**
   * A list of options
   */
  LISTBOX = 119,

  /**
   * Represents a mathematical equation in the accessible name
   */
  FLAT_EQUATION = 120,
  
  /**
   * Represents a cell within a grid. It is used for role="gridcell". Unlike
   * CELL, it allows the calculation of the accessible name from subtree.
   * Also, see TABLE.
   */
  GRID_CELL = 121,

  /**
   * Represents an embedded object. It is used for html:object or html:embed.
   */
  EMBEDDED_OBJECT = 122,

  /**
   * A note. Originally intended to be hidden until activated, but now also used
   * for things like html 'aside'.
   */
  NOTE = 123,

  /**
   * A figure. Used for things like HTML5 figure element.
   */
  FIGURE = 124,

  /**
   * Represents a rich item with a check box.
   */
  CHECK_RICH_OPTION = 125,

  /**
   * Represent a definition list (dl in HTML).
   */
  DEFINITION_LIST = 126,

  /**
   * Represent a term in a definition list (dt in HTML).
   */
  TERM = 127,

  /**
   * Represent a definition in a definition list (dd in HTML)
   */
  DEFINITION = 128
};

} // namespace role

typedef enum mozilla::a11y::roles::Role role;

} // namespace a11y
} // namespace mozilla

#endif
