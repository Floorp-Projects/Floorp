/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#ifndef nsGraphicState_h___
#define nsGraphicState_h___

#include "nsIRenderingContext.h"
#include "nsCRT.h"
#include <QuickDraw.h>

//------------------------------------------------------------------------

class nsGraphicState
{
public:
  nsGraphicState();
  ~nsGraphicState();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

	void				Clear();
	void				Init(nsDrawingSurface aSurface);
	void				Init(GrafPtr aPort);
	void				Init(nsIWidget* aWindow);
	void				Duplicate(nsGraphicState* aGS);	// would you prefer an '=' operator? <anonymous>
																						// - no thanks, <pierre>

protected:
	RgnHandle		DuplicateRgn(RgnHandle aRgn);

public:
  nsTransform2D * 			mTMatrix; 					// transform that all the graphics drawn here will obey

	PRInt32               mOffx;
  PRInt32               mOffy;

  RgnHandle							mMainRegion;
  RgnHandle			    		mClipRegion;

  nscolor               mColor;
  PRInt32               mFont;
  nsIFontMetrics * 			mFontMetrics;
	PRInt32               mCurrFontHandle;
};

//------------------------------------------------------------------------

class nsGraphicStatePool
{
public:
	nsGraphicStatePool();
	~nsGraphicStatePool();

	nsGraphicState*		GetNewGS();
	void							ReleaseGS(nsGraphicState* aGS);

private:
	static const short	kGSPoolCount = 80;  // sizeof(nsGraphicState) = 36 bytes

	typedef struct nsGSRec
	{
			nsGraphicState*		mGS;
			PRBool						mFree;
	} nsGSRec;

	nsGSRec						mGSArray[kGSPoolCount];
};

//------------------------------------------------------------------------

extern nsGraphicStatePool sGraphicStatePool;

//------------------------------------------------------------------------


#endif //nsGraphicState_h___
