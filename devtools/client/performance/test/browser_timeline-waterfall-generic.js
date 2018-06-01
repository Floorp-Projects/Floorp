/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if the waterfall is properly built after finishing a recording.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording, waitForOverviewRenderedWithMarkers } = require("devtools/client/performance/test/helpers/actions");
const { once } = require("devtools/client/performance/test/helpers/event-utils");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const { $, $$, EVENTS, WaterfallView } = panel.panelWin;

  await startRecording(panel);
  ok(true, "Recording has started.");

  // Ensure overview is rendering and some markers were received.
  await waitForOverviewRenderedWithMarkers(panel);

  await stopRecording(panel);
  ok(true, "Recording has ended.");

  // Test the header container.

  ok($(".waterfall-header"),
    "A header container should have been created.");

  // Test the header sidebar (left).

  ok($(".waterfall-header > .waterfall-sidebar"),
    "A header sidebar node should have been created.");

  // Test the header ticks (right).

  ok($(".waterfall-header-ticks"),
    "A header ticks node should have been created.");
  ok($$(".waterfall-header-ticks > .waterfall-header-tick").length > 0,
    "Some header tick labels should have been created inside the tick node.");

  // Test the markers sidebar (left).

  ok($$(".waterfall-tree-item > .waterfall-sidebar").length,
    "Some marker sidebar nodes should have been created.");
  ok($$(".waterfall-tree-item > .waterfall-sidebar > .waterfall-marker-bullet").length,
    "Some marker color bullets should have been created inside the sidebar.");
  ok($$(".waterfall-tree-item > .waterfall-sidebar > .waterfall-marker-name").length,
    "Some marker name labels should have been created inside the sidebar.");

  // Test the markers waterfall (right).

  ok($$(".waterfall-tree-item > .waterfall-marker").length,
    "Some marker waterfall nodes should have been created.");
  ok($$(".waterfall-tree-item > .waterfall-marker .waterfall-marker-bar").length,
    "Some marker color bars should have been created inside the waterfall.");

  // Test the sidebar.

  const detailsView = WaterfallView.details;
  // Make sure the bounds are up to date.
  WaterfallView._recalculateBounds();

  const parentWidthBefore = $("#waterfall-view").getBoundingClientRect().width;
  const sidebarWidthBefore = $(".waterfall-sidebar").getBoundingClientRect().width;
  const detailsWidthBefore = $("#waterfall-details").getBoundingClientRect().width;

  ok(detailsView.hidden,
    "The details view in the waterfall view is hidden by default.");
  is(detailsWidthBefore, 0,
    "The details view width should be 0 when hidden.");
  is(WaterfallView.waterfallWidth,
     parentWidthBefore - sidebarWidthBefore
                       - WaterfallView.WATERFALL_MARKER_SIDEBAR_SAFE_BOUNDS,
     "The waterfall width is correct (1).");

  const waterfallRerendered = once(WaterfallView, EVENTS.UI_WATERFALL_RENDERED);
  $$(".waterfall-tree-item")[0].click();
  await waterfallRerendered;

  const parentWidthAfter = $("#waterfall-view").getBoundingClientRect().width;
  const sidebarWidthAfter = $(".waterfall-sidebar").getBoundingClientRect().width;
  const detailsWidthAfter = $("#waterfall-details").getBoundingClientRect().width;

  ok(!detailsView.hidden,
    "The details view in the waterfall view is now visible.");
  is(parentWidthBefore, parentWidthAfter,
    "The parent view's width should not have changed.");
  is(sidebarWidthBefore, sidebarWidthAfter,
    "The sidebar view's width should not have changed.");
  isnot(detailsWidthAfter, 0,
    "The details view width should not be 0 when visible.");
  is(WaterfallView.waterfallWidth,
     parentWidthAfter - sidebarWidthAfter - detailsWidthAfter
                      - WaterfallView.WATERFALL_MARKER_SIDEBAR_SAFE_BOUNDS,
     "The waterfall width is correct (2).");

  await teardownToolboxAndRemoveTab(panel);
});
