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

#ifndef nsGraphicsStateBeOS_h___
#define nsGraphicsStateBeOS_h___

#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsTransform2D.h"
#include "nsRegionBeOS.h"
#include "nsCOMPtr.h"

class nsGraphicsState
{
public:

  nsTransform2D  *mMatrix;
  nsCOMPtr<nsIRegion> mClipRegion;
  nscolor         mColor;
  nsLineStyle     mLineStyle;
  nsIFontMetrics *mFontMetrics;

  nsGraphicsState *mNext; // link into free list of graphics states.

  friend class nsGraphicsStatePool;

#ifndef USE_GS_POOL
  friend class nsRenderingContextBeOS;
#endif

private:
  nsGraphicsState();
  ~nsGraphicsState();
};

class nsGraphicsStatePool
{
public:

  static nsGraphicsState * GetNewGS();
  static void              ReleaseGS(nsGraphicsState* aGS);
  
  nsGraphicsStatePool();
  ~nsGraphicsStatePool();
  
private:
  nsGraphicsState*	mFreeList;
  
  static nsGraphicsStatePool * PrivateGetPool();
  nsGraphicsState *            PrivateGetNewGS();
  void                         PrivateReleaseGS(nsGraphicsState* aGS);
  
  static nsGraphicsStatePool * gsThePool;
};

#endif /* nsGraphicsStateBeOS_h___ */
