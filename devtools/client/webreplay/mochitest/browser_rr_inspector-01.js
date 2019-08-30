/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/shared-head.js",
  this
);

function getContainerForNodeFront(nodeFront, { markup }) {
  return markup.getContainer(nodeFront);
}

// Test basic inspector functionality in web replay: the inspector is able to
// show contents when paused according to the child's current position.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_inspector_basic.html", {
    waitForRecording: true,
    disableLogging: true,
    skipInterrupt: true,
  });

  const { inspector } = await openInspector();

  let nodeFront = await getNodeFront("#maindiv", inspector);
  let container = getContainerForNodeFront(nodeFront, inspector);
  ok(!container, "No node container while unpaused");

  await interrupt(dbg);

  nodeFront = await getNodeFront("#maindiv", inspector);
  await waitFor(
    () => inspector.markup && getContainerForNodeFront(nodeFront, inspector)
  );
  container = getContainerForNodeFront(nodeFront, inspector);
  ok(
    container.editor.textEditor.textNode.state.value == "GOODBYE",
    "Correct late element text"
  );

  await addBreakpoint(dbg, "doc_inspector_basic.html", 9);
  await rewindToLine(dbg, 9);

  nodeFront = await getNodeFront("#maindiv", inspector);
  await waitFor(
    () => inspector.markup && getContainerForNodeFront(nodeFront, inspector)
  );
  container = getContainerForNodeFront(nodeFront, inspector);
  ok(
    container.editor.textEditor.textNode.state.value == "HELLO",
    "Correct early element text"
  );

  await shutdownDebugger(dbg);
});
