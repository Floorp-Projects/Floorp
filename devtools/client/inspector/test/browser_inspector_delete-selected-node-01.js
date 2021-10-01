/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test to ensure inspector handles deletion of selected node correctly.

const TEST_URL = URL_ROOT + "doc_inspector_delete-selected-node-01.html";

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);

  const span = await getNodeFrontInFrames(["iframe", "span"], inspector);
  await selectNode(span, inspector);

  info("Removing selected <span> element.");
  const parentNode = span.parentNode();
  await inspector.walker.removeNode(span);

  // Wait for the inspector to process the mutation
  await inspector.once("inspector-updated");
  is(
    inspector.selection.nodeFront,
    parentNode,
    "Parent node of selected <span> got selected."
  );
});
