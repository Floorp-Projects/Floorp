/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Globals defined in: devtools/client/inspector/test/head.js */

"use strict";

// Test that the geometry highlighter labels are correct.

const TEST_URL = `data:text/html;charset=utf-8,
                  <div id='positioned' style='
                    background:yellow;
                    position:absolute;
                    left:5rem;
                    top:30px;
                    right:300px;
                    bottom:10em;'></div>
                  <div id='positioned2' style='
                    background:blue;
                    position:absolute;
                    right:10%;
                    top:5vmin;'>test element</div>
                 <div id='relative' style='
                    background:green;
                    position:relative;
                    top:10px;
                    left:20px;
                    bottom:30px;
                    right:40px;
                    width:100px;
                    height:100px;'></div>
                 <div id='relative2' style='
                    background:grey;
                    position:relative;
                    top:0;bottom:-50px;
                    height:3em;'>relative</div>`;

const ID = "geometry-editor-";
const HIGHLIGHTER_TYPE = "GeometryEditorHighlighter";

const POSITIONED_ELEMENT_TESTS = [{
  selector: "#positioned",
  expectedLabels: [
    {side: "left", visible: true, label: "5rem"},
    {side: "top", visible: true, label: "30px"},
    {side: "right", visible: true, label: "300px"},
    {side: "bottom", visible: true, label: "10em"}
  ]
}, {
  selector: "#positioned2",
  expectedLabels: [
    {side: "left", visible: false},
    {side: "top", visible: true, label: "5vmin"},
    {side: "right", visible: true, label: "10%"},
    {side: "bottom", visible: false}
  ]
}, {
  selector: "#relative",
  expectedLabels: [
    {side: "left", visible: true, label: "20px"},
    {side: "top", visible: true, label: "10px"},
    {side: "right", visible: false},
    {side: "bottom", visible: false}
  ]
}, {
  selector: "#relative2",
  expectedLabels: [
    {side: "left", visible: false},
    {side: "top", visible: true, label: "0px"},
    {side: "right", visible: false},
    {side: "bottom", visible: false}
  ]
}];

add_task(async function() {
  const helper = await openInspectorForURL(TEST_URL)
                       .then(getHighlighterHelperFor(HIGHLIGHTER_TYPE));

  helper.prefix = ID;

  const { finalize } = helper;

  await positionLabelsAreCorrect(helper);

  await finalize();
});

async function positionLabelsAreCorrect(
  {show, hide, isElementHidden, getElementTextContent}
) {
  info("Highlight nodes and check position labels");

  for (const {selector, expectedLabels} of POSITIONED_ELEMENT_TESTS) {
    info("Testing node " + selector);

    await show(selector);

    for (const {side, visible, label} of expectedLabels) {
      const id = "label-" + side;

      const hidden = await isElementHidden(id);
      if (visible) {
        ok(!hidden, "The " + side + " label is visible");

        const value = await getElementTextContent(id);
        is(value, label, "The " + side + " label textcontent is correct");
      } else {
        ok(hidden, "The " + side + " label is hidden");
      }
    }

    info("Hiding the highlighter");
    await hide();
  }
}
