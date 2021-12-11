/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the details view utility functions work as advertised.
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
const {
  once,
} = require("devtools/client/performance/test/helpers/event-utils");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window,
  });

  const {
    EVENTS,
    DetailsView,
    WaterfallView,
    JsCallTreeView,
    JsFlameGraphView,
  } = panel.panelWin;

  await startRecording(panel);
  await stopRecording(panel);

  ok(
    DetailsView.isViewSelected(WaterfallView),
    "The waterfall view is selected by default in the details view."
  );

  // Select js calltree view.
  let selected = once(DetailsView, EVENTS.UI_DETAILS_VIEW_SELECTED);
  await DetailsView.selectView("js-calltree");
  await selected;

  ok(
    DetailsView.isViewSelected(JsCallTreeView),
    "The js calltree view is now selected in the details view."
  );

  // Select js flamegraph view.
  selected = once(DetailsView, EVENTS.UI_DETAILS_VIEW_SELECTED);
  await DetailsView.selectView("js-flamegraph");
  await selected;

  ok(
    DetailsView.isViewSelected(JsFlameGraphView),
    "The js flamegraph view is now selected in the details view."
  );

  // Select waterfall view.
  selected = once(DetailsView, EVENTS.UI_DETAILS_VIEW_SELECTED);
  await DetailsView.selectView("waterfall");
  await selected;

  ok(
    DetailsView.isViewSelected(WaterfallView),
    "The waterfall view is now selected in the details view."
  );

  await teardownToolboxAndRemoveTab(panel);
});
