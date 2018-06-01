/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can get the list of dependent promises from the ObjectClient.
 */

"use strict";

const { PromisesFront } = require("devtools/shared/fronts/promises");

var EventEmitter = require("devtools/shared/event-emitter");

add_task(async function() {
  const client = await startTestDebuggerServer("test-promises-dependentpromises");
  const parentProcessActors = await getParentProcessActors(client);
  await attachTab(client, parentProcessActors);

  ok(Promise.toString().includes("native code"), "Expect native DOM Promise.");

  await testGetDependentPromises(client, parentProcessActors, () => {
    const p = new Promise(() => {});
    p.name = "p";
    const q = p.then();
    q.name = "q";
    const r = p.catch(() => {});
    r.name = "r";

    return p;
  });

  const response = await listTabs(client);
  const targetTab = findTab(response.tabs, "test-promises-dependentpromises");
  ok(targetTab, "Found our target tab.");
  await attachTab(client, targetTab);

  await testGetDependentPromises(client, targetTab, () => {
    const debuggee =
      DebuggerServer.getTestGlobal("test-promises-dependentpromises");

    const p = new debuggee.Promise(() => {});
    p.name = "p";
    const q = p.then();
    q.name = "q";
    const r = p.catch(() => {});
    r.name = "r";

    return p;
  });

  await close(client);
});

async function testGetDependentPromises(client, form, makePromises) {
  const front = PromisesFront(client, form);

  await front.attach();
  await front.listPromises();

  // Get the grip for promise p
  const onNewPromise = new Promise(resolve => {
    EventEmitter.on(front, "new-promises", promises => {
      for (const p of promises) {
        if (p.preview.ownProperties.name &&
            p.preview.ownProperties.name.value === "p") {
          resolve(p);
        }
      }
    });
  });

  const promise = makePromises();

  const grip = await onNewPromise;
  ok(grip, "Found our promise p.");

  const objectClient = new ObjectClient(client, grip);
  ok(objectClient, "Got Object Client.");

  // Get the dependent promises for promise p and assert that the list of
  // dependent promises is correct
  await new Promise(resolve => {
    objectClient.getDependentPromises(response => {
      const dependentNames = response.promises.map(p =>
        p.preview.ownProperties.name.value);
      const expectedDependentNames = ["q", "r"];

      equal(dependentNames.length, expectedDependentNames.length,
        "Got expected number of dependent promises.");

      for (let i = 0; i < dependentNames.length; i++) {
        equal(dependentNames[i], expectedDependentNames[i],
          "Got expected dependent name.");
      }

      for (const p of response.promises) {
        equal(p.type, "object", "Expect type to be Object.");
        equal(p.class, "Promise", "Expect class to be Promise.");
        equal(typeof p.promiseState.creationTimestamp, "number",
          "Expect creation timestamp to be a number.");
        ok(!p.promiseState.timeToSettle,
          "Expect time to settle to be undefined.");
      }

      resolve();
    });
  });

  await front.detach();
  // Appease eslint
  void promise;
}
