/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test whether or not we get the time to settle depending on the state of the
 * promise.
 */

"use strict";

add_task(async function() {
  const { promisesFront } = await createMainProcessPromisesFront();

  ok(Promise.toString().includes("native code"), "Expect native DOM Promise.");

  await testGetTimeToSettle(promisesFront, () => {
    const p = new Promise(() => {});
    p.name = "p";
    const q = p.then();
    q.name = "q";

    return p;
  });
});

add_task(async function() {
  const { debuggee, promisesFront } = await createTabPromisesFront();

  await testGetTimeToSettle(promisesFront, () => {
    const p = new debuggee.Promise(() => {});
    p.name = "p";
    const q = p.then();
    q.name = "q";

    return p;
  });
});

async function testGetTimeToSettle(front, makePromises) {
  await front.attach();
  await front.listPromises();

  const onNewPromise = new Promise(resolve => {
    front.on("new-promises", promises => {
      for (const p of promises) {
        if (p.promiseState.state === "pending") {
          ok(
            !p.promiseState.timeToSettle,
            "Expect no time to settle for unsettled promise."
          );
        } else {
          ok(
            p.promiseState.timeToSettle,
            "Expect time to settle for settled promise."
          );
          equal(
            typeof p.promiseState.timeToSettle,
            "number",
            "Expect time to settle to be a number."
          );
        }
      }
      resolve();
    });
  });

  const promise = makePromises();

  await onNewPromise;
  await front.detach();
  // Appease eslint
  void promise;
}
