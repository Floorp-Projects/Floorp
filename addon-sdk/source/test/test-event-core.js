/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const { on, once, off, emit, count, amass } = require('sdk/event/core');
const { LoaderWithHookedConsole } = require("sdk/test/loader");

exports['test add a listener'] = function(assert) {
  let events = [ { name: 'event#1' }, 'event#2' ];
  let target = { name: 'target' };

  on(target, 'message', function(message) {
    assert.equal(this, target, 'this is a target object');
    assert.equal(message, events.shift(), 'message is emitted event');
  });
  emit(target, 'message', events[0]);
  emit(target, 'message', events[0]);
};

exports['test that listener is unique per type'] = function(assert) {
  let actual = []
  let target = {}
  function listener() { actual.push(1) }
  on(target, 'message', listener);
  on(target, 'message', listener);
  on(target, 'message', listener);
  on(target, 'foo', listener);
  on(target, 'foo', listener);

  emit(target, 'message');
  assert.deepEqual([ 1 ], actual, 'only one message listener added');
  emit(target, 'foo');
  assert.deepEqual([ 1, 1 ], actual, 'same listener added for other event');
};

exports['test event type matters'] = function(assert) {
  let target = { name: 'target' }
  on(target, 'message', function() {
    assert.fail('no event is expected');
  });
  on(target, 'done', function() {
    assert.pass('event is emitted');
  });
  emit(target, 'foo')
  emit(target, 'done');
};

exports['test all arguments are pasesd'] = function(assert) {
  let foo = { name: 'foo' }, bar = 'bar';
  let target = { name: 'target' };
  on(target, 'message', function(a, b) {
    assert.equal(a, foo, 'first argument passed');
    assert.equal(b, bar, 'second argument passed');
  });
  emit(target, 'message', foo, bar);
};

exports['test no side-effects in emit'] = function(assert) {
  let target = { name: 'target' };
  on(target, 'message', function() {
    assert.pass('first listener is called');
    on(target, 'message', function() {
      assert.fail('second listener is called');
    });
  });
  emit(target, 'message');
};

exports['test can remove next listener'] = function(assert) {
  let target = { name: 'target' };
  function fail() assert.fail('Listener should be removed');

  on(target, 'data', function() {
    assert.pass('first litener called');
    off(target, 'data', fail);
  });
  on(target, 'data', fail);

  emit(target, 'data', 'hello');
};

exports['test order of propagation'] = function(assert) {
  let actual = [];
  let target = { name: 'target' };
  on(target, 'message', function() { actual.push(1); });
  on(target, 'message', function() { actual.push(2); });
  on(target, 'message', function() { actual.push(3); });
  emit(target, 'message');
  assert.deepEqual([ 1, 2, 3 ], actual, 'called in order they were added');
};

exports['test remove a listener'] = function(assert) {
  let target = { name: 'target' };
  let actual = [];
  on(target, 'message', function listener() {
    actual.push(1);
    on(target, 'message', function() {
      off(target, 'message', listener);
      actual.push(2);
    })
  });

  emit(target, 'message');
  assert.deepEqual([ 1 ], actual, 'first listener called');
  emit(target, 'message');
  assert.deepEqual([ 1, 1, 2 ], actual, 'second listener called');

  emit(target, 'message');
  assert.deepEqual([ 1, 1, 2, 2, 2 ], actual, 'first listener removed');
};

exports['test remove all listeners for type'] = function(assert) {
  let actual = [];
  let target = { name: 'target' }
  on(target, 'message', function() { actual.push(1); });
  on(target, 'message', function() { actual.push(2); });
  on(target, 'message', function() { actual.push(3); });
  on(target, 'bar', function() { actual.push('b') });
  off(target, 'message');

  emit(target, 'message');
  emit(target, 'bar');

  assert.deepEqual([ 'b' ], actual, 'all message listeners were removed');
};

exports['test remove all listeners'] = function(assert) {
  let actual = [];
  let target = { name: 'target' }
  on(target, 'message', function() { actual.push(1); });
  on(target, 'message', function() { actual.push(2); });
  on(target, 'message', function() { actual.push(3); });
  on(target, 'bar', function() { actual.push('b') });
  off(target);

  emit(target, 'message');
  emit(target, 'bar');

  assert.deepEqual([], actual, 'all listeners events were removed');
};

exports['test falsy arguments are fine'] = function(assert) {
  let type, listener, actual = [];
  let target = { name: 'target' }
  on(target, 'bar', function() { actual.push(0) });

  off(target, 'bar', listener);
  emit(target, 'bar');
  assert.deepEqual([ 0 ], actual, '3rd bad ard will keep listeners');

  off(target, type);
  emit(target, 'bar');
  assert.deepEqual([ 0, 0 ], actual, '2nd bad arg will keep listener');

  off(target, type, listener);
  emit(target, 'bar');
  assert.deepEqual([ 0, 0, 0 ], actual, '2nd&3rd bad args will keep listener');
};

exports['test error handling'] = function(assert) {
  let target = Object.create(null);
  let error = Error('boom!');

  on(target, 'message', function() { throw error; })
  on(target, 'error', function(boom) {
    assert.equal(boom, error, 'thrown exception causes error event');
  });
  emit(target, 'message');
};

exports['test unhandled exceptions'] = function(assert) {
  let exceptions = [];
  let { loader, messages } = LoaderWithHookedConsole(module);

  let { emit, on } = loader.require('sdk/event/core');
  let target = {};
  let boom = Error('Boom!');
  let drax = Error('Draax!!');

  on(target, 'message', function() { throw boom; });

  emit(target, 'message');
  assert.equal(messages.length, 1, 'Got the first exception');
  assert.equal(messages[0].type, 'exception', 'The console message is exception');
  assert.ok(~String(messages[0].msg).indexOf('Boom!'),
            'unhandled exception is logged');

  on(target, 'error', function() { throw drax; });
  emit(target, 'message');
  assert.equal(messages.length, 2, 'Got the second exception');
  assert.equal(messages[1].type, 'exception', 'The console message is exception');
  assert.ok(~String(messages[1].msg).indexOf('Draax!'),
            'error in error handler is logged');
};

exports['test unhandled errors'] = function(assert) {
  let exceptions = [];
  let { loader, messages } = LoaderWithHookedConsole(module);

  let { emit, on } = loader.require('sdk/event/core');
  let target = {};
  let boom = Error('Boom!');

  emit(target, 'error', boom);
  assert.equal(messages.length, 1, 'Error was logged');
  assert.equal(messages[0].type, 'exception', 'The console message is exception');
  assert.ok(~String(messages[0].msg).indexOf('Boom!'),
            'unhandled exception is logged');
};


exports['test count'] = function(assert) {
  let target = {};

  assert.equal(count(target, 'foo'), 0, 'no listeners for "foo" events');
  on(target, 'foo', function() {});
  assert.equal(count(target, 'foo'), 1, 'listener registered');
  on(target, 'foo', function() {}, 2, 'another listener registered');
  off(target)
  assert.equal(count(target, 'foo'), 0, 'listeners unregistered');
};

exports['test emit.lazy'] = function(assert) {
  let target = {}, boom = Error('boom!'), errors = [], actual = []

  on(target, 'error', function error(e) errors.push(e))

  on(target, 'a', function() 1);
  on(target, 'a', function() {});
  on(target, 'a', function() 2);
  on(target, 'a', function() { throw boom });
  on(target, 'a', function() 3);

  for each (let value in emit.lazy(target, 'a'))
    actual.push(value);

  assert.deepEqual(actual, [ 1, undefined, 2, 3 ],
                   'all results were collected');
  assert.deepEqual(errors, [ boom ], 'errors reporetd');
};

require('test').run(exports);
