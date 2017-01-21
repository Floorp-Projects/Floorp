/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the color widget color picker works correctly.

const {ColorWidget} = require("devtools/client/shared/widgets/ColorWidget.js");

const TEST_URI = `data:text/html,
  <link rel="stylesheet" href="chrome://devtools/content/shared/widgets/color-widget.css" type="text/css"/>
  <div id="color-widget-container" />`;

add_task(function* () {
  let [host,, doc] = yield createHost("bottom", TEST_URI);

  let container = doc.getElementById("color-widget-container");

  yield testCreateAndDestroyShouldAppendAndRemoveElements(container);
  yield testPassingAColorAtInitShouldSetThatColor(container);
  yield testSettingAndGettingANewColor(container);
  yield testChangingColorShouldEmitEvents(container);
  yield testSettingColorShoudUpdateTheUI(container);
  yield testChangingColorShouldUpdateColorNameDisplay(container);

  host.destroy();
});

function testCreateAndDestroyShouldAppendAndRemoveElements(container) {
  ok(container, "We have the root node to append ColorWidget to");
  is(container.childElementCount, 0, "Root node is empty");

  let s = new ColorWidget(container, [255, 126, 255, 1]);
  s.show();
  ok(container.childElementCount > 0, "ColorWidget has appended elements");

  s.destroy();
  is(container.childElementCount, 0, "Destroying ColorWidget removed all nodes");
}

function testPassingAColorAtInitShouldSetThatColor(container) {
  let initRgba = [255, 126, 255, 1];

  let s = new ColorWidget(container, initRgba);
  s.show();

  let setRgba = s.rgb;

  is(initRgba[0], setRgba[0], "ColorWidget initialized with the right color");
  is(initRgba[1], setRgba[1], "ColorWidget initialized with the right color");
  is(initRgba[2], setRgba[2], "ColorWidget initialized with the right color");
  is(initRgba[3], setRgba[3], "ColorWidget initialized with the right color");

  s.destroy();
}

function testSettingAndGettingANewColor(container) {
  let s = new ColorWidget(container, [0, 0, 0, 1]);
  s.show();

  let colorToSet = [255, 255, 255, 1];
  s.rgb = colorToSet;
  let newColor = s.rgb;

  is(colorToSet[0], newColor[0], "ColorWidget set with the right color");
  is(colorToSet[1], newColor[1], "ColorWidget set with the right color");
  is(colorToSet[2], newColor[2], "ColorWidget set with the right color");
  is(colorToSet[3], newColor[3], "ColorWidget set with the right color");

  s.destroy();
}

function testChangingColorShouldEmitEvents(container) {
  return new Promise(resolve => {
    let s = new ColorWidget(container, [255, 255, 255, 1]);
    s.show();

    s.once("changed", (event, rgba, color) => {
      ok(true, "Changed event was emitted on color change");
      is(rgba[0], 128, "New color is correct");
      is(rgba[1], 64, "New color is correct");
      is(rgba[2], 64, "New color is correct");
      is(rgba[3], 1, "New color is correct");
      is(`rgba(${rgba.join(", ")})`, color, "RGBA and css color correspond");

      s.destroy();
      resolve();
    });

    // Simulate a drag move event by calling the handler directly.
    s.onDraggerMove(s.dragger.offsetWidth / 2, s.dragger.offsetHeight / 2);
  });
}

function testSettingColorShoudUpdateTheUI(container) {
  let s = new ColorWidget(container, [255, 255, 255, 1]);
  s.show();
  let dragHelperOriginalPos = [s.dragHelper.style.top, s.dragHelper.style.left];
  let alphaHelperOriginalPos = s.alphaSliderHelper.style.left;

  s.rgb = [50, 240, 234, .2];
  s.updateUI();

  ok(s.alphaSliderHelper.style.left != alphaHelperOriginalPos, "Alpha helper has moved");
  ok(s.dragHelper.style.top !== dragHelperOriginalPos[0], "Drag helper has moved");
  ok(s.dragHelper.style.left !== dragHelperOriginalPos[1], "Drag helper has moved");

  s.rgb = [240, 32, 124, 0];
  s.updateUI();
  is(s.alphaSliderHelper.style.left, -(s.alphaSliderHelper.offsetWidth / 2) + "px",
    "Alpha range UI has been updated again");

  s.destroy();
}

function testChangingColorShouldUpdateColorNameDisplay(container) {
  let s = new ColorWidget(container, [255, 255, 255, 1]);
  s.show();
  s.updateUI();
  is(s.colorName.textContent, "white", "Color Name should be white");

  s.rgb = [0, 0, 0, 1];
  s.updateUI();
  is(s.colorName.textContent, "black", "Color Name should be black");

  s.rgb = [0, 0, 1, 1];
  s.updateUI();
  is(s.colorName.textContent, "---", "Color Name should be ---");

  s.destroy();
}
