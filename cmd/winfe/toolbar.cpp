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

//	This file is not compiled directly in the project,
//		but is included by other source files requiring to
//		to have the same common toolbar (OLE ipframe.cpp and
//		SDI mainfrm.cpp).

/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "rosetta.h"

// arrays of IDs used to initialize control bars

// This must correspond to position in toolbar
#define EDIT_BUTTON_OFFSET  3

// toolbar buttons - IDs are command buttons
static UINT BASED_CODE buttons[] =
{
    // same order as in the bitmap 'toolbar.bmp'
    ID_NAVIGATE_BACK,
    ID_NAVIGATE_FORWARD,
    ID_NAVIGATE_RELOAD,
    ID_GO_HOME,
	ID_NETSEARCH,
	ID_PLACES,
    ID_VIEW_LOADIMAGES,
    ID_FILE_PRINT,
	HG28118
    ID_NAVIGATE_INTERRUPT,
	ID_HOTLIST_MCOM
};

