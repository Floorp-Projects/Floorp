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

#include "nsIWindow.h"

#include "nsCOMPtr.h"

NS_IMPL_ISUPPORTS_INHERITED1(nsPixmap, nsDrawable, nsIPixmap)

nsPixmap::nsPixmap()
{
  NS_INIT_ISUPPORTS();
}

nsPixmap::~nsPixmap()
{
}

NS_IMETHODIMP nsPixmap::Init(nsIWindow *aParent, gfx_width width, gfx_height height, gfx_depth depth)
{
    mBounds.SizeTo(width, height);
    mDepth = depth;

    Rect			macRect;
    GrafPtr 	savePort;

    mBounds.SizeTo(width, height);
    mDepth = depth;

    ::SetRect(&macRect, 0, 0, width, height);

    // create offscreen, first with normal memory, if that fails use temp memory, if that fails, return
    QDErr osErr = ::NewGWorld(&mGWorld, depth, &macRect, NULL, NULL, NULL);
    if (osErr != noErr)
        osErr = ::NewGWorld(&mGWorld, depth, &macRect, NULL, NULL, useTempMem);
    if (osErr != noErr)
        return NS_ERROR_FAILURE;

    return NS_OK;
}
