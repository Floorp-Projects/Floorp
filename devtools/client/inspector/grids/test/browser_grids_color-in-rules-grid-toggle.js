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

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let { inspector, gridInspector } = yield openLayoutView();
  let { document: doc } = gridInspector;
  let { store } = inspector;
  let cPicker = gridInspector.getSwatchColorPickerTooltip();
  let spectrum = cPicker.spectrum;
  let swatch = doc.querySelector(".grid-color-swatch");

  info("Scrolling into view of the #grid color swatch.");
  swatch.scrollIntoView();

  info("Opening the color picker by clicking on the #grid color swatch.");
  let onColorPickerReady = cPicker.once("ready");
  swatch.click();
  yield onColorPickerReady;

  yield simulateColorPickerChange(cPicker, [0, 255, 0, .5]);

  is(swatch.style.backgroundColor, "rgba(0, 255, 0, 0.5)",
    "The color swatch's background was updated.");

  info("Pressing RETURN to commit the color change.");
  let onGridColorUpdate = waitUntilState(store, state =>
    state.grids[0].color === "#00FF0080");
  let onColorPickerHidden = cPicker.tooltip.once("hidden");
  focusAndSendKey(spectrum.element.ownerDocument.defaultView, "RETURN");
  yield onColorPickerHidden;
  yield onGridColorUpdate;

  is(swatch.style.backgroundColor, "rgba(0, 255, 0, 0.5)",
    "The color swatch's background was kept after RETURN.");

  info("Selecting the rule view.");
  let ruleView = selectRuleView(inspector);
  let highlighters = ruleView.highlighters;

  yield selectNode("#grid", inspector);

  let container = getRuleViewProperty(ruleView, "#grid", "display").valueSpan;
  let gridToggle = container.querySelector(".ruleview-grid");

  info("Toggling ON the CSS grid highlighter from the rule-view.");
  let onHighlighterShown = highlighters.once("grid-highlighter-shown",
    (event, nodeFront, options) => {
      info("Checking the grid highlighter display settings.");
      let {
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
  yield onHighlighterShown;
});
