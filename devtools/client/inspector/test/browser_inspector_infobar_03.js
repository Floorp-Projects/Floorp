/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Bug 1102269 - Make sure info-bar never gets outside of visible area after scrolling

const TEST_URI = URL_ROOT + "doc_inspector_infobar_03.html";

add_task(function* () {
  let {inspector, testActor} = yield openInspectorForURL(TEST_URI);

  let testData = {
    selector: "body",
    position: "overlap",
    style: "top:0px",
  };

  yield testPositionAndStyle(testData, inspector, testActor);
});

function* testPositionAndStyle(test, inspector, testActor) {
  info("Testing " + test.selector);

  yield selectAndHighlightNode(test.selector, inspector);

  let style = yield testActor.getHighlighterNodeAttribute(
    "box-model-nodeinfobar-container", "style");

  is(style.split(";")[0], test.style,
    "Infobar shows on top of the page when page isn't scrolled");

  yield testActor.scrollWindow(0, 500);

  style = yield testActor.getHighlighterNodeAttribute(
    "box-model-nodeinfobar-container", "style");

  is(style.split(";")[0], test.style,
    "Infobar shows on top of the page even if the page is scrolled");
}
