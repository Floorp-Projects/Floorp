/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
Bug 1014547 - CSS transforms highlighter
Test that the highlighter elements created have the right size and coordinates.

Note that instead of hard-coding values here, the assertions are made by
comparing with the result of LayoutHelpers.getAdjustedQuads.

There's a separate test for checking that getAdjustedQuads actually returns
sensible values
(browser/devtools/shared/test/browser_layoutHelpers-getBoxQuads.js),
so the present test doesn't care about that, it just verifies that the css
transform highlighter applies those values correctly to the SVG elements
*/

const TEST_URL = TEST_URL_ROOT + "doc_inspector_highlighter_csstransform.html";

add_task(function*() {
  let {inspector, toolbox, testActor} = yield openInspectorForURL(TEST_URL);
  let front = inspector.inspector;

  let highlighter = yield front.getHighlighterByType("CssTransformHighlighter");

  let nodeFront = yield getNodeFront("#test-node", inspector);

  info("Displaying the transform highlighter on test node");
  yield highlighter.show(nodeFront);

  let data = yield testActor.getAllAdjustedQuads("#test-node");
  let [expected] = data.border;

  let points = yield testActor.getHighlighterNodeAttribute("css-transform-transformed", "points", highlighter);
  let polygonPoints = points.split(" ").map(p => {
    return {
      x: +p.substring(0, p.indexOf(",")),
      y: +p.substring(p.indexOf(",")+1)
    };
  });

  for (let i = 1; i < 5; i ++) {
    is(polygonPoints[i - 1].x, expected["p" + i].x,
      "p" + i + " x coordinate is correct");
    is(polygonPoints[i - 1].y, expected["p" + i].y,
      "p" + i + " y coordinate is correct");
  }

  info("Hiding the transform highlighter");
  yield highlighter.hide();
  yield highlighter.finalize();
});
