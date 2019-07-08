/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

/**
 * Test that we get the expected settlement time for promise time to settle.
 */

"use strict";

const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

add_task(async function() {
  const { promisesFront } = await createMainProcessPromisesFront();

  ok(Promise.toString().includes("native code"), "Expect native DOM Promise.");

  await testGetTimeToSettle(
    promisesFront,
    v => new Promise(resolve => setTimeout(() => resolve(v), 100))
  );
});

add_task(async function() {
  const { debuggee, promisesFront } = await createTabPromisesFront();

  await testGetTimeToSettle(promisesFront, v => {
    return new debuggee.Promise(resolve => setTimeout(() => resolve(v), 100));
  });
});

async function testGetTimeToSettle(front, makePromise) {
  const resolution = "MyLittleSecret" + Math.random();
  let found = false;

  await front.attach();
  await front.listPromises();

  const onNewPromise = new Promise(resolve => {
    front.on("promises-settled", promises => {
      for (const p of promises) {
        if (
          p.promiseState.state === "fulfilled" &&
          p.promiseState.value === resolution
        ) {
          const timeToSettle =
            Math.floor(p.promiseState.timeToSettle / 100) * 100;
          ok(
            timeToSettle >= 100,
            "Expect time to settle for resolved promise to be " +
              "at least 100ms, got " +
              timeToSettle +
              "ms."
          );
          found = true;
          resolve();
        } else {
          dump("Found non-target promise.\n");
        }
      }
    });
  });

  const promise = makePromise(resolution);

  await onNewPromise;
  ok(found, "Found our new promise.");
  await front.detach();
  // Appease eslint
  void promise;
}
