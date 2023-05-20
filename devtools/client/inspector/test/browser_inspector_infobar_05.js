/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Bug 1521188 - Indicate grid/flex container/item in infobar
// Check the text content of the highlighter nodeinfo bar.
const HighlightersBundle = new Localization(
  ["devtools/shared/highlighters.ftl"],
  true
);

const TEST_URI = URL_ROOT + "doc_inspector_infobar_04.html";

const CLASS_GRID_TYPE = "box-model-infobar-grid-type";
const CLASS_FLEX_TYPE = "box-model-infobar-flex-type";

const FLEX_CONTAINER_TEXT =
  HighlightersBundle.formatValueSync("flextype-container");
const FLEX_ITEM_TEXT = HighlightersBundle.formatValueSync("flextype-item");
const FLEX_DUAL_TEXT = HighlightersBundle.formatValueSync("flextype-dual");
const GRID_CONTAINER_TEXT =
  HighlightersBundle.formatValueSync("gridtype-container");
const GRID_ITEM_TEXT = HighlightersBundle.formatValueSync("gridtype-item");
const GRID_DUAL_TEXT = HighlightersBundle.formatValueSync("gridtype-dual");

const TEST_DATA = [
  {
    selector: "#flex-container",
    flexText: FLEX_CONTAINER_TEXT,
    gridText: "",
  },
  {
    selector: "#flex-item",
    flexText: FLEX_ITEM_TEXT,
    gridText: "",
  },
  {
    selector: "#flex-container-item",
    flexText: FLEX_DUAL_TEXT,
    gridText: "",
  },
  {
    selector: "#grid-container",
    flexText: "",
    gridText: GRID_CONTAINER_TEXT,
  },
  {
    selector: "#grid-item",
    flexText: "",
    gridText: GRID_ITEM_TEXT,
  },
  {
    selector: "#grid-container-item",
    flexText: "",
    gridText: GRID_DUAL_TEXT,
  },
  {
    selector: "#flex-item-grid-container",
    flexText: FLEX_ITEM_TEXT,
    gridText: GRID_CONTAINER_TEXT,
  },
];

const TEST_TEXT_DATA = [
  {
    selector: "#flex-text-container",
    flexText: FLEX_ITEM_TEXT,
    gridText: "",
  },
  {
    selector: "#grid-text-container",
    flexText: "",
    gridText: GRID_ITEM_TEXT,
  },
];

add_task(async function () {
  const { inspector, highlighterTestFront } = await openInspectorForURL(
    TEST_URI
  );

  for (const currentTest of TEST_DATA) {
    info("Testing " + currentTest.selector);
    await testTextContent(currentTest, inspector, highlighterTestFront);
  }

  for (const currentTest of TEST_TEXT_DATA) {
    info("Testing " + currentTest.selector);
    await testTextNodeTextContent(currentTest, inspector, highlighterTestFront);
  }
});

async function testTextContent(
  { selector, gridText, flexText },
  inspector,
  highlighterTestFront
) {
  await selectAndHighlightNode(selector, inspector);

  const gridType = await highlighterTestFront.getHighlighterNodeTextContent(
    CLASS_GRID_TYPE
  );
  const flexType = await highlighterTestFront.getHighlighterNodeTextContent(
    CLASS_FLEX_TYPE
  );

  is(gridType, gridText, "node " + selector + ": grid type matches.");
  is(flexType, flexText, "node " + selector + ": flex type matches.");
}

async function testTextNodeTextContent(test, inspector, highlighterTestFront) {
  const { walker } = inspector;
  const div = await walker.querySelector(walker.rootNode, test.selector);
  const { nodes } = await walker.children(div);
  test.selector = nodes[0];
  await testTextContent(test, inspector, highlighterTestFront);
}
