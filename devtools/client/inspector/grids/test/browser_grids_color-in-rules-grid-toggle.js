/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling the grid highlighter in the rule view with changes to the grid color
// from the layout view.

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

  info("Selecting the rule view.");
  const ruleView = selectRuleView(inspector);
  const highlighters = ruleView.highlighters;

  await selectNode("#grid", inspector);

  const container = getRuleViewProperty(ruleView, "#grid", "display").valueSpan;
  const gridToggle = container.querySelector(".js-toggle-grid-highlighter");

  info("Toggling ON the CSS grid highlighter from the rule-view.");
  const onHighlighterShown = highlighters.once(
    "grid-highlighter-shown",
    (nodeFront, options) => {
      info("Checking the grid highlighter display settings.");
      const {
        color,
        showGridAreasOverlay,
        showGridLineNumbers,
        showInfiniteLines,
      } = options;

      is(color, "#00FF0080", "CSS grid highlighter color is correct.");
      ok(!showGridAreasOverlay, "Show grid areas overlay option is off.");
      ok(!showGridLineNumbers, "Show grid line numbers option is off.");
      ok(!showInfiniteLines, "Show infinite lines option is off.");
    }
  );
  gridToggle.click();
  await onHighlighterShown;
});
