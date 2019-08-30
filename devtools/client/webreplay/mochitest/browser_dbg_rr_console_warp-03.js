/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

const BrowserTest = {
  gTestPath,
  ok,
  registerCleanupFunction,
  waitForExplicitFinish,
  BrowserTestUtils,
};

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/webconsole/test/browser/head.js",
  BrowserTest
);

async function checkMessageObjectContents(msg, expected, expandList = []) {
  const oi = msg.querySelector(".tree");
  const node = oi.querySelector(".tree-node");
  BrowserTest.expandObjectInspectorNode(node);

  for (const label of expandList) {
    const labelNode = await waitFor(() =>
      BrowserTest.findObjectInspectorNode(oi, label)
    );
    BrowserTest.expandObjectInspectorNode(labelNode);
  }

  const properties = await waitFor(() => {
    const nodes = BrowserTest.getObjectInspectorNodes(oi);
    if (nodes && nodes.length > 1) {
      return [...nodes].map(n => n.textContent);
    }
    return null;
  });

  expected.forEach(s => {
    ok(properties.find(v => v.includes(s)), `Object contents include "${s}"`);
  });
}

// Test evaluating various expressions in the console after time warping.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_objects.html", {
    waitForRecording: true,
  });

  const { threadFront, toolbox } = dbg;
  const console = await toolbox.selectTool("webconsole");
  const hud = console.hud;

  await warpToMessage(hud, dbg, "Done");

  const requests = await threadFront.debuggerRequests();

  requests.forEach(({ request, stack }) => {
    if (request.type != "pauseData") {
      dump(`Unexpected debugger request stack:\n${stack}\n`);
      ok(false, `Unexpected debugger request while paused: ${request.type}`);
    }
  });

  BrowserTest.execute(hud, "Error('helo')");
  await waitForMessage(hud, "helo");

  BrowserTest.execute(
    hud,
    `
function f() {
  throw Error("there");
}
f();
`
  );
  await BrowserTest.checkMessageStack(hud, "Error: there", [3, 5]);

  let msg;

  BrowserTest.execute(hud, "Array(1, 2, 3)");
  msg = await waitForMessage(hud, "Array(3) [ 1, 2, 3 ]");
  await checkMessageObjectContents(msg, ["0: 1", "1: 2", "2: 3", "length: 3"]);

  BrowserTest.execute(hud, "new Uint8Array([1, 2, 3, 4])");
  msg = await waitForMessage(hud, "Uint8Array(4) [ 1, 2, 3, 4 ]");
  await checkMessageObjectContents(msg, [
    "0: 1",
    "1: 2",
    "2: 3",
    "3: 4",
    "byteLength: 4",
    "byteOffset: 0",
  ]);

  BrowserTest.execute(hud, `RegExp("abc", "g")`);
  msg = await waitForMessage(hud, "/abc/g");
  await checkMessageObjectContents(msg, ["global: true", `source: "abc"`]);

  BrowserTest.execute(hud, "new Set([1, 2, 3])");
  msg = await waitForMessage(hud, "Set(3) [ 1, 2, 3 ]");
  await checkMessageObjectContents(
    msg,
    ["0: 1", "1: 2", "2: 3", "size: 3"],
    ["<entries>"]
  );

  BrowserTest.execute(hud, "new Map([[1, {a:1}], [2, {b:2}]])");
  msg = await waitForMessage(hud, "Map { 1 → {…}, 2 → {…} }");
  await checkMessageObjectContents(
    msg,
    ["0: 1 → Object { … }", "1: 2 → Object { … }", "size: 2"],
    ["<entries>"]
  );

  BrowserTest.execute(hud, "new WeakSet([{a:1}, {b:2}])");
  msg = await waitForMessage(hud, "WeakSet [ {…}, {…} ]");
  await checkMessageObjectContents(
    msg,
    ["0: Object { … }", "1: Object { … }"],
    ["<entries>"]
  );

  BrowserTest.execute(hud, "new WeakMap([[{a:1},{b:1}], [{a:2},{b:2}]])");
  msg = await waitForMessage(hud, "WeakMap { {…} → {…}, {…} → {…} }");
  await checkMessageObjectContents(
    msg,
    ["0: Object { … } → Object { … }", "1: Object { … } → Object { … }"],
    ["<entries>"]
  );

  await shutdownDebugger(dbg);
});
