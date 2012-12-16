/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

this.EXPORTED_SYMBOLS = ["ObjectWrapper"];

// Makes sure that we expose correctly chrome JS objects to content.

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
    } else if (kind == "file") {
      return new aCtxt.File(aObject,
                            { name: aObject.name,
                              type: aObject.type });
    } else if (kind == "blob") {
      return new aCtxt.Blob([aObject]);
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
