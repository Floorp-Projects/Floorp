/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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

