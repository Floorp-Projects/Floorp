/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Make sure that the inset() highlighter displays correctly when using pixels
// on top of screen %.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_cssshapes-percent.html";
const HIGHLIGHTER_TYPE = "ShapesHighlighter";

add_task(async function () {
  const { inspector, highlighterTestFront } = await openInspectorForURL(
    TEST_URL
  );
  const front = inspector.inspectorFront;
  const highlighter = await front.getHighlighterByType(HIGHLIGHTER_TYPE);

  await insetHasCorrectAttrs(highlighterTestFront, inspector, highlighter);

  await highlighter.finalize();
});

async function insetHasCorrectAttrs(
  highlighterTestFront,
  inspector,
  highlighterFront
) {
  info("Testing inset() editor using pixels on page %");

  const top = 10;
  const right = 20;
  const bottom = 30;
  const left = 40;

  // Set the clip-path property
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [top, right, bottom, left],
    (t, r, b, l) => {
      content.document.querySelector(
        "#inset"
      ).style.clipPath = `inset(${t}px ${r}px ${b}px ${l}px)`;
    }
  );

  // Get width and height of page
  const { innerWidth, innerHeight } = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => {
      return {
        innerWidth: content.innerWidth,
        innerHeight: content.innerHeight,
      };
    }
  );

  const insetNode = await getNodeFront("#inset", inspector);
  await highlighterFront.show(insetNode, { mode: "cssClipPath" });

  const x = parseFloat(
    await highlighterTestFront.getHighlighterNodeAttribute(
      "shapes-rect",
      "x",
      highlighterFront
    )
  );
  const y = parseFloat(
    await highlighterTestFront.getHighlighterNodeAttribute(
      "shapes-rect",
      "y",
      highlighterFront
    )
  );
  const width = parseFloat(
    await highlighterTestFront.getHighlighterNodeAttribute(
      "shapes-rect",
      "width",
      highlighterFront
    )
  );
  const height = parseFloat(
    await highlighterTestFront.getHighlighterNodeAttribute(
      "shapes-rect",
      "height",
      highlighterFront
    )
  );

  // Convert pixels to screen percentage
  const expectedX = (left / innerWidth) * 100;
  const expectedY = (top / innerHeight) * 100;
  const expectedWidth = ((innerWidth - (left + right)) / innerWidth) * 100;
  const expectedHeight = ((innerHeight - (top + bottom)) / innerHeight) * 100;

  ok(
    floatEq(x, expectedX),
    `Rect highlighter has correct x (got ${x}, expected ${expectedX})`
  );
  ok(
    floatEq(y, expectedY),
    `Rect highlighter has correct y (got ${y}, expected ${expectedY})`
  );
  ok(
    floatEq(width, expectedWidth),
    `Rect highlighter has correct width (got ${width}, expected ${expectedWidth})`
  );
  ok(
    floatEq(height, expectedHeight),
    `Rect highlighter has correct height (got ${height}, expected ${expectedHeight})`
  );
}

/**
 * Compare two floats with a tolerance of 0.1
 */
function floatEq(f1, f2) {
  return Math.abs(f1 - f2) < 0.1;
}
