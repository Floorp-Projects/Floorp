/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the 3 pane toggle button can toggle on and off the inspector's 3 pane mode,
// and the 3 panes rendered are all of equal widths in the BOTTOM host.

add_task(async function() {
  info("Switch to 2 pane inspector to test the 3 pane toggle button behavior");
  await pushPref("devtools.inspector.three-pane-enabled", false);

  const { inspector } = await openInspectorForURL("about:blank");
  const { panelDoc: doc } = inspector;
  const button = doc.querySelector(".sidebar-toggle");
  const ruleViewSidebar = inspector.sidebarSplitBox.startPanelContainer;
  const toolboxWidth = doc.getElementById("inspector-splitter-box").clientWidth;

  ok(button.classList.contains("pane-collapsed"), "The button is in collapsed state");

  info("Click on the toggle button to toggle ON 3 pane inspector");
  let onRuleViewAdded = inspector.once("ruleview-added");
  EventUtils.synthesizeMouseAtCenter(button, {}, inspector.panelDoc.defaultView);
  await onRuleViewAdded;

  info("Checking the state of the 3 pane inspector");
  let sidebarWidth = inspector.splitBox.state.width;
  const sidebarSplitBoxWidth = inspector.sidebarSplitBox.state.width;
  ok(!button.classList.contains("pane-collapsed"), "The button is in expanded state");
  ok(doc.getElementById("ruleview-panel"), "The rule view panel exist");
  is(inspector.sidebar.getCurrentTabID(), "layoutview",
    "Layout view is shown in the sidebar");
  is(ruleViewSidebar.style.display, "block", "The split rule view sidebar is displayed");
  is(sidebarWidth, toolboxWidth * 2 / 3, "Got correct main split box width");
  is(sidebarSplitBoxWidth, toolboxWidth / 3, "Got correct sidebar split box width");

  info("Click on the toggle button to toggle OFF the 3 pane inspector");
  onRuleViewAdded = inspector.once("ruleview-added");
  EventUtils.synthesizeMouseAtCenter(button, {}, inspector.panelDoc.defaultView);
  await onRuleViewAdded;

  info("Checking the state of the 2 pane inspector");
  sidebarWidth = inspector.splitBox.state.width;
  ok(button.classList.contains("pane-collapsed"), "The button is in collapsed state");
  is(inspector.sidebar.getCurrentTabID(), "ruleview",
    "Rule view is shown in the sidebar");
  is(ruleViewSidebar.style.display, "none", "The split rule view sidebar is hidden");
  is(sidebarWidth, sidebarSplitBoxWidth, "Got correct sidebar width");
});
