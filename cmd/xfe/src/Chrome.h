/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* 
   Chrome.h -- types and such needed by the various pieces of chrome
   Created: Chris Toshok <toshok@netscape.com>, 17-Sep-96.
 */



#ifndef _xfe_chrome_h
#define _xfe_chrome_h

/* This tag type is used by both the toolbars and the menu
   specs to determine what widgets to create */
typedef enum {
   LABEL, // used for titles in popup menus.
   PUSHBUTTON,
   CUSTOMBUTTON, // A button that can be specified by Admin Kit
   TOGGLEBUTTON,
   CASCADEBUTTON,
   FANCY_CASCADEBUTTON,
   COMBOBOX,
   SEPARATOR,
   DYNA_CASCADEBUTTON,	// A cascade button that creates its submenu through a function call
   DYNA_FANCY_CASCADEBUTTON,	// DYNA_CASCADEBUTTON w/ fancy bookmark accent support.
   DYNA_MENUITEMS,	// A portion of a submenu that creates its contents through a function call
   RADIOBUTTON
} EChromeTag;

#endif /* _xfe_chrome_h */
