/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE_URL = URL_ROOT + "page_dom_nodes.html";

/**
 * Checks whether hovering nodes highlight them in the content page
 */
add_task(async function() {
  info("Test DOM panel node highlight started");

  const { panel } = await addTestTab(TEST_PAGE_URL);
  const toolbox = gDevTools.getToolbox(panel.target);

  info("Highlight the node by moving the cursor on it");
  let node = getRowByIndex(panel, 2).querySelector(".objectBox-node");
  // the inspector should be initialized first and then the node should
  // highlight after the hover effect.
  const inspectorFront = await toolbox.target.getFront("inspector");
  let onNodeHighlight = inspectorFront.highlighter.once("node-highlight");
  EventUtils.synthesizeMouseAtCenter(
    node,
    {
      type: "mouseover",
    },
    node.ownerDocument.defaultView
  );
  let nodeFront = await onNodeHighlight;
  is(nodeFront.displayName, "h1", "The correct node was highlighted");

  info("Unhighlight the node by moving away from the node");
  let onNodeUnhighlight = inspectorFront.highlighter.once("node-unhighlight");
  const btn = toolbox.doc.querySelector("#toolbox-meatball-menu-button");
  EventUtils.synthesizeMouseAtCenter(
    btn,
    {
      type: "mouseover",
    },
    btn.ownerDocument.defaultView
  );
  await onNodeUnhighlight;
  ok(true, "node-unhighlight event was fired when moving away from the node");

  info("Expand specified row and wait till children are displayed");
  await expandRow(panel, "_b");

  info("Highlight the node by moving the cursor on it");
  node = getRowByIndex(panel, 3).querySelector(".objectBox-node");

  EventUtils.synthesizeMouseAtCenter(
    node,
    {
      type: "mouseover",
    },
    node.ownerDocument.defaultView
  );
  onNodeHighlight = inspectorFront.highlighter.once("node-highlight");
  nodeFront = await onNodeHighlight;
  is(nodeFront.displayName, "h2", "The correct node was highlighted");

  info("Unhighlight the node by moving away from the node");
  EventUtils.synthesizeMouseAtCenter(
    btn,
    {
      type: "mouseover",
    },
    btn.ownerDocument.defaultView
  );
  onNodeUnhighlight = inspectorFront.highlighter.once("node-unhighlight");
  await onNodeUnhighlight;
  ok(true, "node-unhighlight event was fired when moving away from the node");
});
