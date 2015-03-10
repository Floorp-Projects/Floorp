/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the geometry highlighter labels are correct.

const TEST_URL = "data:text/html;charset=utf-8," +
                 "<div id='positioned' style='background:yellow;position:absolute;left:5rem;top:30px;right:300px;bottom:10em;'></div>" +
                 "<div id='positioned2' style='background:blue;position:absolute;right:10%;top:5vmin;'>test element</div>" +
                 "<div id='relative' style='background:green;position:relative;top:10px;left:20px;bottom:30px;right:40px;width:100px;height:100px;'></div>" +
                 "<div id='relative2' style='background:grey;position:relative;top:0;bottom:-50px;height:3em;'>relative</div>" +
                 "<div id='sized' style='background:red;width:5em;height:50%;'></div>" +
                 "<div id='sized2' style='background:orange;width:40px;position:absolute;right:0;bottom:0'>wow</div>";
const ID = "geometry-editor-";

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
  selector: "#sized",
  expectedLabels: [
    {side: "left", visible: false},
    {side: "top", visible: false},
    {side: "right", visible: false},
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

const SIZED_ELEMENT_TESTS = [{
  selector: "#positioned",
  visible: false
}, {
  selector: "#sized",
  visible: true,
  expected: "\u2194 5em \u2195 50%"
}, {
  selector: "#relative",
  visible: true,
  expected: "\u2194 100px \u2195 100px"
}, {
  selector: "#relative2",
  visible: true,
  expected: "\u2195 3em"
}, {
  selector: "#sized2",
  visible: true,
  expected: "\u2194 40px"
}];

add_task(function*() {
  let {inspector, toolbox} = yield openInspectorForURL(TEST_URL);
  let front = inspector.inspector;

  let highlighter = yield front.getHighlighterByType("GeometryEditorHighlighter");

  yield positionLabelsAreCorrect(highlighter, inspector);
  yield sizeLabelIsCorrect(highlighter, inspector);

  yield highlighter.finalize();
});

function* positionLabelsAreCorrect(highlighterFront, inspector) {
  info("Highlight nodes and check position labels");

  for (let {selector, expectedLabels} of POSITIONED_ELEMENT_TESTS) {
    info("Testing node " + selector);
    let node = yield getNodeFront(selector, inspector);
    yield highlighterFront.show(node);

    for (let {side, visible, label} of expectedLabels) {
      let id = ID + "label-" + side;

      let hidden = yield getHighlighterNodeAttribute(highlighterFront, id, "hidden");
      if (visible) {
        ok(!hidden, "The " + side + " label is visible");

        let value = yield getHighlighterNodeTextContent(highlighterFront, id);
        is(value, label, "The " + side + " label textcontent is correct");
      } else {
        is(hidden, "true", "The " + side + " label is hidden");
      }
    }

    info("Hiding the highlighter");
    yield highlighterFront.hide();
  }
}

function* sizeLabelIsCorrect(highlighterFront, inspector) {
  info("Highlight nodes and check size labels");

  let id = ID + "label-size";
  for (let {selector, visible, expected} of SIZED_ELEMENT_TESTS) {
    info("Testing node " + selector);
    let node = yield getNodeFront(selector, inspector);
    yield highlighterFront.show(node);

    let hidden = yield getHighlighterNodeAttribute(highlighterFront, id, "hidden");
    if (!visible) {
      is(hidden, "true", "The size label is hidden");
    } else {
      ok(!hidden, "The size label is visible");

      let label = yield getHighlighterNodeTextContent(highlighterFront, id);
      is(label, expected, "The size label textcontent is correct");
    }

    info("Hiding the highlighter");
    yield highlighterFront.hide();
  }
}
