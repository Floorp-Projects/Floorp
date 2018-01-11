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
  interfaces: Ci,
  utils: Cu
} = Components;

Cu.importGlobalProperties(["InspectorUtils"]);

function ValueExtractor(aConsole, aBundle) {
  this.console = aConsole;
  this.domBundle = aBundle;
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
        this.console.warn(this.domBundle.formatStringFromName("ManifestInvalidType",
                                                              [objectName, property, expectedType],
                                                              3));
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
    let color;
    if (InspectorUtils.isValidCSSColor(value)) {
      color = value;
    } else if (value) {
      this.console.warn(this.domBundle.formatStringFromName("ManifestInvalidCSSColor",
                                                            [spec.property, value],
                                                            2));
    }
    return color;
  }
};
this.ValueExtractor = ValueExtractor; // jshint ignore:line
this.EXPORTED_SYMBOLS = ['ValueExtractor']; // jshint ignore:line
