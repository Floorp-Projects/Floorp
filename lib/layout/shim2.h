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


extern void shim2_LO_ClearEmbedBlock(DocumentContext context,
				LO_EmbedStruct *embed);
extern void shim2_LO_CopySavedEmbedData(DocumentContext context,
				void *saved_data);
extern void shim2_LO_AddEmbedData(DocumentContext context,
				LO_EmbedStruct *embed, void *session_data);
extern void shim2_LO_FreeDocumentEmbedListData(DocumentContext context,
				void *data);

extern Bool shim2_LO_FindText(DocumentContext context, char *text,
			LO_Element **start_ele_loc, int32 *start_position,
			LO_Element **end_ele_loc, int32 *end_position,
			Bool use_case, Bool forward);
extern Bool shim2_LO_FindGridText(DocumentContext context,
			DocumentContext *ret_context, char *text,
			LO_Element **start_ele_loc, int32 *start_position,
			LO_Element **end_ele_loc, int32 *end_position,
			Bool use_case, Bool forward);

extern void shim2_LO_SelectText(DocumentContext context,
			LO_Element *start, int32 start_pos,
			LO_Element *end, int32 end_pos, int32 *x, int32 *y);
extern void shim2_LO_StartSelection(DocumentContext context,
			int32 x, int32 y, CL_Layer *layer);
extern Bool shim2_LO_Click(DocumentContext context,
			int32 x, int32 y, Bool requireCaret, CL_Layer *layer);
extern void shim2_LO_SelectObject(DocumentContext context,
			int32 x, int32 y, CL_Layer *layer);
extern void shim2_LO_Hit(DocumentContext context,
			int32 x, int32 y, Bool requireCaret,
			LO_HitResult* result, CL_Layer *layer);
extern void shim2_LO_ExtendSelection(DocumentContext context,
			int32 x, int32 y);
extern void shim2_LO_EndSelection(DocumentContext context);
extern void shim2_LO_ClearSelection(DocumentContext context);
extern Bool shim2_LO_HaveSelection(DocumentContext context);
extern XP_Block shim2_LO_GetSelectionText(DocumentContext context);
extern void shim2_LO_GetSelectionEndpoints(DocumentContext context,
			LO_Element **start, LO_Element **end,
			int32 *start_pos, int32 *end_pos);
extern Bool shim2_LO_SelectAll(DocumentContext context);
extern int32 shim2_LO_TextElementWidth(DocumentContext context,
			LO_TextStruct *text_ele, int charOffset);

extern LO_FormElementStruct *shim2_LO_ReturnNextFormElement(
			DocumentContext context,
                        LO_FormElementStruct *current_element);
extern LO_FormElementStruct *shim2_LO_ReturnPrevFormElement(
			DocumentContext context,
			LO_FormElementStruct *current_element);
extern LO_FormElementStruct *shim2_LO_ReturnNextFormElementInTabGroup(
		DocumentContext win_context,
		LO_FormElementStruct *current_element, XP_Bool go_backwards);

extern LO_FormSubmitData *shim2_LO_SubmitForm(DocumentContext context,
			LO_FormElementStruct *form_element);
extern LO_FormSubmitData *shim2_LO_SubmitImageForm(DocumentContext context,
			LO_ImageStruct *image, int32 x, int32 y);
extern void shim2_LO_RedoFormElements(DocumentContext context);
extern void shim2_LO_ResetForm(DocumentContext context,
			LO_FormElementStruct *form_element);
extern LO_FormElementStruct *shim2_LO_FormRadioSet(DocumentContext context,
			LO_FormElementStruct *form_element);
extern void shim2_LO_SaveFormData(DocumentContext context);
extern void shim2_LO_CloneFormData(void* savedData,
			DocumentContext context, URL *url_struct);
extern lo_FormData *shim2_LO_GetFormDataByID(DocumentContext context,
			intn form_id);
extern uint shim2_LO_EnumerateForms(DocumentContext context);
extern uint shim2_LO_EnumerateFormElements(DocumentContext context,
			lo_FormData *form);

extern void shim2_LO_FreeDocumentFormListData(DocumentContext context,
			void *form_data);

extern int32 shim2_LO_EmptyRecyclingBin(DocumentContext context);

extern void shim2_LO_MoveGridEdge(DocumentContext context,
				LO_EdgeStruct *fe_edge, int32 x, int32 y);
extern void shim2_LO_UpdateGridHistory(DocumentContext context);
extern void shim2_LO_CleanupGridHistory(DocumentContext context);
extern void shim2_LO_FreeDocumentGridData(DocumentContext context,
				void *data);
extern Bool shim2_LO_BackInGrid(DocumentContext context);
extern Bool shim2_LO_ForwardInGrid(DocumentContext context);
extern Bool shim2_LO_GridCanGoForward(DocumentContext context);
extern Bool shim2_LO_GridCanGoBackward(DocumentContext context);

extern Bool shim2_LO_BlockedOnImage(DocumentContext context,
				LO_ImageStruct *image);

extern intn shim2_LO_DocumentInfo(DocumentContext context,
				struct netscape_net_Stream *stream);

extern LO_AnchorData *shim2_LO_MapXYToAreaAnchor(DocumentContext context,
			LO_ImageStruct *image, int32 x, int32 y);

extern void shim2_LO_CloseAllTags(DocumentContext context);

extern void shim2_LO_RefreshArea(DocumentContext context,
			int32 x, int32 y,
			uint32 width, uint32 height);

extern LO_Element *shim2_LO_XYToElement(DocumentContext context,
			int32 x, int32 y, CL_Layer *layer);
extern LO_Element *shim2_LO_XYToNearestElement(DocumentContext context,
			int32 x, int32 y, CL_Layer *layer);

extern void shim2_LO_ClearBackdropBlock(DocumentContext context,
			LO_ImageStruct *image, Bool fg_ok);

extern void shim2_LO_SetImageInfo(DocumentContext context,
			int32 ele_id, int32 width, int32 height);

extern void shim2_LO_DiscardDocument(DocumentContext context);

extern void shim2_LO_HighlightAnchor(DocumentContext context,
			LO_Element *element, Bool on);

extern void shim2_LO_RefreshAnchors(DocumentContext context);

extern Bool shim2_LO_LocateNamedAnchor(DocumentContext context,
			URL *url_struct, int32 *xpos, int32 *ypos);

extern Bool shim2_LO_HasBGImage(DocumentContext context);

extern void shim2_LO_InvalidateFontData(DocumentContext context);

extern int16 shim2_LO_WindowWidthInFixedChars(DocumentContext context);

extern LO_ImageStruct *shim2_LO_GetImageByIndex(DocumentContext context,
				intn index);
extern uint shim2_LO_EnumerateImages(DocumentContext context);
extern struct lo_NameList_struct *shim2_LO_GetNamedAnchorByIndex(DocumentContext context,
				uint index);
extern uint shim2_LO_EnumerateNamedAnchors(DocumentContext context);
extern LO_AnchorData *shim2_LO_GetLinkByIndex(DocumentContext context,
				uint index);
extern uint shim2_LO_EnumerateLinks(DocumentContext context);
extern LO_JavaAppStruct *shim2_LO_GetAppletByIndex(DocumentContext context,
				uint index);
extern uint shim2_LO_EnumerateApplets(DocumentContext context);
extern LO_EmbedStruct *shim2_LO_GetEmbedByIndex(DocumentContext context,
				uint index);
extern uint shim2_LO_EnumerateEmbeds(DocumentContext context);

extern void shim2_LO_GetDocumentColor(DocumentContext context,
				int type, LO_Color *color);
extern void shim2_LO_SetDocumentColor(DocumentContext context,
				int type, LO_Color *color);
