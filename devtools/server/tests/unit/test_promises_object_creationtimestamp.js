/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get the approximate time range for promise creation timestamp.
 */

"use strict";

const { PromisesFront } = require("devtools/shared/fronts/promises");

var EventEmitter = require("devtools/shared/event-emitter");

ChromeUtils.defineModuleGetter(this, "Preferences",
                               "resource://gre/modules/Preferences.jsm");

add_task(async function() {
  const timerPrecision = Preferences.get("privacy.reduceTimerPrecision");
  Preferences.set("privacy.reduceTimerPrecision", false);

  registerCleanupFunction(function() {
    Preferences.set("privacy.reduceTimerPrecision", timerPrecision);
  });

  const client = await startTestDebuggerServer("promises-object-test");
  const parentProcessActors = await getParentProcessActors(client);

  ok(Promise.toString().includes("native code"), "Expect native DOM Promise.");

  // We have to attach the chrome target actor before playing with the PromiseActor
  await attachTab(client, parentProcessActors);
  await testPromiseCreationTimestamp(client, parentProcessActors, v => {
    return new Promise(resolve => resolve(v));
  });

  const response = await listTabs(client);
  const targetTab = findTab(response.tabs, "promises-object-test");
  ok(targetTab, "Found our target tab.");

  await testPromiseCreationTimestamp(client, targetTab, v => {
    const debuggee = DebuggerServer.getTestGlobal("promises-object-test");
    return debuggee.Promise.resolve(v);
  });

  await close(client);
});

async function testPromiseCreationTimestamp(client, form, makePromise) {
  const front = PromisesFront(client, form);
  const resolution = "MyLittleSecret" + Math.random();

  await front.attach();
  await front.listPromises();

  const onNewPromise = new Promise(resolve => {
    EventEmitter.on(front, "new-promises", promises => {
      for (const p of promises) {
        if (p.promiseState.state === "fulfilled" &&
            p.promiseState.value === resolution) {
          resolve(p);
        }
      }
    });
  });

  const start = Date.now();
  const promise = makePromise(resolution);
  const end = Date.now();

  const grip = await onNewPromise;
  ok(grip, "Found our new promise.");

  const creationTimestamp = grip.promiseState.creationTimestamp;

  ok(start - 1 <= creationTimestamp && creationTimestamp <= end + 1,
    "Expect promise creation timestamp to be within elapsed time range: " +
     (start - 1) + " <= " + creationTimestamp + " <= " + (end + 1));

  await front.detach();
  // Appease eslint
  void promise;
}
