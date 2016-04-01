/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /* Globals defined in: devtools/client/inspector/test/head.js */

"use strict";

// Test that the arrows/handlers and offsetparent and currentnode elements of
// the geometry highlighter only appear when needed.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter-geometry_02.html";
const ID = "geometry-editor-";
const HIGHLIGHTER_TYPE = "GeometryEditorHighlighter";

const TEST_DATA = [{
  selector: "body",
  isOffsetParentVisible: false,
  isCurrentNodeVisible: false,
  hasVisibleArrowsAndHandlers: false
}, {
  selector: "h1",
  isOffsetParentVisible: false,
  isCurrentNodeVisible: false,
  hasVisibleArrowsAndHandlers: false
}, {
  selector: ".absolute",
  isOffsetParentVisible: false,
  isCurrentNodeVisible: true,
  hasVisibleArrowsAndHandlers: true
}, {
  selector: "#absolute-container",
  isOffsetParentVisible: false,
  isCurrentNodeVisible: true,
  hasVisibleArrowsAndHandlers: false
}, {
  selector: ".absolute-bottom-right",
  isOffsetParentVisible: true,
  isCurrentNodeVisible: true,
  hasVisibleArrowsAndHandlers: true
}, {
  selector: ".absolute-width-margin",
  isOffsetParentVisible: true,
  isCurrentNodeVisible: true,
  hasVisibleArrowsAndHandlers: true
}, {
  selector: ".absolute-all-4",
  isOffsetParentVisible: true,
  isCurrentNodeVisible: true,
  hasVisibleArrowsAndHandlers: true
}, {
  selector: ".relative",
  isOffsetParentVisible: true,
  isCurrentNodeVisible: true,
  hasVisibleArrowsAndHandlers: true
}, {
  selector: ".static",
  isOffsetParentVisible: false,
  isCurrentNodeVisible: false,
  hasVisibleArrowsAndHandlers: false
}, {
  selector: ".static-size",
  isOffsetParentVisible: false,
  isCurrentNodeVisible: true,
  hasVisibleArrowsAndHandlers: false
}, {
  selector: ".fixed",
  isOffsetParentVisible: false,
  isCurrentNodeVisible: true,
  hasVisibleArrowsAndHandlers: true
}];

add_task(function* () {
  let helper = yield openInspectorForURL(TEST_URL)
                       .then(getHighlighterHelperFor(HIGHLIGHTER_TYPE));

  helper.prefix = ID;

  let { hide, finalize } = helper;

  for (let data of TEST_DATA) {
    yield testNode(helper, data);
  }

  info("Hiding the highlighter");
  yield hide();
  yield finalize();
});

function* testNode(helper, data) {
  let { selector } = data;
  yield helper.show(data.selector);

  is((yield isOffsetParentVisible(helper)), data.isOffsetParentVisible,
    "The offset-parent highlighter visibility is correct for node " + selector);
  is((yield isCurrentNodeVisible(helper)), data.isCurrentNodeVisible,
    "The current-node highlighter visibility is correct for node " + selector);
  is((yield hasVisibleArrowsAndHandlers(helper)),
    data.hasVisibleArrowsAndHandlers,
    "The arrows visibility is correct for node " + selector);
}

function* isOffsetParentVisible({isElementHidden}) {
  return !(yield isElementHidden("offset-parent"));
}

function* isCurrentNodeVisible({isElementHidden}) {
  return !(yield isElementHidden("current-node"));
}

function* hasVisibleArrowsAndHandlers({isElementHidden}) {
  for (let side of ["top", "left", "bottom", "right"]) {
    let hidden = yield isElementHidden("arrow-" + side);
    if (!hidden) {
      return !(yield isElementHidden("handler-" + side));
    }
  }
  return false;
}
