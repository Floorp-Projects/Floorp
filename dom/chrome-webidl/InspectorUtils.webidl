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
  // documentOnly tells whether user and UA sheets should get included.
  sequence<StyleSheet> getAllStyleSheets(Document document, optional boolean documentOnly = false);
  sequence<CSSRule> getCSSStyleRules(
    Element element,
    [TreatNullAs=EmptyString] optional DOMString pseudo = "");
  unsigned long getRuleLine(CSSRule rule);
  unsigned long getRuleColumn(CSSRule rule);
  unsigned long getRelativeRuleLine(CSSRule rule);
  boolean hasRulesModifiedByCSSOM(CSSStyleSheet sheet);
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
  sequence<PropertyPref> getCSSPropertyPrefs();
  [Throws] sequence<DOMString> getCSSValuesForProperty(DOMString property);
  [Throws] DOMString rgbToColorName(octet r, octet g, octet b);
  InspectorRGBATuple? colorToRGBA(DOMString colorString);
  boolean isValidCSSColor(DOMString colorString);
  [Throws] sequence<DOMString> getSubpropertiesForCSSProperty(DOMString property);
  [Throws] boolean cssPropertyIsShorthand(DOMString property);

  // TODO: Change this to use an enum.
  const unsigned long TYPE_COLOR = 1;
  const unsigned long TYPE_GRADIENT = 2;
  const unsigned long TYPE_TIMING_FUNCTION = 3;
  [Throws] boolean cssPropertySupportsType(DOMString property, unsigned long type);

  boolean isIgnorableWhitespace(CharacterData dataNode);
  Node? getParentForNode(Node node, boolean showingAnonymousContent);
  [NewObject] NodeList getChildrenForNode(Node node,
                                          boolean showingAnonymousContent);
  sequence<DOMString> getBindingURLs(Element element);
  [Throws] boolean setContentState(Element element, unsigned long long state);
  [Throws] boolean removeContentState(
      Element element,
      unsigned long long state,
      optional boolean clearActiveDocument = false);
  unsigned long long getContentState(Element element);

  // Get the font face(s) actually used to render the text in /range/,
  // as a collection of InspectorFontFace objects (below).
  // If /maxRanges/ is greater than zero, each InspectorFontFace will record
  // up to /maxRanges/ fragments of content that used the face, for the caller
  // to access via its .ranges attribute.
  [NewObject, Throws] sequence<InspectorFontFace> getUsedFontFaces(
      Range range,
      optional unsigned long maxRanges = 0,
      optional boolean skipCollapsedWhitespace = true);

  sequence<DOMString> getCSSPseudoElementNames();
  void addPseudoClassLock(Element element,
                          DOMString pseudoClass,
                          optional boolean enabled = true);
  void removePseudoClassLock(Element element, DOMString pseudoClass);
  boolean hasPseudoClassLock(Element element, DOMString pseudoClass);
  void clearPseudoClassLocks(Element element);
  [Throws] void parseStyleSheet(CSSStyleSheet sheet, DOMString input);
  void scrollElementIntoView(Element element);
};

dictionary PropertyNamesOptions {
  boolean includeAliases = false;
  boolean includeShorthands = true;
  boolean includeExperimentals = false;
};

dictionary PropertyPref {
  required DOMString name;
  required DOMString pref;
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

dictionary InspectorVariationAxis {
  required DOMString tag;
  required DOMString name;
  required float minValue;
  required float maxValue;
  required float defaultValue;
};

dictionary InspectorVariationValue {
  required DOMString axis;
  required float value;
};

dictionary InspectorVariationInstance {
  required DOMString name;
  required sequence<InspectorVariationValue> values;
};

dictionary InspectorFontFeature {
  required DOMString tag;
  required DOMString script;
  required DOMString languageSystem;
};

[ChromeOnly]
interface InspectorFontFace {
  // An indication of how we found this font during font-matching.
  // Note that the same physical font may have been found in multiple ways within a range.
  readonly attribute boolean fromFontGroup;
  readonly attribute boolean fromLanguagePrefs;
  readonly attribute boolean fromSystemFallback;

  // available for all fonts
  readonly attribute DOMString name; // full font name as obtained from the font resource
  readonly attribute DOMString CSSFamilyName; // a family name that could be used in CSS font-family
                                              // (not necessarily the actual name that was used,
                                              // due to aliases, generics, localized names, etc)
  readonly attribute DOMString CSSGeneric; // CSS generic (serif, sans-serif, etc) that was mapped
                                           // to this font, if any (frequently empty!)

  [NewObject,Throws] sequence<InspectorVariationAxis> getVariationAxes();
  [NewObject,Throws] sequence<InspectorVariationInstance> getVariationInstances();
  [NewObject,Throws] sequence<InspectorFontFeature> getFeatures();

  // A list of Ranges of text rendered with this face.
  // This will list the first /maxRanges/ ranges found when InspectorUtils.getUsedFontFaces
  // was called (so it will be empty unless a non-zero maxRanges argument was passed).
  // Note that this indicates how the document was rendered at the time of calling
  // getUsedFontFaces; it does not reflect any subsequent modifications, so if styles
  // have been modified since calling getUsedFontFaces, it may no longer be accurate.
  [Constant,Cached]  readonly attribute sequence<Range> ranges;

  // meaningful only when the font is a user font defined using @font-face
  readonly attribute CSSFontFaceRule? rule; // null if no associated @font-face rule
  readonly attribute long srcIndex; // index in the rule's src list, -1 if no @font-face rule
  readonly attribute DOMString URI; // empty string if not a downloaded font, i.e. local
  readonly attribute DOMString localName; // empty string  if not a src:local(...) rule
  readonly attribute DOMString format; // as per http://www.w3.org/TR/css3-webfonts/#referencing
  readonly attribute DOMString metadata; // XML metadata from WOFF file (if any)
};
