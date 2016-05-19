/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get the approximate time range for promise creation timestamp.
 */

"use strict";

const { PromisesFront } = require("devtools/server/actors/promises");

var events = require("sdk/event/core");

add_task(function* () {
  let client = yield startTestDebuggerServer("promises-object-test");
  let chromeActors = yield getChromeActors(client);

  ok(Promise.toString().includes("native code"), "Expect native DOM Promise.");

  // We have to attach the chrome TabActor before playing with the PromiseActor
  yield attachTab(client, chromeActors);
  yield testPromiseCreationTimestamp(client, chromeActors, v => {
    return new Promise(resolve => resolve(v));
  });

  let response = yield listTabs(client);
  let targetTab = findTab(response.tabs, "promises-object-test");
  ok(targetTab, "Found our target tab.");

  yield testPromiseCreationTimestamp(client, targetTab, v => {
    const debuggee = DebuggerServer.getTestGlobal("promises-object-test");
    return debuggee.Promise.resolve(v);
  });

  yield close(client);
});

function* testPromiseCreationTimestamp(client, form, makePromise) {
  let front = PromisesFront(client, form);
  let resolution = "MyLittleSecret" + Math.random();

  yield front.attach();
  yield front.listPromises();

  let onNewPromise = new Promise(resolve => {
    events.on(front, "new-promises", promises => {
      for (let p of promises) {
        if (p.promiseState.state === "fulfilled" &&
            p.promiseState.value === resolution) {
          resolve(p);
        }
      }
    });
  });

  let start = Date.now();
  let promise = makePromise(resolution);
  let end = Date.now();

  let grip = yield onNewPromise;
  ok(grip, "Found our new promise.");

  let creationTimestamp = grip.promiseState.creationTimestamp;

  ok(start - 1 <= creationTimestamp && creationTimestamp <= end + 1,
    "Expect promise creation timestamp to be within elapsed time range.");

  yield front.detach();
  // Appease eslint
  void promise;
}
