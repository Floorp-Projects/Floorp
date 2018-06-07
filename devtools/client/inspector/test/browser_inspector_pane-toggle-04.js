/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the 3 pane inspector toggle button can render the bottom-left and
// bottom-right panels of equal sizes in the SIDE host.

add_task(async function() {
  info("Switch to 2 pane inspector to test the 3 pane toggle button behavior");
  await pushPref("devtools.inspector.three-pane-enabled", false);

  const { inspector, toolbox } = await openInspectorForURL("about:blank");
  const { panelDoc: doc } = inspector;

  info("Switch the host to the right");
  await toolbox.switchHost("right");

  const button = doc.querySelector(".sidebar-toggle");
  const toolboxWidth = doc.getElementById("inspector-splitter-box").clientWidth;

  info("Click on the toggle button to toggle ON 3 pane inspector");
  let onRuleViewAdded = inspector.once("ruleview-added");
  EventUtils.synthesizeMouseAtCenter(button, {}, inspector.panelDoc.defaultView);
  await onRuleViewAdded;

  info("Checking the sizes of the 3 pane inspector");
  const sidebarSplitBoxWidth = inspector.sidebarSplitBox.state.width;
  is(sidebarSplitBoxWidth, toolboxWidth / 2, "Got correct sidebar split box width");

  info("Click on the toggle button to toggle OFF the 3 pane inspector");
  onRuleViewAdded = inspector.once("ruleview-added");
  EventUtils.synthesizeMouseAtCenter(button, {}, inspector.panelDoc.defaultView);
  await onRuleViewAdded;

  info("Checking the sidebar size of the 2 pane inspector");
  const sidebarWidth = inspector.splitBox.state.width;
  is(sidebarWidth, toolboxWidth, "Got correct sidebar width");
});
