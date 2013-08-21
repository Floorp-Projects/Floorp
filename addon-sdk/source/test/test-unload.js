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

/**
 * Check that destructors are called only once with Traits.
 * - check that public API is calling the destructor and unregister it,
 * - check that composed traits with multiple ensure calls, leads to only
 * one destructor call.
 */
exports.testEnsureWithTraits = function(assert) {
  let { Trait } = require("sdk/deprecated/traits");
  let loader = Loader(module);
  let ul = loader.require("sdk/system/unload");

  let called = 0;
  let composedCalled = 0;
  let composedTrait = Trait.compose({
      constructor: function () {
        // We have to give "public interface" of this trait, as we want to
        // call public `unload` method and ensure that we call it only once,
        // either when we call this public function manually or on add-on unload
        ul.ensure(this._public);
      },
      unload: function unload() {
        composedCalled++;
      }
    });
  let obj = Trait.compose(
    composedTrait.resolve({
      constructor: "_constructor",
      unload : "_unload"
    }), {
      constructor: function constructor() {
        // Same thing applies here, we need to pass public interface
        ul.ensure(this._public);
        this._constructor();
      },
      unload: function unload() {
        called++;
        this._unload();
      }
    })();

  obj.unload();
  assert.equal(called, 1,
                   "unload() should be called");

  assert.equal(composedCalled, 1,
                   "composed object unload() should be called");

  obj.unload();
  assert.equal(called, 1,
                   "unload() should be called only once");
  assert.equal(composedCalled, 1,
                   "composed object unload() should be called only once");

  loader.unload();
  assert.equal(called, 1,
                   "unload() should be called only once, after addon unload");
  assert.equal(composedCalled, 1,
                   "composed object unload() should be called only once, " +
                   "after addon unload");
};

exports.testEnsureWithTraitsPrivate = function(assert) {
  let { Trait } = require("sdk/deprecated/traits");
  let loader = Loader(module);
  let ul = loader.require("sdk/system/unload");

  let called = 0;
  let privateObj = null;
  let obj = Trait.compose({
      constructor: function constructor() {
        // This time wa don't have to give public interface,
        // as we want to call a private method:
        ul.ensure(this, "_unload");
        privateObj = this;
      },
      _unload: function unload() {
        called++;
        this._unload();
      }
    })();

  loader.unload();
  assert.equal(called, 1,
                   "unload() should be called");

  privateObj._unload();
  assert.equal(called, 1,
                   "_unload() should be called only once, after addon unload");
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
