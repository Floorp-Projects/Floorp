/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const asyncStorage = require("resource://devtools/shared/async-storage.js");

// Test that the flexbox highlighter color change in the color picker is committed when
// RETURN is pressed.

const TEST_URI = URL_ROOT + "doc_flexbox_specific_cases.html";

add_task(async function () {
  // Make sure there are no custom highlighter colors stored before starting.
  await asyncStorage.removeItem("flexboxInspectorHostColors");

  await addTab(TEST_URI);
  const { inspector, flexboxInspector, layoutView } = await openLayoutView();
  const { document: doc } = flexboxInspector;
  const { store } = inspector;
  const HIGHLIGHTER_TYPE = inspector.highlighters.TYPES.FLEXBOX;
  const { waitForHighlighterTypeShown, waitForHighlighterTypeHidden } =
    getHighlighterTestHelpers(inspector);
  const cPicker = layoutView.swatchColorPickerTooltip;
  const spectrum = cPicker.spectrum;

  const onColorSwatchRendered = waitForDOM(
    doc,
    ".layout-flexbox-wrapper .layout-color-swatch"
  );
  await selectNode("#container", inspector);
  const [swatch] = await onColorSwatchRendered;

  const checkbox = doc.getElementById("flexbox-checkbox-toggle");

  info("Checking the initial state of the Flexbox Inspector color picker.");
  ok(!checkbox.checked, "Flexbox highlighter toggle is unchecked.");
  is(
    swatch.style.backgroundColor,
    "rgb(148, 0, 255)",
    "The color swatch's background is correct."
  );
  is(
    store.getState().flexbox.color,
    "#9400FF",
    "The flexbox color state is correct."
  );

  info("Toggling ON the flexbox highlighter.");
  const onHighlighterShown = waitForHighlighterTypeShown(HIGHLIGHTER_TYPE);
  const onCheckboxChange = waitUntilState(
    store,
    state => state.flexbox.highlighted
  );
  checkbox.click();
  await onHighlighterShown;
  await onCheckboxChange;

  info("Opening the color picker by clicking on the color swatch.");
  const onColorPickerReady = cPicker.once("ready");
  swatch.click();
  await onColorPickerReady;

  await simulateColorPickerChange(cPicker, [0, 255, 0, 0.5]);

  is(
    swatch.style.backgroundColor,
    "rgba(0, 255, 0, 0.5)",
    "The color swatch's background was updated."
  );

  info("Pressing RETURN to commit the color change.");
  const onColorUpdate = waitUntilState(
    store,
    state => state.flexbox.color === "#00FF0080"
  );
  const onColorPickerHidden = cPicker.tooltip.once("hidden");
  focusAndSendKey(spectrum.element.ownerDocument.defaultView, "RETURN");
  await onColorPickerHidden;
  await onColorUpdate;

  is(
    swatch.style.backgroundColor,
    "rgba(0, 255, 0, 0.5)",
    "The color swatch's background was kept after RETURN."
  );

  info("Toggling OFF the flexbox highlighter.");
  const onHighlighterHidden = waitForHighlighterTypeHidden(HIGHLIGHTER_TYPE);
  checkbox.click();
  await onHighlighterHidden;
});
