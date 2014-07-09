/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the highlighter is correctly displayed over a variety of elements

const TEST_URI = TEST_URL_ROOT + "browser_inspector_highlighter.html";

let test = asyncTest(function*() {
  let { inspector } = yield openInspectorForURL(TEST_URI);

  info("Selecting the simple, non-transformed DIV");
  let div = getNode("#simple-div");
  yield selectNode(div, inspector, "highlight");

  testSimpleDivHighlighted(div);
  yield zoomTo(2);
  testZoomedSimpleDivHighlighted(div);
  yield zoomTo(1);

  info("Selecting the rotated DIV");
  let rotated = getNode("#rotated-div");
  let onBoxModelUpdate = waitForBoxModelUpdate();
  yield selectNode(rotated, inspector, "highlight");
  yield onBoxModelUpdate;

  testMouseOverRotatedHighlights(rotated);

  info("Selecting the zero width height DIV");
  let zeroWidthHeight = getNode("#widthHeightZero-div");
  let onBoxModelUpdate = waitForBoxModelUpdate();
  yield selectNode(zeroWidthHeight, inspector, "highlight");
  yield onBoxModelUpdate;

  testMouseOverWidthHeightZeroDiv(zeroWidthHeight);

});

function testSimpleDivHighlighted(div) {
  ok(isHighlighting(), "The highlighter is shown");
  is(getHighlitNode(), div, "The highlighter's outline corresponds to the simple div");

  info("Checking that the simple div is correctly highlighted");
  isNodeCorrectlyHighlighted(div, "non-zoomed");
}

function testZoomedSimpleDivHighlighted(div) {
  info("Checking that the simple div is correctly highlighted, " +
    "even when the page is zoomed");
  isNodeCorrectlyHighlighted(div, "zoomed");
}

function zoomTo(level) {
  info("Zooming page to " + level);
  let def = promise.defer();

  waitForBoxModelUpdate().then(def.resolve);
  let contentViewer = gBrowser.selectedBrowser.docShell.contentViewer
                              .QueryInterface(Ci.nsIMarkupDocumentViewer);
  contentViewer.fullZoom = level;

  return def.promise;
}

function testMouseOverRotatedHighlights(rotated) {
  ok(isHighlighting(), "The highlighter is shown");
  info("Checking that the rotated div is correctly highlighted");
  isNodeCorrectlyHighlighted(rotated, "rotated");
}

function testMouseOverWidthHeightZeroDiv(zeroHeightWidthDiv) {
  ok(isHighlighting(), "The highlighter is shown");
  info("Checking that the zero width height div is correctly highlighted");
  isNodeCorrectlyHighlighted(zeroHeightWidthDiv, "zero width height");

}
