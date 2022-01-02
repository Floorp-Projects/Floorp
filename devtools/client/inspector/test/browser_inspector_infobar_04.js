/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Check the position and text content of the highlighter nodeinfo bar under page zoom.

const TEST_URI = URL_ROOT + "doc_inspector_infobar_01.html";

add_task(async function() {
  const { inspector, highlighterTestFront } = await openInspectorForURL(
    TEST_URI
  );
  const testData = {
    selector: "#top",
    dims: "500" + " \u00D7 " + "100",
  };

  await testInfobar(testData, inspector, highlighterTestFront);
  info("Change zoom page to level 2.");
  setContentPageZoomLevel(2);
  info("Testing again the infobar after zoom.");
  await testInfobar(testData, inspector, highlighterTestFront);
});

async function testInfobar(test, inspector, highlighterTestFront) {
  info(`Testing ${test.selector}`);
  // First, hide any existing box model highlighter. Duplicate calls to show are ignored.
  await inspector.highlighters.hideHighlighterType(
    inspector.highlighters.TYPES.BOXMODEL
  );
  await selectAndHighlightNode(test.selector, inspector);

  // Ensure the node is the correct one.
  const id = await highlighterTestFront.getHighlighterNodeTextContent(
    "box-model-infobar-id"
  );
  is(id, test.selector, `Node ${test.selector} selected.`);

  const dims = await highlighterTestFront.getHighlighterNodeTextContent(
    "box-model-infobar-dimensions"
  );
  is(dims, test.dims, "Node's infobar displays the right dimensions.");
}
