/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the grid item's color change in the colorpicker is committed when RETURN is
// pressed.

const TEST_URI = `
  <style type='text/css'>
    #grid {
      display: grid;
    }
  </style>
  <div id="grid">
    <div id="cell1">cell1</div>
    <div id="cell2">cell2</div>
  </div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, gridInspector, layoutView } = await openLayoutView();
  const { document: doc } = gridInspector;
  const { store } = inspector;
  const cPicker = layoutView.swatchColorPickerTooltip;
  const spectrum = cPicker.spectrum;
  const swatch = doc.querySelector(
    "#layout-grid-container .layout-color-swatch"
  );

  info("Checking the initial state of the Grid Inspector.");
  is(
    swatch.style.backgroundColor,
    "rgb(148, 0, 255)",
    "The color swatch's background is correct."
  );
  is(
    store.getState().grids[0].color,
    "#9400FF",
    "The grid color state is correct."
  );

  info("Scrolling into view of the #grid color swatch.");
  swatch.scrollIntoView();

  info("Opening the color picker by clicking on the #grid color swatch.");
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
  const onGridColorUpdate = waitUntilState(
    store,
    state => state.grids[0].color === "#00FF0080"
  );
  const onColorPickerHidden = cPicker.tooltip.once("hidden");
  focusAndSendKey(spectrum.element.ownerDocument.defaultView, "RETURN");
  await onColorPickerHidden;
  await onGridColorUpdate;

  is(
    swatch.style.backgroundColor,
    "rgba(0, 255, 0, 0.5)",
    "The color swatch's background was kept after RETURN."
  );
});
