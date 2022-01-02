/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);

// Prevent test failures due to the unhandled rejections in this test file.
PromiseTestUtils.disableUncaughtRejectionObserverForSelfTest();

add_task(async function test_globals() {
  Assert.notEqual(
    PromiseDebugging,
    undefined,
    "PromiseDebugging is available."
  );
});

add_task(async function test_promiseID() {
  let p1 = new Promise(resolve => {});
  let p2 = new Promise(resolve => {});
  let p3 = p2.catch(null);
  let promise = [p1, p2, p3];

  let identifiers = promise.map(PromiseDebugging.getPromiseID);
  info("Identifiers: " + JSON.stringify(identifiers));
  let idSet = new Set(identifiers);
  Assert.equal(
    idSet.size,
    identifiers.length,
    "PromiseDebugging.getPromiseID returns a distinct id per promise"
  );

  let identifiers2 = promise.map(PromiseDebugging.getPromiseID);
  Assert.equal(
    JSON.stringify(identifiers),
    JSON.stringify(identifiers2),
    "Successive calls to PromiseDebugging.getPromiseID return the same id for the same promise"
  );
});

add_task(async function test_observe_uncaught() {
  // The names of Promise instances
  let names = new Map();

  // The results for UncaughtPromiseObserver callbacks.
  let CallbackResults = function(name) {
    this.name = name;
    this.expected = new Set();
    this.observed = new Set();
    this.blocker = new Promise(resolve => (this.resolve = resolve));
  };
  CallbackResults.prototype = {
    observe(promise) {
      info(this.name + " observing Promise " + names.get(promise));
      Assert.equal(
        PromiseDebugging.getState(promise).state,
        "rejected",
        this.name + " observed a rejected Promise"
      );
      if (!this.expected.has(promise)) {
        Assert.ok(
          false,
          this.name +
            " observed a Promise that it expected to observe, " +
            names.get(promise) +
            " (" +
            PromiseDebugging.getPromiseID(promise) +
            ", " +
            PromiseDebugging.getAllocationStack(promise) +
            ")"
        );
      }
      Assert.ok(
        this.expected.delete(promise),
        this.name +
          " observed a Promise that it expected to observe, " +
          names.get(promise) +
          " (" +
          PromiseDebugging.getPromiseID(promise) +
          ")"
      );
      Assert.ok(
        !this.observed.has(promise),
        this.name + " observed a Promise that it has not observed yet"
      );
      this.observed.add(promise);
      if (this.expected.size == 0) {
        this.resolve();
      } else {
        info(
          this.name +
            " is still waiting for " +
            this.expected.size +
            " observations:"
        );
        info(
          JSON.stringify(Array.from(this.expected.values(), x => names.get(x)))
        );
      }
    },
  };

  let onLeftUncaught = new CallbackResults("onLeftUncaught");
  let onConsumed = new CallbackResults("onConsumed");

  let observer = {
    onLeftUncaught(promise, data) {
      onLeftUncaught.observe(promise);
    },
    onConsumed(promise) {
      onConsumed.observe(promise);
    },
  };

  let resolveLater = function(delay = 20) {
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    return new Promise((resolve, reject) => setTimeout(resolve, delay));
  };
  let rejectLater = function(delay = 20) {
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    return new Promise((resolve, reject) => setTimeout(reject, delay));
  };
  let makeSamples = function*() {
    yield {
      promise: Promise.resolve(0),
      name: "Promise.resolve",
    };
    yield {
      promise: Promise.resolve(resolve => resolve(0)),
      name: "Resolution callback",
    };
    yield {
      promise: Promise.resolve(0).catch(null),
      name: "`catch(null)`",
    };
    yield {
      promise: Promise.reject(0).catch(() => {}),
      name: "Reject and catch immediately",
    };
    yield {
      promise: resolveLater(),
      name: "Resolve later",
    };
    yield {
      promise: Promise.reject("Simple rejection"),
      leftUncaught: true,
      consumed: false,
      name: "Promise.reject",
    };

    // Reject a promise now, consume it later.
    let p = Promise.reject("Reject now, consume later");

    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    setTimeout(
      () =>
        p.catch(() => {
          info("Consumed promise");
        }),
      200
    );
    yield {
      promise: p,
      leftUncaught: true,
      consumed: true,
      name: "Reject now, consume later",
    };

    yield {
      promise: Promise.all([Promise.resolve("Promise.all"), rejectLater()]),
      leftUncaught: true,
      name: "Rejecting through Promise.all",
    };
    yield {
      promise: Promise.race([resolveLater(500), Promise.reject()]),
      leftUncaught: true, // The rejection wins the race.
      name: "Rejecting through Promise.race",
    };
    yield {
      promise: Promise.race([Promise.resolve(), rejectLater(500)]),
      leftUncaught: false, // The resolution wins the race.
      name: "Resolving through Promise.race",
    };

    let boom = new Error("`throw` in the constructor");
    yield {
      promise: new Promise(() => {
        throw boom;
      }),
      leftUncaught: true,
      name: "Throwing in the constructor",
    };

    let rejection = Promise.reject("`reject` during resolution");
    yield {
      promise: rejection,
      leftUncaught: false,
      consumed: false, // `rejection` is consumed immediately (see below)
      name: "Promise.reject, again",
    };

    yield {
      promise: new Promise(resolve => resolve(rejection)),
      leftUncaught: true,
      consumed: false,
      name: "Resolving with a rejected promise",
    };

    yield {
      promise: Promise.resolve(0).then(() => rejection),
      leftUncaught: true,
      consumed: false,
      name: "Returning a rejected promise from success handler",
    };

    yield {
      promise: Promise.resolve(0).then(() => {
        throw new Error();
      }),
      leftUncaught: true,
      consumed: false,
      name: "Throwing during the call to the success callback",
    };
  };
  let samples = [];
  for (let s of makeSamples()) {
    samples.push(s);
    info(
      "Promise '" +
        s.name +
        "' has id " +
        PromiseDebugging.getPromiseID(s.promise)
    );
  }

  PromiseDebugging.addUncaughtRejectionObserver(observer);

  for (let s of samples) {
    names.set(s.promise, s.name);
    if (s.leftUncaught || false) {
      onLeftUncaught.expected.add(s.promise);
    }
    if (s.consumed || false) {
      onConsumed.expected.add(s.promise);
    }
  }

  info("Test setup, waiting for callbacks.");
  await onLeftUncaught.blocker;

  info("All calls to onLeftUncaught are complete.");
  if (onConsumed.expected.size != 0) {
    info("onConsumed is still waiting for the following Promise:");
    info(
      JSON.stringify(
        Array.from(onConsumed.expected.values(), x => names.get(x))
      )
    );
    await onConsumed.blocker;
  }

  info("All calls to onConsumed are complete.");
  let removed = PromiseDebugging.removeUncaughtRejectionObserver(observer);
  Assert.ok(removed, "removeUncaughtRejectionObserver succeeded");
  removed = PromiseDebugging.removeUncaughtRejectionObserver(observer);
  Assert.ok(
    !removed,
    "second call to removeUncaughtRejectionObserver didn't remove anything"
  );
});

add_task(async function test_uninstall_observer() {
  let Observer = function() {
    this.blocker = new Promise(resolve => (this.resolve = resolve));
    this.active = true;
  };
  Observer.prototype = {
    set active(x) {
      this._active = x;
      if (x) {
        PromiseDebugging.addUncaughtRejectionObserver(this);
      } else {
        PromiseDebugging.removeUncaughtRejectionObserver(this);
      }
    },
    onLeftUncaught() {
      Assert.ok(this._active, "This observer is active.");
      this.resolve();
    },
    onConsumed() {
      Assert.ok(false, "We should not consume any Promise.");
    },
  };

  info("Adding an observer.");
  let deactivate = new Observer();
  Promise.reject("I am an uncaught rejection.");
  await deactivate.blocker;
  Assert.ok(true, "The observer has observed an uncaught Promise.");
  deactivate.active = false;
  info(
    "Removing the observer, it should not observe any further uncaught Promise."
  );

  info(
    "Rejecting a Promise and waiting a little to give a chance to observers."
  );
  let wait = new Observer();
  Promise.reject("I am another uncaught rejection.");
  await wait.blocker;
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 100));
  // Normally, `deactivate` should not be notified of the uncaught rejection.
  wait.active = false;
});
