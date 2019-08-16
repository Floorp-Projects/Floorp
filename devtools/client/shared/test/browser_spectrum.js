/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the spectrum color picker works correctly

const { Spectrum } = require("devtools/client/shared/widgets/Spectrum");
const {
  accessibility: {
    SCORES: { FAIL, AAA, AA },
  },
} = require("devtools/shared/constants");

loader.lazyRequireGetter(
  this,
  "cssColors",
  "devtools/shared/css/color-db",
  true
);

const TEST_URI = CHROME_URL_ROOT + "doc_spectrum.html";
const REGULAR_TEXT_PROPS = {
  "font-size": { value: "11px" },
  "font-weight": { value: "bold" },
  opacity: { value: "1" },
};
const SINGLE_BG_COLOR = {
  value: cssColors.white,
};
const ZERO_ALPHA_COLOR = [0, 255, 255, 0];

add_task(async function() {
  const [host, , doc] = await createHost("bottom", TEST_URI);

  const container = doc.getElementById("spectrum-container");

  await testCreateAndDestroyShouldAppendAndRemoveElements(container);
  await testPassingAColorAtInitShouldSetThatColor(container);
  await testSettingAndGettingANewColor(container);
  await testChangingColorShouldEmitEvents(container, doc);
  await testSettingColorShoudUpdateTheUI(container);
  await testChangingColorShouldUpdateColorPreview(container);
  await testNotSettingTextPropsShouldNotShowContrastSection(container);
  await testSettingTextPropsAndColorShouldUpdateContrastValue(container);
  await testOnlySelectingLargeTextWithNonZeroAlphaShouldShowIndicator(
    container
  );
  await testSettingMultiColoredBackgroundShouldShowContrastRange(container);

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

function testColorPreviewDisplay(
  spectrum,
  expectedRgbCssString,
  expectedBorderColor
) {
  const { colorPreview } = spectrum;
  const colorPreviewStyle = window.getComputedStyle(colorPreview);
  expectedBorderColor =
    expectedBorderColor === "transparent"
      ? "rgba(0, 0, 0, 0)"
      : expectedBorderColor;

  spectrum.updateUI();

  // Extract the first rgba value from the linear gradient
  const linearGradientStr = colorPreviewStyle.getPropertyValue(
    "background-image"
  );
  const colorPreviewValue = extractRgbaOverlayString(linearGradientStr);

  is(
    colorPreviewValue,
    expectedRgbCssString,
    `Color preview should be ${expectedRgbCssString}`
  );

  info("Test if color preview has a border or not.");
  // Since border-color is a shorthand CSS property, using getComputedStyle will return
  // an empty string. Instead, use one of the border sides to find the border-color value
  // since they will all be the same.
  const borderColorTop = colorPreviewStyle.getPropertyValue("border-top-color");
  is(
    borderColorTop,
    expectedBorderColor,
    "Color preview border color is correct."
  );
}

function testCreateAndDestroyShouldAppendAndRemoveElements(container) {
  ok(container, "We have the root node to append spectrum to");
  is(container.childElementCount, 0, "Root node is empty");

  const s = new Spectrum(container, cssColors.white);
  s.show();
  ok(container.childElementCount > 0, "Spectrum has appended elements");

  s.destroy();
  is(container.childElementCount, 0, "Destroying spectrum removed all nodes");
}

function testPassingAColorAtInitShouldSetThatColor(container) {
  const initRgba = cssColors.white;

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
  const s = new Spectrum(container, cssColors.black);
  s.show();

  const colorToSet = cssColors.white;
  s.rgb = colorToSet;
  const newColor = s.rgb;

  is(colorToSet[0], newColor[0], "Spectrum set with the right color");
  is(colorToSet[1], newColor[1], "Spectrum set with the right color");
  is(colorToSet[2], newColor[2], "Spectrum set with the right color");
  is(colorToSet[3], newColor[3], "Spectrum set with the right color");

  s.destroy();
}

async function testChangingColorShouldEmitEventsHelper(
  spectrum,
  moveFn,
  expectedColor
) {
  const onChanged = spectrum.once("changed", (rgba, color) => {
    is(rgba[0], expectedColor[0], "New color is correct");
    is(rgba[1], expectedColor[1], "New color is correct");
    is(rgba[2], expectedColor[2], "New color is correct");
    is(rgba[3], expectedColor[3], "New color is correct");
    is(`rgba(${rgba.join(", ")})`, color, "RGBA and css color correspond");
  });

  moveFn();
  await onChanged;
  ok(true, "Changed event was emitted on color change");
}

function testChangingColorShouldEmitEvents(container, doc) {
  const s = new Spectrum(container, cssColors.white);
  s.show();

  const sendUpKey = () => EventUtils.sendKey("Up");
  const sendDownKey = () => EventUtils.sendKey("Down");
  const sendLeftKey = () => EventUtils.sendKey("Left");
  const sendRightKey = () => EventUtils.sendKey("Right");

  info(
    "Test that simulating a mouse drag move event emits color changed event"
  );
  const draggerMoveFn = () =>
    s.onDraggerMove(s.dragger.offsetWidth / 2, s.dragger.offsetHeight / 2);
  testChangingColorShouldEmitEventsHelper(s, draggerMoveFn, [128, 64, 64, 1]);

  info(
    "Test that moving the dragger with arrow keys emits color changed event."
  );
  // Focus on the spectrum dragger when spectrum is shown
  s.dragger.focus();
  is(
    doc.activeElement.className,
    "spectrum-color spectrum-box",
    "Spectrum dragger has successfully received focus."
  );
  testChangingColorShouldEmitEventsHelper(s, sendDownKey, [125, 62, 62, 1]);
  testChangingColorShouldEmitEventsHelper(s, sendLeftKey, [125, 63, 63, 1]);
  testChangingColorShouldEmitEventsHelper(s, sendUpKey, [128, 64, 64, 1]);
  testChangingColorShouldEmitEventsHelper(s, sendRightKey, [128, 63, 63, 1]);

  info(
    "Test that moving the hue slider with arrow keys emits color changed event."
  );
  // Tab twice to focus on hue slider
  EventUtils.sendKey("Tab");
  is(
    doc.activeElement.className,
    "devtools-button",
    "Eyedropper has focus now."
  );
  EventUtils.sendKey("Tab");
  is(
    doc.activeElement.className,
    "spectrum-hue-input",
    "Hue slider has successfully received focus."
  );
  testChangingColorShouldEmitEventsHelper(s, sendRightKey, [128, 66, 63, 1]);
  testChangingColorShouldEmitEventsHelper(s, sendLeftKey, [128, 63, 63, 1]);

  info(
    "Test that moving the hue slider with arrow keys emits color changed event."
  );
  // Tab to focus on alpha slider
  EventUtils.sendKey("Tab");
  is(
    doc.activeElement.className,
    "spectrum-alpha-input",
    "Alpha slider has successfully received focus."
  );
  testChangingColorShouldEmitEventsHelper(s, sendLeftKey, [128, 63, 63, 0.99]);
  testChangingColorShouldEmitEventsHelper(s, sendRightKey, [128, 63, 63, 1]);

  s.destroy();
}

function setSpectrumProps(spectrum, props, updateUI = true) {
  for (const prop in props) {
    spectrum[prop] = props[prop];

    // Setting textProps implies contrast should be enabled for spectrum
    if (prop === "textProps") {
      spectrum.contrastEnabled = true;
    }
  }

  if (updateUI) {
    spectrum.updateUI();
  }
}

function testAriaAttributesOnSpectrumElements(
  spectrum,
  colorName,
  rgbString,
  alpha
) {
  for (const slider of [spectrum.dragger, spectrum.hueSlider]) {
    is(
      slider.getAttribute("aria-describedby"),
      "spectrum-dragger",
      "Slider contains the correct describedby text."
    );
    is(
      slider.getAttribute("aria-valuetext"),
      rgbString,
      "Slider contains the correct valuetext text."
    );
  }

  is(
    spectrum.colorPreview.title,
    colorName,
    "Spectrum element contains the correct title text."
  );
}

function testSettingColorShoudUpdateTheUI(container) {
  const s = new Spectrum(container, cssColors.white);
  s.show();
  const dragHelperOriginalPos = [
    s.dragHelper.style.top,
    s.dragHelper.style.left,
  ];
  const alphaSliderOriginalVal = s.alphaSlider.value;
  let hueSliderOriginalVal = s.hueSlider.value;

  setSpectrumProps(s, { rgb: [50, 240, 234, 0.2] });

  ok(s.alphaSlider.value != alphaSliderOriginalVal, "Alpha helper has moved");
  ok(
    s.dragHelper.style.top !== dragHelperOriginalPos[0],
    "Drag helper has moved"
  );
  ok(
    s.dragHelper.style.left !== dragHelperOriginalPos[1],
    "Drag helper has moved"
  );
  ok(s.hueSlider.value !== hueSliderOriginalVal, "Hue helper has moved");
  testAriaAttributesOnSpectrumElements(
    s,
    "Closest to: aqua",
    "rgba(50, 240, 234, 0.2)",
    0.2
  );

  hueSliderOriginalVal = s.hueSlider.value;

  setSpectrumProps(s, { rgb: ZERO_ALPHA_COLOR });
  is(s.alphaSlider.value, 0, "Alpha range UI has been updated again");
  ok(
    hueSliderOriginalVal !== s.hueSlider.value,
    "Hue slider should have move again"
  );
  testAriaAttributesOnSpectrumElements(s, "aqua", "rgba(0, 255, 255, 0)", 0);

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
  s.rgb = cssColors.white;
  testColorPreviewDisplay(s, "rgb(255, 255, 255)", "rgb(204, 204, 204)");

  s.destroy();
}

function testNotSettingTextPropsShouldNotShowContrastSection(container) {
  const s = new Spectrum(container, cssColors.white);
  s.show();

  setSpectrumProps(s, { rgb: cssColors.black });
  ok(
    !s.spectrumContrast.classList.contains("visible"),
    "Contrast section is not shown."
  );

  s.destroy();
}

function testSpectrumContrast(
  spectrum,
  contrastValueEl,
  rgb,
  expectedValue,
  expectedBadgeClass = "",
  expectLargeTextIndicator = false
) {
  setSpectrumProps(spectrum, { rgb });

  is(
    contrastValueEl.textContent,
    expectedValue,
    "Contrast value has the correct text."
  );
  is(
    contrastValueEl.className,
    `accessibility-contrast-value${
      expectedBadgeClass ? " " + expectedBadgeClass : ""
    }`,
    `Contrast value contains ${expectedBadgeClass || "base"} class.`
  );
  is(
    spectrum.contrastLabel.childNodes.length === 3,
    expectLargeTextIndicator,
    `Large text indicator is ${expectLargeTextIndicator ? "" : "not"} shown.`
  );
}

function testSettingTextPropsAndColorShouldUpdateContrastValue(container) {
  const s = new Spectrum(container, cssColors.white);
  s.show();

  ok(
    !s.spectrumContrast.classList.contains("visible"),
    "Contrast value is not available yet."
  );

  info(
    "Test that contrast ratio is calculated on setting 'textProps' and 'rgb'."
  );
  setSpectrumProps(
    s,
    { textProps: REGULAR_TEXT_PROPS, backgroundColorData: SINGLE_BG_COLOR },
    false
  );
  testSpectrumContrast(s, s.contrastValue, [50, 240, 234, 0.8], "1.35", FAIL);

  info("Test that contrast ratio is updated when color is changed.");
  testSpectrumContrast(s, s.contrastValue, cssColors.black, "21.00", AAA);

  info("Test that contrast ratio cannot be calculated with zero alpha.");
  testSpectrumContrast(
    s,
    s.contrastValue,
    ZERO_ALPHA_COLOR,
    "Unable to calculate"
  );

  s.destroy();
}

function testOnlySelectingLargeTextWithNonZeroAlphaShouldShowIndicator(
  container
) {
  let s = new Spectrum(container, cssColors.white);
  s.show();

  ok(
    s.contrastLabel.childNodes.length !== 3,
    "Large text indicator is initially hidden."
  );

  info(
    "Test that selecting large text with non-zero alpha shows large text indicator."
  );
  setSpectrumProps(
    s,
    {
      textProps: {
        "font-size": { value: "24px" },
        "font-weight": { value: "normal" },
        opacity: { value: "1" },
      },
      backgroundColorData: SINGLE_BG_COLOR,
    },
    false
  );
  testSpectrumContrast(s, s.contrastValue, cssColors.black, "21.00", AAA, true);

  info(
    "Test that selecting large text with zero alpha hides large text indicator."
  );
  testSpectrumContrast(
    s,
    s.contrastValue,
    ZERO_ALPHA_COLOR,
    "Unable to calculate"
  );

  // Spectrum should be closed and opened again to reflect changes in text size
  s.destroy();
  s = new Spectrum(container, cssColors.white);
  s.show();

  info("Test that selecting regular text does not show large text indicator.");
  setSpectrumProps(
    s,
    { textProps: REGULAR_TEXT_PROPS, backgroundColorData: SINGLE_BG_COLOR },
    false
  );
  testSpectrumContrast(s, s.contrastValue, cssColors.black, "21.00", AAA);

  s.destroy();
}

function testSettingMultiColoredBackgroundShouldShowContrastRange(container) {
  const s = new Spectrum(container, cssColors.white);
  s.show();

  info(
    "Test setting text with non-zero alpha and multi-colored bg shows contrast range and empty single contrast."
  );
  setSpectrumProps(
    s,
    {
      textProps: REGULAR_TEXT_PROPS,
      backgroundColorData: {
        min: cssColors.yellow,
        max: cssColors.green,
      },
    },
    false
  );
  testSpectrumContrast(s, s.contrastValueMin, cssColors.white, "1.07", FAIL);
  testSpectrumContrast(s, s.contrastValueMax, cssColors.white, "5.14", AA);
  testSpectrumContrast(s, s.contrastValue, cssColors.white, "");
  ok(
    s.spectrumContrast.classList.contains("range"),
    "Contrast section contains range class."
  );

  info("Test setting text with zero alpha shows error in contrast min span.");
  testSpectrumContrast(
    s,
    s.contrastValueMin,
    ZERO_ALPHA_COLOR,
    "Unable to calculate"
  );

  s.destroy();
}
