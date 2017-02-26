/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Check the position and text content of the highlighter nodeinfo bar under page zoom.

const TEST_URI = URL_ROOT + "doc_inspector_infobar_01.html";

add_task(function* () {
  let {inspector, testActor} = yield openInspectorForURL(TEST_URI);
  let testData = {
    selector: "#top",
    dims: "500" + " \u00D7 " + "100"
  };

  yield testInfobar(testData, inspector, testActor);
  info("Change zoom page to level 2.");
  yield testActor.zoomPageTo(2);
  info("Testing again the infobar after zoom.");
  yield testInfobar(testData, inspector, testActor);
});

function* testInfobar(test, inspector, testActor) {
  info(`Testing ${test.selector}`);

  yield selectAndHighlightNode(test.selector, inspector);

  // Ensure the node is the correct one.
  let id = yield testActor.getHighlighterNodeTextContent(
    "box-model-infobar-id");
  is(id, test.selector, `Node ${test.selector} selected.`);

  let dims = yield testActor.getHighlighterNodeTextContent(
    "box-model-infobar-dimensions");
  is(dims, test.dims, "Node's infobar displays the right dimensions.");
}
