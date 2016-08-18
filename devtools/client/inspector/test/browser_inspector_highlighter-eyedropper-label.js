/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test the position of the eyedropper label.
// It should move around when the eyedropper is close to the edges of the viewport so as
// to always stay visible.

const HIGHLIGHTER_TYPE = "EyeDropper";
const ID = "eye-dropper-";

const HTML = `
<style>
html, body {height: 100%; margin: 0;}
body {background: linear-gradient(red, gold); display: flex; justify-content: center;
      align-items: center;}
</style>
Eyedropper label position test
`;
const TEST_PAGE = "data:text/html;charset=utf-8," + encodeURI(HTML);

const TEST_DATA = [{
  desc: "Move the mouse to the center of the screen",
  getCoordinates: (width, height) => {
    return {x: width / 2, y: height / 2};
  },
  expectedPositions: {top: false, right: false, left: false}
}, {
  desc: "Move the mouse to the center left",
  getCoordinates: (width, height) => {
    return {x: 0, y: height / 2};
  },
  expectedPositions: {top: false, right: true, left: false}
}, {
  desc: "Move the mouse to the center right",
  getCoordinates: (width, height) => {
    return {x: width, y: height / 2};
  },
  expectedPositions: {top: false, right: false, left: true}
}, {
  desc: "Move the mouse to the bottom center",
  getCoordinates: (width, height) => {
    return {x: width / 2, y: height};
  },
  expectedPositions: {top: true, right: false, left: false}
}, {
  desc: "Move the mouse to the bottom left",
  getCoordinates: (width, height) => {
    return {x: 0, y: height};
  },
  expectedPositions: {top: true, right: true, left: false}
}, {
  desc: "Move the mouse to the bottom right",
  getCoordinates: (width, height) => {
    return {x: width, y: height};
  },
  expectedPositions: {top: true, right: false, left: true}
}, {
  desc: "Move the mouse to the top left",
  getCoordinates: (width, height) => {
    return {x: 0, y: 0};
  },
  expectedPositions: {top: false, right: true, left: false}
}, {
  desc: "Move the mouse to the top right",
  getCoordinates: (width, height) => {
    return {x: width, y: 0};
  },
  expectedPositions: {top: false, right: false, left: true}
}];

add_task(function* () {
  let {inspector, testActor} = yield openInspectorForURL(TEST_PAGE);
  let helper = yield getHighlighterHelperFor(HIGHLIGHTER_TYPE)({inspector, testActor});
  helper.prefix = ID;

  let {mouse, show, hide, finalize} = helper;
  let {width, height} = yield testActor.getBoundingClientRect("html");

  // This test fails in non-e10s windows if we use width and height. For some reasons, the
  // mouse events can't be dispatched/handled properly when we try to move the eyedropper
  // to the far right and/or bottom of the screen. So just removing 10px from each side
  // fixes it.
  width -= 10;
  height -= 10;

  info("Show the eyedropper on the page");
  yield show("html");

  info("Move the eyedropper around and check that the label appears at the right place");
  for (let {desc, getCoordinates, expectedPositions} of TEST_DATA) {
    info(desc);
    let {x, y} = getCoordinates(width, height);
    info(`Moving the mouse to ${x} ${y}`);
    yield mouse.move(x, y);
    yield checkLabelPositionAttributes(helper, expectedPositions);
  }

  info("Hide the eyedropper");
  yield hide();
  finalize();
});

function* checkLabelPositionAttributes(helper, positions) {
  for (let position in positions) {
    is((yield hasAttribute(helper, position)), positions[position],
       `The label was ${positions[position] ? "" : "not "}moved to the ${position}`);
  }
}

function* hasAttribute({getElementAttribute}, name) {
  let value = yield getElementAttribute("root", name);
  return value !== null;
}
