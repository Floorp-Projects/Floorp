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

/*
 * layimage.c - Image layout and fetching/prefetching
 *
 */

#include "xp.h"
#include "net.h"
#include "xp_rgb.h"
#include "pa_parse.h"
#include "layout.h"
#include "laylayer.h"
#include "laystyle.h"
#include "secnav.h"
#include "prefapi.h"
#include "xlate.h"
#include "layers.h"

extern int MK_OUT_OF_MEMORY;

#define IL_CLIENT               /* Defined by Image Library clients */
#include "libimg.h"             /* Image Library public API. */

#ifdef MOCHA
#include "libevent.h"
#endif /* MOCHA */

#ifdef PROFILE
#pragma profile on
#endif

#define IMAGE_DEF_DIM			50
#define IMAGE_DEF_BORDER		0
#define IMAGE_DEF_ANCHOR_BORDER		2
#define IMAGE_DEF_VERTICAL_SPACE	0
#define IMAGE_DEF_HORIZONTAL_SPACE	0
#define IMAGE_DEF_FLOAT_HORIZONTAL_SPACE	3

/* Closure data to be passed into lo_ImageObserver. */
typedef struct lo_ImageObsClosure {
    MWContext *context;
    LO_ImageStruct *lo_image;
    XP_ObserverList obs_list;
} lo_ImageObsClosure;

extern LO_TextStruct *lo_AltTextElement(MWContext *context);

PUBLIC char *
LO_GetBookmarkIconURLForPage(MWContext *context, BMIconType type)
{
	int32 doc_id;
	lo_TopState *top_state;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if (!top_state)
		return NULL;

	if(type == SMALL_BM_ICON)
		return top_state->small_bm_icon;
	else if(type == SMALL_BM_ICON)
		return top_state->large_bm_icon;

	return NULL;
}


/* Primitive image allocator also sets defaults for image structure */
static LO_ImageStruct *
lo_new_image_element(MWContext *context, lo_DocState *state,
                     void *edit_element, LO_TextAttr *tptr)
{
    LO_ImageStruct *image;
    image = (LO_ImageStruct *)lo_NewElement(context, state, LO_IMAGE, edit_element, 0);
    if (image == NULL)
        return NULL;

    /*
     * preserve the edit element and offset across the bzero.
     * The edit_element and edit_offset were set in lo_NewElement.
     */
    {
        void* save_edit_element = image->edit_element;
        int32 save_edit_offset = image->edit_offset;
        XP_BZERO(image, sizeof(*image));
        image->edit_element = save_edit_element;
        image->edit_offset = save_edit_offset;
    }

    image->type = LO_IMAGE;

    /*
     * Fill in default font information.
     */
    if (tptr == NULL)
    {
        LO_TextAttr tmp_attr;

        lo_SetDefaultFontAttr(state, &tmp_attr, context);
        tptr = lo_FetchTextAttr(state, &tmp_attr);
    }
    image->text_attr = tptr;
    image->image_attr = XP_NEW(LO_ImageAttr);
    if (image->image_attr == NULL)
    {	/* clean up all previously allocated objects */
        lo_FreeElement(context, (LO_Element *)image, FALSE);
        state->top_state->out_of_memory = TRUE;
        return 0;
    }

    image->image_attr->attrmask = 0;
    image->image_attr->layer_id = LO_DOCUMENT_LAYER_ID;
    image->image_attr->form_id = -1;
    image->image_attr->alignment = LO_ALIGN_BASELINE;
    image->image_attr->usemap_name = NULL;
    image->image_attr->usemap_ptr = NULL;
    
    image->ele_attrmask = 0;

    image->sel_start = -1;
    image->sel_end = -1;

    image->is_icon = FALSE;
	image->image_status = IL_START_URL;
	
    return image; 
}

PRIVATE
Bool
lo_parse_rgb(char *rgb, uint8 *red_ptr, uint8 *green_ptr, uint8 *blue_ptr, XP_Bool double_three_byte_codes)
{
	char *ptr;
	int32 i, j, len;
	int32 val, bval;
	int32 red_val, green_val, blue_val;
	intn bytes_per_val;

	*red_ptr = 0;
	*green_ptr = 0;
	*blue_ptr = 0;
	red_val = 0;
	green_val = 0;
	blue_val = 0;

	if (rgb == NULL)
	{
		return FALSE;
	}

	len = XP_STRLEN(rgb);
	if (len == 0)
	{
		return FALSE;
	}

	/*
	 * Strings not starting with a '#' are probably named colors.
	 * look them up in the xp lookup table.
	 */
	ptr = rgb;
	if (*ptr == '#')
	{
		ptr++;
		len--;
	}
	else
	{
		/*
		 * If we successfully look up a color name, return its RGB.
		 */
		if (XP_ColorNameToRGB(ptr, red_ptr, green_ptr, blue_ptr) == 0)
		{
			return TRUE;
		}
	}

	if (len == 0)
	{
		return FALSE;
	}

	bytes_per_val = (intn)((len + 2) / 3);
	if (bytes_per_val > 4)
	{
		bytes_per_val = 4;
	}

	for (j=0; j<3; j++)
	{
		val = 0;
		for (i=0; i<bytes_per_val; i++)
		{
			if (*ptr == '\0')
			{
				bval = 0;
			}
			else
			{
				bval = TOLOWER((unsigned char)*ptr);
				if ((bval >= '0')&&(bval <= '9'))
				{
					bval = bval - '0';
				}
				else if ((bval >= 'a')&&(bval <= 'f'))
				{
					bval = bval - 'a' + 10;
				}
				else
				{
					bval = 0;
				}
				ptr++;
			}
			val = (val << 4) + bval;
		}
		if (j == 0)
		{
			red_val = val;
		}
		else if (j == 1)
		{
			green_val = val;
		}
		else
		{
			blue_val = val;
		}
	}

	if(double_three_byte_codes && bytes_per_val == 1)
	{
		red_val = (red_val << 4) + red_val;
		green_val = (green_val << 4) + green_val;
		blue_val = (blue_val << 4) + blue_val;
	}

	while ((red_val > 255)||(green_val > 255)||(blue_val > 255))
	{
		red_val = (red_val >> 4);
		green_val = (green_val >> 4);
		blue_val = (blue_val >> 4);
	}
	*red_ptr = (uint8)red_val;
	*green_ptr = (uint8)green_val;
	*blue_ptr = (uint8)blue_val;
	return TRUE;
}

Bool
LO_ParseRGB(char *rgb, uint8 *red_ptr, uint8 *green_ptr, uint8 *blue_ptr)
{
	return lo_parse_rgb(rgb, red_ptr, green_ptr, blue_ptr, FALSE);
}


/* parse a comma separated list of 3 values
 *
 * Format is "rgb(red, green, blue)"
 * color values can be in integer 1-255 or percent 1%-100%
 * illegal values must be truncated 
 *
 * if there are fewer than three arguments this function will
 * set colors for whatever number exist.
 * if more than 3 args are passed in they will be ignored
 */
static Bool
lo_parse_style_RGB_functional_notation(char *rgb, 
		     uint8 *red_ptr, 
		     uint8 *green_ptr, 
		     uint8 *blue_ptr)
{
	int index=0;
	char *value, *end;
	int num_value;

	rgb = XP_StripLine(rgb);

    /* go past rgb(  and remove the last ')' */
#define _RGB "rgb"
    if(!strncasecomp(rgb, _RGB, sizeof(_RGB)-1))
    {
        char *end;

        /* go past "RGB" */
        rgb += sizeof(_RGB)-1;

		/* go past spaces */
		while(XP_IS_SPACE(*rgb)) rgb++;

		/* go past '(' */
		if(*rgb == '(')
			rgb++;

		/* kill any more spaces */
		while(XP_IS_SPACE(*rgb)) rgb++;

        end = &rgb[XP_STRLEN(rgb)-1];
        if(*end == ')')
            *end = '\0';
    }

	value = XP_STRTOK(rgb, ",");

	while(value && index < 3)
	{
		XP_Bool is_percentage = FALSE;
		int scaled_value;

		value = XP_StripLine(value);

		num_value = XP_ATOI(value);

		if(num_value < 0)
			num_value = 0;

        end = &rgb[XP_STRLEN(rgb)-1];
        if(*end == '%')
		{
			/* percentage value */

			if(num_value > 100)
				num_value = 100;

			scaled_value = (255 * num_value) / 100;
			
		}
		else
		{
			if(num_value > 255)
				num_value = 255;

			scaled_value = num_value;
		}

		if(index == 0)
			*red_ptr = scaled_value;
		else if(index == 1)
			*green_ptr = scaled_value;
		else if(index == 2)
			*blue_ptr = scaled_value;

		index++;
		value = XP_STRTOK(NULL, ",");
	}

	if(index < 3)
		return FALSE;
	else
		return TRUE;
}

Bool
LO_ParseStyleSheetRGB(char *rgb, uint8 *red_ptr, uint8 *green_ptr, uint8 *blue_ptr)
{
	/* can take the form of:
	 * #f00
	 * #ff0000
	 * or rgb(255, 0, 0)
	 * or rgb(100%, 0%, 0%)
	 * all are equivalent values
	 * need to truncate illegal values like 265 or 110%
	 */

	rgb = XP_StripLine(rgb);

	if(!strcasecomp(rgb, "transparent"))
	{
		*red_ptr = 0;
		*green_ptr = 0;
		*blue_ptr = 0;
		return FALSE;
	}
	else if(!strncasecomp(rgb, _RGB, sizeof(_RGB)-1)) /* functional notation */
	{
		return lo_parse_style_RGB_functional_notation(rgb, 
													  red_ptr,
													  green_ptr,
													  blue_ptr);
	}
	else /* assume #FF0000 or #F00 notation */
	{
		return lo_parse_rgb(rgb, red_ptr, green_ptr, blue_ptr, TRUE);
	}
}

/* 
 * Parses the BACKGROUND attribute (if one exists) and returns a url.
 * If there is no attribute specified, it returns NULL. For code sharing
 * between tags that have a BACKGROUND attribute.
 */
char *
lo_ParseBackgroundAttribute(MWContext *context, lo_DocState *state, 
                            PA_Tag *tag, Bool from_user)
{
	PA_Block buff, image_url;
	char *str;

	/*
	 * Get the required src parameter, and make the resulting
	 * url and absolute url.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_BACKGROUND);
	if (buff != NULL)
	{
		PA_Block new_buff;
		char *new_str;

		PA_LOCK(str, char *, buff);
		if (str != NULL)
		{
			int32 len;

			len = lo_StripTextWhitespace(str, XP_STRLEN(str));
		}
		/*
		 * Do not allow BACKGROUND=""
		 */
		if ((str != NULL)&&(*str != '\0'))
		{
			new_str = NET_MakeAbsoluteURL(
					state->top_state->base_url, str);
		}
		else
		{
			new_str = NULL;
		}
		if (new_str == NULL)
		{
			new_buff = NULL;
		}
		else
		{
			char *url;

			new_buff = PA_ALLOC(XP_STRLEN(new_str) + 1);
			if (new_buff != NULL)
			{
				PA_LOCK(url, char *, new_buff);
				XP_STRCPY(url, new_str);
				PA_UNLOCK(new_buff);
			}
			else
			{
				state->top_state->out_of_memory = TRUE;
			}
			XP_FREE(new_str);
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
		image_url = new_buff;

        /*
         * Make sure we don't load insecure images inside
         * a secure document.
         */
        if (state->top_state->security_level > 0)
        {
            PA_LOCK(str, char *, image_url);
            
            if (((from_user == FALSE)&&(str != NULL))||
                ((from_user != FALSE)&&(str != NULL)&&
                 (XP_STRNCMP(str, "file:/", 6) != 0)))
             {
                 if (NET_IsURLSecure(str) == FALSE)
                 {
                     PA_UNLOCK(image_url);
                     PA_FREE(image_url);
                     image_url = PA_ALLOC(
                         XP_STRLEN("internal-icon-insecure") + 1);
                     PA_LOCK(str, char *, image_url);
                     if (image_url != NULL)
                     {
                         XP_STRCPY(str, "internal-icon-insecure");
                     }
                     else
                     {
                         state->top_state->out_of_memory = TRUE;
                     }
                     if (state->top_state->insecure_images == FALSE)
                     {
                         state->top_state->insecure_images = TRUE;
                         SECNAV_SecurityDialog(context,
											   SD_INSECURE_DOCS_WITHIN_SECURE_DOCS_NOT_SHOWN);
                     }
                 }
             }
            PA_UNLOCK(image_url);
        }
        
        return (char*)image_url;
	}
    else
        return NULL;
}


void
lo_BodyForeground(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	PA_Block buff;
	char *str;
	uint8 red, green, blue;

	/*
	 * Don't do this is we had a TEXT from a previous BODY tag.
	 */
	if ((state->top_state->body_attr & BODY_ATTR_TEXT) == 0)
	{
		/*
		 * Get the document text TEXT parameter.
		 */
		buff = lo_FetchParamValue(context, tag, PARAM_TEXT);
		if (buff != NULL)
		{
            LO_Color text_fg;
            
			PA_LOCK(str, char *, buff);
			LO_ParseRGB(str, &red, &green, &blue);
			PA_UNLOCK(buff);
			PA_FREE(buff);

			text_fg.red = red;
			text_fg.green = green;
			text_fg.blue = blue;
            lo_ChangeBodyTextFGColor(context, state, &text_fg);
		}
	}

	/*
	 * Don't do this is we had a LINK from a previous BODY tag.
	 */
	if ((state->top_state->body_attr & BODY_ATTR_LINK) == 0)
	{
		/*
		 * Get the anchor text LINK parameter.
		 */
		buff = lo_FetchParamValue(context, tag, PARAM_LINK);
		if (buff != NULL)
		{
			state->top_state->body_attr |= BODY_ATTR_LINK;
			PA_LOCK(str, char *, buff);
			LO_ParseRGB(str, &red, &green, &blue);
			PA_UNLOCK(buff);
			PA_FREE(buff);

			state->anchor_color.red = red;
			state->anchor_color.green = green;
			state->anchor_color.blue = blue;
		}
	}

	/*
	 * Don't do this is we had a VLINK from a previous BODY tag.
	 */
	if ((state->top_state->body_attr & BODY_ATTR_VLINK) == 0)
	{
		/*
		 * Get the visited anchor text VLINK parameter.
		 */
		buff = lo_FetchParamValue(context, tag, PARAM_VLINK);
		if (buff != NULL)
		{
			state->top_state->body_attr |= BODY_ATTR_VLINK;
			PA_LOCK(str, char *, buff);
			LO_ParseRGB(str, &red, &green, &blue);
			PA_UNLOCK(buff);
			PA_FREE(buff);

			state->visited_anchor_color.red = red;
			state->visited_anchor_color.green = green;
			state->visited_anchor_color.blue = blue;
		}
	}

	/*
	 * Don't do this is we had a ALINK from a previous BODY tag.
	 */
	if ((state->top_state->body_attr & BODY_ATTR_ALINK) == 0)
	{
		/*
		 * Get the active anchor text ALINK parameter.
		 */
		buff = lo_FetchParamValue(context, tag, PARAM_ALINK);
		if (buff != NULL)
		{
			state->top_state->body_attr |= BODY_ATTR_ALINK;
			PA_LOCK(str, char *, buff);
			LO_ParseRGB(str, &red, &green, &blue);
			PA_UNLOCK(buff);
			PA_FREE(buff);

			state->active_anchor_color.red = red;
			state->active_anchor_color.green = green;
			state->active_anchor_color.blue = blue;
		}
	}
}


static void
lo_GetStyleSheetBodyBackgroundAndColor(MWContext *context,
                                       lo_DocState *state, 
                                       char **image_urlp,
                                       char **color_str,
									   lo_TileMode *tile_mode)
{
    char *image_url;
    char *absolute_image_url;
	char *tile_mode_prop;
	StyleStruct *ss;

    /* Set defaults in case something goes wrong below. */
    *color_str = NULL;
    *image_urlp = NULL;
    
    /* //CLM: This is null in Editor (sometimes?)*/
    if(!state->top_state->style_stack)
        return;
  
	/* get the top style */
	ss = STYLESTACK_GetStyleByIndex(state->top_state->style_stack, 0);

	/* see if it has a bgcolor attribute */
	if(!ss)
		return;

	*color_str = STYLESTRUCT_GetString(ss, BG_COLOR_STYLE);

	image_url = STYLESTRUCT_GetString(ss, BG_IMAGE_STYLE);

	if(!image_url)
		return;

	if(!strcasecomp(image_url, "none"))
	{
		XP_FREE(image_url);
		return;	
	}

	absolute_image_url = NET_MakeAbsoluteURL(state->top_state->base_url, 
											 lo_ParseStyleSheetURL(image_url));
	XP_FREE(image_url);

	if(!absolute_image_url)
		return;
    
    *image_urlp = absolute_image_url;

	/* get the tiling mode */
	tile_mode_prop = STYLESTRUCT_GetString(ss, BG_REPEAT_STYLE);
	if(tile_mode_prop)
	{
		if(!strcasecomp(tile_mode_prop, BG_REPEAT_NONE_STYLE))
			*tile_mode = LO_NO_TILE;
		else if(!strcasecomp(tile_mode_prop, BG_REPEAT_X_STYLE))
			*tile_mode = LO_TILE_HORIZ;
		else if(!strcasecomp(tile_mode_prop, BG_REPEAT_Y_STYLE))
			*tile_mode = LO_TILE_VERT;
		else 
			*tile_mode = LO_TILE_BOTH;
	}
}


void
lo_BodyBackground(MWContext *context, lo_DocState *state, PA_Tag *tag,
	Bool from_user)
{
    LO_Color rgb;
	PA_Block buff=NULL;
	char *color_str = NULL;
	char *image_url = NULL;
    CL_Layer *layer;
	lo_TileMode tile_mode = LO_TILE_BOTH;

    if (! context->compositor)
        return;

	/* if we are here then we must be in the BODY tag.
	 * Get the top state from the style sheet and get BG color and
	 * BG Image from it
	 */
	lo_GetStyleSheetBodyBackgroundAndColor(context,
										   state, 
										   &image_url,
										   &color_str,
										   &tile_mode);

    /* If there isn't a style sheet backdrop get one from the BODY's
       BACKGROUND attribute, if present */
    if (!image_url)
        image_url = lo_ParseBackgroundAttribute(context, state, tag, from_user);

	/*
	 * Get the BGCOLOR parameter.  This is for a color background
	 * instead of an image backdrop.  BACKGROUND if it exists overrides
	 * the BGCOLOR specification.
	 *
	 * If color_str is already set then we got the color from a stylesheet
	 */
	if(!color_str)
	{
		buff = lo_FetchParamValue(context, tag, PARAM_BGCOLOR);
        if (buff) {
            PA_LOCK(color_str, char *, buff);
            LO_ParseRGB(color_str, &rgb.red, &rgb.green, &rgb.blue);
            PA_UNLOCK(buff);
            PA_FREE(buff);
        }
	}
	else
    {
        LO_ParseStyleSheetRGB(color_str, &rgb.red, &rgb.green, &rgb.blue);
		XP_FREE(color_str);
	}

    layer = lo_CurrentLayer(state);

    /*
     * Don't do this is we had a BGCOLOR from a previous BODY tag.
     */
    if (((state->top_state->body_attr & BODY_ATTR_BGCOLOR) == 0)||
	(from_user != FALSE))
    {
	if ((color_str != NULL)&&(!lo_InsideLayer(state))&&(from_user == FALSE))
	{
		state->top_state->body_attr |= BODY_ATTR_BGCOLOR;
	}
	/* Set the document's solid background color */
	if (color_str && !lo_InsideLayer(state)) {
		state->top_state->doc_specified_bg = TRUE;
		state->text_bg.red = rgb.red;
		state->text_bg.green = rgb.green;
		state->text_bg.blue = rgb.blue;
	}
    
	if (color_str)
		LO_SetLayerBgColor(layer, &rgb);
	else if (!lo_InsideLayer(state))
		LO_SetLayerBgColor(layer, &state->text_bg);
    }
    
    /*
     * Don't do this is we had a BACKGROUND from a previous BODY tag.
     */
    if (((state->top_state->body_attr & BODY_ATTR_BACKGROUND) == 0)||
	(from_user != FALSE))
    {
	if ((image_url != NULL)&&(!lo_InsideLayer(state))&&(from_user == FALSE))
	{
		state->top_state->body_attr |= BODY_ATTR_BACKGROUND;
	}
	/* Set the document's backdrop */
	if (image_url) {
		if (!lo_InsideLayer(state))
			state->top_state->doc_specified_bg = TRUE;
		LO_SetLayerBackdropURL(layer, image_url);
		lo_SetLayerBackdropTileMode(layer, tile_mode);
		XP_FREE(image_url);
	}
    }
    
    lo_BodyForeground(context, state, tag);
}

static void lo_edt_AvoidImageBlock(LO_ImageStruct *image);

static void lo_edt_AvoidImageBlock(LO_ImageStruct *image)
{
    int originalWidth = 0;
    int originalHeight = 0;
    IL_GetNaturalDimensions( image->image_req, &originalWidth, &originalHeight);
    if ( originalWidth <= 0 ) {
        originalWidth = 10;
    }
    if ( originalHeight <= 0 ) {
        originalHeight = 10;
    }
    if (image->width == 0 )
    {
        image->width = originalWidth;
    }
    if (image->height == 0)
    {
        image->height = originalHeight;
    }
}

extern Bool
lo_FindMochaExpr(char *str, char **expr_start, char **expr_end);

void
lo_BlockedImageLayout(MWContext *context, lo_DocState *state, PA_Tag *tag,
                      char *base_url)
{
	LO_ImageStruct *image;
	PA_Block buff;
	char *str;
	int32 val;
	/* int32 doc_width; */
	PushTagStatus push_status;
	char *start, *end;
	
    XP_ObserverList image_obs_list;   /* List of observers for an image request. */

	/* this image layout is going on outside the context
	 * of normal layout.  We therfore need to manage the
	 * style stack in here as well as in normal layout
	 * mode so that we can set image widths
	 */
	push_status = LO_PushTagOnStyleStack(context, state, tag);

	/* 
	 * if there are any entities in this tag don't prefetch
	 *   it --- wait until we have had a chance to redo it
	 */
	if (lo_FindMochaExpr((char *)tag->data, &start, &end))
	    return;

	/*
	 * Fill in the image structure with default data
	 */
    image = lo_new_image_element(context, state, tag->edit_element, NULL);
    if (!image)
        return;
	image->ele_id = NEXT_ELEMENT;

	image->border_width = -1;
	image->border_vert_space = IMAGE_DEF_VERTICAL_SPACE;
	image->border_horiz_space = IMAGE_DEF_HORIZONTAL_SPACE;

	/*
	 * Check for the ISMAP parameter
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_ISMAP);
	if (buff != NULL)
	{
		image->image_attr->attrmask |= LO_ATTR_ISMAP;
		PA_FREE(buff);
	}

	/*
	 * Check for the USEMAP parameter.  The name is stored in the
	 * image attributes immediately, the map pointer will be
	 * stored in later.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_USEMAP);
	if (buff != NULL)
	{
		char *new_str;
		char *map_name;
		char *tptr;

		map_name = NULL;
		PA_LOCK(str, char *, buff);

		/*
		 * Make this an absolute URL
		 */
		if (str != NULL)
		{
			int32 len;

			len = lo_StripTextWhitespace(str, XP_STRLEN(str));
		}
		/*
		 * The IETF draft says evaluate #name relative to real,
		 * and not the base url.
		 */
		if ((str != NULL)&&(*str == '#'))
		{
			new_str = NET_MakeAbsoluteURL(
				state->top_state->url, str);
		}
		else
		{
			new_str = NET_MakeAbsoluteURL(
				base_url, str);
		}

		/*
		 * If we have a local URL, we can use the USEMAP.
		 */
		if ((new_str != NULL)&&
			(lo_IsAnchorInDoc(state, new_str) != FALSE))
		{
			tptr = strrchr(new_str, '#');
			if (tptr == NULL)
			{
				tptr = new_str;
			}
			else
			{
				tptr++;
			}
			map_name = XP_STRDUP(tptr);
			XP_FREE(new_str);
		}
		/*
		 * Else a non-local URL, right now we don't support this.
		 */
		else if (new_str != NULL)
		{
			XP_FREE(new_str);
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
		image->image_attr->usemap_name = map_name;
	}

	/*
	 * Check for an align parameter
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_ALIGN);
	if (buff != NULL)
	{
		Bool floating;

		floating = FALSE;
		PA_LOCK(str, char *, buff);
		image->image_attr->alignment = lo_EvalAlignParam(str,
			&floating);
		if (floating != FALSE)
		{
			image->ele_attrmask |= LO_ELE_FLOATING;
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}

	if(state->top_state->style_stack)
	{
		Bool floating = FALSE;

		lo_EvalStyleSheetAlignment(
				STYLESTACK_GetStyleByIndex(state->top_state->style_stack, 0), 
				&(image->image_attr->alignment),
				&floating);	 
		
		if (floating != FALSE)
		{
			image->ele_attrmask |= LO_ELE_FLOATING;
		}
	}

	/*
	 * Get the required src parameter, and make the resulting
	 * url and absolute url.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_SRC);
	if (buff != NULL)
	{
		PA_Block new_buff;
		char *new_str;

		new_buff = NULL;
		PA_LOCK(str, char *, buff);
		if (str != NULL)
		{
			int32 len;

			len = lo_StripTextWhitespace(str, XP_STRLEN(str));
		}
		new_str = NET_MakeAbsoluteURL(base_url, str);
		if (new_str != NULL)
		{
			char *url;

			new_buff = PA_ALLOC(XP_STRLEN(new_str) + 1);
			if (new_buff != NULL)
			{
				PA_LOCK(url, char *, new_buff);
				XP_STRCPY(url, new_str);
				PA_UNLOCK(new_buff);
			}
			XP_FREE(new_str);
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
		buff = new_buff;
		
		if (new_str == NULL || new_buff == NULL)
		{	/* clean up all previously allocated objects */
			lo_FreeElement(context, (LO_Element *)image, TRUE);
			state->top_state->out_of_memory = TRUE;
			return;
		}
	}
	image->image_url = buff;

	/*
	 * Get the optional lowsrc parameter, and make the resulting
	 * url and absolute url.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_LOWSRC);
	if (buff != NULL)
	{
	    PA_Block new_buff;
	    char *new_str;

	    new_buff = NULL;
	    PA_LOCK(str, char *, buff);
	    if (str != NULL)
	    {
		int32 len;

		len = lo_StripTextWhitespace(str, XP_STRLEN(str));
	    }
	    new_str = NET_MakeAbsoluteURL(base_url, str);
	    if (new_str != NULL)
	    {
		    char *url;

		    new_buff = PA_ALLOC(XP_STRLEN(new_str) + 1);
		    if (new_buff != NULL)
		    {
			    PA_LOCK(url, char *, new_buff);
			    XP_STRCPY(url, new_str);
			    PA_UNLOCK(new_buff);
		    }
		    XP_FREE(new_str);
	    }
	    PA_UNLOCK(buff);
	    PA_FREE(buff);
	    buff = new_buff;

	    if (new_str == NULL || new_buff == NULL)
	    {       /* clean up all previously allocated objects */
		    lo_FreeElement(context, (LO_Element *)image, TRUE);
		    state->top_state->out_of_memory = TRUE;
		    return;
	    }
	}
	image->lowres_image_url = buff;

	/*
	 * Get the alt parameter, and store the resulting
	 * text, and its length.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_ALT);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		image->alt_len = XP_STRLEN(str);
		image->alt_len = (int16)lo_StripTextNewlines(str,
						(int32)image->alt_len);
		PA_UNLOCK(buff);
	}
	image->alt = buff;

	/*
	 * Get the suppress feedback parameter, which suppresses the delayed icon,
     * temporary border and alt text when the image is coming in, and also
     * suppresses keyboard navigation feedback.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_SUPPRESS);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);

        if (!XP_STRNCASECMP(str, "true", 4))
            image->suppress_mode = LO_SUPPRESS;
        else if (!XP_STRNCASECMP(str, "false", 5))
            image->suppress_mode = LO_DONT_SUPPRESS;
        else
            image->suppress_mode = LO_SUPPRESS_UNDEFINED;

		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
	
	/*
	doc_width = state->right_margin - state->left_margin;
	*/

	/*
	 * Get the width parameter, in absolute or percentage.
	 * If percentage, make it absolute.
	 */
	/*
	buff = lo_FetchParamValue(context, tag, PARAM_WIDTH);
	if (buff != NULL)
	{
		Bool is_percent;

		PA_LOCK(str, char *, buff);
		val = lo_ValueOrPercent(str, &is_percent);
		if (is_percent != FALSE)
		{
			if (state->allow_percent_width == FALSE)
			{
				val = 0;
			}
			else
			{
				val = doc_width * val / 100;
			}
		}
		else
		{
			val = FEUNITS_X(val, context);
		}
		image->width = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
	*/

	buff = lo_FetchParamValue(context, tag, PARAM_WIDTH);
	if (buff != NULL)
	{
		Bool is_percent;

		PA_LOCK(str, char *, buff);
		val = lo_ValueOrPercent(str, &is_percent);
		if (is_percent != FALSE)
		{
			image->percent_width = val;
		}
		else
		{
			image->percent_width = 0;
			val = FEUNITS_X(val, context);
		}
		image->width = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
	

	/* disabled because it is broken by JS threading 
	 *
	 * val = LO_GetWidthFromStyleSheet(context, state);
	 * if(val)
	 *	image->width = val;
	 */

	/*
	 * Get the height parameter, in absolute or percentage.
	 * If percentage, make it absolute.
	 */

	/*
	buff = lo_FetchParamValue(context, tag, PARAM_HEIGHT);
	if (buff != NULL)
	{
		Bool is_percent;

		PA_LOCK(str, char *, buff);
		val = lo_ValueOrPercent(str, &is_percent);
		if (is_percent != FALSE)
		{
			if (state->allow_percent_height == FALSE)
			{
				val = 0;
			}
			else
			{
				val = state->win_height * val / 100;
			}
		}
		else
		{
			val = FEUNITS_Y(val, context);
		}
		image->height = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
	*/

	buff = lo_FetchParamValue(context, tag, PARAM_HEIGHT);
	if (buff != NULL)
	{
		Bool is_percent;

		PA_LOCK(str, char *, buff);
		val = lo_ValueOrPercent(str, &is_percent);
		if (is_percent != FALSE)
		{
			image->percent_height = val;
		}
		else
		{
			image->percent_height = 0;
			val = FEUNITS_Y(val, context);
		}
		image->height = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}

	/* disabled because it is broken by JS threading 
	 *
	 * val = LO_GetHeightFromStyleSheet(context, state);
	 * if(val)
	 *	image->height = val;
	 */

	/*
	 * Get the border parameter.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_BORDER);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		val = XP_ATOI(str);
		if (val < 0)
		{
			val = 0;
		}
		image->border_width = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
	image->border_width = FEUNITS_X(image->border_width, context);

	/*
	 * Get the extra vertical space parameter.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_VSPACE);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		val = XP_ATOI(str);
		if (val < 0)
		{
			val = 0;
		}
		image->border_vert_space = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
	image->border_vert_space = FEUNITS_Y(image->border_vert_space, context);

	/*
	 * Get the extra horizontal space parameter.
	 */
	if (image->ele_attrmask & LO_ELE_FLOATING)
	{
		image->border_horiz_space = IMAGE_DEF_FLOAT_HORIZONTAL_SPACE;
	}
	else
	{
		image->border_horiz_space = IMAGE_DEF_HORIZONTAL_SPACE;
	}
	buff = lo_FetchParamValue(context, tag, PARAM_HSPACE);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		val = XP_ATOI(str);
		if (val < 0)
		{
			val = 0;
		}
		image->border_horiz_space = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
	image->border_horiz_space = FEUNITS_X(image->border_horiz_space,
						context);

	/*
	 * Make sure we don't load insecure images inside
	 * a secure document.
	 */
	if (state->top_state->security_level > 0)
	{
		if (image->image_url != NULL)
		{
			PA_LOCK(str, char *, image->image_url);
			if (NET_IsURLSecure(str) == FALSE)
			{
				PA_UNLOCK(image->image_url);
				PA_FREE(image->image_url);
				image->image_url = PA_ALLOC(
				    XP_STRLEN("internal-icon-insecure") + 1);
				if (image->image_url != NULL)
				{
				    PA_LOCK(str, char *, image->image_url);
				    XP_STRCPY(str, "internal-icon-insecure");
				    PA_UNLOCK(image->image_url);
				}
				else
				{
				    state->top_state->out_of_memory = TRUE;
				}
				if (state->top_state->insecure_images == FALSE)
				{
					state->top_state->insecure_images = TRUE;
					SECNAV_SecurityDialog(context, SD_INSECURE_DOCS_WITHIN_SECURE_DOCS_NOT_SHOWN);
				}
			}
			else
			{
				PA_UNLOCK(image->image_url);
			}
		}
	}

    if (context->compositor) {
        image->layer = lo_CreateImageLayer(context, image, NULL);
    }

	/* Initiate the loading of this image. */
    image_obs_list = lo_NewImageObserverList(context, image);
    if (!image_obs_list)
        return;
	lo_GetImage(context, context->img_cx, image, image_obs_list,
                state->top_state->force_reload);

	tag->lo_data = (void *)image;

    if (state->top_state->style_stack)
        STYLESTACK_PopTag(state->top_state->style_stack,
                          (char*)pa_PrintTagToken((int32)tag->type));

}


void
lo_PartialFormatImage(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	LO_ImageStruct *image;
    int32 layer_id;
#ifdef MOCHA
    LM_ImageEvent mocha_event;
#endif /* MOCHA */

	image = (LO_ImageStruct *)tag->lo_data;
	if (image == NULL)
	{
		return;
	}

#ifdef MOCHA
    layer_id = lo_CurrentLayerId(state);
    image->layer_id = layer_id;
    /* Reflect the image into Javascript */
    lo_ReflectImage(context, state, tag, image, FALSE, layer_id);

    /* Send any pending events to JavaScript */
    if ((image->image_status == IL_IMAGE_COMPLETE) || 
        (image->image_status == IL_FRAME_COMPLETE) ||
        (image->image_status == IL_NOT_IN_CACHE)) {
        mocha_event = LM_IMGLOAD;
        ET_SendImageEvent(context, image, mocha_event);
    }
    else if (image->image_status == IL_ABORTED) {
        mocha_event = LM_IMGABORT;
        ET_SendImageEvent(context, image, mocha_event);
    }
    else if ((image->image_status == IL_ERROR_NO_DATA) ||
             (image->image_status == IL_ERROR_IMAGE_DATA_CORRUPT) ||
             (image->image_status == IL_ERROR_IMAGE_DATA_TRUNCATED) ||
             (image->image_status == IL_ERROR_IMAGE_DATA_ILLEGAL) ||
             (image->image_status == IL_ERROR_INTERNAL)) {
        mocha_event = LM_IMGERROR;
        ET_SendImageEvent(context, image, mocha_event);
    }
#endif

	tag->lo_data = NULL;

	/*
	 * Assign it a properly sequencial element id.
	 */

	/*
	image->ele_id = NEXT_ELEMENT;
	image->x = state->x;
	image->y = state->y;
	*/


	image->anchor_href = state->current_anchor;

	if (image->border_width < 0)
	{
		if ((image->anchor_href != NULL)||
		    (image->image_attr->usemap_name != NULL))
		{
			image->border_width = IMAGE_DEF_ANCHOR_BORDER;
		}
		else
		{
			image->border_width = IMAGE_DEF_BORDER;
		}
	
		image->border_width = FEUNITS_X(image->border_width, context);

	}

	if (state->font_stack == NULL)
	{
		LO_TextAttr tmp_attr;
		LO_TextAttr *tptr;

		/*
		 * Fill in default font information.
		 */
		lo_SetDefaultFontAttr(state, &tmp_attr, context);
		tptr = lo_FetchTextAttr(state, &tmp_attr);
		image->text_attr = tptr;
	}
	else
	{
		image->text_attr = state->font_stack->text_attr;
	}

	if ((image->text_attr != NULL)&&
		(image->text_attr->attrmask & LO_ATTR_ANCHOR))
	{
		image->image_attr->attrmask |= LO_ATTR_ANCHOR;
	}

	if (image->image_attr->usemap_name != NULL)
	{
		LO_TextAttr tmp_attr;

		lo_CopyTextAttr(image->text_attr, &tmp_attr);
		tmp_attr.fg.red =   STATE_UNVISITED_ANCHOR_RED(state);
		tmp_attr.fg.green = STATE_UNVISITED_ANCHOR_GREEN(state);
		tmp_attr.fg.blue =  STATE_UNVISITED_ANCHOR_BLUE(state);
		image->text_attr = lo_FetchTextAttr(state, &tmp_attr);
	}

	lo_FillInImageGeometry( state, image );

    /* During relayout the editor is not allowed to block. */
    if ( (image->width == 0)||(image->height == 0))
    {
        if ( EDT_IN_RELAYOUT( context ) ){
            lo_edt_AvoidImageBlock(image);
        }
    }

    /*
	 * We may have to block on this image until later
	 * when the front end can give us the width and height.
	 */
	if ((image->width == 0)||(image->height == 0))
	{
		state->top_state->layout_blocking_element = (LO_Element *)image;
	}
	else
	{
		lo_FinishImage(context, state, image);
	}
}


/*************************************
 * Function: lo_FormatImage
 *
 * Description: This function does all the work to format
 *	and lay out an image on a line of elements.
 *	Creates the new image structure.  Fills it
 *	in based on the parameters in the image tag.
 *	Calls the front end to start fetching the image
 *	and get image dimentions if necessary.
 *	Places image on line, breaking line if necessary.
 *
 * Params: Window context, document state, and the image tag data.
 *
 * Returns: Nothing.
 *************************************/
void
lo_FormatImage(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	LO_ImageStruct *image;
    LO_TextAttr *tptr = NULL;
	PA_Block buff;
	char *str;
	int32 val;
	/* int32 doc_width; */
	Bool is_a_form;
    XP_ObserverList image_obs_list;   /* List of observers for an image request. */
    lo_DocLists *doc_lists;
    int32 layer_id;

    doc_lists = lo_GetCurrentDocLists(state);
    
    if (state->font_stack)
        tptr = state->font_stack->text_attr;

	/*
	 * Fill in the image structure with default data
	 */
    image = lo_new_image_element(context, state, tag->edit_element, tptr);
    if (!image)
        return;

	
	/*
	image->ele_id = NEXT_ELEMENT;
	image->x = state->x;
	image->y = state->y;
	*/
	image->anchor_href = state->current_anchor;

	if (image->anchor_href != NULL)
	{
		image->border_width = IMAGE_DEF_ANCHOR_BORDER;
	}
	else
	{
		image->border_width = IMAGE_DEF_BORDER;
	}
	image->border_vert_space = IMAGE_DEF_VERTICAL_SPACE;
	image->border_horiz_space = IMAGE_DEF_HORIZONTAL_SPACE;

	if ((image->text_attr != NULL)&&
		(image->text_attr->attrmask & LO_ATTR_ANCHOR))
	{
		image->image_attr->attrmask |= LO_ATTR_ANCHOR;
	}

	if (tag->type == P_INPUT)
	{
		is_a_form = TRUE;
	}
	else
	{
		is_a_form = FALSE;
	}

	if (is_a_form != FALSE)
	{
		LO_TextAttr tmp_attr;

        image->image_attr->layer_id = lo_CurrentLayerId(state);
		image->image_attr->form_id = doc_lists->current_form_num;
		image->image_attr->attrmask |= LO_ATTR_ISFORM;
		image->border_width = IMAGE_DEF_ANCHOR_BORDER;
		lo_CopyTextAttr(image->text_attr, &tmp_attr);
		tmp_attr.fg.red =   STATE_UNVISITED_ANCHOR_RED(state);
		tmp_attr.fg.green = STATE_UNVISITED_ANCHOR_GREEN(state);
		tmp_attr.fg.blue =  STATE_UNVISITED_ANCHOR_BLUE(state);
		image->text_attr = lo_FetchTextAttr(state, &tmp_attr);
	}

	image->image_attr->usemap_name = NULL;
	image->image_attr->usemap_ptr = NULL;

	image->ele_attrmask = 0;

	image->sel_start = -1;
	image->sel_end = -1;

	/*
	 * Check for the ISMAP parameter
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_ISMAP);
	if (buff != NULL)
	{
		image->image_attr->attrmask |= LO_ATTR_ISMAP;
		PA_FREE(buff);
	}

	/*
	 * Check for the USEMAP parameter.  The name is stored in the
	 * image attributes immediately, the map pointer will be
	 * stored in later.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_USEMAP);
	if (buff != NULL)
	{
		char *new_str;
		char *map_name;
		char *tptr;

		map_name = NULL;
		PA_LOCK(str, char *, buff);

		/*
		 * Make this an absolute URL
		 */
		if (str != NULL)
		{
			int32 len;

			len = lo_StripTextWhitespace(str, XP_STRLEN(str));
		}
		/*
		 * The IETF draft says evaluate #name relative to real,
		 * and not the base url.
		 */
		if ((str != NULL)&&(*str == '#'))
		{
			new_str = NET_MakeAbsoluteURL(
				state->top_state->url, str);
		}
		else
		{
			new_str = NET_MakeAbsoluteURL(
				state->top_state->base_url, str);
		}

		/*
		 * If we have a local URL, we can use the USEMAP.
		 */
		if ((new_str != NULL)&&
			(lo_IsAnchorInDoc(state, new_str) != FALSE))
		{
			tptr = strrchr(new_str, '#');
			if (tptr == NULL)
			{
				tptr = new_str;
			}
			else
			{
				tptr++;
			}
			map_name = XP_STRDUP(tptr);
			XP_FREE(new_str);
		}
		/*
		 * Else a non-local URL, right now we don't support this.
		 */
		else if (new_str != NULL)
		{
			XP_FREE(new_str);
		}

		PA_UNLOCK(buff);
		PA_FREE(buff);
		image->image_attr->usemap_name = map_name;
		if (image->image_attr->usemap_name != NULL)
		{
			LO_TextAttr tmp_attr;

			image->border_width = IMAGE_DEF_ANCHOR_BORDER;
			lo_CopyTextAttr(image->text_attr, &tmp_attr);
			tmp_attr.fg.red =   STATE_UNVISITED_ANCHOR_RED(state);
			tmp_attr.fg.green = STATE_UNVISITED_ANCHOR_GREEN(state);
			tmp_attr.fg.blue =  STATE_UNVISITED_ANCHOR_BLUE(state);
			image->text_attr = lo_FetchTextAttr(state, &tmp_attr);
		}
	}

	/*
	 * Check for an align parameter
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_ALIGN);
	if (buff != NULL)
	{
		Bool floating;

		floating = FALSE;
		PA_LOCK(str, char *, buff);
		image->image_attr->alignment = lo_EvalAlignParam(str,
			&floating);
		if (floating != FALSE)
		{
			image->ele_attrmask |= LO_ELE_FLOATING;
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
	
	if(state->top_state->style_stack)
	{
		Bool floating = FALSE;

		lo_EvalStyleSheetAlignment(
				STYLESTACK_GetStyleByIndex(state->top_state->style_stack, 0), 
				&(image->image_attr->alignment),
				&floating);	 
		
		if (floating != FALSE)
		{
			image->ele_attrmask |= LO_ELE_FLOATING;
		}
	}

	/*
	 * Get the required src parameter, and make the resulting
	 * url and absolute url.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_SRC);
	if (buff != NULL)
	{
		PA_Block new_buff;
		char *new_str;

		PA_LOCK(str, char *, buff);
		if (str != NULL)
		{
			int32 len;

			len = lo_StripTextWhitespace(str, XP_STRLEN(str));
		}
		new_str = NET_MakeAbsoluteURL(state->top_state->base_url, str);
		if (new_str == NULL)
		{
			new_buff = NULL;
		}
		else
		{
			char *url;

			new_buff = PA_ALLOC(XP_STRLEN(new_str) + 1);
			if (new_buff != NULL)
			{
				PA_LOCK(url, char *, new_buff);
				XP_STRCPY(url, new_str);
				PA_UNLOCK(new_buff);
			}
			else
			{
				state->top_state->out_of_memory = TRUE;
			}

			/* Complete kludge: if this image has the magic SRC,
			 * then also parse an HREF from it (the IMG tag doesn't
			 * normally have an HREF parameter.)
			 *
			 * The front ends know that when the user clicks on an
			 * image with the magic SRC, that they may find the
			 * "real" URL in the HREF... Gag me.
			 */
			if (!XP_STRCMP(new_str, "internal-external-reconnect"))
			{
				PA_Block hbuf;

				hbuf = lo_FetchParamValue(context, tag, PARAM_HREF);
				if (hbuf != NULL)
				{
					image->anchor_href =
					    lo_NewAnchor(state, hbuf, NULL);
				}
			}

			XP_FREE(new_str);
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
		buff = new_buff;
	}
	image->image_url = buff;

	/*
	 * Get the optional lowsrc parameter, and make the resulting
	 * url and absolute url.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_LOWSRC);
	if (buff != NULL)
	{
	    PA_Block new_buff;
	    char *new_str;

	    PA_LOCK(str, char *, buff);
	    if (str != NULL)
	    {
		int32 len;

		len = lo_StripTextWhitespace(str, XP_STRLEN(str));
	    }
	    new_str = NET_MakeAbsoluteURL(state->top_state->base_url, str);
	    if (new_str == NULL)
	    {
		    new_buff = NULL;
	    }
	    else
	    {
		    char *url;

		    new_buff = PA_ALLOC(XP_STRLEN(new_str) + 1);
		    if (new_buff != NULL)
		    {
			    PA_LOCK(url, char *, new_buff);
			    XP_STRCPY(url, new_str);
			    PA_UNLOCK(new_buff);
		    }
		    else
		    {
			    state->top_state->out_of_memory = TRUE;
		    }
		    XP_FREE(new_str);
	    }
	    PA_UNLOCK(buff);
	    PA_FREE(buff);
	    buff = new_buff;
	}
	image->lowres_image_url = buff;

	/*
	 * Get the alt parameter, and store the resulting
	 * text, and its length.  Form the special image form
	 * element, store the name in alt.
	 */
	if (is_a_form != FALSE)
	{
		buff = lo_FetchParamValue(context, tag, PARAM_NAME);
	}
	else
	{
		buff = lo_FetchParamValue(context, tag, PARAM_ALT);
	}
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		image->alt_len = XP_STRLEN(str);
		image->alt_len = (int16)lo_StripTextNewlines(str,
						(int32)image->alt_len);
		PA_UNLOCK(buff);
	}
	image->alt = buff;

	/*
	doc_width = state->right_margin - state->left_margin;
	*/

	/*
	 * Get the width parameter, in absolute or percentage.
	 * If percentage, make it absolute.
	 */

	/*
	buff = lo_FetchParamValue(context, tag, PARAM_WIDTH);
	if (buff != NULL)
	{
		Bool is_percent;

		PA_LOCK(str, char *, buff);
		val = lo_ValueOrPercent(str, &is_percent);
		if (is_percent != FALSE)
		{
			if (state->allow_percent_width == FALSE)
			{
				val = 0;
			}
			else
			{
				val = doc_width * val / 100;
			}
		}
		else
		{
			val = FEUNITS_X(val, context);
		}
		image->width = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
	*/


	buff = lo_FetchParamValue(context, tag, PARAM_WIDTH);
	if (buff != NULL)
	{
		Bool is_percent;

		PA_LOCK(str, char *, buff);
		val = lo_ValueOrPercent(str, &is_percent);
		if (is_percent != FALSE)
		{
			image->percent_width = val;
		}
		else
		{
			image->percent_width = 0;
			val = FEUNITS_X(val, context);
		}
		image->width = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}


	/* disabled because it is broken by JS threading 
	 *
	 * val = LO_GetWidthFromStyleSheet(context, state);
	 * if(val)
	 *	image->width = val;
	 */

	/*
	 * Get the height parameter, in absolute or percentage.
	 * If percentage, make it absolute.
	 */
	/*
	buff = lo_FetchParamValue(context, tag, PARAM_HEIGHT);
	if (buff != NULL)
	{
		Bool is_percent;

		PA_LOCK(str, char *, buff);
		val = lo_ValueOrPercent(str, &is_percent);
		if (is_percent != FALSE)
		{
			if (state->allow_percent_height == FALSE)
			{
				val = 0;
			}
			else
			{
				val = state->win_height * val / 100;
			}
		}
		else
		{
			val = FEUNITS_Y(val, context);
		}
		image->height = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
	*/

	buff = lo_FetchParamValue(context, tag, PARAM_HEIGHT);
	if (buff != NULL)
	{
		Bool is_percent;

		PA_LOCK(str, char *, buff);
		val = lo_ValueOrPercent(str, &is_percent);
		if (is_percent != FALSE)
		{
			image->percent_height = val;
		}
		else
		{
			image->percent_height = 0;
			val = FEUNITS_Y(val, context);
		}
		image->height = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}

	/* disabled because it is broken by JS threading 
	 *
	 * val = LO_GetHeightFromStyleSheet(context, state);
	 * if(val)
	 *	image->height = val;
	 */

	/*
	 * Get the border parameter.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_BORDER);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		val = XP_ATOI(str);
		if (val < 0)
		{
			val = 0;
		}
		image->border_width = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
	image->border_width = FEUNITS_X(image->border_width, context);

	/*
	 * Get the extra vertical space parameter.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_VSPACE);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		val = XP_ATOI(str);
		if (val < 0)
		{
			val = 0;
		}
		image->border_vert_space = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
	image->border_vert_space = FEUNITS_Y(image->border_vert_space, context);

	/*
	 * Get the extra horizontal space parameter.
	 */
	if (image->ele_attrmask & LO_ELE_FLOATING)
	{
		image->border_horiz_space = IMAGE_DEF_FLOAT_HORIZONTAL_SPACE;
	}
	else
	{
		image->border_horiz_space = IMAGE_DEF_HORIZONTAL_SPACE;
	}
	buff = lo_FetchParamValue(context, tag, PARAM_HSPACE);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		val = XP_ATOI(str);
		if (val < 0)
		{
			val = 0;
		}
		image->border_horiz_space = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
	image->border_horiz_space = FEUNITS_X(image->border_horiz_space,
						context);


	lo_FillInImageGeometry( state, image );

	/*
	 * Make sure we don't load insecure images inside
	 * a secure document.
	 */
	if (state->top_state->security_level > 0)
	{
		if (image->image_url != NULL)
		{
			PA_LOCK(str, char *, image->image_url);
			if (NET_IsURLSecure(str) == FALSE)
			{
				PA_UNLOCK(image->image_url);
				PA_FREE(image->image_url);
				image->image_url = PA_ALLOC(
				    XP_STRLEN("internal-icon-insecure") + 1);
				if (image->image_url != NULL)
				{
				    PA_LOCK(str, char *, image->image_url);
				    XP_STRCPY(str, "internal-icon-insecure");
				    PA_UNLOCK(image->image_url);
				}
				else
				{
				    state->top_state->out_of_memory = TRUE;
				}
				if (state->top_state->insecure_images == FALSE)
				{
					state->top_state->insecure_images = TRUE;
					SECNAV_SecurityDialog(context, SD_INSECURE_DOCS_WITHIN_SECURE_DOCS_NOT_SHOWN);
				}
			}
			else
			{
				PA_UNLOCK(image->image_url);
			}
		}
	}

#ifdef MOCHA
    layer_id = lo_CurrentLayerId(state);
    image->layer_id = layer_id;
    
    /* Reflect the image into Javascript */
    lo_ReflectImage(context, state, tag, image, FALSE, layer_id);
#endif

    if (context->compositor) {
        image->layer = lo_CreateImageLayer(context, image, NULL);
    }

	/* Initiate the loading of this image. */
    image_obs_list = lo_NewImageObserverList(context, image);
    if (!image_obs_list)
        return;
	lo_GetImage(context, context->img_cx, image, image_obs_list,
                    state->top_state->force_reload);

    /* During relayout the editor is not allowed to block. */
    if ( (image->width == 0)||(image->height == 0))
    {
        if ( EDT_IN_RELAYOUT( context ) ){
            lo_edt_AvoidImageBlock(image);
        }
    }

    /*
	 * We may have to block on this image until later
	 * when the front end can give us the width and height.
	 */
	if ((image->width == 0)||(image->height == 0))
	{
		state->top_state->layout_blocking_element = (LO_Element *)image;
	}
	else
	{
		lo_FinishImage(context, state, image);
	}
}


void
lo_FinishImage(MWContext *context, lo_DocState *state, LO_ImageStruct *image)
{
	/* PA_Block buff;
	char *str;
	Bool line_break;
	*/
	int32 baseline_inc;
	int32 line_inc;
	/*
	int32 image_height, image_width;
	LO_TextStruct tmp_text;
	LO_TextInfo text_info;
	*/

	if (context->compositor && lo_CurrentLayerState(state)) {
	    CL_Layer *parent, *doc_layer, *content_layer;
            lo_LayerDocState *layer_state = lo_CurrentLayerState(state);
        
	    doc_layer = CL_GetCompositorRoot(context->compositor);
	    if (layer_state->layer == doc_layer) 
		parent = CL_GetLayerChildByName(doc_layer,
                                            LO_BODY_LAYER_NAME);
	    else
		parent = layer_state->layer;

            content_layer = CL_GetLayerChildByName(parent, 
                                                   LO_CONTENT_LAYER_NAME);
            XP_ASSERT(content_layer);
            if (content_layer)
                CL_InsertChild(parent, image->layer, content_layer, CL_ABOVE);
	}

	/*
	 * All this work is to get the text_info filled in for the current
	 * font in the font stack. Yuck, there must be a better way.
	 */
	
	/*
	memset (&tmp_text, 0, sizeof (tmp_text));
	buff = PA_ALLOC(1);
	if (buff == NULL)
	{
		state->top_state->out_of_memory = TRUE;
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
	*/

	/*
	 * Eventually this will never happen since we always
	 * have dims here from either the image tag itself or the
	 * front end.
	 */
	
	/*
	if (image->width == 0)
	{
		image->width = IMAGE_DEF_DIM;
	}
	if (image->height == 0)
	{
		image->height = IMAGE_DEF_DIM;
	}

	image_width = image->width + (2 * image->border_width) +
		(2 * image->border_horiz_space);
	image_height = image->height + (2 * image->border_width) +
		(2 * image->border_vert_space);

	*/


	/*
	 * SEVERE FLOW BREAK!  This may be a floating image,
	 * which means at this point we go do something completely
	 * different.
	 */
	if (image->ele_attrmask & LO_ELE_FLOATING)
	{	/*
		if (image->image_attr->alignment == LO_ALIGN_RIGHT)
		{
			if (state->right_margin_stack == NULL)
			{
				image->x = state->right_margin - image_width;
			}
			else
			{
				image->x = state->right_margin_stack->margin -
					image_width;
			}
			if (image->x < 0)
			{
				image->x = 0;
			}
		}
		else
		{
			image->x = state->left_margin;
		}

		image->y = -1;
		image->x_offset += (int16)image->border_horiz_space;
		image->y_offset += (int32)image->border_vert_space;

		lo_AddMarginStack(state, image->x, image->y,
			image->width, image->height,
			image->border_width,
			image->border_vert_space, image->border_horiz_space,
			image->image_attr->alignment);

		*/

		/*
		 * Insert this element into the float list.
		 */
		image->next = state->float_list;
		state->float_list = (LO_Element *)image;

		/* Update its offsets */
		image->x_offset += (int16)image->border_horiz_space;
		image->y_offset += (int32)image->border_vert_space;	

		/* 
		 * Append a dummy layout element in the line list.  When the relayout engine
		 * will see this dummy element, it will call lo_LayoutFloatImage()
		 */
		lo_AppendFloatInLineList( state, (LO_Element *) image, NULL );
		
		lo_LayoutFloatImage( context, state, image, TRUE);
		
		/*
		if (state->at_begin_line != FALSE)
		{
			lo_FindLineMargins(context, state);
			state->x = state->left_margin;
		}
		*/

		return;
	}

	/*
	 * Will this image make the line too wide.
	 */
	/*
	if ((state->x + image_width) > state->right_margin)
	{
		line_break = TRUE;
	}
	else
	{
		line_break = FALSE;
	}
	*/

	/*
	 * We cannot break a line if we have no break positions.
	 * Usually happens with a single line of unbreakable text.
	 */
	/*
	 * This doesn't work right now, I don't know why, seems we
	 * have lost the last old_break_pos somehow.
	 */
#ifdef FIX_THIS
	if ((line_break != FALSE)&&(state->break_pos == -1))
	{
		/*
		 * It may be possible to break a previous
		 * text element on the same line.
		 */
		if (state->old_break_pos != -1)
		{
			lo_BreakOldElement(context, state);
			line_break = FALSE;
		}
		else
		{
			line_break = FALSE;
		}
	}
#endif /* FIX_THIS */

	/*
	 * if we are at the beginning of the line.  There is
	 * no point in breaking, we are just too wide.
	 * Also don't break in unwrapped preformatted text.
	 * Also can't break inside a NOBR section.
	 */
	/*
	if ((state->at_begin_line != FALSE)||
		(state->preformatted == PRE_TEXT_YES)||
		(state->breakable == FALSE))
	{
		line_break = FALSE;
	}
	*/

	/*
	 * break on the image if we have
	 * a break.
	 */
	/*
	if (line_break != FALSE)
	{
	*/
		/*
		 * We need to make the elements sequential, linefeed
		 * before image.
		 */
	/*
		state->top_state->element_id = image->ele_id;

		lo_SoftLineBreak(context, state, TRUE);
		image->x = state->x;
		image->y = state->y;
		image->ele_id = NEXT_ELEMENT;
	}
	*/

	/*
	 * Figure out how to align this image.
	 * baseline_inc is how much to increase the baseline
	 * of previous element of this line.  line_inc is how
	 * much to increase the line height below the baseline.
	 */
	/*
	baseline_inc = 0;
	line_inc = 0;
	*/

	/*
	 * If we are at the beginning of a line, with no baseline,
	 * we first set baseline and line_height based on the current
	 * font, then place the image.
	 */
	if (state->baseline == 0)
	{
#ifdef ALIGN_IMAGE_WITH_FAKE_TEXT
		state->baseline = text_info.ascent;
		state->line_height = text_info.ascent + text_info.descent;
#else
		state->baseline = 0;
/* Why do we need this?
		if (state->line_height < 1)
		{
			state->line_height = 1;
		}
*/
#endif /* ALIGN_IMAGE_WITH_FAKE_TEXT */
	}

	/*
	lo_CalcAlignOffsets(state, &text_info, image->image_attr->alignment,
		image_width, image_height,
		&image->x_offset, &image->y_offset, &line_inc, &baseline_inc);

	image->x_offset += (int16)image->border_horiz_space;
	image->y_offset += (int32)image->border_vert_space;
	*/

	lo_LayoutInflowImage( context, state, image, FALSE, &line_inc, &baseline_inc);

	lo_AppendToLineList(context, state,
		(LO_Element *)image, baseline_inc);

	lo_UpdateStateAfterImageLayout( state, image, line_inc, baseline_inc );

	/*
	state->baseline += (intn) baseline_inc;
	state->line_height += (intn) (baseline_inc + line_inc);
	*/

	/*
	 * Clean up state
	 */

	/*
	state->x = state->x + image->x_offset +
		image_width - image->border_horiz_space;
	state->linefeed_state = 0;
	state->at_begin_line = FALSE;
	state->trailing_space = FALSE;
	state->cur_ele_type = LO_IMAGE;
	*/
}


Bool
LO_BlockedOnImage(MWContext *context, LO_ImageStruct *image)
{
	int32 doc_id;
	lo_TopState *top_state;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return(FALSE);
	}

	if ((top_state->layout_blocking_element != NULL)&&
	    (top_state->layout_blocking_element == (LO_Element *)image))
	{
		return(TRUE);
	}
	return(FALSE);
}


/*
 * Make sure the image library has been informed of all possible
 * image URLs that might be loaded by this document.
 */
void
lo_NoMoreImages(MWContext *context)
{
	PA_Tag *tag_ptr;
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;

	/*
	 * All blocked tags should be at the top level of state
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
#ifndef M12N                    /* XXXM12N Fix me. IL_NoMoreImages was used
                                   by the image library to detect when it
                                   could install a custom colormap. */
		/*
		 * No matter what, we must be done with all images now
		 */
		IL_NoMoreImages(context);
#else
        /* XXXM12N Need an observer callback. */
#endif /* M12N */
		return;
	}
	state = top_state->doc_state;
	tag_ptr = top_state->tags;

	/*
	 * Go through all the remaining blocked tags looking
	 * for <INPUT TYPE=IMAGE> tags.
	 */
	while (tag_ptr != NULL)
	{
		if (tag_ptr->type == P_INPUT)
		{
			PA_Block buff;
			char *str;
			int32 type;

			type = FORM_TYPE_TEXT;
			buff = lo_FetchParamValue(context, tag_ptr, PARAM_TYPE);
			if (buff != NULL)
			{
				PA_LOCK(str, char *, buff);
				type = lo_ResolveInputType(str);
				PA_UNLOCK(buff);
				PA_FREE(buff);
			}

			/*
			 * Prefetch this image.
			 * The ImageStruct created will get stuck into
			 * tag_ptr->lo_data, and freed later in 
			 * lo_ProcessInputTag().
			 */
			if (type == FORM_TYPE_IMAGE)
			{
				lo_BlockedImageLayout(context, state, tag_ptr, 
                                      top_state->base_url);
			}
		}
		tag_ptr = tag_ptr->next;
	}

#ifndef M12N                    /* XXXM12N Fix me. IL_NoMoreImages was used
                                   by the image library to detect when it
                                   could install a custom colormap. */
		/*
		 * No matter what, we must be done with all images now
		 */
		IL_NoMoreImages(context);
#else
        /* XXXM12N Need an observer callback. */
#endif /* M12N */
}

#ifdef EDITOR
LO_ImageStruct* LO_NewImageElement( MWContext* context ){
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	LO_ImageStruct *image;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
	    return 0;
	}
	state = top_state->doc_state;

    state->edit_current_element = 0;
	state->edit_current_offset = 0;

	/*
	 * Fill in the image structure with default data
	 */
    image = lo_new_image_element(context, state, NULL, NULL);
    if (!image)
        return NULL;

    image->border_width = -1;
    image->border_vert_space  = IMAGE_DEF_VERTICAL_SPACE;
    image->border_horiz_space = IMAGE_DEF_HORIZONTAL_SPACE;

	return image;
}
#endif        


/* Create a new observer list which will be passed into to IL_GetImage (via
   lo_GetImage) in order to permit an Image Library request to be
   monitored. The layout image observer is added to this new observer list
   before the function returns. */
XP_ObserverList
lo_NewImageObserverList(MWContext *context, LO_ImageStruct *lo_image)
{
	int32 doc_id;lo_TopState *top_state;
    XP_ObserverList image_obs_list; /* List of observer callbacks. */
    lo_ImageObsClosure *image_obs_closure; /* Closure data to be passed back
                                              to lo_ImageObserver. */
    NS_Error status;

	/* Get the unique document ID, and retrieve this document's layout
       state. */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if (!top_state)	{
	    return NULL;
	}

    /* Create an XP_ObserverList for this image.  The observable IL_ImageReq
       will be set by the Image Library. */
    status = XP_NewObserverList(NULL, &image_obs_list);
    if (status < 0) {
        if (status == MK_OUT_OF_MEMORY)
            top_state->out_of_memory = TRUE;
        return NULL;
    }

    /* Closure data for the image observer. */
    image_obs_closure = XP_NEW_ZAP(lo_ImageObsClosure);
    if (!image_obs_closure) {
        top_state->out_of_memory = TRUE;
        return NULL;
    }
    image_obs_closure->context = context;
    image_obs_closure->lo_image = lo_image;
    image_obs_closure->obs_list = image_obs_list;

    /* Add the layout image observer to the observer list. */
    status = XP_AddObserver(image_obs_list, lo_ImageObserver, image_obs_closure);
    if (status < 0) {
        if (status == MK_OUT_OF_MEMORY)
            top_state->out_of_memory = TRUE;
        return NULL;
    }

    return image_obs_list;
}


static void
lo_image_pixmap_update(MWContext *context, LO_ImageStruct *lo_image,
                       IL_Rect *update_rect)
{
    /* This lo_image cannot correspond to an icon since we are receiving
       pixmap update messages. */
    lo_image->is_icon = FALSE;

    /* Update the image layer if it is appropriate to do so. */
    if (context->compositor && lo_image && lo_image->layer &&
        lo_image->image_attr) {
        if (lo_image->image_attr->attrmask & LO_ATTR_BACKDROP) {
            /* Only update backdrops when the image has
               completed decoding, not when the pixmap is updated. */
        }
        else {
            /* Non-backdrop images only respond to pixmap update messages
               so that they can load incrementally. */
            XP_Rect rect, *valid_rect;

            rect.left = FEUNITS_X(update_rect->x_origin, context);
            rect.top = FEUNITS_Y(update_rect->y_origin, context);
            rect.bottom = rect.top + FEUNITS_Y(update_rect->height, context);
            rect.right = rect.left + FEUNITS_X(update_rect->width, context);

            valid_rect = &lo_image->valid_rect;
            XP_RectsBbox(&rect, valid_rect, valid_rect);
            CL_ResizeLayer(lo_image->layer, valid_rect->right - valid_rect->left,
                           valid_rect->bottom - valid_rect->top);
            CL_UpdateLayerRect(context->compositor, lo_image->layer, &rect,
                               PR_TRUE); /* XXX FALSE on the final update? */
        }
    }
}

static void
lo_frame_complete(MWContext *context, LO_ImageStruct *lo_image)
{
    PRBool is_anim, is_backdrop, is_cell_backdrop;

    XP_ASSERT(lo_image && lo_image->image_attr);
    if (!lo_image || !lo_image->image_attr) /* Paranoia */
        return;

    /* If we've already received a frame-complete message, then this
       must be an animated image. */
    is_anim = (PRBool)(lo_image->image_status == IL_FRAME_COMPLETE); 
    is_backdrop = (PRBool)(lo_image->image_attr->attrmask & LO_ATTR_BACKDROP);
	is_cell_backdrop = (PRBool)(lo_image->image_attr->attrmask & LO_ATTR_CELL_BACKDROP);

    if (!is_cell_backdrop)
	    ET_SendImageEvent(context, lo_image, LM_IMGLOAD);

    if (!lo_image->layer)
        return;

	if (is_backdrop) {
        /* Only update cell/layer/doc backdrops when the image has completed
           loading, not when the pixmap is updated. */
        XP_Rect rect = CL_MAX_RECT;
        CL_UpdateLayerRect(context->compositor, lo_image->layer, &rect, PR_FALSE);

        /* Unlike ordinary images, backdrop images are assumed to be transparent
           until they are displayed.  (Ordinary images are assumed to be opaque
           until they are determined to be transparent.) */
        CL_ChangeLayerFlag(lo_image->layer, CL_OPAQUE, (PRBool)!lo_image->is_transparent);
        
        /* If this animated GIF serves as a backdrop image, force
           offscreen compositing in order to reduce flicker. */
		if (is_anim)
			CL_ChangeLayerFlag(lo_image->layer, CL_PREFER_DRAW_OFFSCREEN, PR_TRUE);
    }
	else {
		/* We may need to reset the layer transparency if the current image
		   is opaque and it is replacing a previous image. */
		if (!lo_image->is_transparent) {
            CL_ChangeLayerFlag(lo_image->layer, CL_OPAQUE, PR_TRUE);
            CL_ChangeLayerFlag(lo_image->layer, CL_PREFER_DRAW_OFFSCREEN,
		                       PR_FALSE);
		}
	}

}


static void
lo_image_dimensions(MWContext *context, LO_ImageStruct *lo_image,
                    int width, int height)
{
    if(!lo_image)    {
        return;
    }

    width = FEUNITS_X(width, context);
    height = FEUNITS_Y(height, context);
    lo_image->width = width;
    lo_image->height = height;

    /* Don't call LO_SetImageInfo() for backdrop images. */
    if (lo_image->image_attr &&
        !(lo_image->image_attr->attrmask & LO_ATTR_BACKDROP))
        {
            LO_SetImageInfo (context, lo_image->ele_id, width, height);
        }
}

static void
lo_icon_box( MWContext *context, LO_ImageStruct *lo_image, int icon_width, int icon_height )
{
	LO_TextStruct text;
	LO_TextAttr text_attr;
	LO_TextInfo text_info;
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	char *image_url;
      
 /* ptn: The code that determines the area needed for the alt text
	will be put in a separate function so that it is the same
	code called for the display alttext as well as the area estimation.
	This code was for an immediate bug fix with time limits */
		
   	PA_LOCK(image_url, char *, lo_image->image_url);
	/* Determine whether the icon was explicitly requested by URL
	(in which case only the icon is drawn) or whether it needs
	to be drawn as a placeholder (along with the ALT text and
	placeholder border) as a result of a failed image request. */
	if (image_url &&
        (*image_url == 'i'              ||
		!XP_STRNCMP(image_url, "/mc-", 4)  ||
		!XP_STRNCMP(image_url, "/ns-", 4))) {
			lo_image->width = FEUNITS_X(icon_width, context);
			lo_image->height = FEUNITS_Y(icon_height, context);
	}
	else {
		lo_image->width = FEUNITS_X(icon_width + (2*ICON_X_OFFSET), 
                                            context);
		lo_image->height = FEUNITS_Y(icon_height + (2*ICON_Y_OFFSET), 
                                             context);
		
		if(lo_image->alt){
				
			/* Get the unique doc ID, and retrieve this doc's layout state. 
			*/
			doc_id = XP_DOCID(context);
			top_state = lo_FetchTopState(doc_id);
			if (!top_state)	{
				return;
			}
			state = lo_CurrentSubState(top_state->doc_state);
			memset (&text, 0, sizeof (text));
			text.text_attr = &text_attr;
			lo_SetDefaultFontAttr(state, text.text_attr, context);
			text.text = lo_image->alt;
			text.text_len = lo_image->alt_len;

 			FE_GetTextInfo(context, &text, &text_info);

			text.width = text_info.max_width;
			text.height = text_info.ascent + text_info.descent;
			if( text.width && text.height ){
				lo_image->width  = FEUNITS_X(icon_width + (4*ICON_X_OFFSET), context) + text.width;
				lo_image->height= (icon_height > text.height) ? 
						FEUNITS_Y(icon_height + (2*ICON_Y_OFFSET), context) : text.height + FEUNITS_Y(2*ICON_Y_OFFSET, context);
			}	
		}
	} 
	PA_UNLOCK(lo_image->image_url);
}

static void
lo_internal_image(MWContext *context, LO_ImageStruct *lo_image, int icon_number,
                  int icon_width, int icon_height)
{
    int bw = lo_image->border_width;


    /* Don't draw icons for mocha images. */
    if (lo_image->image_attr->attrmask & LO_ATTR_MOCHA_IMAGE)
        return;

    /* Mark this lo_image as an icon, so that drawing calls will use
       IL_DisplayIcon instead of IL_DisplaySubImage. */
    lo_image->is_icon = TRUE;
    lo_image->icon_number = icon_number;
	
	/* Set the lo_image->width and lo_image->height so that 
	icons and alt text are displayed, if present */

	if(!lo_image->width || !lo_image->height )
		lo_icon_box( context, lo_image, icon_width, icon_height );

    /* Tell layout the dimensions of the image. */
	LO_SetImageInfo (context, lo_image->ele_id, lo_image->width, lo_image->height);

    /* Refresh the area occupied by the image. */
    if (context->compositor && lo_image->layer) {
        XP_Rect update_rect, *valid_rect;

        update_rect.left = 0;
        update_rect.top = 0;
        update_rect.right = lo_image->width;
        update_rect.bottom = lo_image->height;

        valid_rect = &lo_image->valid_rect;
        XP_RectsBbox(&update_rect, valid_rect, valid_rect);
        CL_SetLayerBbox(lo_image->layer, valid_rect);
        CL_ChangeLayerFlag(lo_image->layer, CL_OPAQUE, PR_FALSE);

        CL_UpdateLayerRect(context->compositor, lo_image->layer, &update_rect,
                           PR_FALSE);
    }
}


static void
lo_image_incomplete(MWContext *context, LO_ImageStruct *lo_image,
					XP_ObservableMsg message, int icon_number,
					int icon_width, int icon_height, LM_ImageEvent mocha_event)
{

    if (lo_image->lowres_image_req) {
        /* If we already have a lowres image, then use that instead. */
        lo_image->image_req = lo_image->lowres_image_req;
    }
    else {
        lo_image->image_status = (uint16)message;
            
        if (lo_image->width && lo_image->height) {
            int valid_width, valid_height;
            XP_Rect *valid_rect = &lo_image->valid_rect;

            valid_width = valid_rect->right - valid_rect->left;
            valid_height = valid_rect->bottom - valid_rect->top;
            if (!valid_height || !valid_width)
                lo_internal_image(context, lo_image, icon_number,
                                  icon_width, icon_height);
        }
        else {
            lo_internal_image(context, lo_image, icon_number,
                              icon_width, icon_height);
        }

		if (lo_image->image_attr &&
			!(lo_image->image_attr->attrmask & LO_ATTR_CELL_BACKDROP))
                ET_SendImageEvent(context, lo_image, mocha_event);
    }
}

		
/* Image observer callback. */
void
lo_ImageObserver(XP_Observable observable, XP_ObservableMsg message,
                 void *message_data, void *closure)
{
    IL_MessageData *data = (IL_MessageData*)message_data;
    lo_ImageObsClosure *image_obs_closure = (lo_ImageObsClosure *)closure;
    MWContext *context;
    LO_ImageStruct *lo_image;
#ifdef MOCHA
    LM_ImageEvent mocha_event;
#endif /* MOCHA */

    if (image_obs_closure) {
        context = image_obs_closure->context;
        lo_image = image_obs_closure->lo_image;
    }

    /* The lo_image may not have a valid image handle at this point, or
       the image's URL property may have been changed in JavaScript,
       so give the lo_image the image handle passed in with the message
       data. */
    if(lo_image)    {
        lo_image->image_req = data->image_instance;
    }

     switch(message) {
    case IL_DIMENSIONS:
        lo_image_dimensions(context, lo_image, data->width, data->height);
        break;

    case IL_IS_TRANSPARENT:
        lo_image->is_transparent = TRUE;
        if (context->compositor && lo_image->layer) {
            CL_ChangeLayerFlag(lo_image->layer, CL_OPAQUE, PR_FALSE);
            CL_ChangeLayerFlag(lo_image->layer, CL_PREFER_DRAW_OFFSCREEN,
                               PR_TRUE);
        }
       break;

    case IL_DESCRIPTION:
        /* This must be a call to the stand alone image viewer, so
            the document title is set in the Title observer in 
           libimg/src/dummy_nc.c.*/  	   
        break;

    case IL_PIXMAP_UPDATE:
        lo_image_pixmap_update(context, lo_image, &data->update_rect);
        break;

    case IL_IMAGE_COMPLETE:
        lo_image->image_status = (uint16)message;
	if( (lo_image->lowres_image_url) != NULL ){
		if(!lo_image->lowres_image_req){
			XP_ObserverList tmp_obs_list;
			int32 doc_id;
			lo_TopState *top_state;
		 
			/* Get the unique document ID, and retrieve this document's layout
               state. 
			*/
			doc_id = XP_DOCID(context);
			top_state = lo_FetchTopState(doc_id);
			if (!top_state)	{
				return;
			}

            /* Hold on to lowres image handle in case the highres image
               fails. */
            lo_image->lowres_image_req = lo_image->image_req;

			tmp_obs_list = lo_NewImageObserverList(context, lo_image);
			/* Lowres image request is overwritten. Destruction of
               lowres image occurs when document is destroyed.	*/
			lo_GetImage(context, context->img_cx, lo_image, tmp_obs_list,
                        top_state->force_reload);
			}
	    }	
        break;
        
    case IL_FRAME_COMPLETE:
        lo_frame_complete(context, lo_image);
        lo_image->image_status = (uint16)message;
        break;

    case IL_NOT_IN_CACHE:
        if (lo_image->lowres_image_req) {
            /* If we already have a lowres image, then use that instead. */
            lo_image->image_req = lo_image->lowres_image_req;
        }
        else {
            lo_image->image_status = (uint16)message;
            lo_internal_image(context, lo_image, data->icon_number,
                              data->icon_width, data->icon_height);
            /* As far as JavaScript events go, we treat an image that wasn't in
               the cache the same as if it was actually loaded.  That way, a
               Javascript program will run the same way with images turned on
               or off. */
             mocha_event = LM_IMGLOAD;
			 if (lo_image->image_attr &&
			     !(lo_image->image_attr->attrmask & LO_ATTR_CELL_BACKDROP))
                      ET_SendImageEvent(context, lo_image, mocha_event);
        }
        break;

    case IL_ABORTED:
       lo_image_incomplete(context, lo_image, message, data->icon_number,
					data->icon_width, data->icon_height, LM_IMGABORT);
       break;

    case IL_ERROR_NO_DATA:
    case IL_ERROR_IMAGE_DATA_CORRUPT:
    case IL_ERROR_IMAGE_DATA_TRUNCATED:
    case IL_ERROR_IMAGE_DATA_ILLEGAL:
    case IL_ERROR_INTERNAL:
        lo_image_incomplete(context, lo_image, message, data->icon_number,
					data->icon_width, data->icon_height, LM_IMGERROR);
        break;

    case IL_INTERNAL_IMAGE:
        lo_internal_image(context, lo_image, data->icon_number, data->icon_width,
                          data->icon_height);
        break;

    case IL_IMAGE_DESTROYED:
        /* Remove ourself from the observer callback list. */
        XP_RemoveObserver(image_obs_closure->obs_list, lo_ImageObserver,
                          image_obs_closure);
        XP_FREE(image_obs_closure);
        lo_image->image_req = NULL;
		
        break;

    default:
        break;
    }
}

/* We want to write a title in the titlebar and tell all other
functions not to write over it. This is only used by il_load_image
for view streams in libimg/dummy_nc.c & external.c. */
void
lo_view_title( MWContext *context, char *title_str ){

    int32 doc_id;
    lo_TopState *top_state;
		 
	/* Get the unique document ID and the layout state.*/
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if (!top_state)	
          return;

    FE_SetDocTitle(context, title_str);
    top_state->have_title = TRUE;
    return;
}

#define FORCE_LOAD_ALL_IMAGES ((char *)1)

static char *force_load_images = NULL;
static XP_Bool autoload_images = TRUE;
static XP_Bool pref_initialized = FALSE;

/* remove Mac warning about missing prototype */
MODULE_PRIVATE int PR_CALLBACK lo_AutoloadPrefChangedFunc(const char *pref,
                                                          void *data);

MODULE_PRIVATE int PR_CALLBACK lo_AutoloadPrefChangedFunc(const char *pref,
                                                          void *data) 
{
	int status;

	if (!XP_STRCASECMP(pref,"general.always_load_images")) 
		status = PREF_GetBoolPref("general.always_load_images",
                                  &autoload_images);

	return status;
}

/* Inform layout of an image URL to be force loaded.  If all_images is TRUE,
   this indicates that all images are to be force loaded.  Note that this
   function does not actually trigger the loading of images. */
void LO_SetForceLoadImage(char *url, XP_Bool all_images)
{
    if (force_load_images &&
        force_load_images != FORCE_LOAD_ALL_IMAGES) {
        XP_FREE(force_load_images);
    }
    
    if (all_images) {
        force_load_images = FORCE_LOAD_ALL_IMAGES;
    }
    else {
        if (url)
            force_load_images = XP_STRDUP(url);
        else
            force_load_images = NULL;
    }
}

/* Initiate the loading of an image. */
void lo_GetImage(MWContext *context, IL_GroupContext *img_cx,
                 LO_ImageStruct *lo_image, XP_ObserverList obs_list,
                 NET_ReloadMethod requested_reload_method)
{

    XP_Bool net_request_allowed; /* Are we willing to make a net request? */
    NET_ReloadMethod reload_method;
    IL_NetContext *net_cx = NULL;
    IL_IRGB *trans_pixel;
    char *image_url, *lowres_image_url, *url_to_fetch;
	IL_ImageReq *dummy_ireq;

    /* Safety checks. */
    if (!context || !lo_image)
        return;

    /* Handle the TextFE. */
    if (context->type == MWContextText) {
        XL_GetTextImage(lo_image);
        return;
    }

    /* More safety checks. */
    if (!img_cx || !obs_list)
        return;

    /* Initialize the autoload images pref, if necessary. */
    if (!pref_initialized) {
        int status = PREF_GetBoolPref("general.always_load_images",
                                      &autoload_images);
        if (status == PREF_NOERROR) {
            PREF_RegisterCallback("general.always_load_images",
                                  lo_AutoloadPrefChangedFunc, NULL);
            pref_initialized = TRUE;
        }
    }

    /* Fetch the lowres image first if there is one.  Layout will call us
       again to fetch the hires image, at which point the lowres_image_url
       will be NULL. */
    if (lo_image->lowres_image_url) {
        PA_LOCK(lowres_image_url, char *, lo_image->lowres_image_url);
    }
    PA_LOCK(image_url, char *, lo_image->image_url);
    if ((context->type == MWContextPostScript) ||
        (context->type == MWContextPrint)) {
        url_to_fetch = image_url;
    }
    else {
		if ((lo_image->lowres_image_url)&&(!lo_image->lowres_image_req))
            url_to_fetch = lowres_image_url;
        else
            url_to_fetch = image_url;
    }
    
    /* Determine whether we are willing to load the image from the Network. */
    if ((context->type == MWContextPostScript) ||
        (context->type == MWContextPrint) ||
		(context->type == MWContextDialog) ||
        autoload_images ||
        (force_load_images == FORCE_LOAD_ALL_IMAGES) ||
        (force_load_images && image_url && !XP_STRCMP(force_load_images, image_url))) {
        net_request_allowed = TRUE;
    }
    else {
        net_request_allowed = FALSE;
    }

    /* JavaScript-generated images can change if the document is reloaded,
       so don't request a cache-only reload, even if layout requested one. */
    if (NET_URL_Type(url_to_fetch) == MOCHA_TYPE_URL)
        requested_reload_method = NET_DONT_RELOAD;
 
    /* Create a dummy Net Context for the Image Library to use for network
       operations.  This will be replaced by a true Net Context when the
       Network Library is modularized. */
    reload_method =
        net_request_allowed ? requested_reload_method : NET_CACHE_ONLY_RELOAD;
    net_cx = IL_NewDummyNetContext(context, reload_method);

    /* Determine whether to request a mask if this is a transparent image.
       In the case of a document backdrop, we ask the image library to fill
       in the transparent area with a solid color.  For all other transparent
       images, we force the creation of a mask by passing in NULL. */
    if (context->type == MWContextPostScript) {
        trans_pixel = XP_NEW_ZAP(IL_IRGB);
        if (trans_pixel)
            trans_pixel->red = trans_pixel->green = trans_pixel->blue = 0xff;
    }
#ifndef XP_WIN32
    else if (context->type == MWContextPrint) {
        trans_pixel = context->transparent_pixel;
    }
#endif
    else {
        if (lo_image->image_attr &&
            (lo_image->image_attr->attrmask & LO_ATTR_BACKDROP) &&
			!(lo_image->image_attr->attrmask & LO_ATTR_CELL_BACKDROP) &&
			!(lo_image->image_attr->attrmask & LO_ATTR_LAYER_BACKDROP)
			) {
            trans_pixel = context->transparent_pixel;
        }
        else {
#ifdef XP_WIN32
		XP_Bool backgroundPref = FALSE;
		PREF_GetBoolPref("browser.print_background",&backgroundPref);
		if (context->type == MWContextPrint && !backgroundPref)
	        trans_pixel = context->transparent_pixel;
		else
#endif
            trans_pixel = NULL;
        }
    }

    /* Fetch the image.  We ignore the return value and only set the lo_image's
       image handle in the observer.  Any context-specific scaling of images,
       e.g. for printing, should be handled by the Front End, so we divide the
       image dimensions by the context scaling factors. */
		dummy_ireq = IL_GetImage(url_to_fetch, img_cx, obs_list,
                                      trans_pixel,
                                      lo_image->width / context->convertPixX,
                                      lo_image->height / context->convertPixY,
                                      0, net_cx);

		if(( dummy_ireq != lo_image->lowres_image_req ) && url_to_fetch )
			lo_image->image_req = dummy_ireq;

    /* Destroy the transparent pixel if this is a PostScript context. */
    if ((context->type == MWContextPostScript) && trans_pixel)
        XP_FREE(trans_pixel);

    /* The Image Library clones the dummy Net Context, so it safe to destroy
       it. */
    IL_DestroyDummyNetContext(net_cx);

    if (lo_image->lowres_image_url)
        PA_UNLOCK(lo_image->lowres_image_url);
    PA_UNLOCK(lo_image->image_url);
}

/* 
 * Fills in x, y, % width, % height of image
 * Does not change anything for absolute width/height images 
 */

void lo_FillInImageGeometry( lo_DocState *state, LO_ImageStruct *image )
{
	int32 doc_width;

	image->ele_id = NEXT_ELEMENT;
	image->x = state->x;
	image->y = state->y;

	/* Set image->width if image has a % width specified */
	doc_width = state->right_margin - state->left_margin;
	if (image->percent_width > 0) {
		int32 val = image->percent_width;
		if (state->allow_percent_width == FALSE) {
			val = 0;
		}
		else {
			val = doc_width * val / 100;
		}
		image->width = val;
	}

	/* Set image->height if image has a % height specified */
	if (image->percent_height > 0) {
		int32 val = image->percent_height;
		if (state->allow_percent_height == FALSE) {
			val = 0;
		}
		else {
			val = state->win_height * val / 100;
		}
		image->height = val;
	}
}


/* Look into possibility of reusing for other floating elements? */
void lo_LayoutFloatImage( MWContext *context, lo_DocState *state, LO_ImageStruct *image, Bool updateFE )
{
	int32 image_width;

	image_width = image->width + (2 * image->border_width) + 
		(2 * image->border_horiz_space);


	if (image->image_attr->alignment == LO_ALIGN_RIGHT)
	{
		if (state->right_margin_stack == NULL)
		{
			image->x = state->right_margin - image_width;
		}
		else
		{
			image->x = state->right_margin_stack->margin -
				image_width;
		}
		if (image->x < 0)
		{
			image->x = 0;
		}
	}
	else
	{
		image->x = state->left_margin;
	}

	image->y = -1;
	/*
	image->x_offset += (int16)image->border_horiz_space;
	image->y_offset += (int32)image->border_vert_space;	
	*/
	lo_AddMarginStack(state, image->x, image->y,
		image->width, image->height,
		image->border_width,
		image->border_vert_space, image->border_horiz_space,
		image->image_attr->alignment);

	if (state->at_begin_line != FALSE)
	{
		lo_FindLineMargins(context, state, updateFE);
		state->x = state->left_margin;
	}

}

void lo_LayoutInflowImage(MWContext *context, lo_DocState *state, LO_ImageStruct *image,
						  Bool inRelayout, int32 *line_inc, int32 *baseline_inc)
{
	PA_Block buff;
	char *str;
	LO_TextStruct tmp_text;
	LO_TextInfo text_info;
	int32 image_height, image_width;
	Bool line_break;

	/* 
	int32 baseline_inc;
	int32 line_inc;
	*/

	/*
	 * Eventually this will never happen since we always
	 * have dims here from either the image tag itself or the
	 * front end.
	 */
	if (image->width == 0)
	{
		image->width = IMAGE_DEF_DIM;
	}
	if (image->height == 0)
	{
		image->height = IMAGE_DEF_DIM;
	}

	image_width = image->width + (2 * image->border_width) +
		(2 * image->border_horiz_space);
	image_height = image->height + (2 * image->border_width) +
		(2 * image->border_vert_space);

	/*
	 * Will this image make the line too wide.
	 */
	if ((state->x + image_width) > state->right_margin)
	{
		line_break = TRUE;
	}
	else
	{
		line_break = FALSE;
	}

	/*
	 * if we are at the beginning of the line.  There is
	 * no point in breaking, we are just too wide.
	 * Also don't break in unwrapped preformatted text.
	 * Also can't break inside a NOBR section.
	 */
	if ((state->at_begin_line != FALSE)||
		(state->preformatted == PRE_TEXT_YES)||
		(state->breakable == FALSE))
	{
		line_break = FALSE;
	}

	/*
	 * break on the image if we have
	 * a break.
	 */
	if (line_break != FALSE)
	{
		/*
		 * We need to make the elements sequential, linefeed
		 * before image.
		 */
		state->top_state->element_id = image->ele_id;

		if (!inRelayout)
		{
			lo_SoftLineBreak(context, state, TRUE);
		}
		else {
			lo_rl_AddSoftBreakAndFlushLine(context, state);
		}
		image->x = state->x;
		image->y = state->y;
		image->ele_id = NEXT_ELEMENT;
	}

	/*
	 * Figure out how to align this image.
	 * baseline_inc is how much to increase the baseline
	 * of previous element of this line.  line_inc is how
	 * much to increase the line height below the baseline.
	 */
	*baseline_inc = 0;
	*line_inc = 0;

	/*
	 * All this work is to get the text_info filled in for the current
	 * font in the font stack. Yuck, there must be a better way.
	 */
	memset (&tmp_text, 0, sizeof (tmp_text));
	buff = PA_ALLOC(1);
	if (buff == NULL)
	{
		state->top_state->out_of_memory = TRUE;
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

	lo_CalcAlignOffsets(state, &text_info, image->image_attr->alignment,
		image_width, image_height,
		&image->x_offset, &image->y_offset, line_inc, baseline_inc);

	image->x_offset += (int16)image->border_horiz_space;
	image->y_offset += (int32)image->border_vert_space;
}


void lo_UpdateStateAfterImageLayout( lo_DocState *state, LO_ImageStruct *image, int32 line_inc, int32 baseline_inc )
{
	int32 image_width;
	int32 x, y;
	
	image_width = image->width + (2 * image->border_width) +
		(2 * image->border_horiz_space);

	state->baseline += (intn) baseline_inc;
	state->line_height += (intn) (baseline_inc + line_inc);
	
	/*
	 * Clean up state
	 */
	state->x = state->x + image->x_offset +
		image_width - image->border_horiz_space;
	state->linefeed_state = 0;
	state->at_begin_line = FALSE;
	state->trailing_space = FALSE;
	state->cur_ele_type = LO_IMAGE;

	/* Determine the new position of the layer. */
	x = image->x + image->x_offset + image->border_width;
	y = image->y + image->y_offset + image->border_width;

    /* Move layer to new position */
	if (image->layer != NULL)
		CL_MoveLayer(image->layer,	x, y);
}

#ifdef PROFILE
#pragma profile off
#endif
