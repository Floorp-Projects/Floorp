/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


/* This file should contain prototypes of all public functions for all
   modules in the client library.

   This file will be included automatically when source includes "client.h".
   By the time this file is included, all global typedefs have been executed.
*/

/* make sure we only include this once */
#ifndef _PROTO_H_
#define _PROTO_H_

#include "ntypes.h"
#include "lo_ele.h"

#ifndef NSPR20
#if defined(__sun)
# include "sunos4.h"
#endif /* __sun */
#endif /* NSPR20 */

XP_BEGIN_PROTOS

/* put your prototypes here..... */

/* --------------------------------------------------------------------------
 * Parser stuff
 */

extern intn PA_ParserInit(PA_Functions *);
extern NET_StreamClass *PA_BeginParseMDL(FO_Present_Types, void *,
	URL_Struct *, MWContext *);
extern intn PA_ParseBlock(NET_StreamClass *, const char *, int);
extern void PA_MDLComplete(NET_StreamClass *);
extern void PA_MDLAbort(NET_StreamClass *, int);
extern Bool PA_HasMocha(PA_Tag *tag);
extern PA_Tag * PA_CloneMDLTag(PA_Tag * src);
extern intn PA_ParseStringToTags(MWContext *, char *, int32, void *);
extern const char *PA_TagString(int32);
extern int32 PA_TagIndex(char *);


/* --------------------------------------------------------------------------
 * Layout stuff
 */

/*#ifndef NO_TAB_NAVIGATION */

extern Bool LO_isTabableElement(MWContext *context, LO_TabFocusData *pCurrentFocus );
extern Bool LO_isTabableFormElement( LO_FormElementStruct * next_ele );
extern Bool LO_isFormElementNeedTextTabFocus( LO_FormElementStruct *pElement );
extern LO_Element * LO_getFirstLastElement(MWContext *context, int wantFirst );
extern Bool LO_getNextTabableElement( MWContext *context, LO_TabFocusData *currentFocus, int forward );

/* NO_TAB_NAVIGATION  */

extern LO_FormElementStruct *
LO_ReturnNextFormElement(MWContext *context,
                         LO_FormElementStruct *current_element);
extern LO_FormElementStruct *
LO_ReturnPrevFormElement(MWContext *context,
                         LO_FormElementStruct *current_element);

/* NO_TAB_NAVIGATION, 
	LO_ReturnNextFormElementInTabGroup() is used to tab through form elements.
	Since the winfe now has TAB_NAVIGATION, it is not used any more.
	If mac and Unix don't use it either, it can be removed.
*/
extern LO_FormElementStruct *
LO_ReturnNextFormElementInTabGroup(MWContext *context,
                                   LO_FormElementStruct *current_element,
						 		   XP_Bool go_backwards);

extern intn LO_ProcessTag(void *, PA_Tag *, intn);
extern void LO_RefreshArea(MWContext *context, int32 left, int32 top,
	uint32 width, uint32 height);
extern Bool LO_CheckForUnload(MWContext *context);

#ifdef LAYERS
extern void LO_MoveLayer(CL_Layer *layer, int32 x, int32 y);
extern int32 LO_GetLayerXOffset(CL_Layer *layer);
extern int32 LO_GetLayerYOffset(CL_Layer *layer);

extern int32 LO_GetLayerWrapWidth(CL_Layer *layer);
extern int32 LO_GetLayerScrollWidth(CL_Layer *layer);
extern int32 LO_GetLayerScrollHeight(CL_Layer *layer);
extern void LO_SetLayerScrollWidth(CL_Layer *layer, uint32 width);
extern void LO_SetLayerScrollHeight(CL_Layer *layer, uint32 height);

extern void LO_SetLayerBbox(CL_Layer *layer, XP_Rect *bbox);

#ifdef DOM
/* Setters for span contents */
extern void LO_SetSpanColor(MWContext *context, void *span, LO_Color *color);
extern void LO_SetSpanBackground(MWContext *context, void *span, LO_Color *color);
extern void LO_SetSpanFontFamily(MWContext* context, void *span, char *family);
extern void LO_SetSpanFontWeight(MWContext* context, void *span, char *weight);
extern void LO_SetSpanFontSize(MWContext* context, void *span, int32 size);
extern void LO_SetSpanFontSlant(MWContext* context, void *span, char *slant);
#endif

extern void LO_SetLayerBgColor(CL_Layer *layer, LO_Color *color);
extern LO_Color * LO_GetLayerBgColor(CL_Layer *layer);
extern void LO_SetLayerBackdropURL(CL_Layer *layer, const char *url);
extern const char *LO_GetLayerBackdropURL(CL_Layer *layer);
extern LO_ImageStruct *LO_GetLayerBackdropImage(CL_Layer *layer);
extern void LO_SetImageURL(MWContext *context, IL_GroupContext *mocha_img_cx,
                           LO_ImageStruct *image, const char *url,
                           NET_ReloadMethod reload_policy);
extern void LO_SetDocBgColor(MWContext *context, LO_Color *rgb);

extern void
lo_SetLayerClipExpansionPolicy(CL_Layer *layer, int policy);
extern int
lo_GetLayerClipExpansionPolicy(CL_Layer *layer);

#ifdef JAVA
/* Java Applet layer code */
extern void LO_SetJavaAppTransparent(LO_JavaAppStruct *javaData);
#endif

extern void LO_SetEmbedType(LO_EmbedStruct *embed, PRBool is_windowed);
extern void LO_SetEmbedSize( MWContext *context, LO_EmbedStruct *embed, int32 width, int32 height );

#ifdef JAVA
extern void LO_SetJavaAppTransparent(LO_JavaAppStruct *javaData);
#endif

extern void *LO_GetLayerMochaObjectFromId(MWContext *context, int32 layer_id);
extern void *LO_GetLayerMochaObjectFromLayer(MWContext *context, 
                                             CL_Layer *layer);
extern void LO_SetLayerMochaObject(MWContext *context, int32 layer_id, 
                                   void *mocha_object);
extern CL_Layer *LO_GetLayerFromId(MWContext *context, int32 layer_id);
extern int32 LO_GetIdFromLayer(MWContext *context, CL_Layer *layer);
extern int32 LO_GetNumberOfLayers(MWContext *context);
#endif
extern NET_ReloadMethod LO_GetReloadMethod(MWContext *context);

#ifdef LAYERS 
extern LO_Element *LO_XYToElement(MWContext *, int32, int32, CL_Layer *);
extern LO_Element *LO_XYToNearestElement(MWContext *, int32, int32, 
                                         CL_Layer *);
#else
extern LO_Element *LO_XYToElement(MWContext *, int32, int32);
extern LO_Element *LO_XYToNearestElement(MWContext *, int32, int32);
#endif /* LAYERS */

extern void LO_MoveGridEdge(MWContext *context, LO_EdgeStruct *edge,
	int32 x, int32 y);
extern void LO_SetImageInfo(MWContext *context, int32 ele_id,
	int32 width, int32 height);
extern void LO_SetForceLoadImage(char *url, XP_Bool all_images);
extern void LO_SetUserOverride(Bool override);
extern void LO_SetDefaultBackdrop(char *url);
extern void LO_SetDefaultColor(intn type, uint8 red, uint8 green, uint8 blue);
extern Bool LO_ParseRGB(char *rgb, uint8 *red, uint8 *green, uint8 *blue);
extern Bool LO_ParseStyleSheetRGB(char *rgb, uint8 *red, uint8 *green, uint8 *blue);
extern void LO_ClearBackdropBlock(MWContext *context,
	LO_ImageStruct *image, Bool fg_ok);
extern void LO_ClearEmbedBlock(MWContext *context, LO_EmbedStruct *embed);
extern Bool LO_BlockedOnImage(MWContext *, LO_ImageStruct *image);
extern void LO_CloseAllTags(MWContext *);
extern void LO_DiscardDocument(MWContext *);
extern LO_FormSubmitData *LO_SubmitForm(MWContext *context,
	LO_FormElementStruct *form_element);
extern LO_FormSubmitData *LO_SubmitImageForm(MWContext *context,
	LO_ImageStruct *image, int32 x, int32 y);
extern void LO_ResetForm(MWContext *context,
	LO_FormElementStruct *form_element);
extern LO_FormElementStruct *
LO_FormRadioSet(MWContext *context, LO_FormElementStruct *form_element);
extern void LO_SaveFormData(MWContext *context);
extern void LO_CloneFormData(SHIST_SavedData *, MWContext *context,
	URL_Struct *url_struct);
extern void LO_HighlightAnchor(MWContext *context, LO_Element *element,Bool on);
#ifdef OLD_POS_HIST
extern void LO_SetDocumentPosition(MWContext *context, int32 x, int32 y);
#endif /* OLD_POS_HIST */
extern void LO_SetDocumentDimensions(MWContext *context,
                                     int32 width, int32 height);
#ifdef LAYERS
extern void LO_StartSelection(MWContext *context, int32 x, int32 y, 
                              CL_Layer *layer);
#else
extern void LO_StartSelection(MWContext *context, int32 x, int32 y);
#endif /* LAYERS */

typedef enum {
	SMALL_BM_ICON,
	LARGE_BM_ICON
} BMIconType;

extern char * LO_GetBookmarkIconURLForPage(MWContext *context, BMIconType type);

/* Re-layout layout elements on resize without destroying them and reloading page
   from scratch */
extern void LO_RelayoutOnResize(MWContext *context, int32 width, int32 height, int32 leftMargin, int32 topMargin);
/* Re-layout layout elements when one changes size. */
extern void LO_RelayoutFromElement(MWContext *context, LO_Element *element);

extern void LO_ExtendSelection(MWContext *context, int32 x, int32 y);
extern void LO_EndSelection(MWContext *context);
extern void LO_ClearSelection(MWContext *context);
extern XP_Block LO_GetSelectionText(MWContext *context);
extern Bool LO_FindText(MWContext *context, char *text,
        LO_Element **start_ele_loc, int32 *start_position,
        LO_Element **end_ele_loc, int32 *end_position,
        Bool use_case, Bool forward);
extern Bool LO_FindGridText(MWContext *context, MWContext **ret_context,
	char *text,
        LO_Element **start_ele_loc, int32 *start_position,
        LO_Element **end_ele_loc, int32 *end_position,
        Bool use_case, Bool forward);
extern Bool LO_SelectAll(MWContext *context);
extern void LO_SelectText(MWContext *context, LO_Element *start,int32 start_pos,
        LO_Element *end, int32 end_pos, int32 *x, int32 *y);
extern void LO_RefreshAnchors(MWContext *context);
extern Bool LO_HaveSelection(MWContext *context);
extern void LO_GetSelectionEndpoints(MWContext *context,
	LO_Element **start, LO_Element **end, int32 *start_pos, int32 *end_pos, CL_Layer **sel_layer);
extern void LO_FreeSubmitData(LO_FormSubmitData *submit_data);
extern void LO_FreeDocumentFormListData(MWContext *context, void *form_data);
extern void LO_FreeDocumentEmbedListData(MWContext *context, void *embed_data);
extern void LO_FreeDocumentGridData(MWContext *context, void *grid_data);
extern void LO_FreeDocumentAppletData(MWContext *context, void *applet_data);
extern void LO_RedoFormElements(MWContext *context);
extern void LO_InvalidateFontData(MWContext *context);
extern void LO_GetDocumentMargins(MWContext *context,
	int32 *margin_width, int32 *margin_height);
extern Bool LO_HasBGImage(MWContext *context);
extern Bool LO_LocateNamedAnchor(MWContext *context, URL_Struct *url_struct,
	int32 *xpos, int32 *ypos);
extern int32 LO_EmptyRecyclingBin(MWContext *context);
extern LO_AnchorData *LO_MapXYToAreaAnchor(MWContext *context,
	LO_ImageStruct *image, int32 x, int32 y);
extern intn LO_DocumentInfo(MWContext *context, NET_StreamClass *stream);
extern intn LO_ChangeFontSize(intn size, char *size_str);
extern double LO_GetScalingFactor(int32 scaler);
extern int16 LO_WindowWidthInFixedChars(MWContext *context);
extern void LO_CleanupGridHistory(MWContext *context);
extern void LO_UpdateGridHistory(MWContext *context);
extern Bool LO_BackInGrid(MWContext *context);
extern Bool LO_ForwardInGrid(MWContext *context);
extern Bool LO_GridCanGoForward(MWContext *context);
extern Bool LO_GridCanGoBackward(MWContext *context);

#if defined(SingleSignon)
extern void SI_RememberSignonData
    (MWContext *context, LO_FormSubmitData *submit);
extern void SI_RestoreOldSignonData
    (MWContext *context, LO_FormElementStruct *form_element, char *URLName);
extern int SI_LoadSignonData(char *filename);
extern int SI_SaveSignonData(char *filename);
extern void SI_RemoveAllSignonData();
extern Bool SI_RemoveUser(char *URLName, char *userName, Bool save);
extern int SI_PromptUsernameAndPassword
    (MWContext *context, char *buf,
    char **username, char **password, char *URLName);
extern char *SI_PromptPassword
    (MWContext *context, char *prompt, char *URLName,
    Bool pickFirstUser, Bool useLastPassword);
extern char * SI_Prompt
    (MWContext *context, char *prompt, char* defaultUsername, char *URLName);
extern void SI_StartOfForm();
#endif

#ifdef LAYERS
extern Bool LO_Click( MWContext *context, int32 x, int32 y, 
                      Bool requireCaret, CL_Layer *layer );
extern void LO_Hit(MWContext *context, int32 x, int32 y,
	Bool requireCaret, LO_HitResult *result, CL_Layer *layer);
#else
extern Bool LO_Click( MWContext *context, int32 x, int32 y, Bool requireCaret );
extern void LO_Hit(MWContext *context, int32 x, int32 y,
	Bool requireCaret, LO_HitResult *result);
#endif /* LAYERS */

extern int32 LO_TextElementWidth(MWContext *context, LO_TextStruct *text_ele, int charOffset);

extern void LO_AddEmbedData(MWContext* context, LO_EmbedStruct* embed, void* data);
extern void LO_CopySavedEmbedData(MWContext* context, SHIST_SavedData* newdata);
extern NET_StreamClass* LO_NewObjectStream(FO_Present_Types format_out, void* type,
				   						   URL_Struct* urls, MWContext* context);

extern void LO_CreateReblockTag(MWContext* context, LO_Element* element);
extern void LO_LockLayout(void);
extern void LO_UnlockLayout(void);
extern Bool LO_VerifyUnlockedLayout();
extern void LO_NetlibComplete(MWContext * context);

extern void LO_UpdateTextData(lo_FormElementTextData * textData, const char * text);

#ifdef EDITOR

/* --------------------------------------------------------------------------
 * Layout stuff specific to the editor
 */


#ifdef LAYERS
extern void LO_PositionCaret( MWContext *context, int32 x, int32 y,
                              CL_Layer *layer);
extern void LO_DoubleClick( MWContext *context, int32 x, int32 y, 
                            CL_Layer *layer );
#else
extern void LO_PositionCaret( MWContext *context, int32 x, int32 y );
extern void LO_DoubleClick( MWContext *context, int32 x, int32 y );
#endif /* LAYERS */

void LO_PositionCaretBounded(MWContext *context, int32 x, int32 y,
			int32 minY, int32 maxY );

extern ED_Buffer* LO_GetEDBuffer( MWContext *context);
extern void LO_GetEffectiveCoordinates( MWContext *pContext, LO_Element *pElement, int32 position,
     int32* pX, int32* pY, int32* pWidth, int32* pHeight );
extern void LO_UpDown( MWContext *pContext, LO_Element *pElement, int32 position, int32 iDesiredX, Bool bSelect, Bool bForward );
extern Bool LO_PreviousPosition( MWContext *pContext,LO_Element *pElement, intn iOffset, 
			ED_Element **ppEdElement, intn* pOffset);
extern Bool LO_NextPosition( MWContext *pContext,LO_Element *pElement, intn iOffset, 
			ED_Element **ppEdElement, intn* pOffset);
extern Bool LO_NavigateChunk( MWContext *pContext, intn chunkType, Bool bSelect, Bool bForward );
extern Bool LO_ComputeNewPosition( MWContext *context, intn chunkType,
    Bool bSelect, Bool bDeselecting, Bool bForward,
    LO_Element** pElement, int32* pPosition );

extern LO_Element* LO_BeginOfLine( MWContext *pContext, LO_Element *pElement );
extern LO_Element* LO_EndOfLine( MWContext *pContext, LO_Element *pElement);
extern LO_Element* LO_FirstElementOnLine( MWContext *pContext,
	int32 x, int32 y, int32 *pLineNum);
#ifdef LAYERS
extern void LO_StartSelectionFromElement( MWContext *context, LO_Element *eptr, 
            int32 new_pos, CL_Layer *layer );
#else
extern void LO_StartSelectionFromElement( MWContext *context, LO_Element *eptr, 
            int32 new_pos );
#endif /* LAYERS */
extern void LO_ExtendSelectionFromElement( MWContext *context, LO_Element *eptr, 
            int32 position, Bool bFromStart );
extern Bool LO_SelectElement( MWContext *context, LO_Element *eptr, 
	int32 position, Bool bFromStart );
extern void LO_SelectRegion( MWContext *context, LO_Element *begin,
	int32 beginPosition, LO_Element *end, int32 endPosition,
	Bool fromStart, Bool forward  );
extern Bool LO_IsSelected( MWContext *context );
extern Bool LO_IsSelectionStarted( MWContext *context );
extern void LO_GetSelectionEndPoints( MWContext *context, 
	LO_Element **ppStart, intn *pStartOffset, LO_Element **ppEnd, 
	intn *pEndOffset, Bool *pbFromStart, Bool *pbSingleElementSelection );
extern void LO_GetSelectionNewPoint( MWContext *context, 
	LO_Element **ppNew, intn *pNewOffset);
extern LO_Element* LO_PreviousEditableElement( LO_Element *pElement );
extern LO_Element* LO_NextEditableElement( LO_Element *pElement );
extern LO_ImageStruct* LO_NewImageElement( MWContext* context );
extern void LO_SetBackgroundImage( MWContext *context, char *pUrl );
extern void LO_RefetchWindowDimensions( MWContext *pContext );
extern void LO_Relayout( MWContext *context, ED_TagCursor *pCursor,
	int32 iLine, int iStartEditOffset, XP_Bool bDisplayTables );

#endif /*EDITOR*/

extern void LO_SetBaseURL( MWContext *pContext, char *pURL );
extern char* LO_GetBaseURL( MWContext *pContext );

#if 0
extern Bool LO_Click( MWContext *context, int32 x, int32 y, Bool requireCaret );
extern void LO_Hit(MWContext *context, int32 x, int32 y, Bool requireCaret, LO_HitResult *result);
#endif

#ifdef LAYERS
extern void LO_SelectObject( MWContext *context, int32 x, int32 y,
                             CL_Layer *layer );
#else
extern void LO_SelectObject( MWContext *context, int32 x, int32 y );
#endif /* LAYERS */

#ifdef LAYERS
extern LO_LayerType LO_GetLayerType(CL_Layer *layer);
extern Bool LO_PrepareLayerForWriting(MWContext *context, int32 layer_id, 
                                      const char *referer, int32 width);
extern Bool LO_SetLayerSrc(MWContext *context, int32 layer_id, char *str, 
                           const char *referer, int32 width);
extern int32 LO_CreateNewLayer(MWContext *context, int32 wrap_width, int32 parent_layer_id);
#endif /* LAYERS */

#ifdef XP_UNIX
extern void LO_DisplayFormElement(LO_FormElementStruct *form);
#endif /* XP_UNIX */

/* Allows front ends to test for empty cell.
 * Used in Composer to display "zero width border"
 * in cells that layout would normally not display
*/
XP_Bool LO_IsEmptyCell(LO_CellStruct *cell);

Bool LO_LayingOut(MWContext * context);


#ifdef DOM
/* Returns true if the layout element is enclosed
   in <SPAN> </SPAN> */
Bool LO_IsWithinSpan( LO_Element *ele );
#endif

/*
 * This is probably the wrong place to add this, but tough,
 * this stuff shouldn't have to exist anyways.
 */
extern void XP_InitializeContext(MWContext *context);
extern MWContext *XP_NewContext(void);
extern void XP_DeleteContext(MWContext *context);
extern XP_List *XP_GetGlobalContextList(void);
extern void XP_AddContextToList(MWContext *context);
extern Bool XP_IsContextInList(MWContext *context);
extern void XP_RemoveContextFromList(MWContext *context);
extern MWContext *XP_GetNonGridContext(MWContext *context);
extern Bool		  XP_IsChildContext(MWContext* parent,  MWContext* child);

extern MWContext *XP_FindNamedContextInList(MWContext * context, char *name);
extern MWContext *XP_FindContextOfType(MWContext *, MWContextType);
extern MWContext *XP_FindSomeContext(void);
extern Bool XP_FindNamedAnchor(MWContext * context, URL_Struct * url, 
			       int32 *xpos, int32 *ypos);
extern void XP_RefreshAnchors(void);
extern void XP_InterruptContext(MWContext * context);
extern Bool XP_IsContextBusy(MWContext * context);
extern Bool XP_IsContextStoppable(MWContext * context);
extern void XP_UpdateParentContext(MWContext * context);
extern int XP_GetSecurityStatus(MWContext *pContext);
extern int XP_ContextCount(MWContextType cxType, XP_Bool bTopLevel);

XP_END_PROTOS

# endif /* _PROTO_H_ */
