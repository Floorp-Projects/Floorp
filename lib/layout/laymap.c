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


extern Bool lo_is_location_in_poly(int32 x, int32 y,
				int32 *coords, int32 coord_cnt);

/* #ifndef NO_TAB_NAVIGATION */
extern Bool lo_find_location_in_poly(int32 *xx, int32 *yy,
				int32 *coords, int32 coord_cnt);
/* NO_TAB_NAVIGATION  */


/*
 * This function parses a list of numbers (coordinates).
 * The numbers can have any integer value, and it is a comma separated list.
 * (optionally, whitespace can replace commas as separators in the list)
 */
int32 *
lo_parse_coord_list(char *str, int32 *value_cnt, Bool must_be_odd)
{
	char *tptr;
	char *n_str;
	int32 i, cnt, acnt;
	int32 *value_list;

	/*
	 * Nothing in an empty list
	 */
	*value_cnt = 0;
	if ((str == NULL)||(*str == '\0'))
	{
		return((int32 *)NULL);
	}

	/*
	 * Skip beginning whitespace, all whitespace is empty list.
	 */
	n_str = str;
	while (XP_IS_SPACE(*n_str))
	{
		n_str++;
	}
	if (*n_str == '\0')
	{
		return((int32 *)NULL);
	}

	/*
	 * Make a pass where any two numbers separated by just whitespace
	 * are given a comma separator.  Count entries while passing.
	 */
	cnt = 0;
	while (*n_str != '\0')
	{
		Bool has_comma;

		/*
		 * Skip to a separator
		 */
		tptr = n_str;
		while ((!XP_IS_SPACE(*tptr))&&(*tptr != ',')&&(*tptr != '\0'))
		{
			tptr++;
		}
		n_str = tptr;

		/*
		 * If no more entries, break out here
		 */
		if (*n_str == '\0')
		{
			break;
		}

		/*
		 * Skip to the end of the separator, noting if we have a
		 * comma.
		 */
		has_comma = FALSE;
		while ((XP_IS_SPACE(*tptr))||(*tptr == ','))
		{
			if (*tptr == ',')
			{
				if (has_comma == FALSE)
				{
					has_comma = TRUE;
				}
				else
				{
					break;
				}
			}
			tptr++;
		}
		/*
		 * If this was trailing whitespace we skipped, we are done.
		 */
		if ((*tptr == '\0')&&(has_comma == FALSE))
		{
			break;
		}
		/*
		 * Else if the separator is all whitespace, and this is not the
		 * end of the string, add a comma to the separator.
		 */
		else if (has_comma == FALSE)
		{
			*n_str = ',';
		}

		/*
		 * count the entry skipped.
		 */
		cnt++;

		n_str = tptr;
	}
	/*
	 * count the last entry in the list.
	 */
	cnt++;
	
	/*
	 * For polygons, we need a fake empty coord at the
	 * end of the list of x,y pairs.
	 */
	if ((must_be_odd != FALSE)&&((cnt & 0x01) == 0))
	{
		acnt = cnt + 1;
	}
	else
	{
		acnt = cnt;
	}

	*value_cnt = acnt;

	/*
	 * Allocate space for the coordinate array.
	 */
	value_list = (int32 *)XP_ALLOC(acnt * sizeof(int32));
	if (value_list == NULL)
	{
		return((int32 *)NULL);
	}

	/*
	 * Second pass to copy integer values into list.
	 */
	tptr = str;
	for (i=0; i<cnt; i++)
	{
		char *ptr;

		ptr = strchr(tptr, ',');
		if (ptr != NULL)
		{
			*ptr = '\0';
		}
		/*
		 * Strip whitespace in front of number because I don't
		 * trust atoi to do it on all platforms.
		 */
		while (XP_IS_SPACE(*tptr))
		{
			tptr++;
		}
		if (*tptr == '\0')
		{
			value_list[i] = 0;
		}
		else
		{
			value_list[i] = (int32)XP_ATOI(tptr);
		}
		if (ptr != NULL)
		{
			*ptr = ',';
			tptr = (char *)(ptr + 1);
		}
	}
	return(value_list);
}


/*
 * The beginning of a usemap MAP record.
 * Allocate the structure and initialize it.  It will be filled
 * by later AREA tags.
 */
void
lo_BeginMap(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	PA_Block buff;
	char *str;
	lo_MapRec *map;

	map = XP_NEW(lo_MapRec);
	if (map == NULL)
	{
		state->top_state->out_of_memory = TRUE;
		return;
	}

	map->name = NULL;
	map->areas = NULL;
	map->areas_last = NULL;
	map->next = NULL;

	buff = lo_FetchParamValue(context, tag, PARAM_NAME);
	if (buff != NULL)
	{
		char *name;

		PA_LOCK(str, char *, buff);
		if (str != NULL)
		{
			int32 len;

			len = lo_StripTextWhitespace(str, XP_STRLEN(str));
		}
		name = (char *)XP_ALLOC(XP_STRLEN(str) + 1);
		if (name == NULL)
		{
			map->name = NULL;
		}
		else
		{
			XP_STRCPY(name, str);
			map->name = name;
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
	else
	{
		map->name = NULL;
	}

	if (map->name == NULL)
	{
		XP_DELETE(map);
		return;
	}

	state->top_state->current_map = map;
}


/*
 * Look through the list of finished maps to find one with
 * a matching name.  If found, remove it.
 */
static lo_MapRec *
lo_remove_named_map(MWContext *context, lo_DocState *state, char *map_name)
{
	lo_MapRec *prev_map;
	lo_MapRec *map_list;

	if (map_name == NULL)
	{
		return(NULL);
	}

	prev_map = NULL;
	map_list = state->top_state->map_list;
	while (map_list != NULL)
	{
		/*
		 * Found a map with a matching name, return it.
		 */
		if (XP_STRCMP(map_name, map_list->name) == 0)
		{
			break;
		}
		prev_map = map_list;
		map_list = map_list->next;
	}

	/*
	 * No map by that name found.
	 */
	if (map_list == NULL)
	{
		return(NULL);
	}

	/*
	 * If no prev_map, our match is the head of the map_list.
	 */
	if (prev_map == NULL)
	{
		state->top_state->map_list = map_list->next;
	}
	else
	{
		prev_map->next = map_list->next;
	}

	map_list->next = NULL;
	return(map_list);
}


/*
 * This map is done, add it to the map list.
 */
void
lo_EndMap(MWContext *context, lo_DocState *state, lo_MapRec *map)
{
	lo_MapRec *old_map;

	if (map == NULL)
	{
		return;
	}

	old_map = lo_remove_named_map(context, state, map->name);
	if (old_map != NULL)
	{
		(void)lo_FreeMap(old_map);
	}

	map->next = state->top_state->map_list;
	state->top_state->map_list = map;
}


lo_MapRec *
lo_FreeMap(lo_MapRec *map)
{
	lo_MapRec *next;

	if (map->areas != NULL)
	{
		lo_MapAreaRec *tmp_area;
		lo_MapAreaRec *areas;

		areas = map->areas;
		while (areas != NULL)
		{
			tmp_area = areas;
			areas = areas->next;
			if (tmp_area->alt != NULL)
			{
				PA_FREE(tmp_area->alt);
			}
			if (tmp_area->coords != NULL)
			{
				XP_FREE(tmp_area->coords);
			}
			XP_DELETE(tmp_area);
		}
	}
	if (map->name != NULL)
	{
		XP_FREE(map->name);
	}
	next = map->next;
	XP_DELETE(map);
	return next;
}


/*
 * This is an AREA tag.  Create the structure, fill it in based on the
 * attributes passed, and add it to the map record for the current
 * MAP tag.
 */
void
lo_BeginMapArea(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	PA_Block buff;
	char *str;
	lo_MapRec *map;
	lo_MapAreaRec *area;
        lo_DocLists *doc_lists;
        
        doc_lists = lo_GetCurrentDocLists(state);

	/*
	 * Get the current map, if there is none, error out.
	 */
	map = state->top_state->current_map;
	if (map == NULL)
	{
		return;
	}

	area = XP_NEW(lo_MapAreaRec);
	if (area == NULL)
	{
		state->top_state->out_of_memory = TRUE;
		return;
	}

	area->type = AREA_SHAPE_RECT;
	area->coords = NULL;
	area->coord_cnt = 0;
	area->anchor = NULL;
	area->alt = NULL;
	area->alt_len = 0;
	area->next = NULL;

	buff = lo_FetchParamValue(context, tag, PARAM_SHAPE);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		if (pa_TagEqual(S_AREA_SHAPE_RECT, str))
		{
			area->type = AREA_SHAPE_RECT;
		}
		else if (pa_TagEqual(S_AREA_SHAPE_CIRCLE, str))
		{
			area->type = AREA_SHAPE_CIRCLE;
		}
		else if (pa_TagEqual(S_AREA_SHAPE_POLY, str))
		{
			area->type = AREA_SHAPE_POLY;
		}
		else if (pa_TagEqual(S_AREA_SHAPE_POLYGON, str))
		{
			area->type = AREA_SHAPE_POLY;
		}
		else if (pa_TagEqual(S_AREA_SHAPE_DEFAULT, str))
		{
			area->type = AREA_SHAPE_DEFAULT;
		}
		else
		{
			area->type = AREA_SHAPE_UNKNOWN;
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}

	/*
	 * Get the alt parameter, and store the resulting
	 * text, and its length.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_ALT);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		area->alt_len = XP_STRLEN(str);
		area->alt_len = (int16)lo_StripTextNewlines(str,
						(int32)area->alt_len);
		PA_UNLOCK(buff);
	}
	area->alt = buff;

	/*
	 * Parse the comma separated coordinate list into an
	 * array of integers.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_COORDS);
	if (buff != NULL)
	{
		int32 cnt;
		Bool must_be_odd;

		must_be_odd = FALSE;
		if (area->type == AREA_SHAPE_POLY)
		{
			must_be_odd = TRUE;
		}
		PA_LOCK(str, char *, buff);
		area->coords = lo_parse_coord_list(str, &cnt, must_be_odd);
		if (area->coords != NULL)
		{
			area->coord_cnt = cnt;
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}

	/*
	 * Get the HREF, and if one exists, get the TARGET to go along
	 * with it.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_HREF);
	if (buff != NULL)
	{
		char *target;
		PA_Block targ_buff;
		PA_Block href_buff;
		LO_AnchorData *anchor;

		anchor = NULL;
		PA_LOCK(str, char *, buff);
		if (str != NULL)
		{
			int32 len;

			len = lo_StripTextWhitespace(str, XP_STRLEN(str));
		}
		str = NET_MakeAbsoluteURL(state->top_state->base_url, str);
		if (str == NULL)
		{
			href_buff = NULL;
		}
		else
		{
			href_buff = PA_ALLOC(XP_STRLEN(str) + 1);
			if (href_buff != NULL)
			{
				char *full_url;

				PA_LOCK(full_url, char *, href_buff);
				XP_STRCPY(full_url, str);
				PA_UNLOCK(href_buff);
			}
			else
			{
				state->top_state->out_of_memory = TRUE;
			}
			XP_FREE(str);
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);

		if (href_buff != NULL)
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

			anchor = lo_NewAnchor(state, href_buff, targ_buff);
			if (anchor == NULL)
			{
				PA_FREE(href_buff);
				if (targ_buff != NULL)
				{
					PA_FREE(targ_buff);
				}
			}
			/*
			 * If the AREA tag has an ALT attribute,
			 * stick that text into the anchor data.
			 */
			else if (area->alt != NULL)
			{
				PA_Block alt_buff;
				char *alt_text;

				PA_LOCK(alt_text, char *, area->alt);
				alt_buff = PA_ALLOC(area->alt_len + 1);
				if (alt_buff != NULL)
				{
					char *new_alt;

					PA_LOCK(new_alt, char *, alt_buff);
					XP_STRCPY(new_alt, alt_text);
					PA_UNLOCK(alt_buff);
				}
				PA_UNLOCK(area->alt);
				anchor->alt = alt_buff;
			}
		}

        /* If SUPPRESS attribute is present, suppress visual feedback (dashed rectangle)
           when link is selected */
        buff = lo_FetchParamValue(context, tag, PARAM_SUPPRESS);
        if (buff && !XP_STRCASECMP((char*)buff, "true"))
        {
            anchor->flags |= ANCHOR_SUPPRESS_FEEDBACK;
        }

		/*
		 * Add this url's block to the list
		 * of all allocated urls so we can free
		 * it later.
		 */
		lo_AddToUrlList(context, state, anchor);
		if (state->top_state->out_of_memory != FALSE)
		{
			return;
		}
		area->anchor = anchor;

		/*
		 * If there are event handlers, reflect this
		 * tag into Mocha as a link.
		 */
		lo_ReflectLink(context, state, tag, anchor,
                               lo_CurrentLayerId(state),
                               doc_lists->url_list_len - 1);
	}

	/*
	 * Add this are to the end of the area list in this
	 * map record.
	 */
	if (map->areas_last == NULL)
	{
		map->areas = area;
		map->areas_last = area;
	}
	else
	{
		map->areas_last->next = area;
		map->areas_last = area;
	}
}


/*
 * Look through the list of finished maps to find one with
 * a matching name.
 */
lo_MapRec *
lo_FindNamedMap(MWContext *context, lo_DocState *state, char *map_name)
{
	lo_MapRec *map_list;

	if (map_name == NULL)
	{
		return(NULL);
	}

	map_list = state->top_state->map_list;
	while (map_list != NULL)
	{
		/*
		 * Found a map with a matching name, return it.
		 */
		if (XP_STRCMP(map_name, map_list->name) == 0)
		{
			break;
		}
		map_list = map_list->next;
	}
	return(map_list);
}


/*
 * Check if a point is in a rectable specified by upper-left and
 * lower-right corners.
 */
static Bool
lo_is_location_in_rect(int32 x, int32 y, int32 *coords)
{
	int32 x1, y1, x2, y2;

	x1 = coords[0];
	y1 = coords[1];
	x2 = coords[2];
	y2 = coords[3];
	if ((x1 > x2)||(y1 > y2))
	{
		return(FALSE);
	}
	if ((x >= x1)&&(x <= x2)&&(y >= y1)&&(y <= y2))
	{
		return(TRUE);
	}
	else
	{
		return(FALSE);
	}
}


/*
 * Check if a point is within the radius of a circle specified
 * by center and radius.
 */
static Bool
lo_is_location_in_circle(int32 x, int32 y, int32 *coords)
{
	int32 x1, y1, radius;
	int32 dx, dy, dist;

	x1 = coords[0];
	y1 = coords[1];
	radius = coords[2];
	if (radius < 0)
	{
		return(FALSE);
	}

	dx = x1 - x;
	dy = y1 - y;

	dist = (dx * dx) + (dy * dy);

	if (dist <= (radius * radius))
	{
		return(TRUE);
	}
	else
	{
		return(FALSE);
	}
}


/*
 * Check if the passed point is withing the area described in
 * the area structure.
 */
static Bool
lo_is_location_in_area(lo_MapAreaRec *area, int32 x, int32 y)
{
	Bool ret_val;

	if (area == NULL)
	{
		return(FALSE);
	}

	ret_val = FALSE;
	switch(area->type)
	{
		case AREA_SHAPE_RECT:
			if (area->coord_cnt < 4)
			{
				ret_val = FALSE;
			}
			else
			{
				ret_val = lo_is_location_in_rect(x, y,
								area->coords);
			}
			break;

		case AREA_SHAPE_CIRCLE:
			if (area->coord_cnt < 3)
			{
				ret_val = FALSE;
			}
			else
			{
				ret_val = lo_is_location_in_circle(x, y,
								area->coords);
			}
			break;

		case AREA_SHAPE_POLY:
			if (area->coord_cnt < 6)
			{
				ret_val = FALSE;
			}
			else
			{
				ret_val = lo_is_location_in_poly(x, y,
						area->coords, area->coord_cnt);
			}
			break;

		case AREA_SHAPE_DEFAULT:
			ret_val = TRUE;
			break;

		case AREA_SHAPE_UNKNOWN:
		default:
			break;
	}
	return(ret_val);
}

/*
 * Given an x,y location and a map, return the anchor data for the
 * usemap.  Return NULL for areas not described or with no specified href.
 */
LO_AnchorData *
lo_MapXYToAnchorData(MWContext *context, lo_DocState *state,
			lo_MapRec *map, int32 x, int32 y)
{
	lo_MapAreaRec *areas;

	if (map == NULL)
	{
		return(NULL);
	}

	areas = map->areas;
	while (areas != NULL)
	{
		/*
		 * If we found a containing area, return its anchor data.
		 */
		if (lo_is_location_in_area(areas, x, y) != FALSE)
		{
			break;
		}
		areas = areas->next;
	}
	if (areas != NULL)
	{
		return(areas->anchor);
	}
	else
	{
		return(NULL);
	}
}


/*
 * Public function for front ends to get the usemap anchor data
 * for an image, based on the x,y of the user in the image.
 */
LO_AnchorData *
LO_MapXYToAreaAnchor(MWContext *context, LO_ImageStruct *image,
			int32 x, int32 y)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	LO_AnchorData *anchor;
	lo_MapRec *map;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return(NULL);
	}
	state = top_state->doc_state;

	/*
	 * If this is not really a USEMAP image, leave now.
	 */
	if (image->image_attr->usemap_name == NULL)
	{
		return(NULL);
	}

	/*
	 * If we havn't looked up and stored the map
	 * pointer for this image before, do that now,
	 * otherwise use the stored map pointer.
	 */
	if (image->image_attr->usemap_ptr == NULL)
	{
		map = lo_FindNamedMap(context, state,
				image->image_attr->usemap_name);
		image->image_attr->usemap_ptr = (void *)map;
	}
	else
	{
		map = (lo_MapRec *)image->image_attr->usemap_ptr;
	}

	anchor = lo_MapXYToAnchorData(context, state, map, x, y);
	return(anchor);
}

/* #ifndef NO_TAB_NAVIGATION */
/*
	see LO_MapXYToAreaAnchor() above for accessing the map structure.
	if currentAreaIndex <= 0 , return the first area.
*/
lo_MapAreaRec *
LO_getTabableMapArea(MWContext *context, LO_ImageStruct *image, int32 wantedIndex )
{
	lo_MapRec		*map;	
	lo_MapAreaRec	*areas;
	LO_AnchorData	*tempData;


	if( image == NULL || image->type != LO_IMAGE )
		return( NULL );

	/* We don't need the Anchor, but make sure we look up and store the map pointer.*/
	tempData = LO_MapXYToAreaAnchor( context, image, 0, 0);

	if (image->image_attr->usemap_name == NULL)	/* 	not really a USEMAP image. */
		return(NULL);

	map = (lo_MapRec *)image->image_attr->usemap_ptr;
	if (map == NULL)
		return(NULL);

	/* TODO if wantedIndex == -2, return the last tabable area */
	if( wantedIndex <= 0 )	
		wantedIndex = 1;
	
	areas = map->areas;
	while (areas != NULL )
	{
		if( areas->anchor != NULL ) { /* only tabable areas are counted */
			if( wantedIndex == 1 )
				return( areas );		/* got it */
			else
				wantedIndex--;				
		}
		areas = areas->next;
	}

	return( NULL );
}

Bool
LO_findApointInArea( lo_MapAreaRec *pArea, int32 *xx, int32 *yy )
{

	if ( pArea == NULL)
		return(FALSE);

	switch( pArea->type )
	{
		case AREA_SHAPE_RECT:
			if (pArea->coord_cnt >= 4)
			{	/* the center of the rect */
				*xx	= ( pArea->coords[0] + pArea->coords[2] ) / 2;		
				*yy	= ( pArea->coords[1] + pArea->coords[3] ) / 2;		
				return( TRUE );
			}
			break;

		case AREA_SHAPE_CIRCLE:
			if ( pArea->coord_cnt >= 3)
			{	/* the center of the rect */
				*xx = pArea->coords[0];
				*yy = pArea->coords[1];
				return( TRUE );
			}
			break;

		case AREA_SHAPE_POLY:
			if (pArea->coord_cnt >= 6)
			{
				return( lo_find_location_in_poly(xx, yy, pArea->coords, pArea->coord_cnt) );
			}
			break;

		case AREA_SHAPE_DEFAULT:
			/* TODO ?????? */
			break;

		case AREA_SHAPE_UNKNOWN:
		default:
			break;
	}

	return( FALSE );
}

/* NO_TAB_NAVIGATION */


