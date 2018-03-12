/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the js call tree view renders the correct columns.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { UI_SHOW_PLATFORM_DATA_PREF } = require("devtools/client/performance/test/helpers/prefs");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { busyWait } = require("devtools/client/performance/test/helpers/wait-utils");
const { once } = require("devtools/client/performance/test/helpers/event-utils");

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { EVENTS, $, $$, DetailsView, JsCallTreeView } = panel.panelWin;

  // Enable platform data to show the platform functions in the tree.
  Services.prefs.setBoolPref(UI_SHOW_PLATFORM_DATA_PREF, true);

  yield startRecording(panel);
  // To show the `busyWait` function in the tree.
  yield busyWait(100);
  yield stopRecording(panel);

  let rendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  yield DetailsView.selectView("js-calltree");
  yield rendered;

  ok(DetailsView.isViewSelected(JsCallTreeView), "The call tree is now selected.");

  testCells($, $$, {
    "duration": true,
    "percentage": true,
    "allocations": false,
    "self-duration": true,
    "self-percentage": true,
    "self-allocations": false,
    "samples": true,
    "function": true
  });

  yield teardownToolboxAndRemoveTab(panel);
});

function testCells($, $$, visibleCells) {
  for (let cell in visibleCells) {
    if (visibleCells[cell]) {
      ok($(`.call-tree-cell[type=${cell}]`),
        `At least one ${cell} column was visible in the tree.`);
    } else {
      ok(!$(`.call-tree-cell[type=${cell}]`),
        `No ${cell} columns were visible in the tree.`);
    }
  }

  is($$(".call-tree-cell", $(".call-tree-item")).length,
    Object.keys(visibleCells).filter(e => visibleCells[e]).length,
    "The correct number of cells were found in the tree.");
}
