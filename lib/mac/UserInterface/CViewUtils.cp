/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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