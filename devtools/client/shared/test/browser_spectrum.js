/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the spectrum color picker works correctly

const {Spectrum} = require("devtools/client/shared/widgets/Spectrum");

const TEST_URI = CHROME_URL_ROOT + "doc_spectrum.html";

add_task(function* () {
  let [host,, doc] = yield createHost("bottom", TEST_URI);

  let container = doc.getElementById("spectrum-container");

  yield testCreateAndDestroyShouldAppendAndRemoveElements(container);
  yield testPassingAColorAtInitShouldSetThatColor(container);
  yield testSettingAndGettingANewColor(container);
  yield testChangingColorShouldEmitEvents(container);
  yield testSettingColorShoudUpdateTheUI(container);

  host.destroy();
});

function testCreateAndDestroyShouldAppendAndRemoveElements(container) {
  ok(container, "We have the root node to append spectrum to");
  is(container.childElementCount, 0, "Root node is empty");

  let s = new Spectrum(container, [255, 126, 255, 1]);
  s.show();
  ok(container.childElementCount > 0, "Spectrum has appended elements");

  s.destroy();
  is(container.childElementCount, 0, "Destroying spectrum removed all nodes");
}

function testPassingAColorAtInitShouldSetThatColor(container) {
  let initRgba = [255, 126, 255, 1];

  let s = new Spectrum(container, initRgba);
  s.show();

  let setRgba = s.rgb;

  is(initRgba[0], setRgba[0], "Spectrum initialized with the right color");
  is(initRgba[1], setRgba[1], "Spectrum initialized with the right color");
  is(initRgba[2], setRgba[2], "Spectrum initialized with the right color");
  is(initRgba[3], setRgba[3], "Spectrum initialized with the right color");

  s.destroy();
}

function testSettingAndGettingANewColor(container) {
  let s = new Spectrum(container, [0, 0, 0, 1]);
  s.show();

  let colorToSet = [255, 255, 255, 1];
  s.rgb = colorToSet;
  let newColor = s.rgb;

  is(colorToSet[0], newColor[0], "Spectrum set with the right color");
  is(colorToSet[1], newColor[1], "Spectrum set with the right color");
  is(colorToSet[2], newColor[2], "Spectrum set with the right color");
  is(colorToSet[3], newColor[3], "Spectrum set with the right color");

  s.destroy();
}

function testChangingColorShouldEmitEvents(container) {
  return new Promise(resolve => {
    let s = new Spectrum(container, [255, 255, 255, 1]);
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
  let s = new Spectrum(container, [255, 255, 255, 1]);
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
