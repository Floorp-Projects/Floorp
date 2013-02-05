/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const deprecate = require("sdk/util/deprecate");
var { Loader } = require("sdk/test/loader");

function LoaderWithHookedConsole() {
  let errors = [];
  let loader = Loader(module, {
    console: Object.create(console, {
      error: { value: function(error) {
        errors.push(error);
      }}
    })
  });

  return {
    loader: loader,
    deprecate: loader.require("sdk/util/deprecate"),
    errors: errors
  }
}

exports["test Deprecate Usage"] = function testDeprecateUsage(assert) {
  let { loader, deprecate, errors } = LoaderWithHookedConsole();

  function functionIsDeprecated() {
    deprecate.deprecateUsage("foo");
  }

  functionIsDeprecated();

  assert.equal(errors.length, 1, "only one error is dispatched");

  let msg = errors[0];

  assert.ok(msg.indexOf("foo") !== -1,
            "message contains the given message");
  assert.ok(msg.indexOf("functionIsDeprecated") !== -1,
            "message contains name of the caller function");
  assert.ok(msg.indexOf(module.uri) !== -1,
             "message contains URI of the caller module");

  loader.unload();
}

exports["test Deprecate Function"] = function testDeprecateFunction(assert) {
  let { loader, deprecate, errors } = LoaderWithHookedConsole();

  let self = {};
  let arg1 = "foo";
  let arg2 = {};

  function originalFunction(a1, a2) {
    assert.equal(this, self);
    assert.equal(a1, arg1);
    assert.equal(a2, arg2);
  };

  let deprecateFunction = deprecate.deprecateFunction(originalFunction,
                                                       "bar");

  deprecateFunction.call(self, arg1, arg2);

  assert.equal(errors.length, 1,
                   "only one error is dispatched");

  let msg = errors[0];
  assert.ok(msg.indexOf("bar") !== -1, "message contains the given message");
  assert.ok(msg.indexOf("testDeprecateFunction") !== -1,
            "message contains name of the caller function");
  assert.ok(msg.indexOf(module.uri) !== -1,
            "message contains URI of the caller module");

  loader.unload();
}

exports.testDeprecateEvent = function(assert, done) {
  let { loader, deprecate, errors } = LoaderWithHookedConsole();

  let { on, emit } = loader.require('sdk/event/core');
  let testObj = {};
  testObj.on = deprecate.deprecateEvent(on.bind(null, testObj), 'BAD', ['fire']);

  testObj.on('fire', function() {
    testObj.on('water', function() {
      assert.equal(errors.length, 1, "only one error is dispatched");
      loader.unload();
      done();
    })
    assert.equal(errors.length, 1, "only one error is dispatched");
    emit(testObj, 'water');
  });
  assert.equal(errors.length, 1, "only one error is dispatched");
  let msg = errors[0];console.log(msg);
  assert.ok(msg.indexOf("BAD") !== -1, "message contains the given message");
  assert.ok(msg.indexOf("deprecateEvent") !== -1,
            "message contains name of the caller function");
  assert.ok(msg.indexOf(module.uri) !== -1,
            "message contains URI of the caller module");

  emit(testObj, 'fire');
}

require("test").run(exports);
