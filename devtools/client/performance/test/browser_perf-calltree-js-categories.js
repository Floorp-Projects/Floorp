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

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { EVENTS, $, $$, DetailsView, JsCallTreeView } = panel.panelWin;

  // Enable platform data to show the categories in the tree.
  Services.prefs.setBoolPref(UI_SHOW_PLATFORM_DATA_PREF, true);

  yield startRecording(panel);
  // To show the `Gecko` category in the tree.
  yield busyWait(100);
  yield stopRecording(panel);

  let rendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  yield DetailsView.selectView("js-calltree");
  yield rendered;

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

  yield teardownToolboxAndRemoveTab(panel);
});

function geckoCategoryPresent($$) {
  for (let elem of $$(".call-tree-category")) {
    if (elem.textContent.trim() == "Gecko") {
      return true;
    }
  }
  return false;
}
