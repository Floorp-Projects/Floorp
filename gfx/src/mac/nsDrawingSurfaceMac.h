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

#ifndef nsDrawingSurfaceMac_h___
#define nsDrawingSurfaceMac_h___

#include "nsIDrawingSurface.h"
#include "nsIDrawingSurfaceMac.h"
#include "nsFontMetricsMac.h"


class GraphicState
{
public:
  GraphicState();
  GraphicState(GraphicState* aGS);
  ~GraphicState();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

	void				Clear();
	void				Init(nsDrawingSurface aSurface);
	void				Init(GrafPtr aPort);
	void				Init(nsIWidget* aWindow);
	void				Duplicate(GraphicState* aGS);	// would you prefer an '=' operator?

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


class nsDrawingSurfaceMac : public nsIDrawingSurface,
                            nsIDrawingSurfaceMac
{
public:
  nsDrawingSurfaceMac();
  ~nsDrawingSurfaceMac();

  NS_DECL_ISUPPORTS

  //nsIDrawingSurface interface
  NS_IMETHOD Lock(PRInt32 aX, PRInt32 aY, PRUint32 aWidth, PRUint32 aHeight,
                  void **aBits, PRInt32 *aStride, PRInt32 *aWidthBytes,
                  PRUint32 aFlags);
  NS_IMETHOD Unlock(void);
  NS_IMETHOD GetDimensions(PRUint32 *aWidth, PRUint32 *aHeight);
  NS_IMETHOD IsOffscreen(PRBool *aOffScreen) {return mIsOffscreen;}
  NS_IMETHOD IsPixelAddressable(PRBool *aAddressable);
  NS_IMETHOD GetPixelFormat(nsPixelFormat *aFormat);

  //nsIDrawingSurfaceMac interface
  NS_IMETHOD Init(nsDrawingSurface aDS);
  NS_IMETHOD Init(GrafPtr aThePort);
  NS_IMETHOD Init(nsIWidget *aTheWidget);
  NS_IMETHOD Init(PRUint32 aDepth,PRUint32 aWidth, PRUint32 aHeight,PRUint32 aFlags);
	NS_IMETHOD GetGrafPtr(GrafPtr	*aTheGrafPtr)   	{*aTheGrafPtr = mPort;return NS_OK;}

  // locals
	GraphicState*	GetGS(void) {return mGS;}

private:
	GrafPtr					mPort;						// the onscreen or offscreen GrafPtr;	
	
  PRUint32      	mWidth;
  PRUint32      	mHeight;
  PRInt32       	mLockOffset;
  PRInt32       	mLockHeight;
  PRUint32      	mLockFlags;
	PRBool					mIsOffscreen;	

	GraphicState*		mGS;						// a graphics state for the surface
};

#endif
