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

add_task(async function() {
  const helper = await openInspectorForURL(TEST_URL)
                       .then(getHighlighterHelperFor(HIGHLIGHTER_TYPE));

  helper.prefix = ID;

  const { hide, finalize } = helper;

  for (const data of TEST_DATA) {
    await testNode(helper, data);
  }

  info("Hiding the highlighter");
  await hide();
  await finalize();
});

async function testNode(helper, data) {
  const { selector } = data;
  await helper.show(data.selector);

  is((await isOffsetParentVisible(helper)), data.isOffsetParentVisible,
    "The offset-parent highlighter visibility is correct for node " + selector);
  is((await isCurrentNodeVisible(helper)), data.isCurrentNodeVisible,
    "The current-node highlighter visibility is correct for node " + selector);
  is((await hasVisibleArrowsAndHandlers(helper)),
    data.hasVisibleArrowsAndHandlers,
    "The arrows visibility is correct for node " + selector);
}

async function isOffsetParentVisible({isElementHidden}) {
  return !(await isElementHidden("offset-parent"));
}

async function isCurrentNodeVisible({isElementHidden}) {
  return !(await isElementHidden("current-node"));
}

async function hasVisibleArrowsAndHandlers({isElementHidden}) {
  for (const side of ["top", "left", "bottom", "right"]) {
    const hidden = await isElementHidden("arrow-" + side);
    if (!hidden) {
      return !(await isElementHidden("handler-" + side));
    }
  }
  return false;
}
