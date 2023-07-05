/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the highlighter stays correctly positioned and has the right aspect
// ratio even when the page is zoomed in or out.

const TEST_URL = "data:text/html;charset=utf-8,<div>zoom me</div>";

// TEST_LEVELS entries should contain the zoom level to test.
const TEST_LEVELS = [2, 1, 0.5];

// Returns the expected style attribute value to check for on the highlighter's elements
// node, for the values given.
const expectedStyle = (w, h, z) =>
  (z !== 1
    ? `transform-origin: left top 0px; transform: scale(${1 / z}); `
    : "") +
  `position: absolute; width: ${w * z}px; height: ${h * z}px; ` +
  "overflow: hidden;";

add_task(async function () {
  const { inspector, highlighterTestFront } = await openInspectorForURL(
    TEST_URL
  );

  const div = await getNodeFront("div", inspector);

  for (const level of TEST_LEVELS) {
    info(`Zoom to level ${level}`);
    setContentPageZoomLevel(level);

    info("Highlight the test node");
    await inspector.highlighters.showHighlighterTypeForNode(
      inspector.highlighters.TYPES.BOXMODEL,
      div
    );

    const isVisible = await highlighterTestFront.isHighlighting();
    ok(isVisible, `The highlighter is visible at zoom level ${level}`);

    await isNodeCorrectlyHighlighted(highlighterTestFront, "div");

    info("Check that the highlighter root wrapper node was scaled down");

    const style = await getElementsNodeStyle(highlighterTestFront);

    const { width, height } = await SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [],
      () => {
        const { require } = ChromeUtils.importESModule(
          "resource://devtools/shared/loader/Loader.sys.mjs"
        );
        const {
          getWindowDimensions,
        } = require("resource://devtools/shared/layout/utils.js");
        return getWindowDimensions(content);
      }
    );

    is(
      style,
      expectedStyle(width, height, level),
      "The style attribute of the root element is correct"
    );

    info("Unhighlight the node");
    await inspector.highlighters.hideHighlighterType(
      inspector.highlighters.TYPES.BOXMODEL
    );
  }
});

async function getElementsNodeStyle(highlighterTestFront) {
  const value = await highlighterTestFront.getHighlighterNodeAttribute(
    "box-model-elements",
    "style"
  );
  return value;
}
