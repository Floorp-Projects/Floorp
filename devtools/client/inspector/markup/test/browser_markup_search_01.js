/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that searching for nodes using the selector-search input expands and
// selects the right nodes in the markup-view, even when those nodes are deeply
// nested (and therefore not attached yet when the markup-view is initialized).

const TEST_URL = TEST_URL_ROOT + "doc_markup_search.html";

add_task(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  let container = yield getContainerForSelector("em", inspector);
  ok(!container, "The <em> tag isn't present yet in the markup-view");

  // Searching for the innermost element first makes sure that the inspector
  // back-end is able to attach the resulting node to the tree it knows at the
  // moment. When the inspector is started, the <body> is the default selected
  // node, and only the parents up to the ROOT are known, and its direct
  // children.
  info("searching for the innermost child: <em>");
  yield searchFor("em", inspector);

  container = yield getContainerForSelector("em", inspector);
  ok(container, "The <em> tag is now imported in the markup-view");

  let nodeFront = yield getNodeFront("em", inspector);
  is(inspector.selection.nodeFront, nodeFront,
    "The <em> tag is the currently selected node");

  info("searching for other nodes too");
  for (let node of ["span", "li", "ul"]) {
    yield searchFor(node, inspector);

    nodeFront = yield getNodeFront(node, inspector);
    is(inspector.selection.nodeFront, nodeFront,
      "The <" + node + "> tag is the currently selected node");
  }
});

function* searchFor(selector, inspector) {
  let onNewNodeFront = inspector.selection.once("new-node-front");

  searchUsingSelectorSearch(selector, inspector);

  yield onNewNodeFront;
  yield inspector.once("inspector-updated");
}
