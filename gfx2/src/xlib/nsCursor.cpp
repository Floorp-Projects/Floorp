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

#include "nsCursor.h"

#include <X11/cursorfont.h>

NS_IMPL_ISUPPORTS1(nsCursor, nsICursor)

#include "nsRunAppRun.h"

nsCursor::nsCursor() :
  mDisplay(nsnull),
  mCursor(0)
{
  NS_INIT_ISUPPORTS();
  mDisplay = nsRunAppRun::sDisplay;
}

nsCursor::~nsCursor()
{
  if (mCursor)
    ::XFreeCursor(mDisplay, mCursor);
}

NS_IMETHODIMP nsCursor::GetCursor(PRInt32 *aCursor)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsCursor::SetCursor(PRInt32 aCursor)
{
  unsigned int cursor = 0;

  switch(aCursor) {
    case nsICursor::select:
      cursor = XC_xterm;
      break;

    case nsICursor::wait:
      cursor = XC_watch;
      break;

    case nsICursor::hyperlink:
      cursor = XC_hand2;
      break;

    case nsICursor::standard:
      cursor = XC_left_ptr;
      break;

    case nsICursor::sizeWE:
    case nsICursor::sizeNS:
      cursor = XC_tcross;
      break;

    case nsICursor::arrow_south:
    case nsICursor::arrow_south_plus:
      cursor = XC_bottom_side;
      break;

    case nsICursor::arrow_north:
    case nsICursor::arrow_north_plus:
      cursor = XC_top_side;
      break;

    case nsICursor::arrow_east:
    case nsICursor::arrow_east_plus:
      cursor = XC_right_side;
      break;

    case nsICursor::arrow_west:
    case nsICursor::arrow_west_plus:
      cursor = XC_left_side;
      break;

    case nsICursor::move:
      cursor = XC_dotbox;
      break;

    default:
      cursor = XC_left_ptr;
      break;
  }

  mCursor = ::XCreateFontCursor(mDisplay, cursor);

  return NS_OK;
}

NS_IMETHODIMP nsCursor::SetToImage(nsIImage *aImage)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
