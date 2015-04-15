/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Check the position and text content of the highlighter nodeinfo bar.

const TEST_URI = TEST_URL_ROOT + "doc_inspector_infobar_01.html";

add_task(function*() {
  let {inspector, testActor} = yield openInspectorForURL(TEST_URI);

  let testData = [
    {
      selector: "#top",
      position: "bottom",
      tag: "DIV",
      id: "top",
      classes: ".class1.class2",
      dims: "500" + " \u00D7 " + "100"
    },
    {
      selector: "#vertical",
      position: "overlap",
      tag: "DIV",
      id: "vertical",
      classes: ""
      // No dims as they will vary between computers
    },
    {
      selector: "#bottom",
      position: "top",
      tag: "DIV",
      id: "bottom",
      classes: "",
      dims: "500" + " \u00D7 " + "100"
    },
    {
      selector: "body",
      position: "bottom",
      tag: "BODY",
      classes: ""
      // No dims as they will vary between computers
    },
  ];

  for (let currTest of testData) {
    yield testPosition(currTest, inspector, testActor);
  }
});

function* testPosition(test, inspector, testActor) {
  info("Testing " + test.selector);

  yield selectAndHighlightNode(test.selector, inspector);

  let position = yield testActor.getHighlighterNodeAttribute("box-model-nodeinfobar-container", "position");
  is(position, test.position, "Node " + test.selector + ": position matches");

  let tag = yield testActor.getHighlighterNodeTextContent("box-model-nodeinfobar-tagname");
  is(tag, test.tag, "node " + test.selector + ": tagName matches.");

  if (test.id) {
    let id = yield testActor.getHighlighterNodeTextContent("box-model-nodeinfobar-id");
    is(id, "#" + test.id, "node " + test.selector  + ": id matches.");
  }

  let classes = yield testActor.getHighlighterNodeTextContent("box-model-nodeinfobar-classes");
  is(classes, test.classes, "node " + test.selector  + ": classes match.");

  if (test.dims) {
    let dims = yield testActor.getHighlighterNodeTextContent("box-model-nodeinfobar-dimensions");
    is(dims, test.dims, "node " + test.selector  + ": dims match.");
  }
}
