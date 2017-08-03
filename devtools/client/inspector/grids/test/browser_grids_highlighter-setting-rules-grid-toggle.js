/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling the grid highlighter in the rule view with changes in the grid
// display setting from the layout view.

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

const SHOW_INFINITE_LINES_PREF = "devtools.gridinspector.showInfiniteLines";

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let { inspector, gridInspector } = yield openLayoutView();
  let { document: doc } = gridInspector;
  let { store } = inspector;

  let checkbox = doc.getElementById("grid-setting-extend-grid-lines");

  ok(!Services.prefs.getBoolPref(SHOW_INFINITE_LINES_PREF),
    "'Extend grid lines infinitely' is pref off by default.");

  info("Toggling ON the 'Extend grid lines infinitely' setting.");
  let onCheckboxChange = waitUntilState(store, state =>
    state.highlighterSettings.showInfiniteLines);
  checkbox.click();
  yield onCheckboxChange;

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

      is(color, "#9400FF", "CSS grid highlighter color is correct.");
      ok(!showGridAreasOverlay, "Show grid areas overlay option is off.");
      ok(!showGridLineNumbers, "Show grid line numbers option is off.");
      ok(showInfiniteLines, "Show infinite lines option is on.");
    }
  );
  gridToggle.click();
  yield onHighlighterShown;

  Services.prefs.clearUserPref(SHOW_INFINITE_LINES_PREF);
});
