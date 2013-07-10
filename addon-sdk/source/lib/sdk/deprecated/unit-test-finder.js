/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "deprecated"
};

const file = require("../io/file");
const memory = require('./memory');
const suites = require('@test/options').allTestModules;
const { Loader } = require("sdk/test/loader");
const cuddlefish = require("sdk/loader/cuddlefish");

const NOT_TESTS = ['setup', 'teardown'];

var TestFinder = exports.TestFinder = function TestFinder(options) {
  memory.track(this);
  this.filter = options.filter;
  this.testInProcess = options.testInProcess === false ? false : true;
  this.testOutOfProcess = options.testOutOfProcess === true ? true : false;
};

TestFinder.prototype = {
  findTests: function findTests(cb) {
    var self = this;
    var tests = [];
    var filter;
    // A filter string is {fileNameRegex}[:{testNameRegex}] - ie, a colon
    // optionally separates a regex for the test fileName from a regex for the
    // testName.
    if (this.filter) {
      var colonPos = this.filter.indexOf(':');
      var filterFileRegex, filterNameRegex;
      if (colonPos === -1) {
        filterFileRegex = new RegExp(self.filter);
      } else {
        filterFileRegex = new RegExp(self.filter.substr(0, colonPos));
        filterNameRegex = new RegExp(self.filter.substr(colonPos + 1));
      }
      // This function will first be called with just the filename; if
      // it returns true the module will be loaded then the function
      // called again with both the filename and the testname.
      filter = function(filename, testname) {
        return filterFileRegex.test(filename) &&
               ((testname && filterNameRegex) ? filterNameRegex.test(testname)
                                              : true);
      };
    } else
      filter = function() {return true};

    suites.forEach(
      function(suite) {
        // Load each test file as a main module in its own loader instance
        // `suite` is defined by cuddlefish/manifest.py:ManifestBuilder.build
        let loader = Loader(module);
        let suiteModule;

        try {
          suiteModule = cuddlefish.main(loader, suite);
        }
        catch (e) {
          if (!/^Unsupported Application/.test(e.message))
            throw e;
          // If `Unsupported Application` error thrown during test,
          // skip the test suite
          suiteModule = {
            'test suite skipped': assert => assert.pass(e.message)
          };
        }

        if (self.testInProcess)
          for each (let name in Object.keys(suiteModule).sort()) {
            if(NOT_TESTS.indexOf(name) === -1 && filter(suite, name)) {
              tests.push({
                           setup: suiteModule.setup,
                           teardown: suiteModule.teardown,
                           testFunction: suiteModule[name],
                           name: suite + "." + name
                         });
            }
          }
      });

    cb(tests);
  }
};
