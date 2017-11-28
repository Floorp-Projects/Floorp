/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsThemeConstants_h_
#define nsThemeConstants_h_

enum ThemeWidgetType : uint8_t {

  // No appearance at all.
  NS_THEME_NONE,

  // A typical dialog button.
  NS_THEME_BUTTON,

  // A radio element within a radio group.
  NS_THEME_RADIO,

  // A checkbox element.
  NS_THEME_CHECKBOX,

  // A rectangular button that contains complex content
  // like images (e.g. HTML <button> elements)
  NS_THEME_BUTTON_BEVEL,

  // A themed focus outline (for outline:auto)
  NS_THEME_FOCUS_OUTLINE,

  // The toolbox that contains the toolbars.
  NS_THEME_TOOLBOX,

  // A toolbar in an application window.
  NS_THEME_TOOLBAR,

  // A single toolbar button (with no associated dropdown)
  NS_THEME_TOOLBARBUTTON,

  // A dual toolbar button (e.g., a Back button with a dropdown)
  NS_THEME_DUALBUTTON,

  // The dropdown portion of a toolbar button
  NS_THEME_TOOLBARBUTTON_DROPDOWN,

  // Various arrows that go in buttons
  NS_THEME_BUTTON_ARROW_UP,
  NS_THEME_BUTTON_ARROW_DOWN,
  NS_THEME_BUTTON_ARROW_NEXT,
  NS_THEME_BUTTON_ARROW_PREVIOUS,

  // A separator.  Can be horizontal or vertical.
  NS_THEME_SEPARATOR,

  // The gripper for a toolbar.
  NS_THEME_TOOLBARGRIPPER,

  // A splitter.  Can be horizontal or vertical.
  NS_THEME_SPLITTER,

  // A status bar in a main application window.
  NS_THEME_STATUSBAR,

  // A single pane of a status bar.
  NS_THEME_STATUSBARPANEL,

  // The resizer background area in a status bar
  // for the resizer widget in the corner of a window.
  NS_THEME_RESIZERPANEL,

  // The resizer itself.
  NS_THEME_RESIZER,

  // List boxes
  NS_THEME_LISTBOX,

  // A listbox item
  NS_THEME_LISTITEM,

  // A tree widget
  NS_THEME_TREEVIEW,

  // A tree item
  NS_THEME_TREEITEM,

  // A tree widget twisty
  NS_THEME_TREETWISTY,

  // A tree widget branch line
  NS_THEME_TREELINE,

  // A listbox or tree widget header
  NS_THEME_TREEHEADER,

  // An individual header cell
  NS_THEME_TREEHEADERCELL,

  // The sort arrow for a header.
  NS_THEME_TREEHEADERSORTARROW,

  // Open tree widget twisty
  NS_THEME_TREETWISTYOPEN,

  // A horizontal progress bar.
  NS_THEME_PROGRESSBAR,

  // The progress bar's progress indicator
  NS_THEME_PROGRESSCHUNK,

  // A vertical progress bar.
  NS_THEME_PROGRESSBAR_VERTICAL,

  // A vertical progress chunk
  NS_THEME_PROGRESSCHUNK_VERTICAL,

  // A horizontal meter bar.
  NS_THEME_METERBAR,

  // The meter bar's meter indicator
  NS_THEME_METERCHUNK,

  // A single tab in a tab widget.
  NS_THEME_TAB,

  // A single pane (inside the tabpanels container)
  NS_THEME_TABPANEL,

  // The tab panels container.
  NS_THEME_TABPANELS,

  // The tabs scroll arrows (left/right)
  NS_THEME_TAB_SCROLL_ARROW_BACK,
  NS_THEME_TAB_SCROLL_ARROW_FORWARD,

  // A tooltip
  NS_THEME_TOOLTIP,

  // A spin control (up/down control for time/date pickers)
  NS_THEME_SPINNER,

  // The up button of a spin control
  NS_THEME_SPINNER_UPBUTTON,

  // The down button of a spin control
  NS_THEME_SPINNER_DOWNBUTTON,

  // The textfield of a spin control
  NS_THEME_SPINNER_TEXTFIELD,

  // For HTML's <input type=number>
  NS_THEME_NUMBER_INPUT,

  // A scrollbar.
  NS_THEME_SCROLLBAR,

  // A small scrollbar.
  NS_THEME_SCROLLBAR_SMALL,

  // The scrollbar slider
  NS_THEME_SCROLLBAR_HORIZONTAL,
  NS_THEME_SCROLLBAR_VERTICAL,

  // A scrollbar button (up/down/left/right)
  NS_THEME_SCROLLBARBUTTON_UP,
  NS_THEME_SCROLLBARBUTTON_DOWN,
  NS_THEME_SCROLLBARBUTTON_LEFT,
  NS_THEME_SCROLLBARBUTTON_RIGHT,

  // The scrollbar track
  NS_THEME_SCROLLBARTRACK_HORIZONTAL,
  NS_THEME_SCROLLBARTRACK_VERTICAL,

  // The scrollbar thumb
  NS_THEME_SCROLLBARTHUMB_HORIZONTAL,
  NS_THEME_SCROLLBARTHUMB_VERTICAL,

  // A non-disappearing scrollbar.
  NS_THEME_SCROLLBAR_NON_DISAPPEARING,

  // A textfield or text area
  NS_THEME_TEXTFIELD,

  // The caret of a text area
  NS_THEME_CARET,

  // A multiline text field
  NS_THEME_TEXTFIELD_MULTILINE,

  // A searchfield
  NS_THEME_SEARCHFIELD,

  // A dropdown list.
  NS_THEME_MENULIST,

  // The dropdown button(s) that open up a dropdown list.
  NS_THEME_MENULIST_BUTTON,

  // The text part of a dropdown list, to left of button
  NS_THEME_MENULIST_TEXT,

  // An editable textfield with a dropdown list (a combobox)
  NS_THEME_MENULIST_TEXTFIELD,

  // A slider
  NS_THEME_SCALE_HORIZONTAL,
  NS_THEME_SCALE_VERTICAL,

  // A slider's thumb
  NS_THEME_SCALETHUMB_HORIZONTAL,
  NS_THEME_SCALETHUMB_VERTICAL,

  // If the platform supports it, the left/right chunks
  // of the slider thumb
  NS_THEME_SCALETHUMBSTART,
  NS_THEME_SCALETHUMBEND,

  // The ticks for a slider.
  NS_THEME_SCALETHUMBTICK,

  // nsRangeFrame and its subparts
  NS_THEME_RANGE,
  NS_THEME_RANGE_THUMB,

  // A groupbox
  NS_THEME_GROUPBOX,

  // A generic container that always repaints on state
  // changes.  This is a hack to make checkboxes and
  // radio buttons work.
  NS_THEME_CHECKBOX_CONTAINER,
  NS_THEME_RADIO_CONTAINER,

  // The label part of a checkbox or radio button, used for painting
  // a focus outline.
  NS_THEME_CHECKBOX_LABEL,
  NS_THEME_RADIO_LABEL,

  // The focus outline box inside of a button
  NS_THEME_BUTTON_FOCUS,

  // Window and dialog backgrounds
  NS_THEME_WINDOW,
  NS_THEME_DIALOG,

  // Menu Bar background
  NS_THEME_MENUBAR,
  // Menu Popup background
  NS_THEME_MENUPOPUP,
  // <menu> and <menuitem> appearances
  NS_THEME_MENUITEM,
  NS_THEME_CHECKMENUITEM,
  NS_THEME_RADIOMENUITEM,

  // menu checkbox/radio appearances
  NS_THEME_MENUCHECKBOX,
  NS_THEME_MENURADIO,
  NS_THEME_MENUSEPARATOR,
  NS_THEME_MENUARROW,
  // An image in the menu gutter, like in bookmarks or history
  NS_THEME_MENUIMAGE,
  // For text on non-iconic menuitems only
  NS_THEME_MENUITEMTEXT,

  // Vista Rebars
  NS_THEME_WIN_COMMUNICATIONS_TOOLBOX,
  NS_THEME_WIN_MEDIA_TOOLBOX,
  NS_THEME_WIN_BROWSERTABBAR_TOOLBOX,

  // Titlebar elements on the Mac
  NS_THEME_MAC_FULLSCREEN_BUTTON,

  // Mac help button
  NS_THEME_MAC_HELP_BUTTON,

  // Vista glass
  NS_THEME_WIN_BORDERLESS_GLASS,
  NS_THEME_WIN_GLASS,

  // Windows themed window frame elements
  NS_THEME_WINDOW_TITLEBAR,
  NS_THEME_WINDOW_TITLEBAR_MAXIMIZED,
  NS_THEME_WINDOW_FRAME_LEFT,
  NS_THEME_WINDOW_FRAME_RIGHT,
  NS_THEME_WINDOW_FRAME_BOTTOM,
  NS_THEME_WINDOW_BUTTON_CLOSE,
  NS_THEME_WINDOW_BUTTON_MINIMIZE,
  NS_THEME_WINDOW_BUTTON_MAXIMIZE,
  NS_THEME_WINDOW_BUTTON_RESTORE,
  NS_THEME_WINDOW_BUTTON_BOX,
  NS_THEME_WINDOW_BUTTON_BOX_MAXIMIZED,

  // moz-apperance style used in setting proper glass margins
  NS_THEME_WIN_EXCLUDE_GLASS,

  NS_THEME_MAC_VIBRANCY_LIGHT,
  NS_THEME_MAC_VIBRANCY_DARK,
  NS_THEME_MAC_VIBRANT_TITLEBAR_LIGHT,
  NS_THEME_MAC_VIBRANT_TITLEBAR_DARK,
  NS_THEME_MAC_DISCLOSURE_BUTTON_OPEN,
  NS_THEME_MAC_DISCLOSURE_BUTTON_CLOSED,

  NS_THEME_GTK_INFO_BAR,
  NS_THEME_MAC_SOURCE_LIST,
  NS_THEME_MAC_SOURCE_LIST_SELECTION,
  NS_THEME_MAC_ACTIVE_SOURCE_LIST_SELECTION,

  ThemeWidgetType_COUNT
};

#endif // nsThemeConstants_h_
