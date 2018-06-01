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

add_task(async function() {
  const helper = await openInspectorForURL(TEST_URL)
                       .then(getHighlighterHelperFor(HIGHLIGHTER_TYPE));

  const { finalize } = helper;

  helper.prefix = ID;

  await hasArrowsAndLabelsAndHandlers(helper);
  await isHiddenForNonPositionedNonSizedElement(helper);
  await sideArrowsAreDisplayedForPositionedNode(helper);

  finalize();
});

async function hasArrowsAndLabelsAndHandlers({getElementAttribute}) {
  info("Checking that the highlighter has the expected arrows and labels");

  for (const name of [...SIDES]) {
    let value = await getElementAttribute("arrow-" + name, "class");
    is(value, ID + "arrow " + name, "The " + name + " arrow exists");

    value = await getElementAttribute("label-text-" + name, "class");
    is(value, ID + "label-text", "The " + name + " label exists");

    value = await getElementAttribute("handler-" + name, "class");
    is(value, ID + "handler-" + name, "The " + name + " handler exists");
  }
}

async function isHiddenForNonPositionedNonSizedElement(
  {show, hide, isElementHidden}) {
  info("Asking to show the highlighter on an inline, non p  ositioned element");

  await show("#inline");

  for (const name of [...SIDES]) {
    let hidden = await isElementHidden("arrow-" + name);
    ok(hidden, "The " + name + " arrow is hidden");

    hidden = await isElementHidden("handler-" + name);
    ok(hidden, "The " + name + " handler is hidden");
  }
}

async function sideArrowsAreDisplayedForPositionedNode(
  {show, hide, isElementHidden}) {
  info("Asking to show the highlighter on the positioned node");

  await show("#positioned");

  for (const name of SIDES) {
    let hidden = await isElementHidden("arrow-" + name);
    ok(!hidden, "The " + name + " arrow is visible for the positioned node");

    hidden = await isElementHidden("handler-" + name);
    ok(!hidden, "The " + name + " handler is visible for the positioned node");
  }

  info("Hiding the highlighter");
  await hide();
}
