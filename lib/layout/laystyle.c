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
 * STYLE tag support.
 */

#include "xp.h"
#include "libmocha.h"
#include "lo_ele.h"
#include "pa_parse.h"
#include "pa_tags.h"
#include "layout.h"
#include "laylayer.h"
#include "laystyle.h"
#include "prefapi.h"
#include "css.h"
#include "intl_csi.h"

void
LO_SetStyleObjectRefs(MWContext *context, void *tags, void *classes, void *ids)
{
	lo_TopState	*top_state;

	LO_LockLayout();
	top_state = lo_FetchTopState(XP_DOCID(context));
	if (top_state && top_state->style_stack) {
		SML_SetObjectRefs(top_state->style_stack, tags, classes, ids);
	}
	LO_UnlockLayout();
}

#define _STYLE_GET_WIDTH  0
#define _STYLE_GET_HEIGHT 1

char *
lo_ParseStyleSheetURL(char *url_string)
{
	if(!url_string)
		return NULL;

	/* look for url() around the url and strip it off */
	if(!strncasecomp(url_string, "url(", 4))
	{
		int end;
		url_string += 4; /* skip past "url(" */
		
		end = XP_STRLEN(url_string)-1;
		/* strip the ")" */
		if(url_string[end] == ')')
			url_string[end] = '\0';
	}

	return url_string;
}

PRIVATE int32
lo_get_font_height(MWContext *context, lo_DocState *state)
{
	int32 font_height = state->text_info.ascent +
           					state->text_info.descent;

	if ((font_height <= 0)&&(state->font_stack != NULL)&&
		(state->font_stack->text_attr != NULL))
	{
		lo_fillin_text_info(context, state);

		font_height = state->text_info.ascent + state->text_info.descent;
	}

	return font_height;
}

PRIVATE double
lo_get_css_scaling_factor_percentage(MWContext * context)
{
	if(context && context->fontScalingPercentage > 0.0)
		return(context->fontScalingPercentage);
    else 
		return(1.0);
}

/* converts any unit into pixels 
 * except fonts get converted to points
 */
void
LO_AdjustSSUnits(SS_Number *number, char *style_type, MWContext *context, lo_DocState *state)
{
	enum unit_type { EMS, EXS, PTS, PXS, PCS, REL, PER, INS, CMS, MMS };
	enum unit_type units;
    double scaler;
	double pixels_per_point=1.0;

	if(!number || !number->units || !style_type || !state || !context)
		return;

	XP_ASSERT(FEUNITS_X(1, context) == FEUNITS_Y(1, context));

#if (defined XP_WIN || defined XP_UNIX)
	/* mac has 72 dots per inch */
	pixels_per_point = context->YpixelsPerPoint;
#endif

#ifdef XP_WIN
	/* multiply ppp by the inverse of th LP to DP decreaser
	 * to obtain DP to LP
	 */
	if(context->type == MWContextPrint) 
		pixels_per_point *= 1000000.0/(double)FE_LPtoDPoint(context, 1000000);
#endif /* WP_WIN */

    scaler = lo_get_css_scaling_factor_percentage(context);

	if(!strncasecomp(number->units, "em", 2))
		units = EMS;
	else if(!strncasecomp(number->units, "ex", 2))
		units = EXS;
	else if(!strncasecomp(number->units, "px", 2))
		units = PXS;
	else if(!strncasecomp(number->units, "pt", 2))
		units = PTS;
	else if(!strncasecomp(number->units, "pc", 2))
		units = PCS;
	else if(!strncasecomp(number->units, "in", 2))
		units = INS;
	else if(!strncasecomp(number->units, "cm", 2))
		units = CMS;
	else if(!strncasecomp(number->units, "mm", 2))
		units = MMS;
	else if(!XP_STRCMP(number->units, "%"))
		units = PER;
	else if(!*number->units)
		units = REL;
	else
		units = PXS;

	if(units == PXS)
	{
		/* if going to a printer, adjust the pixels
		 * to look like there is 120 pixels per inch
		 * we will undo this for pixels below
		 * because layout does this too
		 */
		number->value = FEUNITS_X(number->value, context);

                /* No - we don't scale pixel values.  Pixels is pixels. */
		/* number->value *= scaler; */

	}
	else if(units == EMS || units == EXS)
	{
		int32 font_height = lo_get_font_height(context, state);

		if(units == EMS)
			number->value *= font_height;
		else 
			number->value *= font_height/2;
	}
	else if(units == PER)
	{
		/* percentage values depend on the type */
		if(!strcasecomp(style_type, LINE_HEIGHT_STYLE)
		   || !strcasecomp(style_type, FONTSIZE_STYLE))
		{
			/* line height is a percentage of the current font
			 * just like ems
			 */
			int32 font_height = lo_get_font_height(context, state);

			number->value *= ((double)font_height)/100;
		}
		else if(!strcasecomp(style_type, HEIGHT_STYLE))
		{
			int32 parent_layer_height = lo_GetEnclosingLayerHeight(state);
			number->value = ((number->value*parent_layer_height))/100;
		}
		else if(!strcasecomp(style_type, LAYER_WIDTH_STYLE))
		{
			int32 parent_layer_width = lo_GetEnclosingLayerWidth(state);
			number->value = ((number->value*parent_layer_width))/100;
		}
		else 
		{
			/* all other percentage values are relative to the width */
			if(!state->width)
				number->value *= state->win_width/100; 
			else
				number->value *= state->width/100; 
		}
	}
	else if(units == REL)
	{
		/* relative values depend on the type */
		if(!strcasecomp(style_type, LINE_HEIGHT_STYLE))
		{
			/* line height is a percentage of the current font
			 * just like ems
			 */
			int32 font_height = lo_get_font_height(context, state);

			number->value *= font_height;
		}
		else
		{
			/* all other relative units default to pixels */
			/* scale them as well since they are pixel units */
			number->value = FEUNITS_X(number->value, context);
        	number->value *= scaler;
		}
	}
	else if(units == PTS)
	{
		number->value *= pixels_per_point; /* now it's in pixels */
        number->value *= scaler;
	}
	else if(units == PCS)
	{
		/* 1 PCS == 12 PTS */
		number->value = number->value*12.0; /* now its in points */
		number->value *= pixels_per_point; /* now it's in pixels */
        number->value *= scaler;
	}
	else if(units == INS)
	{
		/* 1pt == 1/72in */
		number->value *= 72.0; /* now its in points */
		number->value *= pixels_per_point; /* now it's in pixels */
        number->value *= scaler;
	}
	else if(units == CMS)
	{
		/* 1in == 2.54cm  and 1in == 72 pts */
		number->value *= 72.0 / 2.54; /* now its in points */
		number->value *= pixels_per_point; /* now it's in pixels */
        number->value *= scaler;
	}
	else if(units == MMS)
	{
		/* 1in == 25.4mm  and 1in == 72 pts */
		number->value *= 72.0 / 25.4; /* now its in points */
		number->value *= pixels_per_point; /* now it's in pixels */
        number->value *= scaler;
	}
	else
	{
		XP_ASSERT(0);
	}

	/* the number has been converted to pixels now */

	XP_FREE(number->units);
	if(!strcasecomp(style_type, FONTSIZE_STYLE))
	{
		/* fonts are expressed in points not pixels
		 * convert to (or back) from pixels
		 */
		number->value /= pixels_per_point;
		number->units = XP_STRDUP("pts");
	}
	else
	{
		/* all non-font values are now scaled to
		 * the FEUNITS coordinate system.  Since
		 * layout will automaticly try to scale
		 * these values up to FEUNITS we need to
		 * down scale the values so that they
		 * are not too large.
		 *
		 * @@@ bug always X scaled not Y scaled
		 */
		number->value = number->value/(double)FEUNITS_X(1, context);
		number->units = XP_STRDUP("px");
	}
	
	return; /* conversion complete */

}
/* returns TRUE if the display: none property
 * is set
 */
XP_Bool
LO_CheckForContentHiding(lo_DocState *state)
{
	StyleStruct *style_struct;
	char *prop;
	XP_Bool hide_content = FALSE;

	if(!state || !state->top_state || !state->top_state->style_stack)
		return FALSE;

	/* check the current stack to see if we are currently hiding text */
	style_struct = STYLESTACK_GetStyleByIndex(state->top_state->style_stack, 0);

	if(style_struct)
	{
		prop = STYLESTRUCT_GetString(style_struct, DISPLAY_STYLE);

		if(prop)
		{
			if(!XP_STRCMP(prop, NONE_STYLE))
				hide_content = TRUE;
	
			XP_FREE(prop);
		}
	}
	
	return hide_content;
}

#define SS_ENABLED_PREF "browser.enable_style_sheets"
Bool lo_style_sheets_enabled = TRUE;

int PR_CALLBACK
lo_ss_enabled_changed(const char *domain, void *closure)
{
    Bool rv;

    /* the scaling factor should increase things by 10% */
    if(!PREF_GetBoolPref(SS_ENABLED_PREF, &rv))
        lo_style_sheets_enabled = rv;
    return 0;
}

XP_Bool
LO_StyleSheetsEnabled(MWContext *context)
{
    static XP_Bool first_time = TRUE;

    if(EDT_IS_EDITOR(context))
	return FALSE;

    if(first_time)
    {
        Bool    rv;
        /* the scaling factor should increase things by 10% */
        if(!PREF_GetBoolPref(SS_ENABLED_PREF, &rv))
           lo_style_sheets_enabled = rv;

        PREF_RegisterCallback(SS_ENABLED_PREF, lo_ss_enabled_changed, NULL);

	first_time = FALSE;
    }

    return(lo_style_sheets_enabled);
}

PA_Tag *
LO_CreateStyleSheetDummyTag(PA_Tag *old_tag)
{
	PA_Tag *new_tag = PA_CloneMDLTag(old_tag);
	if(new_tag)
	{
		new_tag->type = P_UNKNOWN;
		/* print the tag name into an attribute to save it */
		XP_FREE(new_tag->data);
		new_tag->data = (PA_Block)PR_smprintf(NS_STYLE_NAME_ATTR"=\"%s\" %s", 
										pa_PrintTagToken(old_tag->type),
										old_tag->data);
		new_tag->data_len = XP_STRLEN((char*)new_tag->data);
	}

	return(new_tag);
}

PUBLIC PushTagStatus
LO_PushTagOnStyleStack(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	char *class_name, *id_name, *style_value, *new_name;
	char *tag_name=NULL, *tag_name_attr = NULL;
	PushTagStatus rv;
	XP_Bool style_sheets_prev_encountered;
	char *buf;

	char *tableParams[] = 
	{
		NS_STYLE_NAME_ATTR,
		PARAM_CLASS,
		PARAM_ID,
		PARAM_STYLE,
	};

	char *temp_tag_name;
	char *tableParamValues[4];
	const int sizeOfParamsTable = 4;
	int k;

    INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);
    int16 win_csid = INTL_GetCSIWinCSID(c);

	if(!LO_StyleSheetsEnabled(context))
		return PUSH_TAG_ERROR;

	if(tag->type == P_TEXT || !state->top_state || !state->top_state->style_stack)
		return PUSH_TAG_ERROR;

	/*
	 * This is a performance enhancement
	 * We put this test here to keep from calling PA_FetchRequestedNameValues so many times
	 * as PA_FetchRequestedNameValues is a very time expensive routine
	 */
	style_sheets_prev_encountered = 
	   		STYLESTACK_IsSaveOn(state->top_state->style_stack);
	if( !style_sheets_prev_encountered )
	{
		PA_LOCK(buf, char *, tag->data);
		if( (!XP_STRCASESTR( buf, NS_STYLE_NAME_ATTR) && !XP_STRCASESTR( buf, PARAM_STYLE)) )
		{
			PA_UNLOCK(tag->data);
			return PUSH_TAG_ERROR;
		}
		PA_UNLOCK(tag->data);		
	}

	/*
	 * This is a performance enhancement
	 * We put this test here to keep from calling PA_FetchRequestedNameValues so many times
	 * as PA_FetchRequestedNameValues is a very time expensive rountine
	 */
	if( !style_sheets_prev_encountered || !state->in_relayout )
	{
		/* we only want ot fetch a hidden tag name from a secret attribute stored
		 * in the state->subdoc_tags list
		 * if state->in_relayout and style_sheets_prev_encountered  
		 */
		/* normally we want to ignore uknown tags
		 * but we need to pay some special attention
		 * to these tags in the relayout state
		 */
		if(tag->type == P_UNKNOWN )
		{
			rv = PUSH_TAG_ERROR;
			return(rv);
		}
			
	}

	for( k = 0; k < sizeOfParamsTable; k++ )
		tableParamValues[k] = NULL;

    PA_FetchRequestedNameValues( tag, tableParams, sizeOfParamsTable, tableParamValues, win_csid );  

	tag_name_attr = tableParamValues[0];
	tag_name = tag_name_attr;
	class_name = tableParamValues[1];
	id_name = tableParamValues[2];
	style_value = tableParamValues[3];


	/* normally we want to ignore uknown tags
	 * but we need to pay some special attention
	 * to these tags in the relayout state
	 */
	if(tag->type == P_UNKNOWN && !tag_name_attr)
	{
		rv = PUSH_TAG_ERROR;
		XP_FREEIF(style_value);
		XP_FREEIF(tag_name_attr);
		XP_FREEIF(class_name);
		XP_FREEIF(id_name);
		return(rv);
	}

	/*
	 *style_value freed in lo_ProcessStyleAttribute
	 */
	if(lo_ProcessStyleAttribute(context, state, tag, style_value))
	{
		/* enable the tag stack */
    		STYLESTACK_SetSaveOn(state->top_state->style_stack, TRUE);
		rv = PUSH_TAG_BLOCKED;
		XP_FREEIF(tag_name_attr);
		XP_FREEIF(class_name);
		XP_FREEIF(id_name);
		return(rv);
	}

	
	if(id_name)
	{
		new_name = CSS_ConvertToJSCompatibleName(id_name, FALSE);
		if(new_name)
		{
			XP_FREE(id_name);
			id_name = new_name;
		}
	}
	else
		id_name = PR_smprintf(NSIMPLICITID"%ld", state->top_state->tag_count);

    if(class_name)
	{
	    new_name = CSS_ConvertToJSCompatibleName(class_name, FALSE);
		if(new_name)
		{
			XP_FREE(class_name);
			class_name = new_name;
		}
	}

	if(!tag_name)
	{
		temp_tag_name = (char*)pa_PrintTagToken((int32)tag->type);
		tag_name = XP_STRDUP( temp_tag_name );
	}

	/* call into the style stack class */
	rv = STYLESTACK_PushTag(state->top_state->style_stack, 
					   tag_name, 
					   class_name, 
					   id_name);

	return(rv);
}

/* look for implicit tag pops.  For instance the close of a TR tag
 * implicitly pops all TD tags and any other open tags
 */
PUBLIC XP_Bool
LO_ImplicitPop(MWContext *context, lo_DocState **state, PA_Tag *tag)
{

	switch(tag->type) 
	{
		case P_LIST_ITEM:
			/* NOTE: there should be no end LI */
			if(tag->is_end)
				return FALSE;

			LO_PopAllTagsAbove(context, 
				   			state, 
				   			P_LIST_ITEM,
				   			P_LIST_ITEM,
							P_NUM_LIST,
							P_UNUM_LIST);
			return TRUE;

		case P_TABLE_DATA:
			LO_PopAllTagsAbove(context, 
				   			state, 
				   			P_TABLE_DATA,
				   			P_TABLE_DATA,
							P_TABLE_ROW,
							P_TABLE);
			return TRUE;
			
		case P_TABLE_ROW:
			LO_PopAllTagsAbove(context, 
				   			state, 
				   			P_TABLE_ROW,
							P_TABLE_ROW,
							P_TABLE,
							P_UNKNOWN);
			return TRUE;

		case P_TABLE:
			/* table begins don't implicitly close tables */
			if(tag->is_end)
				LO_PopAllTagsAbove(context, 
				   				state, 
		   						P_TABLE,
		   						P_TABLE,
								P_UNKNOWN,
								P_UNKNOWN);
			return TRUE;

		default:
			return FALSE;
	}

}

PUBLIC void
LO_PopStyleTagByIndex(MWContext *context, lo_DocState **state, 
					  TagType tag_type, int32 index)
 {
	StyleStruct * top_style;
	char *property, *page_break_property;
    SS_Number *bottom_margin; 
    SS_Number *bottom_padding; 
	SS_Number *pop_table;

	if(!(*state) || !context || !(*state)->top_state->style_stack || index < 0)
		return;

	top_style = STYLESTACK_GetStyleByIndex((*state)->top_state->style_stack,index);

	if(!top_style)
		return;

#ifdef DEBUG
	{
		TagStruct * stag = STYLESTACK_GetTagByIndex((*state)->top_state->style_stack, 0);
		stag = NULL;  /* set breakpoint here for viewing */
	}
#endif

	page_break_property = STYLESTRUCT_GetString(top_style, PAGE_BREAK_AFTER_STYLE);
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

	/* calculate the bottom margin here since we need the original font
	 * information before closing the table, but don't apply it until
	 * after the table is closed
	 */
	bottom_margin = STYLESTRUCT_GetNumber(top_style, BOTTOMMARGIN_STYLE);
	LO_AdjustSSUnits(bottom_margin, BOTTOMMARGIN_STYLE, context, *state);

	if((property = STYLESTRUCT_GetString(top_style, STYLE_NEED_TO_POP_PRE)) != NULL)
	{
		(*state)->preformatted = PRE_TEXT_NO;
		FE_EndPreSection(context);
		XP_FREE(property);
	}
	else if((property = STYLESTRUCT_GetString(top_style, 
											  STYLE_NEED_TO_RESET_PRE)) != NULL)
	{
		/* @@@ hack
		 * use this stack to reset pre if we exited from it
		 * via a whiteSpace: normal directive
		 */
		(*state)->preformatted = PRE_TEXT_YES;
		FE_BeginPreSection(context);
		XP_FREE(property);
	}

	/* pop any necessary stacks */
	if((pop_table = STYLESTRUCT_GetNumber(top_style, STYLE_NEED_TO_POP_TABLE)) != NULL)
	{
		lo_TopState *top_state;
		int32 doc_id;

		if(pop_table->value)
		{
			/* unset the property to prevent reentrancy from popping the
			 * table twice 
			 */
			STYLESTRUCT_SetString(top_style, 
								  STYLE_NEED_TO_POP_TABLE, 
								  "0", 
								  MAX_STYLESTRUCT_PRIORITY);
			lo_CloseTable(context, *state);

			/* get the new current state */
			doc_id = XP_DOCID(context);
			top_state = lo_FetchTopState(doc_id);
			*state = lo_TopSubState(top_state);
		}

		STYLESTRUCT_FreeSSNumber(top_style, pop_table);
	}
	else
	{

    	/* add bottom padding */
    	bottom_padding = STYLESTRUCT_GetNumber(top_style, BOTTOMPADDING_STYLE);
		LO_AdjustSSUnits(bottom_padding, BOTTOMPADDING_STYLE, context, *state);
    	if(bottom_padding && bottom_padding->value > 0)
    	{
        	int32 move_size = FEUNITS_Y((int32)bottom_padding->value, context);
        	lo_SetSoftLineBreakState(context, *state, FALSE, 1);
        	(*state)->y += move_size;
    	}
		STYLESTRUCT_FreeSSNumber(top_style, bottom_padding);
	}
		
	property = STYLESTRUCT_GetString(top_style, STYLE_NEED_TO_POP_ALIGNMENT);
	if(property)
	{
		lo_AlignStack *lptr;
		/* flush the line to get the alignment correct */
		lo_SetSoftLineBreakState(context, *state, FALSE, 1);
		lptr = lo_PopAlignment(*state);
       	if (lptr != NULL)
       		XP_DELETE(lptr);
		XP_FREE(property);
	}

	/* pop the last List */
	if((property = STYLESTRUCT_GetString(top_style, STYLE_NEED_TO_POP_MARGINS)) != NULL)
	{
		lo_ListStack *lptr;

		XP_FREE(property);

		if((*state)->list_stack)
		{
			lptr = lo_PopList(*state, NULL);
        	if (lptr != NULL)
        		XP_DELETE(lptr);
        	(*state)->left_margin = (*state)->list_stack->old_left_margin;
        	(*state)->right_margin = (*state)->list_stack->old_right_margin;

			lo_SetSoftLineBreakState(context, *state, FALSE, 1);
			(*state)->x = (*state)->left_margin;
		}	 
	}

	if((property = STYLESTRUCT_GetString(top_style, STYLE_NEED_TO_POP_LIST)) != NULL)
	{
		XP_FREE(property);

		lo_TeardownList(context, *state, NULL);
	}

	/* apply bottom margins */
	if(bottom_margin && bottom_margin->value > 0)
	{
        int32 move_size = FEUNITS_Y((int32)bottom_margin->value, context);
        lo_SetSoftLineBreakState(context, *state, FALSE, 1);
        (*state)->y += move_size;
	}
	STYLESTRUCT_FreeSSNumber(top_style, bottom_margin);

	property = STYLESTRUCT_GetString(top_style, STYLE_NEED_TO_POP_LINE_HEIGHT);
	if(property)
	{
		lo_LineHeightStack *lptr;
		lptr = lo_PopLineHeight(*state);
       	if (lptr != NULL)
       		XP_DELETE(lptr);
		XP_FREE(property);
	}

	/* pop layers last since they get pushed first */
	property = STYLESTRUCT_GetString(top_style, STYLE_NEED_TO_POP_LAYER);
	if(property)
	{
		lo_EndLayer(context, *state, PR_TRUE);
	}
	
	if((property = STYLESTRUCT_GetString(top_style, STYLE_NEED_TO_POP_FONT)) != NULL)
	{
		XP_FREE(property);
		lo_PopFont(*state, tag_type);
	}

	if((property = STYLESTRUCT_GetString(top_style, 
					    				STYLE_NEED_TO_POP_CONTENT_HIDING)) != NULL)
	{
		XP_FREE(property);
		(*state)->hide_content = FALSE;
	}

	STYLESTACK_PopTagByIndex((*state)->top_state->style_stack,(char*)pa_PrintTagToken((int32)tag_type), index);

}

PUBLIC void
LO_PopStyleTag(MWContext *context, lo_DocState **state, PA_Tag *tag)
{
	/* look for tags that need to be implicitly pop'd */
	if(!LO_ImplicitPop(context, state, tag))
		LO_PopStyleTagByIndex(context, state, tag->type, 0);

}

PRIVATE int 
compare_tag_type_and_name(TagType tag_type, char *tag_name)
{
	return strcasecomp((char *)pa_PrintTagToken((int32)tag_type), tag_name);
}

/* pops all the tags in the stack above the type specified.
 *
 * the not_below_this argument if non P_UNKNOWN specifies a tag type to
 * halt the search on.  By default this will search to the
 * bottom of the stack.
 *
 * for instance to pop all tags above TR but not below TABLE
 *  LO_PopAllTagsAbove(context, state, P_TR, P_TABLE, P_UNKNOWN, P_UNKNOWN)
 *
 * This function pops the tag of the specified type. (so TR would NOT still 
 * be on the stack)
 *
 * returns TRUE if tag_type was found and popped.
 * returns FALSE if tag_type not found or error
 */
PUBLIC XP_Bool
LO_PopAllTagsAbove(MWContext *context, 
				   lo_DocState **state,
				   TagType tag_type, 
				   TagType not_below_this,
				   TagType or_this,
				   TagType or_this_either)
{
	TagStruct * top_tag;
	int32 index = 0;

	if(!(*state) || !context || !(*state)->top_state->style_stack || index < 0)
		return FALSE;

	top_tag = STYLESTACK_GetTagByIndex((*state)->top_state->style_stack,index);

	while(top_tag)
	{
		if(!compare_tag_type_and_name(tag_type, top_tag->name))
		{
			/* found it */

			/* pop it and all tags above in stack order */
			for(; index > -1; index--)
				LO_PopStyleTagByIndex(context, state, tag_type, 0);
				
			return TRUE;
		}
 
		if(   (not_below_this != P_UNKNOWN
		       && !compare_tag_type_and_name(not_below_this, top_tag->name))
		   || (or_this != P_UNKNOWN
		       && !compare_tag_type_and_name(or_this, top_tag->name))
		   || (or_this_either != P_UNKNOWN
		       && !compare_tag_type_and_name(or_this_either, top_tag->name)))
			return FALSE; /* not found */

		index++;
		top_tag = STYLESTACK_GetTagByIndex((*state)->top_state->style_stack, index);
	}

	return FALSE;
}

PRIVATE int32
lo_get_width_or_height_from_style_sheet(MWContext *context, 
					lo_DocState *state, 
					int type)
{
	StyleStruct *top_style;
	SS_Number *ss_val;
	int32 val=0;

	/* get the most current style struct */
    if ( ! state->top_state->style_stack )
        return 0;
	top_style = STYLESTACK_GetStyleByIndex(state->top_state->style_stack, 0);

	if(!top_style)
		return 0;

	/* get the property */
	if(type == _STYLE_GET_WIDTH)
	{
		ss_val = STYLESTRUCT_GetNumber(top_style, WIDTH_STYLE);
	}
	else if(type == _STYLE_GET_HEIGHT)
	{
		ss_val = STYLESTRUCT_GetNumber(top_style, HEIGHT_STYLE);
	}
	else
	{
		XP_ASSERT(0);
		return 0;
	}

	if(ss_val)
	{
		/* do unit conversions here */
		LO_AdjustSSUnits(ss_val, WIDTH_STYLE, context, state);

		/* convert to fe units */
		if(type == _STYLE_GET_WIDTH)
			val = FEUNITS_X((int32)ss_val->value, context);
		else if(type == _STYLE_GET_HEIGHT)
			val = FEUNITS_Y((int32)ss_val->value, context);
		
		if(val < 1)
			val = 1;

		STYLESTRUCT_FreeSSNumber(top_style, ss_val);
		return val;
	}

	return 0;
}

PUBLIC int32
LO_GetWidthFromStyleSheet(MWContext *context, lo_DocState *state)
{
	return lo_get_width_or_height_from_style_sheet(context, 
												   state, 
												   _STYLE_GET_WIDTH);
}

PUBLIC int32
LO_GetHeightFromStyleSheet(MWContext *context, lo_DocState *state)
{
	return lo_get_width_or_height_from_style_sheet(context, 
												   state, 
												   _STYLE_GET_HEIGHT);
}

#if 0
static XP_Bool
IsJSS(MWContext *context, PA_Tag *tag)
{
	PA_Block    buff;
	char       *str;
	XP_Bool     bResult = FALSE;

	/* See if it's JavaScript style sheets or something else like CSS */
	buff = lo_FetchParamValue(context, tag, PARAM_TYPE);
	
	if (buff != NULL) {
		PA_LOCK(str, char *, buff);

		if (XP_STRCASECMP(str, "text/javascript") == 0)
			bResult = TRUE;

			PA_UNLOCK(buff);
			PA_FREE(buff);
		}

	return bResult;
}
#endif /* 0 */

/* This code is just a wrapper around lo_ProcessScriptTag() */
void
lo_ProcessStyleTag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	if(state && state->top_state && state->top_state->style_stack)
	{
		STYLESTACK_SetSaveOn(state->top_state->style_stack, TRUE);		
	}

	lo_ProcessScriptTag(context, state, tag, NULL);
}
