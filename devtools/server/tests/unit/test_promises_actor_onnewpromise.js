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
  let client = await startTestDebuggerServer("promises-actor-test");
  let chromeActors = await getChromeActors(client);

  ok(Promise.toString().includes("native code"), "Expect native DOM Promise");

  // We have to attach the chrome TabActor before playing with the PromiseActor
  await attachTab(client, chromeActors);
  await testNewPromisesEvent(client, chromeActors,
    v => new Promise(resolve => resolve(v)));

  let response = await listTabs(client);
  let targetTab = findTab(response.tabs, "promises-actor-test");
  ok(targetTab, "Found our target tab.");

  await testNewPromisesEvent(client, targetTab, v => {
    const debuggee = DebuggerServer.getTestGlobal("promises-actor-test");
    return debuggee.Promise.resolve(v);
  });

  await close(client);
});

async function testNewPromisesEvent(client, form, makePromise) {
  let front = PromisesFront(client, form);
  let resolution = "MyLittleSecret" + Math.random();
  let found = false;

  await front.attach();
  await front.listPromises();

  let onNewPromise = new Promise(resolve => {
    EventEmitter.on(front, "new-promises", promises => {
      for (let p of promises) {
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

  let promise = makePromise(resolution);

  await onNewPromise;
  ok(found, "Found our new promise");
  await front.detach();
  // Appease eslint
  void promise;
}
