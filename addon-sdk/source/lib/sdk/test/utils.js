/* vim:ts=2:sts=2:sw=2:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

module.metadata = {
  'stability': 'unstable'
};

function getTestNames (exports)
  Object.keys(exports).filter(name => /^test/.test(name))

function isAsync (fn) fn.length > 1

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
    if (!isAsync(testFn) && !isAsync(beforeFn)) {
      exports[name] = function (assert) {
        beforeFn(name);
        testFn(assert);
      };
    }
    else if (isAsync(testFn) && !isAsync(beforeFn)) {
      exports[name] = function (assert, done) {
        beforeFn(name);
        testFn(assert, done);
      }
    }
    else if (!isAsync(testFn) && isAsync(beforeFn)) {
      exports[name] = function (assert, done) {
        beforeFn(name, () => {
          testFn(assert);
          done();
        });
      }
    } else if (isAsync(testFn) && isAsync(beforeFn)) {
      exports[name] = function (assert, done) {
        beforeFn(name, () => {
          testFn(assert, done);
        });
      }
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
    if (!isAsync(testFn) && !isAsync(afterFn)) {
      exports[name] = function (assert) {
        testFn(assert);
        afterFn(name);
      };
    }
    else if (isAsync(testFn) && !isAsync(afterFn)) {
      exports[name] = function (assert, done) {
        testFn(assert, () => {
          afterFn(name);
          done();
        });
      }
    }
    else if (!isAsync(testFn) && isAsync(afterFn)) {
      exports[name] = function (assert, done) {
        testFn(assert);
        afterFn(name, done);
      }
    } else if (isAsync(testFn) && isAsync(afterFn)) {
      exports[name] = function (assert, done) {
        testFn(assert, () => {
          afterFn(name, done);
        });
      }
    }
  });
}
exports.after = after;
