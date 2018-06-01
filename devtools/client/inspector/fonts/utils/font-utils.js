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

  /**
   * Parse the string value of CSS font-variation-settings into an object with
   * axis tag names and corresponding values. If the string is a keyword or does not
   * contain axes, return an empty object.
   *
   * @param {String} string
   *        Value of font-variation-settings property coming from node's computed style.
   *        Its contents are expected to be stable having been already parsed by the
   *        browser.
   * @return {Object}
   */
  parseFontVariationAxes(string) {
    let axes = {};
    const keywords = ["initial", "normal", "inherit", "unset"];

    if (keywords.includes(string.trim())) {
      return axes;
    }

    // Parse font-variation-settings CSS declaration into an object
    // with axis tags as keys and axis values as values.
    axes = string
      .split(",")
      .reduce((acc, pair) => {
        // Tags are always in quotes. Split by quote and filter excessive whitespace.
        pair = pair.split(/["']/).filter(part => part.trim() !== "");
        const tag = pair[0].trim();
        const value = pair[1].trim();
        acc[tag] = value;
        return acc;
      }, {});

    return axes;
  }
};
