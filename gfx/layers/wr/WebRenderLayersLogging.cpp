/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderLayersLogging.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

void
AppendToString(std::stringstream& aStream, wr::MixBlendMode aMixBlendMode,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  switch (aMixBlendMode) {
  case wr::MixBlendMode::Normal:
    aStream << "wr::MixBlendMode::Normal"; break;
  case wr::MixBlendMode::Multiply:
    aStream << "wr::MixBlendMode::Multiply"; break;
  case wr::MixBlendMode::Screen:
    aStream << "wr::MixBlendMode::Screen"; break;
  case wr::MixBlendMode::Overlay:
    aStream << "wr::MixBlendMode::Overlay"; break;
  case wr::MixBlendMode::Darken:
    aStream << "wr::MixBlendMode::Darken"; break;
  case wr::MixBlendMode::Lighten:
    aStream << "wr::MixBlendMode::Lighten"; break;
  case wr::MixBlendMode::ColorDodge:
    aStream << "wr::MixBlendMode::ColorDodge"; break;
  case wr::MixBlendMode::ColorBurn:
    aStream << "wr::MixBlendMode::ColorBurn"; break;
  case wr::MixBlendMode::HardLight:
    aStream << "wr::MixBlendMode::HardLight"; break;
  case wr::MixBlendMode::SoftLight:
    aStream << "wr::MixBlendMode::SoftLight"; break;
  case wr::MixBlendMode::Difference:
    aStream << "wr::MixBlendMode::Difference"; break;
  case wr::MixBlendMode::Exclusion:
    aStream << "wr::MixBlendMode::Exclusion"; break;
  case wr::MixBlendMode::Hue:
    aStream << "wr::MixBlendMode::Hue"; break;
  case wr::MixBlendMode::Saturation:
    aStream << "wr::MixBlendMode::Saturation"; break;
  case wr::MixBlendMode::Color:
    aStream << "wr::MixBlendMode::Color"; break;
  case wr::MixBlendMode::Luminosity:
    aStream << "wr::MixBlendMode::Luminosity"; break;
  case wr::MixBlendMode::Sentinel:
    NS_ERROR("unknown mix blend mode");
    aStream << "???";
  }
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, wr::ImageRendering aTextureFilter,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  switch (aTextureFilter) {
  case wr::ImageRendering::Auto:
    aStream << "ImageRendering::Auto"; break;
  case wr::ImageRendering::CrispEdges:
    aStream << "ImageRendering::CrispEdges"; break;
  case wr::ImageRendering::Pixelated:
    aStream << "ImageRendering::Pixelated"; break;
  case wr::ImageRendering::Sentinel:
    NS_ERROR("unknown texture filter");
    aStream << "???";
  }
  aStream << sfx;
}

void
AppendToString(std::stringstream& aStream, wr::LayoutVector2D aVector,
               const char* pfx, const char* sfx)
{
  aStream << pfx;
  aStream << nsPrintfCString("(x=%f, y=%f)", aVector.x, aVector.y).get();
  aStream << sfx;
}

} // namespace layers
} // namespace mozilla
