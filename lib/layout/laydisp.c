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

#include "xp.h"
#include "layout.h"
#include "laylayer.h"
#include "libi18n.h"
#include "xlate.h"
#include "layers.h"

#define IL_CLIENT               /* XXXM12N Defined by Image Library clients */
#include "libimg.h"             /* Image Library public API. */

#ifdef PROFILE
#pragma profile on
#endif

#ifdef XP_MAC
# ifdef XP_TRACE
#  undef XP_TRACE
# endif
# define XP_TRACE(X)
#else
#ifndef XP_TRACE
# define XP_TRACE(X) fprintf X
#endif
#endif /* XP_MAC */


/* 
 * BUGBUG LAYERS: This is a hack that is currently being used for selection
 * but might be useful elsewhere. Essentially, for extending the selected
 * text, we know exactly how much we want to draw (using FE_DisplaySubtext).
 * Rather than ask for a synchronous refresh of the area and relying on
 * LO_RefreshArea to do the correct drawing (actually, it can't do the
 * correct drawing since it doesn't draw sub-strings), we temporarily replace
 * the painter_func for the layer in question to one that just does the
 * required drawing. We still do a synchronous refresh, but the temporary
 * painter_func is called instead of the one that calls LO_RefreshArea.
 */
typedef struct LO_SelectionHackStruct {
  MWContext *context;
  LO_TextStruct *text;
  int32 start_pos;
  int32 end_pos;
  Bool need_bg;
} LO_SelectionHackStruct;

static void
lo_selection_hack_painter_func(void *drawable, 
                               CL_Layer *layer, 
                               FE_Region update_region)
{
    LO_SelectionHackStruct *hack = (LO_SelectionHackStruct *)CL_GetLayerClientData(layer);

    FE_SetDrawable(hack->context, drawable);
    
    /* For layers, we don't ever need to clear the background */
    FE_DisplaySubtext(hack->context, FE_VIEW,
                      hack->text, hack->start_pos, hack->end_pos,
                      FALSE);

    FE_SetDrawable(hack->context, NULL);
}

static void
lo_NormalizeStartAndEndOfSelection(LO_TextStruct *text, int32 *start,
	int32 *end)
{
	int32 p1, p2;
	char *string;
	int n;
	int16 charset;

	if (text->ele_attrmask & LO_ELE_SELECTED)
	{
		if (text->sel_start < 0)
		{
			p1 = 0;
		}
		else
		{
			p1 = text->sel_start;
		}

		if (text->sel_end < 0)
		{
			p2 = text->text_len - 1;
		}
		else
		{
			p2 = text->sel_end;
		}

		PA_LOCK(string, char *, text->text);

		/*
		 * find beginning of first character
		 */
		charset = text->text_attr->charset;
		switch (n = INTL_NthByteOfChar(charset, string, (int)(p1+1)))
		{
		case 0:
		case 1:
			break;
		default:
			p1 -= (n - 1);
			if (p1 < 0)
			{
				p1 = 0;
			}
			break;
		}
		if (text->sel_start >= 0)
		{
			text->sel_start = p1;
		}

		/*
		 * find end of last character
		 */
		switch (n = INTL_NthByteOfChar(charset, string, (int)(p2+1)))
		{
		case 0:
			break;
		default:
			p2 -= (n - 1);
			if (p2 < 0)
			{
				p2 = 0;
			}
			/* fall through */
		case 1:
			p2 += INTL_IsLeadByte(charset, string[p2]);
			if (p2 > text->text_len - 1)
			{
				p2 = text->text_len - 1;
			}
			break;
		}
		if (text->sel_end >= 0)
		{
			text->sel_end = p2;
		}

		*start = p1;
		*end = p2;

		PA_UNLOCK(text->text);
	}
}

#ifdef DEBUG
void
LO_DUMP_RECT(XP_Rect *rect)
{
    XP_TRACE(("Compositing rectangle: [%3d,%3d]  %d x %d\n", rect->left, rect->top,
              rect->right - rect->left, rect->bottom - rect->top));
}
#endif

/* 
 * BUGBUG LAYERS: Since we know that lo_DisplaySubtext is only done 
 * for selection, we generate a synchronous composite. This might 
 * not be the right thing to do: maybe we should have an additional
 * argument to the LO_Display routines
 */
void
lo_DisplaySubtext(MWContext *context, LO_TextStruct *text,
                  int32 start_pos, int32 end_pos, Bool need_bg,
                  CL_Layer *sel_layer)
{
    lo_NormalizeStartAndEndOfSelection(text, &start_pos, &end_pos);
    /* If we have to draw, go through the compositor */
    if (context->compositor) {
        XP_Rect rect;
	CL_Layer *layer;
        int32 x_offset, y_offset;

        if (!sel_layer)
            layer = CL_FindLayer(context->compositor, LO_BODY_LAYER_NAME);
        else
            layer = sel_layer;

        FE_GetTextFrame(context, text, start_pos, end_pos, &rect);
        lo_GetLayerXYShift(layer, &x_offset, &y_offset);
        XP_OffsetRect(&rect, -x_offset, -y_offset);
      
        CL_UpdateLayerRect(context->compositor, layer, &rect, PR_FALSE);

        return;
  }                                                      
  else
	/* For layers, we don't ever need to clear the background */
	FE_DisplaySubtext(context, FE_VIEW, text, start_pos, end_pos,
		FALSE);
}


void
lo_DisplayText(MWContext *context,
	LO_TextStruct *text, Bool need_bg)
{
	int32 p1, p2;
	LO_TextAttr tmp_attr;
	LO_TextAttr *hold_attr;

	/* Blinking text elements are placed in a separate layer */
    if (context->compositor && text->text_attr->attrmask & LO_ATTR_BLINK)
    {
		if (! (text->ele_attrmask & LO_ELE_DRAWN))
		{
   		    /* XXX - If the BLINK is in a layer, we need to create the
			   blink layer as a child of that layer, not as a child of
			   the _BODY layer. */
			CL_Layer *body_layer =
			    CL_FindLayer(context->compositor, LO_BODY_LAYER_NAME);
			lo_CreateBlinkLayer(context, text, body_layer);
			text->ele_attrmask |= LO_ELE_DRAWN;
		}

		/* All blink text drawing is handled by the blink layer's
		   painter function.  Don't do any drawing here. */
		return;
    }

	if (text->ele_attrmask & LO_ELE_SELECTED)
	{
		lo_NormalizeStartAndEndOfSelection(text, &p1, &p2);

		if (p1 > 0)
		{
		  /* For layers we don't ever need to clear the background */
			FE_DisplaySubtext(context, FE_VIEW, text,
				0, (p1 - 1), FALSE);
		}

		lo_CopyTextAttr(text->text_attr, &tmp_attr);
		tmp_attr.fg.red = text->text_attr->bg.red;
		tmp_attr.fg.green = text->text_attr->bg.green;
		tmp_attr.fg.blue = text->text_attr->bg.blue;
		tmp_attr.bg.red = text->text_attr->fg.red;
		tmp_attr.bg.green = text->text_attr->fg.green;
		tmp_attr.bg.blue = text->text_attr->fg.blue;

        /* lo_CopyTextAttr doesn't copy the FE_Data. In
         * this case, however, we need to copy the FE_Data
         * because otherwise the front end will
         * synthesize a new copy. This assumes that
         * the FE_Data doesn't care about font color.
         */
        tmp_attr.FE_Data = text->text_attr->FE_Data;

		hold_attr = text->text_attr;
		text->text_attr = &tmp_attr;
		  /* For layers we don't ever need to clear the background */
		FE_DisplaySubtext(context, FE_VIEW, text, p1, p2,
			FALSE);
		text->text_attr = hold_attr;

		if (p2 < (text->text_len - 1))
		{
		  /* For layers we don't ever need to clear the background */
			FE_DisplaySubtext(context, FE_VIEW, text,
				(p2 + 1), (text->text_len - 1),
				FALSE);
		}
	}
	else
	{
	  /* For layers we don't ever need to clear the background */
	  FE_DisplayText(context, FE_VIEW, text, FALSE);
	}
}


void
lo_DisplayEmbed(MWContext *context, LO_EmbedStruct *embed)
{
	CL_Layer *layer;

	if (! context->compositor) {
	    FE_DisplayEmbed(context, FE_VIEW, embed);
	    return;
	}

	/* Don't ever display hidden embeds */
	if (embed->objTag.ele_attrmask & LO_ELE_HIDDEN)
		return;

	layer = embed->objTag.layer;
	XP_ASSERT(layer);
	if (! layer)				/* Paranoia */
		return;

        if (!(embed->objTag.ele_attrmask & LO_ELE_DRAWN)) {
            /* Move layer to new position */
            CL_MoveLayer(layer,
                         embed->objTag.x + embed->objTag.x_offset,
                         embed->objTag.y + embed->objTag.y_offset);
            CL_SetLayerHidden(layer, PR_FALSE);
            embed->objTag.ele_attrmask |= LO_ELE_DRAWN;
        }
}

#ifdef SHACK
void
lo_DisplayBuiltin(MWContext *context, LO_BuiltinStruct *builtin)
{
	CL_Layer *layer;

	/* need to deal with layers here XXX */

	if (! context->compositor) {
		FE_DisplayBuiltin(context, FE_VIEW, builtin);
		return;
	}

	layer = builtin->layer;
	XP_ASSERT(layer);
	if (! layer)				/* Paranoia */
		return;

        if (!(builtin->ele_attrmask & LO_ELE_DRAWN)) {
            /* Move layer to new position */
            CL_MoveLayer(layer,
                         builtin->x + builtin->x_offset,
                         builtin->y + builtin->y_offset);
            CL_SetLayerHidden(layer, PR_FALSE);
            builtin->ele_attrmask |= LO_ELE_DRAWN;
        }
}
#endif /* SHACK */

#ifdef JAVA
void
lo_DisplayJavaApp(MWContext *context, LO_JavaAppStruct *java_app)
{
	CL_Layer *layer;

	if (! context->compositor) {
	    FE_DisplayJavaApp(context, FE_VIEW, java_app);
	    return;
	}

	layer = java_app->objTag.layer;
	XP_ASSERT(layer);
	if (! layer)				/* Paranoia */
		return;

        if (!(java_app->objTag.ele_attrmask & LO_ELE_DRAWN)) {
            /* Move layer to new position */
            CL_MoveLayer(layer,
                         java_app->objTag.x + java_app->objTag.x_offset,
                         java_app->objTag.y + java_app->objTag.y_offset);
            
            /* Now that layer is layed out to its final position, 
               we can display it */
            CL_SetLayerHidden(layer, PR_FALSE);
            java_app->objTag.ele_attrmask |= LO_ELE_DRAWN;
        }
}
#endif

/* Make the image layer visible and draw the image border.  Should only be called
   if there is a compositor. */
static void
lo_DisplayImage(MWContext *context, LO_ImageStruct *image)
{
    int bw = image->border_width;

    XP_ASSERT(context->compositor);
    
    /* Allow the compositor to start drawing the image layer. */
    lo_ActivateImageLayer(context, image);

    /* The image border, if present, draws in the parent of the image layer. */
    if (bw) {
        int x = image->x + image->x_offset + bw;
        int y = image->y + image->y_offset + bw;

        FE_DisplayBorder(context, FE_VIEW, x - bw, y - bw,
                         image->width + 2 * bw, image->height + 2 * bw, bw,
                         &image->text_attr->fg, LO_SOLID);
    }
}

void
lo_DisplayImageWithoutCompositor(MWContext *context, LO_ImageStruct *image)
{
    int bw = image->border_width;
    int x = image->x + image->x_offset + bw;
    int y = image->y + image->y_offset + bw;
    int width = image->width;
    int height = image->height;

    /* This routine should only be called in the absence of a compositor. */
    XP_ASSERT(!context->compositor);
    
    /* Handle the TextFE. */
    if (context->type == MWContextText) {
        XL_DisplayTextImage(context, FE_VIEW, image);
        return;
    }

    if (image->is_icon) {
        IL_DisplayIcon(context->img_cx, image->icon_number, x, y);
    }
    else {
        int cnv_x = context->convertPixX;
        int cnv_y = context->convertPixY;

        IL_DisplaySubImage(image->image_req, x/cnv_x, y/cnv_y, 0, 0,
                           width/cnv_x, height/cnv_y);
    }

    if (bw)
        FE_DisplayBorder(context, FE_VIEW, x - bw, y - bw, width + 2 * bw,
                         height + 2 * bw, bw, &image->text_attr->fg, LO_SOLID);
}

void
lo_DisplaySubImageWithoutCompositor(MWContext *context, LO_ImageStruct *image,
                                    int32 x, int32 y, uint32 width, uint32 height)
{
    int bw = image->border_width;
    int x_pos = image->x + image->x_offset + bw;
    int y_pos = image->y + image->y_offset + bw;
    int sub_x, sub_y, sub_w, sub_h;
            
    /* This routine should only be called in the absence of a compositor. */
    XP_ASSERT(!context->compositor);
    
    /* Handle the TextFE. */
    if (context->type == MWContextText) {
        XL_DisplayTextImage(context, FE_VIEW, image);
        return;
    }

    if (x < x_pos) {
        sub_x = x_pos;
        sub_w = width + x - x_pos;
    }
    else {
        sub_x = x;
        sub_w = width;
    }

    if (y < y_pos) {
        sub_y = y_pos;
        sub_h = height + y - y_pos;
    }
    else {
        sub_y = y;
        sub_h = height;
    }

    if (x + width > x_pos + image->width)
        sub_w += (x_pos + image->width - x - width);
            
    if (y + height > y_pos + image->height)
        sub_h += (y_pos + image->height - y - height);            

    if (sub_w > 0 && sub_h > 0) {
        if (image->is_icon) {
                IL_DisplayIcon(context->img_cx, image->icon_number,
                               x_pos, y_pos);
        }
        else {
                int cnv_x = context->convertPixX;
                int cnv_y = context->convertPixY;
                
                IL_DisplaySubImage(image->image_req, x_pos/cnv_x, y_pos/cnv_y,
                               (sub_x - x_pos)/cnv_x, (sub_y - y_pos)/cnv_y,
                               sub_w/cnv_x, sub_h/cnv_y);
        }        
    }

    if (bw)
        FE_DisplayBorder(context, FE_VIEW, x - bw, y - bw, width + 2 * bw,
                         height + 2 * bw, bw, &image->text_attr->fg,
                         LO_SOLID);
}

/* Should only be called in the absence of a compositor. */
void
lo_ClipImage(MWContext *context, LO_ImageStruct *image,
	int32 x, int32 y, uint32 width, uint32 height)
{
	int32 sub_x, sub_y;
	uint32 sub_w, sub_h;

	/*
	 * If the two don't overlap, do nothing.
	 */
	if (((int32)(image->x + image->x_offset + image->width +
	             2*image->border_width) < x)||
	    ((int32)(x + width) < (int32)(image->x + image->x_offset))||
	    ((int32)(image->y + image->y_offset + image->height +
		      2*image->border_width) < y)||
	    ((int32)(y + height) < (int32)(image->y + image->y_offset)))
	{
		return;
	}

	if ((image->x + image->x_offset) >= x)
	{
	    sub_x = image->x + image->x_offset;
	}
	else
	{
	    sub_x = x;
	}

	if ((int32)(image->x + image->x_offset + image->width + 2*image->border_width)
		<= (int32)(x + width))
	{
	    sub_w = image->x + image->x_offset + image->width +
		2*image->border_width - sub_x;
	}
	else
	{
	    sub_w = x + width - sub_x;
	}

	if ((image->y + image->y_offset) >= y)
	{
	    sub_y = image->y + image->y_offset;
	}
	else
	{
	    sub_y = y;
	}

	if ((int32)(image->y + image->y_offset + image->height + 2*image->border_width)
		<= (int32)(y + height))
	{
	    sub_h = image->y + image->y_offset + image->height +
		2*image->border_width - sub_y;
	}
	else
	{
	    sub_h = y + height - sub_y;
	}

	if (((int32)sub_w >= (int32)(image->width - 2))&&
	    ((int32)sub_h >= (int32)(image->height - 2)))
	{
	    lo_DisplayImageWithoutCompositor(context, (LO_ImageStruct *)image);
	}
	else
	{
	    lo_DisplaySubImageWithoutCompositor(context, (LO_ImageStruct *)image,
		sub_x, sub_y, sub_w, sub_h);
	}
}


void
lo_DisplayEdge(MWContext *context, LO_EdgeStruct *edge)
{
	if (edge->visible != FALSE)
	{
		FE_DisplayEdge(context, FE_VIEW, edge);
	}
}


void
lo_DisplayTable(MWContext *context, LO_TableStruct *table)
{
	FE_DisplayTable(context, FE_VIEW, table);
}


void
lo_DisplaySubDoc(MWContext *context, LO_SubDocStruct *subdoc)
{
	FE_DisplaySubDoc(context, FE_VIEW, subdoc);
}


void
lo_DisplayCell(MWContext *context, LO_CellStruct *cell)
{
	/* If this cell is empty, bail */
	if (cell->cell_list == NULL && cell->cell_float_list == NULL)
		return;

	if (context->compositor && cell->cell_bg_layer)
            CL_SetLayerHidden(cell->cell_bg_layer, PR_FALSE);
        
	FE_DisplayCell(context, FE_VIEW, cell);
}

void
lo_DisplayLineFeed(MWContext *context,
	LO_LinefeedStruct *lfeed, Bool need_bg)
{
	LO_TextAttr tmp_attr;
	LO_TextAttr *hold_attr;

	if (lfeed->ele_attrmask & LO_ELE_SELECTED)
	{
		lo_CopyTextAttr(lfeed->text_attr, &tmp_attr);
		tmp_attr.fg.red = lfeed->text_attr->bg.red;
		tmp_attr.fg.green = lfeed->text_attr->bg.green;
		tmp_attr.fg.blue = lfeed->text_attr->bg.blue;
		tmp_attr.bg.red = lfeed->text_attr->fg.red;
		tmp_attr.bg.green = lfeed->text_attr->fg.green;
		tmp_attr.bg.blue = lfeed->text_attr->fg.blue;
		hold_attr = lfeed->text_attr;
		lfeed->text_attr = &tmp_attr;
		FE_DisplayLineFeed(context, FE_VIEW, lfeed, (XP_Bool)need_bg);
		lfeed->text_attr = hold_attr;
	}
	else
	{
		FE_DisplayLineFeed(context, FE_VIEW, lfeed, (XP_Bool)need_bg);
	}
}


void
lo_DisplayHR(MWContext *context, LO_HorizRuleStruct *hrule)
{
	FE_DisplayHR(context, FE_VIEW, hrule);
}


void
lo_DisplayBullet(MWContext *context, LO_BulletStruct *bullet)
{
	FE_DisplayBullet(context, FE_VIEW, bullet);
}

void
lo_DisplayFormElement(MWContext *context, LO_FormElementStruct *form_element)
{
	CL_Layer *layer;

	if (! context->compositor) {
	    FE_DisplayFormElement(context, FE_VIEW, form_element);
	    return;
	}

	layer = form_element->layer;
	XP_ASSERT(layer);
	if (! layer)				/* Paranoia */
		return;

        if (!(form_element->ele_attrmask & LO_ELE_DRAWN)) {
            /* Move layer to new position */
            CL_MoveLayer(layer,
                         form_element->x + form_element->x_offset +
			 form_element->border_horiz_space,
                         form_element->y + form_element->y_offset +
			 form_element->border_vert_space);
            CL_SetLayerHidden(layer, PR_FALSE);
            form_element->ele_attrmask |= LO_ELE_DRAWN;
        }
}

void
lo_DisplayElement(MWContext *context, LO_Element *tptr,
				  int32 base_x, int32 base_y,
				  int32 x, int32 y,
				  uint32 width, uint32 height)
{
    LO_Any *any = &tptr->lo_any;
    XP_Rect bbox;
    
    lo_GetElementBbox(tptr, &bbox);
    XP_OffsetRect(&bbox, base_x, base_y);

    if (bbox.top >= (int32)(y + height))
		return;

    if (bbox.bottom <= y )
        return;

    if (bbox.left >= (int32)(x + width))
        return;

    if (bbox.right <= x )
		return;

    /* Temporarily translate to new coordinate system */
    any->x += base_x;
    any->y += base_y;

    switch (tptr->type)
    {
    case LO_TEXT:
		if (tptr->lo_text.text != NULL)
			lo_DisplayText(context, (LO_TextStruct *)tptr, FALSE);
		break;

    case LO_LINEFEED:
		lo_DisplayLineFeed(context, (LO_LinefeedStruct *)tptr, FALSE);
		break;

    case LO_HRULE:
		lo_DisplayHR(context, (LO_HorizRuleStruct *)tptr);
		break;

    case LO_FORM_ELE:
		lo_DisplayFormElement(context, (LO_FormElementStruct *)tptr);
		break;

    case LO_BULLET:
		lo_DisplayBullet(context, (LO_BulletStruct *)tptr);
		break;

    case LO_IMAGE:
        if (context->compositor) {
            /* Allow the compositor to start drawing the image. */
            lo_DisplayImage(context, (LO_ImageStruct *)tptr);
        }
        else {
            /* There is no compositor, so draw the image directly, with the
               appropriate clip. */
            lo_ClipImage(context,
                         (LO_ImageStruct *)tptr,
                         (x + base_x),
                         (y + base_y), width, height);
        }
		break;

    case LO_TABLE:
		lo_DisplayTable(context, (LO_TableStruct *)tptr);
		break;

    case LO_EMBED:
		lo_DisplayEmbed(context, (LO_EmbedStruct *)tptr);
		break;

#ifdef SHACK
    case LO_BUILTIN:
		lo_DisplayBuiltin(context, (LO_BuiltinStruct *)tptr);
		break;
#endif /* SHACK */

#ifdef JAVA
    case LO_JAVA:
		lo_DisplayJavaApp(context, (LO_JavaAppStruct *)tptr);
		break;
#endif

    case LO_EDGE:
		lo_DisplayEdge(context, (LO_EdgeStruct *)tptr);
		break;

    case LO_CELL:
        /* If this cell is a container for an inflow layer (and
           therefore not a table cell), don't descend into the cell
           because the layer's painter function will handle its
           display. */
        if (((LO_CellStruct*)tptr)->cell_inflow_layer)
            break;

        /* cmanske: Order changed - display cell contents FIRST
         * so selection feedback near cell border is not wiped out
         * TODO: This doesn't work for images, which are display
         *  asynchronously. Need to add "observer" to image display
        */
		lo_DisplayCellContents(context, (LO_CellStruct *)tptr,
			    			   base_x, base_y, x, y, width, height);
		lo_DisplayCell(context, (LO_CellStruct *)tptr);
		break;

    case LO_SUBDOC:
    {
		LO_SubDocStruct *subdoc;
		int32 new_x, new_y;
		uint32 new_width, new_height;
		lo_DocState *sub_state;

		subdoc = (LO_SubDocStruct *)tptr;
		sub_state = (lo_DocState *)subdoc->state;

		if (sub_state == NULL)
		{
			break;
		}

		lo_DisplaySubDoc(context, subdoc);

		new_x = subdoc->x;
		new_y = subdoc->y;
		new_width = subdoc->x_offset + subdoc->width;
		new_height = subdoc->y_offset + subdoc->height;

		new_x = new_x - subdoc->x;
		new_y = new_y - subdoc->y;
		sub_state->base_x = subdoc->x +
			subdoc->x_offset + subdoc->border_width;
		sub_state->base_y = subdoc->y +
			subdoc->y_offset + subdoc->border_width;
		lo_RefreshDocumentArea(context, sub_state,
							   new_x, new_y, new_width, new_height);
    }
    break;

    case LO_PARAGRAPH:
    case LO_CENTER:
    case LO_MULTICOLUMN:
    case LO_LIST:
    case LO_DESCTITLE:
    case LO_DESCTEXT:
    case LO_BLOCKQUOTE:
    case LO_HEADING:
	case LO_SPAN:
      /* all non-display, doc-state mutating elements. */
      break;
    case LO_TEXTBLOCK:
      break;
    case LO_FLOAT:
    case LO_LAYER:
      break;
    default:
		XP_TRACE(("lo_DisplayElement(%p) skip element type = %d\n", tptr, tptr->type));
		break;
    }

    /* Restore original element's coordinates */
    any->x -= base_x;
    any->y -= base_y;
}

#ifdef PROFILE
#pragma profile off
#endif

