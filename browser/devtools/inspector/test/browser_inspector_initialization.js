/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Tests for different ways to initialize the inspector.

const DOCUMENT_HTML = '<div id="first" style="{margin: 10em; font-size: 14pt;' +
  'font-family: helvetica, sans-serif; color: #AAA}">\n' +
  '<h1>Some header text</h1>\n' +
  '<p id="salutation" style="{font-size: 12pt}">hi.</p>\n' +
  '<p id="body" style="{font-size: 12pt}">I am a test-case. This text exists ' +
  'solely to provide some things to test the inspector initialization.</p>\n' +
  'If you are reading this, you should go do something else instead. Maybe ' +
  'read a book. Or better yet, write some test-cases for another bit of code. ' +
  '<span style="{font-style: italic}">Inspector\'s!</span></p>\n' +
  '<p id="closing">end transmission</p>\n' +
  '</div>';

const TEST_URI = "data:text/html;charset=utf-8," +
  "browser_inspector_initialization.js";

let test = asyncTest(function* () {
  let tab = yield addTab(TEST_URI);
  content.document.body.innerHTML = DOCUMENT_HTML;
  content.document.title = "Inspector Initialization Test";

  yield testToolboxInitialization(tab);
  yield testContextMenuInitialization();
  yield testContextMenuInspectorAlreadyOpen();
});

function* testToolboxInitialization(tab) {
  let target = TargetFactory.forTab(tab);

  info("Opening inspector with gDevTools.");
  let toolbox = yield gDevTools.showToolbox(target, "inspector");
  let inspector = toolbox.getCurrentPanel();

  ok(true, "Inspector started, and notification received.");
  ok(inspector, "Inspector instance is accessible.");
  ok(inspector.isReady, "Inspector instance is ready.");
  is(inspector.target.tab, tab, "Valid target.");

  let p = getNode("p");
  yield selectNode(p, inspector);
  testMarkupView(p, inspector);
  testBreadcrumbs(p, inspector);

  let span = getNode("span");
  span.scrollIntoView();

  yield selectNode(span, inspector);
  testMarkupView(span, inspector);
  testBreadcrumbs(span, inspector);

  info("Destroying toolbox");
  let destroyed = toolbox.once("destroyed");
  toolbox.destroy();
  yield destroyed;

  ok("true", "'destroyed' notification received.");
  ok(!gDevTools.getToolbox(target), "Toolbox destroyed.");
}

function* testContextMenuInitialization() {
  info("Opening inspector by clicking on 'Inspect Element' context menu item");
  let salutation = getNode("#salutation");

  yield clickOnInspectMenuItem(salutation);

  info("Checking inspector state.");
  testMarkupView(salutation);
  testBreadcrumbs(salutation);
}

function* testContextMenuInspectorAlreadyOpen() {
  info("Changing node by clicking on 'Inspect Element' context menu item");

  let inspector = getActiveInspector();
  ok(inspector, "Inspector is active");

  let closing = getNode("#closing");
  yield clickOnInspectMenuItem(closing);

  ok(true, "Inspector was updated when 'Inspect Element' was clicked.");
  testMarkupView(closing, inspector);
  testBreadcrumbs(closing, inspector);
}

function testMarkupView(node, inspector) {
  inspector = inspector || getActiveInspector();
  try {
    is(inspector.markup._selectedContainer.node.rawNode(), node,
       "Right node is selected in the markup view");
  } catch(ex) {
    ok(false, "Got exception while resolving selected node of markup view.");
    console.error(ex);
  }
}

function testBreadcrumbs(node, inspector) {
  inspector = inspector || getActiveInspector();
  let b = inspector.breadcrumbs;
  let expectedText = b.prettyPrintNodeAsText(getNodeFront(node));
  let button = b.container.querySelector("button[checked=true]");
  ok(button, "A crumbs is checked=true");
  is(button.getAttribute("tooltiptext"), expectedText, "Crumb refers to the right node");
}

function* clickOnInspectMenuItem(node) {
  info("Clicking on 'Inspect Element' context menu item of " + node);
  document.popupNode = node;
  var contentAreaContextMenu = getNode("#contentAreaContextMenu", { document });
  var contextMenu = new nsContextMenu(contentAreaContextMenu);

  info("Triggering inspect action.");
  yield contextMenu.inspectNode();

  // Clean up context menu:
  contextMenu.hiding();

  info("Waiting for inspector to update.");
  yield getActiveInspector().once("inspector-updated");
}
