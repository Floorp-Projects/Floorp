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
#include "nsPoint2.h"
#include "nsRect2.h"

class nsGCCache;
class xGC;

#include <X11/Xlib.h>

class nsDrawable : public nsIDrawable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDRAWABLE

  friend class nsPixmap;

  nsDrawable();
  virtual ~nsDrawable();

private:
  /* helper functions */
  void UpdateGC();


protected:
  inline void SetDrawable(Drawable aDrawable) { mDrawable = aDrawable; }

  /* protected members variables */
  Display  *mDisplay;
  xGC      *mGC;

  nsRect2 mBounds;

  gfx_depth mDepth;


private:
  static nsGCCache *sGCCache;

  /* private members variables */
  Drawable  mDrawable;

  /* GC related stuff */
  gfx_coord mLineWidth;

  gfx_color mForegroundColor;
  gfx_color mBackgroundColor;

  nsPoint2 mClipOrigin;
  nsCOMPtr<nsIRegion> mClipRegion;

  nsCOMPtr<nsIFontMetrics> mFontMetrics;

};

#endif  // nsDrawable_h___
