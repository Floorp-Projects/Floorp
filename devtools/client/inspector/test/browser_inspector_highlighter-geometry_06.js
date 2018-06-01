/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the geometry editor resizes properly an element on all sides,
// with different unit measures, and that arrow/handlers are updated correctly.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter-geometry_01.html";
const ID = "geometry-editor-";
const HIGHLIGHTER_TYPE = "GeometryEditorHighlighter";

const SIDES = ["top", "right", "bottom", "left"];

// The object below contains all the tests for this unit test.
// The property's name is the test's description, that points to an
// object contains the steps (what side of the geometry editor to drag,
// the amount of pixels) and the expectation.
const TESTS = {
  "Drag top's handler along x and y, south-east direction": {
    "expects": "Only y axis is used to updated the top's element value",
    "drag": "top",
    "by": {x: 10, y: 10}
  },
  "Drag right's handler along x and y, south-east direction": {
    "expects": "Only x axis is used to updated the right's element value",
    "drag": "right",
    "by": {x: 10, y: 10}
  },
  "Drag bottom's handler along x and y, south-east direction": {
    "expects": "Only y axis is used to updated the bottom's element value",
    "drag": "bottom",
    "by": {x: 10, y: 10}
  },
  "Drag left's handler along x and y, south-east direction": {
    "expects": "Only y axis is used to updated the left's element value",
    "drag": "left",
    "by": {x: 10, y: 10}
  },
  "Drag top's handler along x and y, north-west direction": {
    "expects": "Only y axis is used to updated the top's element value",
    "drag": "top",
    "by": {x: -20, y: -20}
  },
  "Drag right's handler along x and y, north-west direction": {
    "expects": "Only x axis is used to updated the right's element value",
    "drag": "right",
    "by": {x: -20, y: -20}
  },
  "Drag bottom's handler along x and y, north-west direction": {
    "expects": "Only y axis is used to updated the bottom's element value",
    "drag": "bottom",
    "by": {x: -20, y: -20}
  },
  "Drag left's handler along x and y, north-west direction": {
    "expects": "Only y axis is used to updated the left's element value",
    "drag": "left",
    "by": {x: -20, y: -20}
  }
};

add_task(async function() {
  const inspector = await openInspectorForURL(TEST_URL);
  const helper = await getHighlighterHelperFor(HIGHLIGHTER_TYPE)(inspector);

  helper.prefix = ID;

  const { show, hide, finalize } = helper;

  info("Showing the highlighter");
  await show("#node2");

  for (const desc in TESTS) {
    await executeTest(helper, desc, TESTS[desc]);
  }

  info("Hiding the highlighter");
  await hide();
  await finalize();
});

async function executeTest(helper, desc, data) {
  info(desc);

  ok((await areElementAndHighlighterMovedCorrectly(
    helper, data.drag, data.by)), data.expects);
}

async function areElementAndHighlighterMovedCorrectly(helper, side, by) {
  const { mouse, reflow, highlightedNode } = helper;

  const {x, y} = await getHandlerCoords(helper, side);

  const dx = x + by.x;
  const dy = y + by.y;

  const beforeDragStyle = await highlightedNode.getComputedStyle();

  // simulate drag & drop
  await mouse.down(x, y);
  await mouse.move(dx, dy);
  await mouse.up();

  await reflow();

  info(`Checking ${side} handler is moved correctly`);
  await isHandlerPositionUpdated(helper, side, x, y, by);

  let delta = (side === "left" || side === "right") ? by.x : by.y;
  delta = delta * ((side === "right" || side === "bottom") ? -1 : 1);

  info("Checking element's sides are correct after drag & drop");
  return areElementSideValuesCorrect(highlightedNode, beforeDragStyle,
                                     side, delta);
}

async function isHandlerPositionUpdated(helper, name, x, y, by) {
  const {x: afterDragX, y: afterDragY} = await getHandlerCoords(helper, name);

  if (name === "left" || name === "right") {
    is(afterDragX, x + by.x,
      `${name} handler's x axis updated.`);
    is(afterDragY, y,
      `${name} handler's y axis unchanged.`);
  } else {
    is(afterDragX, x,
      `${name} handler's x axis unchanged.`);
    is(afterDragY, y + by.y,
      `${name} handler's y axis updated.`);
  }
}

async function areElementSideValuesCorrect(node, beforeDragStyle, name, delta) {
  const afterDragStyle = await node.getComputedStyle();
  let isSideCorrect = true;

  for (const side of SIDES) {
    const afterValue = Math.round(parseFloat(afterDragStyle[side].value));
    const beforeValue = Math.round(parseFloat(beforeDragStyle[side].value));

    if (side === name) {
      // `isSideCorrect` is used only as test's return value, not to perform
      // the actual test, because with `is` instead of `ok` we gather more
      // information in case of failure
      isSideCorrect = isSideCorrect && (afterValue === beforeValue + delta);

      is(afterValue, beforeValue + delta,
        `${side} is updated.`);
    } else {
      isSideCorrect = isSideCorrect && (afterValue === beforeValue);

      is(afterValue, beforeValue,
        `${side} is unchaged.`);
    }
  }

  return isSideCorrect;
}

async function getHandlerCoords({getElementAttribute}, side) {
  return {
    x: Math.round(await getElementAttribute("handler-" + side, "cx")),
    y: Math.round(await getElementAttribute("handler-" + side, "cy"))
  };
}
