/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Bug 1102269 - Make sure info-bar never gets outside of visible area after scrolling

const TEST_URI = URL_ROOT + "doc_inspector_infobar_03.html";

add_task(async function() {
  const {inspector, testActor} = await openInspectorForURL(TEST_URI);

  const testData = {
    selector: "body",
    position: "overlap",
    style: "position:fixed",
  };

  await testPositionAndStyle(testData, inspector, testActor);
});

async function testPositionAndStyle(test, inspector, testActor) {
  info("Testing " + test.selector);

  await selectAndHighlightNode(test.selector, inspector);

  let style = await testActor.getHighlighterNodeAttribute(
    "box-model-infobar-container", "style");

  is(style.split(";")[0].trim(), test.style,
    "Infobar shows on top of the page when page isn't scrolled");

  await testActor.scrollWindow(0, 500);

  style = await testActor.getHighlighterNodeAttribute(
    "box-model-infobar-container", "style");

  is(style.split(";")[0].trim(), test.style,
    "Infobar shows on top of the page even if the page is scrolled");
}
