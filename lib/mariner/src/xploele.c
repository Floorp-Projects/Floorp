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
   xploele.c --- an API for accessing layout element information.
   Created: Chris Toshok <toshok@netscape.com>, 6-Nov-1997.
 */


#include "xploele.h"
#include "xpassert.h"

void*
XPLO_GetElementFEData(LO_Element *element)
{
  XP_ASSERT(element->type == LO_TEXT
			|| element->type == LO_SUBDOC
			|| element->type == LO_LINEFEED
			|| element->type == LO_HRULE
			|| element->type == LO_BULLET
			|| element->type == LO_TABLE
			|| element->type == LO_CELL
			|| element->type == LO_EMBED
			|| element->type == LO_JAVA
			|| element->type == LO_EDGE);

  switch (element->type)
	{
	case LO_TEXT:
	  return ((LO_TextStruct*)element)->FE_Data;
	case LO_SUBDOC:
	  return ((LO_SubDocStruct*)element)->FE_Data;
	case LO_LINEFEED:
	  return ((LO_LinefeedStruct*)element)->FE_Data;
	case LO_HRULE:
	  return ((LO_HorizRuleStruct*)element)->FE_Data;
	case LO_BULLET:
	  return ((LO_BulletStruct*)element)->FE_Data;
	case LO_TABLE:
	  return ((LO_TableStruct*)element)->FE_Data;
	case LO_CELL:
	  return ((LO_CellStruct*)element)->FE_Data;
	case LO_EMBED:
	  return ((LO_EmbedStruct*)element)->FE_Data;
	case LO_JAVA:
	  return ((LO_EmbedStruct*)element)->FE_Data;
	case LO_EDGE:
	  return ((LO_EdgeStruct*)element)->FE_Data;
	default:
	  return NULL;
	}
}

void
XPLO_SetElementFEData(LO_Element *element, void* fe_data)
{
  XP_ASSERT(element->type == LO_TEXT
			|| element->type == LO_SUBDOC
			|| element->type == LO_LINEFEED
			|| element->type == LO_HRULE
			|| element->type == LO_BULLET
			|| element->type == LO_TABLE
			|| element->type == LO_CELL
			|| element->type == LO_EMBED
			|| element->type == LO_JAVA
			|| element->type == LO_EDGE);

  switch (element->type)
	{
	case LO_TEXT:
	  ((LO_TextStruct*)element)->FE_Data = fe_data;
	  break;
	case LO_SUBDOC:
	  ((LO_SubDocStruct*)element)->FE_Data = fe_data;
	  break;
	case LO_LINEFEED:
	  ((LO_LinefeedStruct*)element)->FE_Data = fe_data;
	  break;
	case LO_HRULE:
	  ((LO_HorizRuleStruct*)element)->FE_Data = fe_data;
	  break;
	case LO_BULLET:
	  ((LO_BulletStruct*)element)->FE_Data = fe_data;
	  break;
	case LO_TABLE:
	  ((LO_TableStruct*)element)->FE_Data = fe_data;
	  break;
	case LO_CELL:
	  ((LO_CellStruct*)element)->FE_Data = fe_data;
	  break;
	case LO_EMBED:
	  ((LO_EmbedStruct*)element)->FE_Data = fe_data;
	  break;
	case LO_JAVA:
	  ((LO_EmbedStruct*)element)->FE_Data = fe_data;
	  break;
	case LO_EDGE:
	  ((LO_EdgeStruct*)element)->FE_Data = fe_data;
	  break;
	default:
	  break;
	}
}

int16
XPLO_GetElementType(LO_Element *element)
{
  return element->type;
}

int32 
XPLO_GetElementID(LO_Element *element)
{
  return ((LO_Any*)element)->ele_id;
}

int16 
XPLO_GetElementXOffset(LO_Element *element)
{
  return ((LO_Any*)element)->x_offset;
}

void 
XPLO_GetElementRect(LO_Element *element,
					int32 *x,
					int32 *y,	
					int32 *width,
					int32 *height)
{
  if (x)
	*x = ((LO_Any*)element)->x;
  if (y)
	*y = ((LO_Any*)element)->y;
  if (width)
	*width = ((LO_Any*)element)->width;
  if (height)
	*height = ((LO_Any*)element)->height;
}

int32 
XPLO_GetElementLineHeight(LO_Element *element)
{
  return ((LO_Any*)element)->line_height;
}

LO_Element* 
XPLO_GetPrevElement(LO_Element *element)
{
  return ((LO_Any*)element)->prev;
}

LO_Element* 
XPLO_GetNextElement(LO_Element *element)
{
  return ((LO_Any*)element)->next;
}

ED_Element* 
XPLO_GetEditElement(LO_Element *element)
{
  return ((LO_Any*)element)->edit_element;
}

int32 
XPLO_GetEditOffset(LO_Element *element)
{
  return ((LO_Any*)element)->edit_offset;
}

LO_AnchorData*	
XPLO_GetAnchorData(LO_Element *element)
{
  XP_ASSERT(element->type == LO_TEXT
			|| element->type == LO_IMAGE
			|| element->type == LO_SUBDOC
			|| element->type == LO_LINEFEED
			|| element->type == LO_TABLE);

  switch (element->type)
	{
	case LO_TEXT:
	  return ((LO_TextStruct*)element)->anchor_href;
	case LO_IMAGE:
	  return ((LO_ImageStruct*)element)->anchor_href;
	case LO_SUBDOC:
	  return ((LO_SubDocStruct*)element)->anchor_href;
	case LO_LINEFEED:
	  return ((LO_LinefeedStruct*)element)->anchor_href;
	case LO_TABLE:
	  return ((LO_TableStruct*)element)->anchor_href;
	default:
	  return NULL;
	}
}
