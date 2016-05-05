/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci} = require("chrome");

const SPECIALVALUES = new Set([
  "initial",
  "inherit",
  "unset"
]);

/**
 * This module is used to convert between various angle units.
 *
 * Usage:
 *   let {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
 *   let {angleUtils} = require("devtools/client/shared/css-angle");
 *   let angle = new angleUtils.CssAngle("180deg");
 *
 *   angle.authored === "180deg"
 *   angle.valid === true
 *   angle.rad === "3,14rad"
 *   angle.grad === "200grad"
 *   angle.turn === "0.5turn"
 *
 *   angle.toString() === "180deg"; // Outputs the angle value and its unit
 *   // Angle objects can be reused
 *   angle.newAngle("-1TURN") === "-1TURN"; // true
 */

function CssAngle(angleValue) {
  this.newAngle(angleValue);
}

module.exports.angleUtils = {
  CssAngle: CssAngle,
  classifyAngle: classifyAngle
};

CssAngle.ANGLEUNIT = {
  "deg": "deg",
  "rad": "rad",
  "grad": "grad",
  "turn": "turn"
};

CssAngle.prototype = {
  _angleUnit: null,
  _angleUnitUppercase: false,

  // The value as-authored.
  authored: null,
  // A lower-cased copy of |authored|.
  lowerCased: null,

  get angleUnit() {
    if (this._angleUnit === null) {
      this._angleUnit = classifyAngle(this.authored);
    }
    return this._angleUnit;
  },

  set angleUnit(unit) {
    this._angleUnit = unit;
  },

  get valid() {
    return /^-?\d+\.?\d*(deg|rad|grad|turn)$/gi.test(this.authored);
  },

  get specialValue() {
    return SPECIALVALUES.has(this.lowerCased) ? this.authored : null;
  },

  get deg() {
    let invalidOrSpecialValue = this._getInvalidOrSpecialValue();
    if (invalidOrSpecialValue !== false) {
      return invalidOrSpecialValue;
    }

    let angleUnit = classifyAngle(this.authored);
    if (angleUnit === CssAngle.ANGLEUNIT.deg) {
      // The angle is valid and is in degree.
      return this.authored;
    }

    let degValue;
    if (angleUnit === CssAngle.ANGLEUNIT.rad) {
      // The angle is valid and is in radian.
      degValue = this.authoredAngleValue / (Math.PI / 180);
    }

    if (angleUnit === CssAngle.ANGLEUNIT.grad) {
      // The angle is valid and is in gradian.
      degValue = this.authoredAngleValue * 0.9;
    }

    if (angleUnit === CssAngle.ANGLEUNIT.turn) {
      // The angle is valid and is in turn.
      degValue = this.authoredAngleValue * 360;
    }

    let unitStr = CssAngle.ANGLEUNIT.deg;
    if (this._angleUnitUppercase === true) {
      unitStr = unitStr.toUpperCase();
    }
    return `${Math.round(degValue * 100) / 100}${unitStr}`;
  },

  get rad() {
    let invalidOrSpecialValue = this._getInvalidOrSpecialValue();
    if (invalidOrSpecialValue !== false) {
      return invalidOrSpecialValue;
    }

    let unit = classifyAngle(this.authored);
    if (unit === CssAngle.ANGLEUNIT.rad) {
      // The angle is valid and is in radian.
      return this.authored;
    }

    let radValue;
    if (unit === CssAngle.ANGLEUNIT.deg) {
      // The angle is valid and is in degree.
      radValue = this.authoredAngleValue * (Math.PI / 180);
    }

    if (unit === CssAngle.ANGLEUNIT.grad) {
      // The angle is valid and is in gradian.
      radValue = this.authoredAngleValue * 0.9 * (Math.PI / 180);
    }

    if (unit === CssAngle.ANGLEUNIT.turn) {
      // The angle is valid and is in turn.
      radValue = this.authoredAngleValue * 360 * (Math.PI / 180);
    }

    let unitStr = CssAngle.ANGLEUNIT.rad;
    if (this._angleUnitUppercase === true) {
      unitStr = unitStr.toUpperCase();
    }
    return `${Math.round(radValue * 10000) / 10000}${unitStr}`;
  },

  get grad() {
    let invalidOrSpecialValue = this._getInvalidOrSpecialValue();
    if (invalidOrSpecialValue !== false) {
      return invalidOrSpecialValue;
    }

    let unit = classifyAngle(this.authored);
    if (unit === CssAngle.ANGLEUNIT.grad) {
      // The angle is valid and is in gradian
      return this.authored;
    }

    let gradValue;
    if (unit === CssAngle.ANGLEUNIT.deg) {
      // The angle is valid and is in degree
      gradValue = this.authoredAngleValue / 0.9;
    }

    if (unit === CssAngle.ANGLEUNIT.rad) {
      // The angle is valid and is in radian
      gradValue = this.authoredAngleValue / 0.9 / (Math.PI / 180);
    }

    if (unit === CssAngle.ANGLEUNIT.turn) {
      // The angle is valid and is in turn
      gradValue = this.authoredAngleValue * 400;
    }

    let unitStr = CssAngle.ANGLEUNIT.grad;
    if (this._angleUnitUppercase === true) {
      unitStr = unitStr.toUpperCase();
    }
    return `${Math.round(gradValue * 100) / 100}${unitStr}`;
  },

  get turn() {
    let invalidOrSpecialValue = this._getInvalidOrSpecialValue();
    if (invalidOrSpecialValue !== false) {
      return invalidOrSpecialValue;
    }

    let unit = classifyAngle(this.authored);
    if (unit === CssAngle.ANGLEUNIT.turn) {
      // The angle is valid and is in turn
      return this.authored;
    }

    let turnValue;
    if (unit === CssAngle.ANGLEUNIT.deg) {
      // The angle is valid and is in degree
      turnValue = this.authoredAngleValue / 360;
    }

    if (unit === CssAngle.ANGLEUNIT.rad) {
      // The angle is valid and is in radian
      turnValue = (this.authoredAngleValue / (Math.PI / 180)) / 360;
    }

    if (unit === CssAngle.ANGLEUNIT.grad) {
      // The angle is valid and is in gradian
      turnValue = this.authoredAngleValue / 400;
    }

    let unitStr = CssAngle.ANGLEUNIT.turn;
    if (this._angleUnitUppercase === true) {
      unitStr = unitStr.toUpperCase();
    }
    return `${Math.round(turnValue * 100) / 100}${unitStr}`;
  },

  /**
   * Check whether the angle value is in the special list e.g.
   * inherit or invalid.
   *
   * @return {String|Boolean}
   *         - If the current angle is a special value e.g. "inherit" then
   *           return the angle.
   *         - If the angle is invalid return an empty string.
   *         - If the angle is a regular angle e.g. 90deg so we return false
   *           to indicate that the angle is neither invalid nor special.
   */
  _getInvalidOrSpecialValue: function() {
    if (this.specialValue) {
      return this.specialValue;
    }
    if (!this.valid) {
      return "";
    }
    return false;
  },

  /**
   * Change angle
   *
   * @param  {String} angle
   *         Any valid angle value + unit string
   */
  newAngle: function(angle) {
    // Store a lower-cased version of the angle to help with format
    // testing.  The original text is kept as well so it can be
    // returned when needed.
    this.lowerCased = angle.toLowerCase();
    this._angleUnitUppercase = (angle === angle.toUpperCase());
    this.authored = angle;

    let reg = new RegExp(
      `(${Object.keys(CssAngle.ANGLEUNIT).join("|")})$`, "i");
    let unitStartIdx = angle.search(reg);
    this.authoredAngleValue = angle.substring(0, unitStartIdx);
    this.authoredAngleUnit = angle.substring(unitStartIdx, angle.length);

    return this;
  },

  nextAngleUnit: function() {
    // Get a reordered array from the formats object
    // to have the current format at the front so we can cycle through.
    let formats = Object.keys(CssAngle.ANGLEUNIT);
    let putOnEnd = formats.splice(0, formats.indexOf(this.angleUnit));
    formats = formats.concat(putOnEnd);
    let currentDisplayedValue = this[formats[0]];

    for (let format of formats) {
      if (this[format].toLowerCase() !== currentDisplayedValue.toLowerCase()) {
        this.angleUnit = CssAngle.ANGLEUNIT[format];
        break;
      }
    }
    return this.toString();
  },

  /**
   * Return a string representing a angle
   */
  toString: function() {
    let angle;

    switch (this.angleUnit) {
      case CssAngle.ANGLEUNIT.deg:
        angle = this.deg;
        break;
      case CssAngle.ANGLEUNIT.rad:
        angle = this.rad;
        break;
      case CssAngle.ANGLEUNIT.grad:
        angle = this.grad;
        break;
      case CssAngle.ANGLEUNIT.turn:
        angle = this.turn;
        break;
      default:
        angle = this.deg;
    }

    if (this._angleUnitUppercase &&
        this.angleUnit != CssAngle.ANGLEUNIT.authored) {
      angle = angle.toUpperCase();
    }
    return angle;
  },

  /**
   * This method allows comparison of CssAngle objects using ===.
   */
  valueOf: function() {
    return this.deg;
  },
};

/**
 * Given a color, classify its type as one of the possible angle
 * units, as known by |CssAngle.angleUnit|.
 *
 * @param  {String} value
 *         The angle, in any form accepted by CSS.
 * @return {String}
 *         The angle classification, one of "deg", "rad", "grad", or "turn".
 */
function classifyAngle(value) {
  value = value.toLowerCase();
  if (value.endsWith("deg")) {
    return CssAngle.ANGLEUNIT.deg;
  }

  if (value.endsWith("grad")) {
    return CssAngle.ANGLEUNIT.grad;
  }

  if (value.endsWith("rad")) {
    return CssAngle.ANGLEUNIT.rad;
  }
  if (value.endsWith("turn")) {
    return CssAngle.ANGLEUNIT.turn;
  }

  return CssAngle.ANGLEUNIT.deg;
}
