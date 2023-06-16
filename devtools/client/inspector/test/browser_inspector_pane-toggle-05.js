/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Tests whether the initial selected tab is displayed when switched from 3 pane to 2 pane
// and back to 3 pane when no other tab is selected explicitly. Rule view is displayed
// immediately on toggling to 2 pane.
add_task(async function () {
  info(
    "Switch to 2 pane inspector and back to 3 pane to test whether the selected tab is used"
  );
  await pushPref("devtools.inspector.three-pane-enabled", true);

  const { inspector } = await openInspectorForURL("about:blank");

  inspector.sidebar.select("changesview");

  is(
    inspector.sidebar.getCurrentTabID(),
    "changesview",
    "Changes view should be the active sidebar"
  );

  info("Click on the toggle button to toggle OFF 3 pane inspector");
  await toggleSidebar(inspector);

  is(
    inspector.sidebar.getCurrentTabID(),
    "ruleview",
    "Rules view should be the active sidebar on toggle to 2 pane"
  );

  info("Click on the toggle button to toggle ON 3 pane inspector");
  await toggleSidebar(inspector);

  is(
    inspector.sidebar.getCurrentTabID(),
    "changesview",
    "Changes view should be the active sidebar again"
  );
});

// Tests whether the selected pane in 2 pane view is also used after toggling to 3 pane view.
add_task(async function () {
  info("Switch to 3 pane to test whether the selected pane is preserved");
  await pushPref("devtools.inspector.three-pane-enabled", false);

  const { inspector } = await openInspectorForURL("about:blank");

  inspector.sidebar.select("changesview");

  is(
    inspector.sidebar.getCurrentTabID(),
    "changesview",
    "Changes view should be the active sidebar"
  );

  info("Click on the toggle button to toggle ON 3 pane inspector");
  await toggleSidebar(inspector);

  is(
    inspector.sidebar.getCurrentTabID(),
    "changesview",
    "Changes view should still be the active sidebar"
  );
});

// Tests whether the selected pane is layout view, if rule view is selected before toggling to 3 pane.
add_task(async function () {
  info("Switch to 3 pane to test whether the selected pane is layout view");
  await pushPref("devtools.inspector.three-pane-enabled", false);

  const { inspector } = await openInspectorForURL("about:blank");

  inspector.sidebar.select("ruleview");

  is(
    inspector.sidebar.getCurrentTabID(),
    "ruleview",
    "Rules view should be the active sidebar in 2 pane"
  );

  info("Click on the toggle button to toggle ON 3 pane inspector");
  await toggleSidebar(inspector);

  is(
    inspector.sidebar.getCurrentTabID(),
    "layoutview",
    "Layout view should be the active sidebar in 3 pane"
  );
});

const toggleSidebar = async inspector => {
  const { panelDoc: doc } = inspector;
  const button = doc.querySelector(".sidebar-toggle");

  const onRuleViewAdded = inspector.once("ruleview-added");
  EventUtils.synthesizeMouseAtCenter(
    button,
    {},
    inspector.panelDoc.defaultView
  );
  await onRuleViewAdded;
};
