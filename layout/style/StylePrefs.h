/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A namespace class for style system prefs */

#ifndef mozilla_StylePrefs_h
#define mozilla_StylePrefs_h

namespace mozilla {

struct StylePrefs
{
  static bool sFontDisplayEnabled;
  static bool sOpentypeSVGEnabled;
  static bool sWebkitPrefixedAliasesEnabled;
  static bool sWebkitDevicePixelRatioEnabled;
  static bool sMozGradientsEnabled;
  static bool sControlCharVisibility;
  static bool sFramesTimingFunctionEnabled;
  static bool sUnprefixedFullscreenApiEnabled;
  static bool sVisitedLinksEnabled;
  static bool sMozDocumentEnabledInContent;
  static bool sGridTemplateSubgridValueEnabled;

  static void Init();
};

} // namespace mozilla

#endif // mozilla_StylePrefs_h
