/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "deprecated"
};

const file = require("../io/file");
const { Loader } = require("../test/loader");

const { isNative } = require('@loader/options');

const cuddlefish = isNative ? require("toolkit/loader") : require("../loader/cuddlefish");

const { defer, resolve } = require("../core/promise");
const { getAddon } = require("../addon/installer");
const { id } = require("sdk/self");
const { newURI } = require('sdk/url/utils');
const { getZipReader } = require("../zip/utils");

const { Cc, Ci, Cu } = require("chrome");
const { AddonManager } = Cu.import("resource://gre/modules/AddonManager.jsm", {});
var ios = Cc['@mozilla.org/network/io-service;1']
          .getService(Ci.nsIIOService);

const CFX_TEST_REGEX = /(([^\/]+\/)(?:lib\/)?)?(tests?\/test-[^\.\/]+)\.js$/;
const JPM_TEST_REGEX = /^()(tests?\/test-[^\.\/]+)\.js$/;

const { mapcat, map, filter, fromEnumerator } = require("sdk/util/sequence");

const toFile = x => x.QueryInterface(Ci.nsIFile);
const isTestFile = ({leafName}) => leafName.substr(0, 5) == "test-" && leafName.substr(-3, 3) == ".js";
const getFileURI = x => ios.newFileURI(x).spec;

const getDirectoryEntries = file => map(toFile, fromEnumerator(_ => file.directoryEntries));
const getTestFiles = directory => filter(isTestFile, getDirectoryEntries(directory));
const getTestURIs = directory => map(getFileURI, getTestFiles(directory));

const isDirectory = x => x.isDirectory();
const getTestEntries = directory => mapcat(entry =>
  /^tests?$/.test(entry.leafName) ? getTestURIs(entry) : getTestEntries(entry),
  filter(isDirectory, getDirectoryEntries(directory)));

const removeDups = (array) => array.reduce((result, value) => {
  if (value != result[result.length - 1]) {
    result.push(value);
  }
  return result;
}, []);

const getSuites = function getSuites({ id, filter }) {
  const TEST_REGEX = isNative ? JPM_TEST_REGEX : CFX_TEST_REGEX;

  return getAddon(id).then(addon => {
    let fileURI = addon.getResourceURI("tests/");
    let isPacked = fileURI.scheme == "jar";
    let xpiURI = addon.getResourceURI();
    let file = xpiURI.QueryInterface(Ci.nsIFileURL).file;
    let suites = [];
    let addEntry = (entry) => {
      if (filter(entry) && TEST_REGEX.test(entry)) {
        let suite = (isNative ? "./" : "") + (RegExp.$2 || "") + RegExp.$3;
        suites.push(suite);
      }
    }

    if (isPacked) {
      return getZipReader(file).then(zip => {
        let entries = zip.findEntries(null);
        while (entries.hasMore()) {
          let entry = entries.getNext();
          addEntry(entry);
        }
        zip.close();

        // sort and remove dups
        suites = removeDups(suites.sort());
        return suites;
      })
    }
    else {
      let tests = [...getTestEntries(file)];
      let rootURI = addon.getResourceURI("/");
      tests.forEach((entry) => {
        addEntry(entry.replace(rootURI.spec, ""));
      });
    }

    // sort and remove dups
    suites = removeDups(suites.sort());
    return suites;
  });
}
exports.getSuites = getSuites;

const makeFilters = function makeFilters(options) {
  options = options || {};

  // A filter string is {fileNameRegex}[:{testNameRegex}] - ie, a colon
  // optionally separates a regex for the test fileName from a regex for the
  // testName.
  if (options.filter) {
    let colonPos = options.filter.indexOf(':');
    let filterFileRegex, filterNameRegex;

    if (colonPos === -1) {
      filterFileRegex = new RegExp(options.filter);
      filterNameRegex = { test: () => true }
    }
    else {
      filterFileRegex = new RegExp(options.filter.substr(0, colonPos));
      filterNameRegex = new RegExp(options.filter.substr(colonPos + 1));
    }

    return {
      fileFilter: (name) => filterFileRegex.test(name),
      testFilter: (name) => filterNameRegex.test(name)
    }
  }

  return {
    fileFilter: () => true,
    testFilter: () => true
  };
}
exports.makeFilters = makeFilters;

var loader = Loader(module);
const NOT_TESTS = ['setup', 'teardown'];

var TestFinder = exports.TestFinder = function TestFinder(options) {
  this.filter = options.filter;
  this.testInProcess = options.testInProcess === false ? false : true;
  this.testOutOfProcess = options.testOutOfProcess === true ? true : false;
};

TestFinder.prototype = {
  findTests: function findTests() {
    let { fileFilter, testFilter } = makeFilters({ filter: this.filter });

    return getSuites({ id: id, filter: fileFilter }).then(suites => {
      let testsRemaining = [];

      let getNextTest = () => {
        if (testsRemaining.length) {
          return testsRemaining.shift();
        }

        if (!suites.length) {
          return null;
        }

        let suite = suites.shift();

        // Load each test file as a main module in its own loader instance
        // `suite` is defined by cuddlefish/manifest.py:ManifestBuilder.build
        let suiteModule;

        try {
          suiteModule = cuddlefish.main(loader, suite);
        }
        catch (e) {
          if (/Unsupported Application/i.test(e.message)) {
            // If `Unsupported Application` error thrown during test,
            // skip the test suite
            suiteModule = {
              'test suite skipped': assert => assert.pass(e.message)
            };
          }
          else {
            console.exception(e);
            throw e;
          }
        }

        if (this.testInProcess) {
          for (let name of Object.keys(suiteModule).sort()) {
            if (NOT_TESTS.indexOf(name) === -1 && testFilter(name)) {
              testsRemaining.push({
                setup: suiteModule.setup,
                teardown: suiteModule.teardown,
                testFunction: suiteModule[name],
                name: suite + "." + name
              });
            }
          }
        }

        return getNextTest();
      };

      return {
        getNext: () => resolve(getNextTest())
      };
    });
  }
};
