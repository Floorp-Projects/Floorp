/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test whether or not we get the time to settle depending on the state of the
 * promise.
 */

"use strict";

const { PromisesFront } = require("devtools/shared/fronts/promises");

var EventEmitter = require("devtools/shared/event-emitter");

add_task(function* () {
  let client = yield startTestDebuggerServer("test-promises-timetosettle");
  let chromeActors = yield getChromeActors(client);

  ok(Promise.toString().includes("native code"), "Expect native DOM Promise.");

  // We have to attach the chrome TabActor before playing with the PromiseActor
  yield attachTab(client, chromeActors);
  yield testGetTimeToSettle(client, chromeActors, () => {
    let p = new Promise(() => {});
    p.name = "p";
    let q = p.then();
    q.name = "q";

    return p;
  });

  let response = yield listTabs(client);
  let targetTab = findTab(response.tabs, "test-promises-timetosettle");
  ok(targetTab, "Found our target tab.");

  yield testGetTimeToSettle(client, targetTab, () => {
    const debuggee =
      DebuggerServer.getTestGlobal("test-promises-timetosettle");

    let p = new debuggee.Promise(() => {});
    p.name = "p";
    let q = p.then();
    q.name = "q";

    return p;
  });

  yield close(client);
});

function* testGetTimeToSettle(client, form, makePromises) {
  let front = PromisesFront(client, form);

  yield front.attach();
  yield front.listPromises();

  let onNewPromise = new Promise(resolve => {
    EventEmitter.on(front, "new-promises", promises => {
      for (let p of promises) {
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

  let promise = makePromises();

  yield onNewPromise;
  yield front.detach();
  // Appease eslint
  void promise;
}
