/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// NOTE: this test is explictly testing non-normal,
//       generator functions.


const { defer: async } = require('sdk/lang/functional');
const { before, after } = require('sdk/test/utils');
const { resolve } = require("sdk/core/promise");

var AFTER_RUN = 0;
var BEFORE_RUN = 0;

/*
 * Tests are dependent on ordering, as the before and after functions
 * are called outside of each test, and sometimes checked in the next test
 * (like in the `after` tests)
 */
exports.testABeforeAsync = function (assert, done) {
  assert.equal(BEFORE_RUN, 1, 'before function was called');
  BEFORE_RUN = 0;
  AFTER_RUN = 0;
  async(done)();
};

exports.testABeforeNameAsync = function (assert, done) {
  assert.equal(BEFORE_RUN, 2, 'before function was called with name');
  BEFORE_RUN = 0;
  AFTER_RUN = 0;
  async(done)();
};

exports.testAfterAsync = function (assert, done) {
  assert.equal(AFTER_RUN, 1, 'after function was called previously');
  BEFORE_RUN = 0;
  AFTER_RUN = 0;
  async(done)();
};

exports.testAfterNameAsync = function (assert, done) {
  assert.equal(AFTER_RUN, 2, 'after function was called with name');
  BEFORE_RUN = 0;
  AFTER_RUN = 0;
  async(done)();
};

exports.testSyncABefore = function (assert) {
  assert.equal(BEFORE_RUN, 1, 'before function was called for sync test');
  BEFORE_RUN = 0;
  AFTER_RUN = 0;
};

exports.testSyncAfter = function (assert) {
  assert.equal(AFTER_RUN, 1, 'after function was called for sync test');
  BEFORE_RUN = 0;
  AFTER_RUN = 0;
};

before(exports, function*(name, assert) {
  yield resolve();
  BEFORE_RUN = (name === 'testABeforeNameAsync') ? 2 : 1;
  assert.pass('assert passed into before function');
});

after(exports, function*(name, assert) {
  yield resolve();
  // testAfterName runs after testAfter, which is where this
  // check occurs in the assertation
  AFTER_RUN = (name === 'testAfterAsync') ? 2 : 1;
  assert.pass('assert passed into after function');
});

require('sdk/test').run(exports);
