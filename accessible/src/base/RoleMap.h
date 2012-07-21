/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Usage: declare the macro ROLE()with the following arguments:
 * ROLE(geckoRole, stringRole, atkRole, macRole, msaaRole, ia2Role, nameRule)
 */

ROLE(NOTHING,
     "nothing",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,
     USE_ROLE_STRING,
     IA2_ROLE_UNKNOWN,
     eFromSubtreeIfRec)

ROLE(TITLEBAR,
     "titlebar",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,  //Irrelevant on OS X; windows are always native.
     ROLE_SYSTEM_TITLEBAR,
     ROLE_SYSTEM_TITLEBAR,
     eNoRule)

ROLE(MENUBAR,
     "menubar",
     ATK_ROLE_MENU_BAR,
     NSAccessibilityMenuBarRole,  //Irrelevant on OS X; the menubar will always be native and on the top of the screen.
     ROLE_SYSTEM_MENUBAR,
     ROLE_SYSTEM_MENUBAR,
     eNoRule)

ROLE(SCROLLBAR,
     "scrollbar",
     ATK_ROLE_SCROLL_BAR,
     NSAccessibilityScrollBarRole,  //We might need to make this its own mozAccessible, to support the children objects (valueindicator, down/up buttons).
     ROLE_SYSTEM_SCROLLBAR,
     ROLE_SYSTEM_SCROLLBAR,
     eNoRule)

ROLE(GRIP,
     "grip",
     ATK_ROLE_UNKNOWN,
     NSAccessibilitySplitterRole,
     ROLE_SYSTEM_GRIP,
     ROLE_SYSTEM_GRIP,
     eNoRule)

ROLE(SOUND,
     "sound",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,  //Unused on OS X.
     ROLE_SYSTEM_SOUND,
     ROLE_SYSTEM_SOUND,
     eNoRule)

ROLE(CURSOR,
     "cursor",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,  //Unused on OS X.
     ROLE_SYSTEM_CURSOR,
     ROLE_SYSTEM_CURSOR,
     eNoRule)

ROLE(CARET,
     "caret",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,  //Unused on OS X.
     ROLE_SYSTEM_CARET,
     ROLE_SYSTEM_CARET,
     eNoRule)

ROLE(ALERT,
     "alert",
     ATK_ROLE_ALERT,
     NSAccessibilityWindowRole,
     ROLE_SYSTEM_ALERT,
     ROLE_SYSTEM_ALERT,
     eNoRule)

ROLE(WINDOW,
     "window",
     ATK_ROLE_WINDOW,
     NSAccessibilityWindowRole,  //Irrelevant on OS X; all window a11y is handled by the system.
     ROLE_SYSTEM_WINDOW,
     ROLE_SYSTEM_WINDOW,
     eNoRule)

ROLE(INTERNAL_FRAME,
     "internal frame",
     ATK_ROLE_INTERNAL_FRAME,
     NSAccessibilityScrollAreaRole,
     USE_ROLE_STRING,
     IA2_ROLE_INTERNAL_FRAME,
     eNoRule)

ROLE(MENUPOPUP,
     "menupopup",
     ATK_ROLE_MENU,
     NSAccessibilityMenuRole,  //The parent of menuitems.
     ROLE_SYSTEM_MENUPOPUP,
     ROLE_SYSTEM_MENUPOPUP,
     eNoRule)

ROLE(MENUITEM,
     "menuitem",
     ATK_ROLE_MENU_ITEM,
     NSAccessibilityMenuItemRole,
     ROLE_SYSTEM_MENUITEM,
     ROLE_SYSTEM_MENUITEM,
     eFromSubtree)

ROLE(TOOLTIP,
     "tooltip",
     ATK_ROLE_TOOL_TIP,
     @"AXHelpTag",  //10.4+ only, so we re-define the constant.
     ROLE_SYSTEM_TOOLTIP,
     ROLE_SYSTEM_TOOLTIP,
     eFromSubtree)

ROLE(APPLICATION,
     "application",
     ATK_ROLE_EMBEDDED,
     NSAccessibilityGroupRole,  //Unused on OS X. the system will take care of this.
     ROLE_SYSTEM_APPLICATION,
     ROLE_SYSTEM_APPLICATION,
     eNoRule)

ROLE(DOCUMENT,
     "document",
     ATK_ROLE_DOCUMENT_FRAME,
     @"AXWebArea",
     ROLE_SYSTEM_DOCUMENT,
     ROLE_SYSTEM_DOCUMENT,
     eNoRule)

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
     ROLE_SYSTEM_GROUPING,
     ROLE_SYSTEM_GROUPING,
     eNoRule)

ROLE(CHART,
     "chart",
     ATK_ROLE_CHART,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_CHART,
     ROLE_SYSTEM_CHART,
     eNoRule)

ROLE(DIALOG,
     "dialog",
     ATK_ROLE_DIALOG,
     NSAccessibilityWindowRole,  //There's a dialog subrole.
     ROLE_SYSTEM_DIALOG,
     ROLE_SYSTEM_DIALOG,
     eNoRule)

ROLE(BORDER,
     "border",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,  //Unused on OS X.
     ROLE_SYSTEM_BORDER,
     ROLE_SYSTEM_BORDER,
     eNoRule)

ROLE(GROUPING,
     "grouping",
     ATK_ROLE_PANEL,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_GROUPING,
     ROLE_SYSTEM_GROUPING,
     eNoRule)

ROLE(SEPARATOR,
     "separator",
     ATK_ROLE_SEPARATOR,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_SEPARATOR,
     ROLE_SYSTEM_SEPARATOR,
     eNoRule)

ROLE(TOOLBAR,
     "toolbar",
     ATK_ROLE_TOOL_BAR,
     NSAccessibilityToolbarRole,
     ROLE_SYSTEM_TOOLBAR,
     ROLE_SYSTEM_TOOLBAR,
     eNoRule)

ROLE(STATUSBAR,
     "statusbar",
     ATK_ROLE_STATUSBAR,
     NSAccessibilityUnknownRole,  //Doesn't exist on OS X (a status bar is its parts; a progressbar, a label, etc.)
     ROLE_SYSTEM_STATUSBAR,
     ROLE_SYSTEM_STATUSBAR,
     eNoRule)

ROLE(TABLE,
     "table",
     ATK_ROLE_TABLE,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_TABLE,
     ROLE_SYSTEM_TABLE,
     eNoRule)

ROLE(COLUMNHEADER,
     "columnheader",
     ATK_ROLE_COLUMN_HEADER,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_COLUMNHEADER,
     ROLE_SYSTEM_COLUMNHEADER,
     eFromSubtree)

ROLE(ROWHEADER,
     "rowheader",
     ATK_ROLE_ROW_HEADER,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_ROWHEADER,
     ROLE_SYSTEM_ROWHEADER,
     eFromSubtree)

ROLE(COLUMN,
     "column",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityColumnRole,
     ROLE_SYSTEM_COLUMN,
     ROLE_SYSTEM_COLUMN,
     eFromSubtree)

ROLE(ROW,
     "row",
     ATK_ROLE_LIST_ITEM,
     NSAccessibilityRowRole,
     ROLE_SYSTEM_ROW,
     ROLE_SYSTEM_ROW,
     eFromSubtree)

ROLE(CELL,
     "cell",
     ATK_ROLE_TABLE_CELL,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_CELL,
     ROLE_SYSTEM_CELL,
     eFromSubtreeIfRec)

ROLE(LINK,
     "link",
     ATK_ROLE_LINK,
     @"AXLink",  //10.4+ the attr first define in SDK 10.4, so we define it here too. ROLE_LINK
     ROLE_SYSTEM_LINK,
     ROLE_SYSTEM_LINK,
     eFromSubtree)

ROLE(HELPBALLOON,
     "helpballoon",
     ATK_ROLE_UNKNOWN,
     @"AXHelpTag",
     ROLE_SYSTEM_HELPBALLOON,
     ROLE_SYSTEM_HELPBALLOON,
     eFromSubtree)

ROLE(CHARACTER,
     "character",
     ATK_ROLE_IMAGE,
     NSAccessibilityUnknownRole,  //Unused on OS X.
     ROLE_SYSTEM_CHARACTER,
     ROLE_SYSTEM_CHARACTER,
     eNoRule)

ROLE(LIST,
     "list",
     ATK_ROLE_LIST,
     NSAccessibilityListRole,
     ROLE_SYSTEM_LIST,
     ROLE_SYSTEM_LIST,
     eFromSubtreeIfRec)

ROLE(LISTITEM,
     "listitem",
     ATK_ROLE_LIST_ITEM,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_LISTITEM,
     ROLE_SYSTEM_LISTITEM,
     eFromSubtree)

ROLE(OUTLINE,
     "outline",
     ATK_ROLE_TREE,
     NSAccessibilityOutlineRole,
     ROLE_SYSTEM_OUTLINE,
     ROLE_SYSTEM_OUTLINE,
     eNoRule)

ROLE(OUTLINEITEM,
     "outlineitem",
     ATK_ROLE_LIST_ITEM,
     NSAccessibilityRowRole,  //XXX: use OutlineRow as subrole.
     ROLE_SYSTEM_OUTLINEITEM,
     ROLE_SYSTEM_OUTLINEITEM,
     eFromSubtree)

ROLE(PAGETAB,
     "pagetab",
     ATK_ROLE_PAGE_TAB,
     NSAccessibilityRadioButtonRole,
     ROLE_SYSTEM_PAGETAB,
     ROLE_SYSTEM_PAGETAB,
     eFromSubtree)

ROLE(PROPERTYPAGE,
     "propertypage",
     ATK_ROLE_SCROLL_PANE,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_PROPERTYPAGE,
     ROLE_SYSTEM_PROPERTYPAGE,
     eNoRule)

ROLE(INDICATOR,
     "indicator",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_INDICATOR,
     ROLE_SYSTEM_INDICATOR,
     eNoRule)

ROLE(GRAPHIC,
     "graphic",
     ATK_ROLE_IMAGE,
     NSAccessibilityImageRole,
     ROLE_SYSTEM_GRAPHIC,
     ROLE_SYSTEM_GRAPHIC,
     eNoRule)

ROLE(STATICTEXT,
     "statictext",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityStaticTextRole,
     ROLE_SYSTEM_STATICTEXT,
     ROLE_SYSTEM_STATICTEXT,
     eNoRule)

ROLE(TEXT_LEAF,
     "text leaf",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityStaticTextRole,
     ROLE_SYSTEM_TEXT,
     ROLE_SYSTEM_TEXT,
     eNoRule)

ROLE(PUSHBUTTON,
     "pushbutton",
     ATK_ROLE_PUSH_BUTTON,
     NSAccessibilityButtonRole,
     ROLE_SYSTEM_PUSHBUTTON,
     ROLE_SYSTEM_PUSHBUTTON,
     eFromSubtree)

ROLE(CHECKBUTTON,
     "checkbutton",
     ATK_ROLE_CHECK_BOX,
     NSAccessibilityCheckBoxRole,
     ROLE_SYSTEM_CHECKBUTTON,
     ROLE_SYSTEM_CHECKBUTTON,
     eFromSubtree)

ROLE(RADIOBUTTON,
     "radiobutton",
     ATK_ROLE_RADIO_BUTTON,
     NSAccessibilityRadioButtonRole,
     ROLE_SYSTEM_RADIOBUTTON,
     ROLE_SYSTEM_RADIOBUTTON,
     eFromSubtree)

ROLE(COMBOBOX,
     "combobox",
     ATK_ROLE_COMBO_BOX,
     NSAccessibilityPopUpButtonRole,
     ROLE_SYSTEM_COMBOBOX,
     ROLE_SYSTEM_COMBOBOX,
     eFromValue)

ROLE(DROPLIST,
     "droplist",
     ATK_ROLE_COMBO_BOX,
     NSAccessibilityPopUpButtonRole,
     ROLE_SYSTEM_DROPLIST,
     ROLE_SYSTEM_DROPLIST,
     eNoRule)

ROLE(PROGRESSBAR,
     "progressbar",
     ATK_ROLE_PROGRESS_BAR,
     NSAccessibilityProgressIndicatorRole,
     ROLE_SYSTEM_PROGRESSBAR,
     ROLE_SYSTEM_PROGRESSBAR,
     eFromValue)

ROLE(DIAL,
     "dial",
     ATK_ROLE_DIAL,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_DIAL,
     ROLE_SYSTEM_DIAL,
     eNoRule)

ROLE(HOTKEYFIELD,
     "hotkeyfield",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_HOTKEYFIELD,
     ROLE_SYSTEM_HOTKEYFIELD,
     eNoRule)

ROLE(SLIDER,
     "slider",
     ATK_ROLE_SLIDER,
     NSAccessibilitySliderRole,
     ROLE_SYSTEM_SLIDER,
     ROLE_SYSTEM_SLIDER,
     eNoRule)

ROLE(SPINBUTTON,
     "spinbutton",
     ATK_ROLE_SPIN_BUTTON,
     NSAccessibilityIncrementorRole,  //Subroles: Increment/Decrement.
     ROLE_SYSTEM_SPINBUTTON,
     ROLE_SYSTEM_SPINBUTTON,
     eNoRule)

ROLE(DIAGRAM,
     "diagram",
     ATK_ROLE_IMAGE,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_DIAGRAM,
     ROLE_SYSTEM_DIAGRAM,
     eNoRule)

ROLE(ANIMATION,
     "animation",
     ATK_ROLE_ANIMATION,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_ANIMATION,
     ROLE_SYSTEM_ANIMATION,
     eNoRule)

ROLE(EQUATION,
     "equation",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_EQUATION,
     ROLE_SYSTEM_EQUATION,
     eNoRule)

ROLE(BUTTONDROPDOWN,
     "buttondropdown",
     ATK_ROLE_PUSH_BUTTON,
     NSAccessibilityPopUpButtonRole,
     ROLE_SYSTEM_BUTTONDROPDOWN,
     ROLE_SYSTEM_BUTTONDROPDOWN,
     eFromSubtree)

ROLE(BUTTONMENU,
     "buttonmenu",
     ATK_ROLE_PUSH_BUTTON,
     NSAccessibilityMenuButtonRole,
     ROLE_SYSTEM_BUTTONMENU,
     ROLE_SYSTEM_BUTTONMENU,
     eFromSubtree)

ROLE(BUTTONDROPDOWNGRID,
     "buttondropdowngrid",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_BUTTONDROPDOWNGRID,
     ROLE_SYSTEM_BUTTONDROPDOWNGRID,
     eFromSubtree)

ROLE(WHITESPACE,
     "whitespace",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_WHITESPACE,
     ROLE_SYSTEM_WHITESPACE,
     eNoRule)

ROLE(PAGETABLIST,
     "pagetablist",
     ATK_ROLE_PAGE_TAB_LIST,
     NSAccessibilityTabGroupRole,
     ROLE_SYSTEM_PAGETABLIST,
     ROLE_SYSTEM_PAGETABLIST,
     eNoRule)

ROLE(CLOCK,
     "clock",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,  //Unused on OS X
     ROLE_SYSTEM_CLOCK,
     ROLE_SYSTEM_CLOCK,
     eNoRule)

ROLE(SPLITBUTTON,
     "splitbutton",
     ATK_ROLE_PUSH_BUTTON,
     NSAccessibilityButtonRole,
     ROLE_SYSTEM_SPLITBUTTON,
     ROLE_SYSTEM_SPLITBUTTON,
     eNoRule)

ROLE(IPADDRESS,
     "ipaddress",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_IPADDRESS,
     ROLE_SYSTEM_IPADDRESS,
     eNoRule)

ROLE(ACCEL_LABEL,
     "accel label",
     ATK_ROLE_ACCEL_LABEL,
     NSAccessibilityStaticTextRole,
     ROLE_SYSTEM_STATICTEXT,
     ROLE_SYSTEM_STATICTEXT,
     eNoRule)

ROLE(ARROW,
     "arrow",
     ATK_ROLE_ARROW,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_INDICATOR,
     ROLE_SYSTEM_INDICATOR,
     eNoRule)

ROLE(CANVAS,
     "canvas",
     ATK_ROLE_CANVAS,
     NSAccessibilityImageRole,
     USE_ROLE_STRING,
     IA2_ROLE_CANVAS,
     eNoRule)

ROLE(CHECK_MENU_ITEM,
     "check menu item",
     ATK_ROLE_CHECK_MENU_ITEM,
     NSAccessibilityMenuItemRole,
     ROLE_SYSTEM_MENUITEM,
     IA2_ROLE_CHECK_MENU_ITEM,
     eFromSubtree)

ROLE(COLOR_CHOOSER,
     "color chooser",
     ATK_ROLE_COLOR_CHOOSER,
     NSAccessibilityColorWellRole,
     ROLE_SYSTEM_DIALOG,
     IA2_ROLE_COLOR_CHOOSER,
     eNoRule)

ROLE(DATE_EDITOR,
     "date editor",
     ATK_ROLE_DATE_EDITOR,
     NSAccessibilityUnknownRole,
     USE_ROLE_STRING,
     IA2_ROLE_DATE_EDITOR,
     eNoRule)

ROLE(DESKTOP_ICON,
     "desktop icon",
     ATK_ROLE_DESKTOP_ICON,
     NSAccessibilityImageRole,
     USE_ROLE_STRING,
     IA2_ROLE_DESKTOP_ICON,
     eNoRule)

ROLE(DESKTOP_FRAME,
     "desktop frame",
     ATK_ROLE_DESKTOP_FRAME,
     NSAccessibilityUnknownRole,
     USE_ROLE_STRING,
     IA2_ROLE_DESKTOP_PANE,
     eNoRule)

ROLE(DIRECTORY_PANE,
     "directory pane",
     ATK_ROLE_DIRECTORY_PANE,
     NSAccessibilityBrowserRole,
     USE_ROLE_STRING,
     IA2_ROLE_DIRECTORY_PANE,
     eNoRule)

ROLE(FILE_CHOOSER,
     "file chooser",
     ATK_ROLE_FILE_CHOOSER,
     NSAccessibilityUnknownRole,  //Unused on OS X
     USE_ROLE_STRING,
     IA2_ROLE_FILE_CHOOSER,
     eNoRule)

ROLE(FONT_CHOOSER,
     "font chooser",
     ATK_ROLE_FONT_CHOOSER,
     NSAccessibilityUnknownRole,
     USE_ROLE_STRING,
     IA2_ROLE_FONT_CHOOSER,
     eNoRule)

ROLE(CHROME_WINDOW,
     "chrome window",
     ATK_ROLE_FRAME,
     NSAccessibilityUnknownRole,  //Unused on OS X
     ROLE_SYSTEM_APPLICATION,
     IA2_ROLE_FRAME,
     eNoRule)

ROLE(GLASS_PANE,
     "glass pane",
     ATK_ROLE_GLASS_PANE,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_GLASS_PANE,
     eNoRule)

ROLE(HTML_CONTAINER,
     "html container",
     ATK_ROLE_HTML_CONTAINER,
     NSAccessibilityUnknownRole,
     USE_ROLE_STRING,
     IA2_ROLE_UNKNOWN,
     eFromSubtreeIfRec)

ROLE(ICON,
     "icon",
     ATK_ROLE_ICON,
     NSAccessibilityImageRole,
     ROLE_SYSTEM_PUSHBUTTON,
     IA2_ROLE_ICON,
     eNoRule)

ROLE(LABEL,
     "label",
     ATK_ROLE_LABEL,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_STATICTEXT,
     IA2_ROLE_LABEL,
     eFromSubtree)

ROLE(LAYERED_PANE,
     "layered pane",
     ATK_ROLE_LAYERED_PANE,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_LAYERED_PANE,
     eNoRule)

ROLE(OPTION_PANE,
     "option pane",
     ATK_ROLE_OPTION_PANE,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_OPTION_PANE,
     eNoRule)

ROLE(PASSWORD_TEXT,
     "password text",
     ATK_ROLE_PASSWORD_TEXT,
     NSAccessibilityTextFieldRole,
     ROLE_SYSTEM_TEXT,
     ROLE_SYSTEM_TEXT,
     eNoRule)

ROLE(POPUP_MENU,
     "popup menu",
     ATK_ROLE_POPUP_MENU,
     NSAccessibilityUnknownRole,  //Unused
     ROLE_SYSTEM_MENUPOPUP,
     ROLE_SYSTEM_MENUPOPUP,
     eNoRule)

ROLE(RADIO_MENU_ITEM,
     "radio menu item",
     ATK_ROLE_RADIO_MENU_ITEM,
     NSAccessibilityMenuItemRole,
     ROLE_SYSTEM_MENUITEM,
     IA2_ROLE_RADIO_MENU_ITEM,
     eFromSubtree)

ROLE(ROOT_PANE,
     "root pane",
     ATK_ROLE_ROOT_PANE,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_ROOT_PANE,
     eNoRule)

ROLE(SCROLL_PANE,
     "scroll pane",
     ATK_ROLE_SCROLL_PANE,
     NSAccessibilityScrollAreaRole,
     USE_ROLE_STRING,
     IA2_ROLE_SCROLL_PANE,
     eNoRule)

ROLE(SPLIT_PANE,
     "split pane",
     ATK_ROLE_SPLIT_PANE,
     NSAccessibilitySplitGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_SPLIT_PANE,
     eNoRule)

ROLE(TABLE_COLUMN_HEADER,
     "table column header",
     ATK_ROLE_TABLE_COLUMN_HEADER,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_COLUMNHEADER,
     ROLE_SYSTEM_COLUMNHEADER,
     eFromSubtree)

ROLE(TABLE_ROW_HEADER,
     "table row header",
     ATK_ROLE_TABLE_ROW_HEADER,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_ROWHEADER,
     ROLE_SYSTEM_ROWHEADER,
     eFromSubtree)

ROLE(TEAR_OFF_MENU_ITEM,
     "tear off menu item",
     ATK_ROLE_TEAR_OFF_MENU_ITEM,
     NSAccessibilityMenuItemRole,
     ROLE_SYSTEM_MENUITEM,
     IA2_ROLE_TEAR_OFF_MENU,
     eFromSubtree)

ROLE(TERMINAL,
     "terminal",
     ATK_ROLE_TERMINAL,
     NSAccessibilityUnknownRole,
     USE_ROLE_STRING,
     IA2_ROLE_TERMINAL,
     eNoRule)

ROLE(TEXT_CONTAINER,
     "text container",
     ATK_ROLE_TEXT,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_TEXT_FRAME,
     eFromSubtreeIfRec)

ROLE(TOGGLE_BUTTON,
     "toggle button",
     ATK_ROLE_TOGGLE_BUTTON,
     NSAccessibilityButtonRole,
     ROLE_SYSTEM_PUSHBUTTON,
     IA2_ROLE_TOGGLE_BUTTON,
     eFromSubtree)

ROLE(TREE_TABLE,
     "tree table",
     ATK_ROLE_TREE_TABLE,
     NSAccessibilityTableRole,
     ROLE_SYSTEM_OUTLINE,
     ROLE_SYSTEM_OUTLINE,
     eNoRule)

ROLE(VIEWPORT,
     "viewport",
     ATK_ROLE_VIEWPORT,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_PANE,
     IA2_ROLE_VIEW_PORT,
     eNoRule)

ROLE(HEADER,
     "header",
     ATK_ROLE_HEADER,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_HEADER,
     eNoRule)

ROLE(FOOTER,
     "footer",
     ATK_ROLE_FOOTER,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_FOOTER,
     eNoRule)

ROLE(PARAGRAPH,
     "paragraph",
     ATK_ROLE_PARAGRAPH,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_PARAGRAPH,
     eFromSubtreeIfRec)

ROLE(RULER,
     "ruler",
     ATK_ROLE_RULER,
     @"AXRuler",  //10.4+ only, so we re-define the constant.
     USE_ROLE_STRING,
     IA2_ROLE_RULER,
     eNoRule)

ROLE(AUTOCOMPLETE,
     "autocomplete",
     ATK_ROLE_AUTOCOMPLETE,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_COMBOBOX,
     ROLE_SYSTEM_COMBOBOX,
     eNoRule)

ROLE(EDITBAR,
     "editbar",
     ATK_ROLE_EDITBAR,
     NSAccessibilityTextFieldRole,
     ROLE_SYSTEM_TEXT,
     IA2_ROLE_EDITBAR,
     eNoRule)

ROLE(ENTRY,
     "entry",
     ATK_ROLE_ENTRY,
     NSAccessibilityTextFieldRole,
     ROLE_SYSTEM_TEXT,
     ROLE_SYSTEM_TEXT,
     eFromValue)

ROLE(CAPTION,
     "caption",
     ATK_ROLE_CAPTION,
     NSAccessibilityStaticTextRole,
     USE_ROLE_STRING,
     IA2_ROLE_CAPTION,
     eFromSubtreeIfRec)

ROLE(DOCUMENT_FRAME,
     "document frame",
     ATK_ROLE_DOCUMENT_FRAME,
     NSAccessibilityScrollAreaRole,
     USE_ROLE_STRING,
     IA2_ROLE_UNKNOWN,
     eNoRule)

ROLE(HEADING,
     "heading",
     ATK_ROLE_HEADING,
     @"AXHeading",
     USE_ROLE_STRING,
     IA2_ROLE_HEADING,
     eFromSubtreeIfRec)

ROLE(PAGE,
     "page",
     ATK_ROLE_PAGE,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_PAGE,
     eNoRule)

ROLE(SECTION,
     "section",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_SECTION,
     eFromSubtreeIfRec)

ROLE(REDUNDANT_OBJECT,
     "redundant object",
     ATK_ROLE_REDUNDANT_OBJECT,
     NSAccessibilityUnknownRole,
     USE_ROLE_STRING,
     IA2_ROLE_REDUNDANT_OBJECT,
     eNoRule)

ROLE(FORM,
     "form",
     ATK_ROLE_FORM,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_FORM,
     eNoRule)

ROLE(IME,
     "ime",
     ATK_ROLE_INPUT_METHOD_WINDOW,
     NSAccessibilityUnknownRole,
     USE_ROLE_STRING,
     IA2_ROLE_INPUT_METHOD_WINDOW,
     eNoRule)

ROLE(APP_ROOT,
     "app root",
     ATK_ROLE_APPLICATION,
     NSAccessibilityUnknownRole,  //Unused on OS X
     ROLE_SYSTEM_APPLICATION,
     ROLE_SYSTEM_APPLICATION,
     eNoRule)

ROLE(PARENT_MENUITEM,
     "parent menuitem",
     ATK_ROLE_MENU,
     NSAccessibilityMenuItemRole,
     ROLE_SYSTEM_MENUITEM,
     ROLE_SYSTEM_MENUITEM,
     eFromSubtree)

ROLE(CALENDAR,
     "calendar",
     ATK_ROLE_CALENDAR,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_CLIENT,
     ROLE_SYSTEM_CLIENT,
     eNoRule)

ROLE(COMBOBOX_LIST,
     "combobox list",
     ATK_ROLE_MENU,
     NSAccessibilityMenuRole,
     ROLE_SYSTEM_LIST,
     ROLE_SYSTEM_LIST,
     eNoRule)

ROLE(COMBOBOX_OPTION,
     "combobox option",
     ATK_ROLE_MENU_ITEM,
     NSAccessibilityMenuItemRole,
     ROLE_SYSTEM_LISTITEM,
     ROLE_SYSTEM_LISTITEM,
     eFromSubtree)

ROLE(IMAGE_MAP,
     "image map",
     ATK_ROLE_IMAGE,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_GRAPHIC,
     ROLE_SYSTEM_GRAPHIC,
     eNoRule)

ROLE(OPTION,
     "listbox option",
     ATK_ROLE_LIST_ITEM,
     NSAccessibilityRowRole,
     ROLE_SYSTEM_LISTITEM,
     ROLE_SYSTEM_LISTITEM,
     eFromSubtree)

ROLE(RICH_OPTION,
     "listbox rich option",
     ATK_ROLE_LIST_ITEM,
     NSAccessibilityRowRole,
     ROLE_SYSTEM_LISTITEM,
     ROLE_SYSTEM_LISTITEM,
     eFromSubtree)

ROLE(LISTBOX,
     "listbox",
     ATK_ROLE_LIST,
     NSAccessibilityListRole,
     ROLE_SYSTEM_LIST,
     ROLE_SYSTEM_LIST,
     eNoRule)

ROLE(FLAT_EQUATION,
     "flat equation",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_EQUATION,
     ROLE_SYSTEM_EQUATION,
     eNoRule)

ROLE(GRID_CELL,
     "gridcell",
     ATK_ROLE_TABLE_CELL,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_CELL,
     ROLE_SYSTEM_CELL,
     eFromSubtree)

ROLE(EMBEDDED_OBJECT,
     "embedded object",
     ATK_ROLE_PANEL,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_EMBEDDED_OBJECT,
     eNoRule)

ROLE(NOTE,
     "note",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_NOTE,
     eFromSubtree)

ROLE(FIGURE,
     "figure",
     ATK_ROLE_PANEL,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_GROUPING,
     ROLE_SYSTEM_GROUPING,
     eNoRule)

ROLE(CHECK_RICH_OPTION,
     "check rich option",
     ATK_ROLE_CHECK_BOX,
     NSAccessibilityCheckBoxRole,
     ROLE_SYSTEM_CHECKBUTTON,
     ROLE_SYSTEM_CHECKBUTTON,
     eFromSubtree)

ROLE(DEFINITION_LIST,
     "definitionlist",
     ATK_ROLE_LIST,
     NSAccessibilityListRole,
     ROLE_SYSTEM_LIST,
     ROLE_SYSTEM_LIST,
     eFromSubtreeIfRec)

ROLE(TERM,
     "term",
     ATK_ROLE_LIST_ITEM,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_LISTITEM,
     ROLE_SYSTEM_LISTITEM,
     eFromSubtree)

ROLE(DEFINITION,
     "definition",
     ATK_ROLE_PARAGRAPH,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_PARAGRAPH,
     eFromSubtree)
