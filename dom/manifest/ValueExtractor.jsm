/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/*
 * Helper functions extract values from manifest members
 * and reports conformance errors.
 */
"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["InspectorUtils"]);

function ValueExtractor(errors, aBundle) {
  this.errors = errors;
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
  //  throwTypeError: boolean, throw a TypeError if the type is incorrect.
  extractValue(options) {
    const {
      expectedType,
      object,
      objectName,
      property,
      throwTypeError,
      trim,
    } = options;
    const value = object[property];
    const isArray = Array.isArray(value);
    // We need to special-case "array", as it's not a JS primitive.
    const type = isArray ? "array" : typeof value;
    if (type !== expectedType) {
      if (type !== "undefined") {
        const warn = this.domBundle.formatStringFromName(
          "ManifestInvalidType",
          [objectName, property, expectedType]
        );
        this.errors.push({ warn });
        if (throwTypeError) {
          throw new TypeError(warn);
        }
      }
      return undefined;
    }
    // Trim string and returned undefined if the empty string.
    const shouldTrim = expectedType === "string" && value && trim;
    if (shouldTrim) {
      return value.trim() || undefined;
    }
    return value;
  },
  extractColorValue(spec) {
    const value = this.extractValue(spec);
    let color;
    if (InspectorUtils.isValidCSSColor(value)) {
      const rgba = InspectorUtils.colorToRGBA(value);
      color = "#" + ((rgba.r << 16) | (rgba.g << 8) | rgba.b).toString(16);
    } else if (value) {
      const warn = this.domBundle.formatStringFromName(
        "ManifestInvalidCSSColor",
        [spec.property, value]
      );
      this.errors.push({ warn });
    }
    return color;
  },
  extractLanguageValue(spec) {
    let langTag;
    const value = this.extractValue(spec);
    if (value !== undefined) {
      try {
        langTag = Intl.getCanonicalLocales(value)[0];
      } catch (err) {
        const warn = this.domBundle.formatStringFromName(
          "ManifestLangIsInvalid",
          [spec.property, value]
        );
        this.errors.push({ warn });
      }
    }
    return langTag;
  },
};
var EXPORTED_SYMBOLS = ["ValueExtractor"]; // jshint ignore:line
