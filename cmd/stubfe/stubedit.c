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
   stubedit.c --- stub functions for fe
                  specific editor stuff.
*/

#include "xp_core.h"
#include "structs.h"
#include "ntypes.h"
#include "edttypes.h"
#include "edt.h"

void
FE_DisplayTextCaret(MWContext* context,
		    int loc,
		    LO_TextStruct* text_data,
		    int char_offset)
{
}

void
FE_DisplayImageCaret(MWContext* context,
		     LO_ImageStruct* pImageData,
		     ED_CaretObjectPosition caretPos)
{
}

void
FE_DisplayGenericCaret(MWContext* context,
		       LO_Any* pLoAny,
		       ED_CaretObjectPosition caretPos)
{
}

Bool
FE_GetCaretPosition(MWContext* context,
		    LO_Position* where,
		    int32* caretX,
		    int32* caretYLow,
		    int32* caretYHigh)
{
}

void
FE_DestroyCaret(MWContext* pContext)
{
}

void
FE_ShowCaret(MWContext* pContext)
{
}

void
FE_DocumentChanged(MWContext* context,
		   int32 iStartY,
		   int32 iHeight)
{
}

MWContext*
FE_CreateNewEditWindow(MWContext* pContext,
		       URL_Struct* pURL)
{
}

char*
FE_URLToLocalName(char* url)
{
}

void
FE_EditorDocumentLoaded(MWContext* context)
{
}

void
FE_GetDocAndWindowPosition(MWContext * context,
			   int32 *pX,
			   int32 *pY,
			   int32 *pWidth,
			   int32 *pHeight)
{
}

void
FE_SetNewDocumentProperties(MWContext* context)
{
}

Bool
FE_CheckAndSaveDocument(MWContext* context)
{
}

Bool
FE_CheckAndAutoSaveDocument(MWContext *context)
{
}

void 
FE_FinishedSave(MWContext* context,
		int status,
		char *pDestURL,
		int iFileNumber)
{
}

char *
XP_BackupFileName (const char *url)
{
}

Bool
XP_ConvertUrlToLocalFile (const char *url,
			  char **localName)
{
}

void
FE_ImageLoadDialog(MWContext* context)
{
}

void
FE_ImageLoadDialogDestroy(MWContext* context)
{
}

void
FE_DisplayAddRowOrColBorder(MWContext * pMWContext,
			    XP_Rect *pRect,
			    XP_Bool bErase)
{
}

void
FE_DisplayEntireTableOrCell(MWContext * pMWContext,
			    LO_Element * pLoElement)
{
}
