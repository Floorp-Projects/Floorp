/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the flexbox highlighter color change in the color picker is reverted when
// ESCAPE is pressed.

const TEST_URI = URL_ROOT + "doc_flexbox_simple.html";

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, flexboxInspector, layoutView } = await openLayoutView();
  const { document: doc } = flexboxInspector;
  const { store } = inspector;
  const cPicker = layoutView.swatchColorPickerTooltip;
  const spectrum = cPicker.spectrum;

  const onColorSwatchRendered = waitForDOM(doc,
    "#layout-flexbox-container .layout-color-swatch");
  await selectNode("#container", inspector);
  const [swatch] = await onColorSwatchRendered;

  info("Checking the initial state of the Flexbox Inspector color picker.");
  is(swatch.style.backgroundColor, "rgb(148, 0, 255)",
    "The color swatch's background is correct.");
  is(store.getState().flexbox.color, "#9400FF", "The flexbox color state is correct.");

  info("Opening the color picker by clicking on the color swatch.");
  const onColorPickerReady = cPicker.once("ready");
  swatch.click();
  await onColorPickerReady;

  await simulateColorPickerChange(cPicker, [0, 255, 0, .5]);

  is(swatch.style.backgroundColor, "rgba(0, 255, 0, 0.5)",
    "The color swatch's background was updated.");

  info("Pressing ESCAPE to close the tooltip.");
  const onColorUpdate = waitUntilState(store, state => state.flexbox.color === "#9400FF");
  const onColorPickerHidden = cPicker.tooltip.once("hidden");
  focusAndSendKey(spectrum.element.ownerDocument.defaultView, "ESCAPE");
  await onColorPickerHidden;
  await onColorUpdate;

  is(swatch.style.backgroundColor, "rgb(148, 0, 255)",
    "The color swatch's background was reverted after ESCAPE.");
});
