/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 "use strict";

var unload = require("sdk/system/unload");
var { Loader, LoaderWithHookedConsole } = require("sdk/test/loader");

exports.testUnloading = function(assert) {
  let { loader, messages } = LoaderWithHookedConsole(module);
  var ul = loader.require("sdk/system/unload");
  var unloadCalled = 0;
  function unload() {
    unloadCalled++;
    throw new Error("error");
  }
  ul.when(unload);

  // This should be ignored, as we already registered it
  ul.when(unload);

  function unload2() { unloadCalled++; }
  ul.when(unload2);
  loader.unload();
  assert.equal(unloadCalled, 2,
                   "Unloader functions are called on unload.");
  assert.equal(messages.length, 1,
                   "One unload handler threw exception 1/2");
  assert.equal(messages[0].type, "exception",
                   "One unload handler threw exception 2/2");
};

exports.testEnsure = function(assert) {
  assert.throws(function() { unload.ensure({}); },
                /object has no 'unload' property/,
                "passing obj with no unload prop should fail");
  assert.throws(function() { unload.ensure({}, "destroy"); },
                /object has no 'destroy' property/,
                "passing obj with no custom unload prop should fail");

  var called = 0;
  var obj = {unload: function() { called++; }};

  unload.ensure(obj);
  obj.unload();
  assert.equal(called, 1,
                   "unload() should be called");
  obj.unload();
  assert.equal(called, 1,
                   "unload() should be called only once");
};

exports.testReason = function (assert) {
  var reason = "Reason doesn't actually have to be anything in particular.";
  var loader = Loader(module);
  var ul = loader.require("sdk/system/unload");
  ul.when(function (rsn) {
    assert.equal(rsn, reason,
                     "when() reason should be reason given to loader");
  });
  var obj = {
    unload: function (rsn) {
      assert.equal(rsn, reason,
                       "ensure() reason should be reason given to loader");
    }
  };
  ul.ensure(obj);
  loader.unload(reason);
};

require("sdk/test").run(exports);
