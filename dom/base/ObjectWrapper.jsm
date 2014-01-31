/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

this.EXPORTED_SYMBOLS = ["ObjectWrapper"];

// Makes sure that we expose correctly chrome JS objects to content.

const TypedArrayThings = [
  "Int8Array",
  "Uint8Array",
  "Uint8ClampedArray",
  "Int16Array",
  "Uint16Array",
  "Int32Array",
  "Uint32Array",
  "Float32Array",
  "Float64Array",
];

this.ObjectWrapper = {
  getObjectKind: function objWrapper_getObjectKind(aObject) {
    if (aObject === null || aObject === undefined) {
      return "primitive";
    } else if (Array.isArray(aObject)) {
      return "array";
    } else if (aObject instanceof Ci.nsIDOMFile) {
      return "file";
    } else if (aObject instanceof Ci.nsIDOMBlob) {
      return "blob";
    } else if (aObject instanceof Date) {
      return "date";
    } else if (TypedArrayThings.indexOf(aObject.constructor.name) !== -1) {
      return aObject.constructor.name;
    } else if (typeof aObject == "object") {
      return "object";
    } else {
      return "primitive";
    }
  },

  wrap: function objWrapper_wrap(aObject, aCtxt) {
    dump("-*- ObjectWrapper is deprecated. Use Components.utils.cloneInto() instead.\n");
    return Cu.cloneInto(aObject, aCtxt, { cloneFunctions: true });
  }
}
