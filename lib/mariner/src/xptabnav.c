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
   xptabnav.c --- an API for XP HTML area tabbing.
   Created: Chris Toshok <toshok@netscape.com>, 6-Nov-1997.
 */


#include "xptabnav.h"
#include "xpassert.h"
#include "structs.h"
#include "proto.h"
#include "client.h"
#include "fe_proto.h"
#include "xp_rect.h"
#include "layers.h"

#define FOCUS_DATA(context) (context->tab_focus_data)
#define FOCUS_ELEMENT(context) (FOCUS_DATA(context)->pElement)
#define FOCUS_INDEX(context) (FOCUS_DATA(context)->mapAreaIndex)
#define FOCUS_ANCHOR(context) (FOCUS_DATA(context)->pAnchor)

void
XPTAB_SetFocusElement(MWContext *context,
					  LO_Element *element)
{
  LO_TabFocusData data;

  XP_ASSERT(FOCUS_DATA(context));
  if (!FOCUS_DATA(context)) return;

  data.pElement = element;
  data.mapAreaIndex = 0;
  data.pAnchor = 0;
  
  if (LO_isTabableElement(context, &data))
    *FOCUS_DATA(context) = data;
}

LO_Element*
XPTAB_GetFocusElement(MWContext *context)
{
  XP_ASSERT(FOCUS_DATA(context));
  if (!FOCUS_DATA(context)) return NULL;

  return FOCUS_ELEMENT(context);
}

int32
XPTAB_GetFocusMapAreaIndex(MWContext *context)
{
  XP_ASSERT(FOCUS_DATA(context));
  if (!FOCUS_DATA(context)) return -1;

  return FOCUS_INDEX(context);
}

void
XPTAB_GetFocusMapArea(MWContext *context,
		      lo_MapAreaRec **map_area)
{
  *map_area = NULL;

  XP_ASSERT(FOCUS_DATA(context));
  if (!FOCUS_DATA(context)) return;

  if (FOCUS_ELEMENT(context)->type == LO_IMAGE
      && FOCUS_INDEX(context))
    *map_area = LO_getTabableMapArea(context,
				     (LO_ImageStruct*)FOCUS_ELEMENT(context),
				     FOCUS_INDEX(context));
}

void
XPTAB_InitTabData(MWContext *context)
{
  printf ("In XPTAB_InitTabData\n");
  FOCUS_DATA(context) = XP_NEW_ZAP(LO_TabFocusData);
}

void
XPTAB_DestroyTabData(MWContext *context)
{
  XP_ASSERT(FOCUS_DATA(context));
  if (!FOCUS_DATA(context)) return;

  XP_FREE(FOCUS_DATA(context));
}

void
XPTAB_ClearTabFocus(MWContext *context)
{
  XP_ASSERT(FOCUS_DATA(context));
  if (!FOCUS_DATA(context)) return;

  XP_MEMSET(FOCUS_DATA(context), 0, sizeof(LO_TabFocusData));
}

static Bool
xptab_MoveFocus(MWContext *context, int forward)
{
  Bool tabbed;
  LO_TabFocusData old_focus_data;

  XP_ASSERT(FOCUS_DATA(context));
  if (!FOCUS_DATA(context)) return FALSE;

  old_focus_data = *FOCUS_DATA(context);

  tabbed = LO_getNextTabableElement(context, FOCUS_DATA(context), forward);
  
  /* if is the same element, just return FALSE. */
  if (old_focus_data.pElement == FOCUS_ELEMENT(context)
      && old_focus_data.mapAreaIndex == FOCUS_INDEX(context))
    return FALSE;
  
  if (tabbed) /* if we actually tabbed someplace. */
    {
      if (old_focus_data.pElement)
	if (old_focus_data.pElement->type == LO_FORM_ELE)
	  {
	    FE_BlurInputElement(context, old_focus_data.pElement);
	  }
	else
	  {
	    /* erase the old feedback */
	    XP_Rect rect;

	    rect.left = old_focus_data.pElement->lo_any.x;
	    rect.top = old_focus_data.pElement->lo_any.y;
	    rect.right = (old_focus_data.pElement->lo_any.x +
			  old_focus_data.pElement->lo_any.width);
	    rect.bottom = (old_focus_data.pElement->lo_any.y +
			   old_focus_data.pElement->lo_any.height);

	    CL_UpdateDocumentRect(context->compositor, &rect, PR_FALSE);
	  }

      {
	/* scroll to make the element visible. */
      }
      
      if (FOCUS_ELEMENT(context)->type == LO_FORM_ELE)
	{
	  FE_FocusInputElement(context, FOCUS_ELEMENT(context));
	}
      else
	{
	  FE_FocusInputElement(context, NULL);
	  
	  /* display a feedback rectangle if it's not a form element */
	  FE_DisplayFeedback(context, FE_VIEW, FOCUS_ELEMENT(context));
	}
      
      if (FOCUS_ANCHOR(context))
	FE_Progress(context, (char*)FOCUS_ANCHOR(context)->anchor);
    }
  
  return tabbed;
}

Bool
XPTAB_TabToNextElement(MWContext *context)
{
  return xptab_MoveFocus(context, TRUE);
}

Bool
XPTAB_TabToPrevElement(MWContext *context)
{
  return xptab_MoveFocus(context, FALSE);
}

Bool
XPTAB_TabToNextFormElement(MWContext *context)
{
}

Bool
XPTAB_TabToPrevFormElement(MWContext *context)
{
}

