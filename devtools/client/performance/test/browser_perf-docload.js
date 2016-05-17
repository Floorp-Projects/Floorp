/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if the sidebar is updated with "DOMContentLoaded" and "load" markers.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording, reload } = require("devtools/client/performance/test/helpers/actions");
const { waitUntil } = require("devtools/client/performance/test/helpers/wait-utils");

add_task(function* () {
  let { panel, target } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { PerformanceController } = panel.panelWin;

  yield startRecording(panel);
  yield reload(target);

  yield waitUntil(() => {
    // Wait until we get the necessary markers.
    let markers = PerformanceController.getCurrentRecording().getMarkers();
    if (!markers.some(m => m.name == "document::DOMContentLoaded") ||
        !markers.some(m => m.name == "document::Load")) {
      return false;
    }

    ok(markers.filter(m => m.name == "document::DOMContentLoaded").length == 1,
      "There should only be one `DOMContentLoaded` marker.");
    ok(markers.filter(m => m.name == "document::Load").length == 1,
      "There should only be one `load` marker.");

    return true;
  });

  yield stopRecording(panel);
  yield teardownToolboxAndRemoveTab(panel);
});
