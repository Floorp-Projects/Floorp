/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can get the list of all live Promise objects from the
 * PromisesActor.
 */

"use strict";

const SECRET = "MyLittleSecret";

add_task(async function() {
  const { promisesFront } = await createMainProcessPromisesFront();
  await testListPromises(promisesFront, v => {
    return new Promise(resolve => resolve(v));
  });
});

add_task(async function() {
  const { debuggee, promisesFront } = await createTabPromisesFront();
  await testListPromises(promisesFront, v => {
    return debuggee.Promise.resolve(v);
  });
});

async function testListPromises(front, makePromise) {
  const resolution = SECRET + Math.random();
  const promise = makePromise(resolution);

  await front.attach();

  const promises = await front.listPromises();

  let found = false;
  for (const p of promises) {
    equal(p.type, "object", "Expect type to be Object");
    equal(p.class, "Promise", "Expect class to be Promise");
    equal(
      typeof p.promiseState.creationTimestamp,
      "number",
      "Expect creation timestamp to be a number"
    );
    if (p.promiseState.state !== "pending") {
      equal(
        typeof p.promiseState.timeToSettle,
        "number",
        "Expect time to settle to be a number"
      );
    }

    if (
      p.promiseState.state === "fulfilled" &&
      p.promiseState.value === resolution
    ) {
      found = true;
    }
  }

  ok(found, "Found our promise");
  await front.detach();
  // Appease eslint
  void promise;
}
