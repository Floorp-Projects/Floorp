/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

const {CSS_ANGLEUNIT} = require("devtools/shared/css/properties-db");
const {getAngleValueInDegrees} = require("devtools/shared/css/parsing-utils");

const {getCSSLexer} = require("devtools/shared/css/lexer");
const {cssColors} = require("devtools/shared/css/color-db");

const COLOR_UNIT_PREF = "devtools.defaultColorUnit";

const SPECIALVALUES = new Set([
  "currentcolor",
  "initial",
  "inherit",
  "transparent",
  "unset"
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
 *
 *   color.toString() === "#f00"; // Outputs the color type determined in the
 *                                   COLOR_UNIT_PREF constant (above).
 *   // Color objects can be reused
 *   color.newColor("green") === "#0f0"; // true
 *
 *   Valid values for COLOR_UNIT_PREF are contained in CssColor.COLORUNIT.
 */

function CssColor(colorValue, supportsCssColor4ColorFunction = false) {
  this.newColor(colorValue);
  this.cssColor4 = supportsCssColor4ColorFunction;
}

module.exports.colorUtils = {
  CssColor: CssColor,
  rgbToHsl: rgbToHsl,
  setAlpha: setAlpha,
  classifyColor: classifyColor,
  rgbToColorName: rgbToColorName,
  colorToRGBA: colorToRGBA,
  isValidCSSColor: isValidCSSColor,
};

/**
 * Values used in COLOR_UNIT_PREF
 */
CssColor.COLORUNIT = {
  "authored": "authored",
  "hex": "hex",
  "name": "name",
  "rgb": "rgb",
  "hsl": "hsl"
};

CssColor.prototype = {
  _colorUnit: null,
  _colorUnitUppercase: false,

  // The value as-authored.
  authored: null,
  // A lower-cased copy of |authored|.
  lowerCased: null,

  // Whether the value should be parsed using css-color-4 rules.
  cssColor4: false,

  _setColorUnitUppercase: function (color) {
    // Specifically exclude the case where the color is
    // case-insensitive.  This makes it so that "#000" isn't
    // considered "upper case" for the purposes of color cycling.
    this._colorUnitUppercase = (color === color.toUpperCase()) &&
      (color !== color.toLowerCase());
  },

  get colorUnit() {
    if (this._colorUnit === null) {
      let defaultUnit = Services.prefs.getCharPref(COLOR_UNIT_PREF);
      this._colorUnit = CssColor.COLORUNIT[defaultUnit];
      this._setColorUnitUppercase(this.authored);
    }
    return this._colorUnit;
  },

  set colorUnit(unit) {
    this._colorUnit = unit;
  },

  /**
   * If the current color unit pref is "authored", then set the
   * default color unit from the given color.  Otherwise, leave the
   * color unit untouched.
   *
   * @param {String} color The color to use
   */
  setAuthoredUnitFromColor: function (color) {
    if (Services.prefs.getCharPref(COLOR_UNIT_PREF) ===
        CssColor.COLORUNIT.authored) {
      this._colorUnit = classifyColor(color);
      this._setColorUnitUppercase(color);
    }
  },

  get hasAlpha() {
    if (!this.valid) {
      return false;
    }
    return this._getRGBATuple().a !== 1;
  },

  get valid() {
    return isValidCSSColor(this.authored, this.cssColor4);
  },

  /**
   * Return true for all transparent values e.g. rgba(0, 0, 0, 0).
   */
  get transparent() {
    try {
      let tuple = this._getRGBATuple();
      return !(tuple.r || tuple.g || tuple.b || tuple.a);
    } catch (e) {
      return false;
    }
  },

  get specialValue() {
    return SPECIALVALUES.has(this.lowerCased) ? this.authored : null;
  },

  get name() {
    let invalidOrSpecialValue = this._getInvalidOrSpecialValue();
    if (invalidOrSpecialValue !== false) {
      return invalidOrSpecialValue;
    }

    try {
      let tuple = this._getRGBATuple();

      if (tuple.a !== 1) {
        return this.hex;
      }
      let {r, g, b} = tuple;
      return rgbToColorName(r, g, b);
    } catch (e) {
      return this.hex;
    }
  },

  get hex() {
    let invalidOrSpecialValue = this._getInvalidOrSpecialValue();
    if (invalidOrSpecialValue !== false) {
      return invalidOrSpecialValue;
    }
    if (this.hasAlpha) {
      return this.alphaHex;
    }

    let hex = this.longHex;
    if (hex.charAt(1) == hex.charAt(2) &&
        hex.charAt(3) == hex.charAt(4) &&
        hex.charAt(5) == hex.charAt(6)) {
      hex = "#" + hex.charAt(1) + hex.charAt(3) + hex.charAt(5);
    }
    return hex;
  },

  get alphaHex() {
    let invalidOrSpecialValue = this._getInvalidOrSpecialValue();
    if (invalidOrSpecialValue !== false) {
      return invalidOrSpecialValue;
    }

    let alphaHex = this.longAlphaHex;
    if (alphaHex.charAt(1) == alphaHex.charAt(2) &&
        alphaHex.charAt(3) == alphaHex.charAt(4) &&
        alphaHex.charAt(5) == alphaHex.charAt(6) &&
        alphaHex.charAt(7) == alphaHex.charAt(8)) {
      alphaHex = "#" + alphaHex.charAt(1) + alphaHex.charAt(3) +
        alphaHex.charAt(5) + alphaHex.charAt(7);
    }
    return alphaHex;
  },

  get longHex() {
    let invalidOrSpecialValue = this._getInvalidOrSpecialValue();
    if (invalidOrSpecialValue !== false) {
      return invalidOrSpecialValue;
    }
    if (this.hasAlpha) {
      return this.longAlphaHex;
    }

    let tuple = this._getRGBATuple();
    return "#" + ((1 << 24) + (tuple.r << 16) + (tuple.g << 8) +
                  (tuple.b << 0)).toString(16).substr(-6);
  },

  get longAlphaHex() {
    let invalidOrSpecialValue = this._getInvalidOrSpecialValue();
    if (invalidOrSpecialValue !== false) {
      return invalidOrSpecialValue;
    }

    let tuple = this._getRGBATuple();
    return "#" + ((1 << 24) + (tuple.r << 16) + (tuple.g << 8) +
                  (tuple.b << 0)).toString(16).substr(-6) +
                  Math.round(tuple.a * 255).toString(16).padEnd(2, "0");
  },

  get rgb() {
    let invalidOrSpecialValue = this._getInvalidOrSpecialValue();
    if (invalidOrSpecialValue !== false) {
      return invalidOrSpecialValue;
    }
    if (!this.hasAlpha) {
      if (this.lowerCased.startsWith("rgb(")) {
        // The color is valid and begins with rgb(.
        return this.authored;
      }
      let tuple = this._getRGBATuple();
      return "rgb(" + tuple.r + ", " + tuple.g + ", " + tuple.b + ")";
    }
    return this.rgba;
  },

  get rgba() {
    let invalidOrSpecialValue = this._getInvalidOrSpecialValue();
    if (invalidOrSpecialValue !== false) {
      return invalidOrSpecialValue;
    }
    if (this.lowerCased.startsWith("rgba(")) {
      // The color is valid and begins with rgba(.
      return this.authored;
    }
    let components = this._getRGBATuple();
    return "rgba(" + components.r + ", " +
                     components.g + ", " +
                     components.b + ", " +
                     components.a + ")";
  },

  get hsl() {
    let invalidOrSpecialValue = this._getInvalidOrSpecialValue();
    if (invalidOrSpecialValue !== false) {
      return invalidOrSpecialValue;
    }
    if (this.lowerCased.startsWith("hsl(")) {
      // The color is valid and begins with hsl(.
      return this.authored;
    }
    if (this.hasAlpha) {
      return this.hsla;
    }
    return this._hsl();
  },

  get hsla() {
    let invalidOrSpecialValue = this._getInvalidOrSpecialValue();
    if (invalidOrSpecialValue !== false) {
      return invalidOrSpecialValue;
    }
    if (this.lowerCased.startsWith("hsla(")) {
      // The color is valid and begins with hsla(.
      return this.authored;
    }
    if (this.hasAlpha) {
      let a = this._getRGBATuple().a;
      return this._hsl(a);
    }
    return this._hsl(1);
  },

  /**
   * Check whether the current color value is in the special list e.g.
   * transparent or invalid.
   *
   * @return {String|Boolean}
   *         - If the current color is a special value e.g. "transparent" then
   *           return the color.
   *         - If the color is invalid return an empty string.
   *         - If the color is a regular color e.g. #F06 so we return false
   *           to indicate that the color is neither invalid or special.
   */
  _getInvalidOrSpecialValue: function () {
    if (this.specialValue) {
      return this.specialValue;
    }
    if (!this.valid) {
      return "";
    }
    return false;
  },

  /**
   * Change color
   *
   * @param  {String} color
   *         Any valid color string
   */
  newColor: function (color) {
    // Store a lower-cased version of the color to help with format
    // testing.  The original text is kept as well so it can be
    // returned when needed.
    this.lowerCased = color.toLowerCase();
    this.authored = color;
    this._setColorUnitUppercase(color);
    return this;
  },

  nextColorUnit: function () {
    // Reorder the formats array to have the current format at the
    // front so we can cycle through.
    let formats = ["hex", "hsl", "rgb", "name"];
    let currentFormat = classifyColor(this.toString());
    let putOnEnd = formats.splice(0, formats.indexOf(currentFormat));
    formats = formats.concat(putOnEnd);
    let currentDisplayedColor = this[formats[0]];

    for (let format of formats) {
      if (this[format].toLowerCase() !== currentDisplayedColor.toLowerCase()) {
        this.colorUnit = CssColor.COLORUNIT[format];
        break;
      }
    }

    return this.toString();
  },

  /**
   * Return a string representing a color of type defined in COLOR_UNIT_PREF.
   */
  toString: function () {
    let color;

    switch (this.colorUnit) {
      case CssColor.COLORUNIT.authored:
        color = this.authored;
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
      default:
        color = this.rgb;
    }

    if (this._colorUnitUppercase &&
        this.colorUnit != CssColor.COLORUNIT.authored) {
      color = color.toUpperCase();
    }

    return color;
  },

  /**
   * Returns a RGBA 4-Tuple representation of a color or transparent as
   * appropriate.
   */
  _getRGBATuple: function () {
    let tuple = colorToRGBA(this.authored, this.cssColor4);

    tuple.a = parseFloat(tuple.a.toFixed(1));

    return tuple;
  },

  _hsl: function (maybeAlpha) {
    if (this.lowerCased.startsWith("hsl(") && maybeAlpha === undefined) {
      // We can use it as-is.
      return this.authored;
    }

    let {r, g, b} = this._getRGBATuple();
    let [h, s, l] = rgbToHsl([r, g, b]);
    if (maybeAlpha !== undefined) {
      return "hsla(" + h + ", " + s + "%, " + l + "%, " + maybeAlpha + ")";
    }
    return "hsl(" + h + ", " + s + "%, " + l + "%)";
  },

  /**
   * This method allows comparison of CssColor objects using ===.
   */
  valueOf: function () {
    return this.rgba;
  },
};

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

  let max = Math.max(r, g, b);
  let min = Math.min(r, g, b);
  let h;
  let s;
  let l = (max + min) / 2;

  if (max == min) {
    h = s = 0;
  } else {
    let d = max - min;
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

function roundTo(number, digits) {
  const multiplier = Math.pow(10, digits);
  return Math.round(number * multiplier) / multiplier;
}

/**
 * Takes a color value of any type (hex, hsl, hsla, rgb, rgba)
 * and an alpha value to generate an rgba string with the correct
 * alpha value.
 *
 * @param  {String} colorValue
 *         Color in the form of hex, hsl, hsla, rgb, rgba.
 * @param  {Number} alpha
 *         Alpha value for the color, between 0 and 1.
 * @param  {Boolean} useCssColor4ColorFunction
 *         use css-color-4 color function or not.
 * @return {String}
 *         Converted color with `alpha` value in rgba form.
 */
function setAlpha(colorValue, alpha, useCssColor4ColorFunction = false) {
  let color = new CssColor(colorValue, useCssColor4ColorFunction);

  // Throw if the color supplied is not valid.
  if (!color.valid) {
    throw new Error("Invalid color.");
  }

  // If an invalid alpha valid, just set to 1.
  if (!(alpha >= 0 && alpha <= 1)) {
    alpha = 1;
  }

  let { r, g, b } = color._getRGBATuple();
  return "rgba(" + r + ", " + g + ", " + b + ", " + alpha + ")";
}

/**
 * Given a color, classify its type as one of the possible color
 * units, as known by |CssColor.colorUnit|.
 *
 * @param  {String} value
 *         The color, in any form accepted by CSS.
 * @return {String}
 *         The color classification, one of "rgb", "hsl", "hex", or "name".
 */
function classifyColor(value) {
  value = value.toLowerCase();
  if (value.startsWith("rgb(") || value.startsWith("rgba(")) {
    return CssColor.COLORUNIT.rgb;
  } else if (value.startsWith("hsl(") || value.startsWith("hsla(")) {
    return CssColor.COLORUNIT.hsl;
  } else if (/^#[0-9a-f]+$/.exec(value)) {
    return CssColor.COLORUNIT.hex;
  }
  return CssColor.COLORUNIT.name;
}

// This holds a map from colors back to color names for use by
// rgbToColorName.
var cssRGBMap;

/**
 * Given a color, return its name, if it has one.  Throws an exception
 * if the color does not have a name.
 *
 * @param {Number} r, g, b  The color components.
 * @return {String} the name of the color
 */
function rgbToColorName(r, g, b) {
  if (!cssRGBMap) {
    cssRGBMap = {};
    for (let name in cssColors) {
      let key = JSON.stringify(cssColors[name]);
      if (!(key in cssRGBMap)) {
        cssRGBMap[key] = name;
      }
    }
  }
  let value = cssRGBMap[JSON.stringify([r, g, b, 1])];
  if (!value) {
    throw new Error("no such color");
  }
  return value;
}

// Translated from nsColor.cpp.
function _hslValue(m1, m2, h) {
  if (h < 0.0) {
    h += 1.0;
  }
  if (h > 1.0) {
    h -= 1.0;
  }
  if (h < 1.0 / 6.0) {
    return m1 + (m2 - m1) * h * 6.0;
  }
  if (h < 1.0 / 2.0) {
    return m2;
  }
  if (h < 2.0 / 3.0) {
    return m1 + (m2 - m1) * (2.0 / 3.0 - h) * 6.0;
  }
  return m1;
}

// Translated from nsColor.cpp.  All three values are expected to be
// in the range 0-1.
function hslToRGB([h, s, l]) {
  let r, g, b;
  let m1, m2;
  if (l <= 0.5) {
    m2 = l * (s + 1);
  } else {
    m2 = l + s - l * s;
  }
  m1 = l * 2 - m2;
  r = Math.round(255 * _hslValue(m1, m2, h + 1.0 / 3.0));
  g = Math.round(255 * _hslValue(m1, m2, h));
  b = Math.round(255 * _hslValue(m1, m2, h - 1.0 / 3.0));
  return [r, g, b];
}

/**
 * A helper function to convert a hex string like "F0C" or "F0C8" to a color.
 *
 * @param {String} name the color string
 * @return {Object} an object of the form {r, g, b, a}; or null if the
 *         name was not a valid color
 */
function hexToRGBA(name) {
  let r, g, b, a = 1;

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
    a = parseInt(name.charAt(3) + name.charAt(3), 16) / 255;
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
    a = parseInt(name.charAt(6) + name.charAt(7), 16) / 255;
  } else {
    return null;
  }
  a = Math.round(a * 10) / 10;
  return {r, g, b, a};
}

/**
 * A helper function to clamp a value.
 *
 * @param {Number} value The value to clamp
 * @param {Number} min The minimum value
 * @param {Number} max The maximum value
 * @return {Number} A value between min and max
 */
function clamp(value, min, max) {
  if (value < min) {
    value = min;
  }
  if (value > max) {
    value = max;
  }
  return value;
}

/**
 * A helper function to get a token from a lexer, skipping comments
 * and whitespace.
 *
 * @param {CSSLexer} lexer The lexer
 * @return {CSSToken} The next non-whitespace, non-comment token; or
 * null at EOF.
 */
function getToken(lexer) {
  if (lexer._hasPushBackToken) {
    lexer._hasPushBackToken = false;
    return lexer._currentToken;
  }

  while (true) {
    let token = lexer.nextToken();
    if (!token || (token.tokenType !== "comment" &&
                   token.tokenType !== "whitespace")) {
      lexer._currentToken = token;
      return token;
    }
  }
}

/**
 * A helper function to put a token back to lexer for the next call of
 * getToken().
 *
 * @param {CSSLexer} lexer The lexer
 */
function unGetToken(lexer) {
  if (lexer._hasPushBackToken) {
    throw new Error("Double pushback.");
  }
  lexer._hasPushBackToken = true;
}

/**
 * A helper function that checks if the next token matches symbol.
 * If so, reads the token and returns true.  If not, pushes the
 * token back and returns false.
 *
 * @param {CSSLexer} lexer The lexer.
 * @param {String} symbol The symbol.
 * @return {Boolean} The expect symbol is parsed or not.
 */
function expectSymbol(lexer, symbol) {
  let token = getToken(lexer);
  if (!token) {
    return false;
  }

  if (token.tokenType !== "symbol" || token.text !== symbol) {
    unGetToken(lexer);
    return false;
  }

  return true;
}

const COLOR_COMPONENT_TYPE = {
  "integer": "integer",
  "number": "number",
  "percentage": "percentage",
};

/**
 * Parse a <integer> or a <number> or a <percentage> color component. If
 * |separator| is provided (not an empty string ""), this function will also
 * attempt to parse that character after parsing the color component. The range
 * of output component value is [0, 1] if the component type is percentage.
 * Otherwise, the range is [0, 255].
 *
 * @param {CSSLexer} lexer The lexer.
 * @param {COLOR_COMPONENT_TYPE} type The color component type.
 * @param {String} separator The separator.
 * @param {Array} colorArray [out] The parsed color component will push into this array.
 * @return {Boolean} Return false on error.
 */
function parseColorComponent(lexer, type, separator, colorArray) {
  let token = getToken(lexer);

  if (!token) {
    return false;
  }

  switch (type) {
    case COLOR_COMPONENT_TYPE.integer:
      if (token.tokenType !== "number" || !token.isInteger) {
        return false;
      }
      break;
    case COLOR_COMPONENT_TYPE.number:
      if (token.tokenType !== "number") {
        return false;
      }
      break;
    case COLOR_COMPONENT_TYPE.percentage:
      if (token.tokenType !== "percentage") {
        return false;
      }
      break;
    default:
      throw new Error("Invalid color component type.");
  }

  let colorComponent = 0;
  if (type === COLOR_COMPONENT_TYPE.percentage) {
    colorComponent = clamp(token.number, 0, 1);
  } else {
    colorComponent = clamp(token.number, 0, 255);
  }

  if (separator !== "" && !expectSymbol(lexer, separator)) {
    return false;
  }

  colorArray.push(colorComponent);

  return true;
}

/**
 * Parse an optional [ separator <alpha-value> ] expression, followed by a
 * close-parenthesis, at the end of a css color function (e.g. rgba() or hsla()).
 * If this function simply encounters a close-parenthesis (without the
 * [ separator <alpha-value> ]), it will still succeed. Then put a fully-opaque
 * alpha value into the colorArray. The range of output alpha value is [0, 1].
 *
 * @param {CSSLexer} lexer The lexer
 * @param {String} separator The separator.
 * @param {Array} colorArray [out] The parsed color component will push into this array.
 * @return {Boolean} Return false on error.
 */
function parseColorOpacityAndCloseParen(lexer, separator, colorArray) {
  // The optional [separator <alpha-value>] was omitted, so set the opacity
  // to a fully-opaque value '1.0' and return success.
  if (expectSymbol(lexer, ")")) {
    colorArray.push(1);
    return true;
  }

  if (!expectSymbol(lexer, separator)) {
    return false;
  }

  let token = getToken(lexer);
  if (!token) {
    return false;
  }

  // <number> or <percentage>
  if (token.tokenType !== "number" && token.tokenType !== "percentage") {
    return false;
  }

  if (!expectSymbol(lexer, ")")) {
    return false;
  }

  colorArray.push(clamp(token.number, 0, 1));

  return true;
}

/**
 * Parse a hue value.
 *   <hue> = <number> | <angle>
 *
 * @param {CSSLexer} lexer The lexer
 * @param {Array} colorArray [out] The parsed color component will push into this array.
 * @return {Boolean} Return false on error.
 */
function parseHue(lexer, colorArray) {
  let token = getToken(lexer);

  if (!token) {
    return false;
  }

  let val = 0;
  if (token.tokenType === "number") {
    val = token.number;
  } else if (token.tokenType === "dimension" && token.text in CSS_ANGLEUNIT) {
    val = getAngleValueInDegrees(token.number, token.text);
  } else {
    return false;
  }

  val = val / 360.0;
  colorArray.push(val - Math.floor(val));

  return true;
}

/**
 * A helper function to parse the color components of hsl()/hsla() function.
 * hsl() and hsla() are now aliases.
 *
 * @param {CSSLexer} lexer The lexer
 * @return {Array} An array of the form [r,g,b,a]; or null on error.
 */
function parseHsl(lexer) {
  // comma-less expression:
  // hsl() = hsl( <hue> <saturation> <lightness> [ / <alpha-value> ]? )
  // the expression with comma:
  // hsl() = hsl( <hue>, <saturation>, <lightness>, <alpha-value>? )
  //
  // <hue> = <number> | <angle>
  // <alpha-value> = <number> | <percentage>

  const commaSeparator = ",";
  let hsl = [];
  let a = [];

  // Parse hue.
  if (!parseHue(lexer, hsl)) {
    return null;
  }

  // Look for a comma separator after "hue" component to determine if the
  // expression is comma-less or not.
  let hasComma = expectSymbol(lexer, commaSeparator);

  // Parse saturation, lightness and opacity.
  // The saturation and lightness are <percentage>, so reuse the <percentage>
  // version of parseColorComponent function for them. No need to check the
  // separator after 'lightness'. It will be checked in opacity value parsing.
  let separatorBeforeAlpha = hasComma ? commaSeparator : "/";
  if (parseColorComponent(lexer, COLOR_COMPONENT_TYPE.percentage,
                          hasComma ? commaSeparator : "", hsl) &&
      parseColorComponent(lexer, COLOR_COMPONENT_TYPE.percentage, "", hsl) &&
      parseColorOpacityAndCloseParen(lexer, separatorBeforeAlpha, a)) {
    return [...hslToRGB(hsl), ...a];
  }

  return null;
}

/**
 * A helper function to parse the color arguments of old style hsl()/hsla()
 * function.
 *
 * @param {CSSLexer} lexer The lexer.
 * @param {Boolean} hasAlpha The color function has alpha component or not.
 * @return {Array} An array of the form [r,g,b,a]; or null on error.
 */
function parseOldStyleHsl(lexer, hasAlpha) {
  // hsla() = hsla( <hue>, <saturation>, <lightness>, <alpha-value> )
  // hsl() = hsl( <hue>, <saturation>, <lightness> )
  //
  // <hue> = <number>
  // <alpha-value> = <number>

  const commaSeparator = ",";
  const closeParen = ")";
  let hsl = [];
  let a = [];

  // Parse hue.
  let token = getToken(lexer);
  if (!token || token.tokenType !== "number") {
    return null;
  }
  if (!expectSymbol(lexer, commaSeparator)) {
    return null;
  }
  let val = token.number / 360.0;
  hsl.push(val - Math.floor(val));

  // Parse saturation, lightness and opacity.
  // The saturation and lightness are <percentage>, so reuse the <percentage>
  // version of parseColorComponent function for them. The opacity is <number>
  if (hasAlpha) {
    if (parseColorComponent(lexer, COLOR_COMPONENT_TYPE.percentage,
                            commaSeparator, hsl) &&
        parseColorComponent(lexer, COLOR_COMPONENT_TYPE.percentage,
                            commaSeparator, hsl) &&
        parseColorComponent(lexer, COLOR_COMPONENT_TYPE.number,
                            closeParen, a)) {
      return [...hslToRGB(hsl), ...a];
    }
  } else if (parseColorComponent(lexer, COLOR_COMPONENT_TYPE.percentage,
                                 commaSeparator, hsl) &&
             parseColorComponent(lexer, COLOR_COMPONENT_TYPE.percentage,
                                 closeParen, hsl)) {
    return [...hslToRGB(hsl), 1];
  }

  return null;
}

/**
 * A helper function to parse the color arguments of rgb()/rgba() function.
 * rgb() and rgba() now are aliases.
 *
 * @param {CSSLexer} lexer The lexer.
 * @return {Array} An array of the form [r,g,b,a]; or null on error.
 */
function parseRgb(lexer) {
  // comma-less expression:
  //   rgb() = rgb( component{3} [ / <alpha-value> ]? )
  // the expression with comma:
  //   rgb() = rgb( component#{3} , <alpha-value>? )
  //
  // component = <number> | <percentage>
  // <alpa-value> = <number> | <percentage>

  const commaSeparator = ",";
  let rgba = [];

  let token = getToken(lexer);
  if (token.tokenType !== "percentage" && token.tokenType !== "number") {
    return null;
  }
  unGetToken(lexer);
  let type = (token.tokenType === "percentage") ?
             COLOR_COMPONENT_TYPE.percentage :
             COLOR_COMPONENT_TYPE.number;

  // Parse R.
  if (!parseColorComponent(lexer, type, "", rgba)) {
    return null;
  }
  let hasComma = expectSymbol(lexer, commaSeparator);

  // Parse G, B and A.
  // No need to check the separator after 'B'. It will be checked in 'A' values
  // parsing.
  let separatorBeforeAlpha = hasComma ? commaSeparator : "/";
  if (parseColorComponent(lexer, type, hasComma ? commaSeparator : "", rgba) &&
      parseColorComponent(lexer, type, "", rgba) &&
      parseColorOpacityAndCloseParen(lexer, separatorBeforeAlpha, rgba)) {
    if (type === COLOR_COMPONENT_TYPE.percentage) {
      rgba[0] = Math.round(255 * rgba[0]);
      rgba[1] = Math.round(255 * rgba[1]);
      rgba[2] = Math.round(255 * rgba[2]);
    }
    return rgba;
  }

  return null;
}

/**
 * A helper function to parse the color arguments of old style rgb()/rgba()
 * function.
 *
 * @param {CSSLexer} lexer The lexer.
 * @param {Boolean} hasAlpha The color function has alpha component or not.
 * @return {Array} An array of the form [r,g,b,a]; or null on error.
 */
function parseOldStyleRgb(lexer, hasAlpha) {
  // rgba() = rgba( component#{3} , <alpha-value> )
  // rgb() = rgb( component#{3} )
  //
  // component = <integer> | <percentage>
  // <alpha-value> = <number>

  const commaSeparator = ",";
  const closeParen = ")";
  let rgba = [];

  let token = getToken(lexer);
  if (token.tokenType !== "percentage" &&
      (token.tokenType !== "number" || !token.isInteger)) {
    return null;
  }
  unGetToken(lexer);
  let type = (token.tokenType === "percentage") ?
             COLOR_COMPONENT_TYPE.percentage :
             COLOR_COMPONENT_TYPE.integer;

  // Parse R. G, B and A.
  if (hasAlpha) {
    if (!parseColorComponent(lexer, type, commaSeparator, rgba) ||
        !parseColorComponent(lexer, type, commaSeparator, rgba) ||
        !parseColorComponent(lexer, type, commaSeparator, rgba) ||
        !parseColorComponent(lexer, COLOR_COMPONENT_TYPE.number,
                             closeParen, rgba)) {
      return null;
    }
  } else if (!parseColorComponent(lexer, type, commaSeparator, rgba) ||
             !parseColorComponent(lexer, type, commaSeparator, rgba) ||
             !parseColorComponent(lexer, type, closeParen, rgba)) {
    return null;
  }

  if (type === COLOR_COMPONENT_TYPE.percentage) {
    rgba[0] = Math.round(255 * rgba[0]);
    rgba[1] = Math.round(255 * rgba[1]);
    rgba[2] = Math.round(255 * rgba[2]);
  }
  if (!hasAlpha) {
    rgba.push(1);
  }

  return rgba;
}

/**
 * Convert a string representing a color to an object holding the
 * color's components.  Any valid CSS color form can be passed in.
 *
 * @param {String} name the color
 * @param {Boolean} useCssColor4ColorFunction use css-color-4 color function or not.
 * @return {Object} an object of the form {r, g, b, a}; or null if the
 *         name was not a valid color
 */
function colorToRGBA(name, useCssColor4ColorFunction = false) {
  name = name.trim().toLowerCase();

  if (name in cssColors) {
    let result = cssColors[name];
    return {r: result[0], g: result[1], b: result[2], a: result[3]};
  } else if (name === "transparent") {
    return {r: 0, g: 0, b: 0, a: 0};
  } else if (name === "currentcolor") {
    return {r: 0, g: 0, b: 0, a: 1};
  }

  let lexer = getCSSLexer(name);

  let func = getToken(lexer);
  if (!func) {
    return null;
  }

  if (func.tokenType === "id" || func.tokenType === "hash") {
    if (getToken(lexer) !== null) {
      return null;
    }
    return hexToRGBA(func.text);
  }

  const expectedFunctions = ["rgba", "rgb", "hsla", "hsl"];
  if (!func || func.tokenType !== "function" ||
      !expectedFunctions.includes(func.text)) {
    return null;
  }

  let hsl = func.text === "hsl" || func.text === "hsla";

  let vals;
  if (!useCssColor4ColorFunction) {
    let hasAlpha = (func.text === "rgba" || func.text === "hsla");
    vals = hsl ? parseOldStyleHsl(lexer, hasAlpha) : parseOldStyleRgb(lexer, hasAlpha);
  } else {
    vals = hsl ? parseHsl(lexer) : parseRgb(lexer);
  }

  if (!vals) {
    return null;
  }
  if (getToken(lexer) !== null) {
    return null;
  }

  return {r: vals[0], g: vals[1], b: vals[2], a: vals[3]};
}

/**
 * Check whether a string names a valid CSS color.
 *
 * @param {String} name The string to check
 * @param {Boolean} useCssColor4ColorFunction use css-color-4 color function or not.
 * @return {Boolean} True if the string is a CSS color name.
 */
function isValidCSSColor(name, useCssColor4ColorFunction = false) {
  return colorToRGBA(name, useCssColor4ColorFunction) !== null;
}
