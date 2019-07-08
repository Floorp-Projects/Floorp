/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can get the list of all new Promise objects from the
 * PromisesActor onNewPromise event handler.
 */

"use strict";

add_task(async function() {
  const { promisesFront } = await createMainProcessPromisesFront();
  ok(Promise.toString().includes("native code"), "Expect native DOM Promise");
  await testNewPromisesEvent(promisesFront, v => {
    return new Promise(resolve => resolve(v));
  });
});

add_task(async function() {
  const { debuggee, promisesFront } = await createTabPromisesFront();

  await testNewPromisesEvent(promisesFront, v => {
    return debuggee.Promise.resolve(v);
  });
});

async function testNewPromisesEvent(front, makePromise) {
  const resolution = "MyLittleSecret" + Math.random();
  let found = false;

  await front.attach();
  await front.listPromises();

  const onNewPromise = new Promise(resolve => {
    front.on("new-promises", promises => {
      for (const p of promises) {
        equal(p.type, "object", "Expect type to be Object");
        equal(p.class, "Promise", "Expect class to be Promise");
        equal(
          typeof p.promiseState.creationTimestamp,
          "number",
          "Expect creation timestamp to be a number"
        );

        if (
          p.promiseState.state === "fulfilled" &&
          p.promiseState.value === resolution
        ) {
          found = true;
          resolve();
        } else {
          dump("Found non-target promise\n");
        }
      }
    });
  });

  const promise = makePromise(resolution);

  await onNewPromise;
  ok(found, "Found our new promise");
  await front.detach();
  // Appease eslint
  void promise;
}
