/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

module.metadata = {
  'stability': 'unstable'
};

function getTestNames (exports)
  Object.keys(exports).filter(name => /^test/.test(name))

function isTestAsync (fn) fn.length > 1
function isHelperAsync (fn) fn.length > 2

/*
 * Takes an `exports` object of a test file and a function `beforeFn`
 * to be run before each test. `beforeFn` is called with a `name` string
 * as the first argument of the test name, and may specify a second
 * argument function `done` to indicate that this function should
 * resolve asynchronously
 */
function before (exports, beforeFn) {
  getTestNames(exports).map(name => {
    let testFn = exports[name];
    if (!isTestAsync(testFn) && !isHelperAsync(beforeFn)) {
      exports[name] = function (assert) {
        beforeFn(name, assert);
        testFn(assert);
      };
    }
    else if (isTestAsync(testFn) && !isHelperAsync(beforeFn)) {
      exports[name] = function (assert, done) {
        beforeFn(name, assert);
        testFn(assert, done);
      };
    }
    else if (!isTestAsync(testFn) && isHelperAsync(beforeFn)) {
      exports[name] = function (assert, done) {
        beforeFn(name, assert, () => {
          testFn(assert);
          done();
        });
      };
    } else if (isTestAsync(testFn) && isHelperAsync(beforeFn)) {
      exports[name] = function (assert, done) {
        beforeFn(name, assert, () => {
          testFn(assert, done);
        });
      };
    }
  });
}
exports.before = before;

/*
 * Takes an `exports` object of a test file and a function `afterFn`
 * to be run after each test. `afterFn` is called with a `name` string
 * as the first argument of the test name, and may specify a second
 * argument function `done` to indicate that this function should
 * resolve asynchronously
 */
function after (exports, afterFn) {
  getTestNames(exports).map(name => {
    let testFn = exports[name];
    if (!isTestAsync(testFn) && !isHelperAsync(afterFn)) {
      exports[name] = function (assert) {
        testFn(assert);
        afterFn(name, assert);
      };
    }
    else if (isTestAsync(testFn) && !isHelperAsync(afterFn)) {
      exports[name] = function (assert, done) {
        testFn(assert, () => {
          afterFn(name, assert);
          done();
        });
      };
    }
    else if (!isTestAsync(testFn) && isHelperAsync(afterFn)) {
      exports[name] = function (assert, done) {
        testFn(assert);
        afterFn(name, assert, done);
      };
    } else if (isTestAsync(testFn) && isHelperAsync(afterFn)) {
      exports[name] = function (assert, done) {
        testFn(assert, () => {
          afterFn(name, assert, done);
        });
      };
    }
  });
}
exports.after = after;
