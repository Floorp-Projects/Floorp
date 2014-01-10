/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable"
};

const { Cu } = require("chrome");
const { Task } = Cu.import("resource://gre/modules/Task.jsm", {});
const { defer } = require("sdk/core/promise");
const BaseAssert = require("sdk/test/assert").Assert;
const { isFunction, isObject } = require("sdk/lang/type");

exports.Assert = BaseAssert;

function extend(target) {
  let descriptor = {}
  Array.slice(arguments, 1).forEach(function(source) {
    Object.getOwnPropertyNames(source).forEach(function onEach(name) {
      descriptor[name] = Object.getOwnPropertyDescriptor(source, name);
    });
  });
  return Object.create(target, descriptor);
}

/**
 * Function takes test `suite` object in CommonJS format and defines all of the
 * tests from that suite and nested suites in a jetpack format on a given
 * `target` object. Optionally third argument `prefix` can be passed to prefix
 * all the test names.
 */
function defineTestSuite(target, suite, prefix) {
  prefix = prefix || "";
  // If suite defines `Assert` that's what `assert` object have to be created
  // from and passed to a test function (This allows custom assertion functions)
  // See for details: http://wiki.commonjs.org/wiki/Unit_Testing/1.1
  let Assert = suite.Assert || BaseAssert;
  // Going through each item in the test suite and wrapping it into a
  // Jetpack test format.
  Object.keys(suite).forEach(function(key) {
     // If name starts with test then it's a test function or suite.
    if (key.indexOf("test") === 0) {
      let test = suite[key];

      // For each test function so we create a wrapper test function in a
      // jetpack format and copy that to a `target` exports.
      if (isFunction(test)) {

        // Since names of the test may match across suites we use full object
        // path as a name to avoid overriding same function.
        target[prefix + key] = function(options) {

          // Creating `assert` functions for this test.
          let assert = Assert(options);
          assert.end = () => options.done();

          // If test function is a generator use a task JS to allow yield-ing
          // style test runs.
          if (test.isGenerator && test.isGenerator()) {
            options.waitUntilDone();
            Task.spawn(test.bind(null, assert)).
                then(null, assert.fail).
                then(assert.end);
          }

          // If CommonJS test function expects more than one argument
          // it means that test is async and second argument is a callback
          // to notify that test is finished.
          else if (1 < test.length) {

            // Letting test runner know that test is executed async and
            // creating a callback function that CommonJS tests will call
            // once it's done.
            options.waitUntilDone();
            test(assert, function() {
              options.done();
            });
          }

          // Otherwise CommonJS test is synchronous so we call it only with
          // one argument.
          else {
            test(assert);
          }
        }
      }

      // If it's an object then it's a test suite containing test function
      // and / or nested test suites. In that case we just extend prefix used
      // and call this function to copy and wrap tests from nested suite.
      else if (isObject(test)) {
        // We need to clone `tests` instead of modifying it, since it's very
        // likely that it is frozen (usually test suites imported modules).
        test = extend(Object.prototype, test, {
          Assert: test.Assert || Assert
        });
        defineTestSuite(target, test, prefix + key + ".");
      }
    }
  });
}

/**
 * This function is a CommonJS test runner function, but since Jetpack test
 * runner and test format is different from CommonJS this function shims given
 * `exports` with all its tests into a Jetpack test format so that the built-in
 * test runner will be able to run CommonJS test without manual changes.
 */
exports.run = function run(exports) {

  // We can't leave old properties on exports since those are test in a CommonJS
  // format that why we move everything to a new `suite` object.
  let suite = {};
  Object.keys(exports).forEach(function(key) {
    suite[key] = exports[key];
    delete exports[key];
  });

  // Now we wrap all the CommonJS tests to a Jetpack format and define
  // those to a given `exports` object since that where jetpack test runner
  // will look for them.
  defineTestSuite(exports, suite);
};
