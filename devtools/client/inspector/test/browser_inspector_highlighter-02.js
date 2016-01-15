/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the highlighter is correctly displayed over a variety of elements

const TEST_URI = URL_ROOT + "doc_inspector_highlighter.html";

add_task(function*() {
  let {toolbox, inspector, testActor} = yield openInspectorForURL(TEST_URI);

  info("Selecting the simple, non-transformed DIV");
  yield selectAndHighlightNode("#simple-div", inspector);

  let isVisible = yield testActor.isHighlighting();
  ok(isVisible, "The highlighter is shown");
  ok((yield testActor.assertHighlightedNode("#simple-div")),
    "The highlighter's outline corresponds to the simple div");
  yield testActor.isNodeCorrectlyHighlighted("#simple-div", is, "non-zoomed");

  info("Selecting the rotated DIV");
  yield selectAndHighlightNode("#rotated-div", inspector);

  isVisible = yield testActor.isHighlighting();
  ok(isVisible, "The highlighter is shown");
  yield testActor.isNodeCorrectlyHighlighted("#rotated-div", is, "rotated");

  info("Selecting the zero width height DIV");
  yield selectAndHighlightNode("#widthHeightZero-div", inspector);

  isVisible = yield testActor.isHighlighting();
  ok(isVisible, "The highlighter is shown");
  yield testActor.isNodeCorrectlyHighlighted("#widthHeightZero-div", is, "zero width height");
});
