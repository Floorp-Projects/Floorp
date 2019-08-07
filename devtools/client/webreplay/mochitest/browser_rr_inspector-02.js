/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/shared-head.js",
  this
);

// Test that the element highlighter works when paused and replaying.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_inspector_basic.html", {
    waitForRecording: true,
  });
  const { threadFront, toolbox } = dbg;

  await threadFront.interrupt();
  await threadFront.resume();

  await threadFront.interrupt();
  const bp = await setBreakpoint(threadFront, "doc_inspector_basic.html", 9);
  await rewindToLine(threadFront, 9);

  const { inspector, testActor } = await openInspector();

  info("Waiting for element picker to become active.");
  toolbox.win.focus();
  await inspector.inspectorFront.nodePicker.start();

  info("Moving mouse over div.");
  await moveMouseOver("#maindiv", 1, 1);

  // Checks in isNodeCorrectlyHighlighted are off for an unknown reason, even
  // though the highlighting appears correctly in the UI.
  info("Performing checks");
  await testActor.isNodeCorrectlyHighlighted("#maindiv", is);

  await threadFront.removeBreakpoint(bp);
  await shutdownDebugger(dbg);

  function moveMouseOver(selector, x, y) {
    info("Waiting for element " + selector + " to be highlighted");
    testActor.synthesizeMouse({
      selector,
      x,
      y,
      options: { type: "mousemove" },
    });
    return inspector.inspectorFront.nodePicker.once("picker-node-hovered");
  }
});
