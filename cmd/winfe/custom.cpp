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
// The main file used to provide customization (locking) of preferences to
// OEM partners and/or MIS directors for the Enterprise market....
//#define WFE_FILE 1

#include "stdafx.h"

#include "custom.h"
#include "resource.h"
#include "mainfrm.h"
#include "xp.h"
#include "sechash.h"
#include "prefapi.h"

BOOL CUST_IsCustomAnimation(int * iFrames)
{
	char *pFile;
	BOOL bRet = FALSE;

	// set iFrames to default

	PREF_CopyConfigString("toolbar.logo.win_small_file",&pFile);
	if (pFile) {
		if (*pFile) bRet = TRUE;
		XP_FREE(pFile);
		pFile = NULL;
	}
	PREF_CopyConfigString("toolbar.logo.win_large_file",&pFile);
	if (pFile) {
		if (*pFile) bRet = TRUE;
		XP_FREE(pFile);
		pFile = NULL;
	}
	if (bRet) {
		int32 iTmp;
		PREF_GetConfigInt("toolbar.logo.frames",&iTmp);
		if (iTmp > 0 && iTmp <1000) // pure arbitrary bounds check
			if (iFrames) *iFrames = iTmp;
	}
	return bRet;
}

void CUST_LoadCustomPaletteEntries() 
{
	int idx;
	char * entry;
	char szBuf[24];
	int red = 0,green = 0,blue = 0;
}
