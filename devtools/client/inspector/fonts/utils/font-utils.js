/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = {
  /**
   * Given a CSS unit type, get the amount by which to increment a numeric value.
   * Used as the step attribute in inputs of type "range" or "number".
   *
   * @param {String} unit
   *        CSS unit type (px, %, em, rem, vh, vw, ...)
   * @return {Number}
   *         Amount by which to increment.
   */
  getStepForUnit(unit) {
    let step = 1;
    if (unit === "em" || unit === "rem") {
      step = 0.1;
    }

    return step;
  },

  /**
   * Get the unit type from the end of a CSS value string.
   * Returns null for non-string input or unitless values.
   *
   * @param {String} value
   *        CSS value string.
   * @return {String|null}
   *         CSS unit type, like "px", "em", "rem", etc or null.
   */
  getUnitFromValue(value) {
    if (typeof value !== "string") {
      return null;
    }

    const match = value.match(/\D+?$/);
    return match && match.length ? match[0] : null;
  },
};
