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

#ifdef DOM

#include "xp.h"
#include "pa_parse.h"
#include "layout.h"
#include "laylayer.h"
#include "laystyle.h"
#include "libmocha.h"
#include "stystruc.h"
#include "stystack.h"
#include "layers.h"


/* Initial attempt at DOM by letting JS set style of SPAN contents */
static void lo_SetColor( LO_Element *ele, LO_Color *color, Bool background);
static void lo_SetFontFamily( MWContext *context, LO_Element *ele, char *family);
static void lo_SetFontWeight( MWContext *context, LO_Element *ele, char *weight);
static void lo_SetFontSlant( MWContext *context, LO_Element *ele, char *weight);
static void lo_SetFontSize( MWContext *context, LO_Element *ele, int32 size);

/* Public functions */
JSObject *
LO_GetMochaObjectOfParentSpan( LO_Element *ele)
{
  LO_SpanStruct *span;
  lo_NameList *name_rec;
  JSObject *obj = NULL;

  if (LO_IsWithinSpan( ele ))
    {
      /* Go back in the layout element list until we find a SPAN element */
      span = lo_FindParentSpan(ele);
      if ( span != NULL )
	{
	  name_rec = (lo_NameList *) span->name_rec;
	  if (name_rec != NULL)
	    obj = name_rec->mocha_object;			
	}
    }
	
  return obj;
}

/* Set the color of all layout elements contained within the span */
void LO_SetSpanColor(MWContext* context, void *span, LO_Color *color)
{
  lo_NameList *name_rec = (lo_NameList *)span;
  LO_SpanStruct *parent_span = lo_FindParentSpan(name_rec->element);
  LO_Element *ele;

  ele = parent_span->lo_any.next;
  while (ele != NULL && ele->lo_any.type != LO_SPAN)
    {
      lo_SetColor(ele, color, FALSE);
      ele = ele->lo_any.next;
    }
}

/* Set the background color of all layout elements contained within the span */
void LO_SetSpanBackground(MWContext* context, void *span, LO_Color *color)
{
  lo_NameList *name_rec = (lo_NameList *)span;
  LO_SpanStruct *parent_span = lo_FindParentSpan(name_rec->element);
  LO_Element *ele;

  ele = parent_span->lo_any.next;
  while (ele != NULL && ele->lo_any.type != LO_SPAN)
    {
      lo_SetColor(ele, color, TRUE);
      ele = ele->lo_any.next;
    }
}

/* Set the font family (Arial, Helvetica, Times) of all the layout
   elements contained within the span */
void
LO_SetSpanFontFamily(MWContext* context, void *span, char *family)
{
  lo_NameList *name_rec = (lo_NameList *)span;
  LO_SpanStruct *parent_span = lo_FindParentSpan(name_rec->element);
  LO_Element *ele;

  ele = parent_span->lo_any.next;
  while (ele != NULL && ele->lo_any.type != LO_SPAN)
    {
      lo_SetFontFamily(context, ele, family);
      ele = ele->lo_any.next;
    }
}

/* Set the font weight (medium, bold, heavy, etc.) of all the layout
   elements contained within the span */
void
LO_SetSpanFontWeight(MWContext* context, void *span, char *weight)
{
  lo_NameList *name_rec = (lo_NameList *)span;
  LO_SpanStruct *parent_span = lo_FindParentSpan(name_rec->element);
  LO_Element *ele;

  ele = parent_span->lo_any.next;
  while (ele != NULL && ele->lo_any.type != LO_SPAN)
    {
      lo_SetFontWeight(context, ele, weight);
      ele = ele->lo_any.next;
    }
}

/* Set the font size (in points) of all the layout elements contained
   within the span */
void
LO_SetSpanFontSize(MWContext* context, void *span, int32 size)
{
  lo_NameList *name_rec = (lo_NameList *)span;
  LO_SpanStruct *parent_span = lo_FindParentSpan(name_rec->element);
  LO_Element *ele;

  ele = parent_span->lo_any.next;
  while (ele != NULL && ele->lo_any.type != LO_SPAN)
    {
      lo_SetFontSize(context, ele, size);
      ele = ele->lo_any.next;
    }
}

/* Set the font slant (in points) of all the layout elements contained
   within the span */
void
LO_SetSpanFontSlant(MWContext* context, void *span, char *slant)
{
  lo_NameList *name_rec = (lo_NameList *)span;
  LO_SpanStruct *parent_span = lo_FindParentSpan(name_rec->element);
  LO_Element *ele;

  ele = parent_span->lo_any.next;
  while (ele != NULL && ele->lo_any.type != LO_SPAN)
    {
      lo_SetFontSlant(context, ele, slant);
      ele = ele->lo_any.next;
    }
}

/* Functions internal to layout */
LO_SpanStruct *
lo_FindParentSpan( LO_Element *ele )
{
  while (ele != NULL && ele->lo_any.type != LO_SPAN)
    ele = ele->lo_any.prev;

  return (LO_SpanStruct *) ele;
}

/* Functions internal to this file */
static void lo_SetColor( LO_Element *ele, LO_Color *color, Bool background)
{
  switch (ele->lo_any.type)
    {
    case LO_TEXTBLOCK:
      if (background)
        {
          ele->lo_textBlock.text_attr->bg = *color;
          ele->lo_textBlock.text_attr->no_background = FALSE;
        }
      else
        {
          ele->lo_textBlock.text_attr->fg = *color;
        }
      break;
    case LO_TEXT:
      if (background)
        {
          ele->lo_text.text_attr->bg = *color;
          ele->lo_text.text_attr->no_background = FALSE;
        }
      else
        {
          ele->lo_text.text_attr->fg = *color;
        }
      break;
    case LO_BULLET:
      if (background)
        {
          ele->lo_bullet.text_attr->bg = *color;
          ele->lo_bullet.text_attr->no_background = FALSE;
        }
      else
        {
          ele->lo_bullet.text_attr->fg = *color;
        }
      break;
    default:
      break;
    }
}

static void 
lo_SetFontFamily( MWContext *context,
                  LO_Element *ele, 
                  char *new_face)
{
  LO_TextAttr *text_attr;
  LO_TextInfo text_info;

  /* if the point size is different, tell the FE to release it's data,
     set the point_size to the new value, and then have the FE
     recompute it's information. */
  switch (ele->lo_any.type)
    {
    case LO_TEXTBLOCK:
      text_attr = ele->lo_textBlock.text_attr;
      break;
    case LO_TEXT:
      text_attr = ele->lo_text.text_attr;
      break;
    case LO_BULLET:
      text_attr = ele->lo_bullet.text_attr;
      break;
    default:
      /* we don't mess with any other type of element */
      return;
    }

	if (ele->lo_any.type == LO_TEXT)
    {
      LO_TextInfo text_info;
      LO_TextStruct tmp_text;
      memset (&tmp_text, 0, sizeof (tmp_text));

      FE_ReleaseTextAttrFeData(context, text_attr);

      /* XP_FREE(text_attr->font_face); */
      text_attr->font_face = strdup(new_face);

      tmp_text = ele->lo_text;

      FE_GetTextInfo(context, &tmp_text, &text_info);
    }
	else if (ele->lo_any.type == LO_TEXTBLOCK)      
    {
	  /* XP_FREE(text_attr->font_face); */
      text_attr->font_face = strdup(new_face);
    }
}

static void 
lo_SetFontWeight( MWContext *context,
                  LO_Element *ele, 
                  char *weight)
{
  LO_TextAttr *text_attr;
  LO_TextInfo text_info;
  int new_weight = atoi(weight);

  /* if the point size is different, tell the FE to release it's data,
     set the point_size to the new value, and then have the FE
     recompute it's information. */
  switch (ele->lo_any.type)
    {
    case LO_TEXTBLOCK:
      text_attr = ele->lo_textBlock.text_attr;
      break;
    case LO_TEXT:
      text_attr = ele->lo_text.text_attr;
      break;
    case LO_BULLET:
      text_attr = ele->lo_bullet.text_attr;
      break;
    default:
      /* we don't mess with any other type of element */
      return;
    }

	if (ele->lo_any.type == LO_TEXT)
    {
      LO_TextInfo text_info;
      LO_TextStruct tmp_text;
      memset (&tmp_text, 0, sizeof (tmp_text));

      FE_ReleaseTextAttrFeData(context, text_attr);

      text_attr->font_weight = new_weight;
      if(text_attr->font_weight > 900)
        text_attr->font_weight = 900;
		
      tmp_text = ele->lo_text;

      FE_GetTextInfo(context, &tmp_text, &text_info);
    }
	else if (ele->lo_any.type == LO_TEXTBLOCK)
	{
      text_attr->font_weight = new_weight;
      if (text_attr->font_weight > 900)
        text_attr->font_weight = 900;
	}

}

static void 
lo_SetFontSlant( MWContext *context,
                 LO_Element *ele, 
                 char *slant)
{
  LO_TextAttr *text_attr;
  LO_TextInfo text_info;
  int flag;

  /* if the point size is different, tell the FE to release it's data,
     set the point_size to the new value, and then have the FE
     recompute it's information. */
  switch (ele->lo_any.type)
    {
    case LO_TEXTBLOCK:
      text_attr = ele->lo_textBlock.text_attr;
      break;
    case LO_TEXT:
      text_attr = ele->lo_text.text_attr;
      break;
    case LO_BULLET:
      text_attr = ele->lo_bullet.text_attr;
      break;
    default:
      /* we don't mess with any other type of element */
      return;
    }

  if (!strcmp(slant, "italic"))
    flag = LO_FONT_ITALIC;
  else if (!strcmp(slant, "fixed"))
    flag = LO_FONT_FIXED;

  if (ele->lo_any.type == LO_TEXTBLOCK
      && !text_attr->fontmask & flag)
    {
      LO_TextInfo text_info;
      LO_TextStruct tmp_text;
      memset (&tmp_text, 0, sizeof (tmp_text));

      FE_ReleaseTextAttrFeData(context, text_attr);

      text_attr->fontmask |= flag;

      tmp_text = ele->lo_text;

      FE_GetTextInfo(context, &tmp_text, &text_info);
    }
}

static void 
lo_SetFontSize( MWContext *context,
                LO_Element *ele, 
                int32 new_size)
{
  LO_TextAttr *text_attr;
  LO_TextInfo text_info;

  /* if the point size is different, tell the FE to release it's data,
     set the point_size to the new value, and then have the FE
     recompute it's information. */
  switch (ele->lo_any.type)
    {
    case LO_TEXTBLOCK:
      text_attr = ele->lo_textBlock.text_attr;
      break;
    case LO_TEXT:
      text_attr = ele->lo_text.text_attr;
      break;
    case LO_BULLET:
      text_attr = ele->lo_bullet.text_attr;
      break;
    default:
      /* we don't mess with any other type of element */
      return;
    }

  	if (ele->lo_any.type == LO_TEXT)
    {
      LO_TextInfo text_info;
      LO_TextStruct tmp_text;
      memset (&tmp_text, 0, sizeof (tmp_text));

      FE_ReleaseTextAttrFeData(context, text_attr);

      text_attr->point_size = new_size;

      tmp_text = ele->lo_text;

      FE_GetTextInfo(context, &tmp_text, &text_info);
    }
	else if (ele->lo_any.type == LO_TEXTBLOCK)
	{
	  text_attr->point_size = new_size;
    }

}

#endif
