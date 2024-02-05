/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the original position of a relative positioned node is correct
// even when zooming.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter-geometry_01.html";
const ID = "geometry-editor-";
const HIGHLIGHTER_TYPE = "GeometryEditorHighlighter";

const TESTS = {
  ".absolute-all-4": {
    desc: "Check absolute positioned element's parentOffset highlight",
    sides: ["top", "right", "bottom", "left"],
  },
  ".relative": {
    desc: "Check relative positioned element original offset highlight",
    sides: ["top", "left"],
  },
};

add_task(async function () {
  const inspector = await openInspectorForURL(TEST_URL);
  const helper = await getHighlighterHelperFor(HIGHLIGHTER_TYPE)(inspector);

  helper.prefix = ID;

  const { finalize } = helper;

  for (const selector in TESTS) {
    await executeTest(inspector, helper, selector, TESTS[selector]);
  }

  await finalize();
});

async function executeTest(inspector, helper, selector, data) {
  const { show, hide } = helper;

  info("Showing the highlighter");
  await show(selector);
  info(data.desc);

  await checkOffsetParentSame(helper, selector, data.sides);

  setContentPageZoomLevel(1.2);
  await reflowContentPage();

  await checkOffsetParentSame(helper, selector, data.sides);

  info("Hiding the highlighter and resetting zoom");
  setContentPageZoomLevel(1);
  await hide();
}

// Check that the offsetParent element does not move around,
// when the target element itself is being moved.
async function checkOffsetParentSame({ getElementAttribute }, selector, sides) {
  const expectedOffsetParent = splitPointsIntoCoordinates(
    await getElementAttribute("offset-parent", "points")
  );
  const expectedArrowStartPos = await checkArrowStartingPos(
    getElementAttribute,
    expectedOffsetParent,
    sides
  );

  // Setting both to 0px would disable the offsetParent highlighter and not update points.
  await setContentPageElementAttribute(selector, "style", "top:1px; left:1px;");
  await reflowContentPage();

  const actualOffsetParent = splitPointsIntoCoordinates(
    await getElementAttribute("offset-parent", "points")
  );
  const actualArrowStartPos = await checkArrowStartingPos(
    getElementAttribute,
    actualOffsetParent,
    sides
  );

  await removeContentPageElementAttribute(selector, "style");

  for (let i = 0; i < 4; i++) {
    for (let j = 0; j < 2; j++) {
      is(
        actualOffsetParent[i][j],
        expectedOffsetParent[i][j],
        `Coordinate ${j + 1}/2 of point ${
          i + 1
        }/4 is the same after repositioning.`
      );
    }
  }

  for (let i = 0; i < expectedArrowStartPos.length; i++) {
    is(
      actualArrowStartPos[i],
      expectedArrowStartPos[i],
      `Start position of arrow-${sides[i]} is the same after repositioning.`
    );
  }
}

// Check that the arrow starts at the boundary of the offsetParent.
// This also returns the start coordinate along the relevant axis.
async function checkArrowStartingPos(
  getElementAttribute,
  offsetParentPoints,
  sides
) {
  // offsetParentPoints is a number[][].
  offsetParentPoints = offsetParentPoints.flat();
  const result = [];

  for (const side of sides) {
    // The only attribute that must not change is the start position of the arrow.
    // Cross-axis and end position could change when the target element is moved.
    const axis = side === "top" || side === "bottom" ? "y" : "x";
    // We have to round because floating point arithmetics are not consistent.
    // (They are consistent enough, they just vary in their 10^(-5))
    const arrowPos = Math.round(
      parseFloat(await getElementAttribute("arrow-" + side, axis + "1"))
    );
    ok(
      offsetParentPoints.includes(arrowPos),
      `arrow-${side} starts at offset parent`
    );
    result.push(arrowPos);
  }

  return result;
}

// Splits the value of the points attribute into coordinates.
function splitPointsIntoCoordinates(points) {
  const result = [];
  for (const coord of points.split(" ")) {
    // We have to round (see above).
    result.push(coord.split(",").map(s => Math.round(parseFloat(s))));
  }

  return result;
}
