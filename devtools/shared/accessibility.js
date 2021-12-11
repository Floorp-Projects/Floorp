/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(this, "colorUtils", "devtools/shared/css/color", true);
const {
  accessibility: {
    SCORES: { FAIL, AA, AAA },
  },
} = require("devtools/shared/constants");

/**
 * Mapping of text size to contrast ratio score levels
 */
const LEVELS = {
  LARGE_TEXT: { AA: 3, AAA: 4.5 },
  REGULAR_TEXT: { AA: 4.5, AAA: 7 },
};

/**
 * Mapping of large text size to CSS pixel value
 */
const LARGE_TEXT = {
  // CSS pixel value (constant) that corresponds to 14 point text size which defines large
  // text when font text is bold (font weight is greater than or equal to 600).
  BOLD_LARGE_TEXT_MIN_PIXELS: 18.66,
  // CSS pixel value (constant) that corresponds to 18 point text size which defines large
  // text for normal text (e.g. not bold).
  LARGE_TEXT_MIN_PIXELS: 24,
};

/**
 * Get contrast ratio score based on WCAG criteria.
 * @param  {Number} ratio
 *         Value of the contrast ratio for a given accessible object.
 * @param  {Boolean} isLargeText
 *         True if the accessible object contains large text.
 * @return {String}
 *         Value that represents calculated contrast ratio score.
 */
function getContrastRatioScore(ratio, isLargeText) {
  const levels = isLargeText ? LEVELS.LARGE_TEXT : LEVELS.REGULAR_TEXT;

  let score = FAIL;
  if (ratio >= levels.AAA) {
    score = AAA;
  } else if (ratio >= levels.AA) {
    score = AA;
  }

  return score;
}

/**
 * Get calculated text style properties from a node's computed style, if possible.
 * @param  {Object} computedStyle
 *         Computed style using which text styling information is to be calculated.
 *         - fontSize   {String}
 *                      Font size of the text
 *         - fontWeight {String}
 *                      Font weight of the text
 *         - color      {String}
 *                      Rgb color of the text
 *         - opacity    {String} Optional
 *                      Opacity of the text
 * @return {Object}
 *         Color and text size information for a given DOM node.
 */
function getTextProperties(computedStyle) {
  const { color, fontSize, fontWeight } = computedStyle;
  let { r, g, b, a } = colorUtils.colorToRGBA(color, true);

  // If the element has opacity in addition to background alpha value, take it
  // into account. TODO: this does not handle opacity set on ancestor elements
  // (see bug https://bugzilla.mozilla.org/show_bug.cgi?id=1544721).
  const opacity = computedStyle.opacity
    ? parseFloat(computedStyle.opacity)
    : null;
  if (opacity) {
    a = opacity * a;
  }

  const textRgbaColor = new colorUtils.CssColor(
    `rgba(${r}, ${g}, ${b}, ${a})`,
    true
  );
  // TODO: For cases where text color is transparent, it likely comes from the color of
  // the background that is underneath it (commonly from background-clip: text
  // property). With some additional investigation it might be possible to calculate the
  // color contrast where the color of the background is used as text color and the
  // color of the ancestor's background is used as its background.
  if (textRgbaColor.isTransparent()) {
    return null;
  }

  const isBoldText = parseInt(fontWeight, 10) >= 600;
  const size = parseFloat(fontSize);
  const isLargeText =
    size >=
    (isBoldText
      ? LARGE_TEXT.BOLD_LARGE_TEXT_MIN_PIXELS
      : LARGE_TEXT.LARGE_TEXT_MIN_PIXELS);

  return {
    color: [r, g, b, a],
    isLargeText,
    isBoldText,
    size,
    opacity,
  };
}

/**
 * Calculates contrast ratio or range of contrast ratios of the referenced DOM node
 * against the given background color data. If background is multi-colored, return a
 * range, otherwise a single contrast ratio.
 *
 * @param  {Object} backgroundColorData
 *         Object with one or more of the following properties:
 *         - value              {Array}
 *                              rgba array for single color background
 *         - min                {Array}
 *                              min luminance rgba array for multi color background
 *         - max                {Array}
 *                              max luminance rgba array for multi color background
 * @param  {Object}  textData
 *         - color              {Array}
 *                              rgba array for text of referenced DOM node
 *         - isLargeText        {Boolean}
 *                              True if text of referenced DOM node is large
 * @return {Object}
 *         An object that may contain one or more of the following fields: error,
 *         isLargeText, value, min, max values for contrast.
 */
function getContrastRatioAgainstBackground(
  backgroundColorData,
  { color, isLargeText }
) {
  if (backgroundColorData.value) {
    const value = colorUtils.calculateContrastRatio(
      backgroundColorData.value,
      color
    );
    return {
      value,
      color,
      backgroundColor: backgroundColorData.value,
      isLargeText,
      score: getContrastRatioScore(value, isLargeText),
    };
  }

  let {
    min: backgroundColorMin,
    max: backgroundColorMax,
  } = backgroundColorData;
  let min = colorUtils.calculateContrastRatio(backgroundColorMin, color);
  let max = colorUtils.calculateContrastRatio(backgroundColorMax, color);

  // Flip minimum and maximum contrast ratios if necessary.
  if (min > max) {
    [min, max] = [max, min];
    [backgroundColorMin, backgroundColorMax] = [
      backgroundColorMax,
      backgroundColorMin,
    ];
  }

  const score = getContrastRatioScore(min, isLargeText);

  return {
    min,
    max,
    color,
    backgroundColorMin,
    backgroundColorMax,
    isLargeText,
    score,
    scoreMin: score,
    scoreMax: getContrastRatioScore(max, isLargeText),
  };
}

exports.getContrastRatioScore = getContrastRatioScore;
exports.getTextProperties = getTextProperties;
exports.getContrastRatioAgainstBackground = getContrastRatioAgainstBackground;
exports.LARGE_TEXT = LARGE_TEXT;
