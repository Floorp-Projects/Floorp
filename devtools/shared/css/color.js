/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const COLOR_UNIT_PREF = "devtools.defaultColorUnit";
const SPECIALVALUES = new Set([
  "currentcolor",
  "initial",
  "inherit",
  "transparent",
  "unset",
]);

/**
 * This module is used to convert between various color types.
 *
 * Usage:
 *   let {colorUtils} = require("devtools/shared/css/color");
 *   let color = new colorUtils.CssColor("red");
 *   // In order to support css-color-4 color function, pass true to the
 *   // second argument.
 *   // e.g.
 *   //   let color = new colorUtils.CssColor("red", true);
 *
 *   color.authored === "red"
 *   color.hasAlpha === false
 *   color.valid === true
 *   color.transparent === false // transparent has a special status.
 *   color.name === "red"        // returns hex when no name available.
 *   color.hex === "#f00"        // returns shortHex when available else returns
 *                                  longHex. If alpha channel is present then we
 *                                  return this.alphaHex if available,
 *                                  or this.longAlphaHex if not.
 *   color.alphaHex === "#f00f"  // returns short alpha hex when available
 *                                  else returns longAlphaHex.
 *   color.longHex === "#ff0000" // If alpha channel is present then we return
 *                                  this.longAlphaHex.
 *   color.longAlphaHex === "#ff0000ff"
 *   color.rgb === "rgb(255, 0, 0)" // If alpha channel is present
 *                                  // then we return this.rgba.
 *   color.rgba === "rgba(255, 0, 0, 1)"
 *   color.hsl === "hsl(0, 100%, 50%)"
 *   color.hsla === "hsla(0, 100%, 50%, 1)" // If alpha channel is present
 *                                             then we return this.rgba.
 *   color.hwb === "hwb(0, 0%, 0%)"
 *
 *   color.toString() === "#f00"; // Outputs the color type determined in the
 *                                   COLOR_UNIT_PREF constant (above).
 *
 *   Valid values for COLOR_UNIT_PREF are contained in CssColor.COLORUNIT.
 */
class CssColor {
  /**
   * @param  {String} colorValue: Any valid color string
   */
  constructor(colorValue) {
    // Store a lower-cased version of the color to help with format
    // testing.  The original text is kept as well so it can be
    // returned when needed.
    this.#lowerCased = colorValue.toLowerCase();
    this.#authored = colorValue;
  }

  /**
   * Values used in COLOR_UNIT_PREF
   */
  static COLORUNIT = {
    authored: "authored",
    hex: "hex",
    name: "name",
    rgb: "rgb",
    hsl: "hsl",
    hwb: "hwb",
  };

  // The value as-authored.
  #authored = null;
  #currentFormat;
  // A lower-cased copy of |authored|.
  #lowerCased = null;

  get hasAlpha() {
    if (!this.valid) {
      return false;
    }
    return this.getRGBATuple().a !== 1;
  }

  /**
   * Return true if the color is a valid color and we can get rgba tuples from it.
   */
  get valid() {
    // We can't use InspectorUtils.isValidCSSColor as colors can be valid but we can't have
    // their rgba tuples (e.g. currentColor, accentColor, … whose actual values depends on
    // additional context we don't have here).
    return InspectorUtils.colorToRGBA(this.#authored) !== null;
  }

  /**
   * Not a real color type but used to preserve accuracy when converting between
   * e.g. 8 character hex -> rgba -> 8 character hex (hex alpha values are
   * 0 - 255 but rgba alpha values are only 0.0 to 1.0).
   */
  get highResTuple() {
    const type = classifyColor(this.#authored);

    if (type === CssColor.COLORUNIT.hex) {
      return hexToRGBA(this.#authored.substring(1), true);
    }

    // If we reach this point then the alpha value must be in the range
    // 0.0 - 1.0 so we need to multiply it by 255.
    const tuple = InspectorUtils.colorToRGBA(this.#authored);
    tuple.a *= 255;
    return tuple;
  }

  /**
   * Return true for all transparent values e.g. rgba(0, 0, 0, 0).
   */
  get transparent() {
    try {
      const tuple = this.getRGBATuple();
      return !(tuple.r || tuple.g || tuple.b || tuple.a);
    } catch (e) {
      return false;
    }
  }

  get specialValue() {
    return SPECIALVALUES.has(this.#lowerCased) ? this.#authored : null;
  }

  get name() {
    const invalidOrSpecialValue = this.#getInvalidOrSpecialValue();
    if (invalidOrSpecialValue !== false) {
      return invalidOrSpecialValue;
    }

    const tuple = this.getRGBATuple();

    if (tuple.a !== 1) {
      return this.hex;
    }
    const { r, g, b } = tuple;
    return InspectorUtils.rgbToColorName(r, g, b) || this.hex;
  }

  get hex() {
    const invalidOrSpecialValue = this.#getInvalidOrSpecialValue();
    if (invalidOrSpecialValue !== false) {
      return invalidOrSpecialValue;
    }
    if (this.hasAlpha) {
      return this.alphaHex;
    }

    let hex = this.longHex;
    if (
      hex.charAt(1) == hex.charAt(2) &&
      hex.charAt(3) == hex.charAt(4) &&
      hex.charAt(5) == hex.charAt(6)
    ) {
      hex = "#" + hex.charAt(1) + hex.charAt(3) + hex.charAt(5);
    }
    return hex;
  }

  get alphaHex() {
    const invalidOrSpecialValue = this.#getInvalidOrSpecialValue();
    if (invalidOrSpecialValue !== false) {
      return invalidOrSpecialValue;
    }

    let alphaHex = this.longAlphaHex;
    if (
      alphaHex.charAt(1) == alphaHex.charAt(2) &&
      alphaHex.charAt(3) == alphaHex.charAt(4) &&
      alphaHex.charAt(5) == alphaHex.charAt(6) &&
      alphaHex.charAt(7) == alphaHex.charAt(8)
    ) {
      alphaHex =
        "#" +
        alphaHex.charAt(1) +
        alphaHex.charAt(3) +
        alphaHex.charAt(5) +
        alphaHex.charAt(7);
    }
    return alphaHex;
  }

  get longHex() {
    const invalidOrSpecialValue = this.#getInvalidOrSpecialValue();
    if (invalidOrSpecialValue !== false) {
      return invalidOrSpecialValue;
    }
    if (this.hasAlpha) {
      return this.longAlphaHex;
    }

    const tuple = this.getRGBATuple();
    return (
      "#" +
      ((1 << 24) + (tuple.r << 16) + (tuple.g << 8) + (tuple.b << 0))
        .toString(16)
        .substr(-6)
    );
  }

  get longAlphaHex() {
    const invalidOrSpecialValue = this.#getInvalidOrSpecialValue();
    if (invalidOrSpecialValue !== false) {
      return invalidOrSpecialValue;
    }

    const tuple = this.highResTuple;

    return (
      "#" +
      ((1 << 24) + (tuple.r << 16) + (tuple.g << 8) + (tuple.b << 0))
        .toString(16)
        .substr(-6) +
      Math.round(tuple.a).toString(16).padStart(2, "0")
    );
  }

  get rgb() {
    const invalidOrSpecialValue = this.#getInvalidOrSpecialValue();
    if (invalidOrSpecialValue !== false) {
      return invalidOrSpecialValue;
    }
    if (!this.hasAlpha) {
      if (this.#lowerCased.startsWith("rgb(")) {
        // The color is valid and begins with rgb(.
        return this.#authored;
      }
      const tuple = this.getRGBATuple();
      return "rgb(" + tuple.r + ", " + tuple.g + ", " + tuple.b + ")";
    }
    return this.rgba;
  }

  get rgba() {
    const invalidOrSpecialValue = this.#getInvalidOrSpecialValue();
    if (invalidOrSpecialValue !== false) {
      return invalidOrSpecialValue;
    }
    if (this.#lowerCased.startsWith("rgba(")) {
      // The color is valid and begins with rgba(.
      return this.#authored;
    }
    const components = this.getRGBATuple();
    return (
      "rgba(" +
      components.r +
      ", " +
      components.g +
      ", " +
      components.b +
      ", " +
      components.a +
      ")"
    );
  }

  get hsl() {
    const invalidOrSpecialValue = this.#getInvalidOrSpecialValue();
    if (invalidOrSpecialValue !== false) {
      return invalidOrSpecialValue;
    }
    if (this.#lowerCased.startsWith("hsl(")) {
      // The color is valid and begins with hsl(.
      return this.#authored;
    }
    if (this.hasAlpha) {
      return this.hsla;
    }
    return this.#hsl();
  }

  get hsla() {
    const invalidOrSpecialValue = this.#getInvalidOrSpecialValue();
    if (invalidOrSpecialValue !== false) {
      return invalidOrSpecialValue;
    }
    if (this.#lowerCased.startsWith("hsla(")) {
      // The color is valid and begins with hsla(.
      return this.#authored;
    }
    if (this.hasAlpha) {
      const a = this.getRGBATuple().a;
      return this.#hsl(a);
    }
    return this.#hsl(1);
  }

  get hwb() {
    const invalidOrSpecialValue = this.#getInvalidOrSpecialValue();
    if (invalidOrSpecialValue !== false) {
      return invalidOrSpecialValue;
    }
    if (this.#lowerCased.startsWith("hwb(")) {
      // The color is valid and begins with hwb(.
      return this.#authored;
    }
    if (this.hasAlpha) {
      const a = this.getRGBATuple().a;
      return this.#hwb(a);
    }
    return this.#hwb();
  }

  /**
   * Check whether the current color value is in the special list e.g.
   * transparent or invalid.
   *
   * @return {String|Boolean}
   *         - If the current color is a special value e.g. "transparent" then
   *           return the color.
   *         - If the current color is a system value e.g. "accentcolor" then
   *           return the color.
   *         - If the color is invalid or that we can't get rgba components from it
   *           (e.g. "accentcolor"), return an empty string.
   *         - If the color is a regular color e.g. #F06 so we return false
   *           to indicate that the color is neither invalid or special.
   */
  #getInvalidOrSpecialValue() {
    if (this.specialValue) {
      return this.specialValue;
    }
    if (!this.valid) {
      return "";
    }
    return false;
  }

  nextColorUnit() {
    // Reorder the formats array to have the current format at the
    // front so we can cycle through.
    // Put "name" at the end as that provides a hex value if there's
    // no name for the color.
    let formats = ["hex", "hsl", "rgb", "hwb", "name"];

    let currentFormat = this.#currentFormat;
    // If we don't have determined the current format yet
    if (!currentFormat) {
      // If the pref value is COLORUNIT.authored, get the actual unit from the authored color,
      // otherwise use the pref value.
      const defaultFormat = Services.prefs.getCharPref(COLOR_UNIT_PREF);
      currentFormat =
        defaultFormat === CssColor.COLORUNIT.authored
          ? classifyColor(this.#authored)
          : defaultFormat;
    }
    const putOnEnd = formats.splice(0, formats.indexOf(currentFormat));
    formats = [...formats, ...putOnEnd];

    const currentDisplayedColor = this[formats[0]];

    let colorUnit;
    for (const format of formats) {
      if (this[format].toLowerCase() !== currentDisplayedColor.toLowerCase()) {
        colorUnit = CssColor.COLORUNIT[format];
        break;
      }
    }

    this.#currentFormat = colorUnit;
    return this.toString(colorUnit);
  }

  /**
   * Return a string representing a color of type defined in COLOR_UNIT_PREF.
   */
  toString(colorUnit, forceUppercase) {
    let color;

    switch (colorUnit) {
      case CssColor.COLORUNIT.authored:
        color = this.#authored;
        break;
      case CssColor.COLORUNIT.hex:
        color = this.hex;
        break;
      case CssColor.COLORUNIT.hsl:
        color = this.hsl;
        break;
      case CssColor.COLORUNIT.name:
        color = this.name;
        break;
      case CssColor.COLORUNIT.rgb:
        color = this.rgb;
        break;
      case CssColor.COLORUNIT.hwb:
        color = this.hwb;
        break;
      default:
        color = this.rgb;
    }

    if (
      forceUppercase ||
      (colorUnit != CssColor.COLORUNIT.authored &&
        colorIsUppercase(this.#authored))
    ) {
      color = color.toUpperCase();
    }

    return color;
  }

  /**
   * Returns a RGBA 4-Tuple representation of a color or transparent as
   * appropriate.
   */
  getRGBATuple() {
    const tuple = InspectorUtils.colorToRGBA(this.#authored);

    tuple.a = parseFloat(tuple.a.toFixed(2));

    return tuple;
  }

  #hsl(maybeAlpha) {
    if (this.#lowerCased.startsWith("hsl(") && maybeAlpha === undefined) {
      // We can use it as-is.
      return this.#authored;
    }

    const { r, g, b } = this.getRGBATuple();
    const [h, s, l] = rgbToHsl([r, g, b]);
    if (maybeAlpha !== undefined) {
      return "hsla(" + h + ", " + s + "%, " + l + "%, " + maybeAlpha + ")";
    }
    return "hsl(" + h + ", " + s + "%, " + l + "%)";
  }

  #hwb(maybeAlpha) {
    if (this.#lowerCased.startsWith("hwb(") && maybeAlpha === undefined) {
      // We can use it as-is.
      return this.#authored;
    }

    const { r, g, b } = this.getRGBATuple();
    const [hue, white, black] = rgbToHwb([r, g, b]);
    return `hwb(${hue} ${white}% ${black}%${
      maybeAlpha !== undefined ? " / " + maybeAlpha : ""
    })`;
  }

  /**
   * This method allows comparison of CssColor objects using ===.
   */
  valueOf() {
    return this.rgba;
  }

  /**
   * Check whether the color is fully transparent (alpha === 0).
   *
   * @return {Boolean} True if the color is transparent and valid.
   */
  isTransparent() {
    return this.getRGBATuple().a === 0;
  }
}

/**
 * Convert rgb value to hsl
 *
 * @param {array} rgb
 *         Array of rgb values
 * @return {array}
 *         Array of hsl values.
 */
function rgbToHsl([r, g, b]) {
  r = r / 255;
  g = g / 255;
  b = b / 255;

  const max = Math.max(r, g, b);
  const min = Math.min(r, g, b);
  let h;
  let s;
  const l = (max + min) / 2;

  if (max == min) {
    h = s = 0;
  } else {
    const d = max - min;
    s = l > 0.5 ? d / (2 - max - min) : d / (max + min);

    switch (max) {
      case r:
        h = ((g - b) / d) % 6;
        break;
      case g:
        h = (b - r) / d + 2;
        break;
      case b:
        h = (r - g) / d + 4;
        break;
    }
    h *= 60;
    if (h < 0) {
      h += 360;
    }
  }

  return [roundTo(h, 1), roundTo(s * 100, 1), roundTo(l * 100, 1)];
}

/**
 * Convert RGB value to HWB
 *
 * @param {array} rgb
 *         Array of RGB values
 * @return {array}
 *         Array of HWB values.
 */
function rgbToHwb([r, g, b]) {
  const hsl = rgbToHsl([r, g, b]);

  r = r / 255;
  g = g / 255;
  b = b / 255;

  const white = Math.min(r, g, b);
  const black = 1 - Math.max(r, g, b);
  return [roundTo(hsl[0], 1), roundTo(white * 100, 1), roundTo(black * 100, 1)];
}

/**
 * Convert rgb value to CIE LAB colorspace (https://en.wikipedia.org/wiki/CIELAB_color_space).
 * Formula from http://www.easyrgb.com/en/math.php.
 *
 * @param {array} rgb
 *        Array of rgb values
 * @return {array}
 *         Array of lab values.
 */
function rgbToLab([r, g, b]) {
  // Convert rgb values to xyz coordinates.
  r = r / 255;
  g = g / 255;
  b = b / 255;

  r = r > 0.04045 ? Math.pow((r + 0.055) / 1.055, 2.4) : r / 12.92;
  g = g > 0.04045 ? Math.pow((g + 0.055) / 1.055, 2.4) : g / 12.92;
  b = b > 0.04045 ? Math.pow((b + 0.055) / 1.055, 2.4) : b / 12.92;

  r = r * 100;
  g = g * 100;
  b = b * 100;

  let [x, y, z] = [
    r * 0.4124 + g * 0.3576 + b * 0.1805,
    r * 0.2126 + g * 0.7152 + b * 0.0722,
    r * 0.0193 + g * 0.1192 + b * 0.9505,
  ];

  // Convert xyz coordinates to lab values.
  // Divisors used are X_10, Y_10, Z_10 (CIE 1964) reference values for D65
  // illuminant (Daylight, sRGB, Adobe-RGB) taken from http://www.easyrgb.com/en/math.php
  x = x / 94.811;
  y = y / 100;
  z = z / 107.304;

  x = x > 0.008856 ? Math.pow(x, 1 / 3) : 7.787 * x + 16 / 116;
  y = y > 0.008856 ? Math.pow(y, 1 / 3) : 7.787 * y + 16 / 116;
  z = z > 0.008856 ? Math.pow(z, 1 / 3) : 7.787 * z + 16 / 116;

  return [116 * y - 16, 500 * (x - y), 200 * (y - z)];
}

/**
 * Calculates the CIE Delta-E value for two lab values (http://www.colorwiki.com/wiki/Delta_E%3a_The_Color_Difference#Delta-E_1976).
 * Formula from http://www.easyrgb.com/en/math.php.
 *
 * @param {array} lab1
 *        Array of lab values for the first color
 * @param {array} lab2
 *        Array of lab values for the second color
 * @return {Number}
 *         DeltaE value between the two colors
 */
function calculateDeltaE([l1, a1, b1], [l2, a2, b2]) {
  return Math.sqrt(
    Math.pow(l1 - l2, 2) + Math.pow(a1 - a2, 2) + Math.pow(b1 - b2, 2)
  );
}

function roundTo(number, digits) {
  const multiplier = Math.pow(10, digits);
  return Math.round(number * multiplier) / multiplier;
}

/**
 * Given a color, classify its type as one of the possible color
 * units, as known by |CssColor.COLORUNIT|.
 *
 * @param  {String} value
 *         The color, in any form accepted by CSS.
 * @return {String}
 *         The color classification, one of "rgb", "hsl", "hwb",
 *         "hex", "name", or if no format is recognized, "authored".
 */
function classifyColor(value) {
  value = value.toLowerCase();
  if (value.startsWith("rgb(") || value.startsWith("rgba(")) {
    return CssColor.COLORUNIT.rgb;
  } else if (value.startsWith("hsl(") || value.startsWith("hsla(")) {
    return CssColor.COLORUNIT.hsl;
  } else if (value.startsWith("hwb(")) {
    return CssColor.COLORUNIT.hwb;
  } else if (/^#[0-9a-f]+$/.exec(value)) {
    return CssColor.COLORUNIT.hex;
  } else if (/^[a-z\-]+$/.exec(value)) {
    return CssColor.COLORUNIT.name;
  }
  return CssColor.COLORUNIT.authored;
}

/**
 * A helper function to convert a hex string like "F0C" or "F0C8" to a color.
 *
 * @param {String} name the color string
 * @param {Boolean} highResolution Forces returned alpha value to be in the
 *                  range 0 - 255 as opposed to 0.0 - 1.0.
 * @return {Object} an object of the form {r, g, b, a}; or null if the
 *         name was not a valid color
 */
function hexToRGBA(name, highResolution) {
  let r,
    g,
    b,
    a = 1;

  if (name.length === 3) {
    // short hex string (e.g. F0C)
    r = parseInt(name.charAt(0) + name.charAt(0), 16);
    g = parseInt(name.charAt(1) + name.charAt(1), 16);
    b = parseInt(name.charAt(2) + name.charAt(2), 16);
  } else if (name.length === 4) {
    // short alpha hex string (e.g. F0CA)
    r = parseInt(name.charAt(0) + name.charAt(0), 16);
    g = parseInt(name.charAt(1) + name.charAt(1), 16);
    b = parseInt(name.charAt(2) + name.charAt(2), 16);
    a = parseInt(name.charAt(3) + name.charAt(3), 16);

    if (!highResolution) {
      a /= 255;
    }
  } else if (name.length === 6) {
    // hex string (e.g. FD01CD)
    r = parseInt(name.charAt(0) + name.charAt(1), 16);
    g = parseInt(name.charAt(2) + name.charAt(3), 16);
    b = parseInt(name.charAt(4) + name.charAt(5), 16);
  } else if (name.length === 8) {
    // alpha hex string (e.g. FD01CDAB)
    r = parseInt(name.charAt(0) + name.charAt(1), 16);
    g = parseInt(name.charAt(2) + name.charAt(3), 16);
    b = parseInt(name.charAt(4) + name.charAt(5), 16);
    a = parseInt(name.charAt(6) + name.charAt(7), 16);

    if (!highResolution) {
      a /= 255;
    }
  } else {
    return null;
  }
  if (!highResolution) {
    a = Math.round(a * 10) / 10;
  }
  return { r, g, b, a };
}

/**
 * Calculates the luminance of a rgba tuple based on the formula given in
 * https://www.w3.org/TR/2008/REC-WCAG20-20081211/#relativeluminancedef
 *
 * @param {Array} rgba An array with [r,g,b,a] values.
 * @return {Number} The calculated luminance.
 */
function calculateLuminance(rgba) {
  for (let i = 0; i < 3; i++) {
    rgba[i] /= 255;
    rgba[i] =
      rgba[i] < 0.03928
        ? rgba[i] / 12.92
        : Math.pow((rgba[i] + 0.055) / 1.055, 2.4);
  }
  return 0.2126 * rgba[0] + 0.7152 * rgba[1] + 0.0722 * rgba[2];
}

/**
 * Blend background and foreground colors takign alpha into account.
 * @param  {Array} foregroundColor
 *         An array with [r,g,b,a] values containing the foreground color.
 * @param  {Array} backgroundColor
 *         An array with [r,g,b,a] values containing the background color. Defaults to
 *         [ 255, 255, 255, 1 ].
 * @return {Array}
 *         An array with combined [r,g,b,a] colors.
 */
function blendColors(foregroundColor, backgroundColor = [255, 255, 255, 1]) {
  const [fgR, fgG, fgB, fgA] = foregroundColor;
  const [bgR, bgG, bgB, bgA] = backgroundColor;
  if (fgA === 1) {
    return foregroundColor;
  }

  return [
    (1 - fgA) * bgR + fgA * fgR,
    (1 - fgA) * bgG + fgA * fgG,
    (1 - fgA) * bgB + fgA * fgB,
    fgA + bgA * (1 - fgA),
  ];
}

/**
 * Calculates the contrast ratio of 2 rgba tuples based on the formula in
 * https://www.w3.org/TR/2008/REC-WCAG20-20081211/#visual-audio-contrast7
 *
 * @param {Array} backgroundColor An array with [r,g,b,a] values containing
 * the background color.
 * @param {Array} textColor An array with [r,g,b,a] values containing
 * the text color.
 * @return {Number} The calculated luminance.
 */
function calculateContrastRatio(backgroundColor, textColor) {
  // Do not modify given colors.
  backgroundColor = Array.from(backgroundColor);
  textColor = Array.from(textColor);

  backgroundColor = blendColors(backgroundColor);
  textColor = blendColors(textColor, backgroundColor);

  const backgroundLuminance = calculateLuminance(backgroundColor);
  const textLuminance = calculateLuminance(textColor);
  const ratio = (textLuminance + 0.05) / (backgroundLuminance + 0.05);

  return ratio > 1.0 ? ratio : 1 / ratio;
}

function colorIsUppercase(color) {
  // Specifically exclude the case where the color is
  // case-insensitive.  This makes it so that "#000" isn't
  // considered "upper case" for the purposes of color cycling.
  return color === color.toUpperCase() && color !== color.toLowerCase();
}

module.exports.colorUtils = {
  CssColor,
  rgbToHsl,
  rgbToHwb,
  rgbToLab,
  classifyColor,
  calculateContrastRatio,
  calculateDeltaE,
  calculateLuminance,
  blendColors,
  colorIsUppercase,
};
