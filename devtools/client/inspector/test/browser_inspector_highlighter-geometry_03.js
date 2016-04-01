/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Globals defined in: devtools/client/inspector/test/head.js */

"use strict";

// Test that the right arrows/labels are shown even when the css properties are
// in several different css rules.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter-geometry_01.html";
const ID = "geometry-editor-";
const HIGHLIGHTER_TYPE = "GeometryEditorHighlighter";
const PROPS = ["left", "right", "top", "bottom"];

add_task(function* () {
  let helper = yield openInspectorForURL(TEST_URL)
                       .then(getHighlighterHelperFor(HIGHLIGHTER_TYPE));

  helper.prefix = ID;

  let { finalize } = helper;

  yield checkArrowsLabelsAndHandlers(
    "#node2", ["top", "left", "bottom", "right"],
     helper);

  yield checkArrowsLabelsAndHandlers("#node3", ["top", "left"], helper);

  yield finalize();
});

function* checkArrowsLabelsAndHandlers(selector, expectedProperties,
  {show, hide, isElementHidden}
) {
  info("Getting node " + selector + " from the page");

  yield show(selector);

  for (let name of expectedProperties) {
    let hidden = (yield isElementHidden("arrow-" + name)) &&
                 (yield isElementHidden("handler-" + name));
    ok(!hidden,
      "The " + name + " label/arrow & handler is visible for node " + selector);
  }

  // Testing that the other arrows are hidden
  for (let name of PROPS) {
    if (expectedProperties.indexOf(name) !== -1) {
      continue;
    }
    let hidden = (yield isElementHidden("arrow-" + name)) &&
                 (yield isElementHidden("handler-" + name));
    ok(hidden,
      "The " + name + " arrow & handler is hidden for node " + selector);
  }

  info("Hiding the highlighter");
  yield hide();
}
