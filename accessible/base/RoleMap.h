/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// clang-format off
/**
 * Usage: declare the macro ROLE()with the following arguments:
 * ROLE(geckoRole, stringRole, atkRole, macRole, macSubrole, msaaRole, ia2Role, nameRule)
 */

ROLE(NOTHING,
     "nothing",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeIfReqRule)

ROLE(TITLEBAR,
     "titlebar",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,  //Irrelevant on OS X; windows are always native.
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_TITLEBAR,
     ROLE_SYSTEM_TITLEBAR,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MENUBAR,
     "menubar",
     ATK_ROLE_MENU_BAR,
     NSAccessibilityMenuBarRole,  //Irrelevant on OS X; the menubar will always be native and on the top of the screen.
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_MENUBAR,
     ROLE_SYSTEM_MENUBAR,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(SCROLLBAR,
     "scrollbar",
     ATK_ROLE_SCROLL_BAR,
     NSAccessibilityScrollBarRole,  //We might need to make this its own mozAccessible, to support the children objects (valueindicator, down/up buttons).
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_SCROLLBAR,
     ROLE_SYSTEM_SCROLLBAR,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromValueRule)

ROLE(GRIP,
     "grip",
     ATK_ROLE_UNKNOWN,
     NSAccessibilitySplitterRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_GRIP,
     ROLE_SYSTEM_GRIP,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(SOUND,
     "sound",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,  //Unused on OS X.
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_SOUND,
     ROLE_SYSTEM_SOUND,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(CURSOR,
     "cursor",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,  //Unused on OS X.
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_CURSOR,
     ROLE_SYSTEM_CURSOR,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(CARET,
     "caret",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,  //Unused on OS X.
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_CARET,
     ROLE_SYSTEM_CARET,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(ALERT,
     "alert",
     ATK_ROLE_ALERT,
     NSAccessibilityGroupRole,
     @"AXApplicationAlert",
     ROLE_SYSTEM_ALERT,
     ROLE_SYSTEM_ALERT,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeIfReqRule)

ROLE(WINDOW,
     "window",
     ATK_ROLE_WINDOW,
     NSAccessibilityWindowRole,  //Irrelevant on OS X; all window a11y is handled by the system.
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_WINDOW,
     ROLE_SYSTEM_WINDOW,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(INTERNAL_FRAME,
     "internal frame",
     ATK_ROLE_INTERNAL_FRAME,
     NSAccessibilityScrollAreaRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_INTERNAL_FRAME,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MENUPOPUP,
     "menupopup",
     ATK_ROLE_MENU,
     NSAccessibilityMenuRole,  //The parent of menuitems.
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_MENUPOPUP,
     ROLE_SYSTEM_MENUPOPUP,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MENUITEM,
     "menuitem",
     ATK_ROLE_MENU_ITEM,
     NSAccessibilityMenuItemRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_MENUITEM,
     ROLE_SYSTEM_MENUITEM,
     java::SessionAccessibility::CLASSNAME_MENUITEM,
     eNameFromSubtreeRule)

ROLE(TOOLTIP,
     "tooltip",
     ATK_ROLE_TOOL_TIP,
     NSAccessibilityGroupRole,
     @"AXUserInterfaceTooltip",
     ROLE_SYSTEM_TOOLTIP,
     ROLE_SYSTEM_TOOLTIP,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(APPLICATION,
     "application",
     ATK_ROLE_EMBEDDED,
     NSAccessibilityGroupRole,  //Unused on OS X. the system will take care of this.
     @"AXLandmarkApplication",
     ROLE_SYSTEM_APPLICATION,
     ROLE_SYSTEM_APPLICATION,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(DOCUMENT,
     "document",
     ATK_ROLE_DOCUMENT_WEB,
     @"AXWebArea",
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_DOCUMENT,
     ROLE_SYSTEM_DOCUMENT,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

/**
 *  msaa comment:
 *   We used to map to ROLE_SYSTEM_PANE, but JAWS would
 *   not read the accessible name for the contaning pane.
 *   However, JAWS will read the accessible name for a groupbox.
 *   By mapping a PANE to a GROUPING, we get no undesirable effects,
 *   but fortunately JAWS will then read the group's label,
 *   when an inner control gets focused.
 */
ROLE(PANE,
     "pane",
     ATK_ROLE_PANEL,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_GROUPING,
     ROLE_SYSTEM_GROUPING,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(CHART,
     "chart",
     ATK_ROLE_CHART,
     NSAccessibilityUnknownRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_CHART,
     ROLE_SYSTEM_CHART,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(DIALOG,
     "dialog",
     ATK_ROLE_DIALOG,
     NSAccessibilityGroupRole,  //There's a dialog subrole.
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_DIALOG,
     ROLE_SYSTEM_DIALOG,
     java::SessionAccessibility::CLASSNAME_DIALOG,
     eNoNameRule)

ROLE(BORDER,
     "border",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,  //Unused on OS X.
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_BORDER,
     ROLE_SYSTEM_BORDER,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(GROUPING,
     "grouping",
     ATK_ROLE_PANEL,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_GROUPING,
     ROLE_SYSTEM_GROUPING,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeIfReqRule)

ROLE(SEPARATOR,
     "separator",
     ATK_ROLE_SEPARATOR,
     NSAccessibilitySplitterRole,
     @"AXContentSeparator",
     ROLE_SYSTEM_SEPARATOR,
     ROLE_SYSTEM_SEPARATOR,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(TOOLBAR,
     "toolbar",
     ATK_ROLE_TOOL_BAR,
     NSAccessibilityToolbarRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_TOOLBAR,
     ROLE_SYSTEM_TOOLBAR,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(STATUSBAR,
     "statusbar",
     ATK_ROLE_STATUSBAR,
     NSAccessibilityGroupRole,
     @"AXApplicationStatus",
     ROLE_SYSTEM_STATUSBAR,
     ROLE_SYSTEM_STATUSBAR,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(TABLE,
     "table",
     ATK_ROLE_TABLE,
     NSAccessibilityTableRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_TABLE,
     ROLE_SYSTEM_TABLE,
     java::SessionAccessibility::CLASSNAME_GRIDVIEW,
     eNameFromSubtreeIfReqRule)

ROLE(COLUMNHEADER,
     "columnheader",
     ATK_ROLE_COLUMN_HEADER,
     NSAccessibilityCellRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_COLUMNHEADER,
     ROLE_SYSTEM_COLUMNHEADER,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(ROWHEADER,
     "rowheader",
     ATK_ROLE_ROW_HEADER,
     NSAccessibilityCellRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_ROWHEADER,
     ROLE_SYSTEM_ROWHEADER,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(COLUMN,
     "column",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityColumnRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_COLUMN,
     ROLE_SYSTEM_COLUMN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(ROW,
     "row",
     ATK_ROLE_TABLE_ROW,
     NSAccessibilityRowRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_ROW,
     ROLE_SYSTEM_ROW,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(CELL,
     "cell",
     ATK_ROLE_TABLE_CELL,
     NSAccessibilityCellRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_CELL,
     ROLE_SYSTEM_CELL,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeIfReqRule)

ROLE(LINK,
     "link",
     ATK_ROLE_LINK,
     NSAccessibilityLinkRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_LINK,
     ROLE_SYSTEM_LINK,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(HELPBALLOON,
     "helpballoon",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityHelpTagRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_HELPBALLOON,
     ROLE_SYSTEM_HELPBALLOON,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(CHARACTER,
     "character",
     ATK_ROLE_IMAGE,
     NSAccessibilityUnknownRole,  //Unused on OS X.
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_CHARACTER,
     ROLE_SYSTEM_CHARACTER,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(LIST,
     "list",
     ATK_ROLE_LIST,
     NSAccessibilityListRole,
     NSAccessibilityContentListSubrole,
     ROLE_SYSTEM_LIST,
     ROLE_SYSTEM_LIST,
     java::SessionAccessibility::CLASSNAME_LISTVIEW,
     eNameFromSubtreeIfReqRule)

ROLE(LISTITEM,
     "listitem",
     ATK_ROLE_LIST_ITEM,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_LISTITEM,
     ROLE_SYSTEM_LISTITEM,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(OUTLINE,
     "outline",
     ATK_ROLE_TREE,
     NSAccessibilityOutlineRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_OUTLINE,
     ROLE_SYSTEM_OUTLINE,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(OUTLINEITEM,
     "outlineitem",
     ATK_ROLE_TREE_ITEM,
     NSAccessibilityRowRole,
     NSAccessibilityOutlineRowSubrole,
     ROLE_SYSTEM_OUTLINEITEM,
     ROLE_SYSTEM_OUTLINEITEM,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(PAGETAB,
     "pagetab",
     ATK_ROLE_PAGE_TAB,
     NSAccessibilityRadioButtonRole,
     @"AXTabButton", // Can be upgraded to NSAccessibilityTabButtonSubrole in 10.13
     ROLE_SYSTEM_PAGETAB,
     ROLE_SYSTEM_PAGETAB,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(PROPERTYPAGE,
     "propertypage",
     ATK_ROLE_SCROLL_PANE,
     NSAccessibilityGroupRole,
     @"AXTabPanel",
     ROLE_SYSTEM_PROPERTYPAGE,
     ROLE_SYSTEM_PROPERTYPAGE,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(INDICATOR,
     "indicator",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_INDICATOR,
     ROLE_SYSTEM_INDICATOR,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(GRAPHIC,
     "graphic",
     ATK_ROLE_IMAGE,
     NSAccessibilityImageRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_GRAPHIC,
     ROLE_SYSTEM_GRAPHIC,
     java::SessionAccessibility::CLASSNAME_IMAGE,
     eNoNameRule)

ROLE(STATICTEXT,
     "statictext",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityStaticTextRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_STATICTEXT,
     ROLE_SYSTEM_STATICTEXT,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(TEXT_LEAF,
     "text leaf",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityStaticTextRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_TEXT,
     ROLE_SYSTEM_TEXT,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(PUSHBUTTON,
     "pushbutton",
     ATK_ROLE_PUSH_BUTTON,
     NSAccessibilityButtonRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_PUSHBUTTON,
     ROLE_SYSTEM_PUSHBUTTON,
     java::SessionAccessibility::CLASSNAME_BUTTON,
     eNameFromSubtreeRule)

ROLE(CHECKBUTTON,
     "checkbutton",
     ATK_ROLE_CHECK_BOX,
     NSAccessibilityCheckBoxRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_CHECKBUTTON,
     ROLE_SYSTEM_CHECKBUTTON,
     java::SessionAccessibility::CLASSNAME_CHECKBOX,
     eNameFromSubtreeRule)

ROLE(RADIOBUTTON,
     "radiobutton",
     ATK_ROLE_RADIO_BUTTON,
     NSAccessibilityRadioButtonRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_RADIOBUTTON,
     ROLE_SYSTEM_RADIOBUTTON,
     java::SessionAccessibility::CLASSNAME_RADIOBUTTON,
     eNameFromSubtreeRule)

// Equivalent of HTML select element with size="1". See also EDITCOMBOBOX.
ROLE(COMBOBOX,
     "combobox",
     ATK_ROLE_COMBO_BOX,
     NSAccessibilityPopUpButtonRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_COMBOBOX,
     ROLE_SYSTEM_COMBOBOX,
     java::SessionAccessibility::CLASSNAME_SPINNER,
     eNameFromValueRule)

ROLE(DROPLIST,
     "droplist",
     ATK_ROLE_COMBO_BOX,
     NSAccessibilityPopUpButtonRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_DROPLIST,
     ROLE_SYSTEM_DROPLIST,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(PROGRESSBAR,
     "progressbar",
     ATK_ROLE_PROGRESS_BAR,
     NSAccessibilityProgressIndicatorRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_PROGRESSBAR,
     ROLE_SYSTEM_PROGRESSBAR,
     java::SessionAccessibility::CLASSNAME_PROGRESSBAR,
     eNameFromValueRule)

ROLE(DIAL,
     "dial",
     ATK_ROLE_DIAL,
     NSAccessibilityUnknownRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_DIAL,
     ROLE_SYSTEM_DIAL,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(HOTKEYFIELD,
     "hotkeyfield",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_HOTKEYFIELD,
     ROLE_SYSTEM_HOTKEYFIELD,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(SLIDER,
     "slider",
     ATK_ROLE_SLIDER,
     NSAccessibilitySliderRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_SLIDER,
     ROLE_SYSTEM_SLIDER,
     java::SessionAccessibility::CLASSNAME_SEEKBAR,
     eNameFromValueRule)

ROLE(SPINBUTTON,
     "spinbutton",
     ATK_ROLE_SPIN_BUTTON,
     NSAccessibilityIncrementorRole,  //Subroles: Increment/Decrement.
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_SPINBUTTON,
     ROLE_SYSTEM_SPINBUTTON,
     java::SessionAccessibility::CLASSNAME_EDITTEXT,
     eNameFromValueRule)

ROLE(DIAGRAM,
     "diagram",
     ATK_ROLE_IMAGE,
     NSAccessibilityImageRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_DIAGRAM,
     ROLE_SYSTEM_DIAGRAM,
     java::SessionAccessibility::CLASSNAME_IMAGE,
     eNoNameRule)

ROLE(ANIMATION,
     "animation",
     ATK_ROLE_ANIMATION,
     NSAccessibilityUnknownRole,
     @"AXApplicationMarquee",
     ROLE_SYSTEM_ANIMATION,
     ROLE_SYSTEM_ANIMATION,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(EQUATION,
     "equation",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_EQUATION,
     ROLE_SYSTEM_EQUATION,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(BUTTONDROPDOWN,
     "buttondropdown",
     ATK_ROLE_PUSH_BUTTON,
     NSAccessibilityPopUpButtonRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_BUTTONDROPDOWN,
     ROLE_SYSTEM_BUTTONDROPDOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(BUTTONMENU,
     "buttonmenu",
     ATK_ROLE_PUSH_BUTTON,
     NSAccessibilityMenuButtonRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_BUTTONMENU,
     ROLE_SYSTEM_BUTTONMENU,
     java::SessionAccessibility::CLASSNAME_SPINNER,
     eNameFromSubtreeRule)

ROLE(BUTTONDROPDOWNGRID,
     "buttondropdowngrid",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_BUTTONDROPDOWNGRID,
     ROLE_SYSTEM_BUTTONDROPDOWNGRID,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(WHITESPACE,
     "whitespace",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_WHITESPACE,
     ROLE_SYSTEM_WHITESPACE,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(PAGETABLIST,
     "pagetablist",
     ATK_ROLE_PAGE_TAB_LIST,
     NSAccessibilityTabGroupRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_PAGETABLIST,
     ROLE_SYSTEM_PAGETABLIST,
     java::SessionAccessibility::CLASSNAME_TABWIDGET,
     eNoNameRule)

ROLE(CLOCK,
     "clock",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,  //Unused on OS X
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_CLOCK,
     ROLE_SYSTEM_CLOCK,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(SPLITBUTTON,
     "splitbutton",
     ATK_ROLE_PUSH_BUTTON,
     NSAccessibilityButtonRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_SPLITBUTTON,
     ROLE_SYSTEM_SPLITBUTTON,
     java::SessionAccessibility::CLASSNAME_BUTTON,
     eNoNameRule)

ROLE(IPADDRESS,
     "ipaddress",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_IPADDRESS,
     ROLE_SYSTEM_IPADDRESS,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(ACCEL_LABEL,
     "accel label",
     ATK_ROLE_ACCEL_LABEL,
     NSAccessibilityStaticTextRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_STATICTEXT,
     ROLE_SYSTEM_STATICTEXT,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(ARROW,
     "arrow",
     ATK_ROLE_ARROW,
     NSAccessibilityUnknownRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_INDICATOR,
     ROLE_SYSTEM_INDICATOR,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(CANVAS,
     "canvas",
     ATK_ROLE_CANVAS,
     NSAccessibilityImageRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_CANVAS,
     java::SessionAccessibility::CLASSNAME_IMAGE,
     eNoNameRule)

ROLE(CHECK_MENU_ITEM,
     "check menu item",
     ATK_ROLE_CHECK_MENU_ITEM,
     NSAccessibilityMenuItemRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_MENUITEM,
     IA2_ROLE_CHECK_MENU_ITEM,
     java::SessionAccessibility::CLASSNAME_MENUITEM,
     eNameFromSubtreeRule)

ROLE(COLOR_CHOOSER,
     "color chooser",
     ATK_ROLE_COLOR_CHOOSER,
     NSAccessibilityColorWellRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_DIALOG,
     IA2_ROLE_COLOR_CHOOSER,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(DATE_EDITOR,
     "date editor",
     ATK_ROLE_DATE_EDITOR,
     @"AXDateField",
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_DATE_EDITOR,
     java::SessionAccessibility::CLASSNAME_SPINNER,
     eNoNameRule)

ROLE(DESKTOP_ICON,
     "desktop icon",
     ATK_ROLE_DESKTOP_ICON,
     NSAccessibilityImageRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_DESKTOP_ICON,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(DESKTOP_FRAME,
     "desktop frame",
     ATK_ROLE_DESKTOP_FRAME,
     NSAccessibilityUnknownRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_DESKTOP_PANE,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(DIRECTORY_PANE,
     "directory pane",
     ATK_ROLE_DIRECTORY_PANE,
     NSAccessibilityBrowserRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_DIRECTORY_PANE,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(FILE_CHOOSER,
     "file chooser",
     ATK_ROLE_FILE_CHOOSER,
     NSAccessibilityUnknownRole,  //Unused on OS X
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_FILE_CHOOSER,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(FONT_CHOOSER,
     "font chooser",
     ATK_ROLE_FONT_CHOOSER,
     NSAccessibilityUnknownRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_FONT_CHOOSER,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(CHROME_WINDOW,
     "chrome window",
     ATK_ROLE_FRAME,
     NSAccessibilityGroupRole,  //Contains the main Firefox UI
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_APPLICATION,
     IA2_ROLE_FRAME,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(GLASS_PANE,
     "glass pane",
     ATK_ROLE_GLASS_PANE,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_GLASS_PANE,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(HTML_CONTAINER,
     "html container",
     ATK_ROLE_HTML_CONTAINER,
     NSAccessibilityUnknownRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeIfReqRule)

ROLE(ICON,
     "icon",
     ATK_ROLE_ICON,
     NSAccessibilityImageRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_PUSHBUTTON,
     IA2_ROLE_ICON,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(LABEL,
     "label",
     ATK_ROLE_LABEL,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_STATICTEXT,
     IA2_ROLE_LABEL,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(LAYERED_PANE,
     "layered pane",
     ATK_ROLE_LAYERED_PANE,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_LAYERED_PANE,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(OPTION_PANE,
     "option pane",
     ATK_ROLE_OPTION_PANE,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_OPTION_PANE,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(PASSWORD_TEXT,
     "password text",
     ATK_ROLE_PASSWORD_TEXT,
     NSAccessibilityTextFieldRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_TEXT,
     ROLE_SYSTEM_TEXT,
     java::SessionAccessibility::CLASSNAME_EDITTEXT,
     eNoNameRule)

ROLE(POPUP_MENU,
     "popup menu",
     ATK_ROLE_POPUP_MENU,
     NSAccessibilityUnknownRole,  //Unused
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_MENUPOPUP,
     ROLE_SYSTEM_MENUPOPUP,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(RADIO_MENU_ITEM,
     "radio menu item",
     ATK_ROLE_RADIO_MENU_ITEM,
     NSAccessibilityMenuItemRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_MENUITEM,
     IA2_ROLE_RADIO_MENU_ITEM,
     java::SessionAccessibility::CLASSNAME_MENUITEM,
     eNameFromSubtreeRule)

ROLE(ROOT_PANE,
     "root pane",
     ATK_ROLE_ROOT_PANE,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_ROOT_PANE,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(SCROLL_PANE,
     "scroll pane",
     ATK_ROLE_SCROLL_PANE,
     NSAccessibilityScrollAreaRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_SCROLL_PANE,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(SPLIT_PANE,
     "split pane",
     ATK_ROLE_SPLIT_PANE,
     NSAccessibilitySplitGroupRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_SPLIT_PANE,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(TABLE_COLUMN_HEADER,
     "table column header",
     ATK_ROLE_TABLE_COLUMN_HEADER,
     NSAccessibilityUnknownRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_COLUMNHEADER,
     ROLE_SYSTEM_COLUMNHEADER,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(TABLE_ROW_HEADER,
     "table row header",
     ATK_ROLE_TABLE_ROW_HEADER,
     NSAccessibilityUnknownRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_ROWHEADER,
     ROLE_SYSTEM_ROWHEADER,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(TEAR_OFF_MENU_ITEM,
     "tear off menu item",
     ATK_ROLE_TEAR_OFF_MENU_ITEM,
     NSAccessibilityMenuItemRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_MENUITEM,
     IA2_ROLE_TEAR_OFF_MENU,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(TERMINAL,
     "terminal",
     ATK_ROLE_TERMINAL,
     NSAccessibilityUnknownRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_TERMINAL,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(TEXT_CONTAINER,
     "text container",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_TEXT_FRAME,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeIfReqRule)

ROLE(TOGGLE_BUTTON,
     "toggle button",
     ATK_ROLE_TOGGLE_BUTTON,
     NSAccessibilityCheckBoxRole,
     NSAccessibilityToggleSubrole,
     ROLE_SYSTEM_PUSHBUTTON,
     IA2_ROLE_TOGGLE_BUTTON,
     java::SessionAccessibility::CLASSNAME_TOGGLEBUTTON,
     eNameFromSubtreeRule)

ROLE(TREE_TABLE,
     "tree table",
     ATK_ROLE_TREE_TABLE,
     NSAccessibilityTableRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_OUTLINE,
     ROLE_SYSTEM_OUTLINE,
     java::SessionAccessibility::CLASSNAME_GRIDVIEW,
     eNoNameRule)

ROLE(VIEWPORT,
     "viewport",
     ATK_ROLE_VIEWPORT,
     NSAccessibilityUnknownRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_PANE,
     IA2_ROLE_VIEW_PORT,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(HEADER,
     "header",
     ATK_ROLE_HEADER,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_HEADER,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(FOOTER,
     "footer",
     ATK_ROLE_FOOTER,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_FOOTER,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(PARAGRAPH,
     "paragraph",
     ATK_ROLE_PARAGRAPH,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_PARAGRAPH,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeIfReqRule)

ROLE(RULER,
     "ruler",
     ATK_ROLE_RULER,
     NSAccessibilityRulerRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_RULER,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(AUTOCOMPLETE,
     "autocomplete",
     ATK_ROLE_AUTOCOMPLETE,
     NSAccessibilityUnknownRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_COMBOBOX,
     ROLE_SYSTEM_COMBOBOX,
     java::SessionAccessibility::CLASSNAME_EDITTEXT,
     eNoNameRule)

ROLE(EDITBAR,
     "editbar",
     ATK_ROLE_EDITBAR,
     NSAccessibilityTextFieldRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_TEXT,
     IA2_ROLE_EDITBAR,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(ENTRY,
     "entry",
     ATK_ROLE_ENTRY,
     NSAccessibilityTextFieldRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_TEXT,
     ROLE_SYSTEM_TEXT,
     java::SessionAccessibility::CLASSNAME_EDITTEXT,
     eNameFromValueRule)

ROLE(CAPTION,
     "caption",
     ATK_ROLE_CAPTION,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_CAPTION,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeIfReqRule)

ROLE(NON_NATIVE_DOCUMENT,
     "non-native document",
     ATK_ROLE_DOCUMENT_FRAME,
     NSAccessibilityGroupRole,
     @"AXDocument",
     ROLE_SYSTEM_DOCUMENT,
     ROLE_SYSTEM_DOCUMENT,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(HEADING,
     "heading",
     ATK_ROLE_HEADING,
     @"AXHeading",
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_HEADING,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(PAGE,
     "page",
     ATK_ROLE_PAGE,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_PAGE,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(SECTION,
     "section",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_SECTION,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeIfReqRule)

ROLE(REDUNDANT_OBJECT,
     "redundant object",
     ATK_ROLE_REDUNDANT_OBJECT,
     NSAccessibilityUnknownRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_REDUNDANT_OBJECT,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(FORM,
     "form",
     ATK_ROLE_FORM,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_FORM,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(IME,
     "ime",
     ATK_ROLE_INPUT_METHOD_WINDOW,
     NSAccessibilityUnknownRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_INPUT_METHOD_WINDOW,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(APP_ROOT,
     "app root",
     ATK_ROLE_APPLICATION,
     NSAccessibilityUnknownRole,  //Unused on OS X
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_APPLICATION,
     ROLE_SYSTEM_APPLICATION,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(PARENT_MENUITEM,
     "parent menuitem",
     ATK_ROLE_MENU,
     NSAccessibilityMenuItemRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_MENUITEM,
     ROLE_SYSTEM_MENUITEM,
     java::SessionAccessibility::CLASSNAME_MENUITEM,
     eNameFromSubtreeRule)

ROLE(CALENDAR,
     "calendar",
     ATK_ROLE_CALENDAR,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_CLIENT,
     ROLE_SYSTEM_CLIENT,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(COMBOBOX_LIST,
     "combobox list",
     ATK_ROLE_MENU,
     NSAccessibilityMenuRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_LIST,
     ROLE_SYSTEM_LIST,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(COMBOBOX_OPTION,
     "combobox option",
     ATK_ROLE_MENU_ITEM,
     NSAccessibilityMenuItemRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_LISTITEM,
     ROLE_SYSTEM_LISTITEM,
     java::SessionAccessibility::CLASSNAME_MENUITEM,
     eNameFromSubtreeRule)

ROLE(IMAGE_MAP,
     "image map",
     ATK_ROLE_IMAGE,
     @"AXImageMap",
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_GRAPHIC,
     ROLE_SYSTEM_GRAPHIC,
     java::SessionAccessibility::CLASSNAME_IMAGE,
     eNoNameRule)

ROLE(OPTION,
     "listbox option",
     ATK_ROLE_LIST_ITEM,
     NSAccessibilityStaticTextRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_LISTITEM,
     ROLE_SYSTEM_LISTITEM,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(RICH_OPTION,
     "listbox rich option",
     ATK_ROLE_LIST_ITEM,
     NSAccessibilityRowRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_LISTITEM,
     ROLE_SYSTEM_LISTITEM,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(LISTBOX,
     "listbox",
     ATK_ROLE_LIST_BOX,
     NSAccessibilityListRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_LIST,
     ROLE_SYSTEM_LIST,
     java::SessionAccessibility::CLASSNAME_LISTVIEW,
     eNoNameRule)

ROLE(FLAT_EQUATION,
     "flat equation",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,
     @"AXDocumentMath",
     ROLE_SYSTEM_EQUATION,
     ROLE_SYSTEM_EQUATION,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(GRID_CELL,
     "gridcell",
     ATK_ROLE_TABLE_CELL,
     NSAccessibilityCellRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_CELL,
     ROLE_SYSTEM_CELL,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(EMBEDDED_OBJECT,
     "embedded object",
     ATK_ROLE_PANEL,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_EMBEDDED_OBJECT,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(NOTE,
     "note",
     ATK_ROLE_COMMENT,
     NSAccessibilityGroupRole,
     @"AXDocumentNote",
     USE_ROLE_STRING,
     IA2_ROLE_NOTE,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeIfReqRule)

ROLE(FIGURE,
     "figure",
     ATK_ROLE_PANEL,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_GROUPING,
     ROLE_SYSTEM_GROUPING,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(CHECK_RICH_OPTION,
     "check rich option",
     ATK_ROLE_CHECK_BOX,
     NSAccessibilityCheckBoxRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_CHECKBUTTON,
     ROLE_SYSTEM_CHECKBUTTON,
     java::SessionAccessibility::CLASSNAME_CHECKBOX,
     eNameFromSubtreeRule)

ROLE(DEFINITION_LIST,
     "definitionlist",
     ATK_ROLE_LIST,
     NSAccessibilityListRole,
     @"AXDescriptionList",
     ROLE_SYSTEM_LIST,
     ROLE_SYSTEM_LIST,
     java::SessionAccessibility::CLASSNAME_LISTVIEW,
     eNameFromSubtreeIfReqRule)

ROLE(TERM,
     "term",
     ATK_ROLE_DESCRIPTION_TERM,
     NSAccessibilityGroupRole,
     @"AXTerm",
     ROLE_SYSTEM_LISTITEM,
     ROLE_SYSTEM_LISTITEM,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(DEFINITION,
     "definition",
     ATK_ROLE_PARAGRAPH,
     NSAccessibilityGroupRole,
     @"AXDescription",
     USE_ROLE_STRING,
     IA2_ROLE_PARAGRAPH,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(KEY,
     "key",
     ATK_ROLE_PUSH_BUTTON,
     NSAccessibilityButtonRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_PUSHBUTTON,
     ROLE_SYSTEM_PUSHBUTTON,
     java::SessionAccessibility::CLASSNAME_BUTTON,
     eNameFromSubtreeRule)

ROLE(SWITCH,
     "switch",
     ATK_ROLE_TOGGLE_BUTTON,
     NSAccessibilityCheckBoxRole,
     NSAccessibilitySwitchSubrole,
     ROLE_SYSTEM_CHECKBUTTON,
     IA2_ROLE_TOGGLE_BUTTON,
     java::SessionAccessibility::CLASSNAME_CHECKBOX,
     eNameFromSubtreeRule)

ROLE(MATHML_MATH,
     "math",
     ATK_ROLE_MATH,
     NSAccessibilityGroupRole,
     @"AXDocumentMath",
     ROLE_SYSTEM_EQUATION,
     ROLE_SYSTEM_EQUATION,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_IDENTIFIER,
     "mathml identifier",
     ATK_ROLE_STATIC,
     NSAccessibilityGroupRole,
     @"AXMathIdentifier",
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(MATHML_NUMBER,
     "mathml number",
     ATK_ROLE_STATIC,
     NSAccessibilityGroupRole,
     @"AXMathNumber",
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(MATHML_OPERATOR,
     "mathml operator",
     ATK_ROLE_STATIC,
     NSAccessibilityGroupRole,
     @"AXMathOperator",
     // XXX: NSAccessibility also uses subroles AXMathSeparatorOperator and
    // AXMathFenceOperator. We should use the NS_MATHML_OPERATOR_FENCE and
    // NS_MATHML_OPERATOR_SEPARATOR bits of nsOperatorFlags, but currently they
    // are only available from the MathML layout code. Hence we just fallback
    // to subrole AXMathOperator for now.
    // XXX bug 1175747 WebKit also creates anonymous operators for <mfenced>
    // which have subroles AXMathSeparatorOperator and AXMathFenceOperator.
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(MATHML_TEXT,
     "mathml text",
     ATK_ROLE_STATIC,
     NSAccessibilityGroupRole,
     @"AXMathRoot",
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(MATHML_STRING_LITERAL,
     "mathml string literal",
     ATK_ROLE_STATIC,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeRule)

ROLE(MATHML_GLYPH,
     "mathml glyph",
     ATK_ROLE_IMAGE,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_IMAGE,
     eNameFromSubtreeRule)

ROLE(MATHML_ROW,
     "mathml row",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     @"AXMathRow",
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_FRACTION,
     "mathml fraction",
     ATK_ROLE_MATH_FRACTION,
     NSAccessibilityGroupRole,
     @"AXMathFraction",
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_SQUARE_ROOT,
     "mathml square root",
     ATK_ROLE_MATH_ROOT,
     NSAccessibilityGroupRole,
     @"AXMathSquareRoot",
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_ROOT,
     "mathml root",
     ATK_ROLE_MATH_ROOT,
     NSAccessibilityGroupRole,
     @"AXMathRoot",
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_FENCED,
     "mathml fenced",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     @"AXMathFenced",      // XXX bug 1176970 This should be AXMathFence, but doing so without implementing the whole fence interface seems to make VoiceOver crash, so we present it as a row for now.
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_ENCLOSED,
     "mathml enclosed",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_STYLE,
     "mathml style",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     @"AXMathRow",
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_SUB,
     "mathml sub",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     @"AXMathSubscriptSuperscript",
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_SUP,
     "mathml sup",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     @"AXMathSubscriptSuperscript",
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_SUB_SUP,
     "mathml sub sup",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     @"AXMathSubscriptSuperscript",
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_UNDER,
     "mathml under",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     @"AXMathUnderOver",
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_OVER,
     "mathml over",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     @"AXMathUnderOver",
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_UNDER_OVER,
     "mathml under over",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     @"AXMathUnderOver",
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_MULTISCRIPTS,
     "mathml multiscripts",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     @"AXMathMultiscript",
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_TABLE,
     "mathml table",
     ATK_ROLE_TABLE,
     NSAccessibilityGroupRole,
     @"AXMathTable",
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_GRIDVIEW,
     eNoNameRule)

ROLE(MATHML_LABELED_ROW,
     "mathml labeled row",
     ATK_ROLE_TABLE_ROW,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_TABLE_ROW,
     "mathml table row",
     ATK_ROLE_TABLE_ROW,
     NSAccessibilityGroupRole,
     @"AXMathTableRow",
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_CELL,
     "mathml cell",
     ATK_ROLE_TABLE_CELL,
     NSAccessibilityGroupRole,
     @"AXMathTableCell",
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_ACTION,
     "mathml action",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_ERROR,
     "mathml error",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     @"AXMathRow",
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_STACK,
     "mathml stack",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_LONG_DIVISION,
     "mathml long division",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_STACK_GROUP,
     "mathml stack group",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_STACK_ROW,
     "mathml stack row",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_STACK_CARRIES,
     "mathml stack carries",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_STACK_CARRY,
     "mathml stack carry",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MATHML_STACK_LINE,
     "mathml stack line",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     0,
     IA2_ROLE_UNKNOWN,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(RADIO_GROUP,
     "grouping",
     ATK_ROLE_PANEL,
     NSAccessibilityRadioGroupRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_GROUPING,
     ROLE_SYSTEM_GROUPING,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(TEXT,
     "text",
     ATK_ROLE_STATIC,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_TEXT_FRAME,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeIfReqRule)

ROLE(DETAILS,
     "details",
     ATK_ROLE_PANEL,
     NSAccessibilityGroupRole,
     @"AXDetails",
     ROLE_SYSTEM_GROUPING,
     ROLE_SYSTEM_GROUPING,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(SUMMARY,
     "summary",
     ATK_ROLE_PUSH_BUTTON,
     NSAccessibilityButtonRole,
     @"AXSummary",
     ROLE_SYSTEM_PUSHBUTTON,
     ROLE_SYSTEM_PUSHBUTTON,
     java::SessionAccessibility::CLASSNAME_BUTTON,
     eNameFromSubtreeRule)

ROLE(LANDMARK,
     "landmark",
     ATK_ROLE_LANDMARK,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_LANDMARK,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(NAVIGATION,
     "navigation",
     ATK_ROLE_LANDMARK,
     NSAccessibilityGroupRole,
     @"AXLandmarkNavigation",
     USE_ROLE_STRING,
     IA2_ROLE_LANDMARK,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(FOOTNOTE,
     "footnote",
     ATK_ROLE_FOOTNOTE,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_FOOTNOTE,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(ARTICLE,
     "article",
     ATK_ROLE_ARTICLE,
     NSAccessibilityGroupRole,
     @"AXDocumentArticle",
     ROLE_SYSTEM_DOCUMENT,
     ROLE_SYSTEM_DOCUMENT,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(REGION,
     "region",
     ATK_ROLE_LANDMARK,
     NSAccessibilityGroupRole,
     @"AXLandmarkRegion",
     USE_ROLE_STRING,
     IA2_ROLE_LANDMARK,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

// A composite widget with a text input and popup. Used for ARIA role combobox.
// See also COMBOBOX.
ROLE(EDITCOMBOBOX,
     "editcombobox",
     ATK_ROLE_COMBO_BOX,
     NSAccessibilityComboBoxRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_COMBOBOX,
     ROLE_SYSTEM_COMBOBOX,
     java::SessionAccessibility::CLASSNAME_EDITTEXT,
     eNameFromValueRule)

ROLE(BLOCKQUOTE,
     "blockquote",
     ATK_ROLE_BLOCK_QUOTE,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_GROUPING,
     IA2_ROLE_BLOCK_QUOTE,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(CONTENT_DELETION,
     "content deletion",
     ATK_ROLE_CONTENT_DELETION,
     NSAccessibilityGroupRole,
     @"AXDeleteStyleGroup",
     USE_ROLE_STRING,
     IA2_ROLE_CONTENT_DELETION,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(CONTENT_INSERTION,
     "content insertion",
     ATK_ROLE_CONTENT_INSERTION,
     NSAccessibilityGroupRole,
     @"AXInsertStyleGroup",
     USE_ROLE_STRING,
     IA2_ROLE_CONTENT_INSERTION,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(FORM_LANDMARK,
     "form",
     ATK_ROLE_LANDMARK,
     NSAccessibilityGroupRole,
     @"AXLandmarkForm",
     USE_ROLE_STRING,
     IA2_ROLE_FORM,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(MARK,
     "mark",
     ATK_ROLE_MARK,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_MARK,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeIfReqRule)

ROLE(SUGGESTION,
     "suggestion",
     ATK_ROLE_SUGGESTION,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_SUGGESTION,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(COMMENT,
     "comment",
     ATK_ROLE_COMMENT,
     NSAccessibilityGroupRole,
     NSAccessibilityUnknownSubrole,
     USE_ROLE_STRING,
     IA2_ROLE_COMMENT,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(CODE,
     "code",
     ATK_ROLE_STATIC,
     NSAccessibilityGroupRole,
     @"AXCodeStyleGroup",
     USE_ROLE_STRING,
     IA2_ROLE_TEXT_FRAME,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(TIME_EDITOR,
     "time editor",
     ATK_ROLE_PANEL,
     @"AXTimeField",
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_GROUPING,
     ROLE_SYSTEM_GROUPING,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromSubtreeIfReqRule)

ROLE(LISTITEM_MARKER,
     "list item marker",
     ATK_ROLE_UNKNOWN,
     @"AXListMarker",
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_STATICTEXT,
     ROLE_SYSTEM_STATICTEXT,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(METER,
     "meter",
     ATK_ROLE_LEVEL_BAR,
     @"AXLevelIndicator",
     NSAccessibilityUnknownSubrole,
     ROLE_SYSTEM_PROGRESSBAR,
     ROLE_SYSTEM_PROGRESSBAR,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNameFromValueRule)

ROLE(SUBSCRIPT,
     "subscript",
     ATK_ROLE_SUBSCRIPT,
     NSAccessibilityGroupRole,
     @"AXSubscriptStyleGroup",
     ROLE_SYSTEM_GROUPING,
     IA2_ROLE_TEXT_FRAME,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)

ROLE(SUPERSCRIPT,
     "superscript",
     ATK_ROLE_SUPERSCRIPT,
     NSAccessibilityGroupRole,
     @"AXSuperscriptStyleGroup",
     ROLE_SYSTEM_GROUPING,
     IA2_ROLE_TEXT_FRAME,
     java::SessionAccessibility::CLASSNAME_VIEW,
     eNoNameRule)
// clang-format on
