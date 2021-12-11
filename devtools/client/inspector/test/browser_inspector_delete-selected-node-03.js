/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test to ensure inspector can handle destruction of selected node inside an iframe.

const TEST_URL = URL_ROOT + "doc_inspector_delete-selected-node-01.html";

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);

  info("Select a node inside the iframe");
  await selectNodeInFrames(["iframe", "span"], inspector);

  info("Removing iframe.");
  const onInspectorUpdated = inspector.once("inspector-updated");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.document.querySelector("iframe").remove();
  });
  await onInspectorUpdated;

  const body = await getNodeFront("body", inspector);
  is(inspector.selection.nodeFront, body, "Selection is now the body node");
});
