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
** These functions are useless for our porpoises  I don't include
** any headers so I don't have to prototype them.
*/

#include "xp_core.h"
#include "il_types.h"

/* this little bit of stupidity wouldn't be necessary
        if we had XP_BEGIN_PROTOS defined properly for
        the Mac
*/

void xl_alldone() { }
void PSFE_SetDocPosition() {}
void PSFE_GetDocPosition() {}
void PSFE_FreeFormElement() { }
void PSFE_ClearView() { }
void PSFE_CreateNewDocWindow() { }
#ifdef EDITOR
void TXFE_CreateNewEditWindow() { }
#endif
void PSFE_SetFormElementToggle() {}
void PSFE_SetProgressBarPercent() { }
void TXFE_SetDocPosition() {}
void TXFE_GetDocPosition() {}
void TXFE_FreeFormElement() { }
void TXFE_ClearView() { }
void TXFE_CreateNewDocWindow() { }
void TXFE_SetFormElementToggle() {}
void TXFE_GetFormElementInfo() { }
void TXFE_SetProgressBarPercent() { }
void PSFE_FormTextIsSubmit() {}
void PSFE_GetFormElementValue() {}
void PSFE_ResetFormElement() {}
void TXFE_FormTextIsSubmit() {}
void TXFE_GetFormElementValue() {}
void TXFE_ResetFormElement() {}
void TXFE_DisplayFormElement() { }
void TXFE_SetDocDimension() { }
void TXFE_SetDocTitle() { }
void TXFE_BeginPreSection() { }
void TXFE_EndPreSection() { }
void PSFE_DisplaySubDoc() { }
void PSFE_UseFancyFTP () {}
void PSFE_FileSortMethod () {}
void PSFE_EnableClicking () {}
void PSFE_ShowAllNewsArticles () {}
void PSFE_GraphProgressDestroy () {}
void TXFE_Confirm () {}
void TXFE_Alert () {}
void TXFE_GraphProgressDestroy () {}
void TXFE_Progress () {}
void TXFE_Prompt () {}
void TXFE_PromptUsernameAndPassword () {}
void TXFE_PromptWithCaption () {}
void PSFE_GraphProgressInit () {}
void TXFE_UseFancyNewsgroupListing () {}
void PSFE_Progress () {}
void TXFE_FileSortMethod () {}
void TXFE_UseFancyFTP () {}
void TXFE_GraphProgress () {}
void PSFE_Confirm () {}
void TXFE_EnableClicking () {}
void TXFE_ShowAllNewsArticles () {}
void PSFE_Prompt () {}
void PSFE_PromptUsernameAndPassword () {}
void PSFE_PromptWithCaption() {}
void PSFE_GraphProgress () {}
void PSFE_Alert () {}
void TXFE_GraphProgressInit () {}
void PSFE_UseFancyNewsgroupListing () {}
void TXFE_SetBackgroundColor() {}
void PSFE_SetBackgroundColor() {}
void TXFE_GetEmbedSize(){}
/*void PSFE_GetEmbedSize(){}*/
void TXFE_FreeEmbedElement(){}
/*void PSFE_FreeEmbedElement(){}*/
void TXFE_CreateEmbedWindow(){}
/*void PSFE_CreateEmbedWindow(){}*/
void TXFE_SaveEmbedWindow(){}
/*void PSFE_SaveEmbedWindow(){}*/
void TXFE_RestoreEmbedWindow(){}
/*void PSFE_RestoreEmbedWindow(){}*/
void TXFE_DestroyEmbedWindow(){}
/*void PSFE_DestroyEmbedWindow(){}*/
void TXFE_DisplayEmbed(){}
/* void PSFE_DisplayEmbed(){} */
void TXFE_GetJavaAppSize(){}
void PSFE_GetJavaAppSize(){}
void TXFE_FreeJavaAppElement(){}
void PSFE_FreeJavaAppElement(){}
void TXFE_HideJavaAppElement(){}
void PSFE_HideJavaAppElement(){}
void TXFE_DisplayJavaApp(){}
void TXFE_DrawJavaApp(){}
void PSFE_DrawJavaApp(){}
void TXFE_HandleClippingView(){}
void PSFE_HandleClippingView(){}
void TXFE_FreeEdgeElement(){}
void PSFE_FreeEdgeElement(){}
void TXFE_DisplayCell(){}
void TXFE_DisplayEdge(){}
void PSFE_DisplayEdge(){}
void TXFE_UpdateStopState(){}
void PSFE_UpdateStopState(){}
#ifdef XP_OS2       /*performance*/
void TXFE_GetMaxWidth(){}
void PSFE_GetMaxWidth(){}
#endif
#ifdef LAYERS
void TXFE_EraseBackground(){}
void PSFE_EraseBackground(){}
void TXFE_SetDrawable(){}
void PSFE_SetDrawable(){}

/* Could be implemented for Postscript and Text, but it's only used for selection */
void TXFE_GetTextFrame(){}
void PSFE_GetTextFrame(){}
#endif
/* Webfonts */
void TXFE_LoadFontResource(){}
void PSFE_LoadFontResource(){}

void TXFE_DisplayBuiltin() {}
void PSFE_DisplayBuiltin() {}

void TXFE_FreeBuiltinElement() {}
void PSFE_FreeBuiltinElement() {}
