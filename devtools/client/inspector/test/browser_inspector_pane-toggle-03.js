/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the 3 pane inspector toggle can render the middle and right panels of equal
// sizes when the original sidebar can be doubled in width and be smaller than half the
// toolbox's width in the BOTTOM host.

const SIDEBAR_WIDTH = 200;

add_task(async function() {
  info("Switch to 2 pane inspector to test the 3 pane toggle button behavior");
  await pushPref("devtools.inspector.three-pane-enabled", false);

  const { inspector } = await openInspectorForURL("about:blank");
  const { panelDoc: doc } = inspector;
  const button = doc.querySelector(".sidebar-toggle");
  const toolboxWidth = doc.getElementById("inspector-splitter-box").clientWidth;

  if (toolboxWidth < 600) {
    ok(true, "Can't run the full test because the toolbox width is too small.");
  } else {
    info("Set the sidebar width to 200px");
    inspector.splitBox.setState({ width: SIDEBAR_WIDTH });

    info("Click on the toggle button to toggle ON 3 pane inspector");
    let onRuleViewAdded = inspector.once("ruleview-added");
    EventUtils.synthesizeMouseAtCenter(button, {}, inspector.panelDoc.defaultView);
    await onRuleViewAdded;

    info("Checking the sizes of the 3 pane inspector");
    let sidebarWidth = inspector.splitBox.state.width;
    const sidebarSplitBoxWidth = inspector.sidebarSplitBox.state.width;
    is(sidebarWidth, SIDEBAR_WIDTH * 2, "Got correct main split box width");
    is(sidebarSplitBoxWidth, SIDEBAR_WIDTH, "Got correct sidebar split box width");

    info("Click on the toggle button to toggle OFF the 3 pane inspector");
    onRuleViewAdded = inspector.once("ruleview-added");
    EventUtils.synthesizeMouseAtCenter(button, {}, inspector.panelDoc.defaultView);
    await onRuleViewAdded;

    info("Checking the sidebar size of the 2 pane inspector");
    sidebarWidth = inspector.splitBox.state.width;
    is(sidebarWidth, SIDEBAR_WIDTH, "Got correct sidebar width");
  }
});
