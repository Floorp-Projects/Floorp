/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Tests for inspecting a node on a XUL document, spanning a tab reload.

const TEST_URI = URL_ROOT_SSL + "doc_inspector_reload_xul.xhtml";

add_task(async function () {
  const { tab, inspector, toolbox } = await openInspectorForURL(TEST_URI);
  await testToolboxInitialization(tab, inspector, toolbox);
});

async function testToolboxInitialization(tab, inspector, toolbox) {
  ok(true, "Inspector started, and notification received.");
  ok(inspector, "Inspector instance is accessible.");

  await selectNode("#p", inspector);
  await testMarkupView("#p", inspector);

  info("Reloading the page.");
  await navigateTo(TEST_URI);

  await selectNode("#q", inspector);
  await testMarkupView("#q", inspector);

  info("Destroying toolbox.");
  await toolbox.destroy();

  ok(true, "'destroyed' notification received.");
  const toolboxForTab = gDevTools.getToolboxForTab(tab);
  ok(!toolboxForTab, "Toolbox destroyed.");
}

async function testMarkupView(selector, inspector) {
  const nodeFront = await getNodeFront(selector, inspector);
  try {
    is(
      inspector.selection.nodeFront,
      nodeFront,
      "Right node is selected in the markup view"
    );
  } catch (ex) {
    ok(false, "Got exception while resolving selected node of markup view.");
    console.error(ex);
  }
}
