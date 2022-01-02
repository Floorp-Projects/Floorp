/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Check the position and text content of the highlighter nodeinfo bar.

const TEST_URI = URL_ROOT + "doc_inspector_infobar_01.html";

add_task(async function() {
  const { inspector, highlighterTestFront } = await openInspectorForURL(
    TEST_URI
  );

  const testData = [
    {
      selector: "#top",
      position: "bottom",
      tag: "div",
      id: "top",
      classes: ".class1.class2",
      dims: "500" + " \u00D7 " + "100",
      arrowed: true,
    },
    {
      selector: "#vertical",
      position: "top",
      tag: "div",
      id: "vertical",
      classes: "",
      arrowed: false,
      // No dims as they will vary between computers
    },
    {
      selector: "#bottom",
      position: "top",
      tag: "div",
      id: "bottom",
      classes: "",
      dims: "500" + " \u00D7 " + "100",
      arrowed: true,
    },
    {
      selector: "body",
      position: "bottom",
      tag: "body",
      classes: "",
      arrowed: true,
      // No dims as they will vary between computers
    },
    {
      selector: "clipPath",
      position: "bottom",
      tag: "clipPath",
      id: "clip",
      classes: "",
      arrowed: false,
      // No dims as element is not displayed and we just want to test tag name
    },
  ];

  for (const currTest of testData) {
    await testPosition(currTest, inspector, highlighterTestFront);
  }
});

async function testPosition(test, inspector, highlighterTestFront) {
  info("Testing " + test.selector);

  await selectAndHighlightNode(test.selector, inspector);

  const position = await highlighterTestFront.getHighlighterNodeAttribute(
    "box-model-infobar-container",
    "position"
  );
  is(position, test.position, "Node " + test.selector + ": position matches");

  const tag = await highlighterTestFront.getHighlighterNodeTextContent(
    "box-model-infobar-tagname"
  );
  is(tag, test.tag, "node " + test.selector + ": tagName matches.");

  if (test.id) {
    const id = await highlighterTestFront.getHighlighterNodeTextContent(
      "box-model-infobar-id"
    );
    is(id, "#" + test.id, "node " + test.selector + ": id matches.");
  }

  const classes = await highlighterTestFront.getHighlighterNodeTextContent(
    "box-model-infobar-classes"
  );
  is(classes, test.classes, "node " + test.selector + ": classes match.");

  const arrowed = !(await highlighterTestFront.getHighlighterNodeAttribute(
    "box-model-infobar-container",
    "hide-arrow"
  ));

  is(
    arrowed,
    test.arrowed,
    "node " + test.selector + ": arrow visibility match."
  );

  if (test.dims) {
    const dims = await highlighterTestFront.getHighlighterNodeTextContent(
      "box-model-infobar-dimensions"
    );
    is(dims, test.dims, "node " + test.selector + ": dims match.");
  }
}
