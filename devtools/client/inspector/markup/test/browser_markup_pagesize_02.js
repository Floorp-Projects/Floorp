/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the markup view loads only as many nodes as specified
// by the devtools.markup.pagesize preference and that pressing the "show all
// nodes" actually shows the nodes

const TEST_URL = URL_ROOT + "doc_markup_pagesize_02.html";

// Make sure nodes are hidden when there are more than 5 in a row
Services.prefs.setIntPref("devtools.markup.pagesize", 5);

add_task(async function() {
  const {inspector, testActor} = await openInspectorForURL(TEST_URL);

  info("Selecting the UL node");
  await clickContainer("ul", inspector);
  info("Reloading the page with the UL node selected will expand its children");
  await reloadPage(inspector, testActor);
  await inspector.markup._waitForChildren();

  info("Click on the 'show all nodes' button in the UL's list of children");
  await showAllNodes(inspector);

  await assertAllNodesAreVisible(inspector, testActor);
});

async function showAllNodes(inspector) {
  const container = await getContainerForSelector("ul", inspector);
  const button = container.elt.querySelector("button");
  ok(button, "All nodes button is here");
  const win = button.ownerDocument.defaultView;

  EventUtils.sendMouseEvent({type: "click"}, button, win);
  await inspector.markup._waitForChildren();
}

async function assertAllNodesAreVisible(inspector, testActor) {
  const container = await getContainerForSelector("ul", inspector);
  ok(!container.elt.querySelector("button"),
     "All nodes button isn't here anymore");
  const numItems = await testActor.getNumberOfElementMatches("ul > *");
  is(container.children.childNodes.length, numItems);
}
