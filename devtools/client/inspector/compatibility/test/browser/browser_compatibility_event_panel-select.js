/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the behavior when the panel is selected.

const TEST_URI = "<body style='background-color: lime;'><div>test</div></body>";
const TEST_ANOTHER_URI = "<body></body>";

const {
  COMPATIBILITY_UPDATE_SELECTED_NODE_START,
  COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_START,
} = require("resource://devtools/client/inspector/compatibility/actions/index.js");

add_task(async function () {
  info(
    "Check that the panel does not update when no changes occur while hidden"
  );

  await pushPref("devtools.inspector.activeSidebar", "");

  const tab = await addTab(_toDataURL(TEST_URI));
  const { inspector } = await openCompatibilityView();

  info("Select another sidebar panel");
  await _selectSidebarPanel(inspector, "changesview");

  info("Select the compatibility panel again");
  let isSelectedNodeUpdated = false;
  let isTopLevelTargetUpdated = false;
  waitForDispatch(
    inspector.store,
    COMPATIBILITY_UPDATE_SELECTED_NODE_START
  ).then(() => {
    isSelectedNodeUpdated = true;
  });
  waitForDispatch(
    inspector.store,
    COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_START
  ).then(() => {
    isTopLevelTargetUpdated = true;
  });

  await _selectSidebarPanel(inspector, "compatibilityview");

  // Check above both flags after taking enough time.
  await wait(1000);

  ok(!isSelectedNodeUpdated, "Avoid updating the selected node pane");
  ok(!isTopLevelTargetUpdated, "Avoid updating the top level target pane");

  await removeTab(tab);
});

add_task(async function () {
  info(
    "Check that the panel only updates for the selected node when the node is changed while the panel is hidden"
  );

  await pushPref("devtools.inspector.activeSidebar", "");

  const tab = await addTab(_toDataURL(TEST_URI));
  const { inspector } = await openCompatibilityView();

  info("Select another sidebar panel");
  await _selectSidebarPanel(inspector, "changesview");

  info("Select another node");
  await selectNode("div", inspector);

  info("Select the compatibility panel again");
  const onSelectedNodePaneUpdated = waitForUpdateSelectedNodeAction(
    inspector.store
  );
  let isTopLevelTargetUpdated = false;
  waitForDispatch(
    inspector.store,
    COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_START
  ).then(() => {
    isTopLevelTargetUpdated = true;
  });

  await _selectSidebarPanel(inspector, "compatibilityview");

  await onSelectedNodePaneUpdated;
  ok(true, "Update the selected node pane");
  ok(!isTopLevelTargetUpdated, "Avoid updating the top level target pane");

  await removeTab(tab);
});

add_task(async function () {
  info(
    "Check that both panes update when the top-level target changed while the panel is hidden"
  );

  await pushPref("devtools.inspector.activeSidebar", "");

  const tab = await addTab(_toDataURL(TEST_URI));
  const { inspector } = await openCompatibilityView();

  info("Select another sidebar panel");
  await _selectSidebarPanel(inspector, "changesview");

  info("Navigate to another page");
  BrowserTestUtils.startLoadingURIString(
    tab.linkedBrowser,
    _toDataURL(TEST_ANOTHER_URI)
  );

  info("Select the compatibility panel again");
  const onSelectedNodePaneUpdated = waitForUpdateSelectedNodeAction(
    inspector.store
  );
  const onTopLevelTargetPaneUpdated = waitForUpdateTopLevelTargetAction(
    inspector.store
  );

  await _selectSidebarPanel(inspector, "compatibilityview");

  await onSelectedNodePaneUpdated;
  await onTopLevelTargetPaneUpdated;
  ok(true, "Update both panes");

  await removeTab(tab);
});

add_task(async function () {
  info(
    "Check that both panes update when a rule is changed changed while the panel is hidden"
  );

  await pushPref("devtools.inspector.activeSidebar", "");

  info("Disable 3 pane mode");
  await pushPref("devtools.inspector.three-pane-enabled", false);

  const tab = await addTab(_toDataURL(TEST_URI));
  const { inspector } = await openCompatibilityView();

  info("Select rule view");
  await _selectSidebarPanel(inspector, "ruleview");

  info("Change a rule");
  await togglePropStatusOnRuleView(inspector, 0, 0);

  info("Select the compatibility panel again");
  const onSelectedNodePaneUpdated = waitForUpdateSelectedNodeAction(
    inspector.store
  );
  const onTopLevelTargetPaneUpdated = waitForUpdateTopLevelTargetAction(
    inspector.store
  );

  await _selectSidebarPanel(inspector, "compatibilityview");

  await onSelectedNodePaneUpdated;
  await onTopLevelTargetPaneUpdated;
  ok(true, "Update both panes");

  await removeTab(tab);
});

async function _selectSidebarPanel(inspector, toolId) {
  const onSelected = inspector.sidebar.once(`${toolId}-selected`);
  inspector.sidebar.select(toolId);
  await onSelected;
}

function _toDataURL(content) {
  return "data:text/html;charset=utf-8," + encodeURIComponent(content);
}
