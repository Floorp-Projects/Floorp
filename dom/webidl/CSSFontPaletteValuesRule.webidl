/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://drafts.csswg.org/css-fonts/#om-fontpalettevalues
 */

[Exposed=Window, Pref="layout.css.font-palette.enabled"]
interface CSSFontPaletteValuesRule : CSSRule {
  readonly attribute UTF8String name;
  readonly attribute UTF8String fontFamily;
  readonly attribute UTF8String basePalette;
  readonly attribute UTF8String overrideColors;
};
