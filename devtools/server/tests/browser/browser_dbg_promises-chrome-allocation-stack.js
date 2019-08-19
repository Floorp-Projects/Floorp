/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can get a stack to a promise's allocation point in the chrome
 * process.
 */

"use strict";

const SOURCE_URL = "browser_dbg_promises-chrome-allocation-stack.js";

const ObjectClient = require("devtools/shared/client/object-client");

const STACK_DATA = [
  { functionDisplayName: "test/<" },
  { functionDisplayName: "testGetAllocationStack" },
];

add_task(async function test() {
  requestLongerTimeout(10);

  // Start the DebuggerServer in a distinct loader as we want to debug system
  // compartments. The actors have to be in a distinct compartment than the
  // context they are debugging. `invisibleToDebugger` force loading modules in
  // a distinct compartments.
  const { DevToolsLoader } = ChromeUtils.import(
    "resource://devtools/shared/Loader.jsm"
  );
  const customLoader = new DevToolsLoader({
    invisibleToDebugger: true,
  });
  const { DebuggerServer } = customLoader.require(
    "devtools/server/debugger-server"
  );

  DebuggerServer.init();
  DebuggerServer.registerAllActors();
  DebuggerServer.allowChromeProcess = true;

  const client = new DebuggerClient(DebuggerServer.connectPipe());
  await client.connect();
  const target = await client.mainRoot.getMainProcess();
  await target.attach();

  await testGetAllocationStack(client, target, () => {
    const p = new Promise(() => {});
    p.name = "p";
    const q = p.then();
    q.name = "q";
    const r = p.catch(() => {});
    r.name = "r";
  });

  await target.destroy();
});

async function testGetAllocationStack(client, target, makePromises) {
  const front = await target.getFront("promises");

  await front.attach();
  await front.listPromises();

  // Get the grip for promise p
  const onNewPromise = new Promise(resolve => {
    front.on("new-promises", promises => {
      for (const p of promises) {
        if (
          p.preview.ownProperties.name &&
          p.preview.ownProperties.name.value === "p"
        ) {
          resolve(p);
        }
      }
    });
  });

  makePromises();

  const form = await onNewPromise;
  ok(form, "Found our promise p");

  const objectClient = new ObjectClient(client, form);
  ok(objectClient, "Got Object Client");

  const response = await objectClient.getPromiseAllocationStack();
  ok(response.allocationStack.length, "Got promise allocation stack.");

  for (let i = 0; i < STACK_DATA.length; i++) {
    const data = STACK_DATA[i];
    const stack = response.allocationStack[i];

    ok(stack.source.url.startsWith("chrome:"), "Got a chrome source URL");
    ok(stack.source.url.endsWith(SOURCE_URL), "Got correct source URL.");
    is(
      stack.functionDisplayName,
      data.functionDisplayName,
      "Got correct function display name."
    );
    is(typeof stack.line, "number", "Expect stack line to be a number.");
    is(typeof stack.column, "number", "Expect stack column to be a number.");
  }

  await front.detach();
}
