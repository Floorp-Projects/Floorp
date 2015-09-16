/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Added noise to test AST walker
for (var i = 0; i < 5; i++) {
  square(i);
}

exports.directoryDefaults = require('./utils');
exports.directoryMain = require('./newmodule');
exports.resolvesJSoverDir= require('./dir/a');
exports.math = require('test-math');
exports.mathInRelative = require('./dir/b');
exports.customMainModule = require('test-custom-main');
exports.customMainModuleRelative = require('test-custom-main-relative');
exports.defaultMain = require('test-default-main');
exports.testJSON = require('./dir/c');
exports.dummyModule = require('./dir/dummy');

exports.eventCore = require('sdk/event/core');
exports.promise = require('sdk/core/promise');

exports.localJSM  = require('./dir/test.jsm');
exports.promisejsm = require('modules/Promise.jsm').Promise;
exports.require = require;

var math = require('test-math');
exports.areModulesCached = (math === exports.math);

// Added noise to test AST walker
function square (x) {
  let tmp = x;
  tmp *= x;
  return tmp;
}

