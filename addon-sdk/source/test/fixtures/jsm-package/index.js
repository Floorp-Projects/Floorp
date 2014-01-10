/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

let { Test } = require('./Test.jsm');
let { Test: Test2 } = require('./Test.jsm');
exports.localJSM = Test.square(16) === 256;
exports.localJSMCached = Test === Test2;

(function () {
let { Promise } = require('resource://gre/modules/Promise.jsm');
let { defer } = require('resource://gre/modules/Promise.jsm').Promise;

exports.isCachedAbsolute = Promise.defer === defer;

exports.isLoadedAbsolute = function (val) {
  let { promise, resolve } = Promise.defer();
  resolve(val);
  return promise;
};
})();

(function () {
let { Promise } = require('modules/Promise.jsm');
let { defer } = require('modules/Promise.jsm').Promise;
exports.isCachedPath = Promise.defer === defer;

exports.isLoadedPath = function (val) {
  let { promise, resolve } = Promise.defer();
  resolve(val);
  return promise;
};
})();

(function () {
let { defer } = require('resource://gre/modules/commonjs/sdk/core/promise.js');
let { defer: defer2 } = require('resource://gre/modules/commonjs/sdk/core/promise.js');
exports.isCachedJSAbsolute = defer === defer2;
exports.isLoadedJSAbsolute = function (val) {
  let { promise, resolve } = defer();
  resolve(val);
  return promise;
};
})();
