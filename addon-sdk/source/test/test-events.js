/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const { LoaderWithHookedConsole } = require("sdk/test/loader");

// Exposing private methods as public in order to test
const EventEmitter = require('sdk/deprecated/events').EventEmitter.compose({
  listeners: function(type) this._listeners(type),
  emit: function() this._emit.apply(this, arguments),
  emitOnObject: function() this._emitOnObject.apply(this, arguments),
  removeAllListeners: function(type) this._removeAllListeners(type)
});

exports['test:add listeners'] = function(test) {
  let e = new EventEmitter();

  let events_new_listener_emited = [];
  let times_hello_emited = 0;

  e.on("newListener", function (event, listener) {
    events_new_listener_emited.push(event)
  })

  e.on("hello", function (a, b) {
    times_hello_emited += 1
    test.assertEqual("a", a)
    test.assertEqual("b", b)
    test.assertEqual(this, e, '`this` pseudo-variable is bound to instance');
  })

  e.emit("hello", "a", "b")
};

exports['test:removeListener'] = function(test) {
  let count = 0;

  function listener1 () {
    count++;
  }
  function listener2 () {
    count++;
  }

  // test adding and removing listener
  let e1 = new EventEmitter();
  test.assertEqual(0, e1.listeners('hello').length);
  e1.on("hello", listener1);
  test.assertEqual(1, e1.listeners('hello').length);
  test.assertEqual(listener1, e1.listeners('hello')[0]);
  e1.removeListener("hello", listener1);
  test.assertEqual(0, e1.listeners('hello').length);
  e1.emit("hello", "");
  test.assertEqual(0, count);

  // test adding one listener and removing another which was not added
  let e2 = new EventEmitter();
  test.assertEqual(0, e2.listeners('hello').length);
  e2.on("hello", listener1);
  test.assertEqual(1, e2.listeners('hello').length);
  e2.removeListener("hello", listener2);
  test.assertEqual(1, e2.listeners('hello').length);
  test.assertEqual(listener1, e2.listeners('hello')[0]);
  e2.emit("hello", "");
  test.assertEqual(1, count);

  // test adding 2 listeners, and removing one
  let e3 = new EventEmitter();
  test.assertEqual(0, e3.listeners('hello').length);
  e3.on("hello", listener1);
  test.assertEqual(1, e3.listeners('hello').length);
  e3.on("hello", listener2);
  test.assertEqual(2, e3.listeners('hello').length);
  e3.removeListener("hello", listener1);
  test.assertEqual(1, e3.listeners('hello').length);
  test.assertEqual(listener2, e3.listeners('hello')[0]);
  e3.emit("hello", "");
  test.assertEqual(2, count);
};

exports['test:removeAllListeners'] = function(test) {
  let count = 0;

  function listener1 () {
    count++;
  }
  function listener2 () {
    count++;
  }

  // test adding a listener and removing all of that type
  let e1 = new EventEmitter();
  e1.on("hello", listener1);
  test.assertEqual(1, e1.listeners('hello').length);
  e1.removeAllListeners("hello");
  test.assertEqual(0, e1.listeners('hello').length);
  e1.emit("hello", "");
  test.assertEqual(0, count);

  // test adding a listener and removing all of another type
  let e2 = new EventEmitter();
  e2.on("hello", listener1);
  test.assertEqual(1, e2.listeners('hello').length);
  e2.removeAllListeners('goodbye');
  test.assertEqual(1, e2.listeners('hello').length);
  e2.emit("hello", "");
  test.assertEqual(1, count);

  // test adding 1+ listeners and removing all of that type
  let e3 = new EventEmitter();
  e3.on("hello", listener1);
  test.assertEqual(1, e3.listeners('hello').length);
  e3.on("hello", listener2);
  test.assertEqual(2, e3.listeners('hello').length);
  e3.removeAllListeners("hello");
  test.assertEqual(0, e3.listeners('hello').length);
  e3.emit("hello", "");
  test.assertEqual(1, count);

  // test adding 2 listeners for 2 types and removing all listeners
  let e4 = new EventEmitter();
  e4.on("hello", listener1);
  test.assertEqual(1, e4.listeners('hello').length);
  e4.on('goodbye', listener2);
  test.assertEqual(1, e4.listeners('goodbye').length);
  e4.emit("goodbye", "");
  e4.removeAllListeners();
  test.assertEqual(0, e4.listeners('hello').length);
  test.assertEqual(0, e4.listeners('goodbye').length);
  e4.emit("hello", "");
  e4.emit("goodbye", "");
  test.assertEqual(2, count);
};

exports['test: modify in emit'] = function(test) {
  let callbacks_called = [ ];
  let e = new EventEmitter();

  function callback1() {
    callbacks_called.push("callback1");
    e.on("foo", callback2);
    e.on("foo", callback3);
    e.removeListener("foo", callback1);
  }
  function callback2() {
    callbacks_called.push("callback2");
    e.removeListener("foo", callback2);
  }
  function callback3() {
    callbacks_called.push("callback3");
    e.removeListener("foo", callback3);
  }

  e.on("foo", callback1);
  test.assertEqual(1, e.listeners("foo").length);

  e.emit("foo");
  test.assertEqual(2, e.listeners("foo").length);
  test.assertEqual(1, callbacks_called.length);
  test.assertEqual('callback1', callbacks_called[0]);

  e.emit("foo");
  test.assertEqual(0, e.listeners("foo").length);
  test.assertEqual(3, callbacks_called.length);
  test.assertEqual('callback1', callbacks_called[0]);
  test.assertEqual('callback2', callbacks_called[1]);
  test.assertEqual('callback3', callbacks_called[2]);

  e.emit("foo");
  test.assertEqual(0, e.listeners("foo").length);
  test.assertEqual(3, callbacks_called.length);
  test.assertEqual('callback1', callbacks_called[0]);
  test.assertEqual('callback2', callbacks_called[1]);
  test.assertEqual('callback3', callbacks_called[2]);

  e.on("foo", callback1);
  e.on("foo", callback2);
  test.assertEqual(2, e.listeners("foo").length);
  e.removeAllListeners("foo");
  test.assertEqual(0, e.listeners("foo").length);

  // Verify that removing callbacks while in emit allows emits to propagate to
  // all listeners
  callbacks_called = [ ];

  e.on("foo", callback2);
  e.on("foo", callback3);
  test.assertEqual(2, e.listeners("foo").length);
  e.emit("foo");
  test.assertEqual(2, callbacks_called.length);
  test.assertEqual('callback2', callbacks_called[0]);
  test.assertEqual('callback3', callbacks_called[1]);
  test.assertEqual(0, e.listeners("foo").length);
};

exports['test:adding same listener'] = function(test) {
  function foo() {}
  let e = new EventEmitter();
  e.on("foo", foo);
  e.on("foo", foo);
  test.assertEqual(
    1,
    e.listeners("foo").length,
    "listener reregistration is ignored"
 );
}

exports['test:errors are reported if listener throws'] = function(test) {
  let e = new EventEmitter(),
      reported = false;
  e.on('error', function(e) reported = true)
  e.on('boom', function() { throw new Error('Boom!') });
  e.emit('boom', 3);
  test.assert(reported, 'error should be reported through event');
};

exports['test:emitOnObject'] = function(test) {
  let e = new EventEmitter();

  e.on("foo", function() {
    test.assertEqual(this, e, "`this` should be emitter");
  });
  e.emitOnObject(e, "foo");

  e.on("bar", function() {
    test.assertEqual(this, obj, "`this` should be other object");
  });
  let obj = {};
  e.emitOnObject(obj, "bar");
};

exports['test:once'] = function(test) {
  let e = new EventEmitter();
  let called = false;

  e.once('foo', function(value) {
    test.assert(!called, "listener called only once");
    test.assertEqual(value, "bar", "correct argument was passed");
  });

  e.emit('foo', 'bar');
  e.emit('foo', 'baz');
};

exports["test:removing once"] = function(test) {
  let e = require("sdk/deprecated/events").EventEmitterTrait.create();
  e.once("foo", function() { test.pass("listener was called"); });
  e.once("error", function() { test.fail("error event was emitted"); });
  e._emit("foo", "bug-656684");
};

// Bug 726967: Ensure that `emit` doesn't do an infinite loop when `error`
// listener throws an exception
exports['test:emitLoop'] = function(test) {
  // Override the console for this test so it doesn't log the exception to the
  // test output
  let { loader } = LoaderWithHookedConsole(module);

  let EventEmitter = loader.require('sdk/deprecated/events').EventEmitter.compose({
    listeners: function(type) this._listeners(type),
    emit: function() this._emit.apply(this, arguments),
    emitOnObject: function() this._emitOnObject.apply(this, arguments),
    removeAllListeners: function(type) this._removeAllListeners(type)
  });

  let e = new EventEmitter();

  e.on("foo", function() {
    throw new Error("foo");
  });

  e.on("error", function() {
    throw new Error("error");
  });
  e.emit("foo");

  test.pass("emit didn't looped");
};
