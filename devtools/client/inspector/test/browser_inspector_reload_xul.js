/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Tests for inspecting a node on a XUL document, spanning a tab reload.

const TEST_URI = URL_ROOT + "doc_inspector_reload_xul.xul";

add_task(async function() {
  await pushPref("dom.allow_XUL_XBL_for_file", false);

  const { tab, inspector, toolbox } = await openInspectorForURL(TEST_URI);
  await testToolboxInitialization(tab, inspector, toolbox);
});

async function testToolboxInitialization(tab, inspector, toolbox) {
  const target = await TargetFactory.forTab(tab);

  ok(true, "Inspector started, and notification received.");
  ok(inspector, "Inspector instance is accessible.");
  ok(inspector.isReady, "Inspector instance is ready.");
  is(inspector.target.tab, tab, "Valid target.");

  await selectNode("#p", inspector);
  await testMarkupView("#p", inspector);

  info("Reloading the page.");
  const markuploaded = inspector.once("markuploaded");
  const onNewRoot = inspector.once("new-root");
  const onUpdated = inspector.once("inspector-updated");
  await toolbox.target.reload();
  info("Waiting for inspector to be ready.");
  await markuploaded;
  await onNewRoot;
  await onUpdated;

  await selectNode("#q", inspector);
  await testMarkupView("#q", inspector);

  info("Destroying toolbox.");
  await toolbox.destroy();

  ok("true", "'destroyed' notification received.");
  ok(!gDevTools.getToolbox(target), "Toolbox destroyed.");
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
