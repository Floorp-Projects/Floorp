/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


#ifndef nsGraphicState_h___
#define nsGraphicState_h___

#include "nsIRenderingContext.h"
#include "nsTransform2D.h"
#include "nsCRT.h"

#ifndef __QUICKDRAW__
#include <Quickdraw.h>
#endif

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

	void				SetChanges(PRUint32 aChanges) { mChanges = aChanges; }
	PRUint32		GetChanges() { return mChanges; }	

protected:
	RgnHandle		DuplicateRgn(RgnHandle aRgn, RgnHandle aDestRgn = nsnull);

public:
	nsTransform2D 				mTMatrix; 						// transform that all the graphics drawn here will obey

	PRInt32               mOffx;
  PRInt32               mOffy;

  RgnHandle							mMainRegion;
  RgnHandle			    		mClipRegion;

  nscolor               mColor;
  PRInt32               mFont;
  nsIFontMetrics * 			mFontMetrics;
	PRInt32               mCurrFontHandle;
	
	PRUint32							mChanges;							// flags indicating changes between this graphics state and the previous.

private:
	friend class nsGraphicStatePool;
	nsGraphicState*				mNext;								// link into free list of graphics states.
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
#if 0
	static const short	kGSPoolCount = 80;  // sizeof(nsGraphicState) = 36 bytes

	typedef struct nsGSRec
	{
			nsGraphicState*		mGS;
			PRBool						mFree;
	} nsGSRec;

	nsGSRec						mGSArray[kGSPoolCount];
#endif

	nsGraphicState*	mFreeList;
};

//------------------------------------------------------------------------

extern nsGraphicStatePool sGraphicStatePool;

//------------------------------------------------------------------------


#endif //nsGraphicState_h___
