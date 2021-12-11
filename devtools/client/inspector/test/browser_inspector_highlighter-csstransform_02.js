/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
Bug 1014547 - CSS transforms highlighter
Test that the highlighter elements created have the right size and coordinates.

Note that instead of hard-coding values here, the assertions are made by
comparing with the result of getAdjustedQuads.

There's a separate test for checking that getAdjustedQuads actually returns
sensible values
(devtools/client/shared/test/browser_layoutHelpers-getBoxQuads.js),
so the present test doesn't care about that, it just verifies that the css
transform highlighter applies those values correctly to the SVG elements
*/

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_csstransform.html";

add_task(async function() {
  const { inspector, highlighterTestFront } = await openInspectorForURL(
    TEST_URL
  );
  const front = inspector.inspectorFront;

  const highlighter = await front.getHighlighterByType(
    "CssTransformHighlighter"
  );

  const nodeFront = await getNodeFront("#test-node", inspector);

  info("Displaying the transform highlighter on test node");
  await highlighter.show(nodeFront);

  const data = await getAllAdjustedQuadsForContentPageElement("#test-node");
  const [expected] = data.border;

  const points = await highlighterTestFront.getHighlighterNodeAttribute(
    "css-transform-transformed",
    "points",
    highlighter
  );
  const polygonPoints = points.split(" ").map(p => {
    return {
      x: +p.substring(0, p.indexOf(",")),
      y: +p.substring(p.indexOf(",") + 1),
    };
  });

  for (let i = 1; i < 5; i++) {
    is(
      polygonPoints[i - 1].x,
      expected["p" + i].x,
      "p" + i + " x coordinate is correct"
    );
    is(
      polygonPoints[i - 1].y,
      expected["p" + i].y,
      "p" + i + " y coordinate is correct"
    );
  }

  info("Hiding the transform highlighter");
  await highlighter.hide();
  await highlighter.finalize();
});
