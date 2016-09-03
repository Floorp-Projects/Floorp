/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

namespace mozilla {
namespace gfx {

// Cleartype can be dynamically enabled/disabled, so we have to check it
// everytime we want to render some text.
static BYTE
GetSystemTextQuality()
{
  BOOL font_smoothing;
  UINT smoothing_type;

  if (!SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &font_smoothing, 0)) {
    return DEFAULT_QUALITY;
  }

  if (font_smoothing) {
      if (!SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE,
                                0, &smoothing_type, 0)) {
        return DEFAULT_QUALITY;
      }

      if (smoothing_type == FE_FONTSMOOTHINGCLEARTYPE) {
        return CLEARTYPE_QUALITY;
      }

      return ANTIALIASED_QUALITY;
  }

  return DEFAULT_QUALITY;
}

static AntialiasMode
GetSystemDefaultAAMode()
{
  AntialiasMode defaultMode = AntialiasMode::SUBPIXEL;

  switch (GetSystemTextQuality()) {
  case CLEARTYPE_QUALITY:
    defaultMode = AntialiasMode::SUBPIXEL;
    break;
  case ANTIALIASED_QUALITY:
    defaultMode = AntialiasMode::GRAY;
    break;
  case DEFAULT_QUALITY:
    defaultMode = AntialiasMode::NONE;
    break;
  }

  return defaultMode;
}

} // namespace gfx
} // namespace mozilla
