/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { L10N } = require("devtools/client/accessibility/utils/l10n");

// Check that DOM nodes in the sidebar can be highlighted and that clicking on the icon
// next to them opens the inspector.

const TEST_URI = `<html>
  <head>
    <meta charset="utf-8"/>
    <title>Accessibility Panel Sidebar DOM Nodes Test</title>
  </head>
  <body>
    <h1 id="select-me">Hello</h1>
  </body>
</html>`;

/**
 * Test that checks the Accessibility panel sidebar.
 */
addA11YPanelTask(
  "Check behavior of DOM nodes in side panel",
  TEST_URI,
  async ({ toolbox, doc }) => {
    info("Select an item having an actual associated DOM node");
    await toggleRow(doc, 0);
    selectRow(doc, 1);

    await BrowserTestUtils.waitForCondition(
      () => getPropertyValue(doc, "name") === `"Hello"`,
      "Wait until the sidebar is updated"
    );

    info("Check DOMNode");
    const domNodeEl = getPropertyItem(doc, "DOMNode");
    ok(domNodeEl, "The DOMNode item was retrieved");

    const openInspectorButton = domNodeEl.querySelector(".open-inspector");
    ok(openInspectorButton, "The open inspector button is displayed");
    is(
      openInspectorButton.getAttribute("title"),
      L10N.getStr("accessibility.accessible.selectNodeInInspector.title"),
      "The open inspector button has expected title"
    );

    info("Check that hovering DOMNode triggers the highlight");
    // Loading the inspector panel at first, to make it possible to listen for
    // new node selections
    await toolbox.loadTool("inspector");
    const highlighter = toolbox.getHighlighter();
    const highlighterTestFront = await getHighlighterTestFront(toolbox);

    const onHighlighterShown = highlighter.waitForHighlighterShown();

    EventUtils.synthesizeMouseAtCenter(
      openInspectorButton,
      { type: "mousemove" },
      doc.defaultView
    );

    const { nodeFront } = await onHighlighterShown;
    is(nodeFront.displayName, "h1", "The correct node was highlighted");
    isVisible = await highlighterTestFront.isHighlighting();
    ok(isVisible, "Highlighter is displayed");

    info("Unhighlight the node by moving away from the node");
    const onHighlighterHidden = highlighter.waitForHighlighterHidden();
    EventUtils.synthesizeMouseAtCenter(
      getPropertyItem(doc, "name"),
      { type: "mousemove" },
      doc.defaultView
    );

    await onHighlighterHidden;
    ok(true, "The highlighter was closed when moving away from the node");

    info(
      "Clicking on the inspector icon and waiting for the inspector to be selected"
    );
    const onNewNode = toolbox.selection.once("new-node-front");
    openInspectorButton.click();
    const inspectorSelectedNodeFront = await onNewNode;

    ok(true, "Inspector selected and new node got selected");
    is(
      inspectorSelectedNodeFront.id,
      "select-me",
      "The expected node was selected"
    );
  }
);

function getPropertyItem(doc, label) {
  const labelEl = Array.from(
    doc.querySelectorAll("#accessibility-properties .object-label")
  ).find(el => el.textContent === label);
  if (!labelEl) {
    return null;
  }
  return labelEl.closest(".node");
}

function getPropertyValue(doc, label) {
  return getPropertyItem(doc, label)?.querySelector(".object-value")
    ?.textContent;
}
