/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
	void				Init(nsIDrawingSurface* aSurface);
	void				Init(CGrafPtr aPort);
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

	nsGraphicState*	mFreeList;
};

//------------------------------------------------------------------------

extern nsGraphicStatePool sGraphicStatePool;

//------------------------------------------------------------------------


#endif //nsGraphicState_h___
