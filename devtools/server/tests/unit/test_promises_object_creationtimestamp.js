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
  let timerPrecision = Preferences.get("privacy.reduceTimerPrecision");
  Preferences.set("privacy.reduceTimerPrecision", false);

  registerCleanupFunction(function() {
    Preferences.set("privacy.reduceTimerPrecision", timerPrecision);
  });

  let client = await startTestDebuggerServer("promises-object-test");
  let chromeActors = await getChromeActors(client);

  ok(Promise.toString().includes("native code"), "Expect native DOM Promise.");

  // We have to attach the chrome TabActor before playing with the PromiseActor
  await attachTab(client, chromeActors);
  await testPromiseCreationTimestamp(client, chromeActors, v => {
    return new Promise(resolve => resolve(v));
  });

  let response = await listTabs(client);
  let targetTab = findTab(response.tabs, "promises-object-test");
  ok(targetTab, "Found our target tab.");

  await testPromiseCreationTimestamp(client, targetTab, v => {
    const debuggee = DebuggerServer.getTestGlobal("promises-object-test");
    return debuggee.Promise.resolve(v);
  });

  await close(client);
});

async function testPromiseCreationTimestamp(client, form, makePromise) {
  let front = PromisesFront(client, form);
  let resolution = "MyLittleSecret" + Math.random();

  await front.attach();
  await front.listPromises();

  let onNewPromise = new Promise(resolve => {
    EventEmitter.on(front, "new-promises", promises => {
      for (let p of promises) {
        if (p.promiseState.state === "fulfilled" &&
            p.promiseState.value === resolution) {
          resolve(p);
        }
      }
    });
  });

  let start = Date.now();
  let promise = makePromise(resolution);
  let end = Date.now();

  let grip = await onNewPromise;
  ok(grip, "Found our new promise.");

  let creationTimestamp = grip.promiseState.creationTimestamp;

  ok(start - 1 <= creationTimestamp && creationTimestamp <= end + 1,
    "Expect promise creation timestamp to be within elapsed time range: " +
     (start - 1) + " <= " + creationTimestamp + " <= " + (end + 1));

  await front.detach();
  // Appease eslint
  void promise;
}
