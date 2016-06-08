/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can get the list of all new Promise objects from the
 * PromisesActor onNewPromise event handler.
 */

"use strict";

const { PromisesFront } = require("devtools/shared/fronts/promises");

var events = require("sdk/event/core");

add_task(function* () {
  let client = yield startTestDebuggerServer("promises-actor-test");
  let chromeActors = yield getChromeActors(client);

  ok(Promise.toString().includes("native code"), "Expect native DOM Promise");

  // We have to attach the chrome TabActor before playing with the PromiseActor
  yield attachTab(client, chromeActors);
  yield testNewPromisesEvent(client, chromeActors,
    v => new Promise(resolve => resolve(v)));

  let response = yield listTabs(client);
  let targetTab = findTab(response.tabs, "promises-actor-test");
  ok(targetTab, "Found our target tab.");

  yield testNewPromisesEvent(client, targetTab, v => {
    const debuggee = DebuggerServer.getTestGlobal("promises-actor-test");
    return debuggee.Promise.resolve(v);
  });

  yield close(client);
});

function* testNewPromisesEvent(client, form, makePromise) {
  let front = PromisesFront(client, form);
  let resolution = "MyLittleSecret" + Math.random();
  let found = false;

  yield front.attach();
  yield front.listPromises();

  let onNewPromise = new Promise(resolve => {
    events.on(front, "new-promises", promises => {
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

  yield onNewPromise;
  ok(found, "Found our new promise");
  yield front.detach();
  // Appease eslint
  void promise;
}
