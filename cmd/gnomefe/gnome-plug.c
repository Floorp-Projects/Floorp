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
   gnomeplug.c --- gnome functions for fe
                   specific plugin stuff.
*/

#include "xp_core.h"
#include "structs.h"
#include "nppriv.h"

void
FE_RegisterPlugins()
{
  printf("FE_RegisterPlugins (empty)\n");
}

NPPluginFuncs*
FE_LoadPlugin(void *plugin,
	      NPNetscapeFuncs *funcs,
              struct _np_handle* handle)
{
  printf("FE_LoadPlugin (empty)\n");
}

void
FE_UnloadPlugin(void *plugin,
                struct _np_handle *handle)
{
  printf("FE_UnloadPlugin (empty)\n");
}

void
FE_EmbedURLExit(URL_Struct *urls,
		int status,
		MWContext *cx)
{
  printf("FE_EmbedURLExit (empty)\n");
}

void
FE_ShowScrollBars(MWContext *context,
		  XP_Bool show)
{
  printf("FE_ShowScrollBars (empty)\n");
}

NPError
FE_PluginGetValue(void *pdesc,
		  NPNVariable variable,
		  void *r_value)
{
  printf("FE_PluginGetValue (empty)\n");
}

