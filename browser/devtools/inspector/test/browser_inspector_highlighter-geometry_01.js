/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test the creation of the geometry highlighter elements.

const TEST_URL = "data:text/html;charset=utf-8," +
                 "<span id='inline'></span>" +
                 "<div id='positioned' style='background:yellow;position:absolute;left:5rem;top:30px;right:300px;bottom:10em;'></div>" +
                 "<div id='sized' style='background:red;width:5em;height:50%;'></div>";
const ID = "geometry-editor-";
const SIDES = ["left", "right", "top", "bottom"];
const SIZES = ["width", "height"];

add_task(function*() {
  let {inspector, testActor} = yield openInspectorForURL(TEST_URL);
  let front = inspector.inspector;

  let highlighter = yield front.getHighlighterByType("GeometryEditorHighlighter");

  yield hasArrowsAndLabels(highlighter, inspector, testActor);
  yield isHiddenForNonPositionedNonSizedElement(highlighter, inspector, testActor);
  yield sideArrowsAreDisplayedForPositionedNode(highlighter, inspector, testActor);
  yield sizeLabelIsDisplayedForSizedNode(highlighter, inspector, testActor);

  yield highlighter.finalize();
});

function* hasArrowsAndLabels(highlighterFront, inspector, testActor) {
  info("Checking that the highlighter has the expected arrows and labels");

  for (let name of [...SIDES]) {
    let value = yield testActor.getHighlighterNodeAttribute(ID + "arrow-" + name, "class", highlighterFront);
    is(value, ID + "arrow " + name, "The " + name + " arrow exists");

    value = yield testActor.getHighlighterNodeAttribute(ID + "label-text-" + name, "class", highlighterFront);
    is(value, ID + "label-text", "The " + name + " label exists");
  }

  let value = yield testActor.getHighlighterNodeAttribute(ID + "label-text-size", "class", highlighterFront);
  is(value, ID + "label-text", "The size label exists");
}

function* isHiddenForNonPositionedNonSizedElement(highlighterFront, inspector, testActor) {
  info("Asking to show the highlighter on an inline, non positioned element");

  let node = yield getNodeFront("#inline", inspector);
  yield highlighterFront.show(node);

  for (let name of [...SIDES]) {
    let hidden = yield testActor.getHighlighterNodeAttribute(ID + "arrow-" + name, "hidden", highlighterFront);
    is(hidden, "true", "The " + name + " arrow is hidden");
  }

  let hidden = yield testActor.getHighlighterNodeAttribute(ID + "label-size", "hidden", highlighterFront);
  is(hidden, "true", "The size label is hidden");
}

function* sideArrowsAreDisplayedForPositionedNode(highlighterFront, inspector, testActor) {
  info("Asking to show the highlighter on the positioned node");

  let node = yield getNodeFront("#positioned", inspector);
  yield highlighterFront.show(node);

  for (let name of SIDES) {
    let hidden = yield testActor.getHighlighterNodeAttribute(ID + "arrow-" + name, "hidden", highlighterFront);
    ok(!hidden, "The " + name + " arrow is visible for the positioned node");
  }

  let hidden = yield testActor.getHighlighterNodeAttribute(ID + "label-size", "hidden", highlighterFront);
  is(hidden, "true", "The size label is hidden");

  info("Hiding the highlighter");
  yield highlighterFront.hide();
}

function* sizeLabelIsDisplayedForSizedNode(highlighterFront, inspector, testActor) {
  info("Asking to show the highlighter on the sized node");

  let node = yield getNodeFront("#sized", inspector);
  yield highlighterFront.show(node);

  let hidden = yield testActor.getHighlighterNodeAttribute(ID + "label-size", "hidden", highlighterFront);
  ok(!hidden, "The size label is visible");

  for (let name of SIDES) {
    let hidden = yield testActor.getHighlighterNodeAttribute(ID + "arrow-" + name, "hidden", highlighterFront);
    is(hidden, "true", "The " + name + " arrow is hidden for the sized node");
  }

  info("Hiding the highlighter");
  yield highlighterFront.hide();
}
