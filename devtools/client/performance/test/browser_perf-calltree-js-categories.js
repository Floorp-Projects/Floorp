/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the categories are shown in the js call tree when
 * platform data is enabled.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { UI_SHOW_PLATFORM_DATA_PREF } = require("devtools/client/performance/test/helpers/prefs");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { busyWait } = require("devtools/client/performance/test/helpers/wait-utils");
const { once } = require("devtools/client/performance/test/helpers/event-utils");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const { EVENTS, $, $$, DetailsView, JsCallTreeView } = panel.panelWin;

  // Enable platform data to show the categories in the tree.
  Services.prefs.setBoolPref(UI_SHOW_PLATFORM_DATA_PREF, true);

  await startRecording(panel);
  // To show the `Gecko` category in the tree.
  await busyWait(100);
  await stopRecording(panel);

  const rendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  await DetailsView.selectView("js-calltree");
  await rendered;

  is($(".call-tree-cells-container").hasAttribute("categories-hidden"), false,
    "The call tree cells container should show the categories now.");
  ok(geckoCategoryPresent($$),
    "A category node with the text `Gecko` is displayed in the tree.");

  // Disable platform data to hide the categories.
  Services.prefs.setBoolPref(UI_SHOW_PLATFORM_DATA_PREF, false);

  is($(".call-tree-cells-container").getAttribute("categories-hidden"), "",
    "The call tree cells container should hide the categories now.");
  ok(!geckoCategoryPresent($$),
    "A category node with the text `Gecko` doesn't exist in the tree anymore.");

  await teardownToolboxAndRemoveTab(panel);
});

function geckoCategoryPresent($$) {
  for (const elem of $$(".call-tree-category")) {
    if (elem.textContent.trim() == "Gecko") {
      return true;
    }
  }
  return false;
}
