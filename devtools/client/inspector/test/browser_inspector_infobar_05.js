/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Bug 1521188 - Indicate grid/flex container/item in infobar
// Check the text content of the highlighter nodeinfo bar.
const STRINGS_URI = "devtools/shared/locales/highlighters.properties";
const L10N = new LocalizationHelper(STRINGS_URI);

const TEST_URI = URL_ROOT + "doc_inspector_infobar_04.html";

const CLASS_GRID_TYPE = "box-model-infobar-grid-type";
const CLASS_FLEX_TYPE = "box-model-infobar-flex-type";

const FLEX_CONTAINER_TEXT = L10N.getStr("flexType.container");
const FLEX_ITEM_TEXT = L10N.getStr("flexType.item");
const FLEX_DUAL_TEXT = L10N.getStr("flexType.dual");
const GRID_CONTAINER_TEXT = L10N.getStr("gridType.container");
const GRID_ITEM_TEXT = L10N.getStr("gridType.item");
const GRID_DUAL_TEXT = L10N.getStr("gridType.dual");

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

add_task(async function() {
  const { inspector, testActor } = await openInspectorForURL(TEST_URI);

  for (const currentTest of TEST_DATA) {
    info("Testing " + currentTest.selector);
    await testTextContent(currentTest, inspector, testActor);
  }

  for (const currentTest of TEST_TEXT_DATA) {
    info("Testing " + currentTest.selector);
    await testTextNodeTextContent(currentTest, inspector, testActor);
  }
});

async function testTextContent(
  { selector, gridText, flexText },
  inspector,
  testActor
) {
  await selectAndHighlightNode(selector, inspector);

  const gridType = await testActor.getHighlighterNodeTextContent(
    CLASS_GRID_TYPE
  );
  const flexType = await testActor.getHighlighterNodeTextContent(
    CLASS_FLEX_TYPE
  );

  is(gridType, gridText, "node " + selector + ": grid type matches.");
  is(flexType, flexText, "node " + selector + ": flex type matches.");
}

async function testTextNodeTextContent(test, inspector, testActor) {
  const { walker } = inspector;
  const div = await walker.querySelector(walker.rootNode, test.selector);
  const { nodes } = await walker.children(div);
  test.selector = nodes[0];
  await testTextContent(test, inspector, testActor);
}
