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
#include "net.h"
#include "pa_parse.h"
#include "layout.h"
#include "laylayer.h"
#include "java.h"
#include "laystyle.h"
#include "layers.h"

#define JAVA_DEF_DIM			50
#define JAVA_DEF_BORDER		0
#define JAVA_DEF_VERTICAL_SPACE	0
#define JAVA_DEF_HORIZONTAL_SPACE	0

void lo_FinishJavaApp(MWContext *, lo_DocState *, LO_JavaAppStruct *);
static void lo_FormatJavaAppInternal(MWContext *context,
									 lo_DocState *state,
									 PA_Tag *tag,
									 LO_JavaAppStruct *java_app);

void
LO_ClearJavaAppBlock(MWContext *context, LO_JavaAppStruct *java_app)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *main_doc_state;
	lo_DocState *state;

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

	if (top_state->layout_blocking_element == (LO_Element *)java_app)
	{
		if (java_app->width == 0)
		{
			java_app->width = JAVA_DEF_DIM;
		}
		if (java_app->height == 0)
		{
			java_app->height = JAVA_DEF_DIM;
		}

		main_doc_state = top_state->doc_state;
		state = lo_CurrentSubState(main_doc_state);

		lo_FinishJavaApp(context, state, java_app);
		lo_FlushBlockage(context, state, main_doc_state);
	}
}


void
lo_FormatJavaObject(MWContext *context, lo_DocState *state, PA_Tag *tag, LO_JavaAppStruct *java_app)
{
    PA_Block buff;

    buff = lo_FetchParamValue(context, tag, PARAM_CLASSID);
    if (buff != NULL)
    {
        char* str;
        PA_LOCK(str, char *, buff);
        if (XP_STRNCASECMP(str, "java:", 5) == 0)
            java_app->selector_type = LO_JAVA_SELECTOR_OBJECT_JAVA;
        else if (XP_STRNCASECMP(str, "javaprogram:", 12) == 0)
            java_app->selector_type = LO_JAVA_SELECTOR_OBJECT_JAVAPROGRAM;
        else if (XP_STRNCASECMP(str, "javabean:", 8) == 0)
            java_app->selector_type = LO_JAVA_SELECTOR_OBJECT_JAVABEAN;
        PA_UNLOCK(buff);
        XP_FREE(buff);
    }

    if (java_app->selector_type == LO_JAVA_SELECTOR_OBJECT_JAVAPROGRAM)
    {
        java_app->ele_attrmask |= LO_ELE_HIDDEN;
    }
    else 
    {
        /* Get the HIDDEN parameter */
        java_app->ele_attrmask = 0;
        buff = lo_FetchParamValue(context, tag, PARAM_HIDDEN);
        if (buff != NULL)
        {
            Bool hidden = TRUE;
            char* str;

            PA_LOCK(str, char *, buff);
            if (pa_TagEqual("no", str))
            {
                hidden = FALSE;
            }
            else if (pa_TagEqual("false", str))
            {
                hidden = FALSE;
            }
            else if (pa_TagEqual("off", str))
            {
                hidden = FALSE;
            }
            PA_UNLOCK(buff);
            PA_FREE(buff);

            if (hidden != FALSE)
            {
                java_app->ele_attrmask |= LO_ELE_HIDDEN;
            }
        }
    }

    /* Finish formatting the object */
    lo_FormatJavaAppInternal(context, state, tag, java_app);
}


void
lo_FormatJavaApp(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	LO_JavaAppStruct *java_app;

	/*
	 * Fill in the java applet structure with default data
	 */
	java_app = (LO_JavaAppStruct *)lo_NewElement(context, state, LO_JAVA, NULL, 0);
	if (java_app == NULL)
	{
		return;
	}

	java_app->type = LO_JAVA;
	java_app->ele_id = NEXT_ELEMENT;
	java_app->x = state->x;
	java_app->x_offset = 0;
	java_app->y = state->y;
	java_app->y_offset = 0;
	java_app->width = 0;
	java_app->height = 0;
	java_app->next = NULL;
	java_app->prev = NULL;

	java_app->ele_attrmask = 0;
	java_app->selector_type = LO_JAVA_SELECTOR_APPLET;

	/* Finish formatting the object */
	lo_FormatJavaAppInternal(context, state, tag, java_app);
}


static void
lo_FormatJavaAppInternal(MWContext *context, lo_DocState *state, PA_Tag *tag, LO_JavaAppStruct *java_app)
{
	PA_Block buff;
	char *str;
	int32 val;
	int32 doc_width;
	Bool widthSpecified = FALSE;
	Bool heightSpecified = FALSE;
	lo_DocLists *doc_lists;

    java_app->nextApplet = NULL;
#ifdef MOCHA
    java_app->mocha_object = NULL;
#endif

	java_app->FE_Data = NULL;
	java_app->session_data = NULL;
	java_app->line_height = state->line_height;

	java_app->base_url = NULL;
	java_app->attr_code = NULL;
	java_app->attr_codebase = NULL;
	java_app->attr_archive = NULL;
	java_app->attr_name = NULL;

	java_app->param_cnt = 0;
	java_app->param_names = NULL;
	java_app->param_values = NULL;

	java_app->may_script = FALSE;

	java_app->alignment = LO_ALIGN_BASELINE;
	java_app->border_width = JAVA_DEF_BORDER;
	java_app->border_vert_space = JAVA_DEF_VERTICAL_SPACE;
	java_app->border_horiz_space = JAVA_DEF_HORIZONTAL_SPACE;
	
	java_app->layer = NULL;
	java_app->tag = tag;

	/*
	 * Assign a unique index for this object 
	 * and increment the master index.
	 */
	java_app->embed_index = state->top_state->embed_count++;

	/*
	 * Save away the base of the document
	 */
	buff = PA_ALLOC(XP_STRLEN(state->top_state->base_url) + 1);
	if (buff != NULL)
	{
		char *cp;
		PA_LOCK(cp, char*, buff);
		XP_STRCPY(cp, state->top_state->base_url);
		PA_UNLOCK(buff);
		java_app->base_url = buff;
	}
	else
	{
		state->top_state->out_of_memory = TRUE;
		return;
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
		java_app->alignment = lo_EvalAlignParam(str, &floating);
		if (floating != FALSE)
		{
			java_app->ele_attrmask |= LO_ELE_FLOATING;
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}

	/*
	 * Get the applet CODE or object CLASSID parameter. In both
	 * cases the value is place in java_app->attr_code
	 */
	if (java_app->selector_type == LO_JAVA_SELECTOR_APPLET)
	{
		/* APPLET tag CODE attribute */

		buff = lo_FetchParamValue(context, tag, PARAM_CODE);
	}
	else
	{
		/* OBJECT tag CLASSID attribute */

		char * str1, * str2;
		PA_Block new_buff;

		int selectorLength;

		selectorLength = 5;
		if (java_app->selector_type == LO_JAVA_SELECTOR_OBJECT_JAVAPROGRAM)
			selectorLength = 12;
		else if (java_app->selector_type == LO_JAVA_SELECTOR_OBJECT_JAVABEAN)
			selectorLength = 9;

		buff = lo_FetchParamValue(context, tag, PARAM_CLASSID);

		if (buff != NULL)
		{
			/* remove the "java:", "javaprogram:", or "javabean:"
			 * protocol selector.
			 */
			PA_LOCK(str1, char *, buff);
			new_buff = PA_ALLOC(XP_STRLEN(str1) + 1);
			PA_LOCK(str2, char *, new_buff);
			XP_STRCPY(str2, str1 + selectorLength);
			PA_UNLOCK(new_buff);
			PA_UNLOCK(buff);
			PA_FREE(buff);
			buff = new_buff;
		}
	}
	java_app->attr_code = buff;

	/*
	 * Check for the loaderbase parameter.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_CODEBASE);
	java_app->attr_codebase = buff;

	/*
	 * Check for the archive parameter.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_ARCHIVE);
	java_app->attr_archive = buff;

	/*
	 * Check for a mayscript attribute
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_MAYSCRIPT);
	if (buff != NULL)
	{
		java_app->may_script = TRUE;
		PA_FREE(buff);
	}

	/*
	 * Get the name of this java applet.
	 */
	if (java_app->selector_type != LO_JAVA_SELECTOR_APPLET)
		buff = lo_FetchParamValue(context, tag, PARAM_ID);
	else
		buff = lo_FetchParamValue(context, tag, PARAM_NAME);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		if (str != NULL)
		{
			int32 len;

			len = lo_StripTextWhitespace(str, XP_STRLEN(str));
		}
		PA_UNLOCK(buff);
	}
	java_app->attr_name = buff;

	doc_width = state->right_margin - state->left_margin;

	/*
	 * Get the width parameter, in absolute or percentage.
	 * If percentage, make it absolute.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_WIDTH);
	if (buff != NULL)
	{
		Bool is_percent;

		PA_LOCK(str, char *, buff);
		val = lo_ValueOrPercent(str, &is_percent);
		if (is_percent != FALSE)
		{
			java_app->percent_width = val;
		}
		else
		{
			java_app->percent_width = 0;
			java_app->width = val;
			val = FEUNITS_X(val, context);
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
		widthSpecified = TRUE;
	}
	val = LO_GetWidthFromStyleSheet(context, state);
	if(val)
	{
		java_app->width = val;
		widthSpecified = TRUE;
	}

	/*
	 * Get the height parameter, in absolute or percentage.
	 * If percentage, make it absolute.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_HEIGHT);
	if (buff != NULL)
	{
		Bool is_percent;

		PA_LOCK(str, char *, buff);
		val = lo_ValueOrPercent(str, &is_percent);
		if (is_percent != FALSE)
		{
			java_app->percent_height = val;
		}
		else
		{
			java_app->percent_height = 0;
			val = FEUNITS_Y(val, context);
		}
		java_app->height = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
		heightSpecified = TRUE;
	}

	val = LO_GetHeightFromStyleSheet(context, state);
	if(val)
	{
		java_app->height = val;
		heightSpecified = TRUE;
	}

	/* If they forgot to specify a width, make one up. */
	if (!widthSpecified) {
		val = 0;
		if (heightSpecified) {
			val = java_app->height;
		}
		else if (state->allow_percent_width) {
			val = state->win_width * 90 / 100;
		}
		if (val < 1) {
			val = 600;
		}
		java_app->width = val;
	}
	
	/* If they forgot to specify a height, make one up. */
	if (!heightSpecified) {
		val = 0;
		if (widthSpecified) {
			val = java_app->width;
		}
		else if (state->allow_percent_height) {
			val = state->win_height / 2;
		}
		if (val < 1) {
			val = 400;
		}
		java_app->height = val;
	}

    /* After going through all the trouble, just to make sure
     * the object tag HIDDEN case is covered as well.
     */
    if (java_app->ele_attrmask & LO_ELE_HIDDEN)
    {
        java_app->width = 0;
        java_app->height = 0;
    }

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
		java_app->border_width = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
	java_app->border_width = FEUNITS_X(java_app->border_width, context);

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
		java_app->border_vert_space = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
	java_app->border_vert_space = FEUNITS_Y(java_app->border_vert_space,
							context);

	/*
	 * Get the extra horizontal space parameter.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_HSPACE);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		val = XP_ATOI(str);
		if (val < 0)
		{
			val = 0;
		}
		java_app->border_horiz_space = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
	java_app->border_horiz_space = FEUNITS_X(java_app->border_horiz_space,
						context);

	lo_FillInJavaAppGeometry(state, java_app, FALSE);

	/*
	 * See if we have some saved embed/java_app session data to restore.
	 */
	if (state->top_state->savedData.EmbedList != NULL)
	{
		lo_SavedEmbedListData *embed_list;
		embed_list = state->top_state->savedData.EmbedList;

		/*
		 * If there is still valid data to restore available.
		 */
		if (java_app->embed_index < embed_list->embed_count)
		{
			lo_EmbedDataElement* embed_data_list;

			PA_LOCK(embed_data_list, lo_EmbedDataElement*,
				embed_list->embed_data_list);
			java_app->session_data =
				embed_data_list[java_app->embed_index].data;
			PA_UNLOCK(embed_list->embed_data_list);
		}
	}

	/* put the applet onto the applet list for later
	 * possible reflection */
    doc_lists = lo_GetCurrentDocLists(state);
    if (state->in_relayout) {
        int32 i, count;
        LO_JavaAppStruct *prev_applet, *cur_applet;
        
        /*
         * In the interest of changing as little as possible, I'm not
         * going to change the applet_list to be in the order of layout
         * (it is currently in reverse order). Instead, we do what 
         * everybody else does - iterate till the end of the list to get
         * find an element with the correct index.
         * If we're in table relayout, we need to replace the element
         * of the same index in this list with the new layout element
         * to prevent multiple reflection.
         */
        count = 0;
        cur_applet = doc_lists->applet_list;
        while (cur_applet) {
            cur_applet = cur_applet->nextApplet;
            count++;
        }
        
        /* reverse order... */
        prev_applet = NULL;
        cur_applet = doc_lists->applet_list;
        for (i = count-1; i >= 0; i--) {
            if (i == doc_lists->applet_list_count) {
                /* Copy over the mocha object (it might not exist) */
                java_app->mocha_object = cur_applet->mocha_object;

                java_app->nextApplet = cur_applet->nextApplet;

                /* Replace the old applet with the new one */
                if (prev_applet == NULL)
                    doc_lists->applet_list = java_app;
                else
                    prev_applet->nextApplet = java_app;
                doc_lists->applet_list_count++;
                break;
            }
            prev_applet = cur_applet;
            cur_applet = cur_applet->nextApplet;
        }
    }
    else {
        java_app->nextApplet = doc_lists->applet_list;
        doc_lists->applet_list = java_app;
        doc_lists->applet_list_count++;
    }

	state->current_java = java_app;
	
	/* don't double-count loading applets due to table relayout */
	if (!state->in_relayout)
	{
		state->top_state->mocha_loading_applets_count++;
	}
}


void
lo_FinishJavaApp(MWContext *context, lo_DocState *state,
	LO_JavaAppStruct *java_app)
{
	int32 baseline_inc;
	int32 line_inc;
	int32 java_app_height, java_app_width;

	if ( java_app->layer == NULL )
		{
		java_app->layer =
		  lo_CreateEmbeddedObjectLayer(context, state, (LO_Element*)java_app);
		}

	if (java_app->selector_type != LO_JAVA_SELECTOR_OBJECT_JAVAPROGRAM)
	{
		/*
		 * Eventually this will never happen since we always
		 * have dims here from either the image tag itself or the
		 * front end.
		 */
		if (java_app->width == 0)
		{
			java_app->width = JAVA_DEF_DIM;
		}
		if (java_app->height == 0)
		{
			java_app->height = JAVA_DEF_DIM;
		}
	}

	java_app_width = java_app->width + (2 * java_app->border_width) +
		(2 * java_app->border_horiz_space);
	java_app_height = java_app->height + (2 * java_app->border_width) +
		(2 * java_app->border_vert_space);

	/*
	 * SEVERE FLOW BREAK!  This may be a floating java_app,
	 * which means at this point we go do something completely
	 * different.
	 */
	if (java_app->ele_attrmask & LO_ELE_FLOATING)
	{
		java_app->x_offset += (int16)java_app->border_horiz_space;
		java_app->y_offset += java_app->border_vert_space;

		/*
		 * Insert this element into the float list.
		 */
		java_app->next = state->float_list;
		state->float_list = (LO_Element *)java_app;

		lo_AppendFloatInLineList(state, (LO_Element*)java_app, NULL);

		lo_LayoutFloatJavaApp(context, state, java_app, TRUE);

		/*
		 * Make sure the applet get created on
		 * the java side before we ever try to display
		 * (which the following if might do).
		 */
		if (java_app->session_data == NULL && !EDT_IS_EDITOR( context ))
		{
			/*
			 * This creates an applet in the java airspace,
			 * sends it the init message and sets up the
			 * session_data of the java_app structure.
			 */
			LJ_CreateApplet(java_app, context, state->top_state->force_reload);
		}

		return;
	}

	/*
	 * Figure out how to align this java_app.
	 * baseline_inc is how much to increase the baseline
	 * of previous element of this line.  line_inc is how
	 * much to increase the line height below the baseline.
	 */
	baseline_inc = 0;
	line_inc = 0;
	/*
	 * If we are at the beginning of a line, with no baseline,
	 * we first set baseline and line_height based on the current
	 * font, then place the image.
	 */
	if (state->baseline == 0)
	{
		state->baseline = 0;
	}

	java_app->x_offset += (int16)java_app->border_horiz_space;
	java_app->y_offset += java_app->border_vert_space;

	lo_LayoutInflowJavaApp(context, state, java_app, FALSE, &line_inc, &baseline_inc);

	lo_AppendToLineList(context, state,
		(LO_Element *)java_app, baseline_inc);

	lo_UpdateStateAfterJavaAppLayout(state, java_app, line_inc, baseline_inc);

	if (java_app->session_data == NULL && !EDT_IS_EDITOR( context )) {
		/*
		** This creates an applet in the java airspace, sends it the init
		** message and sets up the session_data of the java_app structure:
		*/
		LJ_CreateApplet(java_app, context, state->top_state->force_reload);
	}
}


void
lo_CloseJavaApp(MWContext *context, lo_DocState *state,
	LO_JavaAppStruct *java_app)
{
    /*
    ** Create the applet first (if there wasn't a saved one in the
    ** history).  That way we'll be able to remember reload method.
    */
	if (java_app->session_data == NULL && !EDT_IS_EDITOR( context )) {
		/*
		** This creates an applet in the java airspace, sends it the init
		** message and sets up the session_data of the java_app structure:
		*/
		LJ_CreateApplet(java_app, context, state->top_state->force_reload);

                lo_AddEmbedData(context, java_app->session_data, 
                                LJ_DeleteSessionData, 
                                java_app->embed_index); 
	}

	/*
	 * Have the front end fetch this java applet, and tells us
	 * its dimentions if it knows them.
	 */
	FE_GetJavaAppSize(context, java_app, state->top_state->force_reload);

	/*
	 * We may have to block on this java applet until later
	 * when the front end can give us the width and height.
     *
     * Actaully this never can be the case in the current code
     * for applet tag, and since the object tag *can* be 0,0 this
     * is being commented out.
	 */
    /*
	if ((java_app->width == 0)||(java_app->height == 0))
	{
		state->top_state->layout_blocking_element =
			(LO_Element *)java_app;
	}
	else
	{
		lo_FinishJavaApp(context, state, java_app);
	}
    */
	lo_FinishJavaApp(context, state, java_app);

	state->current_java = NULL;
}


void
lo_RelayoutJavaApp(MWContext *context, lo_DocState *state, PA_Tag *tag,
	LO_JavaAppStruct *java_app )
{
	lo_DocLists *doc_lists;

	lo_PreLayoutTag ( context, state, tag );
	if (state->top_state->out_of_memory)
	{
		return;
	}

	java_app->x = state->x;
	java_app->x_offset = 0;
	java_app->y = state->y;
	java_app->y_offset = 0;

	/*
	 * Assign a unique index for this object 
	 * and increment the master index.
	 */
	java_app->embed_index = state->top_state->embed_count++;

    /*
     * Since we saved the applet_list_count during table relayout,
     * we increment it as if we were reprocessing the APPLET tag.
     */
    doc_lists = lo_GetCurrentDocLists(state);
    doc_lists->applet_list_count++;
	
	state->current_java = java_app;

	lo_PostLayoutTag ( context, state, tag, FALSE );
}

void
lo_FillInJavaAppGeometry(lo_DocState *state,
						 LO_JavaAppStruct *java_app,
						 Bool relayout)
{
	int32 doc_width;

	if (relayout == TRUE)
	{
		java_app->ele_id = NEXT_ELEMENT;
	}
	java_app->x = state->x;
	java_app->y = state->y;

	doc_width = state->right_margin - state->left_margin;

	/*
	 * Get the width parameter, in absolute or percentage.
	 * If percentage, make it absolute.
	 */

	if (java_app->percent_width > 0) {
		int32 val = java_app->percent_width;
		if (state->allow_percent_width == FALSE) {
			val = 0;
		}
		else {
			val = doc_width * val / 100;
		}
		java_app->width = val;
	}

	/* Set java_app->height if java_app has a % height specified */
	if (java_app->percent_height > 0) {
		int32 val = java_app->percent_height;
		if (state->allow_percent_height == FALSE) {
			val = 0;
		}
		else {
			val = state->win_height * val / 100;
		}
		java_app->height = val;
	}
}

void
lo_LayoutInflowJavaApp(MWContext *context,
					   lo_DocState *state,
					   LO_JavaAppStruct *java_app,
					   Bool inRelayout,
					   int32 *line_inc,
					   int32 *baseline_inc)
{
  int32 java_app_width, java_app_height;
  Bool line_break;
  PA_Block buff;
  char *str;
  LO_TextStruct tmp_text;
  LO_TextInfo text_info;

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

	java_app_width = java_app->width + (2 * java_app->border_width) +
		(2 * java_app->border_horiz_space);
	java_app_height = java_app->height + (2 * java_app->border_width) +
		(2 * java_app->border_vert_space);

	/*
	 * Will this java_app make the line too wide.
	 */
	if ((state->x + java_app_width) > state->right_margin)
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
	 * break on the java_app if we have
	 * a break.
	 */
	if (line_break != FALSE)
	{
		/*
		 * We need to make the elements sequential, linefeed
		 * before java app.
		 */
		state->top_state->element_id = java_app->ele_id;		

		if (!inRelayout)
		{
			lo_SoftLineBreak(context, state, TRUE);
		}
		else 
		{
			lo_rl_AddSoftBreakAndFlushLine(context, state);
		}
		
		java_app->x = state->x;
		java_app->y = state->y;
		java_app->ele_id = NEXT_ELEMENT;
	}


	lo_CalcAlignOffsets(state, &text_info, java_app->alignment,
						java_app_width, java_app_height,
						&java_app->x_offset, &java_app->y_offset,
						line_inc, baseline_inc);
}

void
lo_LayoutFloatJavaApp(MWContext *context,
					  lo_DocState *state,
					  LO_JavaAppStruct *java_app,
					  Bool updateFE)
{
  int32 java_app_width;

  java_app_width = (2 * java_app->border_width) +
	(2 * java_app->border_horiz_space);

  if (java_app->alignment == LO_ALIGN_RIGHT)
	{
	  java_app->x = state->right_margin - java_app_width;
	  if (java_app->x < 0)
		{
		  java_app->x = 0;
		}
	}
  else
	{
	  java_app->x = state->left_margin;
	}
  
  java_app->y = -1;
  lo_AddMarginStack(state, java_app->x, java_app->y,
					java_app->width, java_app->height,
					java_app->border_width,
					java_app->border_vert_space,
					java_app->border_horiz_space,
					java_app->alignment);

  if (state->at_begin_line != FALSE)
	{
	  lo_FindLineMargins(context, state, updateFE);
	  state->x = state->left_margin;
	}
}

void
lo_UpdateStateAfterJavaAppLayout(lo_DocState *state,
								 LO_JavaAppStruct *java_app,
								 int32 line_inc,
								 int32 baseline_inc)
{
  int32 java_app_width;
  int32 x, y;

  java_app_width = java_app->width + (2 * java_app->border_width) +
	(2 * java_app->border_horiz_space);

  state->baseline += (intn) baseline_inc;
  state->line_height += (intn) (baseline_inc + line_inc);
  
  /*
   * Clean up state
   */
  state->x = state->x + java_app->x_offset +
	java_app_width - java_app->border_horiz_space;
  state->linefeed_state = 0;
  state->at_begin_line = FALSE;
  state->trailing_space = FALSE;
  state->cur_ele_type = LO_JAVA;

  /* Determine the new position of the layer. */
  x = java_app->x + java_app->x_offset + java_app->border_width;
  y = java_app->y + java_app->y_offset + java_app->border_width;
  
  /* Move layer to new position */
  CL_MoveLayer(java_app->layer, x, y);
}
