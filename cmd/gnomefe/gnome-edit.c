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
   gnomeedit.c --- stub functions for fe
                   specific editor stuff.
*/

#include "xp_core.h"
#include "structs.h"
#include "ntypes.h"
#include "edttypes.h"
#include "edt.h"

ED_CharsetEncode FE_EncodingDialog(MWContext *context)
{
  ED_CharsetEncode retval = ED_ENCODE_CANCEL;
  return retval;
}

void
FE_DisplayTextCaret(MWContext* context,
		    int loc,
		    LO_TextStruct* text_data,
		    int char_offset)
{
  printf("FE_DisplayTextCaret (empty)\n");
}

void
FE_DisplayImageCaret(MWContext* context,
		     LO_ImageStruct* pImageData,
		     ED_CaretObjectPosition caretPos)
{
  printf("FE_DisplayImageCaret (empty)\n");
}

void
FE_DisplayGenericCaret(MWContext* context,
		       LO_Any* pLoAny,
		       ED_CaretObjectPosition caretPos)
{
  printf("FE_DisplayGenericCaret (empty)\n");
}

Bool
FE_GetCaretPosition(MWContext* context,
		    LO_Position* where,
		    int32* caretX,
		    int32* caretYLow,
		    int32* caretYHigh)
{
  printf("FE_GetCaretPosition (empty)\n");
}

void
FE_DestroyCaret(MWContext* pContext)
{
  printf("FE_DestroyCaret (empty)\n");
}

void
FE_ShowCaret(MWContext* pContext)
{
  printf("FE_ShowCaret (empty)\n");
}

void
FE_DocumentChanged(MWContext* context,
		   int32 iStartY,
		   int32 iHeight)
{
  printf("FE_DocumentChanged (empty)\n");
}

MWContext*
FE_CreateNewEditWindow(MWContext* pContext,
		       URL_Struct* pURL)
{
  printf("FE_CreateNewEditWindow (empty)\n");
}

char*
FE_URLToLocalName(char* url)
{
  printf("FE_URLToLocalName (empty)\n");
}

void
FE_EditorDocumentLoaded(MWContext* context)
{
  printf("FE_EditorDocumentLoaded (empty)\n");
}

void
FE_GetDocAndWindowPosition(MWContext * context,
			   int32 *pX,
			   int32 *pY,
			   int32 *pWidth,
			   int32 *pHeight)
{
  printf("FE_GetDocAndWindowPosition (empty)\n");
}

void
FE_SetNewDocumentProperties(MWContext* context)
{
  printf("FE_SetNewDocumentProperties (empty)\n");
}

Bool
FE_CheckAndSaveDocument(MWContext* context)
{
  printf("FE_CheckAndSaveDocument (empty)\n");
}

Bool
FE_CheckAndAutoSaveDocument(MWContext *context)
{
  printf("FE_CheckAndAutoSaveDocument (empty)\n");
}

void 
FE_FinishedSave(MWContext* context,
		int status,
		char *pDestURL,
		int iFileNumber)
{
  printf("FE_FinishedSave (empty)\n");
}

char *
XP_BackupFileName (const char *url)
{
  printf("XP_BackupFileName (empty)\n");
}

Bool
XP_ConvertUrlToLocalFile (const char *url,
			  char **localName)
{
  printf("XP_ConvertUrlToLocalFile (empty)\n");
}

void
FE_ImageLoadDialog(MWContext* context)
{
  printf("FE_ImageLoadDialog (empty)\n");
}

void
FE_ImageLoadDialogDestroy(MWContext* context)
{
  printf("FE_ImageLoadDialogDestroy (empty)\n");
}

void
FE_DisplayAddRowOrColBorder(MWContext * pMWContext,
			    XP_Rect *pRect,
			    XP_Bool bErase)
{
  printf("FE_DisplayAddRowOrColBorder (empty)\n");
}

void
FE_DisplayEntireTableOrCell(MWContext * pMWContext,
			    LO_Element * pLoElement)
{
  printf("FE_DisplayEntireTableOrCell (empty)\n");
}
