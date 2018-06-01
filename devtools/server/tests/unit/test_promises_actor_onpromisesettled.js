/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can get the list of Promise objects that have settled from the
 * PromisesActor onPromiseSettled event handler.
 */

"use strict";

ChromeUtils.import("resource://testing-common/PromiseTestUtils.jsm", this);

const { PromisesFront } = require("devtools/shared/fronts/promises");

var EventEmitter = require("devtools/shared/event-emitter");

add_task(async function() {
  const client = await startTestDebuggerServer("promises-actor-test");
  const chromeActors = await getChromeActors(client);

  ok(Promise.toString().includes("native code"), "Expect native DOM Promise");

  // We have to attach the chrome TabActor before playing with the PromiseActor
  await attachTab(client, chromeActors);
  await testPromisesSettled(client, chromeActors,
    v => new Promise(resolve => resolve(v)),
    v => new Promise((resolve, reject) => reject(v)));

  const response = await listTabs(client);
  const targetTab = findTab(response.tabs, "promises-actor-test");
  ok(targetTab, "Found our target tab.");

  await testPromisesSettled(client, targetTab, v => {
    const debuggee = DebuggerServer.getTestGlobal("promises-actor-test");
    return debuggee.Promise.resolve(v);
  }, v => {
    const debuggee = DebuggerServer.getTestGlobal("promises-actor-test");
    return debuggee.Promise.reject(v);
  });

  await close(client);
});

async function testPromisesSettled(client, form, makeResolvePromise,
    makeRejectPromise) {
  const front = PromisesFront(client, form);
  const resolution = "MyLittleSecret" + Math.random();

  await front.attach();
  await front.listPromises();

  let onPromiseSettled = oncePromiseSettled(front, resolution, true, false);
  const resolvedPromise = makeResolvePromise(resolution);
  const foundResolvedPromise = await onPromiseSettled;
  ok(foundResolvedPromise, "Found our resolved promise");

  PromiseTestUtils.expectUncaughtRejection(r => r.message == resolution);
  onPromiseSettled = oncePromiseSettled(front, resolution, false, true);
  const rejectedPromise = makeRejectPromise(resolution);
  const foundRejectedPromise = await onPromiseSettled;
  ok(foundRejectedPromise, "Found our rejected promise");

  await front.detach();
  // Appease eslint
  void resolvedPromise;
  void rejectedPromise;
}

function oncePromiseSettled(front, resolution, resolveValue, rejectValue) {
  return new Promise(resolve => {
    EventEmitter.on(front, "promises-settled", promises => {
      for (const p of promises) {
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
