/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the js call tree view renders the correct columns.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const {
  UI_SHOW_PLATFORM_DATA_PREF,
} = require("devtools/client/performance/test/helpers/prefs");
const {
  initPerformanceInNewTab,
  teardownToolboxAndRemoveTab,
} = require("devtools/client/performance/test/helpers/panel-utils");
const {
  startRecording,
  stopRecording,
} = require("devtools/client/performance/test/helpers/actions");
const {
  busyWait,
} = require("devtools/client/performance/test/helpers/wait-utils");
const {
  once,
} = require("devtools/client/performance/test/helpers/event-utils");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window,
  });

  const { EVENTS, $, $$, DetailsView, JsCallTreeView } = panel.panelWin;

  // Enable platform data to show the platform functions in the tree.
  Services.prefs.setBoolPref(UI_SHOW_PLATFORM_DATA_PREF, true);

  await startRecording(panel);
  // To show the `busyWait` function in the tree.
  await busyWait(100);
  await stopRecording(panel);

  const rendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  await DetailsView.selectView("js-calltree");
  await rendered;

  ok(
    DetailsView.isViewSelected(JsCallTreeView),
    "The call tree is now selected."
  );

  testCells($, $$, {
    duration: true,
    percentage: true,
    allocations: false,
    "self-duration": true,
    "self-percentage": true,
    "self-allocations": false,
    samples: true,
    function: true,
  });

  await teardownToolboxAndRemoveTab(panel);
});

function testCells($, $$, visibleCells) {
  for (const cell in visibleCells) {
    if (visibleCells[cell]) {
      ok(
        $(`.call-tree-cell[type=${cell}]`),
        `At least one ${cell} column was visible in the tree.`
      );
    } else {
      ok(
        !$(`.call-tree-cell[type=${cell}]`),
        `No ${cell} columns were visible in the tree.`
      );
    }
  }

  is(
    $$(".call-tree-cell", $(".call-tree-item")).length,
    Object.keys(visibleCells).filter(e => visibleCells[e]).length,
    "The correct number of cells were found in the tree."
  );
}
