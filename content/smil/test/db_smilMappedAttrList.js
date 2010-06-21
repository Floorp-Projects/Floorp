/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SMIL Test Code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Holbert <dholbert@mozilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* List of SVG presentational attributes in the SVG 1.1 spec, for use in
   mochitests. (These are the attributes that are mapped to CSS properties) */

var gMappedAttrList =
{
  // NOTE: The list here should match the MappedAttributeEntry arrays in
  // nsSVGElement.cpp

  // PresentationAttributes-FillStroke
  fill:              new AdditiveAttribute("fill", "XML", "rect"),
  fill_opacity:      new AdditiveAttribute("fill-opacity", "XML", "rect"),
  fill_rule:         new NonAdditiveAttribute("fill-rule", "XML", "rect"),
  stroke:            new AdditiveAttribute("stroke", "XML", "rect"),
  stroke_dasharray:
    new NonAdditiveAttribute("stroke-dasharray", "XML", "rect"),
  stroke_dashoffset: new AdditiveAttribute("stroke-dashoffset", "XML", "rect"),
  stroke_linecap:    new NonAdditiveAttribute("stroke-linecap", "XML", "rect"),
  stroke_linejoin:   new NonAdditiveAttribute("stroke-linejoin", "XML", "rect"),
  stroke_miterlimit: new AdditiveAttribute("stroke-miterlimit", "XML", "rect"),
  stroke_opacity:    new AdditiveAttribute("stroke-opacity", "XML", "rect"),
  stroke_width:      new AdditiveAttribute("stroke-width", "XML", "rect"),

  // PresentationAttributes-Graphics
  clip_path:         new NonAdditiveAttribute("clip-path", "XML", "rect"),
  clip_rule:         new NonAdditiveAttribute("clip-rule", "XML", "circle"),
  color_interpolation:
    new NonAdditiveAttribute("color-interpolation", "XML", "rect"),
  cursor:            new NonAdditiveAttribute("cursor", "XML", "rect"),
  display:           new NonAdditiveAttribute("display", "XML", "rect"),
  filter:            new NonAdditiveAttribute("filter", "XML", "rect"),
  image_rendering:
    NonAdditiveAttribute("image-rendering", "XML", "image"),
  mask:              new NonAdditiveAttribute("mask", "XML", "line"),
  pointer_events:    new NonAdditiveAttribute("pointer-events", "XML", "rect"),
  shape_rendering:   new NonAdditiveAttribute("shape-rendering", "XML", "rect"),
  text_rendering:    new NonAdditiveAttribute("text-rendering", "XML", "text"),
  visibility:        new NonAdditiveAttribute("visibility", "XML", "rect"),

  // PresentationAttributes-TextContentElements
  // SKIP 'alignment-baseline' property: animatable but not supported by Mozilla
  // SKIP 'baseline-shift' property: animatable but not supported by Mozilla
  direction:         new NonAnimatableAttribute("direction", "XML", "text"),
  dominant_baseline:
    new NonAdditiveAttribute("dominant-baseline", "XML", "text"),
  glyph_orientation_horizontal:
    // NOTE: Not supported by Mozilla, but explicitly non-animatable
    NonAnimatableAttribute("glyph-orientation-horizontal", "XML", "text"),
  glyph_orientation_vertical:
    // NOTE: Not supported by Mozilla, but explicitly non-animatable
    NonAnimatableAttribute("glyph-orientation-horizontal", "XML", "text"),
  // SKIP 'kerning' property: animatable but not supported by Mozilla
  letter_spacing:    new AdditiveAttribute("letter-spacing", "XML", "text"),
  text_anchor:       new NonAdditiveAttribute("text-anchor", "XML", "text"),
  text_decoration:   new NonAdditiveAttribute("text-decoration", "XML", "text"),
  unicode_bidi:      new NonAnimatableAttribute("unicode-bidi", "XML", "text"),
  word_spacing:      new AdditiveAttribute("word-spacing", "XML", "text"),

  // PresentationAttributes-FontSpecification
  font_family:       new NonAdditiveAttribute("font-family", "XML", "text"),
  font_size:         new AdditiveAttribute("font-size", "XML", "text"),
  font_size_adjust:
    new NonAdditiveAttribute("font-size-adjust", "XML", "text"),
  font_stretch:      new NonAdditiveAttribute("font-stretch", "XML", "text"),
  font_style:        new NonAdditiveAttribute("font-style", "XML", "text"),
  font_variant:      new NonAdditiveAttribute("font-variant", "XML", "text"),
  font_weight:       new NonAdditiveAttribute("font-weight", "XML", "text"),

  // PresentationAttributes-GradientStop
  stop_color:        new AdditiveAttribute("stop-color", "XML", "stop"),
  stop_opacity:      new AdditiveAttribute("stop-opacity", "XML", "stop"),

  // PresentationAttributes-Viewports
  overflow:          new NonAdditiveAttribute("overflow", "XML", "marker"),
  clip:              new AdditiveAttribute("clip", "XML", "marker"),

  // PresentationAttributes-Makers
  marker_end:        new NonAdditiveAttribute("marker-end", "XML", "line"),
  marker_mid:        new NonAdditiveAttribute("marker-mid", "XML", "line"),
  marker_start:      new NonAdditiveAttribute("marker-start", "XML", "line"),

  // PresentationAttributes-Color
  color:             new AdditiveAttribute("color", "XML", "rect"),

  // PresentationAttributes-Filters
  color_interpolation_filters:
    new NonAdditiveAttribute("color-interpolation-filters", "XML",
                             "feFlood"),

  // PresentationAttributes-feFlood
  flood_color:       new AdditiveAttribute("flood-color", "XML", "feFlood"),
  flood_opacity:     new AdditiveAttribute("flood-opacity", "XML", "feFlood"),

  // PresentationAttributes-LightingEffects
  lighting_color:
    new AdditiveAttribute("lighting-color", "XML", "feDiffuseLighting"),
};

// Utility method to copy a list of TestcaseBundle objects for CSS properties
// into a list of TestcaseBundles for the corresponding mapped attributes.
function convertCSSBundlesToMappedAttr(bundleList) {
  // Create mapping of property names to the corresponding
  // mapped-attribute object in gMappedAttrList.
  var propertyNameToMappedAttr = {};
  for (attributeLabel in gMappedAttrList) {
    var propName = gMappedAttrList[attributeLabel].attrName;
    propertyNameToMappedAttr[propName] = gMappedAttrList[attributeLabel];
  }

  var convertedBundles = [];
  for (var bundleIdx in bundleList) {
    var origBundle = bundleList[bundleIdx];
    var propName = origBundle.animatedAttribute.attrName;
    if (propertyNameToMappedAttr[propName]) {
      // There's a mapped attribute by this name! Duplicate the TestcaseBundle,
      // using the Mapped Attribute instead of the CSS Property.
      is(origBundle.animatedAttribute.attrType, "CSS",
         "expecting to be converting from CSS to XML");
      convertedBundles.push(
        new TestcaseBundle(propertyNameToMappedAttr[propName],
                           origBundle.testcaseList,
                           origBundle.skipReason));
    }
  }
  return convertedBundles;
}
