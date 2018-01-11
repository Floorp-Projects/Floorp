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
  unsigned long getSelectorCount(CSSStyleRule rule);
  [Throws] DOMString getSelectorText(CSSStyleRule rule,
                                     unsigned long selectorIndex);
  [Throws] unsigned long long getSpecificity(CSSStyleRule rule,
                                             unsigned long selectorIndex);
  [Throws] boolean selectorMatchesElement(
      Element element,
      CSSStyleRule rule,
      unsigned long selectorIndex,
      [TreatNullAs=EmptyString] optional DOMString pseudo = "");
  boolean isInheritedProperty(DOMString property);
  sequence<DOMString> getCSSPropertyNames(optional PropertyNamesOptions options);
  [Throws] sequence<DOMString> getCSSValuesForProperty(DOMString property);
  [Throws] DOMString rgbToColorName(octet r, octet g, octet b);
  InspectorRGBATuple? colorToRGBA(DOMString colorString);
  boolean isValidCSSColor(DOMString colorString);
  [Throws] sequence<DOMString> getSubpropertiesForCSSProperty(DOMString property);
  [Throws] boolean cssPropertyIsShorthand(DOMString property);

  // TODO: Change this to use an enum.
  const unsigned long TYPE_LENGTH = 0;
  const unsigned long TYPE_PERCENTAGE = 1;
  const unsigned long TYPE_COLOR = 2;
  const unsigned long TYPE_URL = 3;
  const unsigned long TYPE_ANGLE = 4;
  const unsigned long TYPE_FREQUENCY = 5;
  const unsigned long TYPE_TIME = 6;
  const unsigned long TYPE_GRADIENT = 7;
  const unsigned long TYPE_TIMING_FUNCTION = 8;
  const unsigned long TYPE_IMAGE_RECT = 9;
  const unsigned long TYPE_NUMBER = 10;
  [Throws] boolean cssPropertySupportsType(DOMString property, unsigned long type);

  boolean isIgnorableWhitespace(CharacterData dataNode);
  Node? getParentForNode(Node node, boolean showingAnonymousContent);
  [NewObject] NodeList getChildrenForNode(Node node,
                                          boolean showingAnonymousContent);
  sequence<DOMString> getBindingURLs(Element element);
};

dictionary PropertyNamesOptions {
  boolean includeAliases = false;
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
