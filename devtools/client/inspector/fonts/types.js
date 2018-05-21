/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

/**
 * A font variation axis.
 */
const fontVariationAxis = exports.fontVariationAxis = {
  // The OpenType tag name of the variation axis
  tag: PropTypes.string,

  // The axis name of the variation axis
  name: PropTypes.string,

  // The minimum value of the variation axis
  minValue: PropTypes.number,

  // The maximum value of the variation axis
  maxValue: PropTypes.number,

  // The default value of the variation axis
  defaultValue: PropTypes.number,
};

const fontVariationInstanceValue = exports.fontVariationInstanceValue = {
  // The axis name of the variation axis
  axis: PropTypes.string,

  // The value of the variation axis
  value: PropTypes.number,
};

/**
 * A font variation instance.
 */
const fontVariationInstance = exports.fontVariationInstance = {
  // The variation instance name of the font
  name: PropTypes.string,

  // The font variation values for the variation instance of the font
  values: PropTypes.arrayOf(PropTypes.shape(fontVariationInstanceValue)),
};

/**
 * A single font.
 */
const font = exports.font = {
  // The format of the font
  format: PropTypes.string,

  // The name of the font
  name: PropTypes.string,

  // URL for the font preview
  previewUrl: PropTypes.string,

  // Object containing the CSS rule for the font
  rule: PropTypes.object,

  // The text of the CSS rule
  ruleText: PropTypes.string,

  // The URI of the font file
  URI: PropTypes.string,

  // The variation axes of the font
  variationAxes: PropTypes.arrayOf(PropTypes.shape(fontVariationAxis)),

  // The variation instances of the font
  variationInstances: PropTypes.arrayOf(PropTypes.shape(fontVariationInstance))
};

exports.fontOptions = {
  // The current preview text
  previewText: PropTypes.string,
};

exports.fontEditor = {
  // Variable font axes and their values
  axes: PropTypes.object,

  // Axes values changed at runtime structured like the "values" property
  // of a fontVariationInstance
  customInstanceValues: PropTypes.array,

  // Fonts applicable to selected element
  fonts: PropTypes.arrayOf(PropTypes.shape(font)),

  // Font variation instance currently selected
  instance: PropTypes.shape(fontVariationInstance),

  // Whether or not the font editor is visible
  isVisible: PropTypes.bool,

  // CSS font properties defined on the element
  properties: PropTypes.object,

  // Selector text of the rule where font properties will be written
  selector: PropTypes.string,
};

/**
 * Font data.
 */
exports.fontData = {
  // The fonts used in the current element.
  fonts: PropTypes.arrayOf(PropTypes.shape(font)),

  // Fonts used elsewhere.
  otherFonts: PropTypes.arrayOf(PropTypes.shape(font)),
};
