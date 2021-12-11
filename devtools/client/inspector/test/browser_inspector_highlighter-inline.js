/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

requestLongerTimeout(2);

// Test that highlighting various inline boxes displays the right number of
// polygons in the page.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_inline.html";
const TEST_DATA = [
  "body",
  "h1",
  "h2",
  "h2 em",
  "p",
  "p span",
  // The following test case used to fail. See bug 1139925.
  "[dir=rtl] > span",
];

add_task(async function() {
  info("Loading the test document and opening the inspector");
  const { inspector, highlighterTestFront } = await openInspectorForURL(
    TEST_URL
  );

  for (const selector of TEST_DATA) {
    info("Selecting and highlighting node " + selector);
    await selectAndHighlightNode(selector, inspector);

    info("Get all quads for this node");
    const data = await getAllAdjustedQuadsForContentPageElement(selector);

    info(
      "Iterate over the box-model regions and verify that the highlighter " +
        "is correct"
    );
    for (const region of ["margin", "border", "padding", "content"]) {
      const { points } = await highlighterTestFront.getHighlighterRegionPath(
        region
      );
      is(
        points.length,
        data[region].length,
        "The highlighter's " +
          region +
          " path defines the correct number of boxes"
      );
    }

    info(
      "Verify that the guides define a rectangle that contains all " +
        "content boxes"
    );

    const expectedContentRect = {
      p1: { x: Infinity, y: Infinity },
      p2: { x: -Infinity, y: Infinity },
      p3: { x: -Infinity, y: -Infinity },
      p4: { x: Infinity, y: -Infinity },
    };
    for (const { p1, p2, p3, p4 } of data.content) {
      expectedContentRect.p1.x = Math.min(expectedContentRect.p1.x, p1.x);
      expectedContentRect.p1.y = Math.min(expectedContentRect.p1.y, p1.y);
      expectedContentRect.p2.x = Math.max(expectedContentRect.p2.x, p2.x);
      expectedContentRect.p2.y = Math.min(expectedContentRect.p2.y, p2.y);
      expectedContentRect.p3.x = Math.max(expectedContentRect.p3.x, p3.x);
      expectedContentRect.p3.y = Math.max(expectedContentRect.p3.y, p3.y);
      expectedContentRect.p4.x = Math.min(expectedContentRect.p4.x, p4.x);
      expectedContentRect.p4.y = Math.max(expectedContentRect.p4.y, p4.y);
    }

    const contentRect = await highlighterTestFront.getGuidesRectangle();

    for (const point of ["p1", "p2", "p3", "p4"]) {
      is(
        Math.round(contentRect[point].x),
        Math.round(expectedContentRect[point].x),
        "x coordinate of point " +
          point +
          " of the content rectangle defined by the outer guides is correct"
      );
      is(
        Math.round(contentRect[point].y),
        Math.round(expectedContentRect[point].y),
        "y coordinate of point " +
          point +
          " of the content rectangle defined by the outer guides is correct"
      );
    }
  }
});
