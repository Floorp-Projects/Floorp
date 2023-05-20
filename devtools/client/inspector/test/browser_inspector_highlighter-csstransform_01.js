/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test the creation of the SVG highlighter elements of the css transform
// highlighter.

const TEST_URL = `
 <div id="transformed"
   style="border:1px solid red;width:100px;height:100px;transform:skew(13deg);">
 </div>
 <div id="untransformed"
   style="border:1px solid blue;width:100px;height:100px;">
 </div>
 <span id="inline"
   style="transform:rotate(90deg);">this is an inline transformed element
 </span>
`;

add_task(async function () {
  const { inspector, highlighterTestFront } = await openInspectorForURL(
    "data:text/html;charset=utf-8," + encodeURI(TEST_URL)
  );
  const front = inspector.inspectorFront;

  const highlighter = await front.getHighlighterByType(
    "CssTransformHighlighter"
  );

  await isHiddenByDefault(highlighterTestFront, highlighter);
  await has2PolygonsAnd4Lines(highlighterTestFront, highlighter);
  await isNotShownForUntransformed(
    highlighterTestFront,
    inspector,
    highlighter
  );
  await isNotShownForInline(highlighterTestFront, inspector, highlighter);
  await isVisibleWhenShown(highlighterTestFront, inspector, highlighter);
  await linesLinkThePolygons(highlighterTestFront, inspector, highlighter);

  await highlighter.finalize();
});

async function isHiddenByDefault(highlighterTestFront, highlighterFront) {
  info("Checking that the highlighter is hidden by default");

  const hidden = await highlighterTestFront.getHighlighterNodeAttribute(
    "css-transform-elements",
    "hidden",
    highlighterFront
  );
  ok(hidden, "The highlighter is hidden by default");
}

async function has2PolygonsAnd4Lines(highlighterTestFront, highlighterFront) {
  info("Checking that the highlighter is made up of 4 lines and 2 polygons");

  let value = await highlighterTestFront.getHighlighterNodeAttribute(
    "css-transform-untransformed",
    "class",
    highlighterFront
  );
  is(value, "css-transform-untransformed", "The untransformed polygon exists");

  value = await highlighterTestFront.getHighlighterNodeAttribute(
    "css-transform-transformed",
    "class",
    highlighterFront
  );
  is(value, "css-transform-transformed", "The transformed polygon exists");

  for (const nb of ["1", "2", "3", "4"]) {
    value = await highlighterTestFront.getHighlighterNodeAttribute(
      "css-transform-line" + nb,
      "class",
      highlighterFront
    );
    is(value, "css-transform-line", "The line " + nb + " exists");
  }
}

async function isNotShownForUntransformed(
  highlighterTestFront,
  inspector,
  highlighterFront
) {
  info("Asking to show the highlighter on the untransformed test node");

  const node = await getNodeFront("#untransformed", inspector);
  await highlighterFront.show(node);

  const hidden = await highlighterTestFront.getHighlighterNodeAttribute(
    "css-transform-elements",
    "hidden",
    highlighterFront
  );
  ok(hidden, "The highlighter is still hidden");
}

async function isNotShownForInline(
  highlighterTestFront,
  inspector,
  highlighterFront
) {
  info("Asking to show the highlighter on the inline test node");

  const node = await getNodeFront("#inline", inspector);
  await highlighterFront.show(node);

  const hidden = await highlighterTestFront.getHighlighterNodeAttribute(
    "css-transform-elements",
    "hidden",
    highlighterFront
  );
  ok(hidden, "The highlighter is still hidden");
}

async function isVisibleWhenShown(
  highlighterTestFront,
  inspector,
  highlighterFront
) {
  info("Asking to show the highlighter on the test node");

  const node = await getNodeFront("#transformed", inspector);
  await highlighterFront.show(node);

  let hidden = await highlighterTestFront.getHighlighterNodeAttribute(
    "css-transform-elements",
    "hidden",
    highlighterFront
  );
  ok(!hidden, "The highlighter is visible");

  info("Hiding the highlighter");
  await highlighterFront.hide();

  hidden = await highlighterTestFront.getHighlighterNodeAttribute(
    "css-transform-elements",
    "hidden",
    highlighterFront
  );
  ok(hidden, "The highlighter is hidden");
}

async function linesLinkThePolygons(
  highlighterTestFront,
  inspector,
  highlighterFront
) {
  info("Showing the highlighter on the transformed node");

  const node = await getNodeFront("#transformed", inspector);
  await highlighterFront.show(node);

  info("Checking that the 4 lines do link the 2 shape's corners");

  const lines = [];
  for (const nb of ["1", "2", "3", "4"]) {
    const x1 = await highlighterTestFront.getHighlighterNodeAttribute(
      "css-transform-line" + nb,
      "x1",
      highlighterFront
    );
    const y1 = await highlighterTestFront.getHighlighterNodeAttribute(
      "css-transform-line" + nb,
      "y1",
      highlighterFront
    );
    const x2 = await highlighterTestFront.getHighlighterNodeAttribute(
      "css-transform-line" + nb,
      "x2",
      highlighterFront
    );
    const y2 = await highlighterTestFront.getHighlighterNodeAttribute(
      "css-transform-line" + nb,
      "y2",
      highlighterFront
    );
    lines.push({ x1, y1, x2, y2 });
  }

  let points1 = await highlighterTestFront.getHighlighterNodeAttribute(
    "css-transform-untransformed",
    "points",
    highlighterFront
  );
  points1 = points1.split(" ");

  let points2 = await highlighterTestFront.getHighlighterNodeAttribute(
    "css-transform-transformed",
    "points",
    highlighterFront
  );
  points2 = points2.split(" ");

  for (let i = 0; i < lines.length; i++) {
    info("Checking line nb " + i);
    const line = lines[i];

    const p1 = points1[i].split(",");
    is(
      p1[0],
      line.x1,
      "line " + i + "'s first point matches the untransformed x coordinate"
    );
    is(
      p1[1],
      line.y1,
      "line " + i + "'s first point matches the untransformed y coordinate"
    );

    const p2 = points2[i].split(",");
    is(
      p2[0],
      line.x2,
      "line " + i + "'s first point matches the transformed x coordinate"
    );
    is(
      p2[1],
      line.y2,
      "line " + i + "'s first point matches the transformed y coordinate"
    );
  }

  await highlighterFront.hide();
}
