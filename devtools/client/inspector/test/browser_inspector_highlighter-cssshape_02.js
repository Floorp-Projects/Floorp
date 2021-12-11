/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Make sure that the CSS shapes highlighters have the correct attributes.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_cssshapes.html";
const HIGHLIGHTER_TYPE = "ShapesHighlighter";

add_task(async function() {
  const { inspector, highlighterTestFront } = await openInspectorForURL(
    TEST_URL
  );
  const front = inspector.inspectorFront;
  const highlighter = await front.getHighlighterByType(HIGHLIGHTER_TYPE);

  await polygonHasCorrectAttrs(highlighterTestFront, inspector, highlighter);
  await circleHasCorrectAttrs(highlighterTestFront, inspector, highlighter);
  await ellipseHasCorrectAttrs(highlighterTestFront, inspector, highlighter);
  await insetHasCorrectAttrs(highlighterTestFront, inspector, highlighter);

  await highlighter.finalize();
});

async function polygonHasCorrectAttrs(
  highlighterTestFront,
  inspector,
  highlighterFront
) {
  info("Checking polygon highlighter has correct points");

  const polygonNode = await getNodeFront("#polygon", inspector);
  await highlighterFront.show(polygonNode, { mode: "cssClipPath" });

  const points = await highlighterTestFront.getHighlighterNodeAttribute(
    "shapes-polygon",
    "points",
    highlighterFront
  );
  const realPoints =
    "0,0 12.5,50 25,0 37.5,50 50,0 62.5,50 " +
    "75,0 87.5,50 100,0 90,100 50,60 10,100";
  is(points, realPoints, "Polygon highlighter has correct points");
}

async function circleHasCorrectAttrs(
  highlighterTestFront,
  inspector,
  highlighterFront
) {
  info("Checking circle highlighter has correct attributes");

  const circleNode = await getNodeFront("#circle", inspector);
  await highlighterFront.show(circleNode, { mode: "cssClipPath" });

  const rx = await highlighterTestFront.getHighlighterNodeAttribute(
    "shapes-ellipse",
    "rx",
    highlighterFront
  );
  const ry = await highlighterTestFront.getHighlighterNodeAttribute(
    "shapes-ellipse",
    "ry",
    highlighterFront
  );
  const cx = await highlighterTestFront.getHighlighterNodeAttribute(
    "shapes-ellipse",
    "cx",
    highlighterFront
  );
  const cy = await highlighterTestFront.getHighlighterNodeAttribute(
    "shapes-ellipse",
    "cy",
    highlighterFront
  );

  is(rx, "25", "Circle highlighter has correct rx");
  is(ry, "25", "Circle highlighter has correct ry");
  is(cx, "30", "Circle highlighter has correct cx");
  is(cy, "40", "Circle highlighter has correct cy");
}

async function ellipseHasCorrectAttrs(
  highlighterTestFront,
  inspector,
  highlighterFront
) {
  info("Checking ellipse highlighter has correct attributes");

  const ellipseNode = await getNodeFront("#ellipse", inspector);
  await highlighterFront.show(ellipseNode, { mode: "cssClipPath" });

  const rx = await highlighterTestFront.getHighlighterNodeAttribute(
    "shapes-ellipse",
    "rx",
    highlighterFront
  );
  const ry = await highlighterTestFront.getHighlighterNodeAttribute(
    "shapes-ellipse",
    "ry",
    highlighterFront
  );
  const cx = await highlighterTestFront.getHighlighterNodeAttribute(
    "shapes-ellipse",
    "cx",
    highlighterFront
  );
  const cy = await highlighterTestFront.getHighlighterNodeAttribute(
    "shapes-ellipse",
    "cy",
    highlighterFront
  );

  is(rx, "40", "Ellipse highlighter has correct rx");
  is(ry, "30", "Ellipse highlighter has correct ry");
  is(cx, "25", "Ellipse highlighter has correct cx");
  is(cy, "30", "Ellipse highlighter has correct cy");
}

async function insetHasCorrectAttrs(
  highlighterTestFront,
  inspector,
  highlighterFront
) {
  info("Checking rect highlighter has correct attributes");

  const insetNode = await getNodeFront("#inset", inspector);
  await highlighterFront.show(insetNode, { mode: "cssClipPath" });

  const x = await highlighterTestFront.getHighlighterNodeAttribute(
    "shapes-rect",
    "x",
    highlighterFront
  );
  const y = await highlighterTestFront.getHighlighterNodeAttribute(
    "shapes-rect",
    "y",
    highlighterFront
  );
  const width = await highlighterTestFront.getHighlighterNodeAttribute(
    "shapes-rect",
    "width",
    highlighterFront
  );
  const height = await highlighterTestFront.getHighlighterNodeAttribute(
    "shapes-rect",
    "height",
    highlighterFront
  );

  is(x, "15", "Rect highlighter has correct x");
  is(y, "25", "Rect highlighter has correct y");
  is(width, "72.5", "Rect highlighter has correct width");
  is(height, "45", "Rect highlighter has correct height");
}
