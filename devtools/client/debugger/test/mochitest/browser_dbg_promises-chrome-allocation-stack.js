/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can get a stack to a promise's allocation point in the chrome
 * process.
 */

"use strict";

const SOURCE_URL = "browser_dbg_promises-chrome-allocation-stack.js";
const PromisesFront = require("devtools/shared/fronts/promises");
var events = require("sdk/event/core");

const STACK_DATA = [
  { functionDisplayName: "test/</<" },
  { functionDisplayName: "testGetAllocationStack" },
];

function test() {
  Task.spawn(function* () {
    requestLongerTimeout(10);

    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
    DebuggerServer.allowChromeProcess = true;

    let client = new DebuggerClient(DebuggerServer.connectPipe());
    yield connect(client);
    let chrome = yield client.getProcess();
    let [, tabClient] = yield attachTab(client, chrome.form);
    yield tabClient.attachThread();

    yield testGetAllocationStack(client, chrome.form, () => {
      let p = new Promise(() => {});
      p.name = "p";
      let q = p.then();
      q.name = "q";
      let r = p.then(null, () => {});
      r.name = "r";
    });

    yield close(client);
    finish();
  }).then(null, error => {
    ok(false, "Got an error: " + error.message + "\n" + error.stack);
  });
}

function* testGetAllocationStack(client, form, makePromises) {
  let front = PromisesFront(client, form);

  yield front.attach();
  yield front.listPromises();

  // Get the grip for promise p
  let onNewPromise = new Promise(resolve => {
    events.on(front, "new-promises", promises => {
      for (let p of promises) {
        if (p.preview.ownProperties.name &&
            p.preview.ownProperties.name.value === "p") {
          resolve(p);
        }
      }
    });
  });

  makePromises();

  let grip = yield onNewPromise;
  ok(grip, "Found our promise p");

  let objectClient = new ObjectClient(client, grip);
  ok(objectClient, "Got Object Client");

  yield new Promise(resolve => {
    objectClient.getPromiseAllocationStack(response => {
      ok(response.allocationStack.length, "Got promise allocation stack.");

      for (let i = 0; i < STACK_DATA.length; i++) {
        let data = STACK_DATA[i];
        let stack = response.allocationStack[i];

        ok(stack.source.url.startsWith("chrome:"), "Got a chrome source URL");
        ok(stack.source.url.endsWith(SOURCE_URL), "Got correct source URL.");
        is(stack.functionDisplayName, data.functionDisplayName,
           "Got correct function display name.");
        is(typeof stack.line, "number", "Expect stack line to be a number.");
        is(typeof stack.column, "number",
           "Expect stack column to be a number.");
      }

      resolve();
    });
  });

  yield front.detach();
}
