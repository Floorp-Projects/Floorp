/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#ifndef nsDrawable_h___
#define nsDrawable_h___

#include "nsIDrawable.h"

#include "nsCOMPtr.h"
#include "nsIFontMetrics.h"
#include "nsIRegion.h"
#include "nsPoint.h"
#include "nsRect.h"

#ifndef __QUICKDRAW__
#include <Quickdraw.h>
#endif

class nsDrawable : public nsIDrawable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDRAWABLE

  nsDrawable();
  virtual ~nsDrawable();

protected:
  inline void SetDrawable(GrafPtr aDrawable) { mGrafPtr = aDrawable; }

  /* protected members variables */
  nsRect mBounds;

  gfx_depth mDepth;

private:
  /* private members variables */
  GrafPtr mGrafPtr;

  /* GC related stuff */
  gfx_coord mLineWidth;

  gfx_color mForegroundColor;
  gfx_color mBackgroundColor;

  nsPoint mClipOrigin;
  nsCOMPtr<nsIRegion> mClipRegion;

  nsCOMPtr<nsIFontMetrics> mFontMetrics;

};

#endif  // nsDrawable_h___
