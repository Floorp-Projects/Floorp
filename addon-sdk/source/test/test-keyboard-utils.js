/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const utils = require("sdk/keyboard/utils");
const runtime = require("sdk/system/runtime");

const isMac = runtime.OS === "Darwin";

exports["test toString"] = function(assert) {
  assert.equal(utils.toString({
    key: "B",
    modifiers: [ "Shift", "Ctrl" ]
  }), "Shift-Ctrl-B", "toString does not normalizes JSON");
  
  assert.equal(utils.toString({
    key: "C",
    modifiers: [],
  }), "C", "Works with objects with empty array of modifiers");

  assert.equal(utils.toString(Object.create((function Type() {}).prototype, {
    key: { value: "d" },
    modifiers: { value: [ "alt" ] },
    method: { value: function() {} }
  })), "alt-d", "Works with non-json objects");

  assert.equal(utils.toString({ 
    modifiers: [ "shift", "alt" ]
  }), "shift-alt-", "works with only modifiers");
};

exports["test toJSON"] = function(assert) {
  assert.deepEqual(utils.toJSON("Shift-Ctrl-B"), {
    key: "b",
    modifiers: [ "control", "shift" ]
  }, "toJSON normalizes input");

  assert.deepEqual(utils.toJSON("Meta-Alt-option-C"), {
    key: "c",
    modifiers: [ "alt", "meta" ]
  }, "removes dublicates");

  assert.deepEqual(utils.toJSON("AccEl+sHiFt+Z", "+"), {
    key: "z",
    modifiers: isMac ? [ "meta", "shift" ] : [ "control", "shift" ]
  }, "normalizes OS specific keys and adjustes seperator");
};

exports["test normalize"] = function assert(assert) {
  assert.equal(utils.normalize("Shift Ctrl A control ctrl", " "),
               "control shift a", "removes reapeted modifiers");
  assert.equal(utils.normalize("shift-ctrl-left"), "control-shift-left",
               "normilizes non printed characters");

  assert.throws(function() {
    utils.normalize("shift-alt-b-z");
  }, "throws if contains more then on non-modifier key");
};

require("test").run(exports);
