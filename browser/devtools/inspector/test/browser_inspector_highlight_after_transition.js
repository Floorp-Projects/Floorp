/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URI = TEST_URL_ROOT + "doc_inspector_highlight_after_transition.html";

// Test that the nodeinfobar is never displayed above the top or below the
// bottom of the content area.
add_task(function*() {
  info("Loading the test document and opening the inspector");

  let {inspector} = yield openInspectorForURL(TEST_URI);

  yield checkDivHeight(inspector);
});

function* checkDivHeight(inspector) {
  let div = getNode("div");

  div.setAttribute("visible", "true");

  yield once(div, "transitionend");
  yield selectAndHighlightNode(div, inspector);

  let height = div.getBoundingClientRect().height;

  is (height, 201, "div is the correct height");
}
