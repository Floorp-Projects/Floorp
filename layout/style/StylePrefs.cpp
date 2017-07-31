/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/StylePrefs.h"

#include "mozilla/Preferences.h"

namespace mozilla {

bool StylePrefs::sOpentypeSVGEnabled;
bool StylePrefs::sWebkitPrefixedAliasesEnabled;
bool StylePrefs::sWebkitDevicePixelRatioEnabled;
bool StylePrefs::sMozGradientsEnabled;
bool StylePrefs::sControlCharVisibility;
bool StylePrefs::sFramesTimingFunctionEnabled;

/* static */ void
StylePrefs::Init()
{
  Preferences::AddBoolVarCache(&sOpentypeSVGEnabled,
                               "gfx.font_rendering.opentype_svg.enabled");
  Preferences::AddBoolVarCache(&sWebkitPrefixedAliasesEnabled,
                               "layout.css.prefixes.webkit");
  Preferences::AddBoolVarCache(&sWebkitDevicePixelRatioEnabled,
                               "layout.css.prefixes.device-pixel-ratio-webkit");
  Preferences::AddBoolVarCache(&sMozGradientsEnabled,
                               "layout.css.prefixes.gradients");
  Preferences::AddBoolVarCache(&sControlCharVisibility,
                               "layout.css.control-characters.visible");
  Preferences::AddBoolVarCache(&sFramesTimingFunctionEnabled,
                               "layout.css.frames-timing.enabled");
}

} // namespace mozilla
