/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test whether or not we get the time to settle depending on the state of the
 * promise.
 */

"use strict";

const { PromisesFront } = require("devtools/shared/fronts/promises");

var EventEmitter = require("devtools/shared/event-emitter");

add_task(async function() {
  const client = await startTestDebuggerServer("test-promises-timetosettle");
  const chromeActors = await getChromeActors(client);

  ok(Promise.toString().includes("native code"), "Expect native DOM Promise.");

  // We have to attach the chrome TabActor before playing with the PromiseActor
  await attachTab(client, chromeActors);
  await testGetTimeToSettle(client, chromeActors, () => {
    const p = new Promise(() => {});
    p.name = "p";
    const q = p.then();
    q.name = "q";

    return p;
  });

  const response = await listTabs(client);
  const targetTab = findTab(response.tabs, "test-promises-timetosettle");
  ok(targetTab, "Found our target tab.");

  await testGetTimeToSettle(client, targetTab, () => {
    const debuggee =
      DebuggerServer.getTestGlobal("test-promises-timetosettle");

    const p = new debuggee.Promise(() => {});
    p.name = "p";
    const q = p.then();
    q.name = "q";

    return p;
  });

  await close(client);
});

async function testGetTimeToSettle(client, form, makePromises) {
  const front = PromisesFront(client, form);

  await front.attach();
  await front.listPromises();

  const onNewPromise = new Promise(resolve => {
    EventEmitter.on(front, "new-promises", promises => {
      for (const p of promises) {
        if (p.promiseState.state === "pending") {
          ok(!p.promiseState.timeToSettle,
            "Expect no time to settle for unsettled promise.");
        } else {
          ok(p.promiseState.timeToSettle,
            "Expect time to settle for settled promise.");
          equal(typeof p.promiseState.timeToSettle, "number",
            "Expect time to settle to be a number.");
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
