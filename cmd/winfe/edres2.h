/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

// Resource definition defines for Editor - NOT EDITABLE BY APP EDITOR

#ifndef EDRES2_H
#define EDRES2_H


// For configuring CComboToolBars
// Maximum number of items in a toolbar!
#define MAX_TOOLBAR_CONTROLS            500 

// *** Keep an eye on values in RESOURCE.H to avoid collisions!
// Change this if IDs in Resource.h are getting close
#define ID_EDT_BASE                     36000

 // Command IDs for formating toolbar and submenus created at runtime
#define ID_FORMAT_PARAGRAPH_BASE        ID_EDT_BASE
#define ID_FORMAT_PARAGRAPH_END         ID_FORMAT_PARAGRAPH_BASE + P_MAX

// For convenience, IDs of Paragraph styles used in toolbar:
#define ID_FORMAT_NUM_LIST              ID_FORMAT_PARAGRAPH_BASE+P_NUM_LIST
#define ID_FORMAT_UNUM_LIST             ID_FORMAT_PARAGRAPH_BASE+P_UNUM_LIST

// Text put into formating comboboxes and submenus
#define ID_LIST_TEXT_PARAGRAPH_BASE     (ID_FORMAT_PARAGRAPH_END + 1)
#define ID_LIST_TEXT_PARAGRAPH_END		(ID_LIST_TEXT_PARAGRAPH_BASE + P_MAX)

#define ID_LIST_TEXT_CHARACTER_BASE     ID_LIST_TEXT_PARAGRAPH_END+1
#define ID_LIST_TEXT_CHARACTER_END		(ID_LIST_TEXT_CHARACTER_BASE + P_MAX)

#define ID_FORMAT_MENU_BASE             ID_LIST_TEXT_CHARACTER_END+1
#define ID_FORMAT_MENU_END              ID_FORMAT_MENU_BASE

#define  ID_FORMAT_FONTSIZE_BASE        (ID_FORMAT_MENU_END+1)
#define  ID_FORMAT_POINTSIZE_BASE       (ID_FORMAT_MENU_END+10)

// AVOID INCLUDING ANOTHER H FILE WITH MAX_FONT_SIZE AND MAX_FONT_COLOR,
//   BE SURE TO ALLOW ENOUGH ROOM HERE FOR SIZE VALUES...

// Submenu created at runtime to pick color from palette
#define ID_FORMAT_FONTCOLOR_BASE        (ID_FORMAT_POINTSIZE_BASE+20)
//... AND HERE FOR COLOR VALUES

// Tools menu 
#define ID_TOOLS_MENU_BASE              (ID_FORMAT_FONTSIZE_BASE+100)
#define ID_CHECK_SPELLING				ID_TOOLS_MENU_BASE
#define ID_SPELLING_LANGUAGE            (ID_TOOLS_MENU_BASE+2)

// Add new Tools menu ids here. Change ID_EDITOR_PLUGINS_BASE if there are too many Tools menu ids.

// Menu id ranges for Plugins
#define ID_EDITOR_PLUGINS_BASE          (ID_TOOLS_MENU_BASE+10)
#define MAX_EDITOR_PLUGINS              50
#define ID_STOP_EDITOR_PLUGIN           (ID_TOOLS_MENU_BASE+1)

#define ID_FORMAT_FONTFACE_BASE         (ID_TOOLS_MENU_BASE+MAX_EDITOR_PLUGINS+10)
// RESERVE MAX_TRUETYPE_FONTS(currently 100)+3 IDs HERE FOR FONTFACE MENU

#define ID_EDIT_HISTORY_BASE            (ID_TOOLS_MENU_BASE+MAX_EDITOR_PLUGINS+10+105)
// RESERVE MAX_EDIT_HISTORY_LOCATIONS (Currently 10) items here
#define ID_EDIT_LAST_ID                 (ID_EDIT_HISTORY_BASE + 10)
#endif
