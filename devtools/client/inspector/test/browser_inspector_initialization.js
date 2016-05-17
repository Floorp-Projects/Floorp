/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Tests for different ways to initialize the inspector.

const HTML = `
  <div id="first" style="margin: 10em; font-size: 14pt;
                         font-family: helvetica, sans-serif; color: gray">
    <h1>Some header text</h1>
    <p id="salutation" style="font-size: 12pt">hi.</p>
    <p id="body" style="font-size: 12pt">I am a test-case. This text exists
    solely to provide some things to test the inspector initialization.</p>
    <p>If you are reading this, you should go do something else instead. Maybe
    read a book. Or better yet, write some test-cases for another bit of code.
      <span style="font-style: italic">Inspector's!</span>
    </p>
    <p id="closing">end transmission</p>
  </div>
`;

const TEST_URI = "data:text/html;charset=utf-8," + encodeURI(HTML);

add_task(function* () {
  let tab = yield addTab(TEST_URI);
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
  } catch (ex) {
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
  is(button.getAttribute("tooltiptext"), expectedText,
     "Crumb refers to the right node");
}
