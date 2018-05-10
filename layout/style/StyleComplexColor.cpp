/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/StyleComplexColor.h"

#include "mozilla/ComputedStyle.h"
#include "mozilla/ComputedStyleInlines.h"
#include "nsIFrame.h"
#include "nsStyleStruct.h"

using namespace mozilla;

static uint32_t
BlendColorComponent(uint32_t aBg, uint32_t aFg, uint32_t aFgAlpha)
{
  return RoundingDivideBy255(aBg * (255 - aFgAlpha) + aFg * aFgAlpha);
}

// Blend one RGBA color with another based on a given ratio.
// It is a linear interpolation on each channel with alpha premultipled.
static nscolor
LinearBlendColors(nscolor aBg, nscolor aFg, uint_fast8_t aFgRatio)
{
  // Common case that either pure background or pure foreground
  if (aFgRatio == 0) {
    return aBg;
  }
  if (aFgRatio == 255) {
    return aFg;
  }
  // Common case that alpha channel is equal (usually both are opaque)
  if (NS_GET_A(aBg) == NS_GET_A(aFg)) {
    auto r = BlendColorComponent(NS_GET_R(aBg), NS_GET_R(aFg), aFgRatio);
    auto g = BlendColorComponent(NS_GET_G(aBg), NS_GET_G(aFg), aFgRatio);
    auto b = BlendColorComponent(NS_GET_B(aBg), NS_GET_B(aFg), aFgRatio);
    return NS_RGBA(r, g, b, NS_GET_A(aFg));
  }

  constexpr float kFactor = 1.0f / 255.0f;

  float p1 = kFactor * (255 - aFgRatio);
  float a1 = kFactor * NS_GET_A(aBg);
  float r1 = a1 * NS_GET_R(aBg);
  float g1 = a1 * NS_GET_G(aBg);
  float b1 = a1 * NS_GET_B(aBg);

  float p2 = 1.0f - p1;
  float a2 = kFactor * NS_GET_A(aFg);
  float r2 = a2 * NS_GET_R(aFg);
  float g2 = a2 * NS_GET_G(aFg);
  float b2 = a2 * NS_GET_B(aFg);

  float a = p1 * a1 + p2 * a2;
  if (a == 0.0) {
    return NS_RGBA(0, 0, 0, 0);
  }

  auto r = ClampColor((p1 * r1 + p2 * r2) / a);
  auto g = ClampColor((p1 * g1 + p2 * g2) / a);
  auto b = ClampColor((p1 * b1 + p2 * b2) / a);
  return NS_RGBA(r, g, b, NSToIntRound(a * 255));
}

nscolor
StyleComplexColor::CalcColor(mozilla::ComputedStyle* aStyle) const {
  MOZ_ASSERT(aStyle);
  auto foregroundColor = aStyle->StyleColor()->mColor;
  return LinearBlendColors(mColor, foregroundColor, mForegroundRatio);
}

nscolor
StyleComplexColor::CalcColor(const nsIFrame* aFrame) const {
  return CalcColor(aFrame->Style());
}
