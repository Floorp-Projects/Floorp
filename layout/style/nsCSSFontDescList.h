/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

CSS_FONT_DESC(font-family, Family)
CSS_FONT_DESC(font-style, Style)
CSS_FONT_DESC(font-weight, Weight)
CSS_FONT_DESC(font-stretch, Stretch)
CSS_FONT_DESC(src, Src)
CSS_FONT_DESC(unicode-range, UnicodeRange)

/* Note: the parsing code explicitly also accepts font-feature-settings
         and font-language-override. */
CSS_FONT_DESC(-moz-font-feature-settings, FontFeatureSettings)
CSS_FONT_DESC(-moz-font-language-override, FontLanguageOverride)
