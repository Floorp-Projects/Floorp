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
    // First check wich kind of object we have.
    let kind = this.getObjectKind(aObject);
    if (kind == "array") {
      let res = Cu.createArrayIn(aCtxt);
      aObject.forEach(function(aObj) {
        res.push(this.wrap(aObj, aCtxt));
      }, this);
      return res;
    } else if (TypedArrayThings.indexOf(kind) !== -1) {
      // This is slow, because from the perspective of the constructor in aCtxt
      // aObject is a CCW, and it gets the indexed properties one by one rather
      // instead of realizing that this is already a typed array thing.
      return new aCtxt[kind](aObject);
    } else if (kind == "file") {
      return new aCtxt.File(aObject,
                            { name: aObject.name,
                              type: aObject.type });
    } else if (kind == "blob") {
      return new aCtxt.Blob([aObject], { type: aObject.type });
    } else if (kind == "date") {
      return Cu.createDateIn(aCtxt, aObject.getTime());
    } else if (kind == "primitive") {
      return aObject;
    }

    // Fall-through, we now have a dictionnary object.
    let res = Cu.createObjectIn(aCtxt);
    let propList = { };
    for (let prop in aObject) {
      propList[prop] = {
        enumerable: true,
        configurable: true,
        writable: true,
        value: this.wrap(aObject[prop], aCtxt)
      }
    }
    Object.defineProperties(res, propList);
    Cu.makeObjectPropsNormal(res);
    return res;
  }
}
