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

// ===========================================================================
// Mark G. Young.
// ===========================================================================

#include "CViewUtils.h"

#include <UException.h>

// ---------------------------------------------------------------------------
//		¥ GetTopMostSuperView
// ---------------------------------------------------------------------------

LView*
CViewUtils::GetTopMostSuperView(
	LPane*			inPane)
{
	ThrowIfNil_(inPane);

	LView* superView = inPane->GetSuperView();
	
	while (superView && superView->GetSuperView())
	{
		superView = superView->GetSuperView();
	}
	
	return superView;
}

// ---------------------------------------------------------------------------
//		¥ SetRect32
// ---------------------------------------------------------------------------

void
CViewUtils::SetRect32(
	SRect32*		outRect,
	Int32			inLeft,
	Int32			inTop,
	Int32			inRight,
	Int32			inBottom)
{
	outRect->left	= inLeft;
	outRect->right	= inRight;
	outRect->top	= inTop;
	outRect->bottom	= inBottom;
}

// ---------------------------------------------------------------------------
//		¥ ImageToLocalRect
// ---------------------------------------------------------------------------

void
CViewUtils::ImageToLocalRect(
	LView*			inView,
	const SRect32&	inRect,
	Rect*			outRect)
{
	SPoint32			theImagePoint;
	Point				theLocalPoint;
	
	theImagePoint.h = inRect.left;
	theImagePoint.v = inRect.top;
	inView->ImageToLocalPoint(theImagePoint, theLocalPoint);
	outRect->left = theLocalPoint.h;
	outRect->top = theLocalPoint.v;
	
	theImagePoint.h = inRect.right;
	theImagePoint.v = inRect.bottom;
	inView->ImageToLocalPoint(theImagePoint, theLocalPoint);
	outRect->right = theLocalPoint.h;
	outRect->bottom = theLocalPoint.v;
}


// ---------------------------------------------------------------------------
//		¥ LocalToImageRect
// ---------------------------------------------------------------------------

void
CViewUtils::LocalToImageRect(
	LView*			inView,
	const Rect&		inRect,
	SRect32*		outRect)
{
	SPoint32		theImagePoint;
	Point			theLocalPoint;
	
	theLocalPoint.h	= inRect.left;
	theLocalPoint.v	= inRect.top;
	inView->LocalToImagePoint(theLocalPoint, theImagePoint);
	outRect->left	= theImagePoint.h;
	outRect->top	= theImagePoint.v;
	
	theLocalPoint.h	= inRect.right;
	theLocalPoint.v	= inRect.bottom;
	inView->LocalToImagePoint(theLocalPoint, theImagePoint);
	outRect->right	= theImagePoint.h;
	outRect->bottom	= theImagePoint.v;
}