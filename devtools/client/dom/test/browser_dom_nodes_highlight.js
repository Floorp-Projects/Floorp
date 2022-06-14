/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE_URL = URL_ROOT + "page_dom_nodes.html";

/**
 * Checks that hovering nodes highlights them in the content page
 */
add_task(async function() {
  info("Test DOM panel node highlight started");

  const { panel, tab } = await addTestTab(TEST_PAGE_URL);
  const toolbox = await gDevTools.getToolboxForTab(tab);
  const highlighter = toolbox.getHighlighter();

  const tests = [
    {
      expected: "h1",
      getNode: async () => {
        return getRowByIndex(panel, 0).querySelector(".objectBox-node");
      },
    },
    {
      expected: "h2",
      getNode: async () => {
        info("Expand specified row and wait till children are displayed");
        await expandRow(panel, "B");
        return getRowByIndex(panel, 1).querySelector(".objectBox-node");
      },
    },
  ];

  for (const test of tests) {
    info(`Get the NodeFront for ${test.expected}`);
    const node = await test.getNode();

    info("Highlight the node by moving the cursor on it");
    const onHighlighterShown = highlighter.waitForHighlighterShown();
    EventUtils.synthesizeMouseAtCenter(
      node,
      {
        type: "mouseover",
      },
      node.ownerDocument.defaultView
    );
    const { nodeFront } = await onHighlighterShown;
    is(
      nodeFront.displayName,
      test.expected,
      "The correct node was highlighted"
    );

    info("Unhighlight the node by moving the cursor away from it");
    const onHighlighterHidden = highlighter.waitForHighlighterHidden();
    const btn = toolbox.doc.querySelector("#toolbox-meatball-menu-button");
    EventUtils.synthesizeMouseAtCenter(
      btn,
      {
        type: "mouseover",
      },
      btn.ownerDocument.defaultView
    );

    const { nodeFront: unhighlightedNode } = await onHighlighterHidden;
    is(
      unhighlightedNode.displayName,
      test.expected,
      "The node was unhighlighted"
    );
  }
});
