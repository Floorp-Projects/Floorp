/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  ConsoleAPIListener
} = require("devtools/server/actors/webconsole/listeners");
const EventEmitter = require("devtools/shared/old-event-emitter");

/**
 * Each method of this object is a test; tests can be synchronous or asynchronous:
 *
 * 1. Plain functions are synchronous tests.
 * 2. methods with `async` keyword are asynchronous tests.
 * 3. methods with `done` as argument are asynchronous tests (`done` needs to be called to
 *    finish the test).
 */
const TESTS = {
  testEventEmitterCreation() {
    let emitter = getEventEmitter();

    ok(emitter, "We have an event emitter");
  },

  testEmittingEvents(done) {
    let emitter = getEventEmitter();

    let beenHere1 = false;
    let beenHere2 = false;

    function next(eventName, str1, str2) {
      equal(eventName, "next", "Got event");
      equal(str1, "abc", "Argument 1 is correct");
      equal(str2, "def", "Argument 2 is correct");

      ok(!beenHere1, "first time in next callback");
      beenHere1 = true;

      emitter.off("next", next);

      emitter.emit("next");

      emitter.once("onlyonce", onlyOnce);

      emitter.emit("onlyonce");
      emitter.emit("onlyonce");
    }

    function onlyOnce() {
      ok(!beenHere2, "\"once\" listener has been called once");
      beenHere2 = true;
      emitter.emit("onlyonce");

      done();
    }

    emitter.on("next", next);
    emitter.emit("next", "abc", "def");
  },

  testThrowingExceptionInListener(done) {
    let emitter = getEventEmitter();
    let listener = new ConsoleAPIListener(null, {
      onConsoleAPICall(message) {
        equal(message.level, "error");
        equal(message.arguments[0], "foo: bar");
        listener.destroy();
        done();
      }
    });

    listener.init();

    function throwListener() {
      emitter.off("throw-exception");
      throw Object.create({
        toString: () => "foo",
        stack: "bar",
      });
    }

    emitter.on("throw-exception", throwListener);
    emitter.emit("throw-exception");
  },

  testKillItWhileEmitting(done) {
    let emitter = getEventEmitter();

    const c1 = () => ok(true, "c1 called");
    const c2 = () => {
      ok(true, "c2 called");
      emitter.off("tick", c3);
    };
    const c3 = () => ok(false, "c3 should not be called");
    const c4 = () => {
      ok(true, "c4 called");
      done();
    };

    emitter.on("tick", c1);
    emitter.on("tick", c2);
    emitter.on("tick", c3);
    emitter.on("tick", c4);

    emitter.emit("tick");
  },

  testOffAfterOnce() {
    let emitter = getEventEmitter();

    let enteredC1 = false;
    let c1 = () => (enteredC1 = true);

    emitter.once("oao", c1);
    emitter.off("oao", c1);

    emitter.emit("oao");

    ok(!enteredC1, "c1 should not be called");
  },

  testPromise() {
    let emitter = getEventEmitter();
    let p = emitter.once("thing");

    // Check that the promise is only resolved once event though we
    // emit("thing") more than once
    let firstCallbackCalled = false;
    let check1 = p.then(arg => {
      equal(firstCallbackCalled, false, "first callback called only once");
      firstCallbackCalled = true;
      equal(arg, "happened", "correct arg in promise");
      return "rval from c1";
    });

    emitter.emit("thing", "happened", "ignored");

    // Check that the promise is resolved asynchronously
    let secondCallbackCalled = false;
    let check2 = p.then(arg => {
      ok(true, "second callback called");
      equal(arg, "happened", "correct arg in promise");
      secondCallbackCalled = true;
      equal(arg, "happened", "correct arg in promise (a second time)");
      return "rval from c2";
    });

    // Shouldn't call any of the above listeners
    emitter.emit("thing", "trashinate");

    // Check that we can still separate events with different names
    // and that it works with no parameters
    let pfoo = emitter.once("foo");
    let pbar = emitter.once("bar");

    let check3 = pfoo.then(arg => {
      ok(arg === undefined, "no arg for foo event");
      return "rval from c3";
    });

    pbar.then(() => {
      ok(false, "pbar should not be called");
    });

    emitter.emit("foo");

    equal(secondCallbackCalled, false, "second callback not called yet");

    return Promise.all([ check1, check2, check3 ]).then(args => {
      equal(args[0], "rval from c1", "callback 1 done good");
      equal(args[1], "rval from c2", "callback 2 done good");
      equal(args[2], "rval from c3", "callback 3 done good");
    });
  }
};

/**
 * Create a runnable tests based on the tests descriptor given.
 *
 * @param {Object} tests
 *  The tests descriptor object, contains the tests to run.
 */
const runnable = (tests) => (async function () {
  for (let name of Object.keys(tests)) {
    info(name);
    if (tests[name].length === 1) {
      await (new Promise(resolve => tests[name](resolve)));
    } else {
      await tests[name]();
    }
  }
});

// We want to run the same tests for both an instance of `EventEmitter` and an object
// decorate with EventEmitter; therefore we create two strategies (`createNewEmitter` and
// `decorateObject`) and a factory (`getEventEmitter`), where the factory is the actual
// function used in the tests.

const createNewEmitter = () => new EventEmitter();
const decorateObject = () => EventEmitter.decorate({});

// First iteration of the tests with a new instance of `EventEmitter`.
let getEventEmitter = createNewEmitter;
add_task(runnable(TESTS));
// Second iteration of the tests with an object decorate using `EventEmitter`
add_task(() => (getEventEmitter = decorateObject));
add_task(runnable(TESTS));
