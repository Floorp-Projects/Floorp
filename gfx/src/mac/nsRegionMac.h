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

#ifndef nsRegionMac_h___
#define nsRegionMac_h___

#include "nsIRegion.h"
#include <quickdraw.h>

//------------------------------------------------------------------------


class NS_EXPORT nsNativeRegionPool
{
public:
	nsNativeRegionPool();
	~nsNativeRegionPool();

	RgnHandle					GetNewRegion();
	void							ReleaseRegion(RgnHandle aRgnHandle);

private:
	struct nsRegionSlot {
		RgnHandle				mRegion;
		nsRegionSlot*		mNext;
	};

	nsRegionSlot*			mRegionSlots;
	nsRegionSlot*			mEmptySlots;
};

//------------------------------------------------------------------------

extern nsNativeRegionPool sNativeRegionPool;

//------------------------------------------------------------------------

class nsRegionMac : public nsIRegion
{
public:
  nsRegionMac();
  ~nsRegionMac();

  NS_DECL_ISUPPORTS

  virtual nsresult Init();

  virtual void SetTo(const nsIRegion &aRegion);
  virtual void SetTo(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  virtual void Intersect(const nsIRegion &aRegion);
  virtual void Intersect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  virtual void Union(const nsIRegion &aRegion);
  virtual void Union(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  virtual void Subtract(const nsIRegion &aRegion);
  virtual void Subtract(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  virtual PRBool IsEmpty(void);
  virtual PRBool IsEqual(const nsIRegion &aRegion);
  virtual void GetBoundingBox(PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight);
  virtual void Offset(PRInt32 aXOffset, PRInt32 aYOffset);
  virtual PRBool ContainsRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  NS_IMETHOD GetRects(nsRegionRectSet **aRects);
  NS_IMETHOD FreeRects(nsRegionRectSet *aRects);
  NS_IMETHOD GetNativeRegion(void *&aRegion) const;
  virtual nsresult SetNativeRegion(void *aRegion);
  NS_IMETHOD GetRegionComplexity(nsRegionComplexity &aComplexity) const;

private:
	RgnHandle			      mRegion;
  nsRegionComplexity  mRegionType;

private:
  virtual void SetRegionType();
  virtual void SetRegionEmpty();
  virtual RgnHandle CreateRectRegion(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);

};

#endif  // nsRegionMac_h___ 
