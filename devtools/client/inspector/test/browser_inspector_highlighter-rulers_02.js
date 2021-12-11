/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test the creation of the geometry highlighter elements.

const TEST_URL =
  "data:text/html;charset=utf-8," +
  "<div style='position:absolute;left: 0; top: 0; " +
  "width: 40000px; height: 8000px'></div>";

const ID = "rulers-highlighter-";

add_task(async function() {
  const { inspector, highlighterTestFront } = await openInspectorForURL(
    TEST_URL
  );
  const front = inspector.inspectorFront;

  const highlighter = await front.getHighlighterByType("RulersHighlighter");

  // the rulers doesn't need any node, but as highligher it seems mandatory
  // ones, so the body is given
  const body = await getNodeFront("body", inspector);
  await highlighter.show(body);

  await isUpdatedAfterScroll(highlighter, inspector, highlighterTestFront);

  await highlighter.finalize();
});

async function isUpdatedAfterScroll(
  highlighterFront,
  inspector,
  highlighterTestFront
) {
  info("Check the rulers' position by default");

  let xAxisRulerTransform = await highlighterTestFront.getHighlighterNodeAttribute(
    `${ID}x-axis-ruler`,
    "transform",
    highlighterFront
  );
  let xAxisTextTransform = await highlighterTestFront.getHighlighterNodeAttribute(
    `${ID}x-axis-text`,
    "transform",
    highlighterFront
  );
  let yAxisRulerTransform = await highlighterTestFront.getHighlighterNodeAttribute(
    `${ID}y-axis-ruler`,
    "transform",
    highlighterFront
  );
  let yAxisTextTransform = await highlighterTestFront.getHighlighterNodeAttribute(
    `${ID}y-axis-text`,
    "transform",
    highlighterFront
  );

  is(xAxisRulerTransform, null, "x axis ruler is positioned properly");
  is(xAxisTextTransform, null, "x axis text are positioned properly");
  is(yAxisRulerTransform, null, "y axis ruler is positioned properly");
  is(yAxisTextTransform, null, "y axis text are positioned properly");

  info("Ask the content window to scroll to specific coords");

  const x = 200,
    y = 300;

  let data = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [x, y],
    (_x, _y) => {
      return new Promise(resolve => {
        content.addEventListener(
          "scroll",
          () => resolve({ x: content.scrollX, y: content.scrollY }),
          { once: true }
        );
        content.scrollTo(_x, _y);
      });
    }
  );

  is(data.x, x, "window scrolled properly horizontally");
  is(data.y, y, "window scrolled properly vertically");

  info("Check the rulers are properly positioned after the scrolling");

  xAxisRulerTransform = await highlighterTestFront.getHighlighterNodeAttribute(
    `${ID}x-axis-ruler`,
    "transform",
    highlighterFront
  );
  xAxisTextTransform = await highlighterTestFront.getHighlighterNodeAttribute(
    `${ID}x-axis-text`,
    "transform",
    highlighterFront
  );
  yAxisRulerTransform = await highlighterTestFront.getHighlighterNodeAttribute(
    `${ID}y-axis-ruler`,
    "transform",
    highlighterFront
  );
  yAxisTextTransform = await highlighterTestFront.getHighlighterNodeAttribute(
    `${ID}y-axis-text`,
    "transform",
    highlighterFront
  );

  is(
    xAxisRulerTransform,
    `translate(-${x})`,
    "x axis ruler is positioned properly"
  );
  is(
    xAxisTextTransform,
    `translate(-${x})`,
    "x axis text are positioned properly"
  );
  is(
    yAxisRulerTransform,
    `translate(0, -${y})`,
    "y axis ruler is positioned properly"
  );
  is(
    yAxisTextTransform,
    `translate(0, -${y})`,
    "y axis text are positioned properly"
  );

  info("Ask the content window to scroll relative to the current position");

  data = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [-50, -60],
    (_deltaX, _deltaY) => {
      return new Promise(resolve => {
        content.addEventListener(
          "scroll",
          () => resolve({ x: content.scrollX, y: content.scrollY }),
          { once: true }
        );
        content.scrollBy(_deltaX, _deltaY);
      });
    }
  );

  is(data.x, x - 50, "window scrolled properly horizontally");
  is(data.y, y - 60, "window scrolled properly vertically");

  info("Check the rulers are properly positioned after the relative scrolling");

  xAxisRulerTransform = await highlighterTestFront.getHighlighterNodeAttribute(
    `${ID}x-axis-ruler`,
    "transform",
    highlighterFront
  );
  xAxisTextTransform = await highlighterTestFront.getHighlighterNodeAttribute(
    `${ID}x-axis-text`,
    "transform",
    highlighterFront
  );
  yAxisRulerTransform = await highlighterTestFront.getHighlighterNodeAttribute(
    `${ID}y-axis-ruler`,
    "transform",
    highlighterFront
  );
  yAxisTextTransform = await highlighterTestFront.getHighlighterNodeAttribute(
    `${ID}y-axis-text`,
    "transform",
    highlighterFront
  );

  is(
    xAxisRulerTransform,
    `translate(-${x - 50})`,
    "x axis ruler is positioned properly"
  );
  is(
    xAxisTextTransform,
    `translate(-${x - 50})`,
    "x axis text are positioned properly"
  );
  is(
    yAxisRulerTransform,
    `translate(0, -${y - 60})`,
    "y axis ruler is positioned properly"
  );
  is(
    yAxisTextTransform,
    `translate(0, -${y - 60})`,
    "y axis text are positioned properly"
  );
}
