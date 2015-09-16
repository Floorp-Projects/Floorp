/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can get a stack to a promise's allocation point.
 */

"use strict";

const TAB_URL = EXAMPLE_URL + "doc_promise-get-allocation-stack.html";
const { PromisesFront } = require("devtools/server/actors/promises");
var events = require("sdk/event/core");

function test() {
  Task.spawn(function* () {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();

    const [ tab,, panel ] = yield initDebugger(TAB_URL);

    let client = new DebuggerClient(DebuggerServer.connectPipe());
    yield connect(client);

    let { tabs } = yield listTabs(client);
    let targetTab = findTab(tabs, TAB_URL);
    yield attachTab(client, targetTab);

    yield testGetAllocationStack(client, targetTab, tab);

    yield close(client);
    yield closeDebuggerAndFinish(panel);
  }).then(null, error => {
    ok(false, "Got an error: " + error.message + "\n" + error.stack);
  });
}

function* testGetAllocationStack(client, form, tab) {
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

  callInTab(tab, "makePromises");

  let grip = yield onNewPromise;
  ok(grip, "Found our promise p");

  let objectClient = new ObjectClient(client, grip);
  ok(objectClient, "Got Object Client");

  yield new Promise(resolve => {
    objectClient.getPromiseAllocationStack(response => {
      ok(response.allocationStack.length, "Got promise allocation stack.");

      for (let stack of response.allocationStack) {
        is(stack.source.url, TAB_URL, "Got correct source URL.");
        is(stack.functionDisplayName, "makePromises",
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
