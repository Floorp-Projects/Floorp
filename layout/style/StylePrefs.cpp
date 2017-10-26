/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/StylePrefs.h"

#include "mozilla/Preferences.h"

namespace mozilla {

bool StylePrefs::sFontDisplayEnabled;
bool StylePrefs::sOpentypeSVGEnabled;
bool StylePrefs::sWebkitPrefixedAliasesEnabled;
bool StylePrefs::sWebkitDevicePixelRatioEnabled;
bool StylePrefs::sMozGradientsEnabled;
bool StylePrefs::sControlCharVisibility;
bool StylePrefs::sFramesTimingFunctionEnabled;
bool StylePrefs::sUnprefixedFullscreenApiEnabled;
bool StylePrefs::sVisitedLinksEnabled;
bool StylePrefs::sMozDocumentEnabledInContent;
bool StylePrefs::sGridTemplateSubgridValueEnabled;
bool StylePrefs::sEmulateMozBoxWithFlex;

/* static */ void
StylePrefs::Init()
{
  Preferences::AddBoolVarCache(&sFontDisplayEnabled,
                               "layout.css.font-display.enabled");
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
  Preferences::AddBoolVarCache(&sUnprefixedFullscreenApiEnabled,
                               "full-screen-api.unprefix.enabled");
  Preferences::AddBoolVarCache(&sVisitedLinksEnabled,
                               "layout.css.visited_links_enabled");
  Preferences::AddBoolVarCache(&sMozDocumentEnabledInContent,
                               "layout.css.moz-document.content.enabled");
  Preferences::AddBoolVarCache(&sGridTemplateSubgridValueEnabled,
                               "layout.css.grid-template-subgrid-value.enabled");

  // Only honor layout.css.emulate-moz-box-with-flex in prerelease builds.
  // (In release builds, sEmulateMozBoxWithFlex will be implicitly false.)
#ifndef RELEASE_OR_BETA
  Preferences::AddBoolVarCache(&sEmulateMozBoxWithFlex,
                               "layout.css.emulate-moz-box-with-flex");
#endif
}

} // namespace mozilla
