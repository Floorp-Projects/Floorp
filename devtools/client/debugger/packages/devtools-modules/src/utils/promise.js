/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A sham for https://dxr.mozilla.org/mozilla-central/source/toolkit/modules/Promise.jsm
 */

/**
 * Promise.jsm is mostly the Promise web API with a `defer` method. Just drop this in here,
 * and use the native web API (although building with webpack/babel, it may replace this
 * with it's own version if we want to target environments that do not have `Promise`.
 */

let p = typeof window != "undefined" ? window.Promise : Promise;
p.defer = function defer() {
  var resolve, reject;
  var promise = new Promise(function() {
    resolve = arguments[0];
    reject = arguments[1];
  });
  return {
    resolve: resolve,
    reject: reject,
    promise: promise,
  };
};

module.exports = p;
