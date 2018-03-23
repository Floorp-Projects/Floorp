/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

requestLongerTimeout(2);

// Checks that the expected default node is selected after a page navigation or
// a reload.
var PAGE_1 = URL_ROOT + "doc_inspector_select-last-selected-01.html";
var PAGE_2 = URL_ROOT + "doc_inspector_select-last-selected-02.html";

// An array of test cases with following properties:
// - url: URL to navigate to. If URL == content.location, reload instead.
// - nodeToSelect: a selector for a node to select before navigation. If null,
//                 whatever is selected stays selected.
// - selectedNode: a selector for a node that is selected after navigation.
var TEST_DATA = [
  {
    url: PAGE_1,
    nodeToSelect: "#id1",
    selectedNode: "#id1"
  },
  {
    url: PAGE_1,
    nodeToSelect: "#id2",
    selectedNode: "#id2"
  },
  {
    url: PAGE_1,
    nodeToSelect: "#id3",
    selectedNode: "#id3"
  },
  {
    url: PAGE_1,
    nodeToSelect: "#id4",
    selectedNode: "#id4"
  },
  {
    url: PAGE_2,
    nodeToSelect: null,
    selectedNode: "body"
  },
  {
    url: PAGE_1,
    nodeToSelect: "#id5",
    selectedNode: "body"
  },
  {
    url: PAGE_2,
    nodeToSelect: null,
    selectedNode: "body"
  }
];

add_task(function* () {
  let { inspector, toolbox, testActor } = yield openInspectorForURL(PAGE_1);

  for (let { url, nodeToSelect, selectedNode } of TEST_DATA) {
    if (nodeToSelect) {
      info("Selecting node " + nodeToSelect + " before navigation.");
      yield selectNode(nodeToSelect, inspector);
    }

    yield navigateToAndWaitForNewRoot(url);

    let nodeFront = yield getNodeFront(selectedNode, inspector);
    ok(nodeFront, "Got expected node front");
    is(inspector.selection.nodeFront, nodeFront,
       selectedNode + " is selected after navigation.");
  }

  function* navigateToAndWaitForNewRoot(url) {
    info("Navigating and waiting for new-root event after navigation.");

    let current = yield testActor.eval("location.href");
    if (url == current) {
      info("Reloading page.");
      let markuploaded = inspector.once("markuploaded");
      let onNewRoot = inspector.once("new-root");
      let onUpdated = inspector.once("inspector-updated");

      let activeTab = toolbox.target.activeTab;
      yield activeTab.reload();
      info("Waiting for inspector to be ready.");
      yield markuploaded;
      yield onNewRoot;
      yield onUpdated;
    } else {
      yield navigateTo(inspector, url);
    }
  }
});
