/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

const Cu = Components.utils; 
const Cc = Components.classes;
const Ci = Components.interfaces;

const EXPORTED_SYMBOLS = ["ObjectWrapper"];

// Makes sure that we expose correctly chrome JS objects to content.

let ObjectWrapper = {
  wrap: function objWrapper_wrap(aObject, aCtxt) {
    let res = Cu.createObjectIn(aCtxt);
    let propList = { };
    for (let prop in aObject) {
      let value;
      if (Array.isArray(aObject[prop])) {
        value = Cu.createArrayIn(aCtxt);
        aObject[prop].forEach(function(aObj) {
          // Only wrap objects.
          if (typeof aObj == "object") {
            value.push(objWrapper_wrap(aObj, aCtxt));
          } else {
            value.push(aObj);
          }
        });
      } else if (typeof(aObject[prop]) == "object") {
        value = objWrapper_wrap(aObject[prop], aCtxt);
      } else {
        value = aObject[prop];
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
