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
#include "pa_parse.h"
#include "layout.h"
#include "laylayer.h"
#include "glhist.h"
#ifdef JAVA
#include "java.h"
#endif
#include "libi18n.h"
#include "edt.h"
#include "laystyle.h"
#include "prefapi.h"
#include "xp_ncent.h"
#include "prefetch.h"
#include "np.h"

/* style sheet tag stack and style struct */
#include "stystack.h"
#include "stystruc.h"
#include "pics.h"

#include "libmocha.h"
#include "libevent.h"

#include "intl_csi.h"
/* WEBFONTS are defined only in laytags.c and layout.c */
#define WEBFONTS

#ifdef WEBFONTS
#include "nf.h"
#include "Mnffbu.h"
#endif /* WEBFONTS */	

#ifdef XP_WIN16
#define SIZE_LIMIT 32000
#endif

/* RDF is currently only in Mac & Windows builds */
#if defined(XP_MAC) || defined(XP_WIN)
#include "htrdf.h"
#endif

#define	LIST_MARGIN_INC		(FEUNITS_X(40, context))
#define MQUOTE_MARGIN_INC   (LIST_MARGIN_INC / 3)

#if 0
/* cmanske: Moved to layout.h so Composer can use them */
#define MIN_FONT_SIZE		1
#define MAX_FONT_SIZE		7

#define DEFAULT_BASE_POINT_SIZE     12
#define MIN_POINT_SIZE		1
#define MAX_POINT_SIZE		1600
#endif

#define DEFAULT_BASE_FONT_WEIGHT  	400
#define MIN_FONT_WEIGHT				100
#define MAX_FONT_WEIGHT				900

#define	HYPE_ANCHOR		"about:hype"
#define	HYPE_TAG_BECOMES	"SRC=internal-gopher-sound BORDER=0>"

/* Added to encapsulate code that was previously in six different places in LO_LayoutTag()! */
static void lo_ProcessFontTag( lo_DocState *state, PA_Tag *tag, int32 fontSpecifier, int32 attrSpecifier );

#ifdef OJI
#define JAVA_PLUGIN_MIMETYPE "application/x-java-vm"
static void lo_AddParam(PA_Tag* tag, char* aName, char* aValue);
#endif

/*************************************
 * Function: LO_ChangeFontSize
 *
 * Description: Utility function to change a size field based on a
 *	string which is either a new absolute value, or a relative
 *	change prefaced with + or -.  Also sanifies the result
 *	to a valid font size.
 *
 * Params: Original size, and change string.
 *
 * Returns: New size.
 *************************************/
PRIVATE
intn
LO_ChangeFontOrPointSize(intn size, 
						 char *size_str, 
						 intn lower_bound, 
						 intn upper_bound)
{
	intn new_size;

	if ((size_str == NULL)||(*size_str == '\0'))
	{
		return(size);
	}

	if (*size_str == '+')
	{
		new_size = size + XP_ATOI((size_str + 1));
	}
	else if (*size_str == '-')
	{
		new_size = size - XP_ATOI((size_str + 1));
	}
	else
	{
		new_size = XP_ATOI(size_str);
	}

	if (new_size < lower_bound)
	{
		new_size = lower_bound;
	}
	if (new_size > upper_bound)
	{
		new_size = upper_bound;
	}

	return(new_size);
}

intn
LO_ChangeFontSize(intn size, char *size_str)
{
	return LO_ChangeFontOrPointSize(size, size_str, MIN_FONT_SIZE, MAX_FONT_SIZE);
}

PRIVATE
intn
LO_ChangePointSize(intn size, char *size_str)
{
	return LO_ChangeFontOrPointSize(size, size_str, MIN_POINT_SIZE, MAX_POINT_SIZE);
}

/*
 * Filter out tags we should ignore based on current state.
 * Return FALSE for tags we should ignore, and TRUE for
 * tags we should keep.
 */
Bool
lo_FilterTag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	lo_TopState *top_state;

	top_state = state->top_state;
	if (top_state == NULL)
	{
		return(FALSE);
	}

	if ((tag->data != NULL) && (tag->newline_count != LO_IGNORE_TAG_MARKER) &&
        (state->in_relayout == FALSE) &&
        (lo_IsTagInSourcedLayer(state, tag) == FALSE))
	{
		top_state->layout_bytes += tag->true_len;
	}

	/*
	 * If we are in a GRID, all non-grid tags are discarded
	 * If we are in a NOGRIDS, all tags except its end are discarded.
	 */
	if (top_state->is_grid != FALSE)
	{
		if (tag->type == P_NOGRIDS)
		{
			if (tag->is_end == FALSE)
			{
				top_state->in_nogrids = TRUE;
			}
			else
			{
				top_state->in_nogrids = FALSE;
			}
			return(FALSE);
		}
		else if (top_state->in_nogrids != FALSE)
		{
			return(FALSE);
		}
		else if ((tag->type != P_GRID)&&
			 (tag->type != P_GRID_CELL))
		{
			return(FALSE);
		}
		return(TRUE);
	}

    /*
     * For the LAYER tag, a SUPPRESS attribute means that all content
     * till the corresponding </LAYER> should be ignored. To figure
     * out which is the corresponding </LAYER> tag, we maintain a
     * nesting count of LAYER tags.
     */
    if ((tag->type == P_LAYER) || (tag->type == P_ILAYER)) {
        /*
         * If we're inside a <LAYER SUPPRESS> and this is a </LAYER>
         * tag, then decrement the layer nesting count. If it goes
         * down to 0, then this is the ending of the suppress tag
         * and we decrement the ignore_tag_nest_level count.
         */
        if (top_state->ignore_layer_nest_level && tag->is_end) {
            if ((--top_state->ignore_layer_nest_level == 0) &&
                (top_state->ignore_tag_nest_level > 0)) {
                top_state->ignore_tag_nest_level--;
                /* We throw away this </LAYER> tag */
                return FALSE;
            }
        }
        /* 
         * If we're inside a <LAYER SUPPRESS> and this is a <LAYER>
         * tag, then just increment the layer nesting count.
         */
        else if (top_state->ignore_tag_nest_level > 0) {
            top_state->ignore_layer_nest_level++;
        }
        /* Otherwise, just check for the SUPPRESS attribute */
        else {
            char *buff;
            
            buff = (char*) lo_FetchParamValue(context, tag, PARAM_SUPPRESS);
            if (buff) {
                top_state->ignore_tag_nest_level++;
                top_state->ignore_layer_nest_level++;
                XP_FREE(buff);
            }
        }
    }
    
/* Not all UNIXes can do embed yet, so display the NOEMBED stuff */
#if defined(XP_UNIX) && !defined(UNIX_EMBED)
    #define SUPPRESS_NOEMBED_CONTENTS  0
#else
    #define SUPPRESS_NOEMBED_CONTENTS  1
#endif

    /*
     * Handle NOSCRIPT, NOEMBED and NOLAYER.
     */
	if (((tag->type == P_NOSCRIPT) && LM_CanDoJS(context)) ||
        ((tag->type == P_NOLAYER)) ||
        ((tag->type == P_NOEMBED) && SUPPRESS_NOEMBED_CONTENTS))
    {
		if (!tag->is_end)
            top_state->ignore_tag_nest_level++;
		else if (top_state->ignore_tag_nest_level > 0)
            top_state->ignore_tag_nest_level--;
	}

	/* Inside NOEMBED, NOSCRIPT, or NOLAYER ignore everything
	 * except the closing tag.
	 */
    if (top_state->ignore_tag_nest_level > 0)
        return FALSE;

	/*
	 * If we're inside an object, check to see whether we
	 * should be filtering the contents or not:
	 *
	 * + If we know how to handle the object, we should filter
	 *	 (except for PARAMs and nested OBJECTs, which we let
	 *	 pass the filter so the code in lo_LayoutTag can keep
	 *	 track of which parameters go with which object).
	 *
	 * + If we don't know how to handle the object (type is
	 *	 LO_UNKNOWN), or don't yet know if we know how to
	 *	 handle the object (type is LO_NONE), we don't filter.
	 *
	 * To determine if we're in a known object, we need to
	 * look up the whole object stack for a known object at
	 * any level.
	 * 
	 * Also adding P_JAVA_APPLET to the list - support for applet redirect to obj tag
	 */
	if (top_state->object_stack != NULL &&
	    tag->type != P_PARAM && tag->type != P_OBJECT
#ifdef OJI
	    && tag->type != P_JAVA_APPLET
#endif
	    )
	{
		/* Check for a known object anywhere on the stack */
		lo_ObjectStack* top = top_state->object_stack;
		while (top != NULL)
		{
			if (top->object != NULL &&
				top->object->lo_element.lo_plugin.type != LO_NONE &&
				top->object->lo_element.lo_plugin.type != LO_UNKNOWN)
			{
				/* The object was known, so filter out its contents */
				return FALSE;
			}
			top = top->next;
		}
	}
	
#ifdef JAVA
	/*
	 * If the user has disabled Java, act like we don't understand
	 * the APPLET tag.
	 *
	 * Warren sez: I factored the LJ_GetJavaEnabled() test into the two 
	 * if statements here because it was pretty inefficient the way it
	 * was written -- LJ_GetJavaEnabled() at one point was going all the
	 * way out to the prefs for this info, and this routine is called
	 * for each line.
	 */
	/*
	 * APPLETs can appear anywhere.  Check for them and
	 * maintain proper in_applet state.
	 */
	if (tag->type == P_JAVA_APPLET && LJ_GetJavaEnabled())
	{
		if (tag->is_end == FALSE)
		{
			top_state->in_applet = TRUE;
#ifndef M12N                    /* XXXM12N Fix me, this has gone away. Sets
                                   cx->colorSpace->install_colormap_forbidden*/
			IL_UseDefaultColormapThisPage(context);
#endif /* M12N */
		}
		else
		{
			top_state->in_applet = FALSE;
		}
		return(TRUE);
	}
	
	/*
	 * Inside applet ignore everything except param and
	 * closing applets.
	 */
	if (top_state->in_applet != FALSE && LJ_GetJavaEnabled())
	{
		if (tag->type != P_PARAM)
		{
			return(FALSE);
		}
		return(TRUE);
	}
#endif /* JAVA */

	return(TRUE);
}

/* returns the index to the first P in the stack starting
 * from the index passed in
 */
static int32
lo_find_P_tag_in_style_stack(StyleAndTagStack *style_stack, int32 index)
{
	TagStruct *tag;

	if(!style_stack)
		return -1;

	while((tag = STYLESTACK_GetTagByIndex(style_stack, index)) != NULL)
	{
		if(tag && !strcasecomp(tag->name, "P"))
			return index;
		index++;
	}

	return -1;
}

static void
lo_pop_paragraph_from_style_stack(lo_DocState **state, 
				  MWContext *context, 
				  XP_Bool is_end_tag)
{
	int32 index=0;

	/* the paragraph can be multiple levels down into
	 * the stylestack. 
	 *
	 * @@@ should I only pop the paragraph or should I pop every
	 * thing above it exept the top one?
	 */
	if(!(*state)->top_state || !(*state)->top_state->style_stack)
		return;

	index = lo_find_P_tag_in_style_stack((*state)->top_state->style_stack, 0);

	if(index < 0)
	{
		/* we must have mistakenly popped the paragraph during this
	 	 * end tag since the paragraph was at the top of the stack
		 * when this end tag was encountered
		 *
  		 * so we need to pop the top tag
		 */
		if(is_end_tag)
			LO_PopStyleTagByIndex(context, state, P_PARAGRAPH, 0);
		return;
	}

	if(index == 0)
	{
		/* we can only pop the top P tag if a close tag is
	 	 * causing us to pop. Pop the next previous P tag
		 */
		if(!is_end_tag)
		{
			index = lo_find_P_tag_in_style_stack((*state)->top_state->style_stack, 1);

			if(index < 0)
				return;
		}

#ifndef M12N                    /* XXXM12N Fix me, this has gone away. Sets
                                   cx->colorSpace->install_colormap_forbidden*/
		IL_UseDefaultColormapThisPage(context);
#endif /* M12N */
	}
	
	LO_PopStyleTagByIndex(context, state, P_PARAGRAPH, index);
}

void
lo_ProcessParagraphElement(MWContext *context,
						   lo_DocState **state,
						   LO_ParagraphStruct *paragraph,
						   XP_Bool in_relayout)
{
  if (!paragraph->is_end)
    {
      if (paragraph->alignment_set)
		lo_PushAlignment(*state, P_PARAGRAPH, paragraph->alignment);
	  
      (*state)->in_paragraph = TRUE;
    }
  else
    {
	  if (paragraph->implicit_end)
		lo_pop_paragraph_from_style_stack(state, context, !paragraph->implicit_end);

      (*state)->in_paragraph = FALSE;
	  
      if (((*state)->align_stack != NULL)&&
		  ((*state)->align_stack->type == P_PARAGRAPH))
		{
		  lo_AlignStack *aptr;
		  
		  lo_SetLineBreakState(context, *state, LO_LINEFEED_BREAK_SOFT,
							   LO_CLEAR_NONE, 1, !in_relayout);
		  
		  aptr = lo_PopAlignment(*state);
		  if (aptr != NULL)
			{
			  XP_DELETE(aptr);
			}
		}
    }
}

static void
lo_OpenParagraph(MWContext *context, lo_DocState **state, PA_Tag *tag, intn blank_lines)
{
	PA_Block buff;
	char *str;
	LO_ParagraphStruct *paragraph;

	paragraph = (LO_ParagraphStruct*)lo_NewElement(context, *state, LO_PARAGRAPH, NULL, 0);

	XP_ASSERT(paragraph);
	if (!paragraph) return;

	paragraph->lo_any.type = LO_PARAGRAPH;

	/* 
	 * Need to initialize these because implicit_end gets
	 * checked in lo_ProcessParagraphElement and pops the
	 * style stack for an open paragraph tag which causes
	 * a crash in lo_LayoutTags 
	 */
	paragraph->is_end = FALSE;
	paragraph->implicit_end = FALSE;
	paragraph->alignment_set = FALSE;
	paragraph->blank_lines = blank_lines;

	buff = lo_FetchParamValue(context, tag, PARAM_ALIGN);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		paragraph->alignment_set = TRUE;
		paragraph->alignment =
		  (int32)lo_EvalDivisionAlignParam(str);
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}

	lo_SetLineBreakState(context, *state, FALSE,
						 LO_LINEFEED_BREAK_PARAGRAPH, paragraph->blank_lines, FALSE);

	paragraph->lo_any.x = (*state)->x;
	paragraph->lo_any.y = (*state)->y;
	paragraph->lo_any.x_offset = 0;
	paragraph->lo_any.y_offset = 0;
	paragraph->lo_any.width = 0;
	paragraph->lo_any.height = 0;
	paragraph->lo_any.line_height = 0;
	paragraph->lo_any.ele_id = (*state)->top_state->element_id++; /*NEXT_ELEMENT doesn't work with *state */
	
	lo_AppendToLineList(context, *state, (LO_Element*)paragraph, 0);

	lo_ProcessParagraphElement(context, state, paragraph, FALSE);
}

void
lo_CloseParagraph(MWContext *context, lo_DocState **state, PA_Tag *tag, intn blank_lines)
{
	/*
	 * If we are want to close a paragraph we are out of
	 * the HEAD section of the HTML and into the BODY
	 *
	 * With the exception of TITLE which can be in head, but tries to
	 * close paragraphs in case it was erroneously placed within body.
	 * This is taken care of in lo_process_title_tag()
	 */
	(*state)->top_state->in_head = FALSE;
	(*state)->top_state->in_body = TRUE;

	if ((*state)->in_paragraph != FALSE)
	{
	  LO_ParagraphStruct *paragraph;

	  paragraph = (LO_ParagraphStruct*)lo_NewElement(context, *state, LO_PARAGRAPH, NULL, 0);
	  XP_ASSERT(paragraph);
	  if (!paragraph) return;

	  paragraph->lo_any.type = LO_PARAGRAPH;	  
	  /* don't pop the style stack if this is an end paragraph tag 
	   * since we already did it 
	   */
	  paragraph->implicit_end = (tag && !(tag->type == P_PARAGRAPH && tag->is_end));

	  paragraph->blank_lines = blank_lines;
	  paragraph->is_end = TRUE;
	  lo_SetLineBreakState(context, *state, FALSE,
						   LO_LINEFEED_BREAK_PARAGRAPH, paragraph->blank_lines, FALSE);

	  paragraph->lo_any.x = (*state)->x;
	  paragraph->lo_any.y = (*state)->y;
	  paragraph->lo_any.x_offset = 0;
	  paragraph->lo_any.y_offset = 0;
	  paragraph->lo_any.width = 0;
	  paragraph->lo_any.height = 0;
	  paragraph->lo_any.line_height = 0;
	  paragraph->lo_any.ele_id = (*state)->top_state->element_id++; /*NEXT_ELEMENT doesn't work with *state */

	  lo_AppendToLineList(context, *state, (LO_Element*)paragraph, 0);

	  lo_ProcessParagraphElement(context, state, paragraph, FALSE);
	}
}


static void
lo_OpenHeader(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	PA_Block buff;
	char *str;

	buff = lo_FetchParamValue(context, tag, PARAM_ALIGN);
	if (buff != NULL)
	{
		int32 alignment;

		PA_LOCK(str, char *, buff);
		alignment = (int32)lo_EvalDivisionAlignParam(str);
		PA_UNLOCK(buff);
		PA_FREE(buff);

		lo_PushAlignment(state, tag->type, alignment);
	}
}


static void
lo_CloseHeader(MWContext *context, lo_DocState *state)
{
	Bool aligned_header;

	/*
	 * Check the top of the alignment stack to see if we are closing
	 * a header that explicitly set an alignment.
	 */
	aligned_header = FALSE;
	if (state->align_stack != NULL)
	{
		intn type;

		type = state->align_stack->type;
		if ((type == P_HEADER_1)||(type == P_HEADER_2)||
		    (type == P_HEADER_3)||(type == P_HEADER_4)||
		    (type == P_HEADER_5)||(type == P_HEADER_6))
		{
			aligned_header = TRUE;
		}
	}

	lo_SetLineBreakState ( context, state, FALSE, LO_LINEFEED_BREAK_HARD, 1, FALSE);

	if (aligned_header != FALSE)
	{
		lo_AlignStack *aptr;

		aptr = lo_PopAlignment(state);
		if (aptr != NULL)
		{
			XP_DELETE(aptr);
		}
	}
	
	/*
	 * Now that we are on the line after the header, we
	 * don't want the next blank line to be the height of the probably
	 * large header, so reset it to the default here.
	 */
	state->line_height = state->default_line_height;
}

static void
lo_process_text_tag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	char *tptr;
	char *tptr2;

	/*
	 * This is normal text, formatted or preformatted.
	 */
	if ((state->text_divert == P_UNKNOWN)||(state->text_divert == P_TEXT))
	{
		/*
		 * If you don't have a current text element
		 * to add this text to, make one here.
		 */
		if (state->cur_ele_type != LO_TEXT)
		{
			lo_FreshText(state);
			state->cur_ele_type = LO_TEXT;
		}

		PA_LOCK (tptr, char *, tag->data);

		/*
		 * If we are processing non-head-data text, and it is
		 * not just white space, then we are out of
		 * the HEAD section of the HTML.
		 *
		 * If the text is inside an unknown element in the HEAD
		 * we can assume this is content we should ignore.
		 */
		if (state->top_state->in_head != FALSE)
		{
			char *cptr;
			Bool real_text;

			real_text = FALSE;
			cptr = tptr;
			while (*cptr != '\0')
			{
				if (!XP_IS_SPACE(*cptr))
				{
					real_text = TRUE;
					break;
				}
				cptr++;
			}
			if (real_text != FALSE)
			{
				if (state->top_state->unknown_head_tag != NULL)
				{
#ifdef DEBUG
XP_TRACE(("Toss out content in unknown HEAD (%s)\n", tptr));
#endif /* DEBUG */
					return;
				}
				/*
				 * If the content isnot ignore, we have left
				 * the HEAD and are in the BODY.
				 */
				state->top_state->in_head = FALSE;
				state->top_state->in_body = TRUE;
			}
		}

		if (state->preformatted != PRE_TEXT_NO)
		{
			lo_PreformatedText(context, state, tptr);
		}
		else
		{
			lo_FormatText(context, state, tptr);
		}
		PA_UNLOCK(tag->data);
	}
	/*
	 * These tags just capture the text flow as is.
	 */
	else if ((state->text_divert == P_TITLE)||
		(state->text_divert == P_TEXTAREA)||
		(state->text_divert == P_OPTION)||
		(state->text_divert == P_CERTIFICATE)||
		(state->text_divert == P_SCRIPT) ||
		(state->text_divert == P_STYLE))
	{
		int32 copy_len;

		if ((state->line_buf_len + tag->data_len + 1) >
			state->line_buf_size)
		{
			int32 new_size;

			new_size = state->line_buf_size +
				tag->data_len + LINE_BUF_INC;
#ifdef XP_WIN16
			if (new_size > SIZE_LIMIT)
			{
				new_size = SIZE_LIMIT;
			}
#endif /* XP_WIN16 */
			state->line_buf = PA_REALLOC(state->line_buf, new_size);
			if (state->line_buf == NULL)
			{
				state->top_state->out_of_memory = TRUE;
				return;
			}
			state->line_buf_size = new_size;
		}

		copy_len = tag->data_len;
#ifdef XP_WIN16
		if ((state->line_buf_len + copy_len) >
			(state->line_buf_size - 1))
		{
			copy_len = state->line_buf_size - 1 -
				state->line_buf_len;
		}
#endif /* XP_WIN16 */

		PA_LOCK(tptr, char *, tag->data);
		PA_LOCK(tptr2, char *, state->line_buf);
		XP_BCOPY(tptr, (char *)(tptr2 + state->line_buf_len),
			(copy_len + 1));
		state->line_buf_len += copy_len;
#ifdef XP_WIN16
		tptr2[state->line_buf_len] = '\0';
#endif /* XP_WIN16 */
		PA_UNLOCK(state->line_buf);
		PA_UNLOCK(tag->data);
	}
	else /* Just ignore this text, it shouldn't be here */
	{
	}
}


static void
lo_process_title_tag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	Bool tmp_in_head;
	Bool tmp_in_body;

	/*
	 * Don't let closing a paragraph before a title change the
	 * in_head or in_body state.
	 */
	tmp_in_head = state->top_state->in_head;
	tmp_in_body = state->top_state->in_body;
	if (state->in_paragraph != FALSE)
	{
		lo_CloseParagraph(context, &state, tag, 2);
	}
	state->top_state->in_head = tmp_in_head;
	state->top_state->in_body = tmp_in_body;

	if (tag->is_end == FALSE)
	{
		/*
		 * Flush the line buffer so we can
		 * start storing the title text there.
		 */
		lo_FlushTextBlock(context, state);

		state->line_buf_len = 0;
		state->text_divert = tag->type;
	}
	else
	{
	    /*
	     * Only if we captured some title text
	     */
	    if (state->line_buf_len != 0)
	    {
		int32 len;
		char *tptr2;

		PA_LOCK(tptr2, char *, state->line_buf);
		/*
		 * Title text is cleaned up before passing
		 * to the front-end.
		 */
		len = lo_CleanTextWhitespace(tptr2, state->line_buf_len);
		/*
		 * Subdocs can't have titles.
		 */
		if ((state->is_a_subdoc == SUBDOC_NOT)&&
			(state->top_state->have_title == FALSE))
		{
			int16 charset;

			charset = state->font_stack->text_attr->charset;
			tptr2 = FE_TranslateISOText(context, charset, tptr2);
#ifdef MAXTITLESIZE
			if ((XP_STRLEN(tptr2) > MAXTITLESIZE)&&
			    (MAXTITLESIZE >= 0))
			{
				tptr2[MAXTITLESIZE] = '\0';
			}
#endif
			FE_SetDocTitle(context, tptr2);
			state->top_state->have_title = TRUE;
		}
		PA_UNLOCK(state->line_buf);
	    }
	    state->line_buf_len = 0;
	    state->text_divert = P_UNKNOWN;
	}
}

void
lo_ProcessCenterElement(MWContext *context, lo_DocState **state, LO_CenterStruct *center,
						XP_Bool in_relayout)
{
	if ((*state)->in_paragraph != FALSE)
	{
		lo_CloseParagraph(context, state, NULL, 2);
	}

	lo_SetLineBreakState(context, *state, LO_LINEFEED_BREAK_SOFT,
						 LO_CLEAR_NONE, 1, !in_relayout);

	if (center->is_end == FALSE)
	{
		lo_PushAlignment(*state, P_CENTER, LO_ALIGN_CENTER);
	}
	else
	{
		lo_AlignStack *aptr;
		aptr = lo_PopAlignment(*state);
		if (aptr != NULL)
		{
			XP_DELETE(aptr);
		}
	}
}

static void
lo_process_center_tag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
  LO_CenterStruct *center;

  center = (LO_CenterStruct*)lo_NewElement(context, state, LO_CENTER, NULL, 0);

  center->lo_any.type = LO_CENTER;
  center->is_end = tag->is_end;

  center->lo_any.ele_id = NEXT_ELEMENT;
  
  center->lo_any.x = state->x;
  center->lo_any.y = state->y;
  center->lo_any.x_offset = 0;
  center->lo_any.y_offset = 0;
  center->lo_any.width = 0;
  center->lo_any.height = 0;
  center->lo_any.line_height = 0;

  lo_AppendToLineList(context, state, (LO_Element*)center, 0);

  lo_ProcessCenterElement(context, &state, center, FALSE);
}

void
lo_ProcessMulticolumnElement(MWContext *context,
			     lo_DocState **state,
			     LO_MulticolumnStruct *multicolumn)
{
	/*
	 * The start of a multicol infers the end of
	 * the HEAD section of the HTML and starts the BODY
	 */
	(*state)->top_state->in_head = FALSE;
	(*state)->top_state->in_body = TRUE;

	

	if (multicolumn->is_end == FALSE)
	{
		lo_AppendMultiColToLineList( context, (*state), multicolumn);
		lo_BeginMulticolumn(context, (*state), multicolumn->tag, multicolumn);
	}
	else
	{
		if ((*state)->current_multicol != NULL)
		{			
			lo_EndMulticolumn(context, (*state), multicolumn->tag,
				(*state)->current_multicol, FALSE);
		}
		lo_AppendMultiColToLineList( context, (*state), multicolumn);
	}
}

void
lo_ProcessDescTitleElement(MWContext *context,
						   lo_DocState *state,
						   LO_DescTitleStruct *title,
						   XP_Bool in_relayout)
{
  /*
   * Undent to the left for a title if necessary.
   */
  if ((state->list_stack->level != 0)&&
	  (state->list_stack->type == P_DESC_LIST)&&
	  (state->list_stack->value > 1))
	{
	  state->list_stack->old_left_margin -=
		LIST_MARGIN_INC;
	  state->list_stack->value = 1;
	}
  
  if (state->linefeed_state >= 1)
	{
	  lo_FindLineMargins(context, state, TRUE);
	}
  else
	{
	  lo_SetLineBreakState(context, state, LO_LINEFEED_BREAK_SOFT,
						   LO_CLEAR_NONE, 1, !in_relayout);
	}
  state->x = state->left_margin;
}

void
lo_ProcessDescTextElement(MWContext *context,
						  lo_DocState *state,
						  LO_DescTextStruct *text,
						  XP_Bool in_relayout)
{
  /*
   * Indent from the title if necessary.
   */
  if ((state->list_stack->type == P_DESC_LIST)&&
	  (state->list_stack->value == 1))
	{
	  state->list_stack->old_left_margin +=
		LIST_MARGIN_INC;
	  state->list_stack->value = 2;
	}
  
  if (state->list_stack->compact == FALSE)
	{
	  if (state->linefeed_state >= 1)
		{
		  lo_FindLineMargins(context, state, TRUE);
		}
	  else
		{
		  lo_SetLineBreakState(context, state, LO_LINEFEED_BREAK_SOFT,
							   LO_CLEAR_NONE, 1, !in_relayout);
		}
	}
  else
	{
	  if (state->x >=
		  state->list_stack->old_left_margin)
		{
		  if (state->linefeed_state >= 1)
			{
			  lo_FindLineMargins(context,
								 state, TRUE);
			}
		  else
			{
			  lo_SetLineBreakState(context, state, LO_LINEFEED_BREAK_SOFT,
								   LO_CLEAR_NONE, 1, !in_relayout);
			}
		}
	  else
		{
		  lo_FindLineMargins(context,
							 state, TRUE);
		  state->x = state->left_margin;
		}
	}
  state->x = state->left_margin;
  
  /*
   * Bug compatibility for <dd> outside a <dl>
   */
  if (state->list_stack->type != P_DESC_LIST)
	{
	  state->x += LIST_MARGIN_INC;
	}
}

void
lo_ProcessBlockQuoteElement(MWContext *context,
							lo_DocState *state,
							LO_BlockQuoteStruct *quote,
							XP_Bool in_relayout)
{
  if (quote->is_end == FALSE)
	{
	  lo_SetLineBreakState(context, state, LO_LINEFEED_BREAK_SOFT,
						   LO_CLEAR_NONE, 2, !in_relayout);
	  
	  /* THIS BREAKS FLOATING IMAGES IN BLOCKQUOTES
		 state->list_stack->old_left_margin =
		 state->left_margin;
		 state->list_stack->old_right_margin =
		 state->right_margin;
	  */
	  lo_PushList(state, quote->tag, quote->quote_type);
	  if (quote->quote_type == QUOTE_MQUOTE)
		{
		  state->left_margin += MQUOTE_MARGIN_INC;
		  state->right_margin -= MQUOTE_MARGIN_INC;
		}
	  else if ((quote->quote_type == QUOTE_JWZ)||
			   (quote->quote_type == QUOTE_CITE))
		{
		  state->left_margin += MQUOTE_MARGIN_INC;
		}
	  else
		{
		  state->left_margin += LIST_MARGIN_INC;
		  state->right_margin -= LIST_MARGIN_INC;
		}
	  state->x = state->left_margin;
	  state->list_stack->old_left_margin =
		state->left_margin;
	  state->list_stack->old_right_margin =
		state->right_margin;
	  
	  if ((quote->quote_type == QUOTE_JWZ)||
		  (quote->quote_type == QUOTE_CITE))
		{
		  lo_PlaceQuoteMarker(context, state,
							  state->list_stack);
		}
	}
  else
	{
	  lo_ListStack *lptr;
	  int8 quote_type;
	  Bool mquote = FALSE;
	  int32 mquote_line_num = 0;
	  int32 mquote_x = 0;
	  
	  quote_type = QUOTE_NONE;
	  lptr = lo_PopList(state, quote->tag);
	  if (lptr != NULL)
		{
		  quote_type = lptr->quote_type;
		  if (lptr->quote_type == QUOTE_MQUOTE) {
			mquote = TRUE;
		  }
		  mquote_line_num = lptr->mquote_line_num;
		  mquote_x = lptr->mquote_x;
		  XP_DELETE(lptr);
		}
	  state->left_margin =
		state->list_stack->old_left_margin;
	  state->right_margin =
		state->list_stack->old_right_margin;
	  
	  /*
	   * Reset the margins properly in case
	   * there is a right-aligned image here.
	   */
	  lo_FindLineMargins(context, state, TRUE);
	  if (state->linefeed_state >= 2)
		{
		  lo_FindLineMargins(context, state, TRUE);
		  /*
		   * If we just popped a citation
		   * blockquote, there should be a
		   * bullet at the end of the
		   * current line list we
		   * need to remove.
		   */
		  if ((quote_type == QUOTE_JWZ)||
			  (quote_type == QUOTE_CITE))
			{
			  LO_Element *line_ptr;
			  
			  line_ptr = state->line_list;
			  while ((line_ptr != NULL)&&
					 (line_ptr->lo_any.next != NULL))
				{
				  line_ptr = line_ptr->lo_any.next;
				}
			  if (line_ptr != NULL)
				{
				  if (line_ptr->lo_any.prev != NULL)
					{
					  line_ptr->lo_any.prev->lo_any.next = NULL;
					}
				  if (line_ptr == state->line_list)
					{
					  state->line_list = NULL;
					}
				  lo_FreeElement(context,
								 line_ptr,
								 FALSE);
				}
			}
		}
	  else
		{
		  lo_SetLineBreakState(context, state, LO_LINEFEED_BREAK_SOFT,
							   LO_CLEAR_NONE, 2, !in_relayout);
		}
	  
	  state->x = state->left_margin;
	  
	  /* Go back and add bullets to all lines between here and the 
		 beginning of the mailing quotation. */
	  if (mquote)
		{
		  lo_add_leading_bullets(context,state,
								 mquote_line_num - 1,
								 state->line_num - state->linefeed_state - 1,
								 mquote_x);
		}
	}
}

static void
lo_process_paragraph_tag(MWContext *context, lo_DocState *state, PA_Tag *tag,
			 StyleStruct *style_struct)
{
	if (tag->is_end == FALSE)
	{
		intn blank_lines=2;
		char *property;

		if (state->in_paragraph != FALSE)
		{
			lo_CloseParagraph(context, &state, tag, 2);
		}

		if(style_struct 
		   && (property = STYLESTRUCT_GetString(style_struct, 
							TOPMARGIN_STYLE)) != NULL)
		{
			/* there is a top margin for this paragraph,
			 * so don't add two CR's just one 
			 */
			blank_lines = 1;
			XP_FREE(property);
		}
		
		lo_OpenParagraph(context, &state, tag, blank_lines);
	}
	else
	{
		char *property;
		intn blank_lines=2;

		if(style_struct
		   && (property = STYLESTRUCT_GetString(style_struct,
							BOTTOMMARGIN_STYLE)) != NULL)
		{
			/* there is a bottom margin for this paragraph,
			 * so don't add two CR's just one 
			 */
			blank_lines = 1;
			XP_FREE(property);
		}
		lo_CloseParagraph(context, &state, tag, blank_lines);
	}
}

static void
lo_process_multicolumn_tag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
  LO_MulticolumnStruct *multicolumn;

  multicolumn = (LO_MulticolumnStruct*)lo_NewElement(context, state, LO_MULTICOLUMN, NULL, 0);
  XP_ASSERT(multicolumn);
  if (!multicolumn) return;

  multicolumn->lo_any.type = LO_MULTICOLUMN;
  /* multicolumn->lo_any.ele_id = NEXT_ELEMENT; */
  multicolumn->is_end = tag->is_end;
  multicolumn->tag = PA_CloneMDLTag(tag);
  multicolumn->multicol = NULL;

  /*
  multicolumn->lo_any.x = state->x;
  multicolumn->lo_any.y = state->y;
  */
  multicolumn->lo_any.x_offset = 0;
  multicolumn->lo_any.y_offset = 0;
  multicolumn->lo_any.width = 0;
  multicolumn->lo_any.height = 0;
  multicolumn->lo_any.line_height = 0;

  /* lo_AppendToLineList(context, state, (LO_Element*)multicolumn, 0); */
  
  lo_ProcessMulticolumnElement(context, &state, multicolumn);
}

static void
lo_process_header_tag(MWContext *context, lo_DocState *state, PA_Tag *tag, int text_attr_size)
{
	if (state->in_paragraph != FALSE)
	{
		lo_CloseParagraph(context, &state, tag, 2);
	}
	if (tag->is_end == FALSE)
	{
		LO_TextAttr *old_attr;
		LO_TextAttr *attr;
		LO_TextAttr tmp_attr;

		/* lo_SetSoftLineBreakState(context, state, FALSE, 2); */
		lo_SetLineBreakState ( context, state, FALSE, LO_LINEFEED_BREAK_HARD, 2, FALSE);
		lo_OpenHeader(context, state, tag);

		old_attr = state->font_stack->text_attr;
		lo_CopyTextAttr(old_attr, &tmp_attr);
		tmp_attr.fontmask |= LO_FONT_BOLD;
		tmp_attr.fontmask &= (~(LO_FONT_FIXED|LO_FONT_ITALIC));
		tmp_attr.size = text_attr_size;
		attr = lo_FetchTextAttr(state, &tmp_attr);

		lo_PushFont(state, tag->type, attr);
	}
	else
	{
		LO_TextAttr *attr;

		attr = lo_PopFont(state, tag->type);

		lo_CloseHeader(context, state);
		/* lo_SetSoftLineBreakState(context, state, FALSE, 2); */
		lo_SetLineBreakState ( context, state, FALSE, LO_LINEFEED_BREAK_HARD, 2, FALSE);
	}
}

static void
lo_process_span_tag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
  LO_SpanStruct *span;
  PA_Block buff;
  lo_DocLists *doc_lists;
  
  doc_lists = lo_GetCurrentDocLists(state);

  span = (LO_SpanStruct*)lo_NewElement(context, state, LO_SPAN, NULL, 0);
  XP_ASSERT(span);
  if (!span) return;

  span->lo_any.type = LO_SPAN;
  span->lo_any.ele_id = NEXT_ELEMENT;
  span->is_end = tag->is_end;

  span->lo_any.x = state->x;
  span->lo_any.y = state->y;
  span->lo_any.x_offset = 0;
  span->lo_any.y_offset = 0;
  span->lo_any.width = 0;
  span->lo_any.height = 0;
  span->lo_any.line_height = 0;

#ifdef DOM
    span->name_rec = NULL;
	if (tag->is_end == FALSE)
	{		
		/* get the span's ID. */
		buff = lo_FetchParamValue(context, tag, PARAM_ID);
		if (buff != NULL)
		{
		  state->in_span = TRUE;
		  state->current_span = buff;
		  if (lo_SetNamedSpan(state, buff))
			{
			  lo_BindNamedSpanToElement(state, buff, NULL);
			  lo_ReflectSpan(context, state, tag,
							 doc_lists->span_list,
							 lo_CurrentLayerId(state));
			  span->name_rec = doc_lists->span_list;
			}

		  PA_UNLOCK(buff);
		}
		lo_AppendToLineList(context, state, (LO_Element*)span, 0);
		state->in_span = TRUE;
	}
	else
	{
		state->in_span = FALSE;
		lo_AppendToLineList(context, state, (LO_Element*)span, 0);
	}
#endif
}

static void
lo_process_div_tag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
  LO_DivStruct *div;

  div = (LO_DivStruct*)lo_NewElement(context, state, LO_DIV, NULL, 0);
  XP_ASSERT(div);
  if (!div) return;

  div->lo_any.type = LO_DIV;
  div->lo_any.ele_id = NEXT_ELEMENT;
  div->is_end = tag->is_end;

  div->lo_any.x = state->x;
  div->lo_any.y = state->y;
  div->lo_any.x_offset = 0;
  div->lo_any.y_offset = 0;
  div->lo_any.width = 0;
  div->lo_any.height = 0;
  div->lo_any.line_height = 0;

  lo_AppendToLineList(context, state, (LO_Element*)div, 0);
}

PRIVATE Bool lo_do_underline = TRUE;

#ifdef XP_MAC
PRIVATE
#endif
int PR_CALLBACK
lo_underline_pref_callback(const char *pref_name, void *closure)
{
    PREF_GetBoolPref("browser.underline_anchors", &lo_do_underline);

    return PREF_NOERROR;
}

PRIVATE
Bool 
lo_underline_anchors()
{
    static Bool first_time = TRUE;

    if(first_time)
    {
        first_time = FALSE;
        PREF_GetBoolPref("browser.underline_anchors", &lo_do_underline);
        PREF_RegisterCallback("browser.underline_anchors", lo_underline_pref_callback, NULL);
    }

    return (lo_do_underline);
}

/* if the color is currently equal to the default visited
 * or unvisited link color, change the color very slightly
 * to make it different.  That way we can tell the difference
 * between the default colors and the colors set by style sheets
 */
static void
lo_make_link_color_different_than_default(lo_DocState *state, LO_TextAttr *tmp_attr)
{
	if(    (tmp_attr->fg.red   == STATE_UNVISITED_ANCHOR_RED(state)
   			&& tmp_attr->fg.green == STATE_UNVISITED_ANCHOR_GREEN(state)
    		&& tmp_attr->fg.blue  == STATE_UNVISITED_ANCHOR_BLUE(state))
		|| (tmp_attr->fg.red   == STATE_VISITED_ANCHOR_RED(state)
   			&& tmp_attr->fg.green == STATE_VISITED_ANCHOR_GREEN(state)
    		&& tmp_attr->fg.blue  == STATE_VISITED_ANCHOR_BLUE(state)))
	{
		/* change the color slightly */
		if(tmp_attr->fg.red < 255)
			tmp_attr->fg.red++;
		else
			tmp_attr->fg.red--;
	}
}

static void
lo_process_anchor_tag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	LO_TextAttr tmp_attr;
    lo_DocLists *doc_lists;
    
    doc_lists = lo_GetCurrentDocLists(state);

	/*
	 * Opening a new anchor
	 */
	if (tag->is_end == FALSE)
	{
		LO_TextAttr *old_attr;
		LO_TextAttr *attr;
		char *url;
		char *full_url;
		PA_Block buff;
		Bool is_new;
		PA_Block anchor;

		/*
		 * Get the NAME param for the name list
		 */
		buff = lo_FetchParamValue(context, tag, PARAM_NAME);
		if (buff != NULL)
		{
		    char *name;

		    /*
		     * If we are looking to start at a name,
		     * fetch the NAME and compare.
		     */
		    if (state->display_blocking_element_id == -1)
		    {
			PA_LOCK(name, char *, buff);
			if (XP_STRCMP(name,
			    (char *)(state->top_state->name_target + 1))
				== 0)
			{
			    XP_FREE(state->top_state->name_target);
			    state->top_state->name_target = NULL;
			    state->display_blocking_element_id =
				state->top_state->element_id;
			}
			PA_UNLOCK(buff);
		    }

            /* Add the named anchor to the name_list. */			
			
			/*
			if (state->current_named_anchor != NULL)
				PA_FREE(state->current_named_anchor);
			*/

		    state->current_named_anchor = buff;
			if (lo_SetNamedAnchor(state, buff))
                {
				lo_BindNamedAnchorToElement(state, buff, NULL);
			    lo_ReflectNamedAnchor(context, state, tag,
                                      doc_lists->name_list,
                                      lo_CurrentLayerId(state));
                }
		}

		is_new = TRUE;
		state->current_anchor = NULL;
		/*
		 * Fetch the HREF, and turn it into an
		 * absolute URL.
		 */
		anchor = lo_FetchParamValue(context, tag, PARAM_HREF);
		if (anchor != NULL)
		{
		    char *target;
		    PA_Block targ_buff;

		    PA_LOCK(url, char *, anchor);
		    if (url != NULL)
		    {
			int32 len;

			len = lo_StripTextWhitespace(url, XP_STRLEN(url));
		    }
		    url = NET_MakeAbsoluteURL(state->top_state->base_url, url);
		    if (url == NULL)
		    {
			buff = NULL;
		    }
		    else
		    {
			buff = PA_ALLOC(XP_STRLEN(url)+1);
			if (buff != NULL)
			{
			    PA_LOCK(full_url, char *, buff);
			    XP_STRCPY(full_url, url);
			    PA_UNLOCK(buff);

			    if (GH_CheckGlobalHistory(url) != -1)
			    {
				is_new = FALSE;
			    }
			}
			else
			{
#ifdef DEBUG
				assert (state);
#endif
				state->top_state->out_of_memory = TRUE;
			}
			
			XP_FREE(url);
		    }
		    PA_UNLOCK(anchor);
		    PA_FREE(anchor);
		    anchor = buff;

		    if (anchor != NULL)
		    {
			targ_buff = lo_FetchParamValue(context, tag, PARAM_TARGET);
			if (targ_buff != NULL)
			{
			    int32 len;

			    PA_LOCK(target, char *, targ_buff);
			    len = lo_StripTextWhitespace(target,
					XP_STRLEN(target));
			    if ((*target == '\0')||
				    (lo_IsValidTarget(target) == FALSE))
			    {
				PA_UNLOCK(targ_buff);
				PA_FREE(targ_buff);
				targ_buff = NULL;
			    }
			    else
			    {
				PA_UNLOCK(targ_buff);
			    }
			}

			/*
			 * If there was no target use the default one.
			 * (default provided by BASE tag)
			 */
			if ((targ_buff == NULL)&&
			    (state->top_state->base_target != NULL))
			{
				targ_buff = PA_ALLOC(XP_STRLEN(
					state->top_state->base_target) + 1);
				if (targ_buff != NULL)
				{
					char *targ;

					PA_LOCK(targ, char *, targ_buff);
					XP_STRCPY(targ,
						state->top_state->base_target);
					PA_UNLOCK(targ_buff);
				}
				else
				{
					state->top_state->out_of_memory = TRUE;
				}
			}

			state->current_anchor =
				lo_NewAnchor(state, anchor, targ_buff);
			if (state->current_anchor == NULL)
			{
			    PA_FREE(anchor);
			    if (targ_buff != NULL)
			    {
				PA_FREE(targ_buff);
			    }
			}
		    }

            /* If SUPPRESS attribute is present, suppress visual feedback (dashed rectangle)
               when link is selected */
            buff = lo_FetchParamValue(context, tag, PARAM_SUPPRESS);
            if (buff && !XP_STRCASECMP((char*)buff, "true"))
            {
                state->current_anchor->flags |= ANCHOR_SUPPRESS_FEEDBACK;
            }

			/* Look for PRE subtag and store its value in prevalue */
			buff = lo_FetchParamValue(context, tag, PARAM_PRE);
			if (buff)
			{
				char * val;
				PA_LOCK(val, char *, buff);
				if (val)
				{
					state->current_anchor->prevalue = atof(val);
					XP_FREE(val);
				}
				PA_UNLOCK(buff);
			}
		    /*
		     * Add this url's block to the list
		     * of all allocated urls so we can free
		     * it later.
		     */
		    lo_AddToUrlList(context, state, state->current_anchor);
		    if (state->top_state->out_of_memory != FALSE)
		    {
			return;
		    }
		    lo_ReflectLink(context, state, tag, state->current_anchor,
                           lo_CurrentLayerId(state),
                           doc_lists->url_list_len - 1);
		}

		/*
		 * Change the anchor attribute only for
		 * anchors with hrefs.
		 */
		if (state->current_anchor != NULL)
		{
            StyleStruct *style_struct = NULL;
            char *property=NULL;

			if (state->top_state->style_stack)
				style_struct = STYLESTACK_GetStyleByIndex(state->top_state->style_stack, 0);
			else
				style_struct = NULL;

            old_attr = state->font_stack->text_attr;
			lo_CopyTextAttr(old_attr, &tmp_attr);

			tmp_attr.attrmask = (old_attr->attrmask | LO_ATTR_ANCHOR);
            
            if(style_struct)
                property = STYLESTRUCT_GetString(style_struct, TEXTDECORATION_STYLE);
            if(property)
                XP_FREE(property);  /* don't underline here since SS will do it if neccessary */
            else if(lo_underline_anchors())
                tmp_attr.attrmask = (old_attr->attrmask  | LO_ATTR_UNDERLINE);

			if (is_new != FALSE)
			{
                if(style_struct)
                    property = STYLESTRUCT_GetString(style_struct, LINK_COLOR);
	            if (property)
                {
               		LO_ParseStyleSheetRGB(property, &tmp_attr.fg.red, &tmp_attr.fg.green, &tmp_attr.fg.blue);
                        lo_make_link_color_different_than_default(state, &tmp_attr);
            		XP_FREE(property);
                }
                else
                {
                        tmp_attr.fg.red =   STATE_UNVISITED_ANCHOR_RED(state);
                        tmp_attr.fg.green = STATE_UNVISITED_ANCHOR_GREEN(state);
                        tmp_attr.fg.blue =  STATE_UNVISITED_ANCHOR_BLUE(state);
                }
			}
			else
			{
                if(style_struct)
                    property = STYLESTRUCT_GetString(style_struct, VISITED_COLOR);
	            if (property)
                {
                        LO_ParseStyleSheetRGB(property, &tmp_attr.fg.red, &tmp_attr.fg.green, &tmp_attr.fg.blue);
                        lo_make_link_color_different_than_default(state, &tmp_attr);
                        XP_FREE(property);
                }
                else
                {
                        tmp_attr.fg.red =   STATE_VISITED_ANCHOR_RED(state);
                        tmp_attr.fg.green = STATE_VISITED_ANCHOR_GREEN(state);
                        tmp_attr.fg.blue =  STATE_VISITED_ANCHOR_BLUE(state);
                }
            }
			attr = lo_FetchTextAttr(state, &tmp_attr);
			lo_PushFont(state, tag->type, attr);
		}
	}
	/*
	 * Closing an anchor.  Anchors can't be nested, so close
	 * ALL anchors.
	 */
	else
	{
		lo_PopAllAnchors(state);
		state->current_anchor = NULL;
	}
}


static void
lo_process_caption_tag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	if ((state->current_table != NULL)&&
	    (state->current_table->caption == NULL))
	{
	    lo_TableRec *table;

	    table = state->current_table;
	    if (tag->is_end == FALSE)
	    {
		/*
		 * If there is an unterminated row open,
		 * terminate it now.
		 */
		if ((table->row_ptr != NULL)&&
			(table->row_ptr->row_done == FALSE))
		{
			lo_EndTableRow(context, state, table);
		}
		lo_BeginTableCaption(context, state, table,tag);
	    }
	    else
	    {
		/*
		 * This never happens because the
		 * close caption feeds into the subdoc.
		 */
	    }
	}
	else if ((state->current_table == NULL)&&
		(state->is_a_subdoc == SUBDOC_CAPTION))
	{
	    if (tag->is_end == FALSE)
	    {
	    }
	    else
	    {
		lo_EndTableCaption(context, state);
	    }
	}
	else if ((state->current_table == NULL)&&
		(state->is_a_subdoc == SUBDOC_CELL))
	{
	    if (tag->is_end == FALSE)
	    {
		lo_TopState *top_state;
		lo_DocState *new_state;
		int32 doc_id;

		/*
		 * This can only happen if the close cell
		 * was omitted, AND the close row
		 * after it was omitted.
		 */
		lo_EndTableCell(context, state, FALSE);

		/*
		 * restore state to the new lowest level.
		 */
		doc_id = XP_DOCID(context);
		top_state = lo_FetchTopState(doc_id);
		new_state = lo_TopSubState(top_state);

		/*
		 * Now act just like a new caption has started.
		 */
		if (new_state->current_table != NULL)
		{
		    lo_TableRec *table;

		    table = new_state->current_table;
		    /*
		     * If there is an unterminated row open,
		     * terminate it now.
		     */
		    if ((table->row_ptr != NULL)&&
			(table->row_ptr->row_done == FALSE))
		    {
			lo_EndTableRow(context,new_state,table);
		    }
		    lo_BeginTableCaption(context, new_state,
					table, tag);
		}
	    }
	}
}

static void
lo_process_table_cell_tag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	if ((state->current_table != NULL)&&
	    (state->current_table->row_ptr != NULL)&&
	    (state->current_table->row_ptr->row_done == FALSE))
	{
	    lo_TableRec *table;
	    Bool is_a_header;

	    if (tag->type == P_TABLE_HEADER)
	    {
		is_a_header = TRUE;
	    }
	    else
	    {
		is_a_header = FALSE;
	    }

	    table = state->current_table;
	    if (tag->is_end == FALSE)
	    {
		lo_BeginTableCell(context, state, table, tag, is_a_header);
	    }
	    else
	    {
		/*
		 * This never happens because the
		 * close cell feeds into the subdoc.
		 */
	    }
	}
	else if ((state->current_table != NULL)&&
	    ((state->current_table->row_ptr == NULL)||
	    (state->current_table->row_ptr->row_done != FALSE)))
	{
	    lo_TableRec *table;
	    Bool is_a_header;

	    if (tag->is_end == FALSE)
	    {
		table = state->current_table;
		/*
		 * This is a starting data cell, with no open
		 * row.  Open up the row for them.
		 */
		lo_BeginTableRow(context, state, table, tag);

		if (tag->type == P_TABLE_HEADER)
		{
		    is_a_header = TRUE;
		}
		else
		{
		    is_a_header = FALSE;
		}

		lo_BeginTableCell(context, state, table, tag, is_a_header);
	    }
	}
	else if ((state->current_table == NULL)&&
		(state->is_a_subdoc == SUBDOC_CELL))
	{
	    if (tag->is_end == FALSE)
	    {
		lo_TopState *top_state;
		lo_DocState *new_state;
		int32 doc_id;

		/*
		 * This can only happen if the close cell
		 * was omitted.
		 */
		lo_EndTableCell(context, state, FALSE);

		/*
		 * restore state to the new lowest level.
		 */
		doc_id = XP_DOCID(context);
		top_state = lo_FetchTopState(doc_id);
		new_state = lo_TopSubState(top_state);

		/*
		 * Now act just like a new cell has started
		 * as above.
		 */
		if ((new_state->current_table != NULL)&&
		    (new_state->current_table->row_ptr != NULL)&&
		    (new_state->current_table->row_ptr->row_done == FALSE))
		{
		    lo_TableRec *table;
		    Bool is_a_header;

		    if (tag->type == P_TABLE_HEADER)
		    {
			is_a_header = TRUE;
		    }
		    else
		    {
			is_a_header = FALSE;
		    }

		    table = new_state->current_table;
		    lo_BeginTableCell(context, new_state, table, tag,
				is_a_header);
		}
	    }
	    else
	    {
		lo_EndTableCell(context, state, FALSE);
	    }
	}
}


static void
lo_process_table_row_tag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	if (state->current_table != NULL)
	{
	    lo_TableRec *table;

	    table = state->current_table;
	    if (tag->is_end == FALSE)
	    {
		/*
		 * If there is an unterminated row open,
		 * terminate it now.
		 */
		if ((table->row_ptr != NULL)&&
			(table->row_ptr->row_done == FALSE))
		{
			lo_EndTableRow(context, state, table);
		}
		lo_BeginTableRow(context, state, table, tag);
	    }
	    else
	    {
		if ((table->row_ptr != NULL)&&
			(table->row_ptr->row_done == FALSE))
		{
			lo_EndTableRow(context, state, table);
		}
	    }
	}
	else if ((state->current_table == NULL)&&
		(state->is_a_subdoc == SUBDOC_CELL))
	{
	    if (tag->is_end == FALSE)
	    {
		lo_TopState *top_state;
		lo_DocState *new_state;
		int32 doc_id;

		/*
		 * This can only happen if the close cell
		 * was omitted, AND the close row
		 * after it was omitted.
		 */
		lo_EndTableCell(context, state, FALSE);

		/*
		 * restore state to the new lowest level.
		 */
		doc_id = XP_DOCID(context);
		top_state = lo_FetchTopState(doc_id);
		new_state = lo_TopSubState(top_state);

		/*
		 * Now act just like a new row has started.
		 */
		if (new_state->current_table != NULL)
		{
		    lo_TableRec *table;

		    table = new_state->current_table;
		    /*
		     * If there is an unterminated row open,
		     * terminate it now.
		     */
		    if ((table->row_ptr != NULL)&&
			(table->row_ptr->row_done == FALSE))
		    {
			lo_EndTableRow(context, new_state, table);
		    }
		    lo_BeginTableRow(context, new_state, table, tag);
		}
	    }
	    else
	    {
		/*
		 * This can only happen if the close cell
		 * was omitted.
		 */
		lo_EndTableCell(context, state, FALSE);

		/*
		 * We should really go on to repeat all the code
		 * here to process the end row tag, but since
		 * we know our layout can recover fine without
		 * it, we don't bother.
		 */
	    }
	}
	else if ((state->current_table == NULL)&&
		(state->is_a_subdoc == SUBDOC_CAPTION))
	{
	    if (tag->is_end == FALSE)
	    {
		lo_TopState *top_state;
		lo_DocState *new_state;
		int32 doc_id;

		/*
		 * This can only happen if the close caption
		 * was omitted.
		 */
		lo_EndTableCaption(context, state);

		/*
		 * restore state to the new lowest level.
		 */
		doc_id = XP_DOCID(context);
		top_state = lo_FetchTopState(doc_id);
		new_state = lo_TopSubState(top_state);

		/*
		 * Now act just like a new row has started.
		 */
		if (new_state->current_table != NULL)
		{
		    lo_TableRec *table;

		    table = new_state->current_table;
		    /*
		     * If there is an unterminated row open,
		     * terminate it now.
		     */
		    if ((table->row_ptr != NULL)&&
			(table->row_ptr->row_done == FALSE))
		    {
			lo_EndTableRow(context, new_state, table);
		    }
		    lo_BeginTableRow(context, new_state, table, tag);
		}
	    }
	}
}


MODULE_PRIVATE void
lo_CloseTable(MWContext *context, lo_DocState *state)
{
	if (state->current_table != NULL)
	{
		lo_TableRec *table;

		table = state->current_table;
		/*
		 * If there is an unterminated row open,
		 * terminate it now.
		 */
		if ((table->row_ptr != NULL)&&
		    (table->row_ptr->row_done == FALSE))
		{
			lo_EndTableRow(context, state, table);
		}
		lo_EndTable(context, state, state->current_table, FALSE);
	}
	else if ((state->current_table == NULL)&&
		(state->is_a_subdoc == SUBDOC_CELL))
	{
		lo_TopState *top_state;
		lo_DocState *new_state;
		int32 doc_id;

		/*
		 * This can only happen if the close cell
		 * was omitted, AND the close row
		 * after it was omitted.
		 */
		lo_EndTableCell(context, state, FALSE);

		/*
		 * restore state to the new lowest level.
		 */
		doc_id = XP_DOCID(context);
		top_state = lo_FetchTopState(doc_id);
		new_state = lo_TopSubState(top_state);

		/*
		 * Now act like a close table with
		 * an omitted close row.
		 */
		if (new_state->current_table != NULL)
		{
			lo_TableRec *table;

			table = new_state->current_table;
			/*
			 * If there is an unterminated row open,
			 * terminate it now.
			 */
			if ((table->row_ptr != NULL)&&
			    (table->row_ptr->row_done == FALSE))
			{
				lo_EndTableRow(context, new_state, table);
			}
			lo_EndTable(context, new_state,
				new_state->current_table, FALSE);
		}
	}
	else if ((state->current_table == NULL)&&
		(state->is_a_subdoc == SUBDOC_CAPTION))
	{
		lo_TopState *top_state;
		lo_DocState *new_state;
		int32 doc_id;

		/*
		 * This can only happen if the close
		 * caption was omitted.
		 */
		lo_EndTableCaption(context, state);

		/*
		 * restore state to the new lowest level.
		 */
		doc_id = XP_DOCID(context);
		top_state = lo_FetchTopState(doc_id);
		new_state = lo_TopSubState(top_state);

		/*
		 * Now act like a close table.
		 */
		if (new_state->current_table != NULL)
		{
			lo_TableRec *table;

			table = new_state->current_table;
			/*
			 * If there is an unterminated row open,
			 * terminate it now.
			 */
			if ((table->row_ptr != NULL)&&
			    (table->row_ptr->row_done == FALSE))
			{
				lo_EndTableRow(context, new_state, table);
			}
			lo_EndTable(context, new_state,
				new_state->current_table, FALSE);
		}
	}
}


static void
lo_process_table_tag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	if (state->in_paragraph != FALSE)
	{
		lo_CloseParagraph(context, &state, tag, 2);
	}
	if (tag->is_end == FALSE)
	{
		lo_BeginTable(context, state, tag);
	}
	else
	{
		lo_CloseTable(context, state);
	}
}


void
lo_CloseOutTable(MWContext *context, lo_DocState *state)
{
	lo_CloseTable(context, state);
}


#ifdef HTML_CERTIFICATE_SUPPORT

static void
lo_process_certificate_tag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	Bool tmp_in_head;
	lo_Certificate *lo_cert;

	/*
	 * Do not let closing a paragraph before a cert change the
	 * in_head state.
	 */
	tmp_in_head = state->top_state->in_head;
	if (state->in_paragraph != FALSE)
	{
		lo_CloseParagraph(context, &state, tag, 2);
	}
	state->top_state->in_head = tmp_in_head;

	if (tag->is_end == FALSE)
	{
		/*
		 * Flush the line buffer so we can start
		 * storing the certificate data there.
		 */
		lo_FlushTextBlock(context, state);

		state->line_buf_len = 0;

		lo_cert = XP_NEW(lo_Certificate);
		if (lo_cert == NULL)
		{
			state->top_state->out_of_memory = TRUE;
			return;
		}

		lo_cert->cert = NULL;
		lo_cert->name = lo_FetchParamValue(context, tag, PARAM_NAME);

		lo_cert->next = state->top_state->cert_list;
		state->top_state->cert_list = lo_cert;

		state->text_divert = tag->type;
		return;
	}

	/*
	 * Get this out of the way so we can just return if we hit
	 * errors below.
	 */
	state->text_divert = P_UNKNOWN;

	/*
	 * Only if we captured a certificate
	 */
	if (state->line_buf_len != 0)
	{
		char *tptr;

		state->line_buf_len = 0;

		lo_cert = state->top_state->cert_list;
		if (lo_cert == NULL)
			return;

		PA_LOCK(tptr, char *, state->line_buf);
		lo_cert->cert = XP_STRDUP(tptr);
		PA_UNLOCK(state->line_buf);

		if (lo_cert->cert == NULL)
		{
			state->top_state->cert_list = lo_cert->next;
			PA_FREE(lo_cert->name);
			XP_FREE(lo_cert);
		}
	}
}

#endif /* HTML_CERTIFICATE_SUPPORT */


/* Prepends a tall thin bullet to all lines numbered between start and end, inclusive.
   Used for mailing quotations. */
void
lo_add_leading_bullets(MWContext *context, lo_DocState *state,
                       int32 start,int32 end,int32 mquote_x)
{
    int32 n;
    LO_Element **line_array;
    LO_TextAttr tmp_attr;
    LO_TextAttr *bullet_attr;
    XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);

    /* Make bullets blue. */
    lo_SetDefaultFontAttr(state, &tmp_attr, context);
    tmp_attr.fg.red = 0;
    tmp_attr.fg.green = 0;
    tmp_attr.fg.blue = 255;
    bullet_attr = lo_FetchTextAttr(state, &tmp_attr);

    for (n = start; n <= end; n++)
    {
        LO_BulletStruct *bullet;
	    
        bullet = (LO_BulletStruct *)lo_NewElement(context, state, LO_BULLET, NULL, 0);
        if (bullet == NULL)
        {
            /* Don't just return, still want to unlock block. */
            n = end;
            break;
        }
       
        bullet->type = LO_BULLET;
	    bullet->ele_id = NEXT_ELEMENT; 

        bullet->x = mquote_x;
        bullet->x_offset = 0; /* LIST_MARGIN_INC / 2; */
        bullet->y = line_array[n]->lo_any.y; 
	    bullet->y_offset = 0;
		bullet->width = 5; /* 2 larger than actual size */
	    bullet->height = line_array[n]->lo_any.line_height;
	    bullet->FE_Data = NULL;
        bullet->line_height = line_array[n]->lo_any.line_height;

        /* No easy way of finding out what the bullet level actually is */
        bullet->level = 0;
        
        bullet->bullet_type = BULLET_MQUOTE;

        bullet->text_attr = bullet_attr;
	    bullet->ele_attrmask = 0;

	    bullet->sel_start = -1;
	    bullet->sel_end = -1;

        bullet->next = line_array[n];
        bullet->prev = line_array[n]->lo_any.prev;
        if (line_array[n]->lo_any.prev) 
        {
            line_array[n]->lo_any.prev->lo_any.next = (LO_Element *)bullet;
        }
        line_array[n]->lo_any.prev = (LO_Element *)bullet;        
        line_array[n] = (LO_Element *)bullet;
        /*
         ** Since these elements are added behind the layout stream, that
         ** means they would only be displayed if the area was refreshed
         ** or not visible as layout streamed through.  In case that would
         ** be bad, we ask them to be displayed as we create them.
         ** --mtoy & ltabb
         */
        if ( state != NULL &&  (!state->display_blocked) )
        {
            lo_DisplayBullet(context, bullet);
        }
    }
        
    XP_UNLOCK_BLOCK(state->line_array);
}

PRIVATE
char *
lo_MakeAttrLowerCase(char *ptr)
{
	char *tmp_ptr;

	if(!ptr)
		return NULL;

	for(tmp_ptr = ptr; *tmp_ptr; tmp_ptr++)
		*tmp_ptr = tolower(*tmp_ptr);

	return(ptr);
}

/* finds the first token in a space separated list 
 * If there are more characters after the first token
 * *end_token will be set to the next set of characters
 * otherwise it will be set to NULL
 */

PRIVATE
char *
lo_find_first_style_token(char *string_ptr, char **end_token)
{
	if(!string_ptr)
		return NULL;

	while(isspace(*string_ptr)) 
		string_ptr++;

	for(*end_token = string_ptr; **end_token && !isspace(**end_token); (*end_token)++)
		; /* null body */

	if(!**end_token)
		*end_token = NULL; /* set end_token to NULL if this is the last token */
	else
		*((*end_token)++) = '\0';

	/* if it's an empty string return NULL */
	if(!*string_ptr)
		return(NULL);
	else
		return(string_ptr);
}

/* finds the next token in a space separated list.
 * the previous value of end_token is used to find
 * the beginning of the next token
 */
PRIVATE
char *
lo_find_next_style_token(char **end_token)
{
	if(end_token)
		return lo_find_first_style_token(*end_token, end_token);

	return NULL;
}

MODULE_PRIVATE int
lo_list_bullet_type(char *type_string, TagType tag_type)
{
    int type = BULLET_BASIC;

    if(!type_string)
        return type;

    if (!strcasecomp(type_string, "none"))
	    type = BULLET_NONE;
    else if (!strcasecomp(type_string, "disc"))
	    type = BULLET_BASIC;
    else if (!strcasecomp(type_string, "circle") || !strcasecomp(type_string, "round"))
	    type = BULLET_ROUND;
    else if (!strcasecomp(type_string, "square"))
	    type = BULLET_SQUARE;
    else if (!strcasecomp(type_string, "decimal"))
	    type = BULLET_NUM;
    else if (!strcasecomp(type_string, "lower-roman"))
	    type = BULLET_NUM_S_ROMAN;
    else if (!strcasecomp(type_string, "upper-roman"))
	    type = BULLET_NUM_L_ROMAN;
    else if (!strcasecomp(type_string, "lower-alpha"))
	    type = BULLET_ALPHA_S;
    else if (!strcasecomp(type_string, "upper-alpha"))
	    type = BULLET_ALPHA_L;
    else if (*type_string == 'A')
	    type = BULLET_ALPHA_L;
    else if (*type_string == 'a')
	    type = BULLET_ALPHA_S;
    else if (*type_string == 'I')
	    type = BULLET_NUM_L_ROMAN;
    else if (*type_string == 'i')
	    type = BULLET_NUM_S_ROMAN;
    else if(tag_type == P_NUM_LIST)
	    type = BULLET_NUM;		
    else
	    type = BULLET_BASIC;

    return type;
}

void
lo_SetupStateForList(MWContext *context,
		     lo_DocState *state,
		     LO_ListStruct *list,
		     XP_Bool in_resize_reflow)
{
  int32 old_right_margin;
  
  if (state->list_stack->level == 0)
	{
	  /*
      lo_SetSoftLineBreakState(context, state,
			       FALSE, 2);
	  */
	  lo_SetLineBreakState (context, state, FALSE, LO_LINEFEED_BREAK_SOFT, 2, in_resize_reflow);
    }
  else
    {
	/*
      lo_SetSoftLineBreakState(context, state,
			       FALSE, 1);
	*/
	  lo_SetLineBreakState (context, state, FALSE, LO_LINEFEED_BREAK_SOFT, 1, in_resize_reflow);
    }
  /* THIS BREAKS FLOATING IMAGES IN LISTS
     state->list_stack->old_left_margin =
     state->left_margin;
     state->list_stack->old_right_margin =
     state->right_margin;
  */
  /*
   * For lists, we leave the right
   * margin unchanged.
   */
  old_right_margin =
    state->list_stack->old_right_margin;

  /*
   * handle nested description lists
   * with the right amount of indenting
   */
  if ((list->tag->type == P_DESC_LIST)&&
	  (state->list_stack->type == P_DESC_LIST)&&
	  (state->list_stack->value == 1))
	{
	  state->left_margin += LIST_MARGIN_INC;
	}

  lo_PushList(state, list->tag, list->quote_type);

  /* <MQUOTE> gives less indent than other lists */
  if (list->quote_type == QUOTE_MQUOTE)
    {
      state->left_margin += MQUOTE_MARGIN_INC;
    }
  else if (list->tag->type != P_DESC_LIST)
    {
      state->left_margin += LIST_MARGIN_INC;
    }

  state->x = state->left_margin;
  state->list_stack->old_left_margin =
    state->left_margin;
  /*
   * Set right margin to be last list's right,
   * irregardless of current margin stack.
   */
  state->list_stack->old_right_margin =
    old_right_margin;
  state->list_stack->bullet_type = (intn) list->bullet_type;
  state->list_stack->value = list->bullet_start;
  state->list_stack->compact = list->compact;
}

void
lo_UpdateStateAfterList(MWContext *context,
			lo_DocState *state,
			LO_ListStruct *list,
			XP_Bool in_resize_reflow)
{
  lo_ListStack *lptr;
  Bool mquote = FALSE;
  int32 mquote_line_num = 0;
  int32 mquote_x = 0;

  /*
   * Reset fake linefeed state used to
   * fake out headers in lists.
   */
  if (state->at_begin_line == FALSE)
	{
	  state->linefeed_state = 0;
	}
  
  lptr = lo_PopList(state, NULL);
  if (lptr != NULL)
	{
	  if (lptr->quote_type == QUOTE_MQUOTE) {
		mquote = TRUE;
	  }
	  else {
		mquote = FALSE;
	  }
	  mquote_line_num = lptr->mquote_line_num;
	  mquote_x = lptr->mquote_x;
	  XP_DELETE(lptr);
	}
  state->left_margin =
	state->list_stack->old_left_margin;
  state->right_margin =
	state->list_stack->old_right_margin;
  /*
   * Reset the margins properly in case
   * there is a right-aligned image here.
   */
  lo_FindLineMargins(context, state, !in_resize_reflow);
  if (state->list_stack->level == 0)
	{
	  if (state->linefeed_state >= 2)
		{
		  lo_FindLineMargins(context, state, !in_resize_reflow);
		}
	  else
		{
			if (!in_resize_reflow)
			{
				/* we only want to append a paragraph break if we're initially laying out the
				document, not if we're reflowing it. */			
				lo_SetLineBreakState(context, state, FALSE,
					LO_LINEFEED_BREAK_PARAGRAPH, 2, FALSE);
			}
		}
	}
  else
	{
	  if (state->linefeed_state >= 1)
		{
		  lo_FindLineMargins(context, state, !in_resize_reflow);
		}
	  else
		{
		  /* lo_SetSoftLineBreakState(context, state, FALSE, 1); */
		  lo_SetLineBreakState (context, state, FALSE, LO_LINEFEED_BREAK_SOFT, 1, in_resize_reflow);
		}
	}

	/* I don't think we need to do this as the linefeeds should reset x for us. However, if we do
	   need this, make sure to reimplement it in a way so as to not break the editor during reflow
	   (max_width was not being correctly set due to the fact that we were resetting x here) */
#if 0
  state->x = state->left_margin;
#endif
  
  /* Go back and add bullets to all lines between here and the 
	 beginning of the mailing quotation. */
  if (mquote)
	{
	  lo_add_leading_bullets(context,state,
							 mquote_line_num - 1,
							 state->line_num - state->linefeed_state - 1,
							 mquote_x);
	}
}

PRIVATE void
lo_setup_list(lo_DocState *state, 
		MWContext *context,
		PA_Tag *tag,
		char *bullet_type,
		char *bullet_start,
		char *compact_attr)
{

		int32 type;
		int32 val;
		Bool compact;
		int8 quote_type = QUOTE_NONE;
		char *list_style_prop=NULL;
		LO_ListStruct *list;

		if (tag->type == P_MQUOTE)
		{
			quote_type = QUOTE_MQUOTE;
		}

		if (state->in_paragraph != FALSE)
		{
			lo_CloseParagraph(context, &state, tag, 2);
		}

		list = (LO_ListStruct*)lo_NewElement(context, state, LO_LIST, NULL, 0);
		XP_ASSERT(list);
		if (!list) return;

		/* check for a bullet type from style sheets */
		if(state && state->top_state && state->top_state->style_stack && tag->type != P_DESC_LIST)
		  {
			StyleStruct *style_struct = STYLESTACK_GetStyleByIndex(state->top_state->style_stack, 0);
			
			if(style_struct)
			  {
				list_style_prop = STYLESTRUCT_GetString(style_struct, LIST_STYLE_TYPE_STYLE);
				if(list_style_prop)
				  {
					bullet_type = list_style_prop;
				  }
			  }
		  }

		type = BULLET_BASIC;
		if(bullet_type && tag->type != P_DESC_LIST)
		{
            type = lo_list_bullet_type(bullet_type, tag->type);

            XP_FREEIF(list_style_prop);
		}
		else if (tag->type == P_NUM_LIST)
		{
			type = BULLET_NUM;
		}
		else if (tag->type != P_DESC_LIST)
		{
			intn lev;

			lev = state->list_stack->level;
			if (lev < 1)
			{
				type = BULLET_BASIC;
			}
			else if (lev == 1)
			{
				type = BULLET_ROUND;
			}
			else if (lev > 1)
			{
				type = BULLET_SQUARE;
			}
		}
		
		if (bullet_start != NULL)
		{

			val = XP_ATOI(bullet_start);
			if (val < 1)
			{
				val = 1;
			}
		}
		else
		{
			val = 1;
		}

		if (compact_attr)
		{
			compact = TRUE;
		}
		else
		{
			compact = FALSE;
		}

		list->lo_any.type = LO_LIST;
  		list->lo_any.ele_id = NEXT_ELEMENT;
		list->is_end = FALSE;
		list->bullet_type = type;
		list->bullet_start = val;
		list->quote_type = quote_type;
		list->compact = compact;
		list->tag = PA_CloneMDLTag(tag);

		list->lo_any.x = state->x;
		list->lo_any.y = state->y;
		list->lo_any.x_offset = 0;
		list->lo_any.y_offset = 0;
		list->lo_any.width = 0;
		list->lo_any.height = 0;
		list->lo_any.line_height = 0;
		
		lo_AppendToLineList(context, state, (LO_Element*)list, 0);
		lo_SetupStateForList(context, state, list, FALSE);
}

MODULE_PRIVATE void
lo_TeardownList(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
		LO_ListStruct *list;

  		if (tag && state->in_paragraph != FALSE)
		{
			lo_CloseParagraph(context, &state, tag, 2);
		}

		list = (LO_ListStruct*)lo_NewElement(context, state, LO_LIST, NULL, 0);
		XP_ASSERT(list);
		if (!list) return;

		list->lo_any.type = LO_LIST;
  		list->lo_any.ele_id = NEXT_ELEMENT;
		list->is_end = TRUE;
		list->tag = PA_CloneMDLTag(tag);

		list->lo_any.x = state->x;
		list->lo_any.y = state->y;
		list->lo_any.x_offset = 0;
		list->lo_any.y_offset = 0;
		list->lo_any.width = 0;
		list->lo_any.height = 0;
		list->lo_any.line_height = 0;

		lo_AppendToLineList(context, state, (LO_Element*)list, 0);
		lo_UpdateStateAfterList(context, state, list, FALSE);
}

PRIVATE int32
lo_calc_distance_to_bottom_of_prev_line(lo_DocState *state)
{
	int32 height;
	LO_LinefeedStruct *linefeed;

	if(!state->end_last_line)
		return 0;

	linefeed = (LO_LinefeedStruct*)state->end_last_line;
	height = state->y - (linefeed->y + linefeed->y_offset + linefeed->line_height);
	

#ifndef ALLOW_NEG_MARGINS
	XP_ASSERT(height > -1); /* this happens w/ neg margins */
#endif

	if(height < 0)
		height = 0;

	return height;
}

/**************************
 * Should be moved to laystyle.c eventually
 */
PRIVATE
void
lo_SetNewMarginsForStyle(lo_DocState *state, 
						 PA_Tag *tag, 
						 StyleStruct *style_struct,
						 int32 text_width,
						 int32 left_margin_offset, 
						 int32 right_margin_offset)
 {

        /* set left and right margins */
        lo_PushList(state, tag, QUOTE_NONE);

		if(left_margin_offset)
		{
            state->left_margin += left_margin_offset;
			if(state->left_margin < 0)
				state->left_margin = 0;
		}

		/* can't be negative */
        if(right_margin_offset)
            state->right_margin -= right_margin_offset;
	
		if(text_width > 0)
		{
			int32 cur_width = state->right_margin - state->left_margin;
			int32 width_diff = text_width - cur_width;

			/* right_margin loses when a width is set */
			state->right_margin += width_diff;
			
			/* keep the right margin on the screen */
			if(state->right_margin > (state->win_width - state->left_margin))
				state->right_margin = state->win_width - state->left_margin;
		}

        state->x = state->left_margin;

        state->list_stack->old_left_margin = state->left_margin;
        state->list_stack->old_right_margin = state->right_margin;
}

PRIVATE
int32
lo_CalcCurrentLineHeight(MWContext *context, lo_DocState *state)
{

		if(state->line_height)
		{
			return(state->line_height);
		}
		else
		{
			int32 new_height = state->text_info.ascent +
            						state->text_info.descent;

			if ((new_height <= 0)&&(state->font_stack != NULL)&&
				(state->font_stack->text_attr != NULL))
			{
				lo_fillin_text_info(context, state);

				new_height = state->text_info.ascent + state->text_info.descent;

			}
			
			return new_height;
		}
}

int PR_CALLBACK  lo_face_attribute_pref_callback(const char *pref_name, void *);
/*************************************
 * Function: lo_face_attribute_pref_callback
 *
 * Description: This function is registered with XP prefs and is called whenever
 *              the value of "browser.use_document_fonts" is changed.
 *
 * Params: pref_name is the pref string that the call is registered with,
 *         "browser.use_document_fonts" and a void pointer that is registered as NULL.
 *
 * Returns: PREF_NOERROR (which is ignored)
 *************************************/
PRIVATE Bool lo_do_face_attribute = TRUE;

int PR_CALLBACK 
lo_face_attribute_pref_callback(const char *pref_name, void *notUsed)
{
	int32	fontPrefValue;
    PREF_GetIntPref(pref_name, &fontPrefValue);

	/* We use the face attribute if the pref value is 1 (face attribute only)
		or 2 (face attribute and web fonts), but not if the it is 0 (never use
		document fonts). */
	lo_do_face_attribute = fontPrefValue ? TRUE: FALSE;

    return PREF_NOERROR;
}

#define PICS_SUPPORT
#ifdef PICS_SUPPORT

PRIVATE Bool
lo_is_url_excluded_from_pics(URL_Struct *URL_s)
{
	int type = NET_URL_Type(URL_s->address);

	switch(type)
	{
		case SECURITY_TYPE_URL:
		case ABOUT_TYPE_URL:
		case ADDRESS_BOOK_TYPE_URL:
		case ADDRESS_BOOK_LDAP_TYPE_URL:
		case INTERNAL_IMAGE_TYPE_URL:
		case HTML_DIALOG_HANDLER_TYPE_URL:
		case HTML_PANEL_HANDLER_TYPE_URL:
		case INTERNAL_SECLIB_TYPE_URL:
		case INTERNAL_CERTLDAP_TYPE_URL:
		case MOCHA_TYPE_URL:
		case NETHELP_TYPE_URL:
		case VIEW_SOURCE_TYPE_URL:
        case WYSIWYG_TYPE_URL:
			return TRUE;

		default:
			return FALSE;
	}
}

/* Interpret a PICS label if there is one.
 * returns true to interrupt layout
 */
PRIVATE
PICS_PassFailReturnVal
lo_ProcessPicsLabel(MWContext *context, lo_DocState *state, char **fail_url)
{
	URL_Struct *URL_s;
	PICS_RatingsStruct *rs = NULL;
    PICS_PassFailReturnVal status = PICS_NO_RATINGS;
    uint i;

#define PICS_HEADER "PICS-Label"

	if(!PICS_IsPICSEnabledByUser())
		return PICS_RATINGS_PASSED;

	if(!state->top_state 
       || !state->top_state->nurl
       || !*state->top_state->nurl->address)
		return PICS_RATINGS_PASSED;
	
	URL_s = state->top_state->nurl;

	if(lo_is_url_excluded_from_pics(URL_s))
		return PICS_RATINGS_PASSED;

	/* check the URL struct for a pics label */
    for(i=0 ;i < URL_s->all_headers.empty_index; i++)
	{
		if(!strcasecomp(URL_s->all_headers.key[i], PICS_HEADER))
		{
            PICS_PassFailReturnVal tmp_status;
            char *ptr;

			/* found a pics header */

            /* change any \n or \r to space */
            if(URL_s->all_headers.value[i])
                for(ptr = URL_s->all_headers.value[i]; *ptr; ptr++)
                    if(*ptr == '\r' || *ptr == '\n')
                        *ptr = ' ';

			/* parse it */
			rs = PICS_ParsePICSLable(URL_s->all_headers.value[i]);

            /* compare to the prefs
	         */
	        tmp_status = PICS_CompareToUserSettings(rs, URL_s->address);

            if(tmp_status == PICS_RATINGS_FAILED)
            {
                status = tmp_status;
                *fail_url = PICS_RStoURL(rs, URL_s->address);
                break;  /* finished */
            }
            else if(tmp_status == PICS_RATINGS_PASSED)
            {
                /* don't overwrite previous passed ratings with
                 * no_rating
                 */
                status = tmp_status;
            }
		}
	}

    if(status == PICS_RATINGS_FAILED)
	{
		/* keep the URL from being cached 
		 * Do it here since we have the URL struct
		 */
		URL_s->dont_cache = TRUE;
	}

    PICS_FreeRatingsStruct(rs);  /* handles NULL */

    return status;
}
#endif /* PICS_SUPPORT */

Bool lo_face_attribute();
/*************************************
 * Function: lo_face_attribute
 * Description: This function determines whether or not we should use the FACE
 *				attribute in FONT tags. It cache the answer in a static and only
 *				calls into the prefs the first time. A callback is registered to
 *				keep the cache value current.
 *
 * Params: none.
 *
 * Returns: true iff the current pref setting calls for the use of the FACE
 *          attribute in FONT tags
 *************************************/
Bool 
lo_face_attribute()
{
    static Bool first_time = TRUE;

    if(first_time)
    {
		int32	fontPrefValue;
        first_time = FALSE;
        PREF_GetIntPref("browser.use_document_fonts", &fontPrefValue);
		/* We use the face attribute if the pref value is 1 (face attribute only)
			or 2 (face attribute and web fonts), but not if the it is 0 (never use
			document fonts). */
		lo_do_face_attribute = fontPrefValue ? TRUE: FALSE;
        PREF_RegisterCallback(	"browser.use_document_fonts",
        						lo_face_attribute_pref_callback,
        						NULL);
    }

    return (lo_do_face_attribute);
}

/* returns true if the tag can or did push a font
 * of a different size onto the font stack
 */
PRIVATE
Bool
lo_tag_pushes_different_size_font(TagType type)
{
	if(type == P_FONT
	   || type == P_HEADER_1
	   || type == P_HEADER_2
	   || type == P_HEADER_3
	   || type == P_HEADER_4
	   || type == P_HEADER_5
	   || type == P_HEADER_6
	  )
		return TRUE;

	return FALSE;
}

static void
lo_adjust_border_thickness_for_style(StyleStruct *style_struct, SS_Number *width)
{
	char *border_style_value;

	if(!width)
		return;

	border_style_value = STYLESTRUCT_GetString(style_struct, 
													 BORDER_STYLE_STYLE);

	if(!border_style_value)
		return;

	if(!strcasecomp(border_style_value, "groove")
	   || !strcasecomp(border_style_value, "ridge"))
	{
		width->value *= 2;	
	}
	else if(!strcasecomp(border_style_value, "double"))
	{
		width->value *= 3;	
	}
}

PRIVATE
void
lo_SetStyleSheetFontProperties(MWContext *context, 
								lo_DocState *state, 
								StyleStruct *style_struct, 
								PA_Tag *tag,
                                XP_Bool use_background_color)
{
	char *property;
	SS_Number *ss_num;
	LO_TextAttr *attr;
	LO_TextAttr tmp_attr;
	XP_Bool push_font = FALSE;

	if(!style_struct || !state)
		return;

	if(state->font_stack)
    {
        lo_CopyTextAttr(state->font_stack->text_attr, &tmp_attr);
    }
    else
    {
        XP_MEMSET(&tmp_attr,0,sizeof(tmp_attr));
    }

	property = STYLESTRUCT_GetString(style_struct, COLOR_STYLE);
	if (property)
	{
		LO_ParseStyleSheetRGB(property, &tmp_attr.fg.red, &tmp_attr.fg.green, &tmp_attr.fg.blue);
		XP_FREE(property);
		push_font = TRUE;
	}

    /* don't inherit text background colors from the body and table tags */
    if(use_background_color
       && tag->type != P_UNKNOWN  /* table relayout dummy tag */
       && tag->type != P_BODY
       && tag->type != P_TABLE
       && tag->type != P_TABLE_DATA
       && tag->type != P_TABLE_HEADER
       && tag->type != P_TABLE_ROW)
    {
        property = STYLESTRUCT_GetString(style_struct, BG_COLOR_STYLE);
        if (property)
        {
	        LO_ParseStyleSheetRGB(property, &tmp_attr.bg.red, &tmp_attr.bg.green, &tmp_attr.bg.blue);
            if(strcasecomp(property, "transparent"))
                tmp_attr.no_background = FALSE;  /* force layout to show the background color */
	    else
                tmp_attr.no_background = TRUE;  /* no background */
	        XP_FREE(property);
	        push_font = TRUE;
        }
    }

	if (lo_face_attribute())
	{
		property = STYLESTRUCT_GetString(style_struct, FONTFACE_STYLE);
		if (property)
		{
			tmp_attr.font_face = lo_FetchFontFace(context, state, property);
			XP_FREE(property);
			push_font = TRUE;
		}
	}
	
	property = STYLESTRUCT_GetString(style_struct, FONTSIZE_STYLE);
	if(property)
	{
		/* scaling factor of 1.4 with medium at 10pts
 		 */
		if(!strcasecomp(property, "xx-small"))		
		{
			ss_num = STYLESTRUCT_StringToSSNumber(style_struct, "6pt");
		}
		else if(!strcasecomp(property, "x-small"))		
		{
			ss_num = STYLESTRUCT_StringToSSNumber(style_struct, "8pt");
		}
		else if(!strcasecomp(property, "small"))		
		{
			ss_num = STYLESTRUCT_StringToSSNumber(style_struct, "10pt");
		}
		else if(!strcasecomp(property, "medium"))		
		{
			ss_num = STYLESTRUCT_StringToSSNumber(style_struct, "12pt");
		}
		else if(!strcasecomp(property, "large"))		
		{
			ss_num = STYLESTRUCT_StringToSSNumber(style_struct, "18pt");
		}
		else if(!strcasecomp(property, "x-large"))		
		{
			ss_num = STYLESTRUCT_StringToSSNumber(style_struct, "27pt");
		}
		else if(!strcasecomp(property, "xx-large"))		
		{
			ss_num = STYLESTRUCT_StringToSSNumber(style_struct, "40pt");
		}
		else if(!strcasecomp(property, "larger"))		
		{
			/* scale up by 1.5 */
			ss_num = STYLESTRUCT_StringToSSNumber(style_struct, "1.5em");
		}
		else if(!strcasecomp(property, "smaller"))		
		{
			/* scale down by 1.5 */
			ss_num = STYLESTRUCT_StringToSSNumber(style_struct, "0.66667em");
		}
		else
		{
			/* interpret the property as a number 
			 * invalid properties will resolve to 0 and be ignored
			 */
			ss_num = STYLESTRUCT_StringToSSNumber(style_struct, property);
		}

		if(ss_num && ss_num->value > 0)
		{
			LO_TextAttr *top_font=NULL;

			/* if the current tag pushed a font of different
			 * size on the stack, temporarily pop it off
			 * so that we can calculate EM units from the parent's
			 * tag size rather than this tags size
			 */
			if(lo_tag_pushes_different_size_font(tag->type))
			    top_font = lo_PopFont(state, tag->type);

			LO_AdjustSSUnits(ss_num, FONTSIZE_STYLE, context, state);
			tmp_attr.point_size = ss_num->value;
			push_font = TRUE;

			/* push the old font back on the stack */
			if(top_font)
				lo_PushFont(state, tag->type, top_font);  
		}

		if(ss_num)
			STYLESTRUCT_FreeSSNumber(style_struct, ss_num);

		XP_FREE(property);
	}

	property = STYLESTRUCT_GetString(style_struct, FONTWEIGHT_STYLE);
	if(property)
	{
		push_font = TRUE;

		if(!strcasecomp(property, "normal"))
			tmp_attr.font_weight = 400;
		else if(!strcasecomp(property, "bold"))
			tmp_attr.font_weight = 700;
		else if(!strcasecomp(property, "bolder"))
		{
			if(!tmp_attr.font_weight)
				tmp_attr.font_weight = 400;  /* normal */
			tmp_attr.font_weight += 100;
			if(tmp_attr.font_weight < 100)
				tmp_attr.font_weight = 100;
		}
   		else if(!strcasecomp(property, "lighter"))
		{
			if(!tmp_attr.font_weight)
				tmp_attr.font_weight = 400;  /* normal */
			tmp_attr.font_weight -= 100;
			if(tmp_attr.font_weight > 900)
				tmp_attr.font_weight = 900;
		}
		else
		{
			ss_num = STYLESTRUCT_StringToSSNumber(style_struct, property);

			if(ss_num && ss_num->value > 0)
			{
				/* normalize value */
				/* 100, 200, 300, 400, 500, 600, 700, 800, or 900 */
				uint32 weight = (int32) ss_num->value;
				weight = weight - (weight % 100);

				if(weight > 0)
				{
					tmp_attr.font_weight = (uint16)weight;
				}
				STYLESTRUCT_FreeSSNumber(style_struct, ss_num);
			}
		}

		XP_FREE(property);
	}

	property = STYLESTRUCT_GetString(style_struct, FONTSTYLE_STYLE);
	if(property)
	{
		tmp_attr.fontmask &= (~LO_FONT_ITALIC);

		if(!strcasecomp(property, NORMAL_STYLE))
		{
			/* do nothing */
		}
		else if(!strcasecomp(property, ITALIC_STYLE))
		{
			tmp_attr.fontmask |= LO_FONT_ITALIC;
		}
		else if(!strcasecomp(property, OBLIQUE_STYLE))
		{
			/* no oblique property yet */
		}
		XP_FREE(property);
		push_font = TRUE;
	}

	property = STYLESTRUCT_GetString(style_struct, TEXTDECORATION_STYLE);
	if(property)
	{
		char *token, *end_token;
			
		tmp_attr.attrmask &= (~(LO_ATTR_BLINK | LO_ATTR_STRIKEOUT | LO_ATTR_UNDERLINE));

		/* parse out separate tokens */
		token = lo_find_first_style_token(property, &end_token);
		while(token)
		{
			if(!strcasecomp(token, "none"))
			{
				/* do nothing */
			}
			else if(!strcasecomp(token, BLINK_STYLE))
			{
				tmp_attr.attrmask |= LO_ATTR_BLINK;
			}
			else if(!strcasecomp(token, STRIKEOUT_STYLE))
			{
				tmp_attr.attrmask |= LO_ATTR_STRIKEOUT;
			}
			else if(!strcasecomp(token, UNDERLINE_STYLE))
			{
				tmp_attr.attrmask |= LO_ATTR_UNDERLINE;
			}
			else   
			{
				/* all others no decoration */
			}

			token = lo_find_next_style_token(&end_token);
		}
		push_font = TRUE;
		XP_FREE(property);
	}

	if(push_font)
	{
		attr = lo_FetchTextAttr(state, &tmp_attr);
        /* don't use the tag type since the A tag is treated special 
         * and we don't want that special treatment
         */
		lo_PushFont(state, P_UNKNOWN, attr);  
		STYLESTRUCT_SetString(style_struct, STYLE_NEED_TO_POP_FONT, "1", 0);
	}

}

static SS_Number *
lo_get_border_width(MWContext *context, lo_DocState *state, StyleStruct *style_struct, char *which_one)
{
	char *property = STYLESTRUCT_GetString(style_struct, which_one);
	SS_Number *width;

	if(!property)
		return NULL;

	if(!strcasecomp(property, "thin"))
	{
		width = STYLESTRUCT_StringToSSNumber(style_struct, "1px");
		lo_adjust_border_thickness_for_style(style_struct, width);
	}
	else if(!strcasecomp(property, "medium"))
	{
		width = STYLESTRUCT_StringToSSNumber(style_struct, "3px");
		lo_adjust_border_thickness_for_style(style_struct, width);
	}
	else if(!strcasecomp(property, "thick"))
	{
		width = STYLESTRUCT_StringToSSNumber(style_struct, "5px");
		lo_adjust_border_thickness_for_style(style_struct, width);
	}
	else
	{
		width = STYLESTRUCT_StringToSSNumber(style_struct, property);
	}

	LO_AdjustSSUnits(width, which_one, context, state);

	return(width);
}

PRIVATE
void
lo_SetStyleSheetBoxProperties(MWContext *context,
                                lo_DocState *state,
                                StyleStruct *style_struct,
                                PA_Tag *tag)
{
    SS_Number *left_margin, *right_margin; 
    SS_Number *left_padding, *right_padding; 
	SS_Number *text_indent;
	SS_Number *text_width;
	char *align_value, *bgimage_value, *border_style_value, *border_color_value;
	SS_Number *borderwidth_value;
	SS_Number *bordertopwidth_value;
	SS_Number *borderbottomwidth_value;
	SS_Number *borderrightwidth_value;
	SS_Number *borderleftwidth_value;
	char *display_prop, *align_property, *page_break_property;
	Bool use_table_for_box=FALSE;
	int32 left_margin_offset=0, right_margin_offset=0;
	Bool is_table_relayout_begin_dummy_tag=FALSE;

    if(!style_struct)
        return;

	/* if we get in here and the tag->type is UKNOWN then this must
	 * be a dummy tag inserted for the sake of table relayout passes
	 * Mark it as such
	 */
	if(tag->type == P_UNKNOWN)
		is_table_relayout_begin_dummy_tag = TRUE;

	page_break_property = STYLESTRUCT_GetString(style_struct, PAGE_BREAK_BEFORE_STYLE);
	if (page_break_property)
	{
		if(!strcasecomp(page_break_property, "auto"))
		{
			/* not currently supported */
		}
		else if(!strcasecomp(page_break_property, "always"))
		{
			/* not currently supported */
		}
		else if(!strcasecomp(page_break_property, "left"))
		{
			/* not currently supported */
		}
		else if(!strcasecomp(page_break_property, "right"))
		{
			/* not currently supported */
		}
		XP_FREE(page_break_property);
	}

	/* add a linebreak for block elements */
	display_prop = STYLESTRUCT_GetString(style_struct, DISPLAY_STYLE);
	if(display_prop && !strcasecomp(display_prop, BLOCK_STYLE))
	{
		if (state->in_paragraph)
		{
			lo_CloseParagraph(context, &state, tag, 2);
		}

		lo_SetLineBreakState(context, state, FALSE,
							 LO_LINEFEED_BREAK_PARAGRAPH, 1, FALSE);
	}
	else if(display_prop && !strcasecomp(display_prop, LIST_ITEM_STYLE))
	{
		char *bullet_type;
		char *list_style_prop = STYLESTRUCT_GetString(style_struct,
													  LIST_STYLE_TYPE_STYLE);


		if(list_style_prop)
			bullet_type = list_style_prop;
		else
			bullet_type = "disc";

		lo_setup_list(state, 
					  context,
					  tag,
					  bullet_type,
					  NULL,  /* start point */
					  NULL); /* compact */

		XP_FREEIF(list_style_prop);

        STYLESTRUCT_SetString(style_struct, STYLE_NEED_TO_POP_LIST, "1", 0);

	}
	XP_FREEIF(display_prop);

	left_margin = STYLESTRUCT_GetNumber(style_struct, LEFTMARGIN_STYLE);
	LO_AdjustSSUnits(left_margin, LEFTMARGIN_STYLE, context, state);
	right_margin = STYLESTRUCT_GetNumber(style_struct, RIGHTMARGIN_STYLE);
	LO_AdjustSSUnits(right_margin, RIGHTMARGIN_STYLE, context, state);

	
	/* don't allow negative margins */
	if(left_margin 
#ifndef ALLOW_NEG_MARGINS
		&& left_margin->value > 0
#endif
	   )
		left_margin_offset = (int32)left_margin->value;

	if(right_margin && right_margin->value > 0)
		right_margin_offset = (int32)right_margin->value;

	left_padding   = STYLESTRUCT_GetNumber(style_struct, LEFTPADDING_STYLE);
	LO_AdjustSSUnits(left_padding, LEFTPADDING_STYLE, context, state);

	right_padding  = STYLESTRUCT_GetNumber(style_struct, RIGHTPADDING_STYLE);
	LO_AdjustSSUnits(right_padding, RIGHTPADDING_STYLE, context, state);

    text_indent = STYLESTRUCT_GetNumber(style_struct, TEXTINDENT_STYLE);
	LO_AdjustSSUnits(text_indent, TEXTINDENT_STYLE, context, state);

	text_width = STYLESTRUCT_GetNumber(style_struct, WIDTH_STYLE);
	LO_AdjustSSUnits(text_width, WIDTH_STYLE, context, state);

	/* process bottom margin and padding in PopTag
	 *    bottom_margin = STYLESTRUCT_GetNumber(style_struct, BOTTOMMARGIN_STYLE);
	 */	   

	align_value = STYLESTRUCT_GetString(style_struct, HORIZONTAL_ALIGN_STYLE);
	if(align_value && !strcasecomp(align_value, "none"))
	{
		XP_FREE(align_value);
		align_value = NULL;
	}

    bgimage_value = STYLESTRUCT_GetString(style_struct, BG_IMAGE_STYLE);
    if(bgimage_value && !strcasecomp(bgimage_value, "none"))
    {
       	XP_FREEIF(bgimage_value);
		bgimage_value = NULL;
    }

	if(bgimage_value
       && (tag->type == P_BODY
           || tag->type == P_TABLE_DATA   /* these tags can do their own backgrounds */
           || tag->type == P_TABLE_HEADER
           || tag->type == P_TABLE_ROW))
	{
        XP_FREEIF(bgimage_value);
        bgimage_value = NULL;
	}

	borderwidth_value       = lo_get_border_width(context, state, style_struct, 
													BORDERWIDTH_STYLE);
	bordertopwidth_value    = lo_get_border_width(context, state, style_struct, 
													BORDERTOPWIDTH_STYLE);
	borderbottomwidth_value = lo_get_border_width(context, state, style_struct, 
													BORDERBOTTOMWIDTH_STYLE);
	borderleftwidth_value   = lo_get_border_width(context, state, style_struct, 
													BORDERLEFTWIDTH_STYLE);
	borderrightwidth_value  = lo_get_border_width(context, state, style_struct, 
													BORDERRIGHTWIDTH_STYLE);

	border_style_value = STYLESTRUCT_GetString(style_struct, BORDER_STYLE_STYLE);
	border_color_value = STYLESTRUCT_GetString(style_struct, BORDER_COLOR_STYLE);

	/* do backgrounds and floats using tables */
	if(!(tag->type == P_TABLE
         || tag->type == P_TABLE_DATA
         || tag->type == P_TABLE_HEADER
         || tag->type == P_TABLE_ROW)  /* never create a table for these tags */
       && (align_value 
           || bgimage_value
		   || (borderwidth_value && borderwidth_value->value > 0)
		   || (bordertopwidth_value && bordertopwidth_value->value > 0)
		   || (borderbottomwidth_value && borderbottomwidth_value->value > 0)
		   || (borderrightwidth_value && borderrightwidth_value->value > 0)
	       || (borderleftwidth_value && borderleftwidth_value->value > 0)))
	{
		use_table_for_box = TRUE;

		/* use a solid border as the default */
		if(!border_style_value)
			border_style_value = XP_STRDUP("solid");
	}
	else
	{
		/* add the paddings to the margin widths since there will not
		 * be a table 
		 */
		if(left_padding && left_padding->value > 0)
			left_margin_offset += (int32)left_padding->value;

		if(right_padding && right_padding->value > 0)
			right_margin_offset += (int32)right_padding->value;
	}


	if(left_margin_offset || right_margin_offset || (text_width && !use_table_for_box))
	{
		/* only set the list to be pop'd for styles outside
		 * a table created by this tag
		 *
		 * TRICKY!: don't set margins for the dummy tag that started
		 * a table, since a table already has established margins
		 */
		if(!is_table_relayout_begin_dummy_tag)
		{
			STYLESTRUCT_SetString(style_struct, STYLE_NEED_TO_POP_MARGINS, "1", 0);
			lo_SetNewMarginsForStyle(state, 
									 tag, 
									 style_struct, 
									 text_width ? (int32)text_width->value : 0,
									 left_margin_offset, 
									 right_margin_offset);
		}
	}

		/* handle text alignment here too, even though it's not really a font */
	align_property = STYLESTRUCT_GetString(style_struct, TEXT_ALIGN_STYLE);
	if (align_property)
	{
	  	intn alignment = lo_EvalDivisionAlignParam(align_property);

		lo_PushAlignment(state, tag->type, alignment);
		STYLESTRUCT_SetString(style_struct, 
							  STYLE_NEED_TO_POP_ALIGNMENT,
							  "1",
							  0);
		XP_FREE(align_property);
	}



	/* add vertical formatting
	 * but only if we are at the beginning of a line 
	 */
	if(state->at_begin_line)
	{
        int32 move_size;
		SS_Number *ss_num=NULL;
		char *line_height_char_prop;
		SS_Number *top_margin;
		int32 line_height_diff = 0;

		line_height_char_prop = STYLESTRUCT_GetString(style_struct, LINE_HEIGHT_STYLE);

		if(line_height_char_prop)
		{
			if(!strcasecomp(line_height_char_prop, "normal"))
			{
				ss_num = NULL;
			}
			else
			{
				ss_num = STYLESTRUCT_StringToSSNumber(style_struct, 
													  line_height_char_prop);
			}
			LO_AdjustSSUnits(ss_num, LINE_HEIGHT_STYLE, context, state);

			XP_FREE(line_height_char_prop);
		}

		/* calculate a y offset to do a correct line height */
		if (ss_num)
		{
			int32 cur_line_height = lo_CalcCurrentLineHeight(context, state);

			if(ss_num->value < 0)
				ss_num->value = 0;  /* negative lineheight values are always illegal */

			/* adjust line-height for printing */
			ss_num->value = FEUNITS_Y(ss_num->value, context);

			line_height_diff = ((int32)ss_num->value) - cur_line_height;

#ifndef ALLOW_NEG_MARGINS
			/* only allow increasing line heights 
			 * explicitly disallow negative diffs
			 * NOTE: this is different than an explicit negative line-height.
			 */
			if(line_height_diff < 0)
			{
				line_height_diff = 0;
			}
			else
#endif
			{
				/* push the height onto the line height stack so that it can be used
				 * at the beginning of every line
				 * Note: this wont happen if the margin would cause negative spacing.
				 */
				lo_PushLineHeight(state, (int32)ss_num->value);
				STYLESTRUCT_SetString(style_struct, STYLE_NEED_TO_POP_LINE_HEIGHT, "1", 0);
			}

			STYLESTRUCT_FreeSSNumber(style_struct, ss_num);

			/* don't apply y offset if we are in the table
			 * relayout step
			 */
			if(!is_table_relayout_begin_dummy_tag)
				state->y += line_height_diff;
		}

		top_margin = STYLESTRUCT_GetNumber(style_struct, TOPMARGIN_STYLE);
		LO_AdjustSSUnits(top_margin, TOPMARGIN_STYLE, context, state);

		/* don't apply y offset if we are in the table
		 * relayout step
		 */
		if(!is_table_relayout_begin_dummy_tag
			&& top_margin 
#ifndef ALLOW_NEG_MARGINS
			&& top_margin->value > -1
#endif
			)
		{
			int32 prev_line_dist = lo_calc_distance_to_bottom_of_prev_line(state);

			move_size = FEUNITS_Y((int32)top_margin->value, context) - prev_line_dist;

#ifndef ALLOW_NEG_MARGINS
			if(move_size > 0)
#endif
				state->y += move_size;
		}

		STYLESTRUCT_FreeSSNumber(style_struct, top_margin);

#ifdef ALLOW_NEG_MARGINS
			if(state->y < 0)
				state->y = 0;
#endif

		/* now add top padding if there is any and we arn't using table padding */
		if(!use_table_for_box)
		{
			SS_Number *top_padding    = STYLESTRUCT_GetNumber(style_struct, TOPPADDING_STYLE);
			LO_AdjustSSUnits(top_padding, TOPPADDING_STYLE, context, state);
	
			if(top_padding && top_padding->value > 0)
			{
				move_size = (int32)top_padding->value;
        		state->y += FEUNITS_Y(move_size, context);
			}

			STYLESTRUCT_FreeSSNumber(style_struct, top_padding);
		}
		
	}

	/* the only way an unknown tag can get in here is if
	 * its a special relayout place holder for table styles
	 * we never want to redo the table in that case
	 */
	if(use_table_for_box
		&& !is_table_relayout_begin_dummy_tag)
	{
		/* begin a table */
		lo_DocState *prev_state;
		LO_TextAttr tmp_attr, *attr;

		SS_Number *top_padding, *bottom_padding;

		/* begin table attributes */
        char *align_attr= align_value;
        char *border_attr=NULL;
        char *border_top_attr=NULL;
        char *border_bottom_attr=NULL;
        char *border_left_attr=NULL;
        char *border_right_attr=NULL;
        char *border_color_attr=border_color_value;
        char *border_style_attr=border_style_value;
        char *vspace_attr=NULL;
        char *hspace_attr=NULL;
        char *bgcolor_attr=NULL;
        char *width_attr= NULL;
        char *height_attr=NULL;
        char *cellpad_attr="0"; /* default to zero */
        char *toppad_attr=NULL;
        char *bottompad_attr=NULL;
        char *leftpad_attr=NULL;
        char *rightpad_attr=NULL;
        char *cellspace_attr=NULL;
        char *cols_attr=NULL;

		/* begin table row attributes */
		char * row_valign_attr= NULL;
        char * row_halign_attr= NULL;
        
		/* begin table data attributes */
        char * colspan_attr= NULL;
        char * rowspan_attr= NULL;
        char * nowrap_attr= NULL;
		char * cell_bgcolor_attr= NULL;
        char * cell_bgimage_attr=lo_ParseStyleSheetURL(bgimage_value);
        char * cell_valign_attr= NULL;
        char * cell_halign_attr= NULL;
        char * cell_width_attr= NULL;
        char * cell_height_attr= NULL;
        Bool is_a_header = FALSE;

		/* the right_margin setting doesn't really effect tables so we need to use
		 * the table width setting to get the desired effect
         *
         * if right_margin == 5000 then the margin is really unknown and we cant
         * do correct margin calculations
		 */
        if(text_width || (state->right_margin != 5000 && (right_margin || left_margin)))
		{
			int32 table_width = state->right_margin - state->left_margin;
			
			if(text_width && table_width > text_width->value)
				table_width = (int32)text_width->value;

			width_attr = PR_smprintf("%ld", table_width);
		}

		if(left_padding)
			leftpad_attr = PR_smprintf("%ld", (int32)left_padding->value);
		if(right_padding)
			rightpad_attr = PR_smprintf("%ld", (int32)right_padding->value);

		/* top and bottom padding values */
		top_padding    = STYLESTRUCT_GetNumber(style_struct, TOPPADDING_STYLE);
	    LO_AdjustSSUnits(top_padding, TOPPADDING_STYLE, context, state);

		if(top_padding)
		{
			toppad_attr = PR_smprintf("%ld", (int32)top_padding->value);
			STYLESTRUCT_FreeSSNumber(style_struct, top_padding);
		}

		bottom_padding    = STYLESTRUCT_GetNumber(style_struct, BOTTOMPADDING_STYLE);
	    LO_AdjustSSUnits(bottom_padding, BOTTOMPADDING_STYLE, context, state);
		
		if(bottom_padding)
		{
			bottompad_attr = PR_smprintf("%ld", (int32)bottom_padding->value);
			STYLESTRUCT_FreeSSNumber(style_struct, bottom_padding);
		}

		if(borderwidth_value)
			border_attr = PR_smprintf("%ld", (int32)borderwidth_value->value);
		if(bordertopwidth_value)
			border_top_attr = PR_smprintf("%ld", (int32)bordertopwidth_value->value);
		if(borderbottomwidth_value)
			border_bottom_attr = PR_smprintf("%ld", (int32)borderbottomwidth_value->value);
		if(borderleftwidth_value)
			border_left_attr = PR_smprintf("%ld", (int32)borderleftwidth_value->value);
		if(borderrightwidth_value)
			border_right_attr = PR_smprintf("%ld", (int32)borderrightwidth_value->value);

        		
    	bgcolor_attr = STYLESTRUCT_GetString(style_struct, BG_COLOR_STYLE);
        if(bgcolor_attr && !strcasecomp(bgcolor_attr, "transparent"))
        {
       	    XP_FREEIF(bgcolor_attr);
		    bgcolor_attr = NULL;
        }
	
		/* mark that we are in a table */
		STYLESTRUCT_SetString(style_struct, STYLE_NEED_TO_POP_TABLE, "1", 0);

		lo_BeginTableAttributes(context,
                        state,
                        align_attr,
                        border_attr,
                        border_top_attr,
                        border_bottom_attr,
                        border_left_attr,
                        border_right_attr,
                        border_color_attr,
                        border_style_attr,
                        vspace_attr,
                        hspace_attr,
                        bgcolor_attr,
                        NULL,   /* Backdrop URL */
                        width_attr,
                        height_attr,
                        cellpad_attr,
                        toppad_attr,
                        bottompad_attr,
						leftpad_attr,
						rightpad_attr,
                        cellspace_attr,
                        cols_attr);

		XP_FREEIF(width_attr);

		if(state->sub_state)
			state = state->sub_state;

	    if(state->current_table)
		{

			/* change the vertical alignment to top so that top and bottom 
			 * margins work correctly.  "Center" seems to be the default
			 */
			row_valign_attr = strdup("TOP");

			/* begin a table row */
			lo_BeginTableRowAttributes(context,
                            			state,
                            			state->current_table,
                            			bgcolor_attr,
                                        NULL, /* Backdrop URL */
                            			row_valign_attr,
                            			row_halign_attr);

			XP_FREEIF(row_valign_attr);

			if(state->sub_state)
				state = state->sub_state;

			if(state->current_table->row_ptr)
			{
				/* now begin the table data */
				lo_BeginTableCellAttributes(context,
                            				state,
                            				state->current_table,
                            				colspan_attr,
                            				rowspan_attr,
                            				nowrap_attr,
                            				cell_bgcolor_attr,
                                            cell_bgimage_attr, /* Backdrop URL */
                                            LO_TILE_BOTH, /* Backdrop tiling mode */
                            				cell_valign_attr,
                            				cell_halign_attr,
                            				cell_width_attr,
                            				cell_height_attr,
                            				is_a_header,
											FALSE);  /* no cell borders */
			}
		}

		prev_state = state;
		if(state->sub_state)
			state = state->sub_state;

        XP_FREEIF(bgcolor_attr);

		/* inherit the font stack from the prev state to allow proper inheritance
		 * rules  -- as far as I know we don't need to inherit any other props now
		 */

		lo_CopyTextAttr(prev_state->font_stack->text_attr, &tmp_attr);
		attr = lo_FetchTextAttr(state, &tmp_attr);
        attr->no_background = TRUE; /* don't inherit text background colors */
        
        /* don't use the tag type since the A tag is treated special 
         * and we don't want that special treatment
         */
		lo_PushFont(state, P_UNKNOWN, attr);  

        if(tag->type == P_PARAGRAPH)
            state->in_paragraph = TRUE;

		/* after starting a table we must reset all the font properties */
        lo_SetStyleSheetFontProperties(context, state, style_struct, tag, FALSE);

	}

	/* add text indent to the current X position */
	if(text_indent)
		state->x += (int32)text_indent->value;

	XP_FREEIF(align_value);
    XP_FREEIF(bgimage_value);
	XP_FREEIF(border_style_value);
	XP_FREEIF(border_color_value);
	
	STYLESTRUCT_FreeSSNumber(style_struct, right_margin);
	STYLESTRUCT_FreeSSNumber(style_struct, left_margin);

	STYLESTRUCT_FreeSSNumber(style_struct, right_padding);
	STYLESTRUCT_FreeSSNumber(style_struct, left_padding);

	STYLESTRUCT_FreeSSNumber(style_struct, text_indent);
	STYLESTRUCT_FreeSSNumber(style_struct, text_width);

	STYLESTRUCT_FreeSSNumber(style_struct, bordertopwidth_value);
	STYLESTRUCT_FreeSSNumber(style_struct, borderbottomwidth_value);
	STYLESTRUCT_FreeSSNumber(style_struct, borderrightwidth_value);
	STYLESTRUCT_FreeSSNumber(style_struct, borderleftwidth_value);
}

PRIVATE
void
lo_SetStyleSheetRandomProperties(MWContext *context, 
								lo_DocState *state, 
								StyleStruct *style_struct, 
								PA_Tag *tag)
{
	char * property;

	property = STYLESTRUCT_GetString(style_struct, WHITESPACE_STYLE);
	if (property)
	{
		if(!strcasecomp(property, "PRE"))
		{
			state->preformatted = PRE_TEXT_YES;
			FE_BeginPreSection(context);
			STYLESTRUCT_SetString(style_struct, STYLE_NEED_TO_POP_PRE, "1", 0);
		}
		else if(!strcasecomp(property, "NORMAL"))
		{
			if(state->preformatted == PRE_TEXT_YES)
			{
				state->preformatted = PRE_TEXT_NO;
				FE_EndPreSection(context);
				STYLESTRUCT_SetString(style_struct, 
									  STYLE_NEED_TO_RESET_PRE, 
									  "1",
									  0);
			}
		}
		else if(!strcasecomp(property, "NOWRAP"))
		{
			/* not currently supported */
		}
		XP_FREE(property);
	}

	property = STYLESTRUCT_GetString(style_struct, CLEAR_STYLE);
	if (property)
	{
		if (!strcasecomp(property, "left"))
		{
			lo_HardLineBreak(context, state, FALSE);
			lo_ClearToLeftMargin(context, state);
		}
		else if (!strcasecomp(property, "right"))
		{
			lo_HardLineBreak(context, state, FALSE);
			lo_ClearToRightMargin(context, state);
		}
		else if (!strcasecomp(property, "both"))
		{
			lo_HardLineBreak(context, state, FALSE);
			lo_ClearToBothMargins(context, state);
		}
		/* note that "none" means to not clear or BR at all */

		/*
		 * Reset the margins properly in case
		 * we are inside a list.
		 */
		lo_FindLineMargins(context, state, TRUE);
		state->x = state->left_margin;
		XP_FREE(property);
	}
}


PRIVATE
void
lo_SetStyleSheetProperties(MWContext *context, 
							StyleStruct *style_struct, 
							PA_Tag *tag)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;

	if(tag->type == P_TEXT || !style_struct)
		return;
	
	/* get a fresh state since it may get free'd by
	 * previous table code
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	state = lo_TopSubState(top_state);

	if(!state)
		return;

	/* ignore uknown tags unless we are in the relayout phase */
	if(tag->type == P_UNKNOWN && state->in_relayout == FALSE)
		return;

	/* if we are hiding content skip applying styles */
	if(state->hide_content)
		return;

	/* optimization!
	 *
 	 * if there we know that there are no styles for this tag
	 * return immediately
	 */
	if(STYLESTRUCT_Count(style_struct) == 0)
		return;

	/* check for display: none 
	 * if set then we are ignoring tags and text for the duration
	 * of this tag span 
	 */
	if(LO_CheckForContentHiding(state))
	{
		state->hide_content = TRUE;
		STYLESTRUCT_SetString(style_struct, 	
							  STYLE_NEED_TO_POP_CONTENT_HIDING, 	
							  "1", 	
							  0);
		/* don't apply any more properties */
		return;
	}
	
	lo_SetStyleSheetFontProperties(context, state, style_struct, tag, TRUE);

    /* if we get in here and the tag->type is UKNOWN then this must
	 * be a dummy tag inserted for the sake of table relayout passes
	 * don't open layers in this case because we would be creating
     * two layers instead of just the one.
	 */
	if(tag->type != P_UNKNOWN)
    	lo_SetStyleSheetLayerProperties(context, state, style_struct, tag);

	lo_SetStyleSheetBoxProperties (context, state, style_struct, tag);

	/* BoxProperties may force a table.  If that is the case then
	 * we need to get the new state
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	state = lo_TopSubState(top_state);

	if(!state)
		return;

	lo_SetStyleSheetRandomProperties(context, state, style_struct, tag);

}

/* return TRUE if the tag type is an empty tag. (not a container tag)
 */
MODULE_PRIVATE /* used in layblock.c */
Bool
lo_IsEmptyTag(TagType type)
{
    if(type == P_OPTION
       || type == P_INPUT
       || type == P_INDEX
       || type == P_HRULE
       || type == P_HYPE
       || type == P_META
       || type == P_COLORMAP
       || type == P_LINEBREAK
       || type == P_SPACER
       || type == P_WORDBREAK
       || type == P_PARAM
       || type == P_IMAGE
       || type == P_NEW_IMAGE
       || type == P_EMBED
       || type == P_KEYGEN
       || type == P_JAVA_APPLET
       || type == P_LIST_ITEM
       || type == P_BASEFONT
       || type == P_AREA
       || type == P_DESC_TITLE
       || type == P_NSDT
       || type == P_DESC_TEXT
       || type == P_BASE)
    {
        return TRUE;
    }

    return FALSE;

}

static void lo_ProcessFontTag( lo_DocState *state, PA_Tag *tag, int32 fontSpecifier, int32 attrSpecifier )
{
	LO_TextAttr tmp_attr;

	if (tag->is_end == FALSE)
	{
		LO_TextAttr *old_attr;
		LO_TextAttr *attr;

		old_attr = state->font_stack->text_attr;
		lo_CopyTextAttr(old_attr, &tmp_attr);
		tmp_attr.fontmask |= fontSpecifier;
		tmp_attr.attrmask |= attrSpecifier;
		attr = lo_FetchTextAttr(state, &tmp_attr);

		lo_PushFont(state, tag->type, attr);
	}
	else
	{
		LO_TextAttr *attr;

		attr = lo_PopFont(state, tag->type);
	}
}

/*************************************
 * Function: lo_LayoutTag
 *
 * Description: This function begins the process of laying
 *	out a single MDL tag.
 *
 * Params: Window context and document state, and the tag to be laid out.
 *
 * Returns: Nothing
 *************************************/
void
lo_LayoutTag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	char *tptr;
	char *tptr2;
#ifdef OJI
	char* javaPlugin;
#endif
	LO_TextAttr tmp_attr;
	StyleStruct *style_struct=NULL;
	XP_Bool has_ss_bottom_margin=FALSE;
    XP_Bool started_in_head=FALSE;
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);

	XP_ASSERT(state);

    if(state->top_state)
        started_in_head = state->top_state->in_head;

#ifdef LOCAL_DEBUG
XP_TRACE(("lo_LayoutTag(%d)\n", tag->type));
#endif /* LOCAL_DEBUG */

	/* 
	 * see if there are any &{} constructs for this tag.  If so,
	 *   convert them now, if we can't convert them (since we couldn't
	 *   get the mocha lock, for example) block layout and try to do 
	 *   this tag convertion again later
	 */
	/*
	 * Don't expand these inside XMP and PLAINTEXT or when the text
	 *   is coming from a document.write()
	 */
	if (state->allow_amp_escapes != FALSE && tag->type != P_TEXT &&
	    (state->top_state->input_write_level == 0 ||
             state->top_state->tag_from_inline_stream == TRUE))
	{
		if (lo_ConvertMochaEntities(context, state, tag) == FALSE) {
			state->top_state->wedged_on_mocha = TRUE;
			lo_CloneTagAndBlockLayout(context, state, tag);
			return;
		} 
	}

	lo_PreLayoutTag(context, state, tag);
	if ( state->top_state->out_of_memory )
		return;
		
	/* implicitly pop tags before we push a new one on the stack 
	 * the call to this may alter the state variable due to
	 * a table closure
	 */
	LO_ImplicitPop(context, &state, tag);

	/* Call into the style sheet code to let it know
	 * that we are starting a new tag
	 *
	 * Since we need to get the id and class out of
	 * the tag->data call an LO function first 
	 */
	if(!tag->is_end)
	{
		PushTagStatus push_status;
		
 		push_status = LO_PushTagOnStyleStack(context, state, tag);

		if(push_status == PUSH_TAG_SUCCESS) {
			if(state->top_state->style_stack)
				style_struct = STYLESTACK_GetStyleByIndex(state->top_state->style_stack, 0);
			else
				style_struct = NULL;
		}
		else if(push_status == PUSH_TAG_BLOCKED)
			/* layout has been locked */
			return;
		else
			style_struct = NULL;  /* push error */
	} 
	else
	{
		/* get has_bottom_margin flag for use in end P */
		char *prop;

        /* don't pop on unknown or empty tags.
         * we have already pop'd empty tags.
         */
		if((tag->type != P_UNKNOWN || state->in_relayout)
            && !lo_IsEmptyTag(tag->type))
		{
			if(state->top_state->style_stack)
				style_struct = STYLESTACK_GetStyleByIndex(state->top_state->style_stack, 0);
			else 
				style_struct = NULL;

			if(style_struct
		   	&& (prop = STYLESTRUCT_GetString(style_struct, BOTTOMMARGIN_STYLE)) != NULL)
			{
				has_ss_bottom_margin = TRUE;
				XP_FREE(prop);
			}
			
			/* pop the previous style tag, since this is an end tag */
			LO_PopStyleTag(context, &state, tag);

			/* if we have a pointer to the style struct it will
			 * be dangling now since we just pop'd and detroyed the
			 * top style struct.
			 */
			style_struct = NULL;
		}
	}

	if(tag->type != P_TEXT && tag->type != P_UNKNOWN)
	{
		XP_ASSERT(state->top_state);
		if(state->top_state)
			state->top_state->tag_count++;
	}

	LO_LockLayout();

	if(!tag->is_end && lo_IsEmptyTag(tag->type))
	{
		lo_SetStyleSheetProperties(context, style_struct, tag);
	}
	
	switch(tag->type)
	  {
		case P_TEXT:
			if(!state->hide_content)
				lo_process_text_tag(context, state, tag);
			break;

		/*
		 * Title text is special, it is not displayed, just
		 * captured and sent to the front end.
		 */
		case P_TITLE:
			lo_process_title_tag(context, state, tag);
			break;

		/*
		 * Certificate text is special, it is not displayed,
		 * but decoded and saved for later reference.
		 */
		case P_CERTIFICATE:
#ifdef HTML_CERTIFICATE_SUPPORT
			lo_process_certificate_tag(context, state, tag);
#endif
			break;

		/*
		 * All these tags just flip the fixed width font
		 * bit, and push a new font on the font stack.
		 */
		case P_FIXED:
		case P_CODE:
		case P_SAMPLE:
		case P_KEYBOARD:
			lo_ProcessFontTag( state, tag, LO_FONT_FIXED, 0);
			break;

#ifdef EDITOR
		process_server:
		case P_SERVER:
			if (tag->is_end == FALSE)
			{
				LO_TextAttr *old_attr;
				LO_TextAttr *attr;

				state->preformatted = PRE_TEXT_YES;
				FE_BeginPreSection(context);

				old_attr = state->font_stack->text_attr;
				lo_CopyTextAttr(old_attr, &tmp_attr);
				tmp_attr.fontmask |= LO_FONT_FIXED;
				if( tag->type == P_SCRIPT ){
					tmp_attr.fg.green = 0;
				    tmp_attr.fg.red = 0xff;
				    tmp_attr.fg.blue = 0;
				}
				else {
					tmp_attr.fg.green = 0;
				    tmp_attr.fg.red = 0;
				    tmp_attr.fg.blue = 0xff;
				}
				tmp_attr.size = 3;
				attr = lo_FetchTextAttr(state, &tmp_attr);

				lo_PushFont(state, tag->type, attr);
			}
			else
			{
				LO_TextAttr *attr;

				attr = lo_PopFont(state, tag->type);
				state->preformatted = PRE_TEXT_NO;
				FE_EndPreSection(context);
			}

			break;
#endif


		/*
		 * All these tags just flip the bold font
		 * bit, and push a new font on the font stack.
		 */
		case P_BOLD:
		case P_STRONG:
			lo_ProcessFontTag( state, tag, LO_FONT_BOLD, 0);
			break;

		/*
		 * All these tags just flip the italic font
		 * bit, and push a new font on the font stack.
		 */
		case P_ITALIC:
		case P_EMPHASIZED:
		case P_VARIABLE:
		case P_CITATION:
			lo_ProcessFontTag( state, tag, LO_FONT_ITALIC, 0);
			break;

		/*
		 * All headers force whitespace, then set the bold
		 * font bit and unset the fixed and/or italic if set.
		 * In addition, they set the size attribute of the
		 * new font they will push, like <font size=num>
		 * Sizes are:
		 * 	H1 -> 6
		 * 	H2 -> 5
		 * 	H3 -> 4
		 * 	H4 -> 3
		 * 	H5 -> 2
		 * 	H6 -> 1
		 */
		case P_HEADER_1:
			lo_process_header_tag(context, state, tag, 6);
			break;
		case P_HEADER_2:
			lo_process_header_tag(context, state, tag, 5);
			break;
		case P_HEADER_3:
			lo_process_header_tag(context, state, tag, 4);
			break;
		case P_HEADER_4:
			lo_process_header_tag(context, state, tag, 3);
			break;
		case P_HEADER_5:
			lo_process_header_tag(context, state, tag, 2);
			break;
		case P_HEADER_6:
			lo_process_header_tag(context, state, tag, 1);
			break;

		/*
		 * Description titles are just normal,
		 * unless value > 1 and then they are outdented
		 * the width of one list indention.
		 */
		case P_NSDT:
		case P_DESC_TITLE:
			/*
			 * The start of a list infers the end of
			 * the HEAD section of the HTML and starts the BODY
			 */
			state->top_state->in_head = FALSE;
			state->top_state->in_body = TRUE;

			if (state->in_paragraph != FALSE)
			{
				lo_CloseParagraph(context, &state, tag, 2);
			}
			if (tag->is_end == FALSE)
			{
				LO_DescTitleStruct *title = (LO_DescTitleStruct*)lo_NewElement(context, state, LO_DESCTITLE, NULL, 0);

				XP_ASSERT(title);
				if (!title) 
				{
				    LO_UnlockLayout();
				    return;
				}

				title->lo_any.type = LO_DESCTITLE;
				title->lo_any.ele_id = NEXT_ELEMENT;
				
				title->lo_any.x = state->x;
				title->lo_any.y = state->y;
				title->lo_any.x_offset = 0;
				title->lo_any.y_offset = 0;
				title->lo_any.width = 0;
				title->lo_any.height = 0;
				title->lo_any.line_height = 0;

				lo_AppendToLineList(context, state, (LO_Element*)title, 0);

				lo_ProcessDescTitleElement(context, state, title, FALSE);
			}
			break;

		/*
		 * Description text, is just normal text, but since there
		 * is no ending <dd> tag, we need to put the
		 * linebreak from the previous line in here.
		 * Compact lists may not linebreak since they try to
		 * put description text on the same line as title
		 * text if there is room.
		 * If value == 1 then we need to indent for the description
		 * text, if it is > 1 then we are already indented.
		 */
		case P_DESC_TEXT:
			/*
			 * The start of a list infers the end of
			 * the HEAD section of the HTML and starts the BODY
			 */
			state->top_state->in_head = FALSE;
			state->top_state->in_body = TRUE;

			if (state->in_paragraph != FALSE)
			{
				lo_CloseParagraph(context, &state, tag, 2);
			}
			if (tag->is_end == FALSE)
			{
				LO_DescTextStruct *text = (LO_DescTextStruct*)lo_NewElement(context, state, LO_DESCTEXT, NULL, 0);

				XP_ASSERT(text);
				if (!text) 
				{
				    LO_UnlockLayout();
				    return;
				}

				text->lo_any.type = LO_DESCTEXT;
				text->lo_any.ele_id = NEXT_ELEMENT;
				
				text->lo_any.x = state->x;
				text->lo_any.y = state->y;
				text->lo_any.x_offset = 0;
				text->lo_any.y_offset = 0;
				text->lo_any.width = 0;
				text->lo_any.height = 0;
				text->lo_any.line_height = 0;

				lo_AppendToLineList(context, state, (LO_Element*)text, 0);

				lo_ProcessDescTextElement(context, state, text, FALSE);
			}
			break;
		/*
		 * A new list level, but don't move the
		 * left margin in until you need to.
		 * val is overloaded to tell us if we are
		 * not indented (val=1) or indented (val > 1).
		 * Push a new element on the list stack.
		 */
		case P_DESC_LIST:
			/*
			 * The start of a list infers the end of
			 * the HEAD section of the HTML and starts the BODY
			 */
			state->top_state->in_head = FALSE;
			state->top_state->in_body = TRUE;

			if (tag->is_end == FALSE)
			{
				PA_Block buff;
				char * t_ptr;

				buff = lo_FetchParamValue(context, tag, PARAM_COMPACT);
				
				PA_LOCK(t_ptr, char *, buff);

				lo_setup_list(state,
							  context,
							  tag,
							  NULL,
							  NULL,
							  t_ptr);
				
				PA_UNLOCK(buff);

				if (buff) PA_FREE(buff);
			}
			else /* tag->is_end is TRUE */
			{
				lo_TeardownList(context, state, tag);
			}

			break;
		/*
		 * Move the left margin in another indent step.
		 * Push a new element on the list stack.
		 */
		case P_NUM_LIST:
		case P_UNUM_LIST:
		case P_MENU:
		case P_DIRECTORY:
    /*    case P_MQUOTE:  mquote tag disabled, replaced by  <blockquote mail> */
			/*
			 * The start of a list infers the end of
			 * the HEAD section of the HTML and starts the BODY
			 */
			state->top_state->in_head = FALSE;
			state->top_state->in_body = TRUE;

			if (tag->is_end == FALSE)
			{
				char *bullet_type;
				char *bullet_start;
				char *compact_attr;

				bullet_type = (char*)lo_FetchParamValue(context, 
														tag, 
														PARAM_TYPE);
				
				bullet_start = (char*)lo_FetchParamValue(context, 
														tag, 
														PARAM_START);

				compact_attr = (char*)lo_FetchParamValue(context, 
														tag, 
														PARAM_COMPACT);

				lo_setup_list(state, 
							  context,
							  tag,
							  bullet_type,
							  bullet_start,
							  compact_attr);

				XP_FREEIF(bullet_type);
				XP_FREEIF(bullet_start);
				XP_FREEIF(compact_attr);
			}
			/* tag->is_end is TRUE */
			else {
				lo_TeardownList(context, state, tag);
			}
			break;

		/*
		 * Complex because we have to place one of many types
		 * of bullet here, or one of several types of numbers.
		 */
		case P_LIST_ITEM:
			/*
			 * The start of a list infers the end of
			 * the HEAD section of the HTML and starts the BODY
			 */
			state->top_state->in_head = FALSE;
			state->top_state->in_body = TRUE;

			if (state->in_paragraph != FALSE)
			{
				lo_CloseParagraph(context, &state, tag, 2);
			}
			if (tag->is_end == FALSE && !state->hide_content)
			{
				int type;
				int bullet_type;
				PA_Block buff;

				/*
				 * Reset fake linefeed state used to
				 * fake out headers in lists.
				 */
				if (state->at_begin_line == FALSE)
				{
					state->linefeed_state = 0;
				}

				lo_SetLineBreakState(context, state, FALSE, LO_LINEFEED_BREAK_HARD, 1, FALSE);
				/*
				 * Artificially setting state to 2 here
				 * allows us to put headers on list items
				 * even if they aren't double spaced.
				 */
				state->linefeed_state = 2;

				type = state->list_stack->bullet_type;
				buff = lo_FetchParamValue(context, tag, PARAM_TYPE);
				if ((buff != NULL)&&
					(state->list_stack->type == P_NUM_LIST))
				{
					char *type_str;

					PA_LOCK(type_str, char *, buff);
					if (*type_str == 'A')
					{
						type = BULLET_ALPHA_L;
					}
					else if (*type_str == 'a')
					{
						type = BULLET_ALPHA_S;
					}
					else if (*type_str == 'I')
					{
						type = BULLET_NUM_L_ROMAN;
					}
					else if (*type_str == 'i')
					{
						type = BULLET_NUM_S_ROMAN;
					}
					else
					{
						type = BULLET_NUM;
					}
					PA_UNLOCK(buff);
				}
				else if ((buff != NULL)&&
					(state->list_stack->type !=P_DESC_LIST))
				{
					char *type_str;

					PA_LOCK(type_str, char *, buff);
					if (pa_TagEqual("round", type_str))
					{
						type = BULLET_ROUND;
					}
					else if (pa_TagEqual("circle",type_str))
					{
						type = BULLET_ROUND;
					}
					else if (pa_TagEqual("square",type_str))
					{
						type = BULLET_SQUARE;
					}
					else
					{
						type = BULLET_BASIC;
					}
					PA_UNLOCK(buff);
				}
				state->list_stack->bullet_type = type;

				if (buff != NULL) 
				{
					PA_FREE(buff);
				}

				buff = lo_FetchParamValue(context, tag, PARAM_VALUE);
				if (buff != NULL)
				{
					int32 val;
					char *val_str;

					PA_LOCK(val_str, char *, buff);
					val = XP_ATOI(val_str);
					if (val < 1)
					{
						val = 1;
					}
					PA_UNLOCK(buff);
					state->list_stack->value = val;

					PA_FREE(buff);
				}

				bullet_type = state->list_stack->bullet_type;
				if ((bullet_type == BULLET_NUM)||
				    (bullet_type == BULLET_ALPHA_L)||
				    (bullet_type == BULLET_ALPHA_S)||
				    (bullet_type == BULLET_NUM_L_ROMAN)||
				    (bullet_type == BULLET_NUM_S_ROMAN))
				{
					lo_PlaceBulletStr(context, state);
					state->list_stack->value++;
				}
				else
				{
					lo_PlaceBullet(context, state);
				}
			}
			break;

		/*
		 * Another font attribute, another font on the font stack
		 */
		case P_BLINK:
            /*
             * Blink is ignored in the editor.
             */
            if ( EDT_IS_EDITOR(context) ) {
                break;
            }
			if (tag->is_end == FALSE)
			{
				LO_TextAttr *old_attr;
				LO_TextAttr *attr;

				old_attr = state->font_stack->text_attr;
				lo_CopyTextAttr(old_attr, &tmp_attr);
				tmp_attr.attrmask |= LO_ATTR_BLINK;
				attr = lo_FetchTextAttr(state, &tmp_attr);

				lo_PushFont(state, tag->type, attr);
			}
			else
			{
				LO_TextAttr *attr;

				attr = lo_PopFont(state, tag->type);
			}
			break;

		/*
		 * Another font attribute, another font on the font stack
		 * Because of spec mutation, two tags do the same thing.
		 * <s> and <strike> are the same.
		 */
		case P_STRIKEOUT:
		case P_STRIKE:
			lo_ProcessFontTag( state, tag, 0, LO_ATTR_STRIKEOUT);			
			break;

		case P_SPELL:
			lo_ProcessFontTag( state, tag, 0, LO_ATTR_SPELL);
			break;


		case P_INLINEINPUT:
			lo_ProcessFontTag( state, tag, 0, LO_ATTR_INLINEINPUT);			
			break;


		case P_INLINEINPUTTHICK:
			lo_ProcessFontTag( state, tag, 0, LO_ATTR_INLINEINPUTTHICK);						
			break;


		case P_INLINEINPUTDOTTED:
			lo_ProcessFontTag( state, tag, 0, LO_ATTR_INLINEINPUTDOTTED);
			break;

		/*
		 * Another font attribute, another font on the font stack
		 */
		case P_UNDERLINE:
			lo_ProcessFontTag( state, tag, 0, LO_ATTR_UNDERLINE);			
			break;

		/*
		 * Center all following lines.  Forces a single
		 * line break to start and end the new centered lines.
		 */
		case P_CENTER:
			lo_process_center_tag(context, state, tag);
			break;

		/*
		 * For plaintext there is no endtag, so this is
		 * really the last tag will we be parsing if this
		 * is a plaintext tag.
		 *
		 * Difference between these three tags are
		 * all based on what things the parser allows to
		 * be tags while inside them.
		 */
		case P_PREFORMAT:
		case P_PLAIN_PIECE:
		case P_PLAIN_TEXT:
			if (state->in_paragraph != FALSE)
			{
				lo_CloseParagraph(context, &state, tag, 2);
			}
			if (tag->is_end == FALSE)
			{
				LO_TextAttr *old_attr;
				LO_TextAttr *attr;
				PA_Block buff;

                lo_SetLineBreakState(context, state, FALSE, LO_LINEFEED_BREAK_HARD,
                                     2, FALSE);

				if ((tag->type == P_PLAIN_PIECE)||
					(tag->type == P_PLAIN_TEXT))
				{
					state->allow_amp_escapes = FALSE;
				}

				state->preformatted = PRE_TEXT_YES;
				FE_BeginPreSection(context);

				/*
				 * Special WRAP attribute of the PRE tag
				 * to make wrapped pre sections.
				 */
				if (tag->type == P_PREFORMAT)
				{
					buff = lo_FetchParamValue(context, tag,
							PARAM_WRAP);
					if (buff != NULL)
					{
						state->preformatted =
							PRE_TEXT_WRAP;
						PA_FREE(buff);
					}

					/*
					 * Wrap at a certain column.
					 */
					buff = lo_FetchParamValue(context, tag,
							PARAM_COLS);
					if (buff != NULL)
					{
						char * str;
						int32 val;
						
						PA_LOCK(str, char *, buff);
						val = XP_ATOI(str);
						if (val < 0)
						{
							val = 0;
						}

						state->preformat_cols = val;
						state->preformatted =
							PRE_TEXT_COLS;
						PA_FREE(buff);
					}
					
					/*
					 * Special TABSTOP attribute of the PRE tag
					 * to set the tab stop width.
					 */
					buff = lo_FetchParamValue(context, tag,
							PARAM_TABSTOP);
					if (buff != NULL)
					{
						char * str;
						int32 val;
						
						PA_LOCK(str, char *, buff);
						val = XP_ATOI(str);
						
						/*
						 * Note that I made a tabwidth of 0 be an
						 * illegal value. lo_PreformatedText divides
						 * with tab_stop.
						 */
						if ( val <= 0 )
						{
							val = DEF_TAB_WIDTH;
						}
						state->tab_stop = val;
						PA_FREE(buff);
					}
				}

				old_attr = state->font_stack->text_attr;
				lo_CopyTextAttr(old_attr, &tmp_attr);

				/*
				 * Allow <PRE VARIABLE> to create a variable
				 * width font preformatted section.  This
				 * is only to be used internally for 
				 * mail and news in Netscape 2.0 and greater.
				 */
				buff = lo_FetchParamValue(context, tag, PARAM_VARIABLE);
				if ((tag->type == P_PREFORMAT)&&(buff != NULL))
				{
					tmp_attr.fontmask &= (~(LO_FONT_FIXED));
				}
				else
				{
					tmp_attr.fontmask |= LO_FONT_FIXED;
				}
				/*
				 * If VARIABLE attribute was there, free
				 * the value buffer.
				 */
				if (buff != NULL)
				{
					PA_FREE(buff);
				}

				attr = lo_FetchTextAttr(state, &tmp_attr);

				lo_PushFont(state, tag->type, attr);
			}
			else
			{
				LO_TextAttr *attr;

				attr = lo_PopFont(state, tag->type);

				state->preformat_cols = 0;
				state->preformatted = PRE_TEXT_NO;
				state->allow_amp_escapes = TRUE;
                lo_SetLineBreakState(context, state, FALSE, LO_LINEFEED_BREAK_HARD,
                                     2, FALSE);
				FE_EndPreSection(context);
			}
			break;

		/*
		 * Historic text type.  Paragraph break, font size change
		 * and font set to fixed.  Was supposed to support
		 * lineprinter listings if you can believe that!
		 */
		case P_LISTING_TEXT:
			if (state->in_paragraph != FALSE)
			{
				lo_CloseParagraph(context, &state, tag, 2);
			}
			if (tag->is_end == FALSE)
			{
				LO_TextAttr *old_attr;
				LO_TextAttr *attr;

				lo_SetSoftLineBreakState(context, state, FALSE, 2);

				state->preformatted = PRE_TEXT_YES;
				FE_BeginPreSection(context);

				old_attr = state->font_stack->text_attr;
				lo_CopyTextAttr(old_attr, &tmp_attr);
				tmp_attr.fontmask |= LO_FONT_FIXED;
				tmp_attr.size = DEFAULT_BASE_FONT_SIZE - 1;
				attr = lo_FetchTextAttr(state, &tmp_attr);

				lo_PushFont(state, tag->type, attr);
			}
			else
			{
				LO_TextAttr *attr;

				attr = lo_PopFont(state, tag->type);

				state->preformatted = PRE_TEXT_NO;
				lo_SetSoftLineBreakState(context, state, FALSE, 2);
				FE_EndPreSection(context);
			}
			break;

		/*
		 * Another historic tag.  Break a line, and switch to italic.
		 */
		case P_ADDRESS:
			if (state->in_paragraph != FALSE)
			{
				lo_CloseParagraph(context, &state, tag, 2);
			}
			if (tag->is_end == FALSE)
			{
				LO_TextAttr *old_attr;
				LO_TextAttr *attr;

				lo_SetSoftLineBreakState(context, state, FALSE, 1);

				old_attr = state->font_stack->text_attr;
				lo_CopyTextAttr(old_attr, &tmp_attr);
				tmp_attr.fontmask |= LO_FONT_ITALIC;
				attr = lo_FetchTextAttr(state, &tmp_attr);

				lo_PushFont(state, tag->type, attr);
			}
			else
			{
				LO_TextAttr *attr;

				attr = lo_PopFont(state, tag->type);

				lo_SetSoftLineBreakState(context, state, FALSE, 1);
			}
			break;

		/*
		 * Subscript.  Move down so the middle of the ascent
		 * of the new text aligns with the baseline of the old.
		 */
		case P_SUB:
			if (state->font_stack != NULL)
			{
				PA_Block buff;
				char *str;
				LO_TextStruct tmp_text;
				LO_TextInfo text_info;

				if (tag->is_end == FALSE)
				{
					LO_TextAttr *old_attr;
					LO_TextAttr *attr;
					intn new_size;

					old_attr = state->font_stack->text_attr;
					lo_CopyTextAttr(old_attr, &tmp_attr);
					new_size = LO_ChangeFontSize(tmp_attr.size,
								"-1");
					tmp_attr.size = new_size;
					attr = lo_FetchTextAttr(state,
						&tmp_attr);

					lo_PushFont(state, tag->type, attr);
				}

				/*
				 * All this work is to get the text_info
				 * filled in for the current font in the font
				 * stack. Yuck, there must be a better way.
				 */
				memset (&tmp_text, 0, sizeof (tmp_text));
				buff = PA_ALLOC(1);
				if (buff == NULL)
				{
					state->top_state->out_of_memory = TRUE;
					LO_UnlockLayout();
					return;
				}
				PA_LOCK(str, char *, buff);
				str[0] = ' ';
				PA_UNLOCK(buff);
				tmp_text.text = buff;
				tmp_text.text_len = 1;
				tmp_text.text_attr =
					state->font_stack->text_attr;
				FE_GetTextInfo(context, &tmp_text, &text_info);
				PA_FREE(buff);

				if (tag->is_end == FALSE)
				{
					state->baseline +=
						(text_info.ascent / 2);
				}
				else
				{
					LO_TextAttr *attr;

					state->baseline -=
						(text_info.ascent / 2);

					attr = lo_PopFont(state, tag->type);
				}
			}
			break;

		/*
		 * Superscript.  Move up so the baseline of the new text
		 * is raised above the baseline of the old by 1/2 the
		 * ascent of the new text.
		 */
		case P_SUPER:
			if (state->font_stack != NULL)
			{
				PA_Block buff;
				char *str;
				LO_TextStruct tmp_text;
				LO_TextInfo text_info;

				if (tag->is_end == FALSE)
				{
					LO_TextAttr *old_attr;
					LO_TextAttr *attr;
					intn new_size;

					old_attr = state->font_stack->text_attr;
					lo_CopyTextAttr(old_attr, &tmp_attr);
					new_size = LO_ChangeFontSize(tmp_attr.size,
								"-1");
					tmp_attr.size = new_size;
					attr = lo_FetchTextAttr(state,
						&tmp_attr);

					lo_PushFont(state, tag->type, attr);
				}

				/*
				 * All this work is to get the text_info
				 * filled in for the current font in the font
				 * stack. Yuck, there must be a better way.
				 */
				memset (&tmp_text, 0, sizeof (tmp_text));
				buff = PA_ALLOC(1);
				if (buff == NULL)
				{
					state->top_state->out_of_memory = TRUE;
					LO_UnlockLayout();
					return;
				}
				PA_LOCK(str, char *, buff);
				str[0] = ' ';
				PA_UNLOCK(buff);
				tmp_text.text = buff;
				tmp_text.text_len = 1;
				tmp_text.text_attr =
					state->font_stack->text_attr;
				FE_GetTextInfo(context, &tmp_text, &text_info);
				PA_FREE(buff);

				if (tag->is_end == FALSE)
				{
					state->baseline -=
						(text_info.ascent / 2);
				}
				else
				{
					LO_TextAttr *attr;

					state->baseline +=
						(text_info.ascent / 2);

					attr = lo_PopFont(state, tag->type);
				}
			}
			break;

		/*
		 * Change the base font size that relative font size changes
		 * are figured from.
		 */
		case P_BASEFONT:
			{
				LO_TextAttr *old_attr;
				LO_TextAttr *attr;
				PA_Block buff;
				intn new_size;
				char *size_str;
				lo_FontStack *fptr;

				buff = lo_FetchParamValue(context, tag, PARAM_SIZE);
				if (buff != NULL)
				{
					PA_LOCK(size_str, char *, buff);
					new_size = LO_ChangeFontSize(
						state->base_font_size,
						size_str);
					PA_UNLOCK(buff);
					PA_FREE(buff);
				}
				else
				{
					new_size = DEFAULT_BASE_FONT_SIZE;
				}
				state->base_font_size = new_size;

				fptr = state->font_stack;
				/*
				 * If the current font is the basefont
				 * we are changing the current font size.
				 */
				if (fptr->tag_type == P_UNKNOWN)
				{
					/*
					 * Flush all current text before
					 * changing the font size.
					 */
					lo_FlushTextBlock(context, state);
				}

				/*
				 * Traverse the font stack to the base font
				 * entry.
				 */
				while (fptr->tag_type != P_UNKNOWN)
				{
					fptr = fptr->next;
				}

				/*
				 * Copy the old base entry, change its
				 * size, and stuff the new one in its place
				 */
				old_attr = fptr->text_attr;
				lo_CopyTextAttr(old_attr, &tmp_attr);
				tmp_attr.size = state->base_font_size;
				attr = lo_FetchTextAttr(state, &tmp_attr);
				if (attr != NULL)
				{
					fptr->text_attr = attr;
				}
			}
			break;


		/*
		 * Change font, currently only lets you change size.
		 * Now let you change color.
		 * Now let you change FACE.
		 */
		case P_FONT:
			if (tag->is_end == FALSE)
			{
				LO_TextAttr *old_attr;
				LO_TextAttr *attr;
				PA_Block buff;
				intn new_size;
				uint16 new_point_size;
				uint16 new_font_weight;
				char *size_str;
				char *str;
				char *new_face;
				Bool has_face;
				Bool has_size;
				Bool has_point_size;
				Bool has_font_weight = FALSE;
				Bool new_colors;
				uint8 red, green, blue;

				new_size = state->base_font_size;
				has_size = FALSE;
				has_point_size = FALSE;
				has_font_weight = FALSE;
				has_face = FALSE;
				buff = lo_FetchParamValue(context, tag, PARAM_SIZE);
				if (buff != NULL)
				{
					PA_LOCK(size_str, char *, buff);
					new_size = LO_ChangeFontSize(
						state->base_font_size,
						size_str);
					PA_UNLOCK(buff);
					PA_FREE(buff);
					has_size = TRUE;
				}

				buff = lo_FetchParamValue(context, tag, PARAM_POINT_SIZE);
				if (buff != NULL)
				{
					PA_LOCK(size_str, char *, buff);
					new_point_size = LO_ChangePointSize(
						DEFAULT_BASE_POINT_SIZE,
						size_str);
					PA_UNLOCK(buff);
					PA_FREE(buff);
					has_point_size = TRUE;
				}

				buff = lo_FetchParamValue(context, tag, PARAM_FONT_WEIGHT);
				if (buff != NULL)
				{
					PA_LOCK(size_str, char *, buff);
					new_font_weight = LO_ChangeFontOrPointSize(
						DEFAULT_BASE_FONT_WEIGHT,
						size_str,
						MIN_FONT_WEIGHT,
						MAX_FONT_WEIGHT);

					/* normalize 100, 200, ... 900 */
					new_font_weight = new_font_weight - (new_font_weight % 100);

					PA_UNLOCK(buff);
					PA_FREE(buff);
					has_font_weight = TRUE;
				}

				/*
				 * For backwards compatibility, a font
				 * tag with NO attributes will reset the
				 * font size to the base size.
				 */
				if (PA_TagHasParams(tag) == FALSE)
				{
					new_size = state->base_font_size;
					has_size = TRUE;
				}

				new_colors = FALSE;
				buff = lo_FetchParamValue(context, tag, PARAM_COLOR);
				if (buff != NULL)
				{
				    PA_LOCK(str, char *, buff);
				    LO_ParseRGB(str, &red, &green, &blue);
				    PA_UNLOCK(buff);
				    PA_FREE(buff);
				    new_colors = TRUE;
				}

				if (lo_face_attribute())
				{
					buff = lo_FetchParamValue(context, tag,
							PARAM_FACE);
					new_face = NULL;
					if (buff != NULL)
					{
						PA_LOCK(str, char *, buff);
						new_face = lo_FetchFontFace(context, state,
								str);
						PA_UNLOCK(buff);
						PA_FREE(buff);
						has_face = TRUE;
					}
				}

				old_attr = state->font_stack->text_attr;
				lo_CopyTextAttr(old_attr, &tmp_attr);

				if (has_size != FALSE)
				{
					tmp_attr.size = new_size;
				}

				if (has_point_size)
					tmp_attr.point_size = (double)new_point_size;

				if (has_font_weight)
					tmp_attr.font_weight = new_font_weight;

				if (new_colors != FALSE)
				{
				    tmp_attr.fg.red = red;
				    tmp_attr.fg.green = green;
				    tmp_attr.fg.blue = blue;
				}

				if (has_face != FALSE)
				{
				    tmp_attr.font_face = new_face;
				}

				attr = lo_FetchTextAttr(state, &tmp_attr);

				lo_PushFont(state, tag->type, attr);
			}
			else
			{
				LO_TextAttr *attr;

				attr = lo_PopFont(state, tag->type);
			}
			break;

		/*
		 * Change font to bigger than current font.
		 */
		case P_BIG:
			if (tag->is_end == FALSE)
			{
				LO_TextAttr *old_attr;
				LO_TextAttr *attr;
				intn new_size;

				old_attr = state->font_stack->text_attr;
				lo_CopyTextAttr(old_attr, &tmp_attr);
				new_size = LO_ChangeFontSize(tmp_attr.size, "+1");
				tmp_attr.size = new_size;
				attr = lo_FetchTextAttr(state, &tmp_attr);

				lo_PushFont(state, tag->type, attr);
			}
			else
			{
				LO_TextAttr *attr;

				attr = lo_PopFont(state, tag->type);
			}
			break;

		/*
		 * Change font to smaller than current font.
		 */
		case P_SMALL:
			if (tag->is_end == FALSE)
			{
				LO_TextAttr *old_attr;
				LO_TextAttr *attr;
				intn new_size;

				old_attr = state->font_stack->text_attr;
				lo_CopyTextAttr(old_attr, &tmp_attr);
				new_size = LO_ChangeFontSize(tmp_attr.size, "-1");
				tmp_attr.size = new_size;
				attr = lo_FetchTextAttr(state, &tmp_attr);

				lo_PushFont(state, tag->type, attr);
			}
			else
			{
				LO_TextAttr *attr;

				attr = lo_PopFont(state, tag->type);
			}
			break;

		/*
		 * Change the absolute URL that describes where this
		 * docuemtn came from for all following
		 * relative URLs.
		 */
		case P_BASE:
			{
				PA_Block buff;
				char *url;
				char *target;

				buff = lo_FetchParamValue(context, tag, PARAM_HREF);
				if (buff != NULL)
				{
					PA_LOCK(url, char *, buff);
					if (url != NULL)
					{
					    int32 len;

					    len = lo_StripTextWhitespace(
							url, XP_STRLEN(url));
					}
                    lo_SetBaseUrl(state->top_state, url, FALSE);
					PA_UNLOCK(buff);
					PA_FREE(buff);
				}

				buff = lo_FetchParamValue(context, tag, PARAM_TARGET);
				if (buff != NULL)
				{
					PA_LOCK(target, char *, buff);
					if (target != NULL)
					{
					    int32 len;

					    len = lo_StripTextWhitespace(
						target, XP_STRLEN(target));
					}
				        if (lo_IsValidTarget(target) == FALSE)
					{
					    PA_UNLOCK(buff);
					    PA_FREE(buff);
#ifdef XP_OS2_FIX /*trap on double free of buff...*/
						buff = NULL;
#endif
					    target = NULL;
					}
					if (state->top_state->base_target !=
						NULL)
					{
					    XP_FREE(
						state->top_state->base_target);
					    state->top_state->base_target =NULL;
					}
					if (target != NULL)
					{
					    state->top_state->base_target =
							XP_STRDUP(target);
					}
					PA_UNLOCK(buff);
					PA_FREE(buff);
				}
			}
			break;

		/*
		 * Begin an anchor.  Change font colors if
		 * there is an HREF here.  Make the HREF absolute
		 * based on the base url.  Check name in case we
		 * are jumping to a named anchor inside this document.
		 */
		case P_ANCHOR:
			lo_process_anchor_tag(context, state, tag);
			if (state->top_state->out_of_memory != FALSE)
			{
				LO_UnlockLayout();
				return;
			}
			break;

		/*
		 * Insert a wordbreak.  Allows layout to break a line
		 * at this location in case it needs to.
		 */
		case P_WORDBREAK:
			lo_InsertWordBreak(context, state);
			break;

		/*
		 * All text inside these tags must be considered one
		 * word, and not broken (unless there is a word break
		 * in there).
		 */
		case P_NOBREAK:
			if (tag->is_end == FALSE)
			{
				state->breakable = FALSE;
			}
			else
			{
				state->breakable = TRUE;
			}
			break;

		/*
		 * spacer element to move stuff around
		 */
		case P_SPACER:
			if (tag->is_end == FALSE && !state->hide_content)
			{
				lo_ProcessSpacerTag(context, state, tag);
			}
			break;

		case P_MULTICOLUMN:
			lo_process_multicolumn_tag(context, state, tag);
			break;

		case P_LAYER:
        case P_ILAYER:

			if(state->hide_content)
			{
				break;  /* skip layer */
            }
			else if (tag->is_end == FALSE)
			{
                /* Can't have a layer inside a form */
                if (state->top_state->in_form != FALSE)
                {
                    lo_EndForm(context, state);
                    lo_SetSoftLineBreakState(context, state, FALSE, 2);
                }

				lo_BeginLayerTag(context, state, tag);
			}
			else
			{
   			    /* Don't close blocks unless they were opened in this scope. */
				if (state->layer_nest_level > 0)
				{
					lo_EndLayerTag(context, state, tag);
				}
			}
			break;

		/*
		 * Break the current line.
		 * Optionally break furthur down to get past floating
		 * elements in the margins.
		 */
		case P_LINEBREAK:
		  
		  {
			uint8 clear_type;

			if(state->hide_content)
				break;  /* skip the whole tag */

			{
			    PA_Block buff;

			    buff = lo_FetchParamValue(context, tag, PARAM_CLEAR);
			    if (buff != NULL)
			    {
				char *clear_str;

				PA_LOCK(clear_str, char *, buff);
				if (pa_TagEqual("left", clear_str))
				{
				  clear_type = LO_CLEAR_TO_LEFT;
				}
				else if (pa_TagEqual("right", clear_str))
				{
				  clear_type = LO_CLEAR_TO_RIGHT;
				}
				else if (pa_TagEqual("all", clear_str))
				{
				  clear_type = LO_CLEAR_TO_BOTH;
				}
				else if (pa_TagEqual("both", clear_str))
				{
				  clear_type = LO_CLEAR_TO_BOTH;
				}
				else if (pa_TagEqual("none", clear_str))
			    {
				  clear_type = LO_CLEAR_NONE;
				}
				else
				{
				  /* default to clear none  */
				  clear_type = LO_CLEAR_NONE;
				}
				PA_UNLOCK(buff);
				PA_FREE(buff);
				/*
				 * Reset the margins properly in case
				 * we are inside a list.
				 */
				lo_FindLineMargins(context, state, TRUE);
				state->x = state->left_margin;
			    }
			}

			/*
			 * <br> tag diabled in preformatted text
			 */
#ifdef EDITOR
			if (state->preformatted != PRE_TEXT_NO && !EDT_IS_EDITOR(context))
#else
			if (state->preformatted != PRE_TEXT_NO)
#endif
			{
				lo_SetSoftLineBreakState(context, state, FALSE, 1);
				break;
			}
			lo_HardLineBreakWithClearType(context, state, clear_type, FALSE);
#ifdef EDITOR
			/* editor hack.  Make breaks always do something. */
			if ( EDT_IS_EDITOR(context) ) {
				state->linefeed_state = 0;
            }
#endif

			switch(clear_type) 
			  {
			  case LO_CLEAR_TO_LEFT:
				lo_ClearToLeftMargin(context, state);
				break;
			  case LO_CLEAR_TO_RIGHT:
				lo_ClearToRightMargin(context, state);
				break;
			  case LO_CLEAR_TO_ALL:
			  case LO_CLEAR_TO_BOTH:
				lo_ClearToBothMargins(context, state);
				break;
			  case LO_CLEAR_NONE:
			  default:
				break;
			}
			/*
			 * Reset the margins properly in case
			 * we are inside a list.
			 */
			lo_FindLineMargins(context, state, TRUE);
			state->x = state->left_margin;
			
			break;
		  }

		/*
		 * The head tag.  The tag is supposed to be a container
		 * for the head of the HTML document.
		 * We use it to try and detect when we should
		 * suppress the content of unknown head tags.
		 */
		case P_HEAD:
			/*
			 * The end HEAD tags defines the end of
			 * the HEAD section of the HTML
			 */
			if (tag->is_end != FALSE)
			{
				state->top_state->in_head = FALSE;
			}
			break;

		/*
		 * The body tag.  The tag is supposed to be a container
		 * for the body of the HTML document.
		 * Right now we are just using it to specify
		 * the document qualities and colors.
		 */
		case P_BODY:
			/*
			 * The start of BODY definitely ends
			 * the HEAD section of the HTML and starts the BODY
			 * Only process this body tag if we aren't already
			 * in a body.
			 */
			if (state->top_state->in_body == FALSE)
			{
				state->top_state->in_head = FALSE;
				state->top_state->in_body = TRUE;

				lo_ProcessBodyTag(context, state, tag);
			}
			else
			{
				state->top_state->in_head = FALSE;
				state->top_state->in_body = TRUE;
				/*
				 * We now process later bodies just to
				 * see if they set attributes not set by
				 * the first body.
				 */
				lo_ProcessBodyTag(context, state, tag);
			}
			break;

		/*
		 * The colormap tag.  This is used to specify the colormap
		 * to be used by all the images on this page.
		 */
		case P_COLORMAP:
			if ((tag->is_end == FALSE)&&
				(state->end_last_line == NULL))
			{
				PA_Block buff;
				char *str;
				char *image_url;

				image_url = NULL;
				buff = lo_FetchParamValue(context, tag, PARAM_SRC);
				if (buff != NULL)
				{
					PA_LOCK(str, char *, buff);
					if (str != NULL)
					{
						int32 len;

						len = lo_StripTextWhitespace(
							str, XP_STRLEN(str));
					}
					image_url = NET_MakeAbsoluteURL(
						state->top_state->base_url,str);
					PA_UNLOCK(buff);
					PA_FREE(buff);
				}
#ifndef M12N                    /* XXXM12N Fix me.  Colormap tag has yet to
                                   implemented. */
				(void)IL_ColormapTag(image_url, context);
#endif /* M12N */
				if (image_url != NULL)
				{
					XP_FREE(image_url);
				}
			}
			break;

		/*
		 * The meta tag.  This tag can only appear in the
		 * head of the document.
		 * It overrides HTTP headers sent by the
		 * web server that served this document.
		 * If URLStruct somehow became NULL, META must
		 * be ignored.
		 */
		case P_META:
			if ((tag->is_end == FALSE)&&
				/* (state->end_last_line == NULL)&& allow in head */
				(state->top_state->nurl != NULL))
			{
				PA_Block buff;
				PA_Block buff2;
				PA_Block buff3;
				char *name;
				char *value;
				char *tptr;

				buff = lo_FetchParamValue(context, tag, "rel");
				if (buff != NULL)
				{
					char *rel_val = (char*)buff;
					
					/* try and fetch the src */
					buff2 = lo_FetchParamValue(context, tag, PARAM_SRC);
					if (buff2 != NULL)
					{
						if(!strcasecomp(rel_val, "SMALL_BOOKMARK_ICON"))
						{
							state->top_state->small_bm_icon =
								 NET_MakeAbsoluteURL(state->top_state->base_url, 
												  rel_val);
						}
						else if(!strcasecomp(rel_val, "LARGE_BOOKMARK_ICON"))
						{
							state->top_state->large_bm_icon =
								 NET_MakeAbsoluteURL(state->top_state->base_url, 
												  rel_val);
						}

						PA_FREE(buff2);
					}

					PA_FREE(buff);
	
				}

				buff = lo_FetchParamValue(context, tag,
					PARAM_HTTP_EQUIV);
				if (buff != NULL)
				{
				    /*
				     * Lou needs a colon on the end of
				     * the name.
				     */
				    PA_LOCK(tptr, char *, buff);
				    buff3 = PA_ALLOC(XP_STRLEN(tptr) + 2);
				    if (buff3 != NULL)
				    {
					PA_LOCK(name, char *, buff3);
					XP_STRCPY(name, tptr);
					XP_STRCAT(name, ":");
					PA_UNLOCK(buff);
					PA_FREE(buff);

					buff2 = lo_FetchParamValue(context, tag,
						PARAM_CONTENT);
					if (buff2 != NULL)
					{
						Bool ok;

						PA_LOCK(value, char *, buff2);
						ok = NET_ParseMimeHeader(
							FO_PRESENT,
							context,
							state->top_state->nurl,
							name, value, FALSE);
						PA_UNLOCK(buff2);
						PA_FREE(buff2);
						if (INTL_GetCSIRelayoutFlag(c) == METACHARSET_REQUESTRELAYOUT)
						{
							INTL_SetCSIRelayoutFlag(c, METACHARSET_FORCERELAYOUT);
							INTL_Relayout(context);
						}

					}
					PA_UNLOCK(buff3);
					PA_FREE(buff3);
				    }
				    else
				    {
					PA_UNLOCK(buff);
					PA_FREE(buff);
				    }
				}
			}
			break;

		/*
		 * The hype tag is just for fun.
		 * It only effects the UNIX version
		 * which can affor to have a sound file
		 * compiled into the binary.
		 */
		case P_HYPE:
#if defined(XP_UNIX) || defined(XP_MAC)
			if (tag->is_end == FALSE && !state->hide_content)
			{
			    PA_Tag tmp_tag;
			    PA_Block buff;
			    PA_Block abuff;
			    LO_AnchorData *hold_current_anchor;
			    LO_AnchorData *hype_anchor;
			    char *str;

			    /*
			     * Try to allocate the hype anchor as if we
			     * were inside:
			     * <A HREF="HYPE_ANCHOR">
			     * Save the real current anchor to restore later
			     */
			    hype_anchor = NULL;
			    abuff = PA_ALLOC(XP_STRLEN(HYPE_ANCHOR) + 1);
			    if (abuff != NULL)
			    {
				hype_anchor = lo_NewAnchor(state, NULL, NULL);
				if (hype_anchor == NULL)
				{
				    PA_FREE(abuff);
				    abuff = NULL;
				}
				else
				{
				    PA_LOCK(str, char *, abuff);
				    XP_STRCPY(str, HYPE_ANCHOR);
				    PA_UNLOCK(abuff);

				    hype_anchor->anchor = abuff;
				    hype_anchor->target = NULL;

				    /*
				     * Add this url's block to the list
				     * of all allocated urls so we can free
				     * it later.
				     */
				    lo_AddToUrlList(context, state,
					hype_anchor);
				    if (state->top_state->out_of_memory !=FALSE)
				    {
					PA_FREE(abuff);
					XP_DELETE(hype_anchor);
					LO_UnlockLayout();
					return;
				    }
				}
			    }
			    hold_current_anchor = state->current_anchor;
			    state->current_anchor = hype_anchor;

			    /*
			     * If we have the memory, create a fake image
			     * tag to replace the <HYPE> tag and process it.
			     */
			    buff = PA_ALLOC(XP_STRLEN(HYPE_TAG_BECOMES)+1);
			    if (buff != NULL)
			    {

				PA_LOCK(str, char *, buff);
				XP_STRCPY(str, HYPE_TAG_BECOMES);
				PA_UNLOCK(buff);

				tmp_tag.type = P_IMAGE;
				tmp_tag.is_end = FALSE;
				tmp_tag.newline_count = tag->newline_count;
				tmp_tag.data = buff;
				tmp_tag.data_len = XP_STRLEN(HYPE_TAG_BECOMES);
				tmp_tag.true_len = 0;
				tmp_tag.lo_data = NULL;
				tmp_tag.next = NULL;
				lo_FormatImage(context, state, &tmp_tag);
				PA_FREE(buff);
			    }

			    /*
			     * Restore the proper current_anchor.
			     */
			    state->current_anchor = hold_current_anchor;
			}
#endif /* XP_UNIX */
			break;

		/*
		 * Insert a horizontal rule element.
		 * They always take up an entire line.
		 */
		case P_HRULE:
			if (state->in_paragraph != FALSE)
			{
				lo_CloseParagraph(context, &state, tag, 2);
			}

			/*
			 * HR now does alignment the same as P, H?, and DIV.
			 * Ignore </hr>.
			 */
			if (tag->is_end == FALSE && !state->hide_content)
			{
				lo_SetSoftLineBreakState(context, state, FALSE, 1);

				lo_HorizontalRule(context, state, tag);
			}
			break;

		/*
		 * Experimental!  May be used to implement TABLES
		 */
		case P_CELL:
			break;

		/*
		 * The start of a caption in a table
		 */
		case P_CAPTION:
			if(!state->hide_content)
				lo_process_caption_tag(context, state, tag);
			break;

		/*
		 * The start of a header or data cell
		 * in a row in a table
		 */
		case P_TABLE_HEADER:
		case P_TABLE_DATA:
			if(!state->hide_content)
				lo_process_table_cell_tag(context, state, tag);
			break;

		/*
		 * The start of a row in a table
		 */
		case P_TABLE_ROW:
			if(!state->hide_content)
				lo_process_table_row_tag(context, state, tag);
			break;

		/*
		 * The start of a table.  Since this can cause the 
		 *   destruction of elements that others may be holding
		 *   pointers to, make sure they are out of their critical
		 *   sections before starting to layout the table
		 */
		case P_TABLE:
			if(!state->hide_content)
				lo_process_table_tag(context, state, tag);
			break;

		/*
		 * Experimental!  Everything in these tags is a complete
		 * separate sub document, it should be formatted, and the
		 * resulting entire document placed just like an image.
		 */
		case P_SUBDOC:
			break;

		/*
		 * Begin a grid.  MUST be the very first thing inside
		 * this HTML document.  After a grid is started, all non-grid
		 * tags will be ignored!
		 * Also, right now the text and postscript FEs can't
		 * handle grids, so we block them.
		 */
		case P_GRID:
			if ((context->type != MWContextText)&&
			    (context->type != MWContextPostScript)&&
				(EDT_IS_EDITOR(context) == FALSE)&&
			    (state->top_state->nothing_displayed != FALSE)&&
                !lo_InsideLayer(state) &&
			    (state->top_state->in_body == FALSE)&&
			    (state->end_last_line == NULL)&&
			    /* (state->line_list == NULL)&& */    /* Can be non-null because we are putting TEXTBLOCK
													     elements on there each time we get text tags*/
			    (state->float_list == NULL)&&
			    (state->top_state->the_grid == NULL))
			{
			    if (tag->is_end == FALSE)
			    {
				if (state->current_grid == NULL)
				{
					state->top_state->is_grid = TRUE;
					/*
					 * We may be restoring a grid from
					 * history.  If so we recreate it
					 * from saved data, and ignore all
					 * the rest of the tags in this
					 * document.  This happens because
					 * we will set the_grid and not
					 * set current_grid, so all
					 * grid tags are ignored, and since
					 * we have set is_grid, all non-grid
					 * tags will be ignored.
					 *
					 * Only recreate if main window
					 * size has not changed.
					 */
					if (state->top_state->savedData.Grid->the_grid != NULL)
					{
					    lo_SavedGridData *savedGridData;
					    int32 width, height;

					    width = state->win_width;
					    height = state->win_height;
					    FE_GetFullWindowSize(context,
						&width, &height);

					    savedGridData =
					      state->top_state->savedData.Grid;
					    if ((savedGridData->main_width ==
							width)&&
					        (savedGridData->main_height ==
							height))
					    {
						lo_RecreateGrid(context, state,
						    savedGridData->the_grid);
						savedGridData->the_grid =NULL;
					    }
					    else
					    {
						/*
						 * If window size has changed
						 * on the grid we are restoring
						 * we process normally, but
						 * hang on to the old data so
						 * we can restore the grid cell
						 * contents once new sizes are
						 * calculated.
						 */
						state->top_state->old_grid =
						    savedGridData->the_grid;
						savedGridData->the_grid =NULL;

						lo_BeginGrid(context, state,
								tag);
					    }
					}
					else
					{
					    lo_BeginGrid(context, state, tag);
					}
				}
				else
				{
					lo_BeginSubgrid(context, state, tag);
				}
			    }
			    else if ((tag->is_end != FALSE)&&
					(state->current_grid != NULL))
			    {
				lo_GridRec *grid;

				grid = state->current_grid;
				if (grid->subgrid == NULL)
				{
					state->current_grid = NULL;
					lo_EndGrid(context, state, grid);
				}
				else
				{
					lo_EndSubgrid(context, state, grid);
				}
			    }
			}
			break;

		/*
		 * A grid cell can only happen inside a grid
		 */
		case P_GRID_CELL:
			if ((state->top_state->nothing_displayed != FALSE)&&
                !lo_InsideLayer(state) &&
			    (state->top_state->in_body == FALSE)&&
			    (state->end_last_line == NULL)&&
			    /* state->line_list == NULL)&& */	/* Can be non-null because we are putting TEXTBLOCK
													   elements on there each time we get text tags*/	
			    (state->float_list == NULL)&&
			    (state->current_grid != NULL))
			{
				if (tag->is_end == FALSE)
				{
					lo_BeginGridCell(context, state, tag);
				}
			}
			break;

		/*
		 * Begin a map.  Maps define area in client-side
		 * image maps.
		 */
		case P_MAP:
			if ((tag->is_end == FALSE)&&
			    (state->top_state->current_map == NULL))
			{
				lo_BeginMap(context, state, tag);
			}
			else if ((tag->is_end != FALSE)&&
				 (state->top_state->current_map != NULL))
			{
				lo_MapRec *map;

				map = state->top_state->current_map;
				state->top_state->current_map = NULL;
				lo_EndMap(context, state, map);
			}
			break;

		/*
		 * An area definition, can only happen inside a MAP
		 * definition for a client-side image map.
		 */
		case P_AREA:
			if ((state->top_state->current_map != NULL)&&
				(tag->is_end == FALSE))
			{
				lo_BeginMapArea(context, state, tag);
			}
			break;

		/*
		 * Shortcut and old historic tag to auto-place
		 * a simple single item query form.
		 */
		case P_INDEX:
			/*
			 * No forms in the scrolling document
			 * hack
			 */
			if (state->top_state->scrolling_doc != FALSE)
			{
				break;
			}

			if ((tag->is_end == FALSE)&&
				(state->top_state->in_form == FALSE))
			{
				if (state->in_paragraph != FALSE)
				{
					lo_CloseParagraph(context, &state, tag, 2);
				}
				if(!state->hide_content)
					lo_ProcessIsIndexTag(context, state, tag);
			}
			break;

		/*
		 * Begin a form.
		 * Form cannot be nested!
		 */
		case P_FORM:
#if defined(SingleSignon)
                        /* Notify the signon module of the new form */
                        SI_StartOfForm();
#endif
			/*
			 * No forms in the scrolling document
			 * hack
			 */
			if (state->top_state->scrolling_doc != FALSE)
			{
				break;
			}

			if (state->in_paragraph != FALSE)
			{
				lo_CloseParagraph(context, &state, tag, 2);
			}
			if (tag->is_end == FALSE)
			{
				/*
				 * Sorry, no nested forms
				 */
				if (state->top_state->in_form == FALSE)
				{
					lo_SetLineBreakState(context, state, FALSE,
										 LO_LINEFEED_BREAK_HARD, 2, FALSE);

					lo_BeginForm(context, state, tag);
				}
			}
			else if (state->top_state->in_form != FALSE)
			{
				lo_EndForm(context, state);
				lo_SetLineBreakState(context, state, FALSE,
									 LO_LINEFEED_BREAK_HARD, 2, FALSE);
			}
			break;

		/*
		 * Deceptive simple form element tag.  Can only be inside a
		 * form.  Really multiplies into one of many possible
		 * input elements.
		 */
		case P_INPUT:
			/*
			 * Input tags only inside an open form.
			 */
			if ((state->top_state->in_form != FALSE)&&
				(tag->is_end == FALSE))
			{
				if (state->in_paragraph != FALSE)
				{
					lo_CloseParagraph(context, &state, tag, 2);
				}
				if(!state->hide_content)
					lo_ProcessInputTag(context, state, tag);
			}
			break;

		/*
		 * A multi-line text input form element.  It encloses text
		 * that will be its default text.
		 */
		case P_TEXTAREA:
			/*
			 * Textarea tags only inside an open form.
			 */
			if (state->top_state->in_form != FALSE
				&& !state->hide_content)
			{
			    if (state->in_paragraph != FALSE)
			    {
				lo_CloseParagraph(context, &state, tag, 2);
			    }
			    if (tag->is_end == FALSE)
			    {
				lo_FlushTextBlock(context, state);

				state->line_buf_len = 0;
				state->text_divert = tag->type;
				lo_BeginTextareaTag(context, state, tag);
			    }
			    else if (state->text_divert == tag->type)
			    {
				PA_Block buff;
				int32 len;
				int32 new_len;

				if (state->line_buf_len != 0)
				{
				    PA_LOCK(tptr2, char *,
					state->line_buf);
				    /*
				     * Don't do this, they were already
				     * expanded by the parser.
				    (void)pa_ExpandEscapes(tptr2,
					state->line_buf_len, &len, TRUE);
				     */
				    len = state->line_buf_len;
				    buff = lo_ConvertToFELinebreaks(tptr2, len,
						&new_len);
				    PA_UNLOCK(state->line_buf);
				}
				else
				{
				    buff = NULL;
				    new_len = 0;
				}
				lo_EndTextareaTag(context, state, buff);
				state->line_buf_len = 0;
				state->text_divert = P_UNKNOWN;
			    }
			}
			break;

		/*
		 * Option tag can only occur inside a select inside a form
		 */
		case P_OPTION:
			/*
			 * Select tags only inside an open select
			 * in an open form.
			 */
			if ((state->top_state->in_form != FALSE)&&
				(tag->is_end == FALSE)&&
				(state->current_ele != NULL)&&
				(!state->hide_content)&&
				(state->current_ele->type == LO_FORM_ELE)&&
                                ((state->current_ele->lo_form.element_data->type
                                        == FORM_TYPE_SELECT_ONE)||
                                 (state->current_ele->lo_form.element_data->type
                                        == FORM_TYPE_SELECT_MULT)))
			{
			    if (state->in_paragraph != FALSE)
			    {
				lo_CloseParagraph(context, &state, tag, 2);
			    }
			    if (state->text_divert == P_OPTION)
			    {
				PA_Block buff;
				int32 len;

				if (state->line_buf_len != 0)
				{
				    PA_LOCK(tptr2, char *,
					state->line_buf);
				    /*  
				     * Don't do this, they were already
				     * expanded by the parser.
				    (void)pa_ExpandEscapes(tptr2,
					state->line_buf_len, &len, TRUE);
				     */
				    len = state->line_buf_len;
				    len = lo_CleanTextWhitespace(tptr2, len);
				    buff = PA_ALLOC(len + 1);
				    if (buff != NULL)
				    {
					PA_LOCK(tptr, char *, buff);
					XP_BCOPY(tptr2, tptr, len);
					tptr[len] = '\0';
					PA_UNLOCK(buff);
				    }
				    PA_UNLOCK(state->line_buf);
				}
				else
				{
				    buff = NULL;
				    len = 0;
				}
				lo_EndOptionTag(context, state, buff);
			    }
			    state->line_buf_len = 0;
			    lo_BeginOptionTag(context, state, tag);
			    state->text_divert = tag->type;
			}
			break;

		/*
		 * Select tag, either an option menu or a scrolled
		 * list inside a form.  Lets you chose N of Many.
		 */
		case P_SELECT:
			/*
			 * Select tags only inside an open form.
			 */
			if ((state->top_state->in_form != FALSE)&&
				(!state->hide_content))
			{
			    /*
			     * Because SELECT doesn't automatically flush,
			     * we must do it here if this is the start of a
			     * select.
			     */
			    if ((tag->is_end == FALSE)&&
				(state->current_ele == NULL))
			    {
				lo_FlushTextBlock(context, state);
			    }

			    if (state->in_paragraph != FALSE)
			    {
				lo_CloseParagraph(context, &state, tag, 2);
			    }
			    if ((tag->is_end == FALSE)&&
				(state->current_ele == NULL))
			    {
				/*
				 * Don't allow text between beginning of select
				 * and beginning of first option.
				 */
			        state->text_divert = tag->type;

				lo_BeginSelectTag(context, state, tag);
			    }
			    else if ((state->current_ele != NULL)&&
				(state->current_ele->type == LO_FORM_ELE)&&
				((state->current_ele->lo_form.element_data->type
					== FORM_TYPE_SELECT_ONE)||
				 (state->current_ele->lo_form.element_data->type
					== FORM_TYPE_SELECT_MULT)))
			    {
			        if (state->text_divert == P_OPTION)
			        {
				    PA_Block buff;
				    int32 len;

				    if (state->line_buf_len != 0)
				    {
				        PA_LOCK(tptr2, char *,
					    state->line_buf);
					/*  
					 * Don't do this, they were already
					 * expanded by the parser.
				        (void)pa_ExpandEscapes(tptr2,
					    state->line_buf_len, &len, TRUE);
					 */
					len = state->line_buf_len;
				        len = lo_CleanTextWhitespace(tptr2,len);
				        buff = PA_ALLOC(len + 1);
				        if (buff != NULL)
				        {
					    PA_LOCK(tptr, char *, buff);
					    XP_BCOPY(tptr2, tptr, len);
					    tptr[len] = '\0';
					    PA_UNLOCK(buff);
				        }
				        PA_UNLOCK(state->line_buf);
				    }
				    else
				    {
				        buff = NULL;
				        len = 0;
				    }
				    lo_EndOptionTag(context, state, buff);
			        }
			        state->line_buf_len = 0;
			        state->text_divert = P_UNKNOWN;

				lo_EndSelectTag(context, state);
			    }
			}
			break;

		/*
		 * Place an embedded object.
		 * Just reserve an area of layout for that object
		 * to use as it wishes.
		 */
		case P_EMBED:
			if (tag->is_end == FALSE && !state->hide_content)
			{
				/*
				 * If we have started loading an EMBED we are
				 * out if the HEAD section of the HTML
				 * and into the BODY
				 */
				state->top_state->in_head = FALSE;
				state->top_state->in_body = TRUE;
				lo_FormatEmbed(context, state, tag);
			}
			break;

		/*
		 * Generate a public/private key pair.  The user sees
		 * a selection of key sizes to choose from; an ascii
		 * representation of the public key becomes the form
		 * field value.
		 */
		case P_KEYGEN:
			/*
			 * Keygen tags only inside an open form.
			 */
			if ((state->top_state->in_form != FALSE)&&
			    (tag->is_end == FALSE)&&
				(!state->hide_content))
			{
				if (state->in_paragraph != FALSE)
				{
					lo_CloseParagraph(context, &state, tag, 2);
				}
				lo_ProcessKeygenTag(context, state, tag);
			}
			break;

		/*
		 * Process Mocha script source in a <SCRIPT LANGUAGE="Mocha">
		 * ...</SCRIPT> container.  Drop all unknown language SCRIPT
		 * contents on the floor.  Process SRC=<url> attributes for
		 * known languages.
		 */
		case P_SCRIPT:
#ifdef EDITOR
			if( EDT_IS_EDITOR( context ) )
			{
				goto process_server;
			}
#endif

			if(!state->hide_content)
				lo_ProcessScriptTag(context, state, tag, NULL);
			break;

	        case P_STYLE:
			if(!state->hide_content)
				lo_ProcessStyleTag(context, state, tag);
			break;

		case P_LINK:
			/* link tag */
            {
				PA_Block buff = lo_FetchParamValue(context, tag, PARAM_REL);
                if (buff != NULL)
				{
					if (strcasestr((char *)buff, "stylesheet"))
					{
						char *media = (char*)lo_FetchParamValue(context, tag, PARAM_MEDIA);

						/* check for media=screen
						 * don't load the style sheet if there 
						 * is a media not equal to screen
						 */
						if(!media || !strcasecomp(media, "screen"))
						{
							if(LO_StyleSheetsEnabled(context))
							{
								/* set stylesheet tag stack on */
								if(state->top_state && state->top_state->style_stack)
								{
									STYLESTACK_SetSaveOn(state->top_state->style_stack, TRUE);
								}

								lo_ProcessScriptTag(context, state, tag, NULL);
								/* now end the script since
								 * link is an empty tag 
								 */
								tag->is_end = TRUE;
								lo_ProcessScriptTag(context, state, tag, NULL);
								tag->is_end = FALSE;
							}
						}

						XP_FREEIF(media);
					}
					else if (!strcasecomp((char *)buff, "fontdef") &&
						!state->top_state->resize_reload)
					{
						/* Webfonts */
						PA_Block buff2 = lo_FetchParamValue(context, tag, PARAM_SRC);
						char *font_url = NULL;
						char *str = NULL;

						if (buff2 != NULL)
						{
							PA_LOCK(str, char *, buff2);
							if (str != NULL)
							{
								(void) lo_StripTextWhitespace(str,
									XP_STRLEN(str));
							}
							font_url = NET_MakeAbsoluteURL(
								state->top_state->base_url, str);
							PA_UNLOCK(buff2);
							PA_FREE(buff2);
						}

						if (font_url)
						{
#ifdef WEBFONTS
							if (WF_fbu)
							{
								/* Download the webfont */
								nffbu_LoadWebfont(WF_fbu, context, font_url,
									FORCE_RELOAD_FLAG(state->top_state), NULL);
							}
#endif /* WEBFONTS */
							XP_FREE(font_url);
						}
					}
					else if (!strcasecomp((char *)buff, "sitemap"))
					{
						/* RDF sitemaps */
						PA_Block buff2 = lo_FetchParamValue(context, tag, PARAM_SRC);
                        PA_Block buff3 = lo_FetchParamValue(context, tag, PARAM_NAME);
						char *rdfURL = NULL;
						char *str = NULL;

						if (buff2 != NULL)
						{
							PA_LOCK(str, char *, buff2);
							if (str != NULL)
							{
								(void) lo_StripTextWhitespace(str,
									XP_STRLEN(str));
							}
							rdfURL = NET_MakeAbsoluteURL(
								state->top_state->base_url, str);
                            if (rdfURL)
                              {
                                XP_AddNavCenterSitemap(context, rdfURL, (char *)buff3);
                              }

							PA_UNLOCK(buff2);
							PA_FREE(buff2);
							PA_UNLOCK(buff3);
							PA_FREE(buff3);

						}

					}
					else if (!strcasecomp((char *)buff, "prefetch"))
					{
						char *prefetchURL;
						char *str;
						PA_Block buff2 = lo_FetchParamValue(context, tag, PARAM_SRC);

						if (buff2)
						{
							PA_LOCK(str, char *, buff2);
							if (str)
							{
								(void) lo_StripTextWhitespace(str, XP_STRLEN(str));
							}
							prefetchURL = NET_MakeAbsoluteURL(
									state->top_state->base_url, str);
							PA_UNLOCK(buff2);
							PA_FREE(buff2);
						}
						if (prefetchURL)
						{
							PRE_AddToList(context, prefetchURL, 1.0);
						}
					}						
						
					PA_FREE(buff);
				}
			}
			break;

		/*
		 * We were blocking on a <script> tag, while being processed
		 *   it did a document.write("something that blocked us").
		 * Now the elements in the above call are no longer blocking
		 *   us but we need to continue to block on the script tag
		 *   since it might have more output
		 */
		case P_NSCP_REBLOCK:
		    if (state->in_relayout == FALSE) 
		    {
				state->top_state->layout_blocking_element = tag->lo_data;
				XP_ASSERT(tag->newline_count == state->top_state->input_write_level);
				state->top_state->input_write_level--;
				ET_DocWriteAck(context, 0);
		    }
		    break;

		/*
		 * This is the NEW HTML-WG approved embedded JAVA
		 * application.
		 */
#if defined(OJI)
		case P_JAVA_APPLET:
			javaPlugin = NPL_FindPluginEnabledForType(JAVA_PLUGIN_MIMETYPE);

			/* If there is a JVM installed as a plugin, redirect to the object tag */
			if(javaPlugin && !state->hide_content)
			{
				if(tag->is_end == FALSE)
				{
					PA_Block buff;
					char* str;

					buff = lo_FetchParamValue(context, tag, PARAM_CODE);
					if(buff)
					{
						PA_LOCK(str, char *, buff);
						lo_AddParam(tag, "DATA", str);
						lo_AddParam(tag, "TYPE", JAVA_PLUGIN_MIMETYPE);

						PA_UNLOCK(buff);
						XP_FREE(buff);

						tag->type = P_OBJECT;
						lo_ProcessObjectTag(context, state, tag, FALSE);
					}
				}
				else
				{
					tag->type = P_OBJECT;
					lo_ProcessObjectTag(context, state, tag, FALSE);
				}
				
				XP_FREE(javaPlugin);	

			}
			break;

#elif defined(JAVA)
		case P_JAVA_APPLET:
		    /*
		     * If the user has disabled Java, act like it we don't
		     * understand the APPLET tag.
		     */
		    if (!state->hide_content && LJ_GetJavaEnabled() != FALSE)
		    {
			if (tag->is_end == FALSE)
			{
			    /*
			     * If we open a new java applet without closing
			     * an old one, force a close of the old one
			     * first.
			     * Now open the new java applet.
			     */
			    if (state->current_java != NULL)
			    {
				lo_CloseJavaApp(context, state,
						state->current_java);
				state->current_java = NULL;

				lo_FormatJavaApp(context, state, tag);
			    }
			    /*
			     * Else this is a new java applet, open it.
			     */
			    else
			    {
				/*
				 * If we have started loading an APPLET we are
				 * out if the HEAD section of the HTML
				 * and into the BODY
				 */
				state->top_state->in_head = FALSE;
				state->top_state->in_body = TRUE;
				
				lo_FormatJavaApp(context, state, tag);
			    }
			}
			else
			{
			    /*
			     * Only close a java app if it exists.
			     */
			    if (state->current_java != NULL)
			    {
				lo_CloseJavaApp(context, state,
						state->current_java);
				state->current_java = NULL;
			    }
			}
		    }
			break;
#endif /* JAVA */

		case P_OBJECT:
			if(!state->hide_content)
				lo_ProcessObjectTag(context, state, tag, FALSE);
			break;

		case P_PARAM:
			if(!state->hide_content)
				lo_ProcessParamTag(context, state, tag, FALSE);
			break;

		/*
		 * Place an image.  Special because if layout of all
		 * other tags was blocked, image tags are still
		 * partially processed at that time.
		 */
		case P_IMAGE:
		case P_NEW_IMAGE:
			if (tag->is_end == FALSE && !state->hide_content)
			{
				/*
				 * If we have started loading an IMAGE we are
				 * out if the HEAD section of the HTML
				 * and into the BODY
				 */
				state->top_state->in_head = FALSE;
				state->top_state->in_body = TRUE;

				if (tag->lo_data != NULL)
				{
					lo_PartialFormatImage(context, state,tag);
				}
				else
				{
					lo_FormatImage(context, state, tag);
				}
			}
			break;

		/*
		 * Paragraph break, a single line of whitespace.
		 */
		case P_PARAGRAPH:
			lo_process_paragraph_tag(context, state, tag, style_struct);
			break;

		/*
		 * Division.  General alignment text block
		 */
		case P_DIVISION:
			/*
			 * The start of a division infers the end of
			 * the HEAD section of the HTML and starts the BODY
			 */
			state->top_state->in_head = FALSE;
			state->top_state->in_body = TRUE;

			if (state->in_paragraph != FALSE)
			{
				lo_CloseParagraph(context, &state, tag, 2);
			}
			lo_SetLineBreakState(context, state, FALSE,
								 LO_LINEFEED_BREAK_HARD, 1, FALSE);
			if (tag->is_end == FALSE)
			{
				PA_Block buff;
				char *str;
				int32 alignment;
				Bool is_multicol;

				/*
				 * By default inherit current alignment
				 * for DIV tags that set COLS but not
				 * ALIGN.
				 */
				if (state->align_stack != NULL)
				{
					alignment = state->align_stack->alignment;
				}
				else
				{
					alignment = LO_ALIGN_LEFT;
				}

				buff = lo_FetchParamValue(context, tag, PARAM_ALIGN);
				if (buff != NULL)
				{
					PA_LOCK(str, char *, buff);
					alignment =
					  (int32)lo_EvalDivisionAlignParam(str);
					PA_UNLOCK(buff);
					PA_FREE(buff);
				}

				is_multicol = FALSE;
				/*
				 * If a DIV tag has a COLS attribute of
				 * greater than 1, then treat it as a
				 * MULTICOL tag.
				 */
				buff = lo_FetchParamValue(context, tag, PARAM_COLS);
				if (buff != NULL)
				{
					int32 cols;

					PA_LOCK(str, char *, buff);
					cols = XP_ATOI(str);
					PA_UNLOCK(buff);
					PA_FREE(buff);
					if (cols > 1)
					{
						is_multicol = TRUE;
					}
				}

				if (is_multicol == FALSE)
				{
					lo_PushAlignment(state, tag->type, alignment);
				}
				else
				{
					lo_PushAlignment(state, P_MULTICOLUMN, alignment);
					/*
					lo_BeginMulticolumn(context, state, tag);
					*/
					lo_process_multicolumn_tag(context, state, tag);
				}
			}
			else
			{
				lo_AlignStack *aptr;
				Bool is_multicol;

				is_multicol = FALSE;
				aptr = lo_PopAlignment(state);
				if (aptr != NULL)
				{
					if (aptr->type == P_MULTICOLUMN)
					{
						is_multicol = TRUE;
					}
					XP_DELETE(aptr);
				}

				/*
				 * If this closing DIV closes a DIV that
				 * was acting like a MULTICOL, then
				 * we must close the fake MULTICOL tag
				 * here.
				 */
				if ((is_multicol != FALSE)&&
					(state->current_multicol != NULL))
				{
					/*
					lo_EndMulticolumn(context, state, tag,
						state->current_multicol);
					*/
					lo_process_multicolumn_tag(context, state, tag);
				}
			}
			break;

		/* added for stylesheet support
		 *
		 * a general container tag.  The default style is
		 * inline.  It doesn't do anything without associated
		 * stylesheet properties.
		 * it's just ignored in normal layout
		 */
		case P_SPAN:
		{
#ifdef DOM
			lo_process_span_tag(context, state, tag);
#endif
			break;
		}

		/*
		 * Just move the left and right margins in for this paragraph
		 * of text.
		 */
		case P_BLOCKQUOTE:
		  {
			int8 quote_type = QUOTE_NONE;
			PA_Block buff;
			/* 
			 * Get the param to see if this is a mailing quote. <BLOCKQUOTE TYPE=CITE>
			 */
			buff = lo_FetchParamValue(context, tag, PARAM_TYPE);
			if (buff != NULL) {
			  char *str;
			  PA_LOCK(str, char *, buff);
			  /*
			   * cite used to map to QUOTE_MQUOTE.
			   * We now use QUOTE_CITE.
			   */
			  if (pa_TagEqual("cite",str)) {
				quote_type = QUOTE_CITE;     
			  }
			  else if (pa_TagEqual("jwz",str)) {
				quote_type = QUOTE_JWZ;     
			  }
			  PA_UNLOCK(buff);
			  PA_FREE(buff);
			}
			
			/*
			 * The start of a blockquote infers the end of
			 * the HEAD section of the HTML and starts the BODY
			 */
			state->top_state->in_head = FALSE;
			state->top_state->in_body = TRUE;
			
			if (state->in_paragraph != FALSE)
			  {
				lo_CloseParagraph(context, &state, tag, 2);
			  }

			{
			  LO_BlockQuoteStruct *quote = (LO_BlockQuoteStruct*)lo_NewElement(context, state, LO_BLOCKQUOTE, NULL, 0);
			  
			  XP_ASSERT(quote);
			  if (!quote) 
			  {
				LO_UnlockLayout();
				return;
			  }
			  
			  quote->lo_any.type = LO_BLOCKQUOTE;
			  quote->lo_any.ele_id = NEXT_ELEMENT;

			  quote->lo_any.x = state->x;
			  quote->lo_any.y = state->y;
			  quote->lo_any.x_offset = 0;
			  quote->lo_any.y_offset = 0;
			  quote->lo_any.width = 0;
			  quote->lo_any.height = 0;
			  quote->lo_any.line_height = 0;
			  
			  quote->is_end = tag->is_end;
			  quote->quote_type = quote_type;
			  quote->tag = PA_CloneMDLTag(tag);

			  lo_AppendToLineList(context, state, (LO_Element*)quote, 0);
			  
			  lo_ProcessBlockQuoteElement(context, state, quote, FALSE);
			}

		  }
			break;

		/*
		 * An internal tag to allow libmocha/lm_doc.c's
		 * doc_open_stream to get the new document's top
		 * state struct early.
		 */
		case P_NSCP_OPEN:
			break;

		/*
		 * An internal tag to allow us to merge two
		 * possibly incomplete HTML docs as neatly
		 * as possible.
		 */
		case P_NSCP_CLOSE:
			if (tag->is_end == FALSE)
			{
				LO_CloseAllTags(context);
				/* the previous function call
				 * closes tags and free's the top
				 * style struct
				 */
				style_struct = NULL;
			}
			break;

		/*
		 * Normally unknown tags are just ignored.
		 * But if the unknown tag is in the head of a document, we
		 * assume it is a tag that can contain content, and ignore
		 * that content.
		 * WARNING:  In special cases with omitted </head> tags,
		 * this can result in the loss of body data!
		 */
		case P_UNKNOWN:
#ifndef MOCHA
		unknown:
#endif
			if (state->top_state->in_head != FALSE)
			{
			    char *tdata;
			    char *tptr;
			    char tchar;

			    PA_LOCK(tdata, char *, tag->data);

			    /*
			     * Throw out non-tags like <!doctype>
			     */
			    if ((tag->data_len < 1)||(tdata[0] == '!'))
			    {
				PA_UNLOCK(tag->data);
				break;
			    }

			    /*
			     * Remove the unknown tag name from any attributes
			     * it might have.
			     */
			    tptr = tdata;
			    while ((!XP_IS_SPACE(*tptr))&&(*tptr != '>'))
			    {
				tptr++;
			    }
			    tchar = *tptr;
			    *tptr = '\0';

			    /*
			     * Save the unknown tag name to match to its
			     * end tag.
			     */
			    if (tag->is_end == FALSE)
			    {
				if (state->top_state->unknown_head_tag == NULL)
				{
				    state->top_state->unknown_head_tag =
					XP_STRDUP(tdata);
				}
			    }
			    /*
			     * Else if we already have a saved start and this
			     * is the matching end.  Clear the knowledge
			     * that we are in an unknown head tag.
			     */
			    else
			    {
				if ((state->top_state->unknown_head_tag != NULL)
				    &&(XP_STRCASECMP(state->top_state->unknown_head_tag, tdata) == 0))
				{
				    XP_FREE(state->top_state->unknown_head_tag);
				    state->top_state->unknown_head_tag = NULL;
				}
			    }

			    *tptr = tchar;
			    PA_UNLOCK(tag->data);
			}
			break;

		default:
			break;
	  }

	if(!tag->is_end)
	{
		if(!lo_IsEmptyTag(tag->type))
		{
			lo_SetStyleSheetProperties(context, style_struct, tag);
		}
		else
		{
			/* get the new current state */
			int32 doc_id = XP_DOCID(context);
			lo_TopState *top_state = lo_FetchTopState(doc_id);
			state = lo_TopSubState(top_state);
		
		
			/* the tag is not a container so pop it off of
		 	 * the style stack
		 	 */
			LO_PopStyleTag(context, &state, tag);
			style_struct = NULL;
		}
	}

    /* the last argument is TRUE if we started in the head 
     */
	lo_PostLayoutTag( context, state, tag, started_in_head);

	LO_UnlockLayout();
}

void lo_PreLayoutTag(MWContext * context, lo_DocState *state, PA_Tag *tag)
{
	if (state->in_relayout == FALSE && 
        lo_IsTagInSourcedLayer(state, tag) == FALSE)
	{
		NET_StreamClass *stream = state->top_state->mocha_write_stream;

		/* @@@ small bug here
		 * if someone uses <script type=text/css>
		 * it will break.
		 */
		if ((stream != NULL) &&
			(state->top_state->in_script == SCRIPT_TYPE_NOT
			 || state->top_state->in_script == SCRIPT_TYPE_CSS
			 || state->top_state->in_script == SCRIPT_TYPE_JSSS))
		{
			switch (tag->type)
			{
				case P_TEXT:
					if (tag->data != NULL)
					{
						stream->put_block(stream,
										  (char *)tag->data,
										  tag->data_len);
					}
					break;

				case P_UNKNOWN:
					if (tag->data != NULL)
					{
						char *tag_str = PR_smprintf("<%s%s",
													tag->is_end ? "/" : "",
													tag->data);

						if (tag_str == NULL)
						{
							state->top_state->out_of_memory = TRUE;
						}
						else
						{
							stream->put_block(stream, tag_str,
											  XP_STRLEN(tag_str));
							XP_FREE(tag_str);
						}
					}
					break;

				case P_NSCP_OPEN:
				case P_NSCP_CLOSE:
				case P_NSCP_REBLOCK:
				case P_SCRIPT:
				case P_NOSCRIPT:
					break;

				default:
					{
						const char *tag_name;
						char *tag_str;

						tag_name = pa_PrintTagToken(tag->type);
						if ((tag->data != NULL) &&
							(*(char *)tag->data != '>'))
						{
							tag_str = PR_smprintf("<%s%s%s",
												  tag->is_end ? "/" : "",
												  tag_name, tag->data);
						}
						else
						{
							tag_str = PR_smprintf("<%s%s>",
												  tag->is_end ? "/" : "",
												  tag_name);
						}
						if (tag_str == NULL)
						{
							state->top_state->out_of_memory = TRUE;
						}
						else
						{
							stream->put_block(stream, tag_str,
											  XP_STRLEN(tag_str));
							XP_FREE(tag_str);
						}
					}
					break;
			}
		}
	}

	/*
	 * If we get a non-text tag, and we have some previous text
	 * elements we have been merging into state, flush those
	 * before processing the new tag.
	 * Exceptions: P_TITLE does special stuff with title text.
	 * Exceptions: P_TEXTAREA does special stuff with text.
	 * Exceptions: P_OPTION does special stuff with text.
	 * Exceptions: P_SELECT does special stuff with option text on close.
	 * Exceptions: P_SCRIPT (and P_STYLE) does special stuff with Mocha script text.
	 * Exceptions: P_CERTIFICATE does special stuff with text.
	 * Exceptions: P_BASEFONT may not change text flow.
	 * Exceptions: P_BASE only affects URLs not text flow.
	 */
	if ((tag->type != P_TEXT)&&(tag->type != P_TITLE)&&
		(tag->type != P_BASEFONT)&&
		(tag->type != P_TEXTAREA)&&
		(tag->type != P_OPTION)&&
		(tag->type != P_SELECT)&&
		(tag->type != P_SCRIPT)&&
		(tag->type != P_STYLE)&&
		(tag->type != P_CERTIFICATE))
	{
		lo_FlushTextBlock(context, state);
		if (state->top_state->out_of_memory)
		{
			return;
		}
	}

#ifdef EDITOR
	/*  
	 * The editor needs all text elements to be individual text elements.
	 * Force the line buffer to be flushed so each individual text item
	 * remains such.
	 */
	if (EDT_IS_EDITOR(context))
	{
		lo_FlushTextBlock(context, state);
		if (state->top_state->out_of_memory)
		{
			return;
		}
	}

	/* LTNOTE:
	 *	This is a kludge.  We should allow all Layout elements to have
	 *	pointers to edit elements.  As should also be setting the
	 *	edit_element at the point we actually parse the tag.
	 */

	if( !state->edit_force_offset )
	{
		state->edit_current_offset = 0;
	}

	if( tag->type == P_TEXT 
			|| tag->type == P_IMAGE 
			|| tag->type == P_HRULE
			|| tag->type == P_LINEBREAK
			)
	{
		state->edit_current_element = tag->edit_element;
		if( tag->type != P_TEXT )
		{
			state->edit_current_offset = 0;
		}
	}
	else 
	{
		state->edit_current_element = 0;
	}
#endif
}

void lo_PostLayoutTag(MWContext * context, lo_DocState *state, PA_Tag *tag, XP_Bool started_in_head)
{
	
#ifdef EDITOR
	/*  
	 * The editor needs all text elements to be individual text elements.
	 * Force the line buffer to be flushed so each individual text item
	 * remains such.
	 */
	if (EDT_IS_EDITOR(context))
	{
		lo_FlushTextBlock(context, state);
	}
#endif

#ifdef PICS_SUPPORT
    if(started_in_head || tag->type == P_META)
    {
   		int32 doc_id = XP_DOCID(context);
		lo_TopState *top_state = lo_FetchTopState(doc_id);

        /* true if we transition from the head to out of the head in
         * the last pass through laytags
         *
         * if started in head is false then we processed a meta tag
         * after the body.
         */
        if(top_state->in_head != started_in_head
            || !started_in_head)
        {
            char *fail_url = NULL;
            PICS_PassFailReturnVal status = lo_ProcessPicsLabel(context, 
                                                                state,
                                                                &fail_url);

            if(status == PICS_NO_RATINGS 
               && PICS_AreRatingsRequired())
            {
		/* check to see if the URL is already in an approved tree 
		 *
		 * if it is then we don't have to fail
		 */
		URL_Struct *URL_s = state->top_state->nurl;

		/* if the URL is not http or https then don't require a
		 * rating.  This allows mail and news and local files
		 * to work.  This is per marketings request, I'm not
		 * entirely sure it's the right thing to do
		 */
		if(!strncasecomp(URL_s->address, "http", 4)
		   && !PICS_CheckForValidTreeRating(URL_s->address))
		{
               	    /* if we just left the head and we didn't find
               	     * any valid ratings and ratings are required
               	     * then fail
               	     */
               	    status = PICS_RATINGS_FAILED;
               	    XP_FREEIF(fail_url);
               	    fail_url = PICS_RStoURL(NULL, URL_s->address);
            	}
	    }

            if(status == PICS_RATINGS_FAILED)
	        {
                /* emit a fake script */
                PA_Tag tmp_tag;
                
                tmp_tag.type = P_IMAGE;
				tmp_tag.is_end = FALSE;
				tmp_tag.newline_count = tag->newline_count;
				tmp_tag.data = NULL;
				tmp_tag.data_len = 0;
				tmp_tag.true_len = 0;
				tmp_tag.lo_data = NULL;
				tmp_tag.next = NULL;
				
                lo_ProcessScriptTag(context, state, &tmp_tag, NULL);

                top_state->in_script = SCRIPT_TYPE_MOCHA;
                tmp_tag.is_end = TRUE;

#define JS_PICS_SCRIPT "location.replace('%s');"

                state->line_buf = (PA_Block)PR_smprintf(JS_PICS_SCRIPT, 
                                    fail_url ? fail_url : "about:pics");
                XP_FREEIF(fail_url);

                if(state->line_buf)
                {
                    state->line_buf_len = XP_STRLEN((char*)state->line_buf);

                    lo_ProcessScriptTag(context, state, &tmp_tag, NULL); 
                }

                state->hide_content = TRUE;
	        }
        }
    }
#endif /* PICS_SUPPORT */
}

#ifdef OJI
static void lo_AddParam(PA_Tag* tag, char* aName, char* aValue)
{
	uint32 nameLen, valueLen, oldTagLen, newTagLen;
	char* tagData;

	nameLen = XP_STRLEN(aName);
	valueLen = XP_STRLEN(aValue);
	oldTagLen = XP_STRLEN((char*)(tag->data));
	newTagLen = oldTagLen + nameLen + valueLen + 2;

	tag->data = XP_REALLOC(tag->data, newTagLen+1);

	/* Remove the '>' character */
	tagData = (char*)(tag->data);
	tagData[oldTagLen-1] = 0;

	/* Add "aName=aValue" */
	XP_STRCAT(tagData, " ");
	XP_STRCAT(tagData, aName);
	XP_STRCAT(tagData, "=");
	XP_STRCAT(tagData, aValue);
	XP_STRCAT(tagData, ">");

	tag->data_len = newTagLen;
}
#endif /* OJI */

