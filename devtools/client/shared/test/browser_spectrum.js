/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the spectrum color picker works correctly

const {Spectrum} = require("devtools/client/shared/widgets/Spectrum");

const TEST_URI = CHROME_URL_ROOT + "doc_spectrum.html";

add_task(async function() {
  const [host,, doc] = await createHost("bottom", TEST_URI);

  const container = doc.getElementById("spectrum-container");

  await testCreateAndDestroyShouldAppendAndRemoveElements(container);
  await testPassingAColorAtInitShouldSetThatColor(container);
  await testSettingAndGettingANewColor(container);
  await testChangingColorShouldEmitEvents(container);
  await testSettingColorShoudUpdateTheUI(container);
  await testChangingColorShouldUpdateColorPreview(container);

  host.destroy();
});

/**
 * Helper method for extracting the rgba overlay value of the color preview's background
 * image style.
 *
 * @param   {String} linearGradientStr
 *          The linear gradient CSS string.
 * @return  {String} Returns the rgba string for the color overlay.
 */
function extractRgbaOverlayString(linearGradientStr) {
  const start = linearGradientStr.indexOf("(");
  const end = linearGradientStr.indexOf(")");

  return linearGradientStr.substring(start + 1, end + 1);
}

function testColorPreviewDisplay(spectrum, expectedRgbCssString, expectedBorderColor) {
  const { colorPreview } = spectrum;
  const colorPreviewStyle = window.getComputedStyle(colorPreview);
  expectedBorderColor = expectedBorderColor === "transparent" ?
                          "rgba(0, 0, 0, 0)"
                          :
                          expectedBorderColor;

  spectrum.updateUI();

  // Extract the first rgba value from the linear gradient
  const linearGradientStr = colorPreviewStyle.getPropertyValue("background-image");
  const colorPreviewValue = extractRgbaOverlayString(linearGradientStr);

  is(colorPreviewValue, expectedRgbCssString,
    `Color preview should be ${expectedRgbCssString}`);

  info("Test if color preview has a border or not.");
  // Since border-color is a shorthand CSS property, using getComputedStyle will return
  // an empty string. Instead, use one of the border sides to find the border-color value
  // since they will all be the same.
  const borderColorTop = colorPreviewStyle.getPropertyValue("border-top-color");
  is(borderColorTop, expectedBorderColor, "Color preview border color is correct.");
}

function testCreateAndDestroyShouldAppendAndRemoveElements(container) {
  ok(container, "We have the root node to append spectrum to");
  is(container.childElementCount, 0, "Root node is empty");

  const s = new Spectrum(container, [255, 126, 255, 1]);
  s.show();
  ok(container.childElementCount > 0, "Spectrum has appended elements");

  s.destroy();
  is(container.childElementCount, 0, "Destroying spectrum removed all nodes");
}

function testPassingAColorAtInitShouldSetThatColor(container) {
  const initRgba = [255, 126, 255, 1];

  const s = new Spectrum(container, initRgba);
  s.show();

  const setRgba = s.rgb;

  is(initRgba[0], setRgba[0], "Spectrum initialized with the right color");
  is(initRgba[1], setRgba[1], "Spectrum initialized with the right color");
  is(initRgba[2], setRgba[2], "Spectrum initialized with the right color");
  is(initRgba[3], setRgba[3], "Spectrum initialized with the right color");

  s.destroy();
}

function testSettingAndGettingANewColor(container) {
  const s = new Spectrum(container, [0, 0, 0, 1]);
  s.show();

  const colorToSet = [255, 255, 255, 1];
  s.rgb = colorToSet;
  const newColor = s.rgb;

  is(colorToSet[0], newColor[0], "Spectrum set with the right color");
  is(colorToSet[1], newColor[1], "Spectrum set with the right color");
  is(colorToSet[2], newColor[2], "Spectrum set with the right color");
  is(colorToSet[3], newColor[3], "Spectrum set with the right color");

  s.destroy();
}

function testChangingColorShouldEmitEvents(container) {
  return new Promise(resolve => {
    const s = new Spectrum(container, [255, 255, 255, 1]);
    s.show();

    s.once("changed", (rgba, color) => {
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
  const s = new Spectrum(container, [255, 255, 255, 1]);
  s.show();
  const dragHelperOriginalPos = [s.dragHelper.style.top, s.dragHelper.style.left];
  const alphaHelperOriginalPos = s.alphaSliderHelper.style.left;
  let hueHelperOriginalPos = s.hueSliderHelper.style.left;

  s.rgb = [50, 240, 234, .2];
  s.updateUI();

  ok(s.alphaSliderHelper.style.left != alphaHelperOriginalPos, "Alpha helper has moved");
  ok(s.dragHelper.style.top !== dragHelperOriginalPos[0], "Drag helper has moved");
  ok(s.dragHelper.style.left !== dragHelperOriginalPos[1], "Drag helper has moved");
  ok(s.hueSliderHelper.style.left !== hueHelperOriginalPos, "Hue helper has moved");

  hueHelperOriginalPos = s.hueSliderHelper.style.left;

  s.rgb = [240, 32, 124, 0];
  s.updateUI();
  is(s.alphaSliderHelper.style.left, -(s.alphaSliderHelper.offsetWidth / 2) + "px",
    "Alpha range UI has been updated again");
  ok(hueHelperOriginalPos !== s.hueSliderHelper.style.left,
    "Hue Helper slider should have move again");

  s.destroy();
}

function testChangingColorShouldUpdateColorPreview(container) {
  const s = new Spectrum(container, [0, 0, 1, 1]);
  s.show();

  info("Test that color preview is black.");
  testColorPreviewDisplay(s, "rgb(0, 0, 1)", "transparent");

  info("Test that color preview is blue.");
  s.rgb = [0, 0, 255, 1];
  testColorPreviewDisplay(s, "rgb(0, 0, 255)", "transparent");

  info("Test that color preview is red.");
  s.rgb = [255, 0, 0, 1];
  testColorPreviewDisplay(s, "rgb(255, 0, 0)", "transparent");

  info("Test that color preview is white and also has a light grey border.");
  s.rgb = [255, 255, 255, 1];
  testColorPreviewDisplay(s, "rgb(255, 255, 255)", "rgb(204, 204, 204)");

  s.destroy();
}
