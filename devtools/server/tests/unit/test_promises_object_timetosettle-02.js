/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

/**
 * Test that we get the expected settlement time for promise time to settle.
 */

"use strict";

const { PromisesFront } = require("devtools/shared/fronts/promises");
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm", {});

var EventEmitter = require("devtools/shared/event-emitter");

add_task(async function() {
  let client = await startTestDebuggerServer("test-promises-timetosettle");
  let chromeActors = await getChromeActors(client);
  await attachTab(client, chromeActors);

  ok(Promise.toString().includes("native code"), "Expect native DOM Promise.");

  // We have to attach the chrome TabActor before playing with the PromiseActor
  await attachTab(client, chromeActors);
  await testGetTimeToSettle(client, chromeActors,
    v => new Promise(resolve => setTimeout(() => resolve(v), 100)));

  let response = await listTabs(client);
  let targetTab = findTab(response.tabs, "test-promises-timetosettle");
  ok(targetTab, "Found our target tab.");
  await attachTab(client, targetTab);

  await testGetTimeToSettle(client, targetTab, v => {
    const debuggee =
      DebuggerServer.getTestGlobal("test-promises-timetosettle");
    return new debuggee.Promise(resolve => setTimeout(() => resolve(v), 100));
  });

  await close(client);
});

async function testGetTimeToSettle(client, form, makePromise) {
  let front = PromisesFront(client, form);
  let resolution = "MyLittleSecret" + Math.random();
  let found = false;

  await front.attach();
  await front.listPromises();

  let onNewPromise = new Promise(resolve => {
    EventEmitter.on(front, "promises-settled", promises => {
      for (let p of promises) {
        if (p.promiseState.state === "fulfilled" &&
            p.promiseState.value === resolution) {
          let timeToSettle = Math.floor(p.promiseState.timeToSettle / 100) * 100;
          ok(timeToSettle >= 100,
            "Expect time to settle for resolved promise to be " +
            "at least 100ms, got " + timeToSettle + "ms.");
          found = true;
          resolve();
        } else {
          dump("Found non-target promise.\n");
        }
      }
    });
  });

  let promise = makePromise(resolution);

  await onNewPromise;
  ok(found, "Found our new promise.");
  await front.detach();
  // Appease eslint
  void promise;
}
