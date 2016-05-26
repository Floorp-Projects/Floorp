/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Check the text content of the highlighter info bar for namespaced elements.

const XHTML = `
  <!DOCTYPE html>
  <html xmlns="http://www.w3.org/1999/xhtml"
        xmlns:svg="http://www.w3.org/2000/svg">
    <body>
      <svg:svg width="100" height="100">
        <svg:circle cx="0" cy="0" r="5"></svg:circle>
      </svg:svg>
    </body>
  </html>
`;

const TEST_URI = "data:application/xhtml+xml;charset=utf-8," + encodeURI(XHTML);

add_task(function* () {
  let {inspector, testActor} = yield openInspectorForURL(TEST_URI);

  let testData = [
    {
      selector: "svg",
      tag: "svg:svg"
    },
    {
      selector: "circle",
      tag: "svg:circle"
    },
  ];

  for (let currTest of testData) {
    yield testNode(currTest, inspector, testActor);
  }
});

function* testNode(test, inspector, testActor) {
  info("Testing " + test.selector);

  yield selectAndHighlightNode(test.selector, inspector);

  let tag = yield testActor.getHighlighterNodeTextContent(
    "box-model-nodeinfobar-tagname");
  is(tag, test.tag, "node " + test.selector + ": tagName matches.");
}
