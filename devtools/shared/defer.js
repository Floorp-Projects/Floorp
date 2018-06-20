/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// We have to keep using Promise.jsm here, because DOM Promises
// start freezing during panel iframes destruction.
// More info in bug 1454373 comment 15.
const Promise = require("promise");

/**
 * Returns a deferred object, with a resolve and reject property.
 * https://developer.mozilla.org/en-US/docs/Mozilla/JavaScript_code_modules/Promise.jsm/Deferred
 */
module.exports = function defer() {
  let resolve, reject;
  const promise = new Promise(function() {
    resolve = arguments[0];
    reject = arguments[1];
  });
  return {
    resolve: resolve,
    reject: reject,
    promise: promise
  };
};
