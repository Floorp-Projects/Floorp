/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const TEST_URL_1 = `http://example.org/document-builder.sjs?html=
<meta charset=utf8><iframe srcdoc="<div>"></iframe><body>`;
const TEST_URL_2 = `http://example.org/document-builder.sjs?html=<meta charset=utf8><div id=url2>`;

// Test that deleting a node in a same-process iframe and doing a navigation
// does not freeze the browser or break the toolbox.
add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL_1);

  info("Select a node in a same-process iframe");
  const node = await selectNodeInFrames(["iframe", "div"], inspector);
  const parentNode = node.parentNode();

  info("Removing selected element with the context menu.");
  await deleteNodeWithContextMenu(node, inspector);

  is(
    inspector.selection.nodeFront,
    parentNode,
    "The parent node of the deleted node was selected."
  );

  const onInspectorReloaded = inspector.once("reloaded");
  await navigateTo(TEST_URL_2);
  await onInspectorReloaded;

  const url2NodeFront = await getNodeFront("#url2", inspector);
  ok(url2NodeFront, "Can retrieve a node front after the navigation");
});
