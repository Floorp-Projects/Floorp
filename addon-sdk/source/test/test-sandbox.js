/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { sandbox, load, evaluate, nuke } = require('sdk/loader/sandbox');
const xulApp = require("sdk/system/xul-app");
const fixturesURI = module.uri.split('test-sandbox.js')[0] + 'fixtures/';

// The following adds Debugger constructor to the global namespace.
const { Cu } = require('chrome');
const { addDebuggerToGlobal } =
  Cu.import('resource://gre/modules/jsdebugger.jsm', {});
addDebuggerToGlobal(this);

exports['test basics'] = function(assert) {
  let fixture = sandbox('http://example.com');
  assert.equal(evaluate(fixture, 'var a = 1;'), undefined,
               'returns expression value');
  assert.equal(evaluate(fixture, 'b = 2;'), 2,
               'returns expression value');
  assert.equal(fixture.b, 2, 'global is defined as property');
  assert.equal(fixture.a, 1, 'global is defined as property');
  assert.equal(evaluate(fixture, 'a + b;'), 3, 'returns correct sum');
};

exports['test non-privileged'] = function(assert) {
  let fixture = sandbox('http://example.com');
  if (xulApp.versionInRange(xulApp.platformVersion, "15.0a1", "18.*")) {
    let rv = evaluate(fixture, 'Compo' + 'nents.utils');
    assert.equal(rv, undefined,
                 "Components's attributes are undefined in content sandboxes");
  }
  else {
    assert.throws(function() {
      evaluate(fixture, 'Compo' + 'nents.utils');
    }, 'Access to components is restricted');
  }
  fixture.sandbox = sandbox;
  assert.throws(function() {
    evaluate(fixture, sandbox('http://foo.com'));
  }, 'Can not call privileged code');
};

exports['test injection'] = function(assert) {
  let fixture = sandbox();
  fixture.hi = name => 'Hi ' + name;
  assert.equal(evaluate(fixture, 'hi("sandbox");'), 'Hi sandbox',
                'injected functions are callable');
};

exports['test exceptions'] = function(assert) {
  let fixture = sandbox();
  try {
    evaluate(fixture, '!' + function() {
      var message = 'boom';
      throw Error(message);
    } + '();');
  }
  catch (error) {
    assert.equal(error.fileName, '[System Principal]', 'No specific fileName reported');
    assert.equal(error.lineNumber, 3, 'reports correct line number');
  }

  try {
    evaluate(fixture, '!' + function() {
      var message = 'boom';
      throw Error(message);
    } + '();', 'foo.js');
  }
  catch (error) {
    assert.equal(error.fileName, 'foo.js', 'correct fileName reported');
    assert.equal(error.lineNumber, 3, 'reports correct line number');
  }

  try {
    evaluate(fixture, '!' + function() {
      var message = 'boom';
      throw Error(message);
    } + '();', 'foo.js', 2);
  }
  catch (error) {
    assert.equal(error.fileName, 'foo.js', 'correct fileName reported');
    assert.equal(error.lineNumber, 4, 'line number was opted');
  }
};

exports['test load'] = function(assert) {
  let fixture = sandbox();
  load(fixture, fixturesURI + 'sandbox-normal.js');
  assert.equal(fixture.a, 1, 'global variable defined');
  assert.equal(fixture.b, 2, 'global via `this` property was set');
  assert.equal(fixture.f(), 4, 'function was defined');
};

exports['test load with data: URL'] = function(assert) {
  let code = "var a = 1; this.b = 2; function f() { return 4; }";
  let fixture = sandbox();
  load(fixture, "data:," + encodeURIComponent(code));

  assert.equal(fixture.a, 1, 'global variable defined');
  assert.equal(fixture.b, 2, 'global via `this` property was set');
  assert.equal(fixture.f(), 4, 'function was defined');
};

exports['test load script with complex char'] = function(assert) {
  let fixture = sandbox();
  load(fixture, fixturesURI + 'sandbox-complex-character.js');
  assert.equal(fixture.chars, 'გამარჯობა', 'complex chars were loaded correctly');
};

exports['test load script with data: URL and complex char'] = function(assert) {
  let code = "var chars = 'გამარჯობა';";
  let fixture = sandbox();
  load(fixture, "data:," + encodeURIComponent(code));

  assert.equal(fixture.chars, 'გამარჯობა', 'complex chars were loaded correctly');
};

exports['test metadata']  = function(assert) {
  let self = require('sdk/self');

  let dbg = new Debugger();
  dbg.onNewGlobalObject = function(global) {
    let metadata = Cu.getSandboxMetadata(global.unsafeDereference());
    assert.ok(metadata, 'this global has attached metadata');
    assert.equal(metadata.addonID, self.id, 'addon ID is set');

    dbg.onNewGlobalObject = undefined;
  }

  let fixture = sandbox();
  assert.equal(dbg.onNewGlobalObject, undefined, 'Should have reset the handler');
}

exports['test nuke sandbox'] = function(assert) {

  let fixture = sandbox('http://example.com');
  fixture.foo = 'foo';

  let ref = evaluate(fixture, 'let a = {bar: "bar"}; a');

  nuke(fixture);

  assert.ok(Cu.isDeadWrapper(fixture), 'sandbox should be dead');

  assert.throws(
    () => fixture.foo,
    /can't access dead object/,
    'property of nuked sandbox should not be accessible'
  );

  assert.ok(Cu.isDeadWrapper(ref), 'ref to object from sandbox should be dead');

  assert.throws(
    () => ref.bar,
    /can't access dead object/,
    'object from nuked sandbox should not be alive'
  );
}

require('sdk/test').run(exports);
