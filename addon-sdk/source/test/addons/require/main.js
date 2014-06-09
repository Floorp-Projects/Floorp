/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

exports["test local vs sdk module"] = function (assert) {
  assert.notEqual(require("memory"),
                  require("sdk/deprecated/memory"),
                  "Local module takes the priority over sdk modules");
  assert.ok(require("memory").local,
            "this module is really the local one");
}

exports["test 3rd party vs sdk module"] = function (assert) {
  // We are testing with a 3rd party package called `tabs` with 3 modules
  // main, page-mod and third-party

  // the only way to require 3rd party package modules are to use absolute paths
  // require("tabs/main"), require("tabs/page-mod"),
  // require("tabs/third-party") and also require("tabs") which will refer
  // to tabs's main package module.

  // So require(page-mod) shouldn't map the 3rd party
  assert.equal(require("page-mod"),
               require("sdk/page-mod"),
               "Third party modules don't overload sdk modules");
  assert.ok(require("page-mod").PageMod,
            "page-mod module is really the sdk one");

  assert.equal(require("tabs/page-mod").id, "page-mod",
               "tabs/page-mod is the 3rd party");

  // But require(tabs) will map to 3rd party main module
  // *and* overload the sdk module
  // and also any local module with the same name
  assert.equal(require("tabs").id, "tabs-main",
               "Third party main module overload sdk modules");
  assert.equal(require("tabs"),
               require("tabs/main"),
               "require(tabs) maps to require(tabs/main)");
  // So that you have to use relative path to ensure getting the local module
  assert.equal(require("./tabs").id,
               "local-tabs",
               "require(./tabs) maps to the local module");

  // It should still be possible to require sdk module with absolute path
  assert.ok(require("sdk/tabs").open,
            "We can bypass this overloading with absolute path to sdk modules");
  assert.equal(require("sdk/tabs"),
               require("addon-kit/tabs"),
               "Old and new layout both work");
}

// /!\ Always use distinct module for each test.
//     Otherwise, the linker can correctly parse and allow the first usage of it
//     but still silently fail on the second. 

exports.testRelativeRequire = function (assert) {
  assert.equal(require('./same-folder').id, 'same-folder');
}

exports.testRelativeSubFolderRequire = function (assert) {
  assert.equal(require('./sub-folder/module').id, 'sub-folder');
}

exports.testMultipleRequirePerLine = function (assert) {
  var a=require('./multiple/a'),b=require('./multiple/b');
  assert.equal(a.id, 'a');
  assert.equal(b.id, 'b');
}

exports.testSDKRequire = function (assert) {
  assert.deepEqual(Object.keys(require('sdk/page-worker')), ['Page']);
  assert.equal(require('page-worker'), require('sdk/page-worker'));
}

require("sdk/test/runner").runTestsFromModule(module);
