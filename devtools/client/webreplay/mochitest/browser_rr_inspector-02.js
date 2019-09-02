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
    disableLogging: true,
  });
  const { toolbox } = dbg;

  await addBreakpoint(dbg, "doc_inspector_basic.html", 9);
  await rewindToLine(dbg, 9);

  const { testActor } = await openInspector();

  info("Waiting for element picker to become active.");
  toolbox.win.focus();
  await toolbox.nodePicker.start();

  info("Moving mouse over div.");
  await moveMouseOver("#maindiv", 1, 1);

  // Checks in isNodeCorrectlyHighlighted are off for an unknown reason, even
  // though the highlighting appears correctly in the UI.
  info("Performing checks");
  await testActor.isNodeCorrectlyHighlighted("#maindiv", is);

  await shutdownDebugger(dbg);

  function moveMouseOver(selector, x, y) {
    info("Waiting for element " + selector + " to be highlighted");
    testActor.synthesizeMouse({
      selector,
      x,
      y,
      options: { type: "mousemove" },
    });
    return toolbox.nodePicker.once("picker-node-hovered");
  }
});
