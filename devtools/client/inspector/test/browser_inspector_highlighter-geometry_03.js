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

add_task(async function() {
  const helper = await openInspectorForURL(TEST_URL)
                       .then(getHighlighterHelperFor(HIGHLIGHTER_TYPE));

  helper.prefix = ID;

  const { finalize } = helper;

  await checkArrowsLabelsAndHandlers(
    "#node2", ["top", "left", "bottom", "right"],
     helper);

  await checkArrowsLabelsAndHandlers("#node3", ["top", "left"], helper);

  await finalize();
});

async function checkArrowsLabelsAndHandlers(selector, expectedProperties,
  {show, hide, isElementHidden}
) {
  info("Getting node " + selector + " from the page");

  await show(selector);

  for (const name of expectedProperties) {
    const hidden = (await isElementHidden("arrow-" + name)) &&
                 (await isElementHidden("handler-" + name));
    ok(!hidden,
      "The " + name + " label/arrow & handler is visible for node " + selector);
  }

  // Testing that the other arrows are hidden
  for (const name of PROPS) {
    if (expectedProperties.includes(name)) {
      continue;
    }
    const hidden = (await isElementHidden("arrow-" + name)) &&
                 (await isElementHidden("handler-" + name));
    ok(hidden,
      "The " + name + " arrow & handler is hidden for node " + selector);
  }

  info("Hiding the highlighter");
  await hide();
}
