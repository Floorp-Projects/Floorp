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

#include "nsPixmap.h"

#include "nsWindow.h"

#include "nsCOMPtr.h"

#include "nsRunAppRun.h"

NS_IMPL_ISUPPORTS_INHERITED1(nsPixmap, nsDrawable, nsIPixmap)

nsPixmap::nsPixmap() :
  mPixmap(0)
{
  NS_INIT_ISUPPORTS();

  mDisplay = nsRunAppRun::sDisplay;
}

nsPixmap::~nsPixmap()
{
  SetDrawable((Drawable)0);

  if (mPixmap)
    ::XFreePixmap(mDisplay, mPixmap);
}

NS_IMETHODIMP nsPixmap::Init(nsIWindow *aParent, gfx_dimension width, gfx_dimension height, gfx_depth depth)
{
  mBounds.SizeTo(width, height);
  mDepth = depth;

  Drawable parent = 0;

  Screen *screen = DefaultScreenOfDisplay(mDisplay);

  if (aParent) {
    parent = NS_STATIC_CAST(nsWindow*, aParent)->mDrawable;
  } else {
    parent = RootWindowOfScreen(screen);
  }

  mPixmap = ::XCreatePixmap(mDisplay, (Window)parent, width, height, depth);

  SetDrawable((Drawable)mPixmap);

  return NS_OK;
}
