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

#ifndef _LayoutFunctions_
#define _LayoutFunctions_
/*
 * The idea is for the FE to create and fill in one of these
 * structures, and then pass it to layout when creating the
 * DocumentContext.  The document context will keep
 * a pointer to this structure, and use the functions in it to
 * interface with the FE.  The fe_context which is the first parameter
 * of each of these functions, is the opaque object that the FE will
 * use to determine which window this command is directed at, the fe_context
 * object is passed as a separate value to LO_CreateDocumentContext() so
 * that multiple fe_context objects can share the same fe_functions
 * structure.
 *
 * Eventually this should be replaced with a JMC interface.
 */

#include "lo_ele.h"
#include "URL.h"

typedef struct fe_functions {
	void (*ShiftImage)(void *fe_context,
                        LO_ImageStruct *image);

	void (*GetJavaAppSize)(void *fe_context,
			LO_JavaAppStruct *java, URL_ReloadMethod force_reload);
	void (*GetEmbedSize)(void *fe_context,
			LO_EmbedStruct *embed, URL_ReloadMethod force_reload);
	void (*GetImageInfo)(void *fe_context,
			LO_ImageStruct *image, URL_ReloadMethod force_reload);

	int (*GetTextInfo)(void *fe_context,
			LO_TextStruct *text, LO_TextInfo *text_info);
	char *(*TranslateISOText)(void *fe_context,
			int charset, char *ISO_Text);

	void (*DisplayText)(void *fe_context,
			int iLocation, LO_TextStruct *text, XP_Bool need_bg);
	void (*DisplaySubtext)(void *fe_context,
			int iLocation, LO_TextStruct *text,
			int32 start_pos, int32 end_pos, XP_Bool need_bg);
	void (*DisplayEmbed)(void *fe_context,
			int iLocation, LO_EmbedStruct *embed);
#ifdef SHACK
	void (*DisplayBuiltin)(void *fe_context,
			int iLocation, LO_BuiltinStruct *builtin);
#endif
	void (*DisplayJavaApp)(void *fe_context,
			int iLocation, LO_JavaAppStruct *java_app);
	void (*DisplayImage)(void *fe_context,
			int iLocation, LO_ImageStruct *image);
	void (*DisplaySubImage)(void *fe_context,
			int iLocation, LO_ImageStruct *image,
			int32 x, int32 y, uint32 width, uint32 height);
	void (*DisplayEdge)(void *fe_context,
			int iLocation, LO_EdgeStruct *edge);
	void (*DisplayTable)(void *fe_context,
			int iLocation, LO_TableStruct *table);
	void (*DisplaySubDoc)(void *fe_context,
			int iLocation, LO_SubDocStruct *subdoc);
	void (*DisplayCell)(void *fe_context,
			int iLocation, LO_CellStruct *cell);
	void (*DisplayLineFeed)(void *fe_context,
		int iLocation, LO_LinefeedStruct *lfeed, XP_Bool need_bg);
	void (*DisplayHR)(void *fe_context,
			int iLocation, LO_HorizRuleStruct *hrule);
	void (*DisplayBullet)(void *fe_context,
			int iLocation, LO_BulletStruct *bullet);
	void (*DisplayFormElement)(void *fe_context,
			int iLocation, LO_FormElementStruct *form_element);

	void (*GetFormElementInfo)(void *fe_context,
			LO_FormElementStruct *form_element);
	void (*GetFormElementValue)(void *fe_context,
			LO_FormElementStruct *form_element, XP_Bool hide);
	void (*FormTextIsSubmit)(void *fe_context,
			LO_FormElementStruct *single_text_ele);
	void (*FreeFormElement)(void *fe_context,
			LO_FormElementData *form_data);
	void (*ResetFormElement)(void *fe_context,
			LO_FormElementStruct *form_element);
	void (*SetFormElementToggle)(void *fe_context,
			LO_FormElementStruct *form_element, XP_Bool toggle);

	void (*FreeEdgeElement)(void *fe_context,
			LO_EdgeStruct *edge);
	void (*FreeEmbedElement)(void *fe_context,
			LO_EmbedStruct *embed);
#ifdef SHACK
	void (*FreeBuiltinElement)(void *fe_context,
			LO_BuiltlinStruct *builtin);
#endif
	void (*HideJavaAppElement)(void *fe_context,
			void *session_data);
	void (*FreeImageElement)(void *fe_context,
			LO_ImageStruct *image);

	void (*GetFullWindowSize)(void *fe_context,
			int32 *width, int32 *height);
	void (*GetEdgeMinSize)(void *fe_context,
			int32 *size
#if defined(XP_WIN) || defined(XP_OS2)
			, Bool no_edge
#endif
			);
	void (*LoadGridCellFromHistory)(void *fe_context,
			void *hist, URL_ReloadMethod force_reload);
	/* The void * return value here is an opaque fe_context
	 * object for the newly created grid window. */
	void *(*MakeGridWindow)(void *fe_context,
			void *history,
			int32 x, int32 y, int32 width, int32 height,
			char *url_str, char *window_name, int8 scrolling,
			URL_ReloadMethod force_reload
#if defined (XP_WIN) || defined (XP_MAC) || defined(XP_OS2)
			, Bool no_edge
#endif
			);
	void (*RestructureGridWindow)(void *fe_context,
			int32 x, int32 y, int32 width, int32 height);
	void *(*FreeGridWindow)(void *fe_context,
			XP_Bool save_history);

	Bool (*SecurityDialog)(void *fe_context,
			int message);

	void (*GetDocPosition)(void *fe_context,
			int location, int32 *x, int32 *y);
	void (*SetDocPosition)(void *fe_context,
			int location, int32 x, int32 y);
	void (*ScrollDocTo)(void *fe_context,
			int location, int32 x, int32 y);
	void (*SetDocDimension)(void *fe_context,
			int location, int32 width, int32 height);
	void (*ClearView)(void *fe_context,
			int location);
	void (*SetDocTitle)(void *fe_context,
			char *title);
	void (*SetBackgroundColor)(void *fe_context,
			uint8 red, uint8 green, uint8 blue);

	void (*SetProgressBarPercent)(void *fe_context,
			int32 percent);

	void (*LayoutNewDocument)(void *fe_context,
		URL *url_struct, int32 *width, int32 *height,			/* tj */
		int32 *margin_width, int32 *margin_height);
	void (*FinishedLayout)(void *fe_context);
	void (*BeginPreSection)(void *fe_context);
	void (*EndPreSection)(void *fe_context);

	void (*GetTextFrame)(void *fe_context,
		LO_TextStruct *text, int32 start, int32 end, XP_Rect *frame);

	void (*EraseBackground)(void *fe_context,
			int location, int32 x, int32 y,
			uint32 width, uint32 height,
			LO_Color *bg, LO_ImageStruct *image);

	PRBool (*HandleLayerEvent)(void *fe_context,
			CL_Layer *layer, CL_Event *event);

	void (*GetOrigin)(void *fe_context,
			int location, int32 *x, int32 *y);
	void (*SetOrigin)(void *fe_context,
			int location, int32 x, int32 y);

} FE_Functions; /* tj */

#endif /* _LayoutFunctions_ */
