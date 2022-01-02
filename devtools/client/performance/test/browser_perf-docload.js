/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if the sidebar is updated with "DOMContentLoaded" and "load" markers.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const {
  initPerformanceInNewTab,
  teardownToolboxAndRemoveTab,
} = require("devtools/client/performance/test/helpers/panel-utils");
const {
  startRecording,
  stopRecording,
  reload,
} = require("devtools/client/performance/test/helpers/actions");
const {
  waitUntil,
} = require("devtools/client/performance/test/helpers/wait-utils");

add_task(async function() {
  // Run this test without server targets as reload would introduce a target switch
  // with a new profile record which wouldn't introduce the DOM load events.
  // This panel has now been deprecated, we might later get rid of this test
  // once removing client side targets (bug 1721852)
  await SpecialPowers.pushPrefEnv({
    set: [["devtools.target-switching.server.enabled", false]],
  });

  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window,
  });

  const { PerformanceController } = panel.panelWin;

  await startRecording(panel);
  await reload(panel);

  await waitUntil(() => {
    // Wait until we get the necessary markers.
    const markers = PerformanceController.getCurrentRecording().getMarkers();
    if (
      !markers.some(m => m.name == "document::DOMContentLoaded") ||
      !markers.some(m => m.name == "document::Load")
    ) {
      return false;
    }

    ok(
      markers.filter(m => m.name == "document::DOMContentLoaded").length == 1,
      "There should only be one `DOMContentLoaded` marker."
    );
    ok(
      markers.filter(m => m.name == "document::Load").length == 1,
      "There should only be one `load` marker."
    );

    return true;
  });

  await stopRecording(panel);
  await teardownToolboxAndRemoveTab(panel);
});
