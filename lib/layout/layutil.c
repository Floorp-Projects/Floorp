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



#include "xp.h"
#include "pa_parse.h"
#include "layout.h"
#include "laylayer.h"
#include "laystyle.h"
#include "libmocha.h"
#include "stystruc.h"
#include "stystack.h"
#include "layers.h"
#ifdef NSPR20
#ifdef XP_MAC
#include "prpriv.h"
#else
#include "private/prpriv.h"		/* for PR_NewNamedMonitor */
#endif
#endif /* NSPR20 */
#include "intl_csi.h"

#ifdef EDITOR
#include "edt.h"
#endif
#ifdef PROFILE
#pragma profile on
#endif

#ifdef XP_WIN16
#define SIZE_LIMIT              32000
#endif /* XP_WIN16 */

#define	FONT_FACE_INC	10
#define	FONT_FACE_MAX	1000

#ifdef DOM
static void lo_SetInSpanAttribute( LO_Element *ele );
#endif

void
lo_GetElementBbox(LO_Element *element, XP_Rect *rect) 
{
    rect->left = element->lo_any.x + element->lo_any.x_offset;
    rect->top = element->lo_any.y + element->lo_any.y_offset;
    rect->right = rect->left + element->lo_any.width;
    rect->bottom = rect->top + element->lo_any.height;

    switch(element->type)
    {
    case LO_IMAGE:
    {
        LO_ImageStruct *image = &element->lo_image;
        rect->right  += 2 * (image->border_width + image->border_horiz_space);
        rect->bottom += 2 * (image->border_width + image->border_vert_space);
    }
    break;

    case LO_SUBDOC:
    {
        LO_SubDocStruct *subdoc = &element->lo_subdoc;
        rect->right  += 2 * (subdoc->border_width + subdoc->border_horiz_space);
        rect->bottom += 2 * (subdoc->border_width + subdoc->border_vert_space);
    }
    break;

    case LO_TABLE:
    {
        LO_TableStruct *table = &element->lo_table;
        rect->right  += 2 * (table->border_width + table->border_horiz_space);
        rect->bottom += 2 * (table->border_width + table->border_vert_space);
    }
    break;

    case LO_CELL:
    {
        LO_CellStruct *cell = &element->lo_cell;
        rect->right  += 2 * (cell->border_width + cell->border_horiz_space);
        rect->bottom += 2 * (cell->border_width + cell->border_vert_space);
    }
    break;

    case LO_EMBED:
    {
        LO_EmbedStruct *embed = &element->lo_embed;
        rect->right  += 2 * (embed->border_width + embed->border_horiz_space);
        rect->bottom += 2 * (embed->border_width + embed->border_vert_space);
    }
    break;

#ifdef SHACK
    case LO_BUILTIN:
    {
        LO_BuiltinStruct *builtin = &element->lo_builtin;
        rect->right  += 2 * (builtin->border_width + builtin->border_horiz_space);
        rect->bottom += 2 * (builtin->border_width + builtin->border_vert_space);
    }
    break;
#endif /* SHACK */

#ifdef JAVA
    case LO_JAVA:
    {
        LO_JavaAppStruct *java = &element->lo_java;
        rect->right  += 2 * (java->border_width + java->border_horiz_space);
        rect->bottom += 2 * (java->border_width + java->border_vert_space);
    }
    break;
#endif

    default:
        break;
    }
}

void
lo_RefreshElement(LO_Element *element, CL_Layer *layer, Bool update_now)
{
    XP_Rect rect;
    int32 x_offset, y_offset;
    CL_Compositor *compositor = CL_GetLayerCompositor(layer);

    lo_GetElementBbox(element, &rect);
    
    lo_GetLayerXYShift(layer, &x_offset, &y_offset);
    XP_OffsetRect(&rect, -x_offset, -y_offset);
    CL_UpdateLayerRect(compositor, layer, &rect, (PRBool)update_now);
}

/* Given a layout element, retrieve its anchor information. */
LO_AnchorData *
lo_GetElementAnchor(LO_Element *element)
{
	switch(element->type)
	{
		case LO_TEXT:
			return element->lo_text.anchor_href;

		case LO_IMAGE:
			return element->lo_image.anchor_href;

		case LO_TABLE:
			return element->lo_table.anchor_href;

		case LO_SUBDOC:
			return element->lo_subdoc.anchor_href;

		case LO_LINEFEED:
			return element->lo_linefeed.anchor_href;

		default:
			return NULL;
	}
}

/* Return an element's foreground color */
void
lo_GetElementFGColor(LO_Element *element, LO_Color *color)
{
	switch (element->type)
	{
	case LO_TEXT:
		color->red   = element->lo_text.text_attr->fg.red;
		color->green = element->lo_text.text_attr->fg.green;
		color->blue  = element->lo_text.text_attr->fg.blue;
		break;
		
	case LO_IMAGE:
		color->red   = element->lo_image.text_attr->fg.red;
		color->green = element->lo_image.text_attr->fg.green;
		color->blue  = element->lo_image.text_attr->fg.blue;
		break;

	case LO_SUBDOC:
		color->red   = element->lo_subdoc.text_attr->fg.red;
		color->green = element->lo_subdoc.text_attr->fg.green;
		color->blue  = element->lo_subdoc.text_attr->fg.blue;
		break;

	default:
		XP_ASSERT(0);			/* Not implemented */
		break;
	}
}

/* Set an element's foreground color */
void
lo_SetElementFGColor(LO_Element *element, LO_Color *color)
{
	switch (element->type)
	{
	case LO_TEXT:
		element->lo_text.text_attr->fg.red = color->red;
		element->lo_text.text_attr->fg.green = color->green;
		element->lo_text.text_attr->fg.blue = color->blue;
		break;
		
	case LO_IMAGE:
		element->lo_image.text_attr->fg.red = color->red;
		element->lo_image.text_attr->fg.green = color->green;
		element->lo_image.text_attr->fg.blue = color->blue;
		break;

	case LO_SUBDOC:
		element->lo_subdoc.text_attr->fg.red = color->red;
		element->lo_subdoc.text_attr->fg.green = color->green;
		element->lo_subdoc.text_attr->fg.blue = color->blue;
		break;

	default:
		XP_ASSERT(0);			/* Not implemented */
		break;
	}
}

void
lo_ShiftElementList(LO_Element *e_list, int32 dx, int32 dy)
{
	LO_Element *eptr;

	if (e_list == NULL)
	{
		return;
	}

	eptr = e_list;
	while (eptr != NULL)
	{
		eptr->lo_any.x += dx;
		eptr->lo_any.y += dy;
		if (eptr->type == LO_CELL)
		{
			lo_ShiftCell((LO_CellStruct *)eptr, dx, dy);
		}

		eptr = eptr->lo_any.next;
	}
}

NET_ReloadMethod
LO_GetReloadMethod(MWContext *context)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return NET_NORMAL_RELOAD;
	}
	state = top_state->doc_state;

	return (FORCE_RELOAD_FLAG(state->top_state));
}


void
LO_InvalidateFontData(MWContext *context)
{
	int32 i, doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	LO_TextAttr **text_attr_hash;
	LO_TextAttr *attr_ptr;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return;
	}
	state = top_state->doc_state;

	if (state->top_state->text_attr_hash == NULL)
	{
		return;
	}

	XP_LOCK_BLOCK(text_attr_hash, LO_TextAttr **,
		state->top_state->text_attr_hash);
	for (i=0; i < FONT_HASH_SIZE; i++)
	{
		attr_ptr = text_attr_hash[i];
		while (attr_ptr != NULL)
		{
			if (attr_ptr->FE_Data != NULL)
			{
				FE_ReleaseTextAttrFeData(context, attr_ptr);
			}
			attr_ptr->FE_Data = NULL;
			attr_ptr = attr_ptr->next;
		}
	}
}


static char *
lo_find_face_in_array(char **face_list, intn len, char *face)
{
	intn i;
	char *the_face;

	if (face == NULL)
	{
		return(NULL);
	}

	the_face = NULL;
	for (i=0; i<len; i++)
	{
		if ((face_list[i] != NULL)&&
			(XP_STRCMP(face, face_list[i]) == 0))
		{
			the_face = face_list[i];
			break;
		}
	}

	return(the_face);
}


Bool
lo_EvalTrueOrFalse(char *str, Bool default_val)
{
	Bool ret_val;

	ret_val = default_val;

	if (str == NULL)
	{
		return(ret_val);
	}

	if (pa_TagEqual("true", str))
	{
		ret_val = TRUE;
	}
	else if (pa_TagEqual("yes", str))
	{
		ret_val = TRUE;
	}
	else if (pa_TagEqual("no", str))
	{
		ret_val = FALSE;
	}
	else if (pa_TagEqual("false", str))
	{
		ret_val = FALSE;
	}

	return(ret_val);
}

char *
lo_FetchFontFace(MWContext *context, lo_DocState *state, char *face)
{
	char **face_list;
	char *new_face;

	/*
	 * If this is our first addition, allocate the array.
	 */
	if (state->top_state->font_face_array == NULL)
	{
		PA_Block buff;

		buff = PA_ALLOC(FONT_FACE_INC * sizeof(char *));
		if (buff == NULL)
		{
			state->top_state->out_of_memory = TRUE;
			return(NULL);
		}
		state->top_state->font_face_array = buff;
		state->top_state->font_face_array_size = FONT_FACE_INC;
		state->top_state->font_face_array_len = 0;
	}
	/*
	 * Else if the list is full, grow the array.
	 */
	else if (state->top_state->font_face_array_len >=
			state->top_state->font_face_array_size)
	{
		PA_Block buff;
		intn new_size;

		if (state->top_state->font_face_array_size >= FONT_FACE_MAX)
		{
			return(NULL);
		}

		new_size = state->top_state->font_face_array_size +
				FONT_FACE_INC;
		if (new_size > FONT_FACE_MAX)
		{
			new_size = FONT_FACE_MAX;
		}
		buff = XP_REALLOC_BLOCK(state->top_state->font_face_array,
			(new_size * sizeof(char *)));
		if (buff == NULL)
		{
			state->top_state->out_of_memory = TRUE;
			return(NULL);
		}
		state->top_state->font_face_array = buff;
		state->top_state->font_face_array_size = new_size;
	}

	PA_LOCK(face_list, char **, state->top_state->font_face_array);
	new_face = lo_find_face_in_array(face_list,
		state->top_state->font_face_array_len, face);
	if (new_face == NULL)
	{
		char *str;

		str = XP_STRDUP(face);
		if (str == NULL)
		{
			PA_UNLOCK(state->top_state->font_face_array);
			state->top_state->out_of_memory = TRUE;
			return(NULL);
		}
		face_list[state->top_state->font_face_array_len] = str;
		state->top_state->font_face_array_len++;
		new_face = str;
	}
	PA_UNLOCK(state->top_state->font_face_array);
	return(new_face);
}


Bool
lo_IsValidTarget(char *target)
{
	if (target == NULL)
	{
		return(FALSE);
	}
	if ((XP_IS_ALPHA(target[0]) != FALSE)||
	    (XP_IS_DIGIT(target[0]) != FALSE)||
	    (target[0] == '_'))
	{
		return(TRUE);
	}
	return(FALSE);
}


int32
lo_StripTextNewlines(char *text, int32 text_len)
{
	char *from_ptr;
	char *to_ptr;
	int32 len;

	if ((text == NULL)||(text_len < 1))
	{
		return(0);
	}

	/*
	 * remove all non-space whitespace from the middle of the string.
	 */
	from_ptr = text;
	len = text_len;
	to_ptr = text;
	while (*from_ptr != '\0')
	{
		if (XP_IS_SPACE(*from_ptr) && (*from_ptr != ' '))
		{
			from_ptr++;
			len--;
		}
		else
		{
			*to_ptr++ = *from_ptr++;
		}
	}
	*to_ptr = '\0';

	return(len);
}


PA_Block
lo_FEToNetLinebreaks(PA_Block text_block)
{
	PA_Block new_block;
	char *new_text;
	char *text;
	char *from_ptr;
	char *to_ptr;
	char break_char;
	int32 len, new_len;
	int32 linebreak;

	if (text_block == NULL)
	{
		return(NULL);
	}
	PA_LOCK(text, char *, text_block);

	XP_BCOPY(LINEBREAK, (char *)(&break_char), 1);
	len = 0;
	linebreak = 0;
	from_ptr = text;
	while (*from_ptr != '\0')
	{
		if (*from_ptr == break_char)
		{
			if (strncmp(from_ptr, LINEBREAK, LINEBREAK_LEN) == 0)
			{
				linebreak++;
				len += LINEBREAK_LEN;
				from_ptr = (char *)(from_ptr + LINEBREAK_LEN);
			}
			else
			{
				len++;
				from_ptr++;
			}
		}
		else
		{
			len++;
			from_ptr++;
		}
	}

	new_len = len - (LINEBREAK_LEN * linebreak) + (2 * linebreak);
	new_block = PA_ALLOC(new_len + 1);
	if (new_block == NULL)
	{
		PA_UNLOCK(text_block);
		return(NULL);
	}
	PA_LOCK(new_text, char *, new_block);
	from_ptr = text;
	to_ptr = new_text;
	while (*from_ptr != '\0')
	{
		if (*from_ptr == break_char)
		{
			if (strncmp(from_ptr, LINEBREAK, LINEBREAK_LEN) == 0)
			{
				from_ptr = (char *)(from_ptr + LINEBREAK_LEN);
				*to_ptr++ = CR;
				*to_ptr++ = LF;
			}
			else
			{
				*to_ptr++ = *from_ptr++;
			}
		}
		else
		{
			*to_ptr++ = *from_ptr++;
		}
	}
	*to_ptr = '\0';

	new_text[new_len] = '\0';
	PA_UNLOCK(new_block);
	PA_UNLOCK(text_block);
	return(new_block);
}


PA_Block
lo_ConvertToFELinebreaks(char *text, int32 text_len, int32 *new_len_ptr)
{
	PA_Block new_block;
	char *new_text;
	char *from_ptr;
	char *to_ptr;
	char break_char;
	int32 len, new_len;
	int32 skip, linebreak;

	if (text == NULL)
	{
		return(NULL);
	}

	break_char = '\0';
	len = 0;
	skip = 0;
	linebreak = 0;
	from_ptr = text;
	to_ptr = text;
	while (len < text_len)
	{
		if ((*from_ptr == CR)||(*from_ptr == LF))
		{
			if ((break_char != '\0')&&(break_char != *from_ptr))
			{
				skip++;
				break_char = '\0';
			}
			else
			{
				skip++;
				linebreak++;
				break_char = *from_ptr;
			}
		}
		from_ptr++;
		len++;
	}

	new_len = len - skip + (LINEBREAK_LEN * linebreak);
	new_block = PA_ALLOC(new_len + 1);
	if (new_block == NULL)
	{
		return(NULL);
	}
	PA_LOCK(new_text, char *, new_block);
	break_char = '\0';
	len = 0;
	from_ptr = text;
	to_ptr = new_text;
	while (len < text_len)
	{
		if ((*from_ptr == CR)||(*from_ptr == LF))
		{
			if ((break_char != '\0')&&(break_char != *from_ptr))
			{
				break_char = '\0';
			}
			else
			{
				XP_BCOPY(LINEBREAK, to_ptr, LINEBREAK_LEN);
				to_ptr = (char *)(to_ptr + LINEBREAK_LEN);
				break_char = *from_ptr;
			}
			from_ptr++;
			len++;
		}
		else
		{
			*to_ptr++ = *from_ptr++;
			len++;
		}
	}

	new_text[new_len] = '\0';
	*new_len_ptr = new_len;
	PA_UNLOCK(new_block);
	return(new_block);
}


intn
lo_EvalVAlignParam(char *str)
{
	intn alignment;

	alignment = LO_ALIGN_TOP;
	if (pa_TagEqual("top", str))
	{
		alignment = LO_ALIGN_TOP;
	}
	else if (pa_TagEqual("bottom", str))
	{
		alignment = LO_ALIGN_BOTTOM;
	}
	else if ((pa_TagEqual("middle", str))||
		(pa_TagEqual("center", str)))
	{
		alignment = LO_ALIGN_CENTER;
	}
	else if (pa_TagEqual("baseline", str))
	{
		alignment = LO_ALIGN_BASELINE;
	}
	return(alignment);
}


intn
lo_EvalDivisionAlignParam(char *str)
{
	intn alignment;

	alignment = LO_ALIGN_LEFT;
	if (pa_TagEqual("left", str))
	{
		alignment = LO_ALIGN_LEFT;
	}
	else if (pa_TagEqual("right", str))
	{
		alignment = LO_ALIGN_RIGHT;
	}
	else if (pa_TagEqual("center", str))
	{
		alignment = LO_ALIGN_CENTER;
	}
	else if (pa_TagEqual("justify", str))
	{
		alignment = LO_ALIGN_JUSTIFY;
	}
	return(alignment);
}


intn
lo_EvalCellAlignParam(char *str)
{
	intn alignment;

	alignment = LO_ALIGN_LEFT;
	if (pa_TagEqual("left", str))
	{
		alignment = LO_ALIGN_LEFT;
	}
	else if (pa_TagEqual("right", str))
	{
		alignment = LO_ALIGN_RIGHT;
	}
	else if ((pa_TagEqual("middle", str))||
		(pa_TagEqual("center", str)))
	{
		alignment = LO_ALIGN_CENTER;
	}
	return(alignment);
}

#ifdef EDITOR
/* this is a replacement routine for the non-editor version and should eventually
 *	replace the non-edit version.  It uses an array of values. instead of an 
 *	unrolled loop.  
*/

/*
 * Indexed by LO_ALIGN_ETC
*/
char* lo_alignStrings[] = {
    "abscenter",		/* LO_ALIGN_CENTER */
    "left",				/* LO_ALIGN_LEFT */
    "right",			/* LO_ALIGN_RIGHT */
    "texttop",			/* LO_ALIGN_TOP */
    "absbottom",		/* LO_ALIGN_BOTTOM */
    "baseline",			/* LO_ALIGN_BASELINE */
    "center",			/* LO_ALIGN_NCSA_CENTER */
    "bottom",			/* LO_ALIGN_NCSA_BOTTOM */
    "top",				/* LO_ALIGN_NCSA */
	0
};

intn
lo_EvalAlignParam(char *str, Bool *floating)
{
	intn alignment;

	*floating = FALSE;
	alignment = LO_ALIGN_BASELINE;
	if (pa_TagEqual("middle", str))
	{
		alignment = LO_ALIGN_NCSA_CENTER;
	}
	else if (pa_TagEqual("absmiddle", str))
	{
		alignment = LO_ALIGN_CENTER;
	}
	else 
	{
		intn i = 0;
		Bool bFound = FALSE;

		while( lo_alignStrings[i] != NULL  && bFound == FALSE )
		{
			if( pa_TagEqual( lo_alignStrings[i], str) )
			{
				bFound = TRUE;
				alignment = i;
			}
			i++;
		}	 
	}
	if( alignment == LO_ALIGN_LEFT || alignment == LO_ALIGN_RIGHT)
	{
		*floating = TRUE;
	}
	return alignment;
} 

#else

intn
lo_EvalAlignParam(char *str, Bool *floating)
{
	intn alignment;

	*floating = FALSE;
	alignment = LO_ALIGN_BASELINE;
	if (pa_TagEqual("top", str))
	{
		alignment = LO_ALIGN_NCSA_TOP;
	}
	else if (pa_TagEqual("texttop", str))
	{
		alignment = LO_ALIGN_TOP;
	}
	else if (pa_TagEqual("bottom", str))
	{
		alignment = LO_ALIGN_NCSA_BOTTOM;
	}
	else if (pa_TagEqual("absbottom", str))
	{
		alignment = LO_ALIGN_BOTTOM;
	}
	else if (pa_TagEqual("baseline", str))
	{
		alignment = LO_ALIGN_BASELINE;
	}
	else if ((pa_TagEqual("middle", str))||
		(pa_TagEqual("center", str)))
	{
		alignment = LO_ALIGN_NCSA_CENTER;
	}
	else if ((pa_TagEqual("absmiddle", str))||
		(pa_TagEqual("abscenter", str)))
	{
		alignment = LO_ALIGN_CENTER;
	}
	else if (pa_TagEqual("left", str))
	{
		alignment = LO_ALIGN_LEFT;
		*floating = TRUE;
	}
	else if (pa_TagEqual("right", str))
	{
		alignment = LO_ALIGN_RIGHT;
		*floating = TRUE;
	}
	return(alignment);
}
#endif

/* set *alignment and *floating iff there
 * is a valid stylesheet alignment property.
 */
MODULE_PRIVATE void
lo_EvalStyleSheetAlignment(StyleStruct *style_struct, 
							intn *alignment,
			   				Bool *floating)
{
	char *valign, *halign;

	if(!style_struct)
		return;

	valign = STYLESTRUCT_GetString(style_struct,VERTICAL_ALIGN_STYLE);
	halign = STYLESTRUCT_GetString(style_struct,HORIZONTAL_ALIGN_STYLE);

	if(valign)
	{
		if (pa_TagEqual("top", valign))
		{
			*alignment = LO_ALIGN_NCSA_TOP;
			*floating = FALSE;
		}
		else if (pa_TagEqual("texttop", valign) 
				 || pa_TagEqual("text-top", valign))
		{
			*alignment = LO_ALIGN_TOP;
			*floating = FALSE;
		}
		else if (pa_TagEqual("bottom", valign))
		{
			*alignment = LO_ALIGN_NCSA_BOTTOM;
			*floating = FALSE;
		}
		else if (pa_TagEqual("absbottom", valign)
				 || pa_TagEqual("text-bottom", valign))
		{
			*alignment = LO_ALIGN_BOTTOM;
			*floating = FALSE;
		}
		else if (pa_TagEqual("baseline", valign))
		{
			*alignment = LO_ALIGN_BASELINE;
			*floating = FALSE;
		}
		else if ((pa_TagEqual("middle", valign))||
			(pa_TagEqual("center", valign)))
		{
			*alignment = LO_ALIGN_NCSA_CENTER;
			*floating = FALSE;
		}
		else if ((pa_TagEqual("absmiddle", valign))||
			(pa_TagEqual("abscenter", valign)))
		{
			*alignment = LO_ALIGN_CENTER;
			*floating = FALSE;
		}
	}

	if(halign)
	{
		if (pa_TagEqual("left", halign))
		{
			*alignment = LO_ALIGN_LEFT;
			*floating = TRUE;
		}
		else if (pa_TagEqual("right", halign))
		{
			*alignment = LO_ALIGN_RIGHT;
			*floating = TRUE;
		}
	}

	XP_FREEIF(valign);
	XP_FREEIF(halign);

	return;
}

void
lo_CalcAlignOffsets(lo_DocState *state, LO_TextInfo *text_info, intn alignment,
	int32 width, int32 height, int16 *x_offset, int32 *y_offset,
	int32 *line_inc, int32 *baseline_inc)
{
	*line_inc = 0;
	*baseline_inc = 0;
	switch (alignment)
	{
		case LO_ALIGN_CENTER:
			*x_offset = 0;
			/*
			 * Center after current text.
			 */
			/*
			 * If the image is shorter than the height
			 * of this line, just increase y_offset
			 * to get it centered.
			 */
			if (height < state->line_height)
			{
				*y_offset += ((state->line_height - height)
						/ 2);
			}
			/*
			 * Else for images taller than the line
			 * we will need to move all their
			 * baselines down, plus increase the rest
			 * of their lineheights to reflect the
			 * big centered image.
			 */
			else
			{
				*y_offset = 0;
				*baseline_inc = (height - state->line_height)
						/ 2;
				*line_inc = height - state->line_height -
					*baseline_inc;
			}
			break;
		case LO_ALIGN_NCSA_CENTER:
			*x_offset = 0;
			/*
			 * Center on baseline of current text.
			 */
			/*
			 * If the image centered on the baseline will not
			 * extend below or above the current line,
			 * just increase y_offset to get it centered.
			 */
			if (((height / 2) <= state->baseline)&&
				((height / 2) <= (state->line_height - 
					state->baseline)))
			{
				*y_offset = state->baseline -
					(height / 2);
			}
			/*
			 * Else the image extends over the bottom, but
			 * not off the top.
			 */
			else if ((height / 2) <= state->baseline)
			{
				*y_offset = state->baseline -
					(height / 2);
				*line_inc = *y_offset + height -
					state->line_height;
			}
			/*
			 * Else for images taller than the line
			 * we will need to move all their
			 * baselines down, plus and maybe increase the rest
			 * of their lineheights to reflect the
			 * big centered image.
			 */
			else
			{
				*y_offset = 0;
				*baseline_inc = (height / 2) - state->baseline;
				if ((state->baseline + (height / 2)) >
					state->line_height)
				{
					*line_inc = state->baseline +
						(height / 2) -
						state->line_height;
				}
			}
			break;
		case LO_ALIGN_TOP:
			*x_offset = 0;
			/*
			 * Move the top of the image to the top of the
			 * last text placed.  On negative offsets
			 * (such as at the start of a line) just
			 * start the image at the top.
			 */
			*y_offset = state->baseline - text_info->ascent;
			if (*y_offset < 0)
			{
				*y_offset = 0;
			}

			/*
			 * Top aligned images can easily hang down and make
			 * all the rest of the line taller.  Set line_inc
			 * to properly increase the height of all previous
			 * elements.
			 */
			if ((*y_offset + height) > state->line_height)
			{
				*line_inc = *y_offset + height -
					state->line_height;
			}
			break;
		case LO_ALIGN_NCSA_TOP:
			*x_offset = 0;
			/*
			 * Move the top of the image to the top of the
			 * line.
			 */
			*y_offset = 0;

			/*
			 * Top aligned images can easily hang down and make
			 * all the rest of the line taller.  Set line_inc
			 * to properly increase the height of all previous
			 * elements.
			 */
			if ((*y_offset + height) > state->line_height)
			{
				*line_inc = *y_offset + height -
					state->line_height;
			}
			break;
		case LO_ALIGN_BOTTOM:
			*x_offset = 0;
			/*
			 * Like centered images, images shorter than
			 * the line just get move with y_offset.
			 */
			if (height < state->line_height)
			{
				*y_offset = state->line_height - height;
			}
			/*
			 * Else since they are bottom aligned.
			 * they move everyone's baselines and can't
			 * change the height below the baseline.
			 */
			else
			{
				*y_offset = 0;
				*baseline_inc = height - state->line_height;
			}
			break;
		case LO_ALIGN_NCSA_BOTTOM:
		case LO_ALIGN_BASELINE:
		default:
			*x_offset = 0;
			/*
			 * Like centered images, images shorter than
			 * the baseline just get move with y_offset.
			 */
			if (height < state->baseline)
			{
				*y_offset = state->baseline - height;
			}
			/*
			 * Else since they are baseline aligned.
			 * they move everyone's baselines and can't
			 * change the height below the baseline.
			 */
			else
			{
				*y_offset = 0;
				*baseline_inc = height - state->baseline;
			}
			break;
	}
}


int32
lo_ValueOrPercent(char *str, Bool *is_percent)
{
	int32 val;
	char *tptr;

	*is_percent = FALSE;

	if (str == NULL)
	{
		return(0);
	}

	tptr = str;
	while (*tptr != '\0')
	{
		if (*tptr == '%')
		{
			*is_percent = TRUE;
			*tptr = '\0';
			break;
		}
		tptr++;
	}
	val = XP_ATOI(str);
        if (*is_percent)
            *tptr = '%';            /* Restore original string */
	return(val);
}


void
lo_SetElementTextAttr(LO_Element *element, LO_TextAttr *attr)
{
	switch(element->type)
	{
	case LO_TEXT:
		element->lo_text.text_attr = attr;
		break;

	case LO_IMAGE:
		element->lo_image.text_attr = attr;
		break;

	case LO_SUBDOC:
		element->lo_subdoc.text_attr = attr;
		break;

	case LO_LINEFEED:
		element->lo_linefeed.text_attr = attr;
		break;

	case LO_FORM_ELE:
		element->lo_form.text_attr = attr;
		break;

	case LO_BULLET:
		element->lo_bullet.text_attr = attr;
		break;

	default:
		XP_ASSERT(0);
	}
}

LO_TextAttr *
lo_GetElementTextAttr(LO_Element *element)
{
	switch(element->type)
	{
	case LO_TEXT:
		return element->lo_text.text_attr;

	case LO_IMAGE:
		return element->lo_image.text_attr;

	case LO_SUBDOC:
		return element->lo_subdoc.text_attr;

	case LO_LINEFEED:
		return element->lo_linefeed.text_attr;

	case LO_FORM_ELE:
		return element->lo_form.text_attr;

	case LO_BULLET:
		return element->lo_bullet.text_attr;

	default:
		return NULL;
	}
}

void
lo_CopyTextAttr(LO_TextAttr *old_attr, LO_TextAttr *new_attr)
{
	if ((old_attr == NULL)||(new_attr == NULL))
	{
		return;
	}

	new_attr->size = old_attr->size;
	new_attr->fontmask = old_attr->fontmask;

	new_attr->fg.red = old_attr->fg.red;
	new_attr->fg.green = old_attr->fg.green;
	new_attr->fg.blue = old_attr->fg.blue;

	new_attr->bg.red = old_attr->bg.red;
	new_attr->bg.green = old_attr->bg.green;
	new_attr->bg.blue = old_attr->bg.blue;

	new_attr->no_background = old_attr->no_background;
	new_attr->attrmask = old_attr->attrmask;
	new_attr->font_face = old_attr->font_face;
	/*
	 * FE_Data is not copied, it belongs to the FE,
	 * And will need to be generated by the FE the first time this
	 * new TextAttr is used.
	 */
	new_attr->FE_Data = NULL;

	new_attr->charset = old_attr->charset;
	new_attr->point_size = old_attr->point_size;
	new_attr->font_weight = old_attr->font_weight;
}


LO_TextAttr *
lo_FetchTextAttr(lo_DocState *state, LO_TextAttr *old_attr)
{
	LO_TextAttr **text_attr_hash;
	LO_TextAttr *attr_ptr;
	int32 hash_index;

	if (old_attr == NULL)
	{
		return(NULL);
	}

	hash_index = (old_attr->fontmask & old_attr->attrmask) % FONT_HASH_SIZE;

	XP_LOCK_BLOCK(text_attr_hash, LO_TextAttr **,
		state->top_state->text_attr_hash);
	attr_ptr = text_attr_hash[hash_index];

	while (attr_ptr != NULL)
	{
		if ((attr_ptr->size == old_attr->size)&&
			(attr_ptr->fontmask == old_attr->fontmask)&&
			(attr_ptr->attrmask == old_attr->attrmask)&&
			(attr_ptr->fg.red == old_attr->fg.red)&&
			(attr_ptr->fg.green == old_attr->fg.green)&&
			(attr_ptr->fg.blue == old_attr->fg.blue)&&
			(attr_ptr->bg.red == old_attr->bg.red)&&
			(attr_ptr->bg.green == old_attr->bg.green)&&
			(attr_ptr->bg.blue == old_attr->bg.blue)&&
			(attr_ptr->font_face == old_attr->font_face)&&
			(attr_ptr->charset == old_attr->charset)&&
			(attr_ptr->point_size == old_attr->point_size)&&
			(attr_ptr->font_weight == old_attr->font_weight) 
#ifdef DOM
			&& (state->in_span == FALSE)  /* Never reuse text attrs in SPANS */
#endif		
			)
		{
			break;
		}
		attr_ptr = attr_ptr->next;
	}
	if (attr_ptr == NULL)
	{
		LO_TextAttr *new_attr;

		new_attr = XP_NEW_ZAP(LO_TextAttr);
		if (new_attr != NULL)
		{
			lo_CopyTextAttr(old_attr, new_attr);
			new_attr->next = text_attr_hash[hash_index];
			text_attr_hash[hash_index] = new_attr;
		}
		else
		{
			state->top_state->out_of_memory = TRUE;
		}
		attr_ptr = new_attr;
	}

	XP_UNLOCK_BLOCK(state->top_state->text_attr_hash);
	return(attr_ptr);
}


void
lo_AddMarginStack(lo_DocState *state, int32 x, int32 y,
		int32 width, int32 height,
		int32 border_width,
		int32 border_vert_space, int32 border_horiz_space,
		intn alignment)
{
	lo_MarginStack *mptr;

	mptr = XP_NEW(lo_MarginStack);
	if (mptr == NULL)
	{
		state->top_state->out_of_memory = TRUE;
		return;
	}

	mptr->y_min = y;
	if (y >= 0)
	{
		mptr->y_max = y + height + (2 * border_width) +
			(2 * border_vert_space);
	}
	else
	{
		mptr->y_max = height + (2 * border_width) +
			(2 * border_vert_space);
	}

	if (alignment == LO_ALIGN_RIGHT)
	{
		if (state->right_margin_stack == NULL)
		{
			mptr->margin = state->right_margin - width -
				(2 * border_width) - (2 * border_horiz_space);
		}
		else
		{
			mptr->margin = state->right_margin_stack->margin -
				width - (2 * border_width) -
				(2 * border_horiz_space);
		}
		mptr->next = state->right_margin_stack;
		state->right_margin_stack = mptr;
	}
	else
	{
		mptr->margin = state->left_margin + width +
			(2 * border_width) + (2 * border_horiz_space);
		mptr->next = state->left_margin_stack;
		state->left_margin_stack = mptr;
	}
}


/*
 * When clipping lines we have essentially scrolled the document up.
 * Move the margin blocks up too.
 */
void
lo_ShiftMarginsUp(MWContext *context, lo_DocState *state, int32 dy)
{
	lo_MarginStack *mptr;
	int32 y;

	mptr = state->left_margin_stack;
	while (mptr != NULL)
	{
		y = mptr->y_min - dy;
		if (y < 0)
		{
			y = 0;
		}
		mptr->y_min = y;

		y = mptr->y_max - dy;
		if (y < 0)
		{
			y = 0;
		}
		mptr->y_max = y;
		mptr = mptr->next;
	}

	mptr = state->right_margin_stack;
	while (mptr != NULL)
	{
		y = mptr->y_min - dy;
		if (y < 0)
		{
			y = 0;
		}
		mptr->y_min = y;

		y = mptr->y_max - dy;
		if (y < 0)
		{
			y = 0;
		}
		mptr->y_max = y;
		mptr = mptr->next;
	}
}


void
lo_ClearToLeftMargin(MWContext *context, lo_DocState *state)
{
	lo_MarginStack *mptr;
	int32 y;

	mptr = state->left_margin_stack;
	if (mptr == NULL)
	{
		return;
	}
	y = state->y;

	if (mptr->y_max > y)
	{
		y = mptr->y_max;
	}
	while (mptr->next != NULL)
	{
		lo_MarginStack *margin;

		if (mptr->y_max > y)
		{
			y = mptr->y_max;
		}
		margin = mptr;
		mptr = mptr->next;
		XP_DELETE(margin);
	}
	XP_DELETE(mptr);
	state->y = y;
	state->left_margin_stack = NULL;
	state->left_margin = state->win_left;
	state->x = state->left_margin;

	/*
	 * Find where we hit the right margin, popping old
	 * obsolete elements as we go.
	 */
	mptr = state->right_margin_stack;
	while ((mptr != NULL)&&(state->y > mptr->y_max))
	{
		lo_MarginStack *margin;

		margin = mptr;
		mptr = mptr->next;
		XP_DELETE(margin);
	}
	if (mptr != NULL)
	{
		state->right_margin_stack = mptr;
		state->right_margin = mptr->margin;
	}
	else
	{
		state->right_margin_stack = NULL;
		state->right_margin = (state->list_stack != NULL)
							? state->list_stack->old_right_margin
							: 0;
	}
}


void
lo_ClearToRightMargin(MWContext *context, lo_DocState *state)
{
	lo_MarginStack *mptr;
	int32 y;

	mptr = state->right_margin_stack;
	if (mptr == NULL)
	{
		return;
	}
	y = state->y;

	if (mptr->y_max > y)
	{
		y = mptr->y_max;
	}
	while (mptr->next != NULL)
	{
		lo_MarginStack *margin;

		if (mptr->y_max > y)
		{
			y = mptr->y_max;
		}
		margin = mptr;
		mptr = mptr->next;
		XP_DELETE(margin);
	}
	XP_DELETE(mptr);
	state->y = y;
	state->right_margin_stack = NULL;
	state->right_margin = state->win_width - state->win_right;

	/*
	 * Find where we hit the left margin, popping old
	 * obsolete elements as we go.
	 */
	mptr = state->left_margin_stack;
	while ((mptr != NULL)&&(state->y > mptr->y_max))
	{
		lo_MarginStack *margin;

		margin = mptr;
		mptr = mptr->next;
		XP_DELETE(margin);
	}
	if (mptr != NULL)
	{
		state->left_margin_stack = mptr;
		state->left_margin = mptr->margin;
	}
	else
	{
		state->left_margin_stack = NULL;
		state->left_margin = (state->list_stack != NULL)
						   ? state->list_stack->old_left_margin
						   : 0;
	}
}


void
lo_ClearToBothMargins(MWContext *context, lo_DocState *state)
{
	lo_MarginStack *mptr;
	int32 y;

	y = state->y;

	mptr = state->right_margin_stack;
	if (mptr != NULL)
	{
		if (mptr->y_max > y)
		{
			y = mptr->y_max;
		}
		while (mptr->next != NULL)
		{
			lo_MarginStack *margin;

			if (mptr->y_max > y)
			{
				y = mptr->y_max;
			}
			margin = mptr;
			mptr = mptr->next;
			XP_DELETE(margin);
		}
		XP_DELETE(mptr);
		state->y = y;
		state->right_margin_stack = NULL;

		state->right_margin = state->win_width - state->win_right;
	}

	mptr = state->left_margin_stack;
	if (mptr != NULL)
	{
		if (mptr->y_max > y)
		{
			y = mptr->y_max;
		}
		while (mptr->next != NULL)
		{
			lo_MarginStack *margin;

			if (mptr->y_max > y)
			{
				y = mptr->y_max;
			}
			margin = mptr;
			mptr = mptr->next;
			XP_DELETE(margin);
		}
		XP_DELETE(mptr);
		if (y > state->y)
		{
			state->y = y;
		}
		state->left_margin_stack = NULL;

		state->left_margin = state->win_left;
		state->x = state->left_margin;
	}
}


void
lo_FindLineMargins(MWContext *context, lo_DocState *state, Bool updateFE)
{
	lo_MarginStack *mptr;
	LO_Element *eptr;

	eptr = state->float_list;
	/*
	while ((eptr != NULL)&&(eptr->lo_any.y < 0))
	*/
	while (eptr != NULL)
	{
		LO_Element *next_eptr = NULL;

		if (eptr->lo_any.y < 0) 
		{
			int32 expose_y;
			int32 expose_height;
			/* LO_Element *next_eptr; */

			next_eptr = NULL;
			/*
			 * Float the y to our current position.
			 */
			eptr->lo_any.y = state->y;

			/*
			 * These get coppied into special variables because they
			 * may get changed by a table in the floating list.
			 */
			expose_y = eptr->lo_any.y;
			expose_height = eptr->lo_any.height;

			/*
			 * Special case:  If we have just placed a TABLE element,
			 * we need to properly offset all the CELLs inside it.
			 */
			if (eptr->type == LO_TABLE)
			{
				int32 y2;
				int32 change_y;
				LO_Element *tptr;

				/*
				 * We need to know the bottom and top extents of
				 * the table to know big an area to refresh.
				 */
				y2 = expose_y + eptr->lo_any.y_offset + expose_height;

				/*
				 * This is how far to move the cells based on
				 * where the table thought it was placed.
				 */
				change_y = state->y - eptr->lo_table.expected_y;

				/*
				 * set tptr to the start of the cell list.
				 */
				tptr = eptr->lo_any.next;
				while ((tptr != NULL)&&(tptr->type == LO_CELL))
				{
					/*
					 * Only do this work if the table has moved.
					 */
					if (change_y != 0)
					{
						tptr->lo_any.y += change_y;
						lo_ShiftCell((LO_CellStruct *)tptr,
							0, change_y);
					}

					/*
					 * Find upper and lower bounds of the
					 * cells.
					 */
					if (tptr->lo_any.y < expose_y)
					{
						expose_y = tptr->lo_any.y;
					}
					if ((tptr->lo_any.y + tptr->lo_any.y_offset +
						tptr->lo_any.height) > y2)
					{
						y2 = tptr->lo_any.y +
							tptr->lo_any.y_offset +
							tptr->lo_any.height;
					}

					tptr = tptr->lo_any.next;
				}

				/*
				 * Whatever is after all the table cells will
				 * really be the next eptr.
				 */
				if (tptr != NULL)
				{
					next_eptr = tptr;
				}

				/*
				 * Adjust exposed area height for lower bound.
				 * Upper bound already possibly moved.
				 */
				if ((y2 - expose_y - eptr->lo_any.y_offset) >
					expose_height)
				{
					expose_height = y2 - expose_y -
						eptr->lo_any.y_offset;
				}
			}

			if (updateFE) 
			{
				/*
				 * We may have been blocking display on this floating image.
				 * If so deal with it here.
				 */
				if ((state->display_blocked != FALSE)&&
					(state->is_a_subdoc == SUBDOC_NOT)&&
					(state->display_blocking_element_y == 0)&&
					(state->display_blocking_element_id != -1)&&
					(eptr->lo_any.ele_id >=
					state->display_blocking_element_id))
				{
					state->display_blocking_element_y =
						state->y;
					/*
					 * Special case, if the blocking element
					 * is on the first line, no blocking
					 * at all needs to happen.
					 */
					if (state->y == state->win_top)
					{
						state->display_blocked = FALSE;
						FE_SetDocPosition(context,
							FE_VIEW, 0, state->base_y);

						if (context->compositor)
						{
							XP_Rect rect;
                    
							rect.left = 0;
							rect.top = 0;
							rect.right = state->win_width;
							rect.bottom = state->win_height;
							CL_UpdateDocumentRect(context->compositor,
												  &rect, (PRBool)FALSE);
						}
					}
				}

			}	/* end if (updateFE) */



			if (context->compositor)
			{
				XP_Rect rect;
           
				rect.left = eptr->lo_any.x + eptr->lo_any.x_offset;
				rect.top = expose_y + eptr->lo_any.y_offset;
				rect.right = rect.left + eptr->lo_any.width;
				rect.bottom = rect.top + expose_height;
				CL_UpdateDocumentRect(context->compositor,
										  &rect, (PRBool)FALSE);

			}			

			/*
			 * In case these floating elements are the only thing in the
			 * doc (or subdoc) we need to set the state min and max widths
			 * when we place them.
			 */
		{
			int32 width;
			XP_Rect rect;

			rect.left = 0;
			rect.right = 0;
			lo_GetElementBbox(eptr, &rect);
			width = rect.right - rect.left;
			if (width > 0)
			{
				if (width > state->min_width)
				{
					state->min_width = width;
				}

				/*
				 * Only do this if not in a table
				 */
				if (state->is_a_subdoc == SUBDOC_NOT)
				{
					width += rect.left;
				}
				if (width > state->max_width)
				{
					state->max_width = width;
				}
			}
		
		}
			/*
			eptr = eptr->lo_any.next;
			*/
			/*
			 * If we are skipping a table's cells, set eptr
			 * properly now.
			 */
			/*
			if (next_eptr != NULL)
			{
				eptr = next_eptr;
			}
			*/
		}  /* End if (eptr->lo_any.y < 0) */

		eptr = eptr->lo_any.next;

		/*
		 * If we are skipping a table's cells, set eptr
		 * properly now.
		 */
		if (next_eptr != NULL)
		{
			eptr = next_eptr;
		}
	}  /* End while */
	
	/*
	 * Find floating elements just added to the left margin stack
	 * from the last line, and set their absolute
	 * coordinates.
	 */
	mptr = state->left_margin_stack;
	while ((mptr != NULL)&&(mptr->y_min < 0))
	{
		mptr->y_min = state->y;
		mptr->y_max = mptr->y_min + mptr->y_max;
		mptr = mptr->next;
	}

	/*
	 * Find where we hit the left margin, popping old
	 * obsolete elements as we go.
	 */
	mptr = state->left_margin_stack;
	while ((mptr != NULL)&&(state->y > mptr->y_max))
	{
		lo_MarginStack *margin;

		margin = mptr;
		mptr = mptr->next;
		XP_DELETE(margin);
	}
	if (mptr != NULL)
	{
		state->left_margin_stack = mptr;
		state->left_margin = mptr->margin;
		/*
		 * For a list wrapped around a floating image we may
		 * need to be indented from the image edge.
		 */
		if (state->list_stack->old_left_margin > mptr->margin)
		{
			state->left_margin = state->list_stack->old_left_margin;
		}
	}
	else
	{
		state->left_margin_stack = NULL;
		state->left_margin = (state->list_stack != NULL)
						   ? state->list_stack->old_left_margin
						   : 0;
	}

	/*
	 * Find floating elements just added to the right margin stack
	 * from the last line, and set their absolute
	 * coordinates.
	 */
	mptr = state->right_margin_stack;
	while ((mptr != NULL)&&(mptr->y_min < 0))
	{
		mptr->y_min = state->y;
		mptr->y_max = mptr->y_min + mptr->y_max;
		mptr = mptr->next;
	}

	/*
	 * Find where we hit the right margin, popping old
	 * obsolete elements as we go.
	 */
	mptr = state->right_margin_stack;
	while ((mptr != NULL)&&(state->y > mptr->y_max))
	{
		lo_MarginStack *margin;

		margin = mptr;
		mptr = mptr->next;
		XP_DELETE(margin);
	}
	if (mptr != NULL)
	{
		state->right_margin_stack = mptr;
		state->right_margin = mptr->margin;
	}
	else
	{
		state->right_margin_stack = NULL;
		state->right_margin = (state->list_stack != NULL)
							? state->list_stack->old_right_margin
							: 0;
	}
}

void
lo_RecycleElements(MWContext *context, lo_DocState *state, LO_Element *elements)
{
	LO_Element *eptr;

	if (elements == NULL)
	{
		return;
	}

	eptr = elements;
	while(eptr->lo_any.next != NULL)
	{
		lo_ScrapeElement(context, eptr);
		eptr = eptr->lo_any.next;
	}
	lo_ScrapeElement(context, eptr);
#ifdef MEMORY_ARENAS
    if ( EDT_IS_EDITOR(context) ) {
	    eptr->lo_any.next = state->top_state->recycle_list;
	    state->top_state->recycle_list = elements;
    }
#else
	eptr->lo_any.next = state->top_state->recycle_list;
	state->top_state->recycle_list = elements;
#endif /* MEMORY_ARENAS */
}


LO_Element *
lo_NewElement(MWContext *context, lo_DocState *state, intn type, 
		ED_Element *edit_element, intn edit_offset)
{
	LO_Element *eptr;

#ifdef MEMORY_ARENAS
	int32 size;

	switch(type)
	{
		case LO_TEXT:
			size = sizeof(LO_TextStruct);
			break;
		case LO_LINEFEED:
			size = sizeof(LO_LinefeedStruct);
			break;
		case LO_HRULE:
			size = sizeof(LO_HorizRuleStruct);
			break;
		case LO_IMAGE:
			size = sizeof(LO_ImageStruct);
			break;
		case LO_BULLET:
			size = sizeof(LO_BulletStruct);
			break;
		case LO_FORM_ELE:
			size = sizeof(LO_FormElementStruct);
			break;
		case LO_SUBDOC:
			size = sizeof(LO_SubDocStruct);
			break;
		case LO_TABLE:
			size = sizeof(LO_TableStruct);
			break;
#ifdef SHACK
		case LO_BUILTIN:
			size = sizeof(LO_BuiltinStruct);
			break;
#endif
		case LO_CELL:
			size = sizeof(LO_CellStruct);
			break;
		case LO_EMBED:
			size = sizeof(LO_EmbedStruct);
			break;
#ifdef JAVA
		case LO_JAVA:
			size = sizeof(LO_JavaAppStruct);
			break;
#endif
		case LO_EDGE:
			size = sizeof(LO_EdgeStruct);
			break;
		case LO_OBJECT:
			size = sizeof(LO_ObjectStruct);
			break;
		case LO_PARAGRAPH:
			size = sizeof(LO_ParagraphStruct);
			break;
		case LO_CENTER:
			size = sizeof(LO_CenterStruct);
			break;
		case LO_MULTICOLUMN:
			size = sizeof(LO_MulticolumnStruct);
			break;
		case LO_TEXTBLOCK:
			size = sizeof(LO_TextBlock);
			break;
		case LO_LIST:
			size = sizeof(LO_ListStruct);
			break;
		case LO_DESCTITLE:
			size = sizeof(LO_DescTitleStruct);
			break;
		case LO_DESCTEXT:
			size = sizeof(LO_DescTextStruct);
			break;
		case LO_BLOCKQUOTE:
			size = sizeof(LO_BlockQuoteStruct);
			break;
		case LO_LAYER:
			size = sizeof(LO_LayerStruct);
			break;
		case LO_HEADING:
			size = sizeof(LO_HeadingStruct);
			break;
		case LO_SPAN:
			size = sizeof(LO_SpanStruct);
			break;
		case LO_DIV:
			size = sizeof(LO_DivStruct);
			break;
		case LO_SPACER:
			size = sizeof(LO_SpacerStruct);
			break;
		default:
			size = sizeof(LO_Any);
			break;
	}
    if ( ! EDT_IS_EDITOR(context) ) {
    	eptr = (LO_Element *)lo_MemoryArenaAllocate(state->top_state, size);
    }
    else {
	    if (state->top_state->recycle_list != NULL)
	    {
		    eptr = state->top_state->recycle_list;
		    state->top_state->recycle_list = eptr->lo_any.next;
	    }
	    else
	    {
            eptr = XP_NEW_ZAP(LO_Element);
        }
    }
#else

	/* sorry about all the asserts, in my debug version somehow state
	 * becomes nil after a malloc failure. */
#ifdef DEBUG
	assert (state);
#endif
	
	if (state->top_state->recycle_list != NULL)
	{
		eptr = state->top_state->recycle_list;
		state->top_state->recycle_list = eptr->lo_any.next;
	}
	else
	{
		eptr = XP_NEW(LO_Element);
#ifdef DEBUG
		assert (state);
#endif
	} 
#endif /* MEMORY_ARENAS */
	
	if (eptr == NULL)
	{
#ifdef DEBUG
		assert (state);
#endif
		state->top_state->out_of_memory = TRUE;
	}
	else
	{
		eptr->lo_any.next = NULL;
		eptr->lo_any.prev = NULL;
#ifdef EDITOR
		eptr->lo_any.edit_element = NULL;
		eptr->lo_any.edit_offset = 0;

		if( EDT_IS_EDITOR(context) )
        {
            if( lo_EditableElement( type ) )
		    {
			    if( edit_element == 0 ){
				    edit_element = state->edit_current_element;
				    edit_offset = state->edit_current_offset;
			    }
			    EDT_SetLayoutElement( edit_element, 
									    edit_offset,
									    type,
									    eptr );
            } else if( edit_element && (type == LO_TABLE || type == LO_CELL) )
            {
                /* _TABLE_EXPERIMENT_  SAVE POINTER TO EDIT ELEMENT IN LO STRUCT */
                eptr->lo_any.edit_element = edit_element;

            }
        }
		state->edit_force_offset = FALSE;
#endif
	}

	return(eptr);
}


/* Add a named anchor to the global list of named anchors.  This does not
   associate the anchor with a layout element.  lo_SetNamedAnchor should be
   followed by a call to lo_BindNamedAnchorToElement in order to associate
   the named anchor with a layout element. */
Bool
lo_SetNamedAnchor(lo_DocState *state, PA_Block name)
{
	lo_NameList *name_rec = NULL;
    lo_DocLists *doc_lists;

	/*
	 * No named anchors are allowed within a
	 * hacked scrolling content layout document.
	 */
	if (state->top_state->scrolling_doc != FALSE)
	{
		if (name != NULL)
		{
			PA_FREE(name);
		}
		return FALSE;
	}

    doc_lists = lo_GetCurrentDocLists(state);
    
#ifdef MOCHA
    if (state->in_relayout) {
        /* Look for the old lo_NameList.  Its mocha_object is still valid. */
        lo_NameList *name_list = doc_lists->name_list;
        lo_NameList *prev;
        Bool no_match;
        char *s1, *s2;
        
        XP_ASSERT(name_list);   /* If in relayout, this cannot be empty. */
        
        prev = name_list;
        name_rec = name_list;
        while (name_rec != NULL) {
            PA_LOCK(s1, char*, name);
            PA_LOCK(s2, char*, name_rec->name);
            no_match = XP_STRCMP(s1, s2);
            PA_UNLOCK(name_rec->name);
            PA_UNLOCK(name);
            if (!no_match) {
                if (name_rec == name_list)
                    doc_lists->name_list = name_list->next;
                else
                    prev->next = name_rec->next;
                PA_FREE(name_rec->name); /* No need for this, use new one. */
                break;
            }
            prev = name_rec;
            name_rec = name_rec->next;
        }
    }
    if (!name_rec || !state->in_relayout) {
#endif /* MOCHA */
        name_rec = XP_NEW(lo_NameList);
        if (name_rec == NULL)
            {
                state->top_state->out_of_memory = TRUE;
                PA_FREE(name);
                return FALSE;
            }
#ifdef MOCHA
        name_rec->mocha_object = NULL;
    }
#endif /* MOCHA */
    name_rec->x = 0;
    name_rec->y = 0;
    name_rec->name = name;
    name_rec->element = NULL;   /* Set by lo_BindNamedAnchorToElement. */

	name_rec->next = doc_lists->name_list;
	doc_lists->name_list = name_rec;
	return TRUE;
}

/* Associate a named anchor with a layout element.  The named anchor should
   be the first one in the list of named anchors.*/
Bool
lo_BindNamedAnchorToElement(lo_DocState *state, PA_Block name,
                           LO_Element *element)
{
    Bool no_match;
	LO_Element *eptr;
	lo_NameList *name_rec;
    char *s1, *s2;
    lo_DocLists *doc_lists;

    doc_lists = lo_GetCurrentDocLists(state);
    
	if (element == NULL)
	{
		if (state->line_list == NULL)
		{
			eptr = state->end_last_line;
		}
		else
		{
			eptr = state->line_list;
			while (eptr->lo_any.next != NULL)
			{
				eptr = eptr->lo_any.next;
			}
		}
	}
	else
	{
		eptr = element;
	}

    /* Find the matching name_rec in the name_list */
    name_rec = doc_lists->name_list;
    PA_LOCK(s1, char*, name);
    while (name_rec != NULL)
    {
	PA_LOCK(s2, char*, name_rec->name);
	no_match = XP_STRCMP(s1, s2);
	PA_UNLOCK(name_rec->name);
	if (!no_match)
	    break;
	name_rec = name_rec->next;
    }
    PA_UNLOCK(name);
    if (name_rec == NULL)
        return FALSE;

	if (eptr == NULL)
	{
		name_rec->x = 0;
		name_rec->y = 0;
	}
	else
	{
		name_rec->x = eptr->lo_any.x;
		name_rec->y = eptr->lo_any.y;
	}

	name_rec->element = eptr;
    return TRUE;
}


#ifdef DOM
/* Add a named span to the global list of named spans.  This does not
   associate the span with a layout element.  lo_SetNamedSpan should be
   followed by a call to lo_BindNamedSpanToElement in order to associate
   the named span with a layout element. */
Bool
lo_SetNamedSpan(lo_DocState *state, PA_Block id)
{
	lo_NameList *name_rec = NULL;
    lo_DocLists *doc_lists;

	/*
	 * No named spans are allowed within a
	 * hacked scrolling content layout document.
	 */
	if (state->top_state->scrolling_doc != FALSE)
	{
		if (id != NULL)
		{
			PA_FREE(id);
		}
		return FALSE;
	}

    doc_lists = lo_GetCurrentDocLists(state);
    
#ifdef MOCHA
    if (state->in_relayout) {
        /* Look for the old lo_NameList.  Its mocha_object is still valid. */
        lo_NameList *span_list = doc_lists->span_list;
        lo_NameList *prev;
        Bool no_match;
        char *s1, *s2;
        
        XP_ASSERT(span_list);   /* If in relayout, this cannot be empty. */
        
        prev = span_list;
        name_rec = span_list;
        while (name_rec != NULL) {
            PA_LOCK(s1, char*, id);
            PA_LOCK(s2, char*, name_rec->name);
            no_match = XP_STRCMP(s1, s2);
            PA_UNLOCK(name_rec->name);
            PA_UNLOCK(id);
            if (!no_match) {
                if (name_rec == span_list)
                    doc_lists->span_list = span_list->next;
                else
                    prev->next = name_rec->next;
                PA_FREE(name_rec->name); /* No need for this, use new one. */
                break;
            }
            prev = name_rec;
            name_rec = name_rec->next;
        }
    }
    if (!name_rec || !state->in_relayout) {
#endif /* MOCHA */
        name_rec = XP_NEW(lo_NameList);
        if (name_rec == NULL)
            {
                state->top_state->out_of_memory = TRUE;
                PA_FREE(id);
                return FALSE;
            }
#ifdef MOCHA
        name_rec->mocha_object = NULL;
    }
#endif /* MOCHA */
    name_rec->x = 0;
    name_rec->y = 0;
    name_rec->name = id;
    name_rec->element = NULL;   /* Set by lo_BindNamedSpanToElement. */

	name_rec->next = doc_lists->span_list;
	doc_lists->span_list = name_rec;
	return TRUE;
}

/* Associate a named span with a layout element.  The named span should
   be the first one in the list of named spans.*/
Bool
lo_BindNamedSpanToElement(lo_DocState *state, PA_Block name,
						  LO_Element *element)
{
    Bool no_match;
	LO_Element *eptr;
	lo_NameList *name_rec;
    char *s1, *s2;
    lo_DocLists *doc_lists;

    doc_lists = lo_GetCurrentDocLists(state);

	if (element == NULL)
	{
		if (state->line_list == NULL)
		{
			eptr = state->end_last_line;
		}
		else
		{
			eptr = state->line_list;
			while (eptr->lo_any.next != NULL)
			{
				eptr = eptr->lo_any.next;
			}
		}
	}
	else
	{
		eptr = element;
	}

    /* Find the matching name_rec in the span_list */
    name_rec = doc_lists->span_list;
    PA_LOCK(s1, char*, name);
    while (name_rec != NULL)
    {
	PA_LOCK(s2, char*, name_rec->name);
	no_match = XP_STRCMP(s1, s2);
	PA_UNLOCK(name_rec->name);
	if (!no_match)
	    break;
	name_rec = name_rec->next;
    }
    PA_UNLOCK(name);
    if (name_rec == NULL)
        return FALSE;

	if (eptr == NULL)
	{
		name_rec->x = 0;
		name_rec->y = 0;
	}
	else
	{
		name_rec->x = eptr->lo_any.x;
		name_rec->y = eptr->lo_any.y;
	}

	name_rec->element = eptr;
    return TRUE;
}
#endif  /* DOM */

void
lo_AddNameList(lo_DocState *state, lo_DocState *old_state)
{
/*
 * Now named anchors are added directly to the
 * top_state (or layer's) name_list, so we don't have to push
 * them up. We do, however, need to update their positions, 
 * though it seems like this is done at the end of table 
 * processing. Hence this code is no longer necessary.
 */
#if 0
	lo_NameList *name_list;
	lo_NameList *nptr;

	name_list = old_state->name_list;
	while (name_list != NULL)
	{
		/*
		 * Extract the current record from the list.
		 */
		nptr = name_list;
		name_list = name_list->next;

		/*
		 * The element almost certainly has been moved
		 * from subdoc to main cell, correct its position.
		 */
		if (nptr->element != NULL)
		{
			nptr->x = nptr->element->lo_any.x;
			nptr->y = nptr->element->lo_any.y;
		}

		/*
		 * Stick it into the main name list.
		 */
		nptr->next = state->name_list;
		state->name_list = nptr;
	}
	old_state->name_list = name_list;
#endif /* 0 */
}


void
lo_CheckNameList(MWContext *context, lo_DocState *state, int32 min_id)
{
	lo_NameList *nptr;
    lo_DocLists *doc_lists;

	/*
	 * Positions of named elements are only accurate if
	 * we are not in a nested subdoc.
	 */
	if (state->is_a_subdoc != SUBDOC_NOT)
	{
		return;
	}

    doc_lists = lo_GetCurrentDocLists(state);
    
	nptr = doc_lists->name_list;
	while (nptr != NULL)
	{
		/*
		 * If the element's id is greater
		 * than the passed minimum, then
		 * the element almost certainly has been moved
		 * during table processing,
		 * correct its position.
		 */
		if ((nptr->element != NULL)&&
			(nptr->element->lo_any.ele_id > min_id))
		{
			nptr->x = nptr->element->lo_any.x;
			nptr->y = nptr->element->lo_any.y;
		}

		/*
		 * If we are blocked on a named element.
		 * Check if this name is one we are waiting on.
		 */
		if ((state->display_blocking_element_id == -1)&&
			(nptr->name != NULL))
		{
			char *name;

			PA_LOCK(name, char *, nptr->name);
			if (XP_STRCMP(name,
			    (char *)(state->top_state->name_target + 1)) == 0)
			{
				XP_FREE(state->top_state->name_target);
				state->top_state->name_target = NULL;
				if (nptr->element != NULL)
				{
					state->display_blocking_element_id =
						nptr->element->lo_any.ele_id;
				}
				else
				{
					state->display_blocking_element_id =
						state->top_state->element_id;
				}
				/*
				 * If the display is blocked for an element
				 * we havn't reached yet, check to see if
				 * it is in this line, and if so, save its
				 * y position.
				 */
				if ((state->display_blocked != FALSE)&&
				    (state->is_a_subdoc == SUBDOC_NOT)&&
				    (state->display_blocking_element_y == 0)&&
				    (state->display_blocking_element_id != -1))
				{
					state->display_blocking_element_y = nptr->y;
				}
				PA_UNLOCK(nptr->name);
			}
			else
			{
				PA_UNLOCK(nptr->name);
			}
		}

		nptr = nptr->next;
	}
}


void
lo_AppendToLineList(MWContext *context, lo_DocState *state,
	LO_Element *element, int32 baseline_inc)
{
	LO_Element *eptr;

#ifdef DOM
	if (state->in_span)
	{
		lo_SetInSpanAttribute( element );
	}

	if (state->current_span != NULL) 
	{
		if (!lo_BindNamedSpanToElement(state, state->current_span,
                                         element)) 
		{
            XP_ASSERT(FALSE);
        }
		state->current_span = NULL;
	}
#endif
	
	if (state->current_named_anchor != NULL) {
		if (!lo_BindNamedAnchorToElement(state, state->current_named_anchor,
                                         element)) {
            XP_ASSERT(FALSE);
        }
	state->current_named_anchor = NULL;
	}
#ifdef MOCHA
    else if (state->current_anchor) {
        state->current_anchor->element = element;
    }
#endif /* MOCHA */

	element->lo_any.next = NULL;
	element->lo_any.prev = NULL;
	eptr = state->line_list;
	if (eptr == NULL)
	{
		state->line_list = element;
	}
	else
	{
		while (eptr->lo_any.next != NULL)
		{
			eptr->lo_any.y_offset += baseline_inc;
			eptr = eptr->lo_any.next;
		}
		eptr->lo_any.y_offset += baseline_inc;
		eptr->lo_any.next = element;
		element->lo_any.prev = eptr;
	}
}

void
lo_SetLineArrayEntry(lo_DocState *state, LO_Element *eptr, int32 line)
{
	LO_Element **line_array;

#ifdef XP_WIN16
        intn a_size;
        intn a_indx;
        intn a_line;
        XP_Block *larray_array;
#endif /* XP_WIN16 */

	if ((line < 0) || (line >= (state->line_num - 1)))
	{
		return;
	}

#ifdef XP_WIN16
        a_size = SIZE_LIMIT / sizeof(LO_Element *);
        a_indx = (intn)(line / a_size);
        a_line = (intn)(line - (a_indx * a_size));

        XP_LOCK_BLOCK(larray_array, XP_Block *, state->larray_array);
        state->line_array = larray_array[a_indx];
        XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);
	line_array[a_line] = eptr;

        XP_UNLOCK_BLOCK(state->line_array);
        XP_UNLOCK_BLOCK(state->larray_array);
#else
        XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);
	line_array[line] = eptr;
        XP_UNLOCK_BLOCK(state->line_array);
#endif /* XP_WIN16 */
}

#define POSITIVE_SCALE_FACTOR 1.10 /* 10% */
#define NEGATIVE_SCALE_FACTOR .90  /* 10% */


/*
 * Return the scaling percentage given a font scaler
 *
 */
double
LO_GetScalingFactor(int32 scaler)
{
    double scale=1.0;
    double mult;
    int32 count;

    if(scaler < 0)
    {
        count = -scaler;
        mult = NEGATIVE_SCALE_FACTOR;
    }
    else
    {
       count = scaler;
       mult = POSITIVE_SCALE_FACTOR;
    }

    /* use the percentage scaling factor to the power of the pref */
    while(count--)
        scale *= mult;

    return scale;
}


/*
 * Return the width of the window for this context in chars in the default
 * fixed width font.  Return -1 on error.
 */
int16
LO_WindowWidthInFixedChars(MWContext *context)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	LO_TextInfo tmp_info;
	LO_TextAttr tmp_attr;
	LO_TextStruct tmp_text;
	PA_Block buff;
	char *str;
	int32 width;
	int16 char_cnt;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return((int16)-1);
	}
	state = top_state->doc_state;

	/*
	 * Create a fake text buffer to measure an 'M'.
	 */
	memset (&tmp_text, 0, sizeof (tmp_text));
	buff = PA_ALLOC(1);
	if (buff == NULL)
	{
		return((int16)-1);
	}
	PA_LOCK(str, char *, buff);
	str[0] = 'M';
	PA_UNLOCK(buff);
	tmp_text.text = buff;
	tmp_text.text_len = 1;

	/*
	 * Fill in default fixed font information.
	 */
	lo_SetDefaultFontAttr(state, &tmp_attr, context);
	tmp_attr.fontmask |= LO_FONT_FIXED;
	tmp_text.text_attr = &tmp_attr;

	/*
	 * Get the width of that 'M'
	 */
	FE_GetTextInfo(context, &tmp_text, &tmp_info);
	PA_FREE(buff);

	/*
	 * Error if bad character width
	 */
	if (tmp_info.max_width <= 0)
	{
		return((int16)-1);
	}

	width = state->win_width - state->win_left - state->win_right;
	char_cnt = (int16)(width / tmp_info.max_width);
	return(char_cnt);
}


LO_Element *
lo_FirstElementOfLine(MWContext *context, lo_DocState *state, int32 line)
{
	LO_Element *eptr;
	LO_Element **line_array;

	if ((line < 0)||(line > (state->line_num - 2)))
	{
		return(NULL);
	}

#ifdef XP_WIN16
	{
		intn a_size;
		intn a_indx;
		intn a_line;
		XP_Block *larray_array;

		a_size = SIZE_LIMIT / sizeof(LO_Element *);
		a_indx = (intn)(line / a_size);
		a_line = (intn)(line - (a_indx * a_size));
		XP_LOCK_BLOCK(larray_array, XP_Block *,
				state->larray_array);
		state->line_array = larray_array[a_indx];
		XP_LOCK_BLOCK(line_array, LO_Element **,
				state->line_array);
		eptr = line_array[a_line];
		XP_UNLOCK_BLOCK(state->line_array);
		XP_UNLOCK_BLOCK(state->larray_array);
	}
#else
	{
		XP_LOCK_BLOCK(line_array, LO_Element **,
				state->line_array);
		eptr = line_array[line];
		XP_UNLOCK_BLOCK(state->line_array);
	}
#endif /* XP_WIN16 */
	return(eptr);
}


Bool
lo_FindMochaExpr(char *str, char **expr_start, char **expr_end)
{
	char *tptr;
	char *start;
	char *end;

	if (str == NULL)
	{
		return(FALSE);
	}

	start = NULL;
	end = NULL;
	tptr = str;
	while (*tptr != '\0')
	{
		if ((*tptr == '&')&&(*(char *)(tptr + 1) == '{'))
		{
			start = tptr;
			break;
		}
		tptr++;
	}

	if (start == NULL)
	{
		return(FALSE);
	}

	tptr = (char *)(start + 2);
	while (*tptr != '\0')
	{
		if ((*tptr == '}')&&(*(char *)(tptr + 1) == ';'))
		{
			end = tptr;
			break;
		}
		tptr++;
	}

	if (end == NULL)
	{
		return(FALSE);
	}

	end = (char *)(end + 1);
	*expr_start = start;
	*expr_end = end;
	return(TRUE);
}


static char *
lo_add_string(char *base, char *add)
{
	char *new;
	int32 base_len, add_len;

	if (add == NULL)
	{
		return(base);
	}

	if (base == NULL)
	{
		new = XP_STRDUP(add);
		return(new);
	}

	base_len = XP_STRLEN(base);
	add_len = XP_STRLEN(add);
	new = (char *)XP_ALLOC(base_len + add_len + 1);
	if (new == NULL)
	{
		return(base);
	}

	XP_STRCPY(new, base);
	XP_STRCAT(new, add);
	XP_FREE(base);
	return(new);
}


static char *
lo_eval_javascript_entities(MWContext *context, char *orig_value,
							uint newline_count)
{
	char *tptr;
	char *str;
	char *new_str;
	int32 len;
	char tchar;
	Bool has_mocha;
	char *start, *end;

	if (orig_value == NULL)
	{
		return(NULL);
	}

	str = orig_value;
	len = XP_STRLEN(str);

	new_str = NULL;
	tptr = str;
	has_mocha = lo_FindMochaExpr(tptr, &start, &end);
	while (has_mocha != FALSE)
	{
		char *expr_start, *expr_end;

		if (start > tptr)
		{
			tchar = *start;
			*start = '\0';
			new_str = lo_add_string(new_str, tptr);
			*start = tchar;
		}

		expr_start = (char *)(start + 2);
		expr_end = (char *)(end - 1);
		if (expr_end > expr_start)
		{
			char *eval_str;

			tchar = *expr_end;
			*expr_end = '\0';
			eval_str = LM_EvaluateAttribute(context, expr_start,
											newline_count + 1);
			*expr_end = tchar;
			new_str = lo_add_string(new_str, eval_str);
			if (eval_str != NULL)
			{
				XP_FREE(eval_str);
			}
		}

		tptr = (char *)(end + 1);
		has_mocha = lo_FindMochaExpr(tptr, &start, &end);
	}
	if (tptr != str)
	{
		new_str = lo_add_string(new_str, tptr);
	}

	if (new_str == NULL)
	{
		new_str = str;
	}

	return(new_str);
}


void
lo_ConvertAllValues(MWContext *context, char **value_list, int32 list_len,
					uint newline_count)
{
	int32 i;

	if (LM_CanDoJS(context) == FALSE)
	{
		return;
	}

	for (i=0; i < list_len; i++)
	{
		char *str;
		char *new_str;

		str = value_list[i];
		if (str != NULL)
		{
			new_str = lo_eval_javascript_entities(context, str, newline_count);
			if (new_str != str)
			{
				XP_FREE(str);
				value_list[i] = new_str;
			}
		}
	}
}


PA_Block
lo_FetchParamValue(MWContext *context, PA_Tag *tag, char *param_name)
{
	PA_Block buff;
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);
	buff = PA_FetchParamValue(tag, param_name, INTL_GetCSIWinCSID(c));
	return(buff);
}

#include "prthread.h"
#include "prmon.h"

static PRMonitor * layout_lock_monitor = NULL;
static PRThread  * layout_lock_owner = NULL;
static int layout_lock_count = 0;

/*
 * Grab the lock around layout so that we can delete things / manipulate
 *   layout owned data structures in a separate thread
 */
void
LO_LockLayout()
{
    if(!layout_lock_monitor)
	layout_lock_monitor = PR_NewNamedMonitor("layout-lock");

    PR_EnterMonitor(layout_lock_monitor);

    while(TRUE) {

	/* no current owner or owned by this thread */
	if(layout_lock_owner == NULL ||
	   layout_lock_owner == PR_CurrentThread()) {
	    layout_lock_owner = PR_CurrentThread();
	    layout_lock_count++;

	    PR_ExitMonitor(layout_lock_monitor);
	    return;
	}

	/* owned by someone else -- wait till we can get it */
	PR_Wait(layout_lock_monitor, PR_INTERVAL_NO_TIMEOUT);

    }
}

/*
 * Release the lock and wake up anyone who was waiting to get it if
 *   we are the last release of the lock from this thread
 */
void
LO_UnlockLayout()
{

    PR_EnterMonitor(layout_lock_monitor);

#ifdef DEBUG
    /* make sure someone doesn't try to free a lock they don't own */
    XP_ASSERT(layout_lock_owner == PR_CurrentThread());
#endif

    layout_lock_count--;

    if(layout_lock_count == 0) {
	layout_lock_owner = NULL;
	PR_Notify(layout_lock_monitor);
    }
    PR_ExitMonitor(layout_lock_monitor);

}


/*
 * A (debugging) way to make sure the current thread doesn't have the
 *   layout lock
 */
Bool
LO_VerifyUnlockedLayout()
{
#ifdef DEBUG
    Bool bHolding = TRUE;
    
    if (layout_lock_monitor) {
	PR_EnterMonitor(layout_lock_monitor);
	bHolding = (layout_lock_owner != PR_CurrentThread());
	PR_ExitMonitor(layout_lock_monitor);
    }

    return bHolding;

#else
    return TRUE;
#endif
}


/*
 * The mocha lock has been released, flush the blockage to see if
 *   we can get our blocked entities evaluated now.
 */
static void
lo_JSLockReleased(void * data)
{
    MWContext * context = (MWContext *) data;
    lo_TopState * top_state;

    top_state = lo_FetchTopState(XP_DOCID(context));
    if (!top_state)
	return;

    /* free the fake element we created earlier */
    lo_FreeElement(context, top_state->layout_blocking_element, FALSE);

    /* flush the blockage */
    lo_UnblockLayout(context, top_state);
}

/* 
 * see if there are any &{} constructs for this tag.  If so,
 *   convert them now, if we can't convert them (since we couldn't
 *   get the mocha lock, for example) return FALSE
 */
Bool
lo_ConvertMochaEntities(MWContext * context, lo_DocState *state, PA_Tag * tag)
{

    	char *tptr;
	char *str;
	char *new_str;
	int32 len;
	char tchar;
	Bool has_mocha;
	char *start, *end;
    int32 cur_layer_id;

	PA_LOCK(str, char *, tag->data);
	len = tag->data_len;

	if (LM_CanDoJS(context) == FALSE)
	    return TRUE;

	new_str = NULL;
	tptr = str;
	has_mocha = lo_FindMochaExpr(tptr, &start, &end);

	if (has_mocha == FALSE)
	    return TRUE;

	/* attempt to lock JS */
	if (!LM_AttemptLockJS(lo_JSLockReleased, context))
	    return FALSE;

    /* 
     * Make sure that the current layer and all its ancestors have
     * been reflected and that the current layer has been set as the 
     * active layer, since we want to ensure correct scoping and there's 
     * no way to know that the reflect event has reached Mocha yet. 
     * Note that reflecting the object multiple times is safe, since the 
     * reflection code is idempotent.
     */
    cur_layer_id = lo_CurrentLayerId(state);
    if (cur_layer_id != LO_DOCUMENT_LAYER_ID) {
        CL_Layer *layer, *parent;
	int32 child_layer_id = cur_layer_id;
        int32 parent_layer_id;
        
	do {
	    layer = LO_GetLayerFromId(context, child_layer_id);
	    parent = CL_GetLayerParent(layer);
	    parent_layer_id = LO_GetIdFromLayer(context, parent);
	    LM_ReflectLayer(context, cur_layer_id, parent_layer_id, NULL);
	    child_layer_id = parent_layer_id;
	} while (child_layer_id != LO_DOCUMENT_LAYER_ID);

    }
    LM_SetActiveLayer(context, cur_layer_id);
	while (has_mocha != FALSE)
	{
		char *expr_start, *expr_end;

		if (start > tptr)
		{
			tchar = *start;
			*start = '\0';
			new_str = lo_add_string(new_str, tptr);
			*start = tchar;
		}

		expr_start = (char *)(start + 2);
		expr_end = (char *)(end - 1);
		if (expr_end > expr_start)
		{
			char *eval_str;

			tchar = *expr_end;
			*expr_end = '\0';
            if (state->top_state && state->top_state->nurl) {
                LM_SetDecoderStream(context, NULL, 
                                    state->top_state->nurl,
                                    JS_FALSE);
            }
			eval_str = LM_EvaluateAttribute(context, expr_start,
							tag->newline_count + 1);
			*expr_end = tchar;
			new_str = lo_add_string(new_str, eval_str);
			if (eval_str != NULL)
			{
				XP_FREE(eval_str);
			}
		}

		tptr = (char *)(end + 1);
		has_mocha = lo_FindMochaExpr(tptr, &start, &end);
	}
	if (tptr != str)
	{
		new_str = lo_add_string(new_str, tptr);
	}

	PA_FREE(tag->data);
	tag->data = (PA_Block) new_str;
	tag->data_len = XP_STRLEN((char *) tag->data);
	PA_UNLOCK(tag->data);

	LM_UnlockJS();

	return TRUE;

}

/* Called from front ends as a threadsafe way to update values of text fields. */
void
LO_UpdateTextData(lo_FormElementTextData * textData, const char * text) {

    LO_LockLayout();

    if(textData->current_text)
	XP_FREE(textData->current_text);
	        
    textData->current_text = (PA_Block) XP_STRDUP(text);

    LO_UnlockLayout();
}

LO_TableStruct *lo_GetParentTable(MWContext *pContext, LO_Element *pElement)
{
    LO_Element *tptr;
    if( !pElement )
        return NULL;

    if( pElement->lo_any.type == LO_CELL )
    {
        /* Supplied element is already a cell, start table search with it */
        tptr = pElement;
    } else {
        if( !pContext )
            return NULL;

        /* First find the cell that contains the given element */
        tptr = (LO_Element*)lo_GetParentCell(pContext, pElement);
    }

    /* Simply search back from current cell to find table element */
    while( tptr )
    {
        if( tptr->lo_any.type == LO_TABLE )
        {
            return (LO_TableStruct*)tptr;
        }
        tptr = tptr->lo_any.prev;
    } 
    return NULL;
}


/* Recusively search for cell struct containing the given element 
 * If table is found, it searches into that as well
 */
LO_CellStruct *lo_GetCellContainingElement( LO_Element *pElement, LO_CellStruct *pFirstCell )
{
    LO_CellStruct *pNestedCell = NULL;
    LO_CellStruct *pFirstNestedCell;
    LO_Element    *cell_list_ptr;
    LO_CellStruct *pLoCell;

    pLoCell = pFirstCell;
    if( pLoCell == NULL )
        return NULL;

    do {
	    cell_list_ptr = pLoCell->cell_list;
        while( cell_list_ptr )
        {
            if( cell_list_ptr == pElement )
            {
                /* We found the element - return cell containing it */
                return pLoCell;
            }
            if( cell_list_ptr->type == LO_TABLE )
            {
                /* We found a nested table
                 * Get first cell in it and search through it
                */
                pFirstNestedCell = (LO_CellStruct*)cell_list_ptr->lo_any.next;
                XP_ASSERT(pFirstNestedCell->type == LO_CELL);
                pNestedCell = lo_GetCellContainingElement(pElement, pFirstNestedCell);
                if( pNestedCell )
                {
                   /* We found cell in nested table */
                   return pNestedCell;
                }
            }
            cell_list_ptr = cell_list_ptr->lo_any.next;
        }
        pLoCell = (LO_CellStruct*)pLoCell->next;
    }  /* End when there's no more cells or we hit linefeed at end of line (top-level table)*/
    while (pLoCell && pLoCell->type == LO_CELL);

    return NULL;
}

/* Search algorithm is very similar to CEditBuffer::GetTableHitRegion()
 *   (which is similar to lo_XYtoDocumentElement except it finds only cells)
 * Search for the Cell that contains pElement
*/
LO_CellStruct *lo_GetParentCell(MWContext * pContext, LO_Element *pElement)
{
    LO_Element *tptr;
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	XP_Bool in_table;
	int32 line;
	LO_Element *end_ptr;
	LO_Element *tptrTable;
    int32 x, y, t_width, t_height;

	doc_id = XP_DOCID(pContext);
	top_state = lo_FetchTopState(doc_id);

    /* Supplied element is a cell - return it ??? */
    if( pElement && pElement->type == LO_CELL )
        return (LO_CellStruct*)pElement;

	if( pContext == NULL || pElement == NULL || top_state == NULL || top_state->doc_state == NULL )
		return NULL;
	
    state = top_state->doc_state;
	tptrTable = NULL;
	in_table = FALSE;
    
    /* Find the line that contains the supplied element
     * This defines the top-level table
    */
	x = pElement->lo_any.x+1;
    y = pElement->lo_any.y+1;

    line = lo_PointToLine(pContext, state, x, y);
    lo_GetLineEnds(pContext, state, line, &tptr, &end_ptr);

	while (tptr != end_ptr)
	{
   		t_width = tptr->lo_any.width;
   		t_height = tptr->lo_any.height;

		if (t_width <= 0)
		{
			t_width = 1;
		}

		if ((y >= tptr->lo_any.y) &&
            (y < tptr->lo_any.y + tptr->lo_any.y_offset + t_height) &&
			(x >= tptr->lo_any.x) &&
			(x < (tptr->lo_any.x + tptr->lo_any.x_offset + t_width)))
		{
            /* We are only interested in finding cells */
			if (tptr->type == LO_CELL)
            {
                /* Search through the cell's element list,
                 * Including all nested tables in that cell
                */
                LO_CellStruct * pLoCell = lo_GetCellContainingElement(pElement, (LO_CellStruct*)tptr);
                /* We found it - done */
                if( pLoCell )
                    return pLoCell;
            }
		}
		tptr = tptr->lo_any.next;
	}

    /* Failed to find element */
    return NULL;
}


/* Find the first cell with with closest left border x-value <= than the given x
 * value or, if bGetColumn=FALSE, find the cell with closest top border 
 *   y-value <= than the given Y value. Also returns the pointer to the
 *    last LO element so we can search for other cells until pEndElement is reached.
*/
LO_Element* lo_GetFirstCellInColumnOrRow(MWContext *pContext, LO_Element *pElement, int32 x, int32 y, XP_Bool bGetColumn, LO_Element **ppLastCellInTable)
{
	LO_Element *pFirstCell = NULL;
	LO_Element *pLastCellInTable = NULL;
	LO_Element *tptr = NULL;
    int32 closest = 0;

    if( pElement->type == LO_TABLE )
    {
        /* Start at first element after the table */
        tptr = pElement->lo_any.next;
    } else {
    
        if( pElement->type == LO_CELL )
        {
            /* Element is a cell */
            tptr = pElement;
        } else {
            /* Get parent cell of supplied element */
            tptr = (LO_Element*)lo_GetParentCell(pContext, pElement);
        }
        if( !tptr )
            return NULL;
    
        /* Search back until we are at first cell in table */
        while ( (tptr->lo_any.prev)->type != LO_TABLE )
        {
            tptr = tptr->lo_any.prev;
        }
    }
    
    /* Scan all cells to find first and last */
    do {
        if( tptr->type == LO_CELL )
        {
            if( bGetColumn && tptr->lo_any.x <= x &&
                tptr->lo_any.x > closest  )
            {
                /* We found a cell candidate */
                closest = tptr->lo_any.x;
                pFirstCell = tptr;
            } else if( !bGetColumn && tptr->lo_any.y <= y &&
                       tptr->lo_any.y > closest )
            {
                closest = tptr->lo_any.y;
                pFirstCell = tptr;
            }
            pLastCellInTable = tptr;
        }
        tptr = tptr->lo_any.next;

       /* Table ends at linefeed (top level) or null element (nested table) */
    } while (tptr && tptr->type != LO_LINEFEED ); 

    /* Return last cell element encountered */
    if( ppLastCellInTable )
        *ppLastCellInTable = pLastCellInTable;

    return pFirstCell;    
}

/* Assumptions;
 * Given a LO_CellStruct, the table element defines the begining of table
 *   and the last cell is that whose "next" pointer is NULL (nested table)
 *   or a linefeed element (top-level table)
*/
LO_Element *lo_GetFirstAndLastCellsInTable(MWContext *pContext, LO_Element *pElement, LO_Element **ppLastCell)
{
	LO_Element *tptr = NULL;
	LO_Element *pFirstCell = NULL;
	LO_Element *pLastCell = NULL;
	LO_Element *pCurrentCell = NULL;

    if(pContext == NULL || pElement == NULL )
        return NULL;

    if( pElement->type == LO_CELL )
    {
        pCurrentCell = pElement;
    }
    else if( pElement->type == LO_TABLE )
    {
        /* First cell SHOULD be next element, but let's be sure */
        pCurrentCell = pElement->lo_table.next;
        while(pCurrentCell->type != LO_CELL)
        {
            pCurrentCell = pCurrentCell->lo_any.next;
        }
    } else {
        pCurrentCell = (LO_Element*)lo_GetParentCell(pContext, pElement);
    }

    if( pCurrentCell == NULL )
        return NULL;

    /* Scan backwards until we hit the table element
     *  that's before all the cells
    */
    tptr = pCurrentCell;
    while (tptr && (tptr->lo_any.prev)->type != LO_TABLE )
    {
        tptr = tptr->lo_any.prev;
    }
    
	/* Scan forward to find last cell -- next is null or line-feed element */
    do {
        if( tptr->type == LO_CELL )
        {
            if( !pFirstCell )
                pFirstCell = tptr;

            pLastCell = tptr;
        }
        tptr = tptr->lo_any.next;
    } while (tptr && tptr->type != LO_LINEFEED); 

    /* Return last cell element encountered */
    if( ppLastCell )
        *ppLastCell = pLastCell;

    return pFirstCell;    
}

LO_Element *lo_GetFirstAndLastCellsInColumnOrRow(LO_Element *pCellElement, LO_Element **ppLastCell, XP_Bool bInColumn )
{
	LO_Element *tptr = NULL;
	LO_Element *pFirstCell = NULL;
	LO_Element *pLastCell = NULL;

    if(pCellElement == NULL || pCellElement->type != LO_CELL)
        return NULL;

    /* Scan backwards until we hit the table element
     *  that's before all the cells
    */
    tptr = pCellElement;
    while (tptr && (tptr->lo_any.prev)->type != LO_TABLE )
    {
        tptr = tptr->lo_any.prev;
    }
    
	/* Scan forward to find first and last cells,
     *   based on sharing left edge (column) or top (row)
     */
    do {
        if( tptr->type == LO_CELL )
        {
            if( (bInColumn && pCellElement->lo_any.x == tptr->lo_any.x) ||
                (!bInColumn && pCellElement->lo_any.y == tptr->lo_any.y) )
            {
                if( !pFirstCell )
                    pFirstCell = tptr;

                pLastCell = tptr;
            }
        }
        tptr = tptr->lo_any.next;
    } while (tptr && tptr->type != LO_LINEFEED); 

    /* Return last cell element encountered */
    if( ppLastCell )
        *ppLastCell = pLastCell;

    return pFirstCell;    
}

XP_Bool lo_AllCellsSelectedInColumnOrRow( LO_CellStruct *pCell, XP_Bool bInColumn )
{
	LO_Element *pFirstCell;
	LO_Element *pLastCell = NULL;
	LO_Element *tptr;

    /* Get first and last cells in row or column */
    pFirstCell = lo_GetFirstAndLastCellsInColumnOrRow( (LO_Element*)pCell, &pLastCell, bInColumn );
    if( !pFirstCell || !pLastCell )
        return FALSE;

    tptr = pFirstCell;

    do {
        if( tptr->type == LO_CELL )
        {
            if( (bInColumn && pCell->x == tptr->lo_any.x) ||
                (!bInColumn && pCell->y == tptr->lo_any.y) )
            {
                /* If any cell is not selected, return FALSE */
                if( !(pCell->ele_attrmask & LO_ELE_SELECTED) )
                    return FALSE;
            }
        }
        tptr = tptr->lo_any.next;
    } while (tptr != pLastCell); 

    return TRUE;
}

int32 lo_GetNumberOfCellsInTable(LO_TableStruct *pTable )
{
    int32 iCount = 0;
    if( pTable )
    {
        LO_Element *tptr = (LO_Element*)pTable;
        while( tptr && tptr->type != LO_LINEFEED )
        {
            if( tptr->type == LO_CELL)
                iCount++;
            
            tptr = tptr->lo_any.next;
        }
    }
    return iCount;
}

/* Subtract borders and inter-cell spacing to get the available table width
 *   to use when cell width in HTML is "percent of table"
*/
int32 lo_CalcTableWidthForPercentMode(LO_Element *pCellElement)
{
    LO_Element     *pLastCell;
    LO_Element     *pFirstCell;
    LO_Element     *pCell;
    int32          iWidth;
    XP_Bool        bDone;

    /* Never return 0 to avoid divide by zero errors */
    if( !pCellElement || pCellElement->type != LO_CELL )
        return -1;
 
    iWidth = 0;

    /* We calculate the width by simply adding up widths of all cells in the row */
    pFirstCell = lo_GetFirstAndLastCellsInColumnOrRow(pCellElement, &pLastCell, FALSE );
    pCell = pFirstCell;

    if( pCell && pLastCell )
    {
        bDone = FALSE;
        while(!bDone) 
        {
            if( pCell == pLastCell )
                bDone = TRUE;

            /* Add widths of all cells in the row (must have same top) */
            if( pCell->type == LO_CELL && 
                (pCell->lo_any.y == pFirstCell->lo_any.y) )
            {
                iWidth += pCell->lo_cell.width;            
            }
            pCell = pCell->lo_any.next;
        }
    }    
    return (iWidth > 0) ? iWidth : -1;
}

XP_Bool LO_IsEmptyCell(LO_CellStruct *cell)
{
    if ( cell )
    {
        LO_Element *element = cell->cell_list;

        /* Unlikely, but if nothing inside cell, we're empty! */
        if(element)
        {
            LO_Element *pNext = element->lo_any.next;
            while(element)
            {
                /* Any non-text element except linefeed is not "empty"
                 *  and check for text length
                 * Length of any element is 1. Length of text = real length,
                 * (but we don't want to consider linefeeds)
                */
                if( element->type != LO_LINEFEED && lo_GetElementLength(element) > 0 )
                {
                    return FALSE;
                }
                element = element->lo_any.next;
            }
        }
    }
    return TRUE;
}

/* The LO_CellStruct.width does not include border, cell padding etc and is complicated
 * by Column Span as well. Calculate the value to use for <TD WIDTH> param that 
 * would result in current pCellElement->lo_cell.width during the next layout
*/ 
int32 lo_GetCellTagWidth(LO_Element *pCellElement)
{
    /* XP_ASSERT(pCellElement->type == LO_CELL); */ /* Why doesn't this compile! */
    int32 iColSpan = lo_GetColSpan(pCellElement);
    int32 iPadding = iColSpan * 2 * (pCellElement->lo_cell.border_width + 
                                     lo_GetCellPadding(pCellElement));
    
    /* Cells spanning across a border include the inter-cell space as well */
    if( iColSpan > 1 )
        iPadding += (iColSpan - 1) * pCellElement->lo_cell.inter_cell_space;

    return pCellElement->lo_cell.width - iPadding;
}

/* Similar calculation for height. Unfortunately, there are differences from width.
 * e.g., the cell border must be subtracted from width, but not height! (a bug???)
*/
int32 lo_GetCellTagHeight(LO_Element *pCellElement)
{
    /*XP_ASSERT(pCellElement->type == LO_CELL);*/

    int32 iRowSpan = lo_GetRowSpan(pCellElement);
    int32 iPadding = iRowSpan * 2 * (/*pCellElement->lo_cell.border_width + */
                                     lo_GetCellPadding(pCellElement));
    
    if( iRowSpan > 1 )
        iPadding += (iRowSpan - 1) * pCellElement->lo_cell.inter_cell_space;

    return pCellElement->lo_cell.height - iPadding;
}

/* Helpers to access the lo_TableCell members now accessible through the LO_CellStruct */
int32 lo_GetRowSpan(LO_Element *pCellElement)
{
    XP_ASSERT(pCellElement->type == LO_CELL);
    
    if( pCellElement && pCellElement->lo_any.type == LO_CELL &&
        pCellElement->lo_cell.table_cell )
    {
        return ((lo_TableCell*)pCellElement->lo_cell.table_cell)->rowspan;
    }
    /* Should never fail, but since default value is 1,
       this will tell caller we failed */
    return 0;
}

int32 lo_GetColSpan(LO_Element *pCellElement)
{
    XP_ASSERT(pCellElement->type == LO_CELL);
    if( pCellElement && pCellElement->lo_any.type == LO_CELL &&
        pCellElement->lo_cell.table_cell )
    {
        return ((lo_TableCell*)pCellElement->lo_cell.table_cell)->colspan;
    }
    return 0;
}

/* TODO: CHANGE THIS TO GET LEFT, TOP, RIGHT, OR BOTTOM SEPARATELY? */
int32 lo_GetCellPadding(LO_Element *pCellElement)
{
    XP_ASSERT(pCellElement->type == LO_CELL);
    return ((lo_TableRec*)pCellElement->lo_cell.table)->inner_top_pad;
}

LO_Element * lo_GetLastElementInList( LO_Element *eleList )
{
	LO_Element *retEle = NULL;

	if (eleList != NULL)
	{
		while (eleList->lo_any.next != NULL)
			eleList = eleList->lo_any.next;

		retEle = eleList;
	}

	return retEle;
		
}

#ifdef DOM
/* Checks the layout element to see if the element is enclosed within <SPAN> */
Bool LO_IsWithinSpan( LO_Element *ele )
{
	Bool isInSpan = FALSE;

	if (ele)
	{
		switch (ele->type) {

		case LO_TEXT:
			isInSpan = ele->lo_text.ele_attrmask & LO_ELE_IN_SPAN ? TRUE : FALSE;
			break;
		case LO_IMAGE:
			isInSpan = ele->lo_image.ele_attrmask & LO_ELE_IN_SPAN ? TRUE : FALSE;
			break;
		case LO_FORM_ELE:
			isInSpan = ele->lo_form.ele_attrmask & LO_ELE_IN_SPAN ? TRUE : FALSE;
			break;
		case LO_EMBED:
			isInSpan = ele->lo_embed.ele_attrmask & LO_ELE_IN_SPAN ? TRUE : FALSE;
			break;
		case LO_JAVA:
			isInSpan = ele->lo_java.ele_attrmask & LO_ELE_IN_SPAN ? TRUE : FALSE;
			break;
#ifdef SHACK
		case LO_BUILTIN:
			isInSpan = ele->lo_builtin.ele_attrmask & LO_ELE_IN_SPAN ? TRUE : FALSE;
			break;
#endif
		}
	}
	return isInSpan;
}

static void lo_SetInSpanAttribute( LO_Element *ele )
{
	switch (ele->type) {

	case LO_TEXT:
		ele->lo_text.ele_attrmask |= LO_ELE_IN_SPAN;
		break;
	case LO_IMAGE:
		ele->lo_image.ele_attrmask |= LO_ELE_IN_SPAN;
		break;
	case LO_FORM_ELE:
		ele->lo_form.ele_attrmask |= LO_ELE_IN_SPAN;
		break;
	case LO_EMBED:
		ele->lo_embed.ele_attrmask |= LO_ELE_IN_SPAN;
		break;
#ifdef JAVA
	case LO_JAVA:
		ele->lo_java.ele_attrmask |= LO_ELE_IN_SPAN;
		break;
#endif
	}
}
#endif

#ifdef PROFILE
#pragma profile off
#endif
