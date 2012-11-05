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
  getObjectKind: function objWrapper_getobjectkind(aObject) {
    if (!aObject) {
      return "null";
    }

    if (Array.isArray(aObject)) {
      return "array";
    } else if (aObject.mozSlice && (typeof aObject.mozSlice == "function")) {
      return "blob";
    } else if (typeof aObject == "object") {
      return "object";
    } else {
      return "primitive";
    }
  },

  wrap: function objWrapper_wrap(aObject, aCtxt) {
    if (!aObject) {
      return null;
    }

    // First check wich kind of object we have.
    let kind = this.getObjectKind(aObject);
    if (kind == "array") {
      let res = Cu.createArrayIn(aCtxt);
      aObject.forEach(function(aObj) {
        res.push(this.wrap(aObj, aCtxt));
      }, this);
      return res;
    } else if (kind == "blob") {
      return new aCtxt.Blob([aObject]);
    } else if (kind == "primitive") {
      return aObject;
    }

    //  Fall-through, we now have a dictionnary object.
    let res = Cu.createObjectIn(aCtxt);
    let propList = { };
    for (let prop in aObject) {
      let value;
      let objProp = aObject[prop];
      let propKind = this.getObjectKind(objProp);
      if (propKind == "array") {
        value = Cu.createArrayIn(aCtxt);
        objProp.forEach(function(aObj) {
          value.push(this.wrap(aObj, aCtxt));
        }, this);
      } else if (propKind == "blob") {
        value = new aCtxt.Blob([objProp]);
      } else if (propKind == "object") {
        value = this.wrap(objProp, aCtxt);
      } else {
        value = objProp;
      }
      propList[prop] = {
        enumerable: true,
        configurable: true,
        writable: true,
        value: value
      }
    }
    Object.defineProperties(res, propList);
    Cu.makeObjectPropsNormal(res);
    return res;
  }
}
