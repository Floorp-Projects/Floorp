/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Make sure that the CSS shapes highlighters have the correct size
// in different zoom levels and with different geometry-box.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_cssshapes.html";
const HIGHLIGHTER_TYPE = "ShapesHighlighter";
const TEST_LEVELS = [0.5, 1, 2];

add_task(function* () {
  let inspector = yield openInspectorForURL(TEST_URL);
  let helper = yield getHighlighterHelperFor(HIGHLIGHTER_TYPE)(inspector);
  let {testActor} = inspector;

  yield testZoomSize(testActor, helper);
  yield testGeometryBox(testActor, helper);
  yield testStrokeBox(testActor, helper);

  yield helper.finalize();
});

function* testZoomSize(testActor, helper) {
  yield helper.show("#polygon", {mode: "cssClipPath"});
  let quads = yield testActor.getAllAdjustedQuads("#polygon");
  let { top, left, width, height } = quads.border[0].bounds;
  let expectedStyle = `top:${top}px;left:${left}px;width:${width}px;height:${height}px;`;

  // The top/left/width/height of the highlighter should not change at any zoom level.
  // It should always match the element being highlighted.
  for (let zoom of TEST_LEVELS) {
    info(`Setting zoom level to ${zoom}.`);
    yield testActor.zoomPageTo(zoom, helper.actorID);
    let style = yield helper.getElementAttribute("shapes-shape-container", "style");

    is(style, expectedStyle, `Highlighter has correct quads at zoom level ${zoom}`);
  }
}

function* testGeometryBox(testActor, helper) {
  yield testActor.zoomPageTo(1, helper.actorID);
  yield helper.show("#ellipse", {mode: "cssClipPath"});
  let quads = yield testActor.getAllAdjustedQuads("#ellipse");
  let { top: cTop, left: cLeft,
        width: cWidth, height: cHeight } = quads.content[0].bounds;
  let expectedStyle = `top:${cTop}px;left:${cLeft}px;` +
                      `width:${cWidth}px;height:${cHeight}px;`;
  let style = yield helper.getElementAttribute("shapes-shape-container", "style");
  is(style, expectedStyle, "Highlighter has correct quads for content-box");

  yield helper.show("#ellipse-padding-box", {mode: "cssClipPath"});
  quads = yield testActor.getAllAdjustedQuads("#ellipse-padding-box");
  let { top: pTop, left: pLeft,
        width: pWidth, height: pHeight } = quads.padding[0].bounds;
  expectedStyle = `top:${pTop}px;left:${pLeft}px;` +
                  `width:${pWidth}px;height:${pHeight}px;`;
  style = yield helper.getElementAttribute("shapes-shape-container", "style");
  is(style, expectedStyle, "Highlighter has correct quads for padding-box");
}

function* testStrokeBox(testActor, helper) {
  // #rect has a stroke and doesn't have the clip-path option stroke-box,
  // so we must adjust the quads to reflect the object bounding box.
  yield helper.show("#rect", {mode: "cssClipPath"});
  let quads = yield testActor.getAllAdjustedQuads("#rect");
  let { top, left, width, height } = quads.border[0].bounds;
  let { highlightedNode } = helper;
  let computedStyle = yield highlightedNode.getComputedStyle();
  let strokeWidth = computedStyle["stroke-width"].value;
  let delta = parseFloat(strokeWidth) / 2;

  let expectedStyle = `top:${top + delta}px;left:${left + delta}px;` +
                      `width:${width - 2 * delta}px;height:${height - 2 * delta}px;`;
  let style = yield helper.getElementAttribute("shapes-shape-container", "style");
  is(style, expectedStyle,
    "Highlighter has correct quads for SVG rect with stroke and stroke-box");
}
