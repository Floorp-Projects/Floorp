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
#ifdef EDITOR
#include "edt.h"
#endif
#include "java.h"
#include "layers.h"
#include "pa_parse.h"

#define IL_CLIENT               /* XXXM12N Defined by Image Library clients */
#include "libimg.h"             /* Image Library public API. */

extern void NPL_DeleteSessionData(MWContext*, void*);

#ifdef PROFILE
#pragma profile on
#endif


void
lo_FreeFormElementData(LO_FormElementData *element_data)
{
	int32 element_type;

	if (element_data == NULL)
	{
		return;
	}

	element_type = element_data->type;

	switch(element_type)
	{
		case FORM_TYPE_TEXT:
		case FORM_TYPE_PASSWORD:
		case FORM_TYPE_FILE:
			{
				lo_FormElementTextData *form_data;

				form_data = (lo_FormElementTextData *)
						element_data;
				if (form_data->name != NULL)
				{
					PA_FREE(form_data->name);
				}
				if (form_data->default_text != NULL)
				{
					PA_FREE(form_data->default_text);
				}
				if (form_data->current_text != NULL)
				{
					PA_FREE(form_data->current_text);
				}
			}
			break;
		case FORM_TYPE_SUBMIT:
		case FORM_TYPE_RESET:
		case FORM_TYPE_BUTTON:
		case FORM_TYPE_HIDDEN:
		case FORM_TYPE_READONLY:
			{
				lo_FormElementMinimalData *form_data;

				form_data = (lo_FormElementMinimalData *)
						element_data;
				if (form_data->name != NULL)
				{
					PA_FREE(form_data->name);
				}
				if (form_data->value != NULL)
				{
					PA_FREE(form_data->value);
				}
			}
			break;
		case FORM_TYPE_RADIO:
		case FORM_TYPE_CHECKBOX:
			{
				lo_FormElementToggleData *form_data;

				form_data = (lo_FormElementToggleData *)
						element_data;
				if (form_data->name != NULL)
				{
					PA_FREE(form_data->name);
				}
				if (form_data->value != NULL)
				{
					PA_FREE(form_data->value);
				}
			}
			break;
		case FORM_TYPE_TEXTAREA:
			{
				lo_FormElementTextareaData *form_data;

				form_data = (lo_FormElementTextareaData *)
						element_data;
				if (form_data->name != NULL)
				{
					PA_FREE(form_data->name);
				}
				if (form_data->default_text != NULL)
				{
					PA_FREE(form_data->default_text);
				}
				if (form_data->current_text != NULL)
				{
					PA_FREE(form_data->current_text);
				}
			}
			break;
		case FORM_TYPE_SELECT_ONE:
		case FORM_TYPE_SELECT_MULT:
			{
				int i;
				lo_FormElementSelectData *form_data;
				lo_FormElementOptionData *options;

				form_data = (lo_FormElementSelectData *)
						element_data;
				if (form_data->name != NULL)
				{
				    PA_FREE(form_data->name);
				}
				if (form_data->option_cnt > 0)
				{
				    PA_LOCK(options,
					lo_FormElementOptionData *,
					form_data->options);
				    for (i=0; i < form_data->option_cnt; i++)
				    {
					if (options[i].text_value != NULL)
					{
					   PA_FREE(options[i].text_value);
					}
					if (options[i].value != NULL)
					{
					   PA_FREE(options[i].value);
					}
				    }
				    PA_UNLOCK(form_data->options);
				    PA_FREE(form_data->options);
				}
			}
			break;
		case FORM_TYPE_KEYGEN:
			{
				lo_FormElementKeygenData *form_data;

				form_data = (lo_FormElementKeygenData *)
						element_data;
				if (form_data->name != NULL)
				{
					PA_FREE(form_data->name);
				}
				if (form_data->challenge != NULL)
				{
					PA_FREE(form_data->challenge);
				}
				if (form_data->value_str != NULL)
				{
					XP_FREE(form_data->value_str);
				}
			}
			break;
		case FORM_TYPE_OBJECT:
			{
				lo_FormElementObjectData *form_data;

				form_data = (lo_FormElementObjectData *)
						element_data;
				if (form_data->name != NULL)
				{
					PA_FREE(form_data->name);
				}
			}
			break;
		default:
			break;
	}
}


void
lo_CleanFormElementData(LO_FormElementData *element_data)
{
	int32 element_type;

	if (element_data == NULL)
	{
		return;
	}

	element_type = element_data->type;

	switch(element_type)
	{
		case FORM_TYPE_TEXT:
		case FORM_TYPE_PASSWORD:
		case FORM_TYPE_FILE:
			{
				lo_FormElementTextData *form_data;

				form_data = (lo_FormElementTextData *)
						element_data;
				if (form_data->name != NULL)
				{
					PA_FREE(form_data->name);
					form_data->name = NULL;
				}
				if (form_data->default_text != NULL)
				{
					PA_FREE(form_data->default_text);
					form_data->default_text = NULL;
				}
			}
			break;
		case FORM_TYPE_SUBMIT:
		case FORM_TYPE_RESET:
		case FORM_TYPE_BUTTON:
		case FORM_TYPE_HIDDEN:
		case FORM_TYPE_READONLY:
			{
				lo_FormElementMinimalData *form_data;

				form_data = (lo_FormElementMinimalData *)
						element_data;
				if (form_data->name != NULL)
				{
					PA_FREE(form_data->name);
					form_data->name = NULL;
				}
				if (form_data->value != NULL)
				{
					PA_FREE(form_data->value);
					form_data->value = NULL;
				}
			}
			break;
		case FORM_TYPE_RADIO:
		case FORM_TYPE_CHECKBOX:
			{
				lo_FormElementToggleData *form_data;

				form_data = (lo_FormElementToggleData *)
						element_data;
				if (form_data->name != NULL)
				{
					PA_FREE(form_data->name);
					form_data->name = NULL;
				}
				if (form_data->value != NULL)
				{
					PA_FREE(form_data->value);
					form_data->value = NULL;
				}
			}
			break;
		case FORM_TYPE_TEXTAREA:
			{
				lo_FormElementTextareaData *form_data;

				form_data = (lo_FormElementTextareaData *)
						element_data;
				if (form_data->name != NULL)
				{
					PA_FREE(form_data->name);
					form_data->name = NULL;
				}
				if (form_data->default_text != NULL)
				{
					PA_FREE(form_data->default_text);
					form_data->default_text = NULL;
				}
			}
			break;
		case FORM_TYPE_SELECT_ONE:
		case FORM_TYPE_SELECT_MULT:
			{
/* OPTION arrays are freed later
				int i;
				lo_FormElementOptionData *options;
*/
				lo_FormElementSelectData *form_data;

				form_data = (lo_FormElementSelectData *)
						element_data;
				if (form_data->name != NULL)
				{
				    PA_FREE(form_data->name);
				    form_data->name = NULL;
				}
/* OPTION arrays are freed later
				if (form_data->option_cnt > 0)
				{
				    PA_LOCK(options,
					lo_FormElementOptionData *,
					form_data->options);
				    for (i=0; i < form_data->option_cnt; i++)
				    {
					if (options[i].text_value != NULL)
					{
					   PA_FREE(options[i].text_value);
					}
					if (options[i].value != NULL)
					{
					   PA_FREE(options[i].value);
					}
				    }
				    PA_UNLOCK(form_data->options);
				    PA_FREE(form_data->options);
				}
*/
			}
			break;
		case FORM_TYPE_KEYGEN:
			{
				lo_FormElementKeygenData *form_data;

				form_data = (lo_FormElementKeygenData *)
						element_data;
				if (form_data->name != NULL)
				{
					PA_FREE(form_data->name);
					form_data->name = NULL;
				}
				if (form_data->challenge != NULL)
				{
					PA_FREE(form_data->challenge);
					form_data->challenge = NULL;
				}
				if (form_data->value_str != NULL)
				{
					XP_FREE(form_data->value_str);
					form_data->value_str = NULL;
				}
			}
			break;
		case FORM_TYPE_OBJECT:
			{
				lo_FormElementObjectData *form_data;

				form_data = (lo_FormElementObjectData *)
						element_data;
				if (form_data->name != NULL)
				{
					PA_FREE(form_data->name);
					form_data->name = NULL;
				}
			}
			break;
		default:
			break;
	}
}

PRIVATE
Bool
lo_remove_image(lo_DocLists *doc_lists, LO_Element *element)
{
    LO_ImageStruct *image, *last = NULL;
    
    for (image = doc_lists->image_list;
         image != (LO_ImageStruct *)element;
         image = image->next_image)
    {
        if (image == NULL)
            break;
        last = image;
    }
    if (image != NULL)
    {
        if (last != NULL)
        {
            last->next_image = image->next_image;
        }
        else
        {
            doc_lists->image_list = image->next_image;
        }
        if (image == doc_lists->last_image)
        {
            doc_lists->last_image = last;
        }
        
        return TRUE;
    }
    
    return FALSE;
}


void
lo_ScrapeElement(MWContext *context, LO_Element *element)
{
	if (element == NULL)
	{
		return;
	}
#ifdef EDITOR
    if ( EDT_IS_EDITOR(context) )
    {
        if ( element->lo_any.edit_element != NULL )
        {
            EDT_ResetLayoutElement(element->lo_any.edit_element, element->lo_any.edit_offset, element);
            element->lo_any.edit_element = NULL;
            element->lo_any.edit_offset = 0;
        }
    }
#endif
	switch(element->type)
	{
		case LO_TEXT:
			/* BRAIN DAMAGE: We currently override LO_ELE_INVISIBLE to flag that the text element uses the */
			/* new layout scheme and hence the text ptr is not a real ptr */
			if ( ( element->lo_text.text != NULL ) && !( element->lo_text.ele_attrmask & LO_ELE_INVISIBLE ))
			{
				PA_FREE(element->lo_text.text);
				element->lo_text.text = NULL;
			}
			break;

		case LO_LINEFEED:
		case LO_HRULE:
		case LO_BULLET:
			break;

		case LO_EDGE:
			FE_FreeEdgeElement(context, (LO_EdgeStruct *)element);
			break;

		case LO_FORM_ELE:
/*
 * these are now persistent, don't free element->lo_form.element_data
 */
			element->lo_form.element_data = NULL;
			break;

		case LO_EMBED:
			FE_FreeEmbedElement(context, (LO_EmbedStruct *)element);
			if (element->lo_embed.embed_src != NULL)
			{
				PA_FREE(element->lo_embed.embed_src);
			}
			element->lo_embed.embed_src = NULL;

			/* Free all the attribute names */
			if (element->lo_embed.attribute_list != NULL)
			{
				int32 	current;
				
				for (current=0; current<element->lo_embed.attribute_cnt; current++)
				{
					PA_FREE(element->lo_embed.attribute_list[current]);
					element->lo_embed.attribute_list[current] = NULL;
				}
						
				PA_FREE(element->lo_embed.attribute_list);
				element->lo_embed.attribute_list = NULL;
			}

			/* Free all the attribute values */
			if (element->lo_embed.value_list != NULL)
			{
				int32 	current;
				
				for (current=0; current<element->lo_embed.attribute_cnt; current++)
				{
					PA_FREE(element->lo_embed.value_list[current]);
					element->lo_embed.value_list[current] = NULL;
				}

				PA_FREE(element->lo_embed.value_list);
				element->lo_embed.value_list = NULL;
			}

			/*
			 * If there is session data here after we
			 * return from the front end freeing the embed 
			 * element, they want us to save this in history,
			 * and pass it back if/when we come back here.
			 * Otherwise, save the NULL.
			 */
			lo_AddEmbedData(context,
					element->lo_embed.session_data,
					NPL_DeleteSessionData,
					element->lo_embed.embed_index);
			element->lo_embed.session_data = NULL;
			break;
#ifdef SHACK
	    case LO_BUILTIN:
			FE_FreeBuiltinElement (context, (LO_BuiltinStruct *)element);
			/* maybe other stuff here */
			break;
#endif /* SHACK */

#ifdef JAVA
		case LO_JAVA:
			FE_HideJavaAppElement(context, element->lo_java.session_data);
			if (element->lo_java.attr_code != NULL)
			{
				PA_FREE(element->lo_java.attr_code);
				element->lo_java.attr_code = NULL;
			}
			if (element->lo_java.attr_codebase != NULL)
			{
				PA_FREE(element->lo_java.attr_codebase);
				element->lo_java.attr_codebase = NULL;
			}
			if (element->lo_java.attr_archive != NULL)
			{
				PA_FREE(element->lo_java.attr_archive);
				element->lo_java.attr_archive = NULL;
			}
			if (element->lo_java.attr_name != NULL)
			{
				PA_FREE(element->lo_java.attr_name);
				element->lo_java.attr_name = NULL;
			}
			if (element->lo_java.base_url != NULL)
			{
				PA_FREE(element->lo_java.base_url);
				element->lo_java.base_url = NULL;
			}
			if (element->lo_java.param_names != NULL)
			{
				int32 	current;
				
				for (current=0; current<element->lo_java.param_cnt; current++)
				{
					PA_FREE(element->lo_java.param_names[current]);
					element->lo_java.param_names[current] = NULL;
				}
						
				PA_FREE(element->lo_java.param_names);
				element->lo_java.param_names = NULL;
			}
			if (element->lo_java.param_values != NULL)
			{
				int32 	current;
				
				for (current=0; current<element->lo_java.param_cnt; current++)
				{
					PA_FREE(element->lo_java.param_values[current]);
					element->lo_java.param_values[current] = NULL;
				}

				PA_FREE(element->lo_java.param_values);
				element->lo_java.param_values = NULL;
			}

			/*
			 * If there is session data here after we
			 * return from the front end freeing the java applet 
			 * element, they want us to save this in history,
			 * and pass it back if/when we come back here.
			 * Otherwise, save the NULL.
			 */
			lo_AddEmbedData(context,
					element->lo_java.session_data,
					LJ_DeleteSessionData,
					element->lo_java.embed_index);
			element->lo_java.session_data = NULL;
			break;
#endif /* JAVA */
		case LO_IMAGE:
                        if (element->lo_image.image_req != NULL) 
                            IL_DestroyImage(element->lo_image.image_req);
                        element->lo_image.image_req = NULL;

                        if (element->lo_image.image_attr &&
                            !(element->lo_image.image_attr->attrmask & LO_ATTR_BACKDROP) &&
                            element->lo_image.layer) {
			    CL_Layer *parent = CL_GetLayerParent(element->lo_image.layer);
			    /* We might not have yet attached the image layer to its parent 
			       because we don't know which layer it should be place in. */
			    if (parent)
				CL_RemoveChild(parent, element->lo_image.layer);
                            CL_DestroyLayer(element->lo_image.layer);
                            element->lo_image.layer = NULL;
                        }
                    
			if (element->lo_image.alt != NULL)
			{
				PA_FREE(element->lo_image.alt);
			}
			element->lo_image.alt = NULL;
			if (element->lo_image.image_url != NULL)
			{
				PA_FREE(element->lo_image.image_url);
			}
			element->lo_image.image_url = NULL;
			if (element->lo_image.lowres_image_url != NULL)
			{
				PA_FREE(element->lo_image.lowres_image_url);
			}
			element->lo_image.lowres_image_url = NULL;
			lo_FreeImageAttributes(element->lo_image.image_attr);
			element->lo_image.image_attr = NULL;

			/* 
			 * make sure this element isn't sitting on the
			 *   image list
			 */
			{
			    int32 doc_id;
			    lo_TopState *top_state;
                            
			    doc_id = XP_DOCID(context);
			    top_state = lo_FetchTopState(doc_id);
			    if (top_state == NULL)
				break;

			    /* need to remove from the top_state->image_list */
                            lo_remove_image(&top_state->doc_lists,
                                            element);
			}
			break;
		case LO_CELL:
			{
			    int32 doc_id;
			    lo_TopState *top_state;

			    doc_id = XP_DOCID(context);
			    top_state = lo_FetchTopState(doc_id);
			    if ((top_state != NULL)&&
				(top_state->doc_state != NULL))
			    {
				lo_DocState *state;

				state = top_state->doc_state;
				if (element->lo_cell.cell_list != NULL)
				{
				    if (element->lo_cell.cell_list_end != NULL)
				    {
					element->lo_cell.cell_list_end->lo_any.next = NULL;
				    }
				    lo_RecycleElements(context, state,
					element->lo_cell.cell_list);
				    element->lo_cell.cell_list = NULL;
				    element->lo_cell.cell_list_end = NULL;
				}
				if (element->lo_cell.cell_float_list != NULL)
				{
				    lo_RecycleElements(context, state,
					element->lo_cell.cell_float_list);
				    element->lo_cell.cell_float_list = NULL;
				}
                                XP_FREEIF(element->lo_cell.backdrop.bg_color);
			    }

                            /* Free the background layer */
                            if ( element->lo_cell.cell_bg_layer != NULL)
                            {
                                CL_Layer *layer = element->lo_cell.cell_bg_layer;
                                CL_RemoveChild(CL_GetLayerParent(layer),
                                               layer);
                                CL_DestroyLayer(layer);
                                element->lo_cell.cell_bg_layer = NULL;
                            }
			}
			break;


		case LO_TABLE:
			{
				lo_TableRec *table = (lo_TableRec *) element->lo_table.table;
				lo_TopState *top_state;
				lo_DocState *state;
                            
			    if (table != NULL)
				{
					top_state = lo_FetchTopState(XP_DOCID(context));
					state = top_state->doc_state;
					lo_free_table_record(context, state, table, TRUE);
				}

			}
			break;


		case LO_MULTICOLUMN:
			/* should we do this? */
		  	if (element->lo_multicolumn.tag != NULL)
			  {
			    PA_FreeTag(element->lo_multicolumn.tag);
				if (element->lo_multicolumn.multicol == NULL)
				{
					XP_DELETE(element->lo_multicolumn.multicol);
				}
			    element->lo_multicolumn.tag = NULL;
			  }
			break;

		case LO_BLOCKQUOTE:
			/* should we do this? */
		  	if (element->lo_blockquote.tag != NULL)
			  {
			    PA_FreeTag(element->lo_blockquote.tag);
			    element->lo_blockquote.tag = NULL;
			  }
			break;
		case LO_SPACER:			
			if (element->lo_spacer.tag != NULL)
			  {
			    PA_FreeTag(element->lo_spacer.tag);
			    element->lo_spacer.tag = NULL;
			  }
			break;
		case LO_TEXTBLOCK:
			/* LO_TEXTBLOCK */
			if ( element->lo_textBlock.text_buffer != NULL )
			{
				XP_FREE ( element->lo_textBlock.text_buffer );
				element->lo_textBlock.text_buffer = NULL;
			}
			
			if ( element->lo_textBlock.break_table != NULL )
			{
				XP_FREE ( element->lo_textBlock.break_table );
				element->lo_textBlock.break_table = NULL;
			}
			break;
	}
}


void
lo_FreeElement(MWContext *context, LO_Element *element, Bool do_scrape)
{
	if (element == NULL)
	{
		return;
	}

	if (do_scrape != FALSE)
	{
		lo_ScrapeElement(context, element);
	}

#ifdef MEMORY_ARENAS
    if (EDT_IS_EDITOR(context)) {
        XP_DELETE(element);
    }
#else
	XP_DELETE(element);
#endif /* MEMORY_ARENAS */
}

void
lo_FreeImageAttributes(LO_ImageAttr *image_attr)
{
	if (image_attr)
	{
		if (image_attr->usemap_name)
		{
			XP_FREE(image_attr->usemap_name);
			image_attr->usemap_name = NULL;
		}
		
		/*	***** WARNING *****  Do NOT free "usemap_ptr" here since it is a pointer
			into a shared list in top_state that will get freed later. */
		
		XP_DELETE(image_attr);
	}
}

#ifdef XP_WIN16
#include <malloc.h>
#endif

int32
lo_FreeRecycleList(MWContext* context, LO_Element *recycle_list)
{
	LO_Element *element;
	LO_Element *eptr;

	int32 cnt;

	LO_LockLayout();

	cnt = 0;
#ifdef MEMORY_ARENAS
	if ( EDT_IS_EDITOR(context) ) {
#endif /* MEMORY_ARENAS */
	    eptr = recycle_list;
	    while (eptr != NULL)
	    {
		element = eptr;
		eptr = eptr->lo_any.next;
		element->lo_any.next = NULL;
		element->lo_any.prev = NULL;
		XP_DELETE(element);
		cnt++;
	    }
	    cnt = cnt * sizeof(LO_Element);
#ifdef MEMORY_ARENAS
	}
#endif /* MEMORY_ARENAS */

#ifdef XP_WIN16
	_heapmin();
#endif

	LO_UnlockLayout();

	return(cnt);
}


void
lo_FreeElementList(MWContext *context, LO_Element *elist)
{
	LO_Element *eptr;
	LO_Element *element;

	eptr = elist;
	while (eptr != NULL)
	{
		element = eptr;
		eptr = eptr->lo_any.next;
		lo_FreeElement(context, element, TRUE);
	}
}


void
lo_FreeDocumentFormListData(MWContext *context, lo_SavedFormListData *fptr)
{
	int i;
	LO_FormElementData **data_list;

	if (fptr == NULL)
	{
		return;
	}

	fptr->valid = FALSE;
	PA_LOCK(data_list, LO_FormElementData **, fptr->data_list);
	if (data_list != NULL)
	{
	    for (i=0; i < (fptr->data_count); i++)
	    {
		if (data_list[i] != NULL)
		{
			FE_FreeFormElement(context, (LO_FormElementData *)data_list[i]);
			lo_FreeFormElementData(data_list[i]);
			XP_DELETE(data_list[i]);
		}
	    }
	}
	PA_UNLOCK(fptr->data_list);

	if (fptr->data_list != NULL)
	{
		PA_FREE(fptr->data_list);
	}

	fptr->data_index = 0;
        fptr->data_count = 0;
        fptr->data_list = NULL;
        fptr->next = NULL;
}


void
lo_FreeDocumentEmbedListData(MWContext *context, lo_SavedEmbedListData *eptr)
{
	if (eptr == NULL)
	{
		return;
	}
	/* MWH -- If this routine is changed, place make sure that cmd\winfe\cxprint.cpp and 
	cmd\winfe\cxmeat.cpp is changed also.   */
#ifdef XP_WIN
	
	/* do not free the data, we need it for printing and metafile.*/
	if (context->type == MWContextPrint || context->type == MWContextMetaFile)
		return;
#endif
	if (eptr->embed_data_list != NULL)
	{
		int32 i;
		lo_EmbedDataElement* embed_data_list;

		PA_LOCK(embed_data_list, lo_EmbedDataElement*, eptr->embed_data_list);
		for (i=0; i < eptr->embed_count; i++)
		{
			if (embed_data_list[i].freeProc && embed_data_list[i].data)
				(*(embed_data_list[i].freeProc))(context, embed_data_list[i].data);
		}
		PA_UNLOCK(eptr->embed_data_list);
		PA_FREE(eptr->embed_data_list);
	}

	eptr->embed_count = 0;
	eptr->embed_data_list = NULL;
}


void
lo_FreeDocumentGridData(MWContext *context, lo_SavedGridData *sgptr)
{
	if (sgptr == NULL)
	{
		return;
	}

	if (sgptr->the_grid != NULL)
	{
		lo_GridRec *grid;
		lo_GridCellRec *cell_ptr;

		grid = sgptr->the_grid;
		cell_ptr = grid->cell_list;
		while (cell_ptr != NULL)
		{
			lo_GridCellRec *tmp_cell;

			tmp_cell = cell_ptr;
			cell_ptr = cell_ptr->next;
			lo_FreeGridCellRec(context, grid, tmp_cell);
		}
		lo_FreeGridRec(grid);
	}

	sgptr->main_width = 0;
	sgptr->main_height = 0;
	sgptr->the_grid = NULL;
}

void
LO_FreeSubmitData(LO_FormSubmitData *submit_data)
{
	int32 i;
	PA_Block *name_array, *value_array;

	if (submit_data == NULL)
	{
		return;
	}

	if (submit_data->action != NULL)
	{
		PA_FREE(submit_data->action);
	}

	PA_LOCK(name_array, PA_Block *, submit_data->name_array);
	PA_LOCK(value_array, PA_Block *, submit_data->value_array);
	for (i=0; i < (submit_data->value_cnt); i++)
	{
		if (name_array[i] != NULL)
		{
			PA_FREE(name_array[i]);
		}
		if (value_array[i] != NULL)
		{
			PA_FREE(value_array[i]);
		}
	}
	PA_UNLOCK(submit_data->value_array);
	PA_UNLOCK(submit_data->name_array);
	if (submit_data->name_array != NULL)
	{
		PA_FREE(submit_data->name_array);
	}
	if (submit_data->value_array != NULL)
	{
		PA_FREE(submit_data->value_array);
	}
	if (submit_data->type_array != NULL)
	{
		PA_FREE(submit_data->type_array);
	}

	XP_DELETE(submit_data);
}


int32
LO_EmptyRecyclingBin(MWContext *context)
{
	int32 doc_id;
	lo_TopState *top_state;
	int32 cnt;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return(0);
	}

	cnt = lo_FreeRecycleList(context, top_state->recycle_list);
	top_state->recycle_list = NULL;
#ifdef MEMORY_ARENAS
	if (top_state->current_arena != NULL)
	{
		int32 cnt2;

		cnt2 = lo_FreeMemoryArena(top_state->current_arena->next);
		top_state->current_arena->next = NULL;
		cnt += cnt2;
	}
#endif /* MEMORY_ARENAS */

	return(cnt);
}


#ifdef PROFILE
#pragma profile off
#endif
