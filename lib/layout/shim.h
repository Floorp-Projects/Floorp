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


extern void *lo_InitFEFunctions(void *context);

extern void shim_IL_StartPage(DocumentContext context);
extern void shim_IL_NoMoreImages(DocumentContext context);
extern void shim_IL_UseDefaultColormapThisPage(DocumentContext context);
extern void shim_IL_ColormapTag(char *url, DocumentContext context);
extern void shim_IL_ReplaceImage(DocumentContext context,
			LO_ImageStruct *ret_image, LO_ImageStruct *image);
extern void shim_FE_ShiftImage(DocumentContext context,
			LO_ImageStruct *image);


extern void shim_FE_GetJavaAppSize(DocumentContext context,
			LO_JavaAppStruct *java, URL_ReloadMethod force_reload);
extern void shim_FE_GetEmbedSize(DocumentContext context,
			LO_EmbedStruct *embed, URL_ReloadMethod force_reload);
extern void shim_FE_GetImageInfo(DocumentContext context,
			LO_ImageStruct *image, URL_ReloadMethod force_reload);

extern void shim_FE_GetTextInfo(DocumentContext context,
			LO_TextStruct *text, LO_TextInfo *text_info);
extern char *shim_FE_TranslateISOText(DocumentContext context,
			int charset, char *ISO_Text);


extern void shim_FE_DisplayText(DocumentContext context,
			int iLocation, LO_TextStruct *text,
			XP_Bool need_bg);
extern void shim_FE_DisplaySubtext(DocumentContext context,
			int iLocation, LO_TextStruct *text,
			int32 start_pos, int32 end_pos, XP_Bool need_bg);
extern void shim_FE_DisplayEmbed(DocumentContext context,
			int iLocation, LO_EmbedStruct *embed);
#ifdef SHACK
extern void shim_FE_DisplayBuiltin(DocumentContext context,
			int iLocation, LO_BuiltinStruct *builtin);
#endif
extern void shim_FE_DisplayJavaApp(DocumentContext context,
			int iLocation, LO_JavaAppStruct *java_app);
extern void shim_FE_DisplayImage(DocumentContext context,
			int iLocation, LO_ImageStruct *image);
extern void shim_FE_DisplaySubImage(DocumentContext context,
			int iLocation, LO_ImageStruct *image,
			int32 x, int32 y, uint32 width, uint32 height);
extern void shim_FE_DisplayEdge(DocumentContext context,
			int iLocation, LO_EdgeStruct *edge);
extern void shim_FE_DisplayTable(DocumentContext context,
			int iLocation, LO_TableStruct *table);
extern void shim_FE_DisplaySubDoc(DocumentContext context,
			int iLocation, LO_SubDocStruct *subdoc);
extern void shim_FE_DisplayCell(DocumentContext context,
			int iLocation, LO_CellStruct *cell);
extern void shim_FE_DisplayLineFeed(DocumentContext context,
		int iLocation, LO_LinefeedStruct *lfeed, XP_Bool need_bg);
extern void shim_FE_DisplayHR(DocumentContext context,
			int iLocation, LO_HorizRuleStruct *hrule);
extern void shim_FE_DisplayBullet(DocumentContext context,
			int iLocation, LO_BulletStruct *bullet);
extern void shim_FE_DisplayFormElement(DocumentContext context,
			int iLocation, LO_FormElementStruct *form_element);

extern void shim_FE_GetFormElementInfo(DocumentContext context,
			LO_FormElementStruct *form_element);
extern void shim_FE_GetFormElementValue(DocumentContext context,
			LO_FormElementStruct *form_element, XP_Bool hide);
extern void shim_FE_FormTextIsSubmit(DocumentContext context,
			LO_FormElementStruct *single_text_ele);
extern void shim_FE_FreeFormElement(DocumentContext context,
			LO_FormElementData *form_data);
extern void shim_FE_ResetFormElement(DocumentContext context,
			LO_FormElementStruct *form_element);
extern void shim_FE_SetFormElementToggle(DocumentContext context,
			LO_FormElementStruct *form_element, XP_Bool toggle);

extern void shim_FE_FreeEdgeElement(DocumentContext context,
			LO_EdgeStruct *edge);
extern void shim_FE_FreeEmbedElement(DocumentContext context,
			LO_EmbedStruct *embed);
#ifdef SHACK
extern void shim_FE_FreeBuiltinElement(DocumentContext context,
			LO_BuiltinStruct *builtin);
#endif
extern void shim_FE_HideJavaAppElement(DocumentContext context,
			void *session_data);
extern void shim_FE_FreeImageElement(DocumentContext context,
			LO_ImageStruct *image);

extern void shim_FE_GetFullWindowSize(DocumentContext context,
			int32 *width, int32 *height);
extern void shim_FE_GetEdgeMinSize(DocumentContext context,
			int32 *size
#if defined(XP_WIN) || defined(XP_OS2)
			, Bool no_edge
#endif
			);
extern void shim_FE_LoadGridCellFromHistory(DocumentContext context,
			void *hist, URL_ReloadMethod force_reload);
extern DocumentContext shim_FE_MakeGridWindow(DocumentContext context,
			void *history,
			int32 x, int32 y, int32 width, int32 height,
			char *url_str, char *window_name, int8 scrolling,
			URL_ReloadMethod force_reload
			, Bool no_edge
			);
extern void shim_FE_RestructureGridWindow(DocumentContext context,
			int32 x, int32 y, int32 width, int32 height);
extern void *shim_FE_FreeGridWindow(DocumentContext context,
			XP_Bool save_history);


extern void shim_FE_SecurityDialog(DocumentContext context, int message);

extern void shim_FE_GetDocPosition(DocumentContext context,
			int location, int32 *x, int32 *y);
extern void shim_FE_SetDocPosition(DocumentContext context,
			int location, int32 x, int32 y);
extern void shim_FE_ScrollDocTo(DocumentContext context,
			int location, int32 x, int32 y);
extern void shim_FE_SetDocDimension(DocumentContext context,
			int location, int32 width, int32 height);
extern void shim_FE_ClearView(DocumentContext context,
			int location);
extern void shim_FE_SetDocTitle(DocumentContext context,
			char *title);
extern void shim_FE_SetBackgroundColor(DocumentContext context,
			uint8 red, uint8 green, uint8 blue);

extern void shim_FE_SetProgressBarPercent(DocumentContext context,
			int32 percent);

extern void shim_FE_LayoutNewDocument(DocumentContext context,
		URL *url_struct, int32 *width, int32 *height,
		int32 *margin_width, int32 *margin_height);
extern void shim_FE_FinishedLayout(DocumentContext context);
extern void shim_FE_BeginPreSection(DocumentContext context);
extern void shim_FE_EndPreSection(DocumentContext context);

extern void shim_NPL_DeleteSessionData(DocumentContext context,
			void *sessionData);
extern void shim_LJ_DeleteSessionData(DocumentContext context,
			void *sessionData);

extern void shim_LJ_CreateApplet(LO_JavaAppStruct *java,
			DocumentContext context);

extern PRBool shim_SECMOZ_GenKeyFromChoice(DocumentContext context,
			LO_Element *form,
			char *choice, char *challenge,
			char **pValue, PRBool *pDone);
extern PRBool shim_SECMOZ_MakeSubmitPaymentDialog(DocumentContext context,
			LO_Element *form,
			void *values, void *cert_str,
			char *order,
			char **pValue, PRBool *pDone);

extern int16 shim_INTL_DefaultDocCharSetID(DocumentContext context);
extern int16 shim_INTL_DefaultTextAttributeCharSetID(DocumentContext context);
extern void shim_INTL_Relayout(DocumentContext context);

extern HISTORY *shim_SHIST_GetList(DocumentContext context);
extern HST_ENT *shim_SHIST_GetPrevious(DocumentContext context);
extern void shim_SHIST_FreeHistoryEntry(DocumentContext context,
				HST_ENT *hist);
extern void shim_SHIST_SetCurrentDocFormListData(DocumentContext context,
		void *data);
extern void shim_SHIST_SetCurrentDocEmbedListData(DocumentContext context,
		void *data);
extern void shim_SHIST_SetCurrentDocGridData(DocumentContext context,
		void *data);

extern void shim_LM_SendLoadEvent(DocumentContext context,
				LM_LoadEvent event);
extern void shim_LM_SendImageEvent(DocumentContext context,
				LO_ImageStruct *images, LM_ImageEvent event);
extern void shim_LM_ReleaseDocument(DocumentContext context, PRBool resize_reload);
extern void shim_LM_ClearContextStream(DocumentContext context);
/* EJB - until we have mocha modularized, MochaDecoder and MochaObject
   will become void
*/
extern void *shim_LM_GetMochaDecoder(DocumentContext context);

extern void *shim_LM_ReflectFormElement(DocumentContext context,
				LO_FormElementStruct *form_element);
extern void *shim_LM_ReflectForm(DocumentContext context,
				struct lo_FormData_struct *form_data);
extern void *shim_LM_ReflectImage(DocumentContext context,
				LO_ImageStruct *image, char *name);
extern void *shim_LM_ReflectNamedAnchor(DocumentContext context,
			struct lo_NameList_struct *name_rec, uint index);
extern void *shim_LM_ReflectLink(DocumentContext context,
				LO_AnchorData *anchor, uint index);
extern void *shim_LM_ReflectApplet(DocumentContext context,
				LO_JavaAppStruct *applet, uint index);
extern void *shim_LM_ReflectEmbed(DocumentContext context,
				LO_EmbedStruct *embed, uint index);
extern void *shim_LM_ReflectLayer(DocumentContext context,
				CL_Layer *layer, CL_Layer *parent_layer);

extern void shim_XP_InterruptContext(DocumentContext context);

/* EJB this stream interface is totally new in the modularized
   world, I don't know what to do here.
extern struct netscape_net_Stream *shim_NET_NewStream(char *name,
		       MKStreamWriteFunc put,
		       MKStreamCompleteFunc complete,
		       MKStreamAbortFunc abort,
		       MKStreamWriteReadyFunc ready,
		       void *data,
		       DocumentContext context);
*/
extern int shim_NET_GetURL(URL *url_struct,
		int16 output_format,
		DocumentContext context,
		void *exit_routine);
extern Bool shim_NET_ParseMimeHeader(DocumentContext context,
		URL *url_struct,
		char *name, char *value,
		XP_Bool is_http);

extern void shim_FE_GetTextFrame(DocumentContext context,
		LO_TextStruct *text, int32 start, int32 end, XP_Rect *frame);

extern void shim_FE_EraseBackground(DocumentContext context,
			int location, int32 x, int32 y,
			uint32 width, uint32 height,
			LO_Color *bg, LO_ImageStruct *image);

extern PRBool shim_FE_HandleLayerEvent(DocumentContext context,
			CL_Layer *layer, CL_Event *event);

extern void shim_FE_GetOrigin(DocumentContext context,
			int location, int32 *x, int32 *y);
extern void shim_FE_SetOrigin(DocumentContext context,
			int location, int32 x, int32 y);
