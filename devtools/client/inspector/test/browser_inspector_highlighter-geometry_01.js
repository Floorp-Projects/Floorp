/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test the creation of the geometry highlighter elements.

const TEST_URL = `data:text/html;charset=utf-8,
                  <span id='inline'></span>
                  <div id='positioned' style='
                    background:yellow;
                    position:absolute;
                    left:5rem;
                    top:30px;
                    right:300px;
                    bottom:10em;'></div>
                  <div id='sized' style='
                    background:red;
                    width:5em;
                    height:50%;'></div>`;

const HIGHLIGHTER_TYPE = "GeometryEditorHighlighter";

const ID = "geometry-editor-";
const SIDES = ["left", "right", "top", "bottom"];

add_task(function* () {
  let helper = yield openInspectorForURL(TEST_URL)
                       .then(getHighlighterHelperFor(HIGHLIGHTER_TYPE));

  let { finalize } = helper;

  helper.prefix = ID;

  yield hasArrowsAndLabelsAndHandlers(helper);
  yield isHiddenForNonPositionedNonSizedElement(helper);
  yield sideArrowsAreDisplayedForPositionedNode(helper);

  finalize();
});

function* hasArrowsAndLabelsAndHandlers({getElementAttribute}) {
  info("Checking that the highlighter has the expected arrows and labels");

  for (let name of [...SIDES]) {
    let value = yield getElementAttribute("arrow-" + name, "class");
    is(value, ID + "arrow " + name, "The " + name + " arrow exists");

    value = yield getElementAttribute("label-text-" + name, "class");
    is(value, ID + "label-text", "The " + name + " label exists");

    value = yield getElementAttribute("handler-" + name, "class");
    is(value, ID + "handler-" + name, "The " + name + " handler exists");
  }
}

function* isHiddenForNonPositionedNonSizedElement(
  {show, hide, isElementHidden}) {
  info("Asking to show the highlighter on an inline, non p  ositioned element");

  yield show("#inline");

  for (let name of [...SIDES]) {
    let hidden = yield isElementHidden("arrow-" + name);
    ok(hidden, "The " + name + " arrow is hidden");

    hidden = yield isElementHidden("handler-" + name);
    ok(hidden, "The " + name + " handler is hidden");
  }
}

function* sideArrowsAreDisplayedForPositionedNode(
  {show, hide, isElementHidden}) {
  info("Asking to show the highlighter on the positioned node");

  yield show("#positioned");

  for (let name of SIDES) {
    let hidden = yield isElementHidden("arrow-" + name);
    ok(!hidden, "The " + name + " arrow is visible for the positioned node");

    hidden = yield isElementHidden("handler-" + name);
    ok(!hidden, "The " + name + " handler is visible for the positioned node");
  }

  info("Hiding the highlighter");
  yield hide();
}
