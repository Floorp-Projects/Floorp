/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/*
 * Helper functions extract values from manifest members
 * and reports conformance violations.
 */
/*globals Components*/
'use strict';
const {
  classes: Cc,
  interfaces: Ci
} = Components;

function ValueExtractor(aConsole) {
  this.console = aConsole;
}

ValueExtractor.prototype = {
  // This function takes a 'spec' object and destructures
  // it to extract a value. If the value is of th wrong type, it
  // warns the developer and returns undefined.
  //  expectType: is the type of a JS primitive (string, number, etc.)
  //  object: is the object from which to extract the value.
  //  objectName: string used to construct the developer warning.
  //  property: the name of the property being extracted.
  //  trim: boolean, if the value should be trimmed (used by string type).
  extractValue({expectedType, object, objectName, property, trim}) {
    const value = object[property];
    const isArray = Array.isArray(value);
    // We need to special-case "array", as it's not a JS primitive.
    const type = (isArray) ? 'array' : typeof value;
    if (type !== expectedType) {
      if (type !== 'undefined') {
        let msg = `Expected the ${objectName}'s ${property} `;
        msg += `member to be a ${expectedType}.`;
        this.console.log(msg);
      }
      return undefined;
    }
    // Trim string and returned undefined if the empty string.
    const shouldTrim = expectedType === 'string' && value && trim;
    if (shouldTrim) {
      return value.trim() || undefined;
    }
    return value;
  },
  extractColorValue(spec) {
    const value = this.extractValue(spec);
    const DOMUtils = Cc['@mozilla.org/inspector/dom-utils;1']
      .getService(Ci.inIDOMUtils);
    let color;
    if (DOMUtils.isValidCSSColor(value)) {
      color = value;
    } else if (value) {
      const msg = `background_color: ${value} is not a valid CSS color.`;
      this.console.warn(msg);
    }
    return color;
  }
};
this.ValueExtractor = ValueExtractor; // jshint ignore:line
this.EXPORTED_SYMBOLS = ['ValueExtractor']; // jshint ignore:line
