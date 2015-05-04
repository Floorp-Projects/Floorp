/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsViewportInfo.h"
#include "mozilla/Assertions.h"
#include <algorithm>

using namespace mozilla;

void
nsViewportInfo::SetDefaultZoom(const CSSToScreenScale& aDefaultZoom)
{
  MOZ_ASSERT(aDefaultZoom.scale >= 0.0f);
  mDefaultZoom = aDefaultZoom;
}

void
nsViewportInfo::ConstrainViewportValues()
{
  // Constrain the min/max zoom as specified at:
  // dev.w3.org/csswg/css-device-adapt section 6.2
  mMaxZoom = std::max(mMinZoom, mMaxZoom);

  mDefaultZoom = mDefaultZoom < mMaxZoom ? mDefaultZoom : mMaxZoom;
  mDefaultZoom = mDefaultZoom > mMinZoom ? mDefaultZoom : mMinZoom;
}
