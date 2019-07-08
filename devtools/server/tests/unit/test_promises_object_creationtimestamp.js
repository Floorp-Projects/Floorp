/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get the approximate time range for promise creation timestamp.
 */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "Preferences",
  "resource://gre/modules/Preferences.jsm"
);

add_task(async function() {
  const timerPrecision = Preferences.get("privacy.reduceTimerPrecision");
  Preferences.set("privacy.reduceTimerPrecision", false);

  registerCleanupFunction(function() {
    Preferences.set("privacy.reduceTimerPrecision", timerPrecision);
  });

  const { promisesFront } = await createMainProcessPromisesFront();

  ok(Promise.toString().includes("native code"), "Expect native DOM Promise.");

  // We have to attach the chrome target actor before playing with the PromiseActor
  await testPromiseCreationTimestamp(promisesFront, v => {
    return new Promise(resolve => resolve(v));
  });
});

add_task(async function() {
  const { debuggee, promisesFront } = await createTabPromisesFront();

  await testPromiseCreationTimestamp(promisesFront, v => {
    return debuggee.Promise.resolve(v);
  });
});

async function testPromiseCreationTimestamp(front, makePromise) {
  const resolution = "MyLittleSecret" + Math.random();

  await front.attach();
  await front.listPromises();

  const onNewPromise = new Promise(resolve => {
    front.on("new-promises", promises => {
      for (const p of promises) {
        if (
          p.promiseState.state === "fulfilled" &&
          p.promiseState.value === resolution
        ) {
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

  ok(
    start - 1 <= creationTimestamp && creationTimestamp <= end + 1,
    "Expect promise creation timestamp to be within elapsed time range: " +
      (start - 1) +
      " <= " +
      creationTimestamp +
      " <= " +
      (end + 1)
  );

  await front.detach();
  // Appease eslint
  void promise;
}
