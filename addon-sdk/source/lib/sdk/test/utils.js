/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'stability': 'unstable'
};

const { defer } = require('../core/promise');
const { setInterval, clearInterval } = require('../timers');
const { getTabs, closeTab } = require("../tabs/utils");
const { windows: getWindows } = require("../window/utils");
const { close: closeWindow } = require("../window/helpers");
const { isGenerator } = require("../lang/type");
const { env } = require("../system/environment");
const { Task } = require("resource://gre/modules/Task.jsm");

const getTestNames = (exports) =>
  Object.keys(exports).filter(name => /^test/.test(name));

const isTestAsync = ({length}) => length > 1;
const isHelperAsync = ({length}) => length > 2;

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

    // GENERATOR TESTS
    if (isGenerator(testFn) && isGenerator(beforeFn)) {
      exports[name] = function*(assert) {
        yield Task.spawn(beforeFn.bind(null, name, assert));
        yield Task.spawn(testFn.bind(null, assert));
      }
    }
    else if (isGenerator(testFn) && !isHelperAsync(beforeFn)) {
      exports[name] = function*(assert) {
        beforeFn(name, assert);
        yield Task.spawn(testFn.bind(null, assert));
      }
    }
    else if (isGenerator(testFn) && isHelperAsync(beforeFn)) {
      exports[name] = function*(assert) {
        yield new Promise(resolve => beforeFn(name, assert, resolve));
        yield Task.spawn(testFn.bind(null, assert));
      }
    }
    // SYNC TESTS
    else if (!isTestAsync(testFn) && isGenerator(beforeFn)) {
      exports[name] = function*(assert) {
        yield Task.spawn(beforeFn.bind(null, name, assert));
        testFn(assert);
      };
    }
    else if (!isTestAsync(testFn) && !isHelperAsync(beforeFn)) {
      exports[name] = function (assert) {
        beforeFn(name, assert);
        testFn(assert);
      };
    }
    else if (!isTestAsync(testFn) && isHelperAsync(beforeFn)) {
      exports[name] = function (assert, done) {
        beforeFn(name, assert, () => {
          testFn(assert);
          done();
        });
      };
    }
    // ASYNC TESTS
    else if (isTestAsync(testFn) && isGenerator(beforeFn)) {
      exports[name] = function*(assert) {
        yield Task.spawn(beforeFn.bind(null, name, assert));
        yield new Promise(resolve => testFn(assert, resolve));
      };
    }
    else if (isTestAsync(testFn) && !isHelperAsync(beforeFn)) {
      exports[name] = function (assert, done) {
        beforeFn(name, assert);
        testFn(assert, done);
      };
    }
    else if (isTestAsync(testFn) && isHelperAsync(beforeFn)) {
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

    // GENERATOR TESTS
    if (isGenerator(testFn) && isGenerator(afterFn)) {
      exports[name] = function*(assert) {
        yield Task.spawn(testFn.bind(null, assert));
        yield Task.spawn(afterFn.bind(null, name, assert));
      }
    }
    else if (isGenerator(testFn) && !isHelperAsync(afterFn)) {
      exports[name] = function*(assert) {
        yield Task.spawn(testFn.bind(null, assert));
        afterFn(name, assert);
      }
    }
    else if (isGenerator(testFn) && isHelperAsync(afterFn)) {
      exports[name] = function*(assert) {
        yield Task.spawn(testFn.bind(null, assert));
        yield new Promise(resolve => afterFn(name, assert, resolve));
      }
    }
    // SYNC TESTS
    else if (!isTestAsync(testFn) && isGenerator(afterFn)) {
      exports[name] = function*(assert) {
        testFn(assert);
        yield Task.spawn(afterFn.bind(null, name, assert));
      };
    }
    else if (!isTestAsync(testFn) && !isHelperAsync(afterFn)) {
      exports[name] = function (assert) {
        testFn(assert);
        afterFn(name, assert);
      };
    }
    else if (!isTestAsync(testFn) && isHelperAsync(afterFn)) {
      exports[name] = function (assert, done) {
        testFn(assert);
        afterFn(name, assert, done);
      };
    }
    // ASYNC TESTS
    else if (isTestAsync(testFn) && isGenerator(afterFn)) {
      exports[name] = function*(assert) {
        yield new Promise(resolve => testFn(assert, resolve));
        yield Task.spawn(afterFn.bind(null, name, assert));
      };
    }
    else if (isTestAsync(testFn) && !isHelperAsync(afterFn)) {
      exports[name] = function*(assert) {
        yield new Promise(resolve => testFn(assert, resolve));
        afterFn(name, assert);
      };
    }
    else if (isTestAsync(testFn) && isHelperAsync(afterFn)) {
      exports[name] = function*(assert) {
        yield new Promise(resolve => testFn(assert, resolve));
        yield new Promise(resolve => afterFn(name, assert, resolve));
      };
    }
  });
}
exports.after = after;

function waitUntil (predicate, delay) {
  let { promise, resolve } = defer();
  let interval = setInterval(() => {
    if (!predicate()) return;
    clearInterval(interval);
    resolve();
  }, delay || 10);
  return promise;
}
exports.waitUntil = waitUntil;

var cleanUI = function cleanUI() {
  let { promise, resolve } = defer();

  let windows = getWindows(null, { includePrivate: true });
  if (windows.length > 1) {
    return closeWindow(windows[1]).then(cleanUI);
  }

  getTabs(windows[0]).slice(1).forEach(closeTab);

  resolve();

  return promise;
}
exports.cleanUI = cleanUI;

exports.isTravisCI = ("TRAVIS" in env && "CI" in env);
