/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that clicking the DOM node button in any ObjectInspect
// opens the Inspector panel

"use strict";

add_task(async function () {
  // Ensures the end panel is wide enough to show the inspector icon
  await pushPref("devtools.debugger.end-panel-size", 600);
  // Disable 3-pane inspector as it might trigger unwanted server communication.
  await pushPref("devtools.inspector.three-pane-enabled", false);

  const dbg = await initDebugger("doc-script-switching.html");
  const { toolbox } = dbg;
  const highlighterTestFront = await getHighlighterTestFront(toolbox);
  const highlighter = toolbox.getHighlighter();

  // Bug 1562165: the WhyPaused element is displayed for a few hundred ms when adding an
  // expression, which can break synthesizeMouseAtCenter. So here we wait for the
  // whyPaused element to be displayed then hidden before testing the highlight feature.
  const onWhyPausedDisplayed = waitUntil(() =>
    dbg.win.document.querySelector(".why-paused")
  );
  await addExpression(dbg, "window.document.querySelector('button')");
  // TODO: Remove when Bug 1562165 lands.
  await onWhyPausedDisplayed;
  // TODO: Remove when Bug 1562165 lands.
  await waitUntil(() => !dbg.win.document.querySelector(".why-paused"));

  info(
    "Check that hovering over DOM element highlights the node in content panel"
  );
  let onNodeHighlight = highlighter.waitForHighlighterShown();

  info("Mouseover the open in inspector button");
  const inspectorNode = await waitFor(() => findElement(dbg, "openInspector"));
  const view = inspectorNode.ownerDocument.defaultView;
  EventUtils.synthesizeMouseAtCenter(
    inspectorNode,
    { type: "mouseover" },
    view
  );

  info("Wait for highligther to be shown");
  const { nodeFront } = await onNodeHighlight;
  is(nodeFront.displayName, "button", "The correct node was highlighted");

  info("Check that moving the mouse away from the node hides the highlighter");
  let onNodeUnhighlight = highlighter.waitForHighlighterHidden();
  const nonHighlightEl = inspectorNode.closest(".object-node");
  EventUtils.synthesizeMouseAtCenter(
    nonHighlightEl,
    { type: "mouseover" },
    view
  );

  await onNodeUnhighlight;
  isVisible = await highlighterTestFront.isHighlighting();
  is(isVisible, false, "The highlighter is not displayed anymore");

  info("Check we don't have zombie highlighters when briefly hovering a node");
  onNodeHighlight = highlighter.waitForHighlighterShown();
  onNodeUnhighlight = highlighter.waitForHighlighterHidden();

  // Move hover the node and then, right after, move out.
  EventUtils.synthesizeMouseAtCenter(
    inspectorNode,
    { type: "mousemove" },
    view
  );
  EventUtils.synthesizeMouseAtCenter(
    nonHighlightEl,
    { type: "mousemove" },
    view
  );

  await Promise.all([onNodeHighlight, onNodeUnhighlight]);
  isVisible = await highlighterTestFront.isHighlighting();
  is(isVisible, false, "The highlighter is not displayed anymore - no zombie");

  info("Ensure panel changes when button is clicked");
  // Loading the inspector panel at first, to make it possible to listen for
  // new node selections
  const inspector = await toolbox.loadTool("inspector");
  const onInspectorSelected = toolbox.once("inspector-selected");
  const onInspectorUpdated = inspector.once("inspector-updated");
  const onNewNode = toolbox.selection.once("new-node-front");

  inspectorNode.click();

  await onInspectorSelected;
  await onInspectorUpdated;
  const inspectorNodeFront = await onNewNode;

  ok(true, "Inspector selected and new node got selected");
  is(
    inspectorNodeFront.displayName,
    "button",
    "The expected node was selected"
  );
});

add_task(async function () {
  // Disable 3-pane inspector as it might trigger unwanted server communication.
  await pushPref("devtools.inspector.three-pane-enabled", false);

  const dbg = await initDebugger("doc-event-handler.html");
  const { toolbox } = dbg;

  invokeInTab("synthesizeClick");
  await waitForPaused(dbg);

  findElement(dbg, "frame", 2).focus();
  clickElement(dbg, "frame", 2);
  await waitForPaused(dbg);
  await waitForSelectedSource(dbg, "doc-event-handler.html");

  // Hover over the token to launch preview popup
  await tryHovering(dbg, 5, 8, "popup");

  info("Wait for top level node to expand and child nodes to load");
  await waitUntil(
    () => dbg.win.document.querySelectorAll(".preview-popup .node").length > 1
  );

  // Click the first inspector button to view node in inspector
  await waitForElement(dbg, "openInspector");

  // Loading the inspector panel at first, to make it possible to listen for
  // new node selections
  const inspector = await toolbox.loadTool("inspector");

  const onInspectorSelected = toolbox.once("inspector-selected");
  const onInspectorUpdated = inspector.once("inspector-updated");
  const onNewNode = toolbox.selection.once("new-node-front");

  findElement(dbg, "openInspector").click();

  await onInspectorSelected;
  await onInspectorUpdated;
  await onNewNode;

  ok(true, "Inspector selected and new node got selected");
});
