/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsViewportInfo.h"

void
nsViewportInfo::SetDefaultZoom(const double aDefaultZoom)
{
  MOZ_ASSERT(aDefaultZoom >= 0.0f);
  mDefaultZoom = aDefaultZoom;
}

void
nsViewportInfo::ConstrainViewportValues()
{
  // Constrain the min/max zoom as specified at:
  // dev.w3.org/csswg/css-device-adapt section 6.2
  mMaxZoom = NS_MAX(mMinZoom, mMaxZoom);

  mDefaultZoom = NS_MIN(mDefaultZoom, mMaxZoom);
  mDefaultZoom = NS_MAX(mDefaultZoom, mMinZoom);
}
