/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Bug 1102269 - Make sure info-bar never gets outside of visible area after scrolling

const TEST_URI = URL_ROOT + "doc_inspector_infobar_03.html";

add_task(async function() {
  const { inspector, highlighterTestFront } = await openInspectorForURL(
    TEST_URI
  );

  const testData = {
    selector: "body",
    position: "overlap",
    style: "position:fixed",
  };

  await testPositionAndStyle(testData, inspector, highlighterTestFront);
});

async function testPositionAndStyle(test, inspector, highlighterTestFront) {
  info("Testing " + test.selector);

  await selectAndHighlightNode(test.selector, inspector);

  let style = await highlighterTestFront.getHighlighterNodeAttribute(
    "box-model-infobar-container",
    "style"
  );

  is(
    style.split(";")[0].trim(),
    test.style,
    "Infobar shows on top of the page when page isn't scrolled"
  );

  info("Scroll down");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    return new Promise(resolve => {
      content.addEventListener("scroll", () => resolve(), { once: true });
      content.scrollTo({ top: 500 });
    });
  });

  style = await highlighterTestFront.getHighlighterNodeAttribute(
    "box-model-infobar-container",
    "style"
  );

  is(
    style.split(";")[0].trim(),
    test.style,
    "Infobar shows on top of the page even if the page is scrolled"
  );
}
