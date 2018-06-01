/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals getTestActorWithoutToolbox */
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

add_task(async function() {
  const tab = await addTab(TEST_URI);
  const testActor = await getTestActorWithoutToolbox(tab);

  await testToolboxInitialization(testActor, tab);
  await testContextMenuInitialization(testActor);
  await testContextMenuInspectorAlreadyOpen(testActor);
});

async function testToolboxInitialization(testActor, tab) {
  const target = TargetFactory.forTab(tab);

  info("Opening inspector with gDevTools.");
  const toolbox = await gDevTools.showToolbox(target, "inspector");
  const inspector = toolbox.getCurrentPanel();

  ok(true, "Inspector started, and notification received.");
  ok(inspector, "Inspector instance is accessible.");
  ok(inspector.isReady, "Inspector instance is ready.");
  is(inspector.target.tab, tab, "Valid target.");

  await selectNode("p", inspector);
  await testMarkupView("p", inspector);
  await testBreadcrumbs("p", inspector);

  await testActor.scrollIntoView("span");

  await selectNode("span", inspector);
  await testMarkupView("span", inspector);
  await testBreadcrumbs("span", inspector);

  info("Destroying toolbox");
  await toolbox.destroy();

  ok("true", "'destroyed' notification received.");
  ok(!gDevTools.getToolbox(target), "Toolbox destroyed.");
}

async function testContextMenuInitialization(testActor) {
  info("Opening inspector by clicking on 'Inspect Element' context menu item");
  await clickOnInspectMenuItem(testActor, "#salutation");

  info("Checking inspector state.");
  await testMarkupView("#salutation");
  await testBreadcrumbs("#salutation");
}

async function testContextMenuInspectorAlreadyOpen(testActor) {
  info("Changing node by clicking on 'Inspect Element' context menu item");

  const inspector = getActiveInspector();
  ok(inspector, "Inspector is active");

  await clickOnInspectMenuItem(testActor, "#closing");

  ok(true, "Inspector was updated when 'Inspect Element' was clicked.");
  await testMarkupView("#closing", inspector);
  await testBreadcrumbs("#closing", inspector);
}

async function testMarkupView(selector, inspector) {
  inspector = inspector || getActiveInspector();
  const nodeFront = await getNodeFront(selector, inspector);
  try {
    is(inspector.selection.nodeFront, nodeFront,
       "Right node is selected in the markup view");
  } catch (ex) {
    ok(false, "Got exception while resolving selected node of markup view.");
    console.error(ex);
  }
}

async function testBreadcrumbs(selector, inspector) {
  inspector = inspector || getActiveInspector();
  const nodeFront = await getNodeFront(selector, inspector);

  const b = inspector.breadcrumbs;
  const expectedText = b.prettyPrintNodeAsText(nodeFront);
  const button = b.container.querySelector("button[checked=true]");
  ok(button, "A crumbs is checked=true");
  is(button.getAttribute("title"), expectedText,
     "Crumb refers to the right node");
}
