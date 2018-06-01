/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the 'Extend grid lines infinitely' grid highlighter setting will update
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

const SHOW_INFINITE_LINES_PREF = "devtools.gridinspector.showInfiniteLines";

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, gridInspector } = await openLayoutView();
  const { document: doc } = gridInspector;
  const { store } = inspector;

  await selectNode("#grid", inspector);
  const checkbox = doc.getElementById("grid-setting-extend-grid-lines");

  ok(!Services.prefs.getBoolPref(SHOW_INFINITE_LINES_PREF),
    "'Extend grid lines infinitely' is pref off by default.");

  info("Toggling ON the 'Extend grid lines infinitely' setting.");
  let onCheckboxChange = waitUntilState(store, state =>
    state.highlighterSettings.showInfiniteLines);
  checkbox.click();
  await onCheckboxChange;

  info("Toggling OFF the 'Extend grid lines infinitely' setting.");
  onCheckboxChange = waitUntilState(store, state =>
    !state.highlighterSettings.showInfiniteLines);
  checkbox.click();
  await onCheckboxChange;

  ok(!Services.prefs.getBoolPref(SHOW_INFINITE_LINES_PREF),
    "'Extend grid lines infinitely' is pref off.");

  Services.prefs.clearUserPref(SHOW_INFINITE_LINES_PREF);
});
