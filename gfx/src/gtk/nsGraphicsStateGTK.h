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

#ifndef nsGraphicsStateGTK_h___
#define nsGraphicsStateGTK_h___

#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsTransform2D.h"
#include "nsRegionGTK.h"

class nsGraphicsState
{
public:

  nsTransform2D  *mMatrix;
  nsRegionGTK    *mClipRegion;
  nscolor         mColor;
  nsLineStyle     mLineStyle;
  nsIFontMetrics *mFontMetrics;

  nsGraphicsState *mNext; // link into free list of graphics states.

  friend class nsGraphicsStatePool;

#ifndef USE_GS_POOL
  friend class nsRenderingContextGTK;
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

#endif /* nsGraphicsStateGTK_h___ */
