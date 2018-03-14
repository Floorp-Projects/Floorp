/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that shapes in iframes are updated correctly on mouse events.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_cssshapes_iframe.html";
const HIGHLIGHTER_TYPE = "ShapesHighlighter";

add_task(function* () {
  let inspector = yield openInspectorForURL(TEST_URL);
  let helper = yield getHighlighterHelperFor(HIGHLIGHTER_TYPE)(inspector);
  let {testActor} = inspector;

  yield testPolygonIframeMovePoint(testActor, helper);

  yield helper.finalize();
});

function* testPolygonIframeMovePoint(testActor, helper) {
  info("Displaying polygon");
  yield helper.show("#polygon", {mode: "cssClipPath"}, "#frame");
  let { mouse, highlightedNode } = helper;

  info("Moving polygon point visible in iframe");
  yield mouse.down(10, 10);
  yield mouse.move(20, 20);
  yield mouse.up();
  yield testActor.reflow();

  let computedStyle = yield highlightedNode.getComputedStyle();
  let definition = computedStyle["clip-path"].value;
  ok(definition.includes("10px 10px"), "Point moved to 10px 10px");

  info("Moving polygon point not visible in iframe");
  yield mouse.down(110, 410);
  yield mouse.move(120, 420);
  yield mouse.up();
  yield testActor.reflow();

  computedStyle = yield highlightedNode.getComputedStyle();
  definition = computedStyle["clip-path"].value;
  ok(definition.includes("110px 51.25%"), "Point moved to 110px 51.25%");
}
