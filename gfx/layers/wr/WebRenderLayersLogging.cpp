/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderLayersLogging.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

void
AppendToString(std::stringstream& aStream, WrMixBlendMode aMixBlendMode,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  switch (aMixBlendMode) {
  case WrMixBlendMode::Normal:
    aStream << "WrMixBlendMode::Normal"; break;
  case WrMixBlendMode::Multiply:
    aStream << "WrMixBlendMode::Multiply"; break;
  case WrMixBlendMode::Screen:
    aStream << "WrMixBlendMode::Screen"; break;
  case WrMixBlendMode::Overlay:
    aStream << "WrMixBlendMode::Overlay"; break;
  case WrMixBlendMode::Darken:
    aStream << "WrMixBlendMode::Darken"; break;
  case WrMixBlendMode::Lighten:
    aStream << "WrMixBlendMode::Lighten"; break;
  case WrMixBlendMode::ColorDodge:
    aStream << "WrMixBlendMode::ColorDodge"; break;
  case WrMixBlendMode::ColorBurn:
    aStream << "WrMixBlendMode::ColorBurn"; break;
  case WrMixBlendMode::HardLight:
    aStream << "WrMixBlendMode::HardLight"; break;
  case WrMixBlendMode::SoftLight:
    aStream << "WrMixBlendMode::SoftLight"; break;
  case WrMixBlendMode::Difference:
    aStream << "WrMixBlendMode::Difference"; break;
  case WrMixBlendMode::Exclusion:
    aStream << "WrMixBlendMode::Exclusion"; break;
  case WrMixBlendMode::Hue:
    aStream << "WrMixBlendMode::Hue"; break;
  case WrMixBlendMode::Saturation:
    aStream << "WrMixBlendMode::Saturation"; break;
  case WrMixBlendMode::Color:
    aStream << "WrMixBlendMode::Color"; break;
  case WrMixBlendMode::Luminosity:
    aStream << "WrMixBlendMode::Luminosity"; break;
  case WrMixBlendMode::Sentinel:
    NS_ERROR("unknown mix blend mode");
    aStream << "???";
  }
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, WrTextureFilter aTextureFilter,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  switch (aTextureFilter) {
  case WrTextureFilter::Linear:
    aStream << "WrTextureFilter::Linear"; break;
  case WrTextureFilter::Point:
    aStream << "WrTextureFilter::Point"; break;
  case WrTextureFilter::Sentinel:
    NS_ERROR("unknown texture filter");
    aStream << "???";
  }
  aStream << sfx;
}

} // namespace layers
} // namespace mozilla
