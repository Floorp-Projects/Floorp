/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the waterfall view renders content after recording.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const {
  initPerformanceInNewTab,
  teardownToolboxAndRemoveTab,
} = require("devtools/client/performance/test/helpers/panel-utils");
const {
  startRecording,
  stopRecording,
} = require("devtools/client/performance/test/helpers/actions");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window,
  });

  const { DetailsView, WaterfallView } = panel.panelWin;

  await startRecording(panel);
  // Already waits for EVENTS.UI_WATERFALL_RENDERED.
  await stopRecording(panel);

  ok(
    DetailsView.isViewSelected(WaterfallView),
    "The waterfall view is selected by default in the details view."
  );

  ok(true, "WaterfallView rendered after recording is stopped.");

  await startRecording(panel);
  // Already waits for EVENTS.UI_WATERFALL_RENDERED.
  await stopRecording(panel);

  ok(
    DetailsView.isViewSelected(WaterfallView),
    "The waterfall view is still selected in the details view."
  );

  ok(
    true,
    "WaterfallView rendered again after recording completed a second time."
  );

  await teardownToolboxAndRemoveTab(panel);
});
