/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxPrefs.h"

namespace mozilla {
namespace gfx {


extern BYTE sSystemTextQuality;

static BYTE
GetSystemTextQuality()
{
  return sSystemTextQuality;
}

// Cleartype can be dynamically enabled/disabled, so we have to allow for dynamically
// updating it.
static void
UpdateSystemTextQuality()
{
  BOOL font_smoothing;
  UINT smoothing_type;

  if (!SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &font_smoothing, 0)) {
    sSystemTextQuality = DEFAULT_QUALITY;
    return;
  }

  if (font_smoothing) {
      if (!SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE,
                                0, &smoothing_type, 0)) {
        sSystemTextQuality = DEFAULT_QUALITY;
        return;
      }

      if (smoothing_type == FE_FONTSMOOTHINGCLEARTYPE) {
        sSystemTextQuality = CLEARTYPE_QUALITY;
        return;
      }

      sSystemTextQuality = ANTIALIASED_QUALITY;
      return;
  }

  sSystemTextQuality = DEFAULT_QUALITY;
}

static AntialiasMode
GetSystemDefaultAAMode()
{
  AntialiasMode defaultMode = AntialiasMode::SUBPIXEL;
  if (gfxPrefs::DisableAllTextAA()) {
    return AntialiasMode::NONE;
  }

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
