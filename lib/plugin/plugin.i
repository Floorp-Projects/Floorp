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

#ifndef PLUGIN_I
#define PLUGIN_I

#include "xpresdef.h"


BEGIN_STR (PLUGIN_strings)

ResDef(XP_PLUGIN_LOADING_PLUGIN,                XP_MSG_BASE+308,
 "Loading plug-in")

ResDef(XP_PLUGIN_NOT_FOUND,                      XP_MSG_BASE+325,
 "A plugin for the mime type %s\nwas not found.")

ResDef(XP_PLUGIN_CANT_LOAD_PLUGIN,               XP_MSG_BASE+326,
  "Could not load the plug-in '%s' for the MIME type '%s'.  \n\n\
  Make sure enough memory is available and that the plug-in is installed correctly.")

END_STR (PLUGIN_strings)

#endif
