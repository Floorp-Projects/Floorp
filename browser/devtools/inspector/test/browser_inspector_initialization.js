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

const TEST_URI = "data:text/html;charset=utf-8,test page";

add_task(function* () {
  let tab = yield addTab(TEST_URI);
  content.document.body.innerHTML = DOCUMENT_HTML;
  content.document.title = "Inspector Initialization Test";

  let deferred = promise.defer();
  executeSoon(deferred.resolve);
  yield deferred.promise;

  let testActor = yield getTestActorWithoutToolbox(tab);

  yield testToolboxInitialization(tab);
  yield testContextMenuInitialization(testActor);
  yield testContextMenuInspectorAlreadyOpen(testActor);
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

  yield selectNode("p", inspector);
  yield testMarkupView("p", inspector);
  yield testBreadcrumbs("p", inspector);

  let span = getNode("span");
  span.scrollIntoView();

  yield selectNode("span", inspector);
  yield testMarkupView("span", inspector);
  yield testBreadcrumbs("span", inspector);

  info("Destroying toolbox");
  let destroyed = toolbox.once("destroyed");
  toolbox.destroy();
  yield destroyed;

  ok("true", "'destroyed' notification received.");
  ok(!gDevTools.getToolbox(target), "Toolbox destroyed.");
}

function* testContextMenuInitialization(testActor) {
  info("Opening inspector by clicking on 'Inspect Element' context menu item");
  yield clickOnInspectMenuItem(testActor, "#salutation");

  info("Checking inspector state.");
  yield testMarkupView("#salutation");
  yield testBreadcrumbs("#salutation");
}

function* testContextMenuInspectorAlreadyOpen(testActor) {
  info("Changing node by clicking on 'Inspect Element' context menu item");

  let inspector = getActiveInspector();
  ok(inspector, "Inspector is active");

  yield clickOnInspectMenuItem(testActor, "#closing");

  ok(true, "Inspector was updated when 'Inspect Element' was clicked.");
  yield testMarkupView("#closing", inspector);
  yield testBreadcrumbs("#closing", inspector);
}

function* testMarkupView(selector, inspector) {
  inspector = inspector || getActiveInspector();
  let nodeFront = yield getNodeFront(selector, inspector);
  try {
    is(inspector.selection.nodeFront, nodeFront,
       "Right node is selected in the markup view");
  } catch(ex) {
    ok(false, "Got exception while resolving selected node of markup view.");
    console.error(ex);
  }
}

function* testBreadcrumbs(selector, inspector) {
  inspector = inspector || getActiveInspector();
  let nodeFront = yield getNodeFront(selector, inspector);

  let b = inspector.breadcrumbs;
  let expectedText = b.prettyPrintNodeAsText(nodeFront);
  let button = b.container.querySelector("button[checked=true]");
  ok(button, "A crumbs is checked=true");
  is(button.getAttribute("tooltiptext"), expectedText, "Crumb refers to the right node");
}

function* clickOnInspectMenuItem(testActor, selector) {
  info("Showing the contextual menu on node " + selector);
  yield testActor.synthesizeMouse({
    selector: selector,
    center: true,
    options: {type: "contextmenu", button: 2}
  });

  // nsContextMenu also requires the popupNode to be set, but we can't set it to
  // node under e10s as it's a CPOW, not a DOM node. But under e10s,
  // nsContextMenu won't use the property anyway, so just try/catching is ok.
  try {
    document.popupNode = content.document.querySelector(selector);
  } catch (e) {}

  let contentAreaContextMenu = document.querySelector("#contentAreaContextMenu");
  let contextMenu = new nsContextMenu(contentAreaContextMenu);

  info("Triggering inspect action and hiding the menu.");
  yield contextMenu.inspectNode();

  contentAreaContextMenu.hidden = true;
  contentAreaContextMenu.hidePopup();
  contextMenu.hiding();

  info("Waiting for inspector to update.");
  yield getActiveInspector().once("inspector-updated");
}
