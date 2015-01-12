/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const deprecate = require("sdk/util/deprecate");
const { LoaderWithHookedConsole } = require("sdk/test/loader");
const { get, set } = require("sdk/preferences/service");
const PREFERENCE = "devtools.errorconsole.deprecation_warnings";

exports["test Deprecate Usage"] = function testDeprecateUsage(assert) {
  set(PREFERENCE, true);
  let { loader, messages } = LoaderWithHookedConsole(module);
  let deprecate = loader.require("sdk/util/deprecate");

  function functionIsDeprecated() {
    deprecate.deprecateUsage("foo");
  }

  functionIsDeprecated();

  assert.equal(messages.length, 1, "only one error is dispatched");
  assert.equal(messages[0].type, "error", "the console message is an error");

  let msg = messages[0].msg;

  assert.ok(msg.indexOf("foo") !== -1,
            "message contains the given message");
  assert.ok(msg.indexOf("functionIsDeprecated") !== -1,
            "message contains name of the caller function");
  assert.ok(msg.indexOf(module.uri) !== -1,
             "message contains URI of the caller module");

  loader.unload();
}

exports["test Deprecate Function"] = function testDeprecateFunction(assert) {
  set(PREFERENCE, true);
  let { loader, messages } = LoaderWithHookedConsole(module);
  let deprecate = loader.require("sdk/util/deprecate");

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

  assert.equal(messages.length, 1, "only one error is dispatched");
  assert.equal(messages[0].type, "error", "the console message is an error");

  let msg = messages[0].msg;
  assert.ok(msg.indexOf("bar") !== -1, "message contains the given message");
  assert.ok(msg.indexOf("testDeprecateFunction") !== -1,
            "message contains name of the caller function");
  assert.ok(msg.indexOf(module.uri) !== -1,
            "message contains URI of the caller module");

  loader.unload();
}

exports.testDeprecateEvent = function(assert, done) {
  set(PREFERENCE, true);
  let { loader, messages } = LoaderWithHookedConsole(module);
  let deprecate = loader.require("sdk/util/deprecate");

  let { on, emit } = loader.require('sdk/event/core');
  let testObj = {};
  testObj.on = deprecate.deprecateEvent(on.bind(null, testObj), 'BAD', ['fire']);

  testObj.on('fire', function() {
    testObj.on('water', function() {
      assert.equal(messages.length, 1, "only one error is dispatched");
      loader.unload();
      done();
    })
    assert.equal(messages.length, 1, "only one error is dispatched");
    emit(testObj, 'water');
  });
  assert.equal(messages.length, 1, "only one error is dispatched");
  assert.equal(messages[0].type, "error", "the console message is an error");
  let msg = messages[0].msg;
  assert.ok(msg.indexOf("BAD") !== -1, "message contains the given message");
  assert.ok(msg.indexOf("deprecateEvent") !== -1,
            "message contains name of the caller function");
  assert.ok(msg.indexOf(module.uri) !== -1,
            "message contains URI of the caller module");

  emit(testObj, 'fire');
}

exports.testDeprecateSettingToggle = function (assert, done) {
  let { loader, messages } = LoaderWithHookedConsole(module);
  let deprecate = loader.require("sdk/util/deprecate");
  
  function fn () { deprecate.deprecateUsage("foo"); }

  set(PREFERENCE, false);
  fn();
  assert.equal(messages.length, 0, 'no deprecation warnings');
  
  set(PREFERENCE, true);
  fn();
  assert.equal(messages.length, 1, 'deprecation warnings when toggled');

  set(PREFERENCE, false);
  fn();
  assert.equal(messages.length, 1, 'no new deprecation warnings');
  done();
};

exports.testDeprecateSetting = function (assert, done) {
  // Set devtools.errorconsole.deprecation_warnings to false
  set(PREFERENCE, false);

  let { loader, messages } = LoaderWithHookedConsole(module);
  let deprecate = loader.require("sdk/util/deprecate");

  // deprecateUsage test
  function functionIsDeprecated() {
    deprecate.deprecateUsage("foo");
  }
  functionIsDeprecated();

  assert.equal(messages.length, 0,
    "no errors dispatched on deprecateUsage");

  // deprecateFunction test
  function originalFunction() {};

  let deprecateFunction = deprecate.deprecateFunction(originalFunction,
                                                       "bar");
  deprecateFunction();

  assert.equal(messages.length, 0,
    "no errors dispatched on deprecateFunction");

  // deprecateEvent
  let { on, emit } = loader.require('sdk/event/core');
  let testObj = {};
  testObj.on = deprecate.deprecateEvent(on.bind(null, testObj), 'BAD', ['fire']);

  testObj.on('fire', () => {
    assert.equal(messages.length, 0,
      "no errors dispatched on deprecateEvent");
    done();
  });

  emit(testObj, 'fire');
}
require("test").run(exports);
