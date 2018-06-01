/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the when a custom color has been previously set, we initialize
// the grid with that color.

const TEST_URI = `
  <style type='text/css'>
    #grid {
      display: grid;
    }
  </style>
  <div id="grid">
    <div class="cell1">cell1</div>
    <div class="cell2">cell2</div>
  </div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, gridInspector, toolbox } = await openLayoutView();
  const { document: doc } = gridInspector;
  const { store } = inspector;
  const cPicker = gridInspector.getSwatchColorPickerTooltip();
  const swatch = doc.querySelector(".grid-color-swatch");

  info("Scrolling into view of the #grid color swatch.");
  swatch.scrollIntoView();

  info("Opening the color picker by clicking on the #grid color swatch.");
  const onColorPickerReady = cPicker.once("ready");
  swatch.click();
  await onColorPickerReady;

  await simulateColorPickerChange(cPicker, [51, 48, 0, 1]);

  info("Closing the toolbox.");
  await toolbox.destroy();
  info("Open the toolbox again.");
  await openLayoutView();

  info("Check that the previously set custom color is used.");
  is(swatch.style.backgroundColor, "rgb(51, 48, 0)",
    "The color swatch's background is correct.");
  is(store.getState().grids[0].color, "#333000", "The grid color state is correct.");
});
