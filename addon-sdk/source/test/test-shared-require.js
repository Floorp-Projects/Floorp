/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cu } = require("chrome");

const requireURI = require.resolve("toolkit/require.js");

const jsm = Cu.import(requireURI, {});

exports.testRequire = assert => {
  assert.equal(typeof(jsm.require), "function",
               "require is a function");
  assert.equal(typeof(jsm.require.resolve), "function",
               "require.resolve is a function");

  assert.equal(typeof(jsm.require("method/core")), "function",
               "can import modules that aren't under sdk");

  assert.equal(typeof(jsm.require("sdk/base64").encode), "function",
               "can import module from sdk");
};

const required = require("toolkit/require")

exports.testSameRequire = (assert) => {
  assert.equal(jsm.require("method/core"),
               required.require("method/core"),
               "jsm and module return same instance");

  assert.equal(jsm.require, required.require,
               "require function is same in both contexts");
};

require("sdk/test").run(exports)
