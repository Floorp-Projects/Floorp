/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get the expected settlement time for promise time to settle.
 */

"use strict";

const { PromisesFront } = require("devtools/server/actors/promises");
const { setTimeout } = require("sdk/timers");

var events = require("sdk/event/core");

add_task(function*() {
  let client = yield startTestDebuggerServer("test-promises-timetosettle");
  let chromeActors = yield getChromeActors(client);

  ok(Promise.toString().contains("native code"), "Expect native DOM Promise.");

  yield testGetTimeToSettle(client, chromeActors,
    v => new Promise(resolve => setTimeout(() => resolve(v), 100)));

  let response = yield listTabs(client);
  let targetTab = findTab(response.tabs, "test-promises-timetosettle");
  ok(targetTab, "Found our target tab.");

  yield testGetTimeToSettle(client, targetTab, v => {
    const debuggee =
      DebuggerServer.getTestGlobal("test-promises-timetosettle");
    return new debuggee.Promise(resolve => setTimeout(() => resolve(v), 100));
  });

  yield close(client);
});

function* testGetTimeToSettle(client, form, makePromise) {
  let front = PromisesFront(client, form);
  let resolution = "MyLittleSecret" + Math.random();
  let found = false;

  yield front.attach();
  yield front.listPromises();

  let onNewPromise = new Promise(resolve => {
    events.on(front, "promises-settled", promises => {
      for (let p of promises) {
        if (p.promiseState.state === "fulfilled" &&
            p.promiseState.value === resolution) {
          equal(Math.floor(p.promiseState.timeToSettle / 100) * 100, 100,
            "Expect time to settle for resolved promise to be " +
            "approximately 100ms.");
          found = true;
          resolve();
        } else {
          dump("Found non-target promise.\n");
        }
      }
    });
  });

  let promise = makePromise(resolution);

  yield onNewPromise;
  ok(found, "Found our new promise.");
  yield front.detach();
  // Appease eslint
  void promise;
}
