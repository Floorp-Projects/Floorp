/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * A collection of utility methods for use by devtools.
 *
 * See InspectorUtils.h for documentation on these methods.
 */
[ChromeOnly]
namespace InspectorUtils {
  sequence<StyleSheet> getAllStyleSheets(Document document);
  sequence<CSSRule> getCSSStyleRules(
    Element element,
    [TreatNullAs=EmptyString] optional DOMString pseudo = "");
  unsigned long getRuleLine(CSSRule rule);
  unsigned long getRuleColumn(CSSRule rule);
  unsigned long getRelativeRuleLine(CSSRule rule);
  [NewObject] CSSLexer getCSSLexer(DOMString text);
};

dictionary InspectorRGBTriple {
  /*
   * NOTE: Using octet for RGB components is not generally OK, because
   * they can be outside the 0-255 range, but for backwards-compatible
   * named colors (which is what we use this dictionary for) the 0-255
   * assumption is fine.
   */
  octet r = 0;
  octet g = 0;
  octet b = 0;
};

dictionary InspectorRGBATuple {
  /*
   * NOTE: This tuple is in the normal 0-255-sized RGB space but can be
   * fractional and may extend outside the 0-255 range.
   *
   * a is in the range 0 - 1.
   */
  double r = 0;
  double g = 0;
  double b = 0;
  double a = 1;
};
