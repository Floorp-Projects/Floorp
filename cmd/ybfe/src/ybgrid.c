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
   ybgrid.c --- yb fe handling of Grid windows
                  (FRAMESET's and FRAME).
*/

#include "structs.h"
#include "ntypes.h"
#include "xpassert.h"
#include "proto.h"
#include "fe_proto.h"

void
FE_LoadGridCellFromHistory(MWContext *context,
			   void *hist,
			   NET_ReloadMethod force_reload)
{
}

void*
FE_FreeGridWindow(MWContext *context,
		  XP_Bool save_history)
{
}

void
FE_RestructureGridWindow(MWContext *context,
			 int32 x,
			 int32 y,
			 int32 width,
			 int32 height)
{
}

MWContext *
FE_MakeGridWindow(MWContext *old_context,
		  void *hist_list,
		  void *history,
		  int32 x,
		  int32 y,
		  int32 width,
		  int32 height,
		  char *url_str,
		  char *window_name,
		  int8 scrolling,
		  NET_ReloadMethod force_reload,
		  Bool no_edge)
{
}

void
FE_GetFullWindowSize(MWContext *context,
		     int32 *width,
		     int32 *height)
{
}

void
FE_GetEdgeMinSize(MWContext *context,
		  int32 *size)
{
}
