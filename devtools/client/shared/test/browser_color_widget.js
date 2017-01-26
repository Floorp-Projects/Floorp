/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the color widget works correctly.

const {ColorWidget} = require("devtools/client/shared/widgets/ColorWidget");

const TEST_URI = `data:text/html,
  <link rel="stylesheet" href="chrome://devtools/content/shared/widgets/color-widget.css" type="text/css"/>
  <div id="colorwidget-container" />`;

add_task(function* () {
  let [host,, doc] = yield createHost("bottom", TEST_URI);

  let container = doc.getElementById("colorwidget-container");

  yield testCreateAndDestroyShouldAppendAndRemoveElements(container);
  yield testPassingAColorAtInitShouldSetThatColor(container);
  yield testSettingAndGettingANewColor(container);
  yield testChangingColorShouldEmitEvents(container);
  yield testSettingColorShoudUpdateTheUI(container);

  host.destroy();
});

function testCreateAndDestroyShouldAppendAndRemoveElements(container) {
  ok(container, "We have the root node to append color widget to");
  is(container.childElementCount, 0, "Root node is empty");

  let cw = new ColorWidget(container, [255, 126, 255, 1]);
  cw.show();
  ok(container.childElementCount > 0, "Color widget has appended elements");

  cw.destroy();
  is(container.childElementCount, 0, "Destroying color widget removed all nodes");
}

function testPassingAColorAtInitShouldSetThatColor(container) {
  let initRgba = [255, 126, 255, 1];

  let cw = new ColorWidget(container, initRgba);
  cw.show();

  let setRgba = cw.rgb;

  is(initRgba[0], setRgba[0], "Color widget initialized with the right color");
  is(initRgba[1], setRgba[1], "Color widget initialized with the right color");
  is(initRgba[2], setRgba[2], "Color widget initialized with the right color");
  is(initRgba[3], setRgba[3], "Color widget initialized with the right color");

  cw.destroy();
}

function testSettingAndGettingANewColor(container) {
  let cw = new ColorWidget(container, [0, 0, 0, 1]);
  cw.show();

  let colorToSet = [255, 255, 255, 1];
  cw.rgb = colorToSet;
  let newColor = cw.rgb;

  is(colorToSet[0], newColor[0], "Color widget set with the right color");
  is(colorToSet[1], newColor[1], "Color widget set with the right color");
  is(colorToSet[2], newColor[2], "Color widget set with the right color");
  is(colorToSet[3], newColor[3], "Color widget set with the right color");

  cw.destroy();
}

function testChangingColorShouldEmitEvents(container) {
  return new Promise(resolve => {
    let cw = new ColorWidget(container, [255, 255, 255, 1]);
    cw.show();

    cw.once("changed", (event, rgba, color) => {
      ok(true, "Changed event was emitted on color change");
      is(rgba[0], 128, "New color is correct");
      is(rgba[1], 64, "New color is correct");
      is(rgba[2], 64, "New color is correct");
      is(rgba[3], 1, "New color is correct");
      is(`rgba(${rgba.join(", ")})`, color, "RGBA and css color correspond");

      cw.destroy();
      resolve();
    });

    // Simulate a drag move event by calling the handler directly.
    cw.onDraggerMove(cw.dragger.offsetWidth / 2, cw.dragger.offsetHeight / 2);
  });
}

function testSettingColorShoudUpdateTheUI(container) {
  let cw = new ColorWidget(container, [255, 255, 255, 1]);
  cw.show();
  let dragHelperOriginalPos = [cw.dragHelper.style.top, cw.dragHelper.style.left];
  let alphaHelperOriginalPos = cw.alphaSliderHelper.style.left;

  cw.rgb = [50, 240, 234, .2];
  cw.updateUI();

  ok(cw.alphaSliderHelper.style.left != alphaHelperOriginalPos, "Alpha helper has moved");
  ok(cw.dragHelper.style.top !== dragHelperOriginalPos[0], "Drag helper has moved");
  ok(cw.dragHelper.style.left !== dragHelperOriginalPos[1], "Drag helper has moved");

  cw.rgb = [240, 32, 124, 0];
  cw.updateUI();
  is(cw.alphaSliderHelper.style.left, -(cw.alphaSliderHelper.offsetWidth / 2) + "px",
    "Alpha range UI has been updated again");

  cw.destroy();
}
