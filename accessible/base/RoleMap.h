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
     eNameFromSubtreeIfReqRule)

ROLE(TITLEBAR,
     "titlebar",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,  //Irrelevant on OS X; windows are always native.
     ROLE_SYSTEM_TITLEBAR,
     ROLE_SYSTEM_TITLEBAR,
     eNoNameRule)

ROLE(MENUBAR,
     "menubar",
     ATK_ROLE_MENU_BAR,
     NSAccessibilityMenuBarRole,  //Irrelevant on OS X; the menubar will always be native and on the top of the screen.
     ROLE_SYSTEM_MENUBAR,
     ROLE_SYSTEM_MENUBAR,
     eNoNameRule)

ROLE(SCROLLBAR,
     "scrollbar",
     ATK_ROLE_SCROLL_BAR,
     NSAccessibilityScrollBarRole,  //We might need to make this its own mozAccessible, to support the children objects (valueindicator, down/up buttons).
     ROLE_SYSTEM_SCROLLBAR,
     ROLE_SYSTEM_SCROLLBAR,
     eNameFromValueRule)

ROLE(GRIP,
     "grip",
     ATK_ROLE_UNKNOWN,
     NSAccessibilitySplitterRole,
     ROLE_SYSTEM_GRIP,
     ROLE_SYSTEM_GRIP,
     eNoNameRule)

ROLE(SOUND,
     "sound",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,  //Unused on OS X.
     ROLE_SYSTEM_SOUND,
     ROLE_SYSTEM_SOUND,
     eNoNameRule)

ROLE(CURSOR,
     "cursor",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,  //Unused on OS X.
     ROLE_SYSTEM_CURSOR,
     ROLE_SYSTEM_CURSOR,
     eNoNameRule)

ROLE(CARET,
     "caret",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,  //Unused on OS X.
     ROLE_SYSTEM_CARET,
     ROLE_SYSTEM_CARET,
     eNoNameRule)

ROLE(ALERT,
     "alert",
     ATK_ROLE_ALERT,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_ALERT,
     ROLE_SYSTEM_ALERT,
     eNoNameRule)

ROLE(WINDOW,
     "window",
     ATK_ROLE_WINDOW,
     NSAccessibilityWindowRole,  //Irrelevant on OS X; all window a11y is handled by the system.
     ROLE_SYSTEM_WINDOW,
     ROLE_SYSTEM_WINDOW,
     eNoNameRule)

ROLE(INTERNAL_FRAME,
     "internal frame",
     ATK_ROLE_INTERNAL_FRAME,
     NSAccessibilityScrollAreaRole,
     USE_ROLE_STRING,
     IA2_ROLE_INTERNAL_FRAME,
     eNoNameRule)

ROLE(MENUPOPUP,
     "menupopup",
     ATK_ROLE_MENU,
     NSAccessibilityMenuRole,  //The parent of menuitems.
     ROLE_SYSTEM_MENUPOPUP,
     ROLE_SYSTEM_MENUPOPUP,
     eNoNameRule)

ROLE(MENUITEM,
     "menuitem",
     ATK_ROLE_MENU_ITEM,
     NSAccessibilityMenuItemRole,
     ROLE_SYSTEM_MENUITEM,
     ROLE_SYSTEM_MENUITEM,
     eNameFromSubtreeRule)

ROLE(TOOLTIP,
     "tooltip",
     ATK_ROLE_TOOL_TIP,
     @"AXHelpTag",  //10.4+ only, so we re-define the constant.
     ROLE_SYSTEM_TOOLTIP,
     ROLE_SYSTEM_TOOLTIP,
     eNameFromSubtreeRule)

ROLE(APPLICATION,
     "application",
     ATK_ROLE_EMBEDDED,
     NSAccessibilityGroupRole,  //Unused on OS X. the system will take care of this.
     ROLE_SYSTEM_APPLICATION,
     ROLE_SYSTEM_APPLICATION,
     eNoNameRule)

ROLE(DOCUMENT,
     "document",
     ATK_ROLE_DOCUMENT_FRAME,
     @"AXWebArea",
     ROLE_SYSTEM_DOCUMENT,
     ROLE_SYSTEM_DOCUMENT,
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
     ROLE_SYSTEM_GROUPING,
     ROLE_SYSTEM_GROUPING,
     eNoNameRule)

ROLE(CHART,
     "chart",
     ATK_ROLE_CHART,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_CHART,
     ROLE_SYSTEM_CHART,
     eNoNameRule)

ROLE(DIALOG,
     "dialog",
     ATK_ROLE_DIALOG,
     NSAccessibilityWindowRole,  //There's a dialog subrole.
     ROLE_SYSTEM_DIALOG,
     ROLE_SYSTEM_DIALOG,
     eNoNameRule)

ROLE(BORDER,
     "border",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,  //Unused on OS X.
     ROLE_SYSTEM_BORDER,
     ROLE_SYSTEM_BORDER,
     eNoNameRule)

ROLE(GROUPING,
     "grouping",
     ATK_ROLE_PANEL,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_GROUPING,
     ROLE_SYSTEM_GROUPING,
     eNoNameRule)

ROLE(SEPARATOR,
     "separator",
     ATK_ROLE_SEPARATOR,
     NSAccessibilitySplitterRole,
     ROLE_SYSTEM_SEPARATOR,
     ROLE_SYSTEM_SEPARATOR,
     eNoNameRule)

ROLE(TOOLBAR,
     "toolbar",
     ATK_ROLE_TOOL_BAR,
     NSAccessibilityToolbarRole,
     ROLE_SYSTEM_TOOLBAR,
     ROLE_SYSTEM_TOOLBAR,
     eNoNameRule)

ROLE(STATUSBAR,
     "statusbar",
     ATK_ROLE_STATUSBAR,
     NSAccessibilityUnknownRole,  //Doesn't exist on OS X (a status bar is its parts; a progressbar, a label, etc.)
     ROLE_SYSTEM_STATUSBAR,
     ROLE_SYSTEM_STATUSBAR,
     eNoNameRule)

ROLE(TABLE,
     "table",
     ATK_ROLE_TABLE,
     NSAccessibilityTableRole,
     ROLE_SYSTEM_TABLE,
     ROLE_SYSTEM_TABLE,
     eNoNameRule)

ROLE(COLUMNHEADER,
     "columnheader",
     ATK_ROLE_COLUMN_HEADER,
     NSAccessibilityCellRole,
     ROLE_SYSTEM_COLUMNHEADER,
     ROLE_SYSTEM_COLUMNHEADER,
     eNameFromSubtreeRule)

ROLE(ROWHEADER,
     "rowheader",
     ATK_ROLE_ROW_HEADER,
     NSAccessibilityCellRole,
     ROLE_SYSTEM_ROWHEADER,
     ROLE_SYSTEM_ROWHEADER,
     eNameFromSubtreeRule)

ROLE(COLUMN,
     "column",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityColumnRole,
     ROLE_SYSTEM_COLUMN,
     ROLE_SYSTEM_COLUMN,
     eNameFromSubtreeRule)

ROLE(ROW,
     "row",
     ATK_ROLE_TABLE_ROW,
     NSAccessibilityRowRole,
     ROLE_SYSTEM_ROW,
     ROLE_SYSTEM_ROW,
     eNameFromSubtreeRule)

ROLE(CELL,
     "cell",
     ATK_ROLE_TABLE_CELL,
     NSAccessibilityCellRole,
     ROLE_SYSTEM_CELL,
     ROLE_SYSTEM_CELL,
     eNameFromSubtreeIfReqRule)

ROLE(LINK,
     "link",
     ATK_ROLE_LINK,
     @"AXLink",  //10.4+ the attr first define in SDK 10.4, so we define it here too. ROLE_LINK
     ROLE_SYSTEM_LINK,
     ROLE_SYSTEM_LINK,
     eNameFromSubtreeRule)

ROLE(HELPBALLOON,
     "helpballoon",
     ATK_ROLE_UNKNOWN,
     @"AXHelpTag",
     ROLE_SYSTEM_HELPBALLOON,
     ROLE_SYSTEM_HELPBALLOON,
     eNameFromSubtreeRule)

ROLE(CHARACTER,
     "character",
     ATK_ROLE_IMAGE,
     NSAccessibilityUnknownRole,  //Unused on OS X.
     ROLE_SYSTEM_CHARACTER,
     ROLE_SYSTEM_CHARACTER,
     eNoNameRule)

ROLE(LIST,
     "list",
     ATK_ROLE_LIST,
     NSAccessibilityListRole,
     ROLE_SYSTEM_LIST,
     ROLE_SYSTEM_LIST,
     eNameFromSubtreeIfReqRule)

ROLE(LISTITEM,
     "listitem",
     ATK_ROLE_LIST_ITEM,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_LISTITEM,
     ROLE_SYSTEM_LISTITEM,
     eNameFromSubtreeRule)

ROLE(OUTLINE,
     "outline",
     ATK_ROLE_TREE,
     NSAccessibilityOutlineRole,
     ROLE_SYSTEM_OUTLINE,
     ROLE_SYSTEM_OUTLINE,
     eNoNameRule)

ROLE(OUTLINEITEM,
     "outlineitem",
     ATK_ROLE_TREE_ITEM,
     NSAccessibilityRowRole,
     ROLE_SYSTEM_OUTLINEITEM,
     ROLE_SYSTEM_OUTLINEITEM,
     eNameFromSubtreeRule)

ROLE(PAGETAB,
     "pagetab",
     ATK_ROLE_PAGE_TAB,
     NSAccessibilityRadioButtonRole,
     ROLE_SYSTEM_PAGETAB,
     ROLE_SYSTEM_PAGETAB,
     eNameFromSubtreeRule)

ROLE(PROPERTYPAGE,
     "propertypage",
     ATK_ROLE_SCROLL_PANE,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_PROPERTYPAGE,
     ROLE_SYSTEM_PROPERTYPAGE,
     eNoNameRule)

ROLE(INDICATOR,
     "indicator",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_INDICATOR,
     ROLE_SYSTEM_INDICATOR,
     eNoNameRule)

ROLE(GRAPHIC,
     "graphic",
     ATK_ROLE_IMAGE,
     NSAccessibilityImageRole,
     ROLE_SYSTEM_GRAPHIC,
     ROLE_SYSTEM_GRAPHIC,
     eNoNameRule)

ROLE(STATICTEXT,
     "statictext",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityStaticTextRole,
     ROLE_SYSTEM_STATICTEXT,
     ROLE_SYSTEM_STATICTEXT,
     eNoNameRule)

ROLE(TEXT_LEAF,
     "text leaf",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityStaticTextRole,
     ROLE_SYSTEM_TEXT,
     ROLE_SYSTEM_TEXT,
     eNoNameRule)

ROLE(PUSHBUTTON,
     "pushbutton",
     ATK_ROLE_PUSH_BUTTON,
     NSAccessibilityButtonRole,
     ROLE_SYSTEM_PUSHBUTTON,
     ROLE_SYSTEM_PUSHBUTTON,
     eNameFromSubtreeRule)

ROLE(CHECKBUTTON,
     "checkbutton",
     ATK_ROLE_CHECK_BOX,
     NSAccessibilityCheckBoxRole,
     ROLE_SYSTEM_CHECKBUTTON,
     ROLE_SYSTEM_CHECKBUTTON,
     eNameFromSubtreeRule)

ROLE(RADIOBUTTON,
     "radiobutton",
     ATK_ROLE_RADIO_BUTTON,
     NSAccessibilityRadioButtonRole,
     ROLE_SYSTEM_RADIOBUTTON,
     ROLE_SYSTEM_RADIOBUTTON,
     eNameFromSubtreeRule)

// Equivalent of HTML select element with size="1". See also EDITCOMBOBOX.
ROLE(COMBOBOX,
     "combobox",
     ATK_ROLE_COMBO_BOX,
     NSAccessibilityPopUpButtonRole,
     ROLE_SYSTEM_COMBOBOX,
     ROLE_SYSTEM_COMBOBOX,
     eNameFromValueRule)

ROLE(DROPLIST,
     "droplist",
     ATK_ROLE_COMBO_BOX,
     NSAccessibilityPopUpButtonRole,
     ROLE_SYSTEM_DROPLIST,
     ROLE_SYSTEM_DROPLIST,
     eNoNameRule)

ROLE(PROGRESSBAR,
     "progressbar",
     ATK_ROLE_PROGRESS_BAR,
     NSAccessibilityProgressIndicatorRole,
     ROLE_SYSTEM_PROGRESSBAR,
     ROLE_SYSTEM_PROGRESSBAR,
     eNameFromValueRule)

ROLE(DIAL,
     "dial",
     ATK_ROLE_DIAL,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_DIAL,
     ROLE_SYSTEM_DIAL,
     eNoNameRule)

ROLE(HOTKEYFIELD,
     "hotkeyfield",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_HOTKEYFIELD,
     ROLE_SYSTEM_HOTKEYFIELD,
     eNoNameRule)

ROLE(SLIDER,
     "slider",
     ATK_ROLE_SLIDER,
     NSAccessibilitySliderRole,
     ROLE_SYSTEM_SLIDER,
     ROLE_SYSTEM_SLIDER,
     eNameFromValueRule)

ROLE(SPINBUTTON,
     "spinbutton",
     ATK_ROLE_SPIN_BUTTON,
     NSAccessibilityIncrementorRole,  //Subroles: Increment/Decrement.
     ROLE_SYSTEM_SPINBUTTON,
     ROLE_SYSTEM_SPINBUTTON,
     eNameFromValueRule)

ROLE(DIAGRAM,
     "diagram",
     ATK_ROLE_IMAGE,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_DIAGRAM,
     ROLE_SYSTEM_DIAGRAM,
     eNoNameRule)

ROLE(ANIMATION,
     "animation",
     ATK_ROLE_ANIMATION,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_ANIMATION,
     ROLE_SYSTEM_ANIMATION,
     eNoNameRule)

ROLE(EQUATION,
     "equation",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_EQUATION,
     ROLE_SYSTEM_EQUATION,
     eNoNameRule)

ROLE(BUTTONDROPDOWN,
     "buttondropdown",
     ATK_ROLE_PUSH_BUTTON,
     NSAccessibilityPopUpButtonRole,
     ROLE_SYSTEM_BUTTONDROPDOWN,
     ROLE_SYSTEM_BUTTONDROPDOWN,
     eNameFromSubtreeRule)

ROLE(BUTTONMENU,
     "buttonmenu",
     ATK_ROLE_PUSH_BUTTON,
     NSAccessibilityMenuButtonRole,
     ROLE_SYSTEM_BUTTONMENU,
     ROLE_SYSTEM_BUTTONMENU,
     eNameFromSubtreeRule)

ROLE(BUTTONDROPDOWNGRID,
     "buttondropdowngrid",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_BUTTONDROPDOWNGRID,
     ROLE_SYSTEM_BUTTONDROPDOWNGRID,
     eNameFromSubtreeRule)

ROLE(WHITESPACE,
     "whitespace",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_WHITESPACE,
     ROLE_SYSTEM_WHITESPACE,
     eNoNameRule)

ROLE(PAGETABLIST,
     "pagetablist",
     ATK_ROLE_PAGE_TAB_LIST,
     NSAccessibilityTabGroupRole,
     ROLE_SYSTEM_PAGETABLIST,
     ROLE_SYSTEM_PAGETABLIST,
     eNoNameRule)

ROLE(CLOCK,
     "clock",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,  //Unused on OS X
     ROLE_SYSTEM_CLOCK,
     ROLE_SYSTEM_CLOCK,
     eNoNameRule)

ROLE(SPLITBUTTON,
     "splitbutton",
     ATK_ROLE_PUSH_BUTTON,
     NSAccessibilityButtonRole,
     ROLE_SYSTEM_SPLITBUTTON,
     ROLE_SYSTEM_SPLITBUTTON,
     eNoNameRule)

ROLE(IPADDRESS,
     "ipaddress",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_IPADDRESS,
     ROLE_SYSTEM_IPADDRESS,
     eNoNameRule)

ROLE(ACCEL_LABEL,
     "accel label",
     ATK_ROLE_ACCEL_LABEL,
     NSAccessibilityStaticTextRole,
     ROLE_SYSTEM_STATICTEXT,
     ROLE_SYSTEM_STATICTEXT,
     eNoNameRule)

ROLE(ARROW,
     "arrow",
     ATK_ROLE_ARROW,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_INDICATOR,
     ROLE_SYSTEM_INDICATOR,
     eNoNameRule)

ROLE(CANVAS,
     "canvas",
     ATK_ROLE_CANVAS,
     NSAccessibilityImageRole,
     USE_ROLE_STRING,
     IA2_ROLE_CANVAS,
     eNoNameRule)

ROLE(CHECK_MENU_ITEM,
     "check menu item",
     ATK_ROLE_CHECK_MENU_ITEM,
     NSAccessibilityMenuItemRole,
     ROLE_SYSTEM_MENUITEM,
     IA2_ROLE_CHECK_MENU_ITEM,
     eNameFromSubtreeRule)

ROLE(COLOR_CHOOSER,
     "color chooser",
     ATK_ROLE_COLOR_CHOOSER,
     NSAccessibilityColorWellRole,
     ROLE_SYSTEM_DIALOG,
     IA2_ROLE_COLOR_CHOOSER,
     eNoNameRule)

ROLE(DATE_EDITOR,
     "date editor",
     ATK_ROLE_DATE_EDITOR,
     NSAccessibilityUnknownRole,
     USE_ROLE_STRING,
     IA2_ROLE_DATE_EDITOR,
     eNoNameRule)

ROLE(DESKTOP_ICON,
     "desktop icon",
     ATK_ROLE_DESKTOP_ICON,
     NSAccessibilityImageRole,
     USE_ROLE_STRING,
     IA2_ROLE_DESKTOP_ICON,
     eNoNameRule)

ROLE(DESKTOP_FRAME,
     "desktop frame",
     ATK_ROLE_DESKTOP_FRAME,
     NSAccessibilityUnknownRole,
     USE_ROLE_STRING,
     IA2_ROLE_DESKTOP_PANE,
     eNoNameRule)

ROLE(DIRECTORY_PANE,
     "directory pane",
     ATK_ROLE_DIRECTORY_PANE,
     NSAccessibilityBrowserRole,
     USE_ROLE_STRING,
     IA2_ROLE_DIRECTORY_PANE,
     eNoNameRule)

ROLE(FILE_CHOOSER,
     "file chooser",
     ATK_ROLE_FILE_CHOOSER,
     NSAccessibilityUnknownRole,  //Unused on OS X
     USE_ROLE_STRING,
     IA2_ROLE_FILE_CHOOSER,
     eNoNameRule)

ROLE(FONT_CHOOSER,
     "font chooser",
     ATK_ROLE_FONT_CHOOSER,
     NSAccessibilityUnknownRole,
     USE_ROLE_STRING,
     IA2_ROLE_FONT_CHOOSER,
     eNoNameRule)

ROLE(CHROME_WINDOW,
     "chrome window",
     ATK_ROLE_FRAME,
     NSAccessibilityGroupRole,  //Contains the main Firefox UI
     ROLE_SYSTEM_APPLICATION,
     IA2_ROLE_FRAME,
     eNoNameRule)

ROLE(GLASS_PANE,
     "glass pane",
     ATK_ROLE_GLASS_PANE,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_GLASS_PANE,
     eNoNameRule)

ROLE(HTML_CONTAINER,
     "html container",
     ATK_ROLE_HTML_CONTAINER,
     NSAccessibilityUnknownRole,
     USE_ROLE_STRING,
     IA2_ROLE_UNKNOWN,
     eNameFromSubtreeIfReqRule)

ROLE(ICON,
     "icon",
     ATK_ROLE_ICON,
     NSAccessibilityImageRole,
     ROLE_SYSTEM_PUSHBUTTON,
     IA2_ROLE_ICON,
     eNoNameRule)

ROLE(LABEL,
     "label",
     ATK_ROLE_LABEL,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_STATICTEXT,
     IA2_ROLE_LABEL,
     eNameFromSubtreeRule)

ROLE(LAYERED_PANE,
     "layered pane",
     ATK_ROLE_LAYERED_PANE,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_LAYERED_PANE,
     eNoNameRule)

ROLE(OPTION_PANE,
     "option pane",
     ATK_ROLE_OPTION_PANE,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_OPTION_PANE,
     eNoNameRule)

ROLE(PASSWORD_TEXT,
     "password text",
     ATK_ROLE_PASSWORD_TEXT,
     NSAccessibilityTextFieldRole,
     ROLE_SYSTEM_TEXT,
     ROLE_SYSTEM_TEXT,
     eNoNameRule)

ROLE(POPUP_MENU,
     "popup menu",
     ATK_ROLE_POPUP_MENU,
     NSAccessibilityUnknownRole,  //Unused
     ROLE_SYSTEM_MENUPOPUP,
     ROLE_SYSTEM_MENUPOPUP,
     eNoNameRule)

ROLE(RADIO_MENU_ITEM,
     "radio menu item",
     ATK_ROLE_RADIO_MENU_ITEM,
     NSAccessibilityMenuItemRole,
     ROLE_SYSTEM_MENUITEM,
     IA2_ROLE_RADIO_MENU_ITEM,
     eNameFromSubtreeRule)

ROLE(ROOT_PANE,
     "root pane",
     ATK_ROLE_ROOT_PANE,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_ROOT_PANE,
     eNoNameRule)

ROLE(SCROLL_PANE,
     "scroll pane",
     ATK_ROLE_SCROLL_PANE,
     NSAccessibilityScrollAreaRole,
     USE_ROLE_STRING,
     IA2_ROLE_SCROLL_PANE,
     eNoNameRule)

ROLE(SPLIT_PANE,
     "split pane",
     ATK_ROLE_SPLIT_PANE,
     NSAccessibilitySplitGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_SPLIT_PANE,
     eNoNameRule)

ROLE(TABLE_COLUMN_HEADER,
     "table column header",
     ATK_ROLE_TABLE_COLUMN_HEADER,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_COLUMNHEADER,
     ROLE_SYSTEM_COLUMNHEADER,
     eNameFromSubtreeRule)

ROLE(TABLE_ROW_HEADER,
     "table row header",
     ATK_ROLE_TABLE_ROW_HEADER,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_ROWHEADER,
     ROLE_SYSTEM_ROWHEADER,
     eNameFromSubtreeRule)

ROLE(TEAR_OFF_MENU_ITEM,
     "tear off menu item",
     ATK_ROLE_TEAR_OFF_MENU_ITEM,
     NSAccessibilityMenuItemRole,
     ROLE_SYSTEM_MENUITEM,
     IA2_ROLE_TEAR_OFF_MENU,
     eNameFromSubtreeRule)

ROLE(TERMINAL,
     "terminal",
     ATK_ROLE_TERMINAL,
     NSAccessibilityUnknownRole,
     USE_ROLE_STRING,
     IA2_ROLE_TERMINAL,
     eNoNameRule)

ROLE(TEXT_CONTAINER,
     "text container",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_TEXT_FRAME,
     eNameFromSubtreeIfReqRule)

ROLE(TOGGLE_BUTTON,
     "toggle button",
     ATK_ROLE_TOGGLE_BUTTON,
     NSAccessibilityButtonRole,
     ROLE_SYSTEM_PUSHBUTTON,
     IA2_ROLE_TOGGLE_BUTTON,
     eNameFromSubtreeRule)

ROLE(TREE_TABLE,
     "tree table",
     ATK_ROLE_TREE_TABLE,
     NSAccessibilityTableRole,
     ROLE_SYSTEM_OUTLINE,
     ROLE_SYSTEM_OUTLINE,
     eNoNameRule)

ROLE(VIEWPORT,
     "viewport",
     ATK_ROLE_VIEWPORT,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_PANE,
     IA2_ROLE_VIEW_PORT,
     eNoNameRule)

ROLE(HEADER,
     "header",
     ATK_ROLE_HEADER,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_HEADER,
     eNoNameRule)

ROLE(FOOTER,
     "footer",
     ATK_ROLE_FOOTER,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_FOOTER,
     eNoNameRule)

ROLE(PARAGRAPH,
     "paragraph",
     ATK_ROLE_PARAGRAPH,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_PARAGRAPH,
     eNameFromSubtreeIfReqRule)

ROLE(RULER,
     "ruler",
     ATK_ROLE_RULER,
     @"AXRuler",  //10.4+ only, so we re-define the constant.
     USE_ROLE_STRING,
     IA2_ROLE_RULER,
     eNoNameRule)

ROLE(AUTOCOMPLETE,
     "autocomplete",
     ATK_ROLE_AUTOCOMPLETE,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_COMBOBOX,
     ROLE_SYSTEM_COMBOBOX,
     eNoNameRule)

ROLE(EDITBAR,
     "editbar",
     ATK_ROLE_EDITBAR,
     NSAccessibilityTextFieldRole,
     ROLE_SYSTEM_TEXT,
     IA2_ROLE_EDITBAR,
     eNoNameRule)

ROLE(ENTRY,
     "entry",
     ATK_ROLE_ENTRY,
     NSAccessibilityTextFieldRole,
     ROLE_SYSTEM_TEXT,
     ROLE_SYSTEM_TEXT,
     eNameFromValueRule)

ROLE(CAPTION,
     "caption",
     ATK_ROLE_CAPTION,
     NSAccessibilityStaticTextRole,
     USE_ROLE_STRING,
     IA2_ROLE_CAPTION,
     eNameFromSubtreeIfReqRule)

ROLE(DOCUMENT_FRAME,
     "document frame",
     ATK_ROLE_DOCUMENT_FRAME,
     NSAccessibilityScrollAreaRole,
     USE_ROLE_STRING,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(HEADING,
     "heading",
     ATK_ROLE_HEADING,
     @"AXHeading",
     USE_ROLE_STRING,
     IA2_ROLE_HEADING,
     eNameFromSubtreeIfReqRule)

ROLE(PAGE,
     "page",
     ATK_ROLE_PAGE,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_PAGE,
     eNoNameRule)

ROLE(SECTION,
     "section",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_SECTION,
     eNameFromSubtreeIfReqRule)

ROLE(REDUNDANT_OBJECT,
     "redundant object",
     ATK_ROLE_REDUNDANT_OBJECT,
     NSAccessibilityUnknownRole,
     USE_ROLE_STRING,
     IA2_ROLE_REDUNDANT_OBJECT,
     eNoNameRule)

ROLE(FORM,
     "form",
     ATK_ROLE_FORM,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_FORM,
     eNoNameRule)

ROLE(IME,
     "ime",
     ATK_ROLE_INPUT_METHOD_WINDOW,
     NSAccessibilityUnknownRole,
     USE_ROLE_STRING,
     IA2_ROLE_INPUT_METHOD_WINDOW,
     eNoNameRule)

ROLE(APP_ROOT,
     "app root",
     ATK_ROLE_APPLICATION,
     NSAccessibilityUnknownRole,  //Unused on OS X
     ROLE_SYSTEM_APPLICATION,
     ROLE_SYSTEM_APPLICATION,
     eNoNameRule)

ROLE(PARENT_MENUITEM,
     "parent menuitem",
     ATK_ROLE_MENU,
     NSAccessibilityMenuItemRole,
     ROLE_SYSTEM_MENUITEM,
     ROLE_SYSTEM_MENUITEM,
     eNameFromSubtreeRule)

ROLE(CALENDAR,
     "calendar",
     ATK_ROLE_CALENDAR,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_CLIENT,
     ROLE_SYSTEM_CLIENT,
     eNoNameRule)

ROLE(COMBOBOX_LIST,
     "combobox list",
     ATK_ROLE_MENU,
     NSAccessibilityMenuRole,
     ROLE_SYSTEM_LIST,
     ROLE_SYSTEM_LIST,
     eNoNameRule)

ROLE(COMBOBOX_OPTION,
     "combobox option",
     ATK_ROLE_MENU_ITEM,
     NSAccessibilityMenuItemRole,
     ROLE_SYSTEM_LISTITEM,
     ROLE_SYSTEM_LISTITEM,
     eNameFromSubtreeRule)

ROLE(IMAGE_MAP,
     "image map",
     ATK_ROLE_IMAGE,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_GRAPHIC,
     ROLE_SYSTEM_GRAPHIC,
     eNoNameRule)

ROLE(OPTION,
     "listbox option",
     ATK_ROLE_LIST_ITEM,
     NSAccessibilityStaticTextRole,
     ROLE_SYSTEM_LISTITEM,
     ROLE_SYSTEM_LISTITEM,
     eNameFromSubtreeRule)

ROLE(RICH_OPTION,
     "listbox rich option",
     ATK_ROLE_LIST_ITEM,
     NSAccessibilityRowRole,
     ROLE_SYSTEM_LISTITEM,
     ROLE_SYSTEM_LISTITEM,
     eNameFromSubtreeRule)

ROLE(LISTBOX,
     "listbox",
     ATK_ROLE_LIST_BOX,
     NSAccessibilityListRole,
     ROLE_SYSTEM_LIST,
     ROLE_SYSTEM_LIST,
     eNoNameRule)

ROLE(FLAT_EQUATION,
     "flat equation",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityUnknownRole,
     ROLE_SYSTEM_EQUATION,
     ROLE_SYSTEM_EQUATION,
     eNoNameRule)

ROLE(GRID_CELL,
     "gridcell",
     ATK_ROLE_TABLE_CELL,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_CELL,
     ROLE_SYSTEM_CELL,
     eNameFromSubtreeRule)

ROLE(EMBEDDED_OBJECT,
     "embedded object",
     ATK_ROLE_PANEL,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_EMBEDDED_OBJECT,
     eNoNameRule)

ROLE(NOTE,
     "note",
     ATK_ROLE_COMMENT,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_NOTE,
     eNameFromSubtreeIfReqRule)

ROLE(FIGURE,
     "figure",
     ATK_ROLE_PANEL,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_GROUPING,
     ROLE_SYSTEM_GROUPING,
     eNoNameRule)

ROLE(CHECK_RICH_OPTION,
     "check rich option",
     ATK_ROLE_CHECK_BOX,
     NSAccessibilityCheckBoxRole,
     ROLE_SYSTEM_CHECKBUTTON,
     ROLE_SYSTEM_CHECKBUTTON,
     eNameFromSubtreeRule)

ROLE(DEFINITION_LIST,
     "definitionlist",
     ATK_ROLE_LIST,
     NSAccessibilityListRole,
     ROLE_SYSTEM_LIST,
     ROLE_SYSTEM_LIST,
     eNameFromSubtreeIfReqRule)

ROLE(TERM,
     "term",
     ATK_ROLE_DESCRIPTION_TERM,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_LISTITEM,
     ROLE_SYSTEM_LISTITEM,
     eNameFromSubtreeRule)

ROLE(DEFINITION,
     "definition",
     ATK_ROLE_PARAGRAPH,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_PARAGRAPH,
     eNameFromSubtreeRule)

ROLE(KEY,
     "key",
     ATK_ROLE_PUSH_BUTTON,
     NSAccessibilityButtonRole,
     ROLE_SYSTEM_PUSHBUTTON,
     ROLE_SYSTEM_PUSHBUTTON,
     eNameFromSubtreeRule)

ROLE(SWITCH,
     "switch",
     ATK_ROLE_TOGGLE_BUTTON,
     NSAccessibilityCheckBoxRole,
     ROLE_SYSTEM_CHECKBUTTON,
     IA2_ROLE_TOGGLE_BUTTON,
     eNameFromSubtreeRule)

ROLE(MATHML_MATH,
     "math",
     ATK_ROLE_MATH,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_EQUATION,
     ROLE_SYSTEM_EQUATION,
     eNoNameRule)

ROLE(MATHML_IDENTIFIER,
     "mathml identifier",
     ATK_ROLE_STATIC,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNameFromSubtreeRule)

ROLE(MATHML_NUMBER,
     "mathml number",
     ATK_ROLE_STATIC,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNameFromSubtreeRule)

ROLE(MATHML_OPERATOR,
     "mathml operator",
     ATK_ROLE_STATIC,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNameFromSubtreeRule)

ROLE(MATHML_TEXT,
     "mathml text",
     ATK_ROLE_STATIC,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNameFromSubtreeRule)

ROLE(MATHML_STRING_LITERAL,
     "mathml string literal",
     ATK_ROLE_STATIC,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNameFromSubtreeRule)

ROLE(MATHML_GLYPH,
     "mathml glyph",
     ATK_ROLE_IMAGE,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNameFromSubtreeRule)

ROLE(MATHML_ROW,
     "mathml row",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_FRACTION,
     "mathml fraction",
     ATK_ROLE_MATH_FRACTION,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_SQUARE_ROOT,
     "mathml square root",
     ATK_ROLE_MATH_ROOT,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_ROOT,
     "mathml root",
     ATK_ROLE_MATH_ROOT,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_FENCED,
     "mathml fenced",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_ENCLOSED,
     "mathml enclosed",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_STYLE,
     "mathml style",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_SUB,
     "mathml sub",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_SUP,
     "mathml sup",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_SUB_SUP,
     "mathml sub sup",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_UNDER,
     "mathml under",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_OVER,
     "mathml over",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_UNDER_OVER,
     "mathml under over",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_MULTISCRIPTS,
     "mathml multiscripts",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_TABLE,
     "mathml table",
     ATK_ROLE_TABLE,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_LABELED_ROW,
     "mathml labeled row",
     ATK_ROLE_TABLE_ROW,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_TABLE_ROW,
     "mathml table row",
     ATK_ROLE_TABLE_ROW,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_CELL,
     "mathml cell",
     ATK_ROLE_TABLE_CELL,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_ACTION,
     "mathml action",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_ERROR,
     "mathml error",
     ATK_ROLE_SECTION,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_STACK,
     "mathml stack",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_LONG_DIVISION,
     "mathml long division",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_STACK_GROUP,
     "mathml stack group",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_STACK_ROW,
     "mathml stack row",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_STACK_CARRIES,
     "mathml stack carries",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_STACK_CARRY,
     "mathml stack carry",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(MATHML_STACK_LINE,
     "mathml stack line",
     ATK_ROLE_UNKNOWN,
     NSAccessibilityGroupRole,
     0,
     IA2_ROLE_UNKNOWN,
     eNoNameRule)

ROLE(RADIO_GROUP,
     "grouping",
     ATK_ROLE_PANEL,
     NSAccessibilityRadioGroupRole,
     ROLE_SYSTEM_GROUPING,
     ROLE_SYSTEM_GROUPING,
     eNoNameRule)

ROLE(TEXT,
     "text",
     ATK_ROLE_STATIC,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_TEXT_FRAME,
     eNameFromSubtreeIfReqRule)

ROLE(DETAILS,
     "details",
     ATK_ROLE_PANEL,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_GROUPING,
     ROLE_SYSTEM_GROUPING,
     eNoNameRule)

ROLE(SUMMARY,
     "summary",
     ATK_ROLE_PUSH_BUTTON,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_PUSHBUTTON,
     ROLE_SYSTEM_PUSHBUTTON,
     eNameFromSubtreeRule)

ROLE(LANDMARK,
     "landmark",
     ATK_ROLE_LANDMARK,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_LANDMARK,
     eNoNameRule)

ROLE(NAVIGATION,
     "navigation",
     ATK_ROLE_LANDMARK,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_LANDMARK,
     eNoNameRule)

ROLE(FOOTNOTE,
     "footnote",
     ATK_ROLE_FOOTNOTE,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_FOOTNOTE,
     eNoNameRule)

ROLE(ARTICLE,
     "article",
     ATK_ROLE_ARTICLE,
     NSAccessibilityGroupRole,
     ROLE_SYSTEM_DOCUMENT,
     ROLE_SYSTEM_DOCUMENT,
     eNoNameRule)

ROLE(REGION,
     "region",
     ATK_ROLE_LANDMARK,
     NSAccessibilityGroupRole,
     USE_ROLE_STRING,
     IA2_ROLE_LANDMARK,
     eNoNameRule)

// A composite widget with a text input and popup. Used for ARIA role combobox.
// See also COMBOBOX.
ROLE(EDITCOMBOBOX,
     "editcombobox",
     ATK_ROLE_COMBO_BOX,
     NSAccessibilityComboBoxRole,
     ROLE_SYSTEM_COMBOBOX,
     ROLE_SYSTEM_COMBOBOX,
     eNameFromValueRule)
