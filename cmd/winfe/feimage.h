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

#ifndef _FEIMAGE_H_
#define _FEIMAGE_H_
#define OLDIMAGE 1
#include "ni_pixmp.h"
#include "il_types.h"
#include "mimgcb.h"

struct FEBitmapInfo {
	int width;
	int height;
	int targetWidth;
	int targetHeight;
	BOOL iconDisplayed;
	BOOL imageDisplayed;
	HBITMAP hBitmap;
	BITMAPINFO *bmpInfo;
	CAbstractCX *pContext;
	BOOL	IsMask;
	BOOL	IsTile; // True if we are currently tiling this image.
	FEBitmapInfo(){
		width = height = targetWidth = targetHeight = 0;
		iconDisplayed = 0;
		hBitmap = NULL;
		bmpInfo = NULL;
		pContext = NULL;
		IsMask = FALSE;
		IsTile = FALSE;
		imageDisplayed = FALSE;
	}
	~FEBitmapInfo() {
		if (bmpInfo)
			XP_FREE( bmpInfo);
		if (hBitmap) 
			::DeleteObject(hBitmap);
	}
};

XP_BEGIN_PROTOS
void	ImageGroupObserver(XP_Observable observable,
			XP_ObservableMsg message,
			void *message_data,
			void *closure);
XP_END_PROTOS
#endif
