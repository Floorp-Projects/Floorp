/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/shared-head.js",
  this
);

function getContainerForNodeFront(nodeFront, {markup}) {
  return markup.getContainer(nodeFront);
}

// Test basic inspector functionality in web replay: the inspector is able to
// show contents when paused according to the child's current position.
add_task(async function() {
  const dbg = await attachRecordingDebugger(
    "doc_inspector_basic.html",
    { waitForRecording: true }
  );
  const {threadClient, tab, toolbox} = dbg;
  await threadClient.resume();

  const {inspector} = await openInspector();

  let nodeFront = await getNodeFront("#maindiv", inspector);
  let container = getContainerForNodeFront(nodeFront, inspector);
  ok(!container, "No node container while unpaused");

  await threadClient.interrupt();

  nodeFront = await getNodeFront("#maindiv", inspector);
  await waitFor(() => inspector.markup && getContainerForNodeFront(nodeFront, inspector));
  container = getContainerForNodeFront(nodeFront, inspector);
  ok(container.editor.textEditor.value.textContent == "GOODBYE",
     "Correct late element text");

  const bp = await setBreakpoint(threadClient, "doc_inspector_basic.html", 9);

  await rewindToLine(threadClient, 9);

  nodeFront = await getNodeFront("#maindiv", inspector);
  await waitFor(() => inspector.markup && getContainerForNodeFront(nodeFront, inspector));
  container = getContainerForNodeFront(nodeFront, inspector);
  ok(container.editor.textEditor.value.textContent == "HELLO",
     "Correct early element text");

  await threadClient.removeBreakpoint(bp);
  await toolbox.closeToolbox();
  await gBrowser.removeTab(tab);
});
