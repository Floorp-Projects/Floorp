/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   xphist.h --- an API for XP history navigation.
   Created: Chris Toshok <toshok@netscape.com>, 6-Sep-1998.
*/

#include "structs.h"
#include "shist.h"
#include "shistele.h"

void
XPHIST_GoBack(MWContext *top_context)
{
  if (top_context->grid_children && !XP_ListIsEmpty(top_context->grid_children))
    {
      if (LO_BackInGrid(top_context))
	return;
    }
  else
    {
      URL_Struct *url;
      History_entry *prev_entry = SHIST_GetPrevious(top_context);
#ifdef ELEMENT_LIST_IN_SHIST
      History_entry *cur_entry = SHIST_GetCurrent(&top_context->hist);

      cur_entry->savedData.ElementList = /* XXX */ 0;
      if (prev_entry->savedData.ElementList)
	{
	  /* XXX */
	  return;
	}
#endif

      url = SHIST_CreateURLStructFromHistoryEntry (top_context,
						   prev_entry);
      if (url)
	{
	  FE_GetURL (top_context, url);
	}
      else
	{
	  FE_Alert (top_context, "foo" /*fe_globalData.no_previous_url_message*/);
	}
    }
}

void
XPHIST_GoForward(MWContext *top_context)
{
#ifdef ELEMENT_LIST_IN_SHIST
#endif
  if (top_context->grid_children && !XP_ListIsEmpty(top_context->grid_children))
    {
      if (LO_ForwardInGrid(top_context))
	return;
    }
  else
    {
      URL_Struct *url;
      History_entry *next_entry = SHIST_GetNext(top_context);
#ifdef ELEMENT_LIST_IN_SHIST
      History_entry *cur_entry = SHIST_GetCurrent(&top_context->hist);

      cur_entry->savedData.ElementList = /* XXX */ 0;
      if (next_entry->savedData.ElementList)
	{
	  /* XXX */
	  return;
	}
#endif
      
      url = SHIST_CreateURLStructFromHistoryEntry (top_context,
						   next_entry);
      if (url)
	{
	  FE_GetURL (top_context, url);
	}
      else
	{
	  FE_Alert (top_context, "foo" /*fe_globalData.no_previous_url_message*/);
	}
    }
}
