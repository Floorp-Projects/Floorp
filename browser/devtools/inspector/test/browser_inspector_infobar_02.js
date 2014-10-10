/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

///////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("Protocol error (unknownError): Error: Got an invalid root window in DocumentWalker");

const TEST_URI = "http://example.com/browser/browser/devtools/inspector/" +
                 "test/browser_inspector_infobar_02.html";
const DOORHANGER_ARROW_HEIGHT = 5;

// Test that the nodeinfobar is never displayed above the top or below the
// bottom of the content area.
let test = asyncTest(function*() {
  info("Loading the test document and opening the inspector");

  yield addTab(TEST_URI);

  let {inspector} = yield openInspector();

  yield checkInfoBarAboveTop(inspector);
  yield checkInfoBarBelowFindbar(inspector);

  gBrowser.removeCurrentTab();
});

function* checkInfoBarAboveTop(inspector) {
  yield selectAndHighlightNode("#above-top", inspector);

  let positioner = getPositioner();
  let positionerTop = parseInt(positioner.style.top, 10);
  let insideContent = positionerTop >= -DOORHANGER_ARROW_HEIGHT;

  ok(insideContent, "Infobar is inside the content window (top = " +
                    positionerTop + ", content = '" +
                    positioner.textContent +"')");
}

function* checkInfoBarBelowFindbar(inspector) {
  gFindBar.open();

  // Ensure that the findbar is fully open.
  yield once(gFindBar, "transitionend");
  yield selectAndHighlightNode("#below-bottom", inspector);

  let positioner = getPositioner();
  let positionerBottom =
    positioner.getBoundingClientRect().bottom - DOORHANGER_ARROW_HEIGHT;
  let findBarTop = gFindBar.getBoundingClientRect().top;

  let insideContent = positionerBottom <= findBarTop;

  ok(insideContent, "Infobar does not overlap the findbar (findBarTop = " +
                    findBarTop + ", positionerBottom = " + positionerBottom +
                    ", content = '" + positioner.textContent +"')");

  gFindBar.close();
  yield once(gFindBar, "transitionend");
}

function getPositioner() {
  let browser = gBrowser.selectedBrowser;
  let stack = browser.parentNode;

  return stack.querySelector(".highlighter-nodeinfobar-positioner");
}
