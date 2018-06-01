/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can get the list of all new Promise objects from the
 * PromisesActor onNewPromise event handler.
 */

"use strict";

const { PromisesFront } = require("devtools/shared/fronts/promises");

var EventEmitter = require("devtools/shared/event-emitter");

add_task(async function() {
  const client = await startTestDebuggerServer("promises-actor-test");
  const parentProcessActors = await getParentProcessActors(client);

  ok(Promise.toString().includes("native code"), "Expect native DOM Promise");

  // We have to attach the chrome target actor before playing with the PromiseActor
  await attachTab(client, parentProcessActors);
  await testNewPromisesEvent(client, parentProcessActors,
    v => new Promise(resolve => resolve(v)));

  const response = await listTabs(client);
  const targetTab = findTab(response.tabs, "promises-actor-test");
  ok(targetTab, "Found our target tab.");

  await testNewPromisesEvent(client, targetTab, v => {
    const debuggee = DebuggerServer.getTestGlobal("promises-actor-test");
    return debuggee.Promise.resolve(v);
  });

  await close(client);
});

async function testNewPromisesEvent(client, form, makePromise) {
  const front = PromisesFront(client, form);
  const resolution = "MyLittleSecret" + Math.random();
  let found = false;

  await front.attach();
  await front.listPromises();

  const onNewPromise = new Promise(resolve => {
    EventEmitter.on(front, "new-promises", promises => {
      for (const p of promises) {
        equal(p.type, "object", "Expect type to be Object");
        equal(p.class, "Promise", "Expect class to be Promise");
        equal(typeof p.promiseState.creationTimestamp, "number",
          "Expect creation timestamp to be a number");

        if (p.promiseState.state === "fulfilled" &&
            p.promiseState.value === resolution) {
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
