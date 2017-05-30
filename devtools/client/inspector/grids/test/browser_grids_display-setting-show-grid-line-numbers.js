/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the 'Display numbers on lines' grid highlighter setting will update
// the redux store and pref setting.

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

const SHOW_GRID_LINE_NUMBERS = "devtools.gridinspector.showGridLineNumbers";

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let { inspector, gridInspector } = yield openLayoutView();
  let { document: doc } = gridInspector;
  let { store } = inspector;

  yield selectNode("#grid", inspector);
  let checkbox = doc.getElementById("grid-setting-show-grid-line-numbers");

  info("Checking the initial state of the CSS grid highlighter setting.");
  ok(!Services.prefs.getBoolPref(SHOW_GRID_LINE_NUMBERS),
    "'Display numbers on lines' is pref off by default.");

  info("Toggling ON the 'Display numbers on lines' setting.");
  let onCheckboxChange = waitUntilState(store, state =>
    state.highlighterSettings.showGridLineNumbers);
  checkbox.click();
  yield onCheckboxChange;

  ok(Services.prefs.getBoolPref(SHOW_GRID_LINE_NUMBERS),
    "'Display numbers on lines' is pref on.");

  info("Toggling OFF the 'Display numbers on lines' setting.");
  onCheckboxChange = waitUntilState(store, state =>
    !state.highlighterSettings.showGridLineNumbers);
  checkbox.click();
  yield onCheckboxChange;

  ok(!Services.prefs.getBoolPref(SHOW_GRID_LINE_NUMBERS),
    "'Display numbers on lines' is pref off.");

  Services.prefs.clearUserPref(SHOW_GRID_LINE_NUMBERS);
});
