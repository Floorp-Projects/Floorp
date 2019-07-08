/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can get the list of Promise objects that have settled from the
 * PromisesActor onPromiseSettled event handler.
 */

"use strict";

ChromeUtils.import("resource://testing-common/PromiseTestUtils.jsm", this);

add_task(async function() {
  const { promisesFront } = await createMainProcessPromisesFront();

  ok(Promise.toString().includes("native code"), "Expect native DOM Promise");

  await testPromisesSettled(
    promisesFront,
    v => new Promise(resolve => resolve(v)),
    v => new Promise((resolve, reject) => reject(v))
  );
});

add_task(async function() {
  const { debuggee, promisesFront } = await createTabPromisesFront();

  await testPromisesSettled(
    promisesFront,
    v => {
      return debuggee.Promise.resolve(v);
    },
    v => {
      return debuggee.Promise.reject(v);
    }
  );
});

async function testPromisesSettled(
  front,
  makeResolvePromise,
  makeRejectPromise
) {
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
    front.on("promises-settled", promises => {
      for (const p of promises) {
        equal(p.type, "object", "Expect type to be Object");
        equal(p.class, "Promise", "Expect class to be Promise");
        equal(
          typeof p.promiseState.creationTimestamp,
          "number",
          "Expect creation timestamp to be a number"
        );
        equal(
          typeof p.promiseState.timeToSettle,
          "number",
          "Expect time to settle to be a number"
        );

        if (
          p.promiseState.state === "fulfilled" &&
          p.promiseState.value === resolution
        ) {
          resolve(resolveValue);
        } else if (
          p.promiseState.state === "rejected" &&
          p.promiseState.reason === resolution
        ) {
          resolve(rejectValue);
        } else {
          dump("Found non-target promise\n");
        }
      }
    });
  });
}
