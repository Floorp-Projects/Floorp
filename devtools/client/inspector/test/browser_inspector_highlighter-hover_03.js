/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that once a node has been hovered over and marked as such, if it is
// navigated away using the keyboard, the highlighter moves to the new node, and
// if it is then navigated back to, it is briefly highlighted again

const TEST_PAGE =
  "data:text/html;charset=utf-8," + '<p id="one">one</p><p id="two">two</p>';

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_PAGE);
  const { waitForHighlighterTypeShown } = getHighlighterTestHelpers(inspector);

  info("Making sure the markup-view frame is focused");
  inspector.markup._frame.focus();

  let highlightedNode = null;

  async function isHighlighting(selector, desc) {
    const nodeFront = await getNodeFront(selector, inspector);
    is(highlightedNode, nodeFront, desc);
  }

  async function waitForHighlighterShown() {
    const onShownData = await waitForHighlighterTypeShown(
      inspector.highlighters.TYPES.BOXMODEL
    );
    highlightedNode = onShownData.nodeFront;
  }

  async function waitForInspectorUpdated() {
    await inspector.once("inspector-updated");
  }

  info("Hover over <p#one> line in the markup-view");
  let onShown = waitForHighlighterShown();
  await hoverContainer("#one", inspector);
  await onShown;
  await isHighlighting("#one", "<p#one> is highlighted");

  info("Navigate to <p#two> with the keyboard");
  let onUpdated = waitForInspectorUpdated();
  EventUtils.synthesizeKey("VK_DOWN", {}, inspector.panelWin);
  await onUpdated;
  onUpdated = waitForInspectorUpdated();
  onShown = waitForHighlighterShown();
  EventUtils.synthesizeKey("VK_DOWN", {}, inspector.panelWin);
  await Promise.all([onShown, onUpdated]);
  await isHighlighting("#two", "<p#two> is highlighted");

  info("Navigate back to <p#one> with the keyboard");
  onUpdated = waitForInspectorUpdated();
  onShown = waitForHighlighterShown();
  EventUtils.synthesizeKey("VK_UP", {}, inspector.panelWin);
  await Promise.all([onShown, onUpdated]);
  await isHighlighting("#one", "<p#one> is highlighted again");
});
