/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* COMPSTD.H - standard Compose window header file contains standard
 * definitions which are used throughout all comp*.* files.
 */

#ifndef __COMPSTD_H
#define __COMPSTD_H

#include "msgcom.h"

#define IDX_COMPOSEADDRESS      0       // tab indexes for attach/address
#define IDX_COMPOSEATTACH       1
#define IDX_COMPOSEOPTIONS      2
#define IDX_COMPOSEATTACHFILE   3
#define IDX_COMPOSEATTACHMAIL   4

#define IDX_TOOL_ADDRESS        0
#define IDX_TOOL_ATTACH         1
#define IDX_TOOL_OPTIONS        2
#define IDX_TOOL_COLLAPSE       3

#define EDIT_FIELD_OFFSET       4       // pixel border surrounding fields
#define EDIT_MARGIN_OFFSET      4
#define EDIT_TOP_MARGIN         4

#define IDC_ADDRESSADD          150
#define IDC_ADDRESSREMOVE       151
#define IDC_ADDRESSSELECT       152
#define IDC_DIRECTORY           153

#define IDC_ADDRESSTAB          155
#define IDC_ATTACHTAB           156
#define IDC_OPTIONSTAB          157

#define	TABCTRL_HOME			      -2
#define TABCTRL_END				      -1

#define IDC_COMPOSEEDITOR       338     // control id for editor

#define FLIPPY_WIDTH		        16      // width and height of the expansion bitmap
#define FLIPPY_HEIGHT		        16

extern char * EDT_NEW_DOC_URL;          // this is for the Gold editor.  Loading this
                                        // URL causes the creation of a new (blank)
                                        // editor file.

class CNSAttachDropTarget : public COleDropTarget
{
public:
//Construction
    CNSAttachDropTarget() { };
    BOOL        OnDrop(CWnd *, COleDataObject *, DROPEFFECT, CPoint);
    DROPEFFECT  OnDragEnter(CWnd *, COleDataObject *, DWORD, CPoint);
    DROPEFFECT  OnDragOver(CWnd *, COleDataObject *, DWORD, CPoint);
    void        OnDragLeave(CWnd *);
};

extern "C" int FE_LoadExchangeInfo(MWContext * context);

// temporary functions to allow the regular text editor to be functional
extern "C" int FE_GetMessageBody(
    MSG_Pane *pPane, 
    char **ppBody, 
    uint32 *ulBodySize, 
    MSG_FontCode **ppFontChanges);
    
extern "C" void FE_InsertMessageCompositionText(
    MWContext * context,
	const char* text,
	XP_Bool leaveCursorAtBeginning);

////////////////////////////////////////////////////////////
// backend candidates

MSG_HEADER_SET MSG_StringToHeaderMask(char * string);
const char * MSG_HeaderMaskToString(MSG_HEADER_SET header);

////////////////////////////////////////////////////////////
// end backend candidates

#endif
