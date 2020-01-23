/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

function checkJumpIcon(msg) {
  const jumpIcon = msg.querySelector(".jump-definition");
  ok(jumpIcon, "Found a jump icon");
}

// Test the objects produced by console.log() calls and by evaluating various
// expressions in the console after time warping.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_objects.html", {
    waitForRecording: true,
  });

  const { threadFront, toolbox } = dbg;
  const console = await toolbox.selectTool("webconsole");
  const hud = console.hud;
  let msg;

  await waitForMessage(hud, "Array(20) [ 0, 1, 2, 3, 4, 5,");
  await waitForMessage(hud, "Uint8Array(20) [ 0, 1, 2, 3, 4, 5,");
  await waitForMessage(hud, "Set(22) [ {…}, {…}, 0, 1, 2, 3, 4, 5,");
  await waitForMessage(
    hud,
    "Map(21) { {…} → {…}, 0 → 1, 1 → 2, 2 → 3, 3 → 4, 4 → 5,"
  );
  await waitForMessage(hud, "WeakSet(20) [ {…}, {…}, {…},");
  await waitForMessage(hud, "WeakMap(20) { {…} → {…}, {…} → {…},");
  await waitForMessage(
    hud,
    "Object { a: 0, a0: 0, a1: 1, a2: 2, a3: 3, a4: 4,"
  );
  await waitForMessage(hud, "/abc/gi");
  await waitForMessage(hud, "Date");

  // Note: this message has an associated stack but we don't have an easy way to
  // check its contents as BrowserTest.checkMessageStack requires the stack to
  // be collapsed.
  await waitForMessage(hud, 'RangeError: "foo"');

  msg = await waitForMessage(hud, "function bar()");
  checkJumpIcon(msg);

  await warpToMessage(hud, dbg, "Done");

  const requests = await threadFront.debuggerRequests();

  requests.forEach(({ request, stack }) => {
    if (request.type != "pauseData" && request.type != "repaint") {
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

  BrowserTest.execute(hud, `RegExp("abd", "g")`);
  msg = await waitForMessage(hud, "/abd/g");
  await checkMessageObjectContents(msg, ["global: true", `source: "abd"`]);

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

  BrowserTest.execute(hud, "baz");
  msg = await waitForMessage(hud, "function baz()");
  checkJumpIcon(msg);

  await shutdownDebugger(dbg);
});
