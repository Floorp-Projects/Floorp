/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the markup view loads only as many nodes as specified
// by the devtools.markup.pagesize preference and that pressing the "show all nodes"
// actually shows the nodes

const TEST_URL = TEST_URL_ROOT + "doc_markup_pagesize_02.html";

// Make sure nodes are hidden when there are more than 5 in a row
Services.prefs.setIntPref("devtools.markup.pagesize", 5);

let test = asyncTest(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  info("Selecting the UL node");
  yield clickContainer("ul", inspector);
  info("Reloading the page with the UL node selected will expand its children");
  yield reloadPage(inspector);
  yield inspector.markup._waitForChildren();

  info("Click on the 'show all nodes' button in the UL's list of children");
  yield showAllNodes(inspector);

  yield assertAllNodesAreVisible(inspector);
});

function* showAllNodes(inspector) {
  let container = yield getContainerForSelector("ul", inspector);
  let button = container.elt.querySelector("button");
  ok(button, "All nodes button is here");
  let win = button.ownerDocument.defaultView;

  EventUtils.sendMouseEvent({type: "click"}, button, win);
  yield inspector.markup._waitForChildren();
}

function* assertAllNodesAreVisible(inspector) {
  let container = yield getContainerForSelector("ul", inspector);
  ok(!container.elt.querySelector("button"), "All nodes button isn't here anymore");
  is(container.children.childNodes.length, getNode("ul").children.length);
}
