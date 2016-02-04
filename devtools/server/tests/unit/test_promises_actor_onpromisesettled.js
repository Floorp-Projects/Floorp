/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can get the list of Promise objects that have settled from the
 * PromisesActor onPromiseSettled event handler.
 */

"use strict";

Cu.import("resource://testing-common/PromiseTestUtils.jsm", this);

const { PromisesFront } = require("devtools/server/actors/promises");

var events = require("sdk/event/core");

add_task(function*() {
  let client = yield startTestDebuggerServer("promises-actor-test");
  let chromeActors = yield getChromeActors(client);

  ok(Promise.toString().contains("native code"), "Expect native DOM Promise");

  // We have to attach the chrome TabActor before playing with the PromiseActor
  yield attachTab(client, chromeActors);
  yield testPromisesSettled(client, chromeActors,
    v => new Promise(resolve => resolve(v)),
    v => new Promise((resolve, reject) => reject(v)));

  let response = yield listTabs(client);
  let targetTab = findTab(response.tabs, "promises-actor-test");
  ok(targetTab, "Found our target tab.");

  yield testPromisesSettled(client, targetTab, v => {
    const debuggee = DebuggerServer.getTestGlobal("promises-actor-test");
    return debuggee.Promise.resolve(v);
  }, v => {
    const debuggee = DebuggerServer.getTestGlobal("promises-actor-test");
    return debuggee.Promise.reject(v);
  });

  yield close(client);
});

function* testPromisesSettled(client, form, makeResolvePromise,
    makeRejectPromise) {
  let front = PromisesFront(client, form);
  let resolution = "MyLittleSecret" + Math.random();

  yield front.attach();
  yield front.listPromises();

  let onPromiseSettled = oncePromiseSettled(front, resolution, true, false);
  let resolvedPromise = makeResolvePromise(resolution);
  let foundResolvedPromise = yield onPromiseSettled;
  ok(foundResolvedPromise, "Found our resolved promise");

  PromiseTestUtils.expectUncaughtRejection(r => r.message == resolution);
  onPromiseSettled = oncePromiseSettled(front, resolution, false, true);
  let rejectedPromise = makeRejectPromise(resolution);
  let foundRejectedPromise = yield onPromiseSettled;
  ok(foundRejectedPromise, "Found our rejected promise");

  yield front.detach();
  // Appease eslint
  void resolvedPromise;
  void rejectedPromise;
}

function oncePromiseSettled(front, resolution, resolveValue, rejectValue) {
  return new Promise(resolve => {
    events.on(front, "promises-settled", promises => {
      for (let p of promises) {
        equal(p.type, "object", "Expect type to be Object");
        equal(p.class, "Promise", "Expect class to be Promise");
        equal(typeof p.promiseState.creationTimestamp, "number",
          "Expect creation timestamp to be a number");
        equal(typeof p.promiseState.timeToSettle, "number",
          "Expect time to settle to be a number");

        if (p.promiseState.state === "fulfilled" &&
            p.promiseState.value === resolution) {
          resolve(resolveValue);
        } else if (p.promiseState.state === "rejected" &&
                   p.promiseState.reason === resolution) {
          resolve(rejectValue);
        } else {
          dump("Found non-target promise\n");
        }
      }
    });
  });
}
