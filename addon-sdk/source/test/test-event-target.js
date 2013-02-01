/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const { emit } = require('sdk/event/core');
const { EventTarget } = require('sdk/event/target');
const { Loader } = require('sdk/test/loader');

exports['test add a listener'] = function(assert) {
  let events = [ { name: 'event#1' }, 'event#2' ];
  let target = EventTarget();

  target.on('message', function(message) {
    assert.equal(this, target, 'this is a target object');
    assert.equal(message, events.shift(), 'message is emitted event');
  });

  emit(target, 'message', events[0]);
  emit(target, 'message', events[0]);
};

exports['test pass in listeners'] = function(assert) {
  let actual = [ ];
  let target = EventTarget({
    onMessage: function onMessage(message) {
      assert.equal(this, target, 'this is an event target');
      actual.push(1);
    },
    onFoo: null,
    onbla: function() {
      assert.fail('`onbla` is not supposed to be called');
    }
  });
  target.on('message', function(message) {
    assert.equal(this, target, 'this is an event target');
    actual.push(2);
  });

  emit(target, 'message');
  emit(target, 'missing');

  assert.deepEqual([ 1, 2 ], actual, 'all listeners trigerred in right order');
};

exports['test that listener is unique per type'] = function(assert) {
  let actual = []
  let target = EventTarget();
  function listener() { actual.push(1) }
  target.on('message', listener);
  target.on('message', listener);
  target.on('message', listener);
  target.on('foo', listener);
  target.on('foo', listener);

  emit(target, 'message');
  assert.deepEqual([ 1 ], actual, 'only one message listener added');
  emit(target, 'foo');
  assert.deepEqual([ 1, 1 ], actual, 'same listener added for other event');
};

exports['test event type matters'] = function(assert) {
  let target = EventTarget();
  target.on('message', function() {
    assert.fail('no event is expected');
  });
  target.on('done', function() {
    assert.pass('event is emitted');
  });

  emit(target, 'foo');
  emit(target, 'done');
};

exports['test all arguments are pasesd'] = function(assert) {
  let foo = { name: 'foo' }, bar = 'bar';
  let target = EventTarget();
  target.on('message', function(a, b) {
    assert.equal(a, foo, 'first argument passed');
    assert.equal(b, bar, 'second argument passed');
  });
  emit(target, 'message', foo, bar);
};

exports['test no side-effects in emit'] = function(assert) {
  let target = EventTarget();
  target.on('message', function() {
    assert.pass('first listener is called');
    target.on('message', function() {
      assert.fail('second listener is called');
    });
  });
  emit(target, 'message');
};

exports['test order of propagation'] = function(assert) {
  let actual = [];
  let target = EventTarget();
  target.on('message', function() { actual.push(1); });
  target.on('message', function() { actual.push(2); });
  target.on('message', function() { actual.push(3); });
  emit(target, 'message');
  assert.deepEqual([ 1, 2, 3 ], actual, 'called in order they were added');
};

exports['test remove a listener'] = function(assert) {
  let target = EventTarget();
  let actual = [];
  target.on('message', function listener() {
    actual.push(1);
    target.on('message', function() {
      target.removeListener('message', listener);
      actual.push(2);
    })
  });

  target.removeListener('message'); // must do nothing.
  emit(target, 'message');
  assert.deepEqual([ 1 ], actual, 'first listener called');
  emit(target, 'message');
  assert.deepEqual([ 1, 1, 2 ], actual, 'second listener called');
  emit(target, 'message');
  assert.deepEqual([ 1, 1, 2, 2, 2 ], actual, 'first listener removed');
};

exports['test error handling'] = function(assert) {
  let target = EventTarget();
  let error = Error('boom!');

  target.on('message', function() { throw error; })
  target.on('error', function(boom) {
    assert.equal(boom, error, 'thrown exception causes error event');
  });
  emit(target, 'message');
};

exports['test unhandled errors'] = function(assert) {
  let exceptions = [];
  let loader = Loader(module);
  let { emit } = loader.require('sdk/event/core');
  let { EventTarget } = loader.require('sdk/event/target');
  Object.defineProperties(loader.sandbox('sdk/event/core'), {
    console: { value: {
      exception: function(e) {
        exceptions.push(e);
      }
    }}
  });
  let target = EventTarget();
  let boom = Error('Boom!');
  let drax = Error('Draax!!');

  target.on('message', function() { throw boom; });

  emit(target, 'message');
  assert.ok(~String(exceptions[0]).indexOf('Boom!'),
            'unhandled exception is logged');

  target.on('error', function() { throw drax; });
  emit(target, 'message');
  assert.ok(~String(exceptions[1]).indexOf('Draax!'),
            'error in error handler is logged');
};

require('test').run(exports);

