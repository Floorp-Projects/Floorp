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
    selectedNode: "#id1",
  },
  {
    url: PAGE_1,
    nodeToSelect: "#id2",
    selectedNode: "#id2",
  },
  {
    url: PAGE_1,
    nodeToSelect: "#id3",
    selectedNode: "#id3",
  },
  {
    url: PAGE_1,
    nodeToSelect: "#id4",
    selectedNode: "#id4",
  },
  {
    url: PAGE_2,
    nodeToSelect: null,
    selectedNode: "body",
  },
  {
    url: PAGE_1,
    nodeToSelect: "#id5",
    selectedNode: "body",
  },
  {
    url: PAGE_2,
    nodeToSelect: null,
    selectedNode: "body",
  },
];

add_task(async function() {
  const { inspector } = await openInspectorForURL(PAGE_1);

  for (const { url, nodeToSelect, selectedNode } of TEST_DATA) {
    if (nodeToSelect) {
      info("Selecting node " + nodeToSelect + " before navigation.");
      await selectNode(nodeToSelect, inspector);
    }

    await navigateTo(url);

    const nodeFront = await getNodeFront(selectedNode, inspector);
    ok(nodeFront, "Got expected node front");
    is(
      inspector.selection.nodeFront,
      nodeFront,
      selectedNode + " is selected after navigation."
    );
  }
});
