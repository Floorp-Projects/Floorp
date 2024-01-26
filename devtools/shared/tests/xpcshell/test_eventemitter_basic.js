/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  ConsoleAPIListener,
} = require("resource://devtools/server/actors/webconsole/listeners/console-api.js");
const EventEmitter = require("resource://devtools/shared/event-emitter.js");
const hasMethod = (target, method) =>
  method in target && typeof target[method] === "function";

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
    const emitter = getEventEmitter();
    const isAnEmitter = emitter instanceof EventEmitter;

    ok(emitter, "We have an event emitter");
    ok(
      hasMethod(emitter, "on") &&
        hasMethod(emitter, "off") &&
        hasMethod(emitter, "once") &&
        hasMethod(emitter, "count") &&
        !hasMethod(emitter, "decorate"),
      `Event Emitter ${
        isAnEmitter ? "instance" : "mixin"
      } has the expected methods.`
    );
  },

  testEmittingEvents(done) {
    const emitter = getEventEmitter();

    let beenHere1 = false;
    let beenHere2 = false;

    function next(str1, str2) {
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
      ok(!beenHere2, '"once" listener has been called once');
      beenHere2 = true;
      emitter.emit("onlyonce");

      done();
    }

    emitter.on("next", next);
    emitter.emit("next", "abc", "def");
  },

  testThrowingExceptionInListener(done) {
    const emitter = getEventEmitter();
    const listener = new ConsoleAPIListener(null, message => {
      equal(message.level, "error");
      const [arg] = message.arguments;
      equal(arg.message, "foo");
      equal(arg.stack, "bar");
      listener.destroy();
      done();
    });

    listener.init();

    function throwListener() {
      emitter.off("throw-exception");
      const err = new Error("foo");
      err.stack = "bar";
      throw err;
    }

    emitter.on("throw-exception", throwListener);
    emitter.emit("throw-exception");
  },

  testKillItWhileEmitting(done) {
    const emitter = getEventEmitter();

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
    const emitter = getEventEmitter();

    let enteredC1 = false;
    const c1 = () => (enteredC1 = true);

    emitter.once("oao", c1);
    emitter.off("oao", c1);

    emitter.emit("oao");

    ok(!enteredC1, "c1 should not be called");
  },

  testPromise() {
    const emitter = getEventEmitter();
    const p = emitter.once("thing");

    // Check that the promise is only resolved once event though we
    // emit("thing") more than once
    let firstCallbackCalled = false;
    const check1 = p.then(arg => {
      equal(firstCallbackCalled, false, "first callback called only once");
      firstCallbackCalled = true;
      equal(arg, "happened", "correct arg in promise");
      return "rval from c1";
    });

    emitter.emit("thing", "happened", "ignored");

    // Check that the promise is resolved asynchronously
    let secondCallbackCalled = false;
    const check2 = p.then(arg => {
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
    const pfoo = emitter.once("foo");
    const pbar = emitter.once("bar");

    const check3 = pfoo.then(arg => {
      Assert.strictEqual(arg, undefined, "no arg for foo event");
      return "rval from c3";
    });

    pbar.then(() => {
      ok(false, "pbar should not be called");
    });

    emitter.emit("foo");

    equal(secondCallbackCalled, false, "second callback not called yet");

    return Promise.all([check1, check2, check3]).then(args => {
      equal(args[0], "rval from c1", "callback 1 done good");
      equal(args[1], "rval from c2", "callback 2 done good");
      equal(args[2], "rval from c3", "callback 3 done good");
    });
  },

  testClearEvents() {
    const emitter = getEventEmitter();

    const received = [];
    const listener = (...args) => received.push(args);

    emitter.on("a", listener);
    emitter.on("b", listener);
    emitter.on("c", listener);

    emitter.emit("a", 1);
    emitter.emit("b", 1);
    emitter.emit("c", 1);

    equal(received.length, 3, "the listener was triggered three times");

    emitter.clearEvents();
    emitter.emit("a", 1);
    emitter.emit("b", 1);
    emitter.emit("c", 1);
    equal(received.length, 3, "the listener was not called after clearEvents");
  },

  testOnReturn() {
    const emitter = getEventEmitter();

    let called = false;
    const removeOnTest = emitter.on("test", () => {
      called = true;
    });

    equal(typeof removeOnTest, "function", "`on` returns a function");
    removeOnTest();

    emitter.emit("test");
    equal(called, false, "event listener wasn't called");
  },

  async testEmitAsync() {
    const emitter = getEventEmitter();

    let resolve1, resolve2;
    emitter.once("test", async () => {
      return new Promise(r => {
        resolve1 = r;
      });
    });

    // Adding a listener which doesn't return a promise should trigger a console warning.
    emitter.once("test", () => {});

    emitter.once("test", async () => {
      return new Promise(r => {
        resolve2 = r;
      });
    });

    info("Emit an event and wait for all listener resolutions");
    const onConsoleWarning = onConsoleWarningLogged(
      "Listener for event 'test' did not return a promise."
    );
    const onEmitted = emitter.emitAsync("test");
    let resolved = false;
    onEmitted.then(() => {
      info("emitAsync just resolved");
      resolved = true;
    });

    info("Waiting for warning message about the second listener");
    await onConsoleWarning;

    // Spin the event loop, to ensure that emitAsync did not resolved too early
    await new Promise(r => Services.tm.dispatchToMainThread(r));

    ok(resolve1, "event listener has been called");
    ok(!resolved, "but emitAsync hasn't resolved yet");

    info("Resolve the first listener function");
    resolve1();
    ok(!resolved, "emitAsync isn't resolved until all listener resolve");

    info("Resolve the second listener function");
    resolve2();

    // emitAsync is only resolved in the next event loop
    await new Promise(r => Services.tm.dispatchToMainThread(r));
    ok(resolved, "once we resolve all the listeners, emitAsync is resolved");
  },

  testCount() {
    const emitter = getEventEmitter();

    equal(emitter.count("foo"), 0, "no listeners for 'foo' events");
    emitter.on("foo", () => {});
    equal(emitter.count("foo"), 1, "listener registered");
    emitter.on("foo", () => {});
    equal(emitter.count("foo"), 2, "another listener registered");
    emitter.off("foo");
    equal(emitter.count("foo"), 0, "listeners unregistered");
  },
};

// Wait for the next call to console.warn which includes
// the text passed as argument
function onConsoleWarningLogged(warningMessage) {
  return new Promise(resolve => {
    const ConsoleAPIStorage = Cc[
      "@mozilla.org/consoleAPI-storage;1"
    ].getService(Ci.nsIConsoleAPIStorage);

    const observer = subject => {
      // This is the first argument passed to console.warn()
      const message = subject.wrappedJSObject.arguments[0];
      if (message.includes(warningMessage)) {
        ConsoleAPIStorage.removeLogEventListener(observer);
        resolve();
      }
    };

    ConsoleAPIStorage.addLogEventListener(
      observer,
      Cc["@mozilla.org/systemprincipal;1"].createInstance(Ci.nsIPrincipal)
    );
  });
}

/**
 * Create a runnable tests based on the tests descriptor given.
 *
 * @param {Object} tests
 *  The tests descriptor object, contains the tests to run.
 */
const runnable = tests =>
  async function () {
    for (const name of Object.keys(tests)) {
      info(name);
      if (tests[name].length === 1) {
        await new Promise(resolve => tests[name](resolve));
      } else {
        await tests[name]();
      }
    }
  };

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
