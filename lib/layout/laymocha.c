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
/*
 * Mocha layout interface.
 */
#include "lo_ele.h"
#include "layout.h"
#include "laylayer.h"
#include "pa_parse.h"
#include "libevent.h"
#ifdef JAVA
#include "jsjava.h"
#endif
#include "layers.h"

PRIVATE
lo_TopState *
lo_GetTopState(MWContext *context)
{
    int32 doc_id;
    lo_TopState *top_state;

	if (!context)
		return NULL ;

    doc_id = XP_DOCID(context);
    top_state = lo_FetchTopState(doc_id);
    if (top_state != NULL && top_state->doc_state == NULL)
        return NULL;
    return top_state;
}

lo_TopState *
lo_GetMochaTopState(MWContext *context)
{
    lo_TopState *top_state;

    top_state = lo_GetTopState(context);
    if (top_state == NULL)
        return NULL;
    return top_state;
}

lo_FormData *
LO_GetFormDataByID(MWContext *context, int32 layer_id, intn form_id)
{
    lo_TopState *top_state;
    lo_FormData *form;
    lo_DocLists *doc_lists;

    top_state = lo_GetTopState(context);
    if (top_state == NULL)
        return NULL;

    doc_lists = lo_GetDocListsById(top_state->doc_state, layer_id);
    if (!doc_lists)
        return NULL;
    
    for (form = doc_lists->form_list; form != NULL; form = form->next) {
        if (form->id == form_id)
            return form;
    }
    return NULL;
}

/*
 * This can only safely be called while holding the JS_Lock
 */
uint
LO_EnumerateForms(MWContext *context, int32 layer_id)
{
    lo_TopState *top_state;
    lo_FormData *form;
    lo_DocLists *doc_lists;

    top_state = lo_GetMochaTopState(context);
    if (top_state == NULL)
        return 0;

    doc_lists = lo_GetDocListsById(top_state->doc_state, layer_id);
    if (!doc_lists)
        return 0;

    /* Try to reflect all forms in case someone is enumerating. */
    for (form = doc_lists->form_list; form != NULL; form = form->next) {
        if (form->mocha_object == NULL)
	    LM_ReflectForm(context, form, NULL, layer_id, 0);
    }
    return doc_lists->current_form_num;
}

/*
 * Note that index is zero-based from the first form, and if in the second
 * form, is one more than the index of the last element in the first form.
 * So we subtract form's first element index before subscripting.
 */
LO_FormElementStruct *
LO_GetFormElementByIndex(lo_FormData *form, int32 index)
{
    LO_Element **ele_list;
    LO_FormElementStruct *form_element;

    if (form->form_elements == NULL)
        return NULL;
    PA_LOCK(ele_list, LO_Element **, form->form_elements);
    form_element = (LO_FormElementStruct *)ele_list[0];
    index -= form_element->element_index;
    if ((uint32)index < (uint32)form->form_ele_cnt)
        form_element = (LO_FormElementStruct *)ele_list[index];
    else
        form_element = NULL;
    PA_UNLOCK(form->form_elements);
    if (form_element == NULL || form_element->element_data == NULL)
        return NULL;
    return form_element;
}


/*
 * This can only safely be called while holding the JS_Lock
 */
uint
LO_EnumerateFormElements(MWContext *context, lo_FormData *form)
{
    LO_Element **ele_list;
    LO_FormElementStruct *form_element;
    uint i;

    /* Try to reflect all elements in case someone is enumerating. */
    PA_LOCK(ele_list, LO_Element **, form->form_elements);
    for (i = 0; i < (uint)form->form_ele_cnt; i++) {
        form_element = (LO_FormElementStruct *)ele_list[i];
        if (form_element->mocha_object == NULL)
	    LM_ReflectFormElement(context, form_element->layer_id,
			  form->id, form_element->element_index, NULL);
    }
    PA_UNLOCK(form->form_elements);
    return form->form_ele_cnt;
}

void
lo_BeginReflectForm(MWContext *context, lo_DocState *doc_state, PA_Tag *tag,
                    lo_FormData *form)
{
    int32 layer_id;
    
    if(!PA_HasMocha(tag))
        return;

    layer_id = lo_CurrentLayerId(doc_state);
    ET_ReflectObject(context, (void *) form, tag, layer_id, 
                     form->id, LM_FORMS);
    ET_SetActiveForm(context, form);

}

LO_ImageStruct *
LO_GetImageByIndex(MWContext *context, int32 layer_id, intn index)
{
    lo_TopState *top_state;
    LO_ImageStruct *image;
    lo_DocLists *doc_lists;
    
    top_state = lo_GetTopState(context);
    if (top_state == NULL)
        return NULL;

    doc_lists = lo_GetDocListsById(top_state->doc_state, layer_id);
    if (!doc_lists)
        return NULL;
    
    for (image = doc_lists->image_list; image != NULL;
         image = image->next_image) {
        if (image && image->seq_num == index)
            return image;
    }
    return NULL;
}

uint
LO_EnumerateImages(MWContext *context, int32 layer_id)
{
    lo_TopState *top_state;
    uint image_num;
    LO_ImageStruct *img;
    lo_DocLists *doc_lists;

    top_state = lo_GetMochaTopState(context);
    if (top_state == NULL)
        return 0;

    doc_lists = lo_GetDocListsById(top_state->doc_state, layer_id);
    if (!doc_lists)
        return 0;

    /* 
     * Try to reflect all images in case someone is enumerating.
     * If its already on the image_list then it already has a valid
     *   seq_num
     */
    image_num = 0;
    for (img = doc_lists->image_list; img; img = img->next_image) {
	if (img->mocha_object == NULL)
	    LM_ReflectImage(context, img, NULL, layer_id, img->seq_num);
	image_num++;
    }
    return image_num;
}

void
lo_EndReflectForm(MWContext *context, lo_FormData *form_data)
{
    ET_SetActiveForm(context, NULL);
}

void
lo_ReflectImage(MWContext *context, lo_DocState *doc_state, PA_Tag *tag,
                LO_ImageStruct *image, Bool blocked, int32 layer_id)
{
    lo_DocLists *doc_lists;

    /* Reflect only IMG tags, not form INPUT tags of TYPE=IMAGE */
    if (tag->type != P_IMAGE && tag->type != P_NEW_IMAGE)
        return;

    /* Has this image already been reflected ? */
    if ((image->image_attr->attrmask & LO_ATTR_ON_IMAGE_LIST)) {
        if (! blocked) {
	    /* Flush any image events that were sent when the image
               was blocked */
            ET_SendImageEvent(context, image, LM_IMGUNBLOCK);
        }
        return;
    } else {
	/* Add image to global list of images for this document. */
        doc_lists = lo_GetDocListsById(doc_state, layer_id);
        if (!doc_lists)
            return;

        image->image_attr->attrmask |= LO_ATTR_ON_IMAGE_LIST;
        image->seq_num = doc_lists->image_list_count++;
        /*
         * If we're in table relayout, we replace the old image
         * in the image list. Note that we also look to see if 
         * the corresponding mocha object has already been allocated.
         * If so, we copy it. If not, it will be set when the 
         * the JS thread gets around to it (the mocha reflection
         * code gets the image by index, so it will get the right
         * one.
         */
        if (doc_state->in_relayout) {
            LO_ImageStruct *cur_image, *prev_image;
            prev_image = NULL;
            
            for (cur_image = doc_lists->image_list; cur_image != NULL;
                 prev_image=cur_image, cur_image=cur_image->next_image) {
                if (cur_image->seq_num == image->seq_num) {
                    /* Copy over the mocha object (it might not exist) */
                    image->mocha_object = cur_image->mocha_object;
                    
                    /* Replace the old image with the new one */
                    image->next_image = cur_image->next_image;
                    if (prev_image == NULL)
                        doc_lists->image_list = image;
                    else
                        prev_image->next_image = image;
                    if (doc_lists->last_image == cur_image)
                        doc_lists->last_image = image;
                    /* Note that the old image is recycled by the table code */
                    break;
                }
            }
            return;
        }
        /* Add it to the end of the list */
        else {
            if (doc_lists->last_image)
                doc_lists->last_image->next_image = image;
            else
                doc_lists->image_list = image;
            doc_lists->last_image = image;
        }
    }
 
    /* The image is reflected into a Javascript object lazily:
       Immediate reflection only occurs if there are Javascript-only
       attributes in the IMG tag */
    if(!PA_HasMocha(tag))
        return;

    ET_ReflectObject(context, (LO_Element *) image, tag, layer_id,
                     image->seq_num, LM_IMAGES);

    /* Since the image is unblocked, there is no need to buffer
       any JavaScript image events.  Deliver them as they're generated. */
    if (! blocked)
        ET_SendImageEvent(context, image, LM_IMGUNBLOCK);

}

void
lo_ReflectFormElement(MWContext *context, lo_DocState *doc_state, PA_Tag *tag,
                      LO_FormElementStruct *form_element)
{
    lo_DocLists *doc_lists;

    /* Don't reflect form elements twice, it makes arrays out of them! */
    if (doc_state->in_relayout)
        return;

    /* don't do squat if we don't have any mocha */
    if (!PA_HasMocha(tag))
        return;

    doc_lists = lo_GetDocListsById(doc_state, form_element->layer_id);
    if (!doc_lists)
        return;
    
    /* reflect the form element */
    ET_ReflectFormElement(context, doc_lists->form_list, form_element, tag);

}

void
lo_ReflectNamedAnchor(MWContext *context, lo_DocState *doc_state, PA_Tag *tag,
                      lo_NameList *name_rec, int32 layer_id)
{
    lo_DocLists *doc_lists;

    if (!doc_state->in_relayout) {
        doc_lists = lo_GetDocListsById(doc_state, layer_id);
        if (!doc_lists)
            return;
        name_rec->index = doc_lists->anchor_count++;

        if(!PA_HasMocha(tag))
            return;

        ET_ReflectObject(context, (void *) name_rec, tag, 
                         layer_id, name_rec->index, LM_NAMEDANCHORS);

    }
}


lo_NameList *
LO_GetNamedAnchorByIndex(MWContext *context, int32 layer_id, uint index)
{
    lo_TopState *top_state;
    lo_NameList *name_rec, *nptr;
    lo_DocLists *doc_lists;
    
    top_state = lo_GetTopState(context);
    if (top_state == NULL)
        return NULL;

    doc_lists = lo_GetDocListsById(top_state->doc_state, layer_id);
    if (!doc_lists)
        return NULL;

    /* The list is not guaranteed to be in reverse-source order when nested
       tables are involved, so search for matching index instead. */
    name_rec = NULL;
    for (nptr = doc_lists->name_list; nptr != NULL; nptr = nptr->next) {
        if (nptr->index == index) {
            name_rec = nptr;
            break;
        }
    }
    return name_rec;
}

uint
LO_EnumerateNamedAnchors(MWContext *context, int32 layer_id)
{
    lo_TopState *top_state;
    lo_NameList *name_rec;
    uint count;
    lo_DocLists *doc_lists;

    top_state = lo_GetTopState(context);
    if (top_state == NULL)
        return 0;

    doc_lists = lo_GetDocListsById(top_state->doc_state, layer_id);
    if (!doc_lists)
        return 0;

    count = 0;
    for (name_rec = doc_lists->name_list; name_rec != NULL;
         name_rec = name_rec->next) {
        if (name_rec->mocha_object == NULL)
	    LM_ReflectNamedAnchor(context, (void *) name_rec, NULL, 
				  layer_id, name_rec->index);
        count++;
    }
    return count;
}

#ifdef DOM
void
lo_ReflectSpan(MWContext *context, lo_DocState *doc_state, PA_Tag *tag,
                      lo_NameList *name_rec, int32 layer_id)
{
    lo_DocLists *doc_lists;

    if (!doc_state->in_relayout) {
        doc_lists = lo_GetDocListsById(doc_state, layer_id);
        if (!doc_lists)
            return;
        name_rec->index = doc_lists->span_count++;

        if(!PA_HasMocha(tag))
            return;

        ET_ReflectObject(context, (void *) name_rec, tag, 
                         layer_id, name_rec->index, LM_SPANS);

    }
}

lo_NameList *
LO_GetSpanByIndex(MWContext *context, int32 layer_id, uint index)
{
    lo_TopState *top_state;
    lo_NameList *name_rec, *nptr;
    lo_DocLists *doc_lists;
    
    top_state = lo_GetTopState(context);
    if (top_state == NULL)
        return NULL;

    doc_lists = lo_GetDocListsById(top_state->doc_state, layer_id);
    if (!doc_lists)
        return NULL;

    /* The list is not guaranteed to be in reverse-source order when nested
       tables are involved, so search for matching index instead. */
    name_rec = NULL;
    for (nptr = doc_lists->span_list; nptr != NULL; nptr = nptr->next) {
        if (nptr->index == index) {
            name_rec = nptr;
            break;
        }
    }
    return name_rec;
}

uint
LO_EnumerateSpans(MWContext *context, int32 layer_id)
{
    lo_TopState *top_state;
    lo_NameList *name_rec;
    uint count;
    lo_DocLists *doc_lists;

    top_state = lo_GetTopState(context);
    if (top_state == NULL)
        return 0;

    doc_lists = lo_GetDocListsById(top_state->doc_state, layer_id);
    if (!doc_lists)
        return 0;

    count = 0;
    for (name_rec = doc_lists->span_list; name_rec != NULL;
         name_rec = name_rec->next) {
        if (name_rec->mocha_object == NULL)
	    LM_ReflectSpan(context, (void *) name_rec, NULL, 
				  layer_id, name_rec->index);
        count++;
    }
    return count;
}
#endif

void
lo_ReflectLink(MWContext *context, lo_DocState *doc_state, PA_Tag *tag,
               LO_AnchorData *anchor_data, int32 layer_id, uint index)
{
    /* if this tag has any mocha, reflect it now */
    if(!PA_HasMocha(tag))
        return;

    ET_ReflectObject(context, anchor_data, tag, layer_id, index, LM_LINKS);
}

LO_AnchorData *
LO_GetLinkByIndex(MWContext *context, int32 layer_id, uint index)
{
    lo_TopState *top_state;
    LO_AnchorData **anchor_array;
    LO_AnchorData *anchor_data;
    lo_DocLists *doc_lists;

    top_state = lo_GetTopState(context);
    if (top_state == NULL)
        return NULL;

    doc_lists = lo_GetDocListsById(top_state->doc_state, layer_id);
    if (!doc_lists)
        return NULL;
    
    if (index >= (uint)doc_lists->url_list_len)
        return NULL;

    XP_LOCK_BLOCK(anchor_array, LO_AnchorData **, doc_lists->url_list);
    anchor_data = anchor_array[index];
    XP_UNLOCK_BLOCK(doc_lists->url_list);
    return anchor_data;
}

uint
LO_EnumerateLinks(MWContext *context, int32 layer_id)
{
    lo_TopState *top_state;
    uint count, index;
    LO_AnchorData **anchor_array;
    LO_AnchorData *anchor_data;
    lo_DocLists *doc_lists;

    top_state = lo_GetMochaTopState(context);
    if (top_state == NULL)
        return 0;

    doc_lists = lo_GetDocListsById(top_state->doc_state, layer_id);
    if (!doc_lists)
        return 0;
    
    /* Try to reflect all links in case someone is enumerating. */
    count = (uint)doc_lists->url_list_len;
    XP_LOCK_BLOCK(anchor_array, LO_AnchorData **, doc_lists->url_list);
    for (index = 0; index < count; index++) {
        anchor_data = anchor_array[index];
        if (anchor_data->mocha_object == NULL)
	    LM_ReflectLink(context, anchor_data, NULL, layer_id, index);
    }
    XP_UNLOCK_BLOCK(doc_lists->url_list);
    return count;
}

#ifdef JAVA
LO_JavaAppStruct *
LO_GetAppletByIndex(MWContext *context, int32 layer_id, uint index)
{
    lo_TopState *top_state;
    LO_JavaAppStruct *applet;
    int i, count;
    lo_DocLists *doc_lists;

    /* XXX */
    if (!JSJ_IsEnabled())
        return NULL;

    top_state = lo_GetTopState(context);
    if (top_state == NULL)
        return NULL;

    doc_lists = lo_GetDocListsById(top_state->doc_state, layer_id);
    if (!doc_lists)
        return NULL;

    /* count 'em */
    count = 0;
    applet = doc_lists->applet_list;
    while (applet) {
        applet = applet->nextApplet;
        count++;
    }

    /* reverse order... */
    applet = doc_lists->applet_list;
    for (i = count-1; i >= 0; i--) {
        if ((uint)i == index)
            return applet;
        applet = applet->nextApplet;
    }
    return NULL;
}

uint
LO_EnumerateApplets(MWContext *context, int32 layer_id)
{
    lo_TopState *top_state;
    int count, index;
    LO_JavaAppStruct *applet;
    lo_DocLists *doc_lists;

    /* XXX */
    if (!JSJ_IsEnabled())
        return 0;

    top_state = lo_GetMochaTopState(context);
    if (top_state == NULL)
        return 0;

    doc_lists = lo_GetDocListsById(top_state->doc_state, layer_id);
    if (!doc_lists)
        return 0;

    /* count 'em */
    count = 0;
    applet = doc_lists->applet_list;
    while (applet) {
        applet = applet->nextApplet;
        count++;
    }

    /* reflect all applets in reverse order */
    applet = doc_lists->applet_list;
    for (index = count-1; index >= 0; index--) {
        if (applet->mocha_object == NULL)
	    LM_ReflectApplet(context, (void *) applet, NULL, layer_id, index);
        applet = applet->nextApplet;
    }

    return count;
}

LO_EmbedStruct *
LO_GetEmbedByIndex(MWContext *context, int32 layer_id, uint index)
{
    lo_TopState *top_state;
    LO_EmbedStruct *embed;
    int i, count;
    lo_DocLists *doc_lists;

    /* XXX */
    if (!JSJ_IsEnabled())
        return NULL;

    top_state = lo_GetTopState(context);
    if (top_state == NULL)
        return NULL;

    doc_lists = lo_GetDocListsById(top_state->doc_state, layer_id);
    if (!doc_lists)
        return NULL;

    /* count 'em */
    count = 0;
    embed = doc_lists->embed_list;
    while (embed) {
        embed = embed->nextEmbed;
        count++;
    }

    /* reverse order... */
    embed = doc_lists->embed_list;
    for (i = count-1; i >= 0; i--) {
        if ((uint)i == index)
            return embed;
        embed = embed->nextEmbed;
    }
    return NULL;
}

uint
LO_EnumerateEmbeds(MWContext *context, int32 layer_id)
{
    lo_TopState *top_state;
    int count, index;
    LO_EmbedStruct *embed;
    lo_DocLists *doc_lists;

    /* XXX */
    if (!JSJ_IsEnabled())
        return 0;

    top_state = lo_GetMochaTopState(context);
    if (top_state == NULL)
        return 0;

    doc_lists = lo_GetDocListsById(top_state->doc_state, layer_id);
    if (!doc_lists)
        return 0;

    /* count 'em */
    count = 0;
    embed = doc_lists->embed_list;
    while (embed) {
        embed = embed->nextEmbed;
        count++;
    }

    /* reflect all embeds in reverse order */
    embed = doc_lists->embed_list;
    for (index = count-1; index >= 0; index--) {
	if (embed->mocha_object == NULL)
        	LM_ReflectEmbed(context, (void *) embed, NULL, layer_id, index);
        embed = embed->nextEmbed;
    }

    return count;
}
#endif /* JAVA */


/* XXX lo_DocState should use a colors[LO_NCOLORS] array to shrink code here
       and in layout.c.
 */
void
LO_GetDocumentColor(MWContext *context, int type, LO_Color *color)
{
    lo_TopState *top_state;
    lo_DocState *doc_state;

    top_state = lo_GetTopState(context);
    if (top_state == NULL)
        return;
    doc_state = top_state->doc_state;

    switch (type) {
      case LO_COLOR_BG:
        *color = doc_state->text_bg;
        break;
      case LO_COLOR_FG:
        *color = doc_state->text_fg;
        break;
      case LO_COLOR_LINK:
        *color = doc_state->anchor_color;
        break;
      case LO_COLOR_VLINK:
        *color = doc_state->visited_anchor_color;
        break;
      case LO_COLOR_ALINK:
        *color = doc_state->active_anchor_color;
        break;
      default:
        break;
    }
}

void
LO_SetDocumentColor(MWContext *context, int type, LO_Color *color)
{
    lo_TopState *top_state;
    lo_DocState *doc_state;

    top_state = lo_GetTopState(context);
    if (top_state == NULL)
        return;
    doc_state = top_state->doc_state;
    if (color == NULL)
        color = &lo_master_colors[type];

    switch (type) {
      case LO_COLOR_BG:
        doc_state->text_bg = *color;
        top_state->doc_specified_bg = TRUE;
        LO_SetDocBgColor(context, color);
        break;
      case LO_COLOR_FG:
        lo_ChangeBodyTextFGColor(context, doc_state, color);
        break;
      case LO_COLOR_LINK:
        doc_state->anchor_color = *color;
        break;
      case LO_COLOR_VLINK:
        doc_state->visited_anchor_color = *color;
        break;
      case LO_COLOR_ALINK:
        doc_state->active_anchor_color = *color;
        break;
      default:;
    }
}

XP_Bool
lo_ProcessContextEventHandlers(MWContext *context, lo_DocState *doc_state,
                               PA_Tag *tag)
{
    PA_Block onload, onunload, onfocus, onblur, onhelp, onmouseover, onmouseout,
	ondragdrop, onmove, onresize, id;
    lo_TopState *top_state;
    char *all = 0;
    XP_Bool ret;

    ret = FALSE;
    onload = lo_FetchParamValue(context, tag, PARAM_ONLOAD);
    onunload = lo_FetchParamValue(context, tag, PARAM_ONUNLOAD);
    onfocus = lo_FetchParamValue(context, tag, PARAM_ONFOCUS);
    onblur = lo_FetchParamValue(context, tag, PARAM_ONBLUR);
    onhelp = lo_FetchParamValue(context, tag, PARAM_ONHELP);
    onmouseover = lo_FetchParamValue(context, tag, PARAM_ONMOUSEOVER);
    onmouseout = lo_FetchParamValue(context, tag, PARAM_ONMOUSEOUT);
    ondragdrop = lo_FetchParamValue(context, tag, PARAM_ONDRAGDROP);
    onmove = lo_FetchParamValue(context, tag, PARAM_ONMOVE);
    onresize = lo_FetchParamValue(context, tag, PARAM_ONRESIZE);

    if (onload == NULL && onunload == NULL && onfocus == NULL && onblur == NULL
    	&& onhelp == NULL && onmouseover == NULL && onmouseout == NULL && ondragdrop == NULL
	&& onmove == NULL && onresize == NULL)
    {
        return ret;
    }

    ret = TRUE;

    id = lo_FetchParamValue(context, tag, PARAM_ID);
    all = NULL;
    StrAllocCopy(all, (char *) tag->data);

    top_state = doc_state->top_state;

    /* 
     * If we're in a layer, pass along the tag to the layer reflection 
     * function so that the event handlers get attached to the layer.
     */
    if (lo_InsideLayer(doc_state)) {
        int32 cur_layer_id = lo_CurrentLayerId(doc_state);
        CL_Layer *layer = LO_GetLayerFromId(context, cur_layer_id);
        CL_Layer *parent = CL_GetLayerParent(layer);
        int32 parent_layer_id = LO_GetIdFromLayer(context, parent);
        
        ET_ReflectObject(context, NULL, tag, parent_layer_id, cur_layer_id, 
                         LM_LAYERS);
    }
    else {
        ET_ReflectWindow(context, onload, onunload, onfocus, onblur, onhelp, onmouseover,
			 onmouseout, ondragdrop, onmove, onresize, 
                         id, all, tag->type != P_GRID, tag->newline_count);

        if (onload != NULL && tag->type == P_BODY)
            top_state->mocha_has_onload = TRUE;
        
        if (onunload != NULL && tag->type == P_BODY)
            top_state->mocha_has_onunload = TRUE;
    
        if (tag->type == P_GRID) {
            top_state->savedData.OnUnload = onunload;
            top_state->savedData.OnFocus = onfocus;
            top_state->savedData.OnBlur = onblur;
            top_state->savedData.OnLoad = onload;
            top_state->savedData.OnHelp = onhelp;
            top_state->savedData.OnMouseOver = onmouseover;
            top_state->savedData.OnMouseOut = onmouseout;
            top_state->savedData.OnDragDrop = ondragdrop;
            top_state->savedData.OnMove = onmove;
            top_state->savedData.OnResize = onresize;
        }
    }
    return ret;
}

void
lo_RestoreContextEventHandlers(MWContext *context, lo_DocState *doc_state,
                               PA_Tag *tag, SHIST_SavedData *saved_data)
{
    PA_Block onload, onunload, onfocus, onblur, onhelp, onmouseover, onmouseout, ondragdrop,
	onmove, onresize;
    lo_TopState *top_state;

    onload = saved_data->OnLoad;
    onunload = saved_data->OnUnload;
    onfocus = saved_data->OnFocus;
    onblur = saved_data->OnBlur;
    onhelp = saved_data->OnHelp;
    onmouseover = saved_data->OnMouseOver;
    onmouseout = saved_data->OnMouseOut;
    ondragdrop = saved_data->OnDragDrop;
    onmove = saved_data->OnMove;
    onresize = saved_data->OnResize;
    if (onload == NULL && onunload == NULL && onfocus == NULL && onblur == NULL
    	&& onhelp == NULL && onmouseover == NULL && onmouseout == NULL && ondragdrop == NULL
	&& onmove == NULL && onresize == NULL)
        return;

    top_state = doc_state->top_state;

    /* XXX - could these be signed? */
    ET_ReflectWindow(context, onload, onunload, onfocus, onblur, onhelp, onmouseover,
		     onmouseout, ondragdrop, onmove, onresize, 
                     NULL, NULL, FALSE, tag->newline_count);

    if (onload != NULL && tag->type == P_BODY)
        top_state->mocha_has_onload = TRUE;

    if (onunload != NULL && tag->type == P_BODY)
        top_state->mocha_has_onunload = TRUE;

    top_state->savedData.OnUnload = onunload;
    top_state->savedData.OnFocus = onfocus;
    top_state->savedData.OnBlur = onblur;
    top_state->savedData.OnLoad = onload;
    top_state->savedData.OnHelp = onhelp;
    top_state->savedData.OnMouseOver = onmouseover;
    top_state->savedData.OnMouseOut = onmouseout;
    top_state->savedData.OnDragDrop = ondragdrop;
    top_state->savedData.OnMove = onmove;
    top_state->savedData.OnResize = onresize;

}

