/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the 'Display grid areas' grid highlighter setting will update
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

const SHOW_GRID_AREAS_PREF = "devtools.gridinspector.showGridAreas";

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, gridInspector } = await openLayoutView();
  const { document: doc } = gridInspector;
  const { store } = inspector;

  await selectNode("#grid", inspector);
  const checkbox = doc.getElementById("grid-setting-show-grid-areas");

  ok(!Services.prefs.getBoolPref(SHOW_GRID_AREAS_PREF),
    "'Display grid areas' is pref off by default.");

  info("Toggling ON the 'Display grid areas' setting.");
  let onCheckboxChange = waitUntilState(store, state =>
    state.highlighterSettings.showGridAreasOverlay);
  checkbox.click();
  await onCheckboxChange;

  info("Toggling OFF the 'Display grid areas' setting.");
  onCheckboxChange = waitUntilState(store, state =>
    !state.highlighterSettings.showGridAreasOverlay);
  checkbox.click();
  await onCheckboxChange;

  ok(!Services.prefs.getBoolPref(SHOW_GRID_AREAS_PREF),
    "'Display grid areas' is pref off.");

  Services.prefs.clearUserPref(SHOW_GRID_AREAS_PREF);
});
