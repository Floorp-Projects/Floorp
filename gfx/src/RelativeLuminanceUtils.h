/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RelativeLuminanceUtils_h
#define mozilla_RelativeLuminanceUtils_h

#include "nsColor.h"

namespace mozilla {

// Utilities for calculating relative luminance based on the algorithm
// defined in https://www.w3.org/TR/WCAG20/#relativeluminancedef
class RelativeLuminanceUtils
{
public:
  // Compute the relative luminance.
  static float Compute(nscolor aColor)
  {
    float r = ComputeComponent(NS_GET_R(aColor));
    float g = ComputeComponent(NS_GET_G(aColor));
    float b = ComputeComponent(NS_GET_B(aColor));
    return ComputeFromComponents(r, g, b);
  }

  // Adjust the relative luminance of the given color.
  static nscolor Adjust(nscolor aColor, float aLuminance)
  {
    float r = ComputeComponent(NS_GET_R(aColor));
    float g = ComputeComponent(NS_GET_G(aColor));
    float b = ComputeComponent(NS_GET_B(aColor));
    float luminance = ComputeFromComponents(r, g, b);
    float factor = aLuminance / luminance;
    uint8_t r1 = DecomputeComponent(r * factor);
    uint8_t g1 = DecomputeComponent(g * factor);
    uint8_t b1 = DecomputeComponent(b * factor);
    return NS_RGB(r1, g1, b1);
  }

private:
  static float ComputeComponent(uint8_t aComponent)
  {
    float v = float(aComponent) / 255.0f;
    if (v <= 0.03928f) {
      return v / 12.92f;
    }
    return std::pow((v + 0.055f) / 1.055f, 2.4f);
  }

  static constexpr float ComputeFromComponents(float aR, float aG, float aB)
  {
    return 0.2126f * aR + 0.7152f * aG + 0.0722f * aB;
  }

  // Inverse function of ComputeComponent.
  static uint8_t DecomputeComponent(float aComponent)
  {
    if (aComponent <= 0.03928f / 12.92f) {
      aComponent *= 12.92f;
    } else {
      aComponent = std::pow(aComponent, 1.0f / 2.4f) * 1.055f - 0.055f;
    }
    return ClampColor(aComponent * 255.0f);
  }
};

} // namespace mozilla

#endif // mozilla_RelativeLuminanceUtils_h
