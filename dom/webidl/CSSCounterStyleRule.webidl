/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://drafts.csswg.org/css-counter-styles-3/#the-csscounterstylerule-interface
 */

// https://drafts.csswg.org/css-counter-styles-3/#the-csscounterstylerule-interface
[Exposed=Window]
interface CSSCounterStyleRule : CSSRule {
  attribute DOMString name;
  attribute UTF8String system;
  attribute UTF8String symbols;
  attribute UTF8String additiveSymbols;
  attribute UTF8String negative;
  attribute UTF8String prefix;
  attribute UTF8String suffix;
  attribute UTF8String range;
  attribute UTF8String pad;
  attribute UTF8String speakAs;
  attribute UTF8String fallback;
};
