/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the arrows and offsetparent and currentnode elements of the
// geometry highlighter only appear when needed.

const TEST_URL = TEST_URL_ROOT + "doc_inspector_highlighter-geometry_02.html";
const ID = "geometry-editor-";

const TEST_DATA = [{
  selector: "body",
  isOffsetParentVisible: false,
  isCurrentNodeVisible: false,
  hasVisibleArrows: false,
  isSizeVisible: false
}, {
  selector: "h1",
  isOffsetParentVisible: false,
  isCurrentNodeVisible: false,
  hasVisibleArrows: false,
  isSizeVisible: false
}, {
  selector: ".absolute",
  isOffsetParentVisible: false,
  isCurrentNodeVisible: true,
  hasVisibleArrows: true,
  isSizeVisible: false
}, {
  selector: "#absolute-container",
  isOffsetParentVisible: false,
  isCurrentNodeVisible: true,
  hasVisibleArrows: false,
  isSizeVisible: true
}, {
  selector: ".absolute-bottom-right",
  isOffsetParentVisible: true,
  isCurrentNodeVisible: true,
  hasVisibleArrows: true,
  isSizeVisible: false
}, {
  selector: ".absolute-width-margin",
  isOffsetParentVisible: true,
  isCurrentNodeVisible: true,
  hasVisibleArrows: true,
  isSizeVisible: true
}, {
  selector: ".absolute-all-4",
  isOffsetParentVisible: true,
  isCurrentNodeVisible: true,
  hasVisibleArrows: true,
  isSizeVisible: false
}, {
  selector: ".relative",
  isOffsetParentVisible: true,
  isCurrentNodeVisible: true,
  hasVisibleArrows: true,
  isSizeVisible: false
}, {
  selector: ".static",
  isOffsetParentVisible: false,
  isCurrentNodeVisible: false,
  hasVisibleArrows: false,
  isSizeVisible: false
}, {
  selector: ".static-size",
  isOffsetParentVisible: false,
  isCurrentNodeVisible: true,
  hasVisibleArrows: false,
  isSizeVisible: true
}, {
  selector: ".fixed",
  isOffsetParentVisible: false,
  isCurrentNodeVisible: true,
  hasVisibleArrows: true,
  isSizeVisible: false
}];

add_task(function*() {
  let {inspector, testActor} = yield openInspectorForURL(TEST_URL);
  let front = inspector.inspector;

  let highlighter = yield front.getHighlighterByType("GeometryEditorHighlighter");

  for (let data of TEST_DATA) {
    yield testNode(inspector, highlighter, testActor, data);
  }

  info("Hiding the highlighter");
  yield highlighter.hide();
  yield highlighter.finalize();
});

function* testNode(inspector, highlighter, testActor, data) {
  info("Highlighting the test node " + data.selector);
  let node = yield getNodeFront(data.selector, inspector);
  yield highlighter.show(node);

  is((yield isOffsetParentVisible(highlighter, testActor)), data.isOffsetParentVisible,
    "The offset-parent highlighter visibility is correct for node " + data.selector);
  is((yield isCurrentNodeVisible(highlighter, testActor)), data.isCurrentNodeVisible,
    "The current-node highlighter visibility is correct for node " + data.selector);
  is((yield hasVisibleArrows(highlighter, testActor)), data.hasVisibleArrows,
    "The arrows visibility is correct for node " + data.selector);
  is((yield isSizeVisible(highlighter, testActor)), data.isSizeVisible,
    "The size label visibility is correct for node " + data.selector);
}

function* isOffsetParentVisible(highlighter, testActor) {
  let hidden = yield testActor.getHighlighterNodeAttribute(ID + "offset-parent", "hidden", highlighter);
  return !hidden;
}

function* isCurrentNodeVisible(highlighter, testActor) {
  let hidden = yield testActor.getHighlighterNodeAttribute(ID + "current-node", "hidden", highlighter);
  return !hidden;
}

function* hasVisibleArrows(highlighter, testActor) {
  for (let side of ["top", "left", "bottom", "right"]) {
    let hidden = yield testActor.getHighlighterNodeAttribute(ID + "arrow-" + side, "hidden", highlighter);
    if (!hidden) {
      return true;
    }
  }
  return false;
}

function* isSizeVisible(highlighter, testActor) {
  let hidden = yield testActor.getHighlighterNodeAttribute(ID + "label-size", "hidden", highlighter);
  return !hidden;
}
