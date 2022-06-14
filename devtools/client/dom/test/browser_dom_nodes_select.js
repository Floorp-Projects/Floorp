/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE_URL = URL_ROOT + "page_dom_nodes.html";

/**
 * Checks whether hovering nodes highlight them in the content page
 */
add_task(async function() {
  info("Test DOM panel node highlight started");

  const { panel, tab } = await addTestTab(TEST_PAGE_URL);
  const toolbox = await gDevTools.getToolboxForTab(tab);
  const node = getRowByIndex(panel, 0);

  // Loading the inspector panel at first, to make it possible to listen for
  // new node selections

  await toolbox.loadTool("inspector");
  const inspector = toolbox.getPanel("inspector");

  const openInInspectorIcon = node.querySelector(".open-inspector");
  ok(node !== null, "Node was logged as expected");

  info(
    "Clicking on the inspector icon and waiting for the " +
      "inspector to be selected"
  );
  const onInspectorSelected = toolbox.once("inspector-selected");
  const onInspectorUpdated = inspector.once("inspector-updated");
  const onNewNode = toolbox.selection.once("new-node-front");

  openInInspectorIcon.click();

  await onInspectorSelected;
  await onInspectorUpdated;
  const nodeFront = await onNewNode;

  ok(true, "Inspector selected and new node got selected");
  is(nodeFront.displayName, "h1", "The expected node was selected");
});
