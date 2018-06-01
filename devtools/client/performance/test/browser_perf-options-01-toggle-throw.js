/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that toggling preferences before there are any recordings does not throw.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const { DetailsView, JsCallTreeView } = panel.panelWin;

  await DetailsView.selectView("js-calltree");

  // Manually call the _onPrefChanged function so we can catch an error.
  try {
    JsCallTreeView._onPrefChanged(null, "invert-call-tree", true);
    ok(true, "Toggling preferences before there are any recordings should not fail.");
  } catch (e) {
    ok(false, "Toggling preferences before there are any recordings should not fail.");
  }

  await teardownToolboxAndRemoveTab(panel);
});
