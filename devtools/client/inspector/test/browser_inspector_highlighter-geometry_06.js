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

add_task(function* () {
  let inspector = yield openInspectorForURL(TEST_URL);
  let helper = yield getHighlighterHelperFor(HIGHLIGHTER_TYPE)(inspector);

  helper.prefix = ID;

  let { show, hide, finalize } = helper;

  info("Showing the highlighter");
  yield show("#node2");

  for (let desc in TESTS) {
    yield executeTest(helper, desc, TESTS[desc]);
  }

  info("Hiding the highlighter");
  yield hide();
  yield finalize();
});

function* executeTest(helper, desc, data) {
  info(desc);

  ok((yield areElementAndHighlighterMovedCorrectly(
    helper, data.drag, data.by)), data.expects);
}

function* areElementAndHighlighterMovedCorrectly(helper, side, by) {
  let { mouse, reflow, highlightedNode } = helper;

  let {x, y} = yield getHandlerCoords(helper, side);

  let dx = x + by.x;
  let dy = y + by.y;

  let beforeDragStyle = yield highlightedNode.getComputedStyle();

  // simulate drag & drop
  yield mouse.down(x, y);
  yield mouse.move(dx, dy);
  yield mouse.up();

  yield reflow();

  info(`Checking ${side} handler is moved correctly`);
  yield isHandlerPositionUpdated(helper, side, x, y, by);

  let delta = (side === "left" || side === "right") ? by.x : by.y;
  delta = delta * ((side === "right" || side === "bottom") ? -1 : 1);

  info("Checking element's sides are correct after drag & drop");
  return yield areElementSideValuesCorrect(highlightedNode, beforeDragStyle,
                                           side, delta);
}

function* isHandlerPositionUpdated(helper, name, x, y, by) {
  let {x: afterDragX, y: afterDragY} = yield getHandlerCoords(helper, name);

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

function* areElementSideValuesCorrect(node, beforeDragStyle, name, delta) {
  let afterDragStyle = yield node.getComputedStyle();
  let isSideCorrect = true;

  for (let side of SIDES) {
    let afterValue = Math.round(parseFloat(afterDragStyle[side].value));
    let beforeValue = Math.round(parseFloat(beforeDragStyle[side].value));

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

function* getHandlerCoords({getElementAttribute}, side) {
  return {
    x: Math.round(yield getElementAttribute("handler-" + side, "cx")),
    y: Math.round(yield getElementAttribute("handler-" + side, "cy"))
  };
}
