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

#ifndef nsRegionWin_h___
#define nsRegionWin_h___

#include "nsIRegion.h"
#include "windows.h"

class nsRegionWin : public nsIRegion
{
public:
  nsRegionWin();

  NS_DECL_ISUPPORTS

  virtual nsresult Init();

  virtual void SetTo(const nsIRegion &aRegion);
  virtual void SetTo(const nsRect &aRect);
  virtual void Intersect(const nsIRegion &aRegion);
  virtual void Intersect(const nsRect &aRect);
  virtual void Union(const nsIRegion &aRegion);
  virtual void Union(const nsRect &aRect);
  virtual void Subtract(const nsIRegion &aRegion);
  virtual PRBool IsEmpty(void);
  virtual PRBool IsEqual(const nsIRegion &aRegion);
  virtual void GetBoundingBox(nsRect &aRect);
  virtual void Offset(nscoord aXOffset, nscoord aYOffset);
  virtual PRBool ContainsRect(const nsRect &aRect);
  virtual PRBool ForEachRect(nsRectInRegionFunc *func, void *closure);

  //windows specific

  HRGN GetHRGN(void);

private:
  ~nsRegionWin();

  HRGN  mRegion;
};

#endif  // nsRegionWin_h___ 
