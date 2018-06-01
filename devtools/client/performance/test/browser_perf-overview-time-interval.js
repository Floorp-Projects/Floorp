/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the `setTimeInterval` and `getTimeInterval` functions
 * work properly.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { once } = require("devtools/client/performance/test/helpers/event-utils");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const { EVENTS, OverviewView } = panel.panelWin;

  try {
    OverviewView.setTimeInterval({ starTime: 0, endTime: 1 });
    ok(false, "Setting a time interval shouldn't have worked.");
  } catch (e) {
    ok(true, "Setting a time interval didn't work, as expected.");
  }

  try {
    OverviewView.getTimeInterval();
    ok(false, "Getting the time interval shouldn't have worked.");
  } catch (e) {
    ok(true, "Getting the time interval didn't work, as expected.");
  }

  await startRecording(panel);
  await stopRecording(panel);

  // Get/set the time interval and wait for the event propagation.

  const rangeSelected = once(OverviewView, EVENTS.UI_OVERVIEW_RANGE_SELECTED);
  OverviewView.setTimeInterval({ startTime: 10, endTime: 20 });
  await rangeSelected;

  const firstInterval = OverviewView.getTimeInterval();
  info("First interval start time: " + firstInterval.startTime);
  info("First interval end time: " + firstInterval.endTime);
  is(Math.round(firstInterval.startTime), 10,
    "The interval's start time was properly set.");
  is(Math.round(firstInterval.endTime), 20,
    "The interval's end time was properly set.");

  // Get/set another time interval and make sure there's no event propagation.

  function fail() {
    ok(false, "The selection event should not have propagated.");
  }

  OverviewView.on(EVENTS.UI_OVERVIEW_RANGE_SELECTED, fail);
  OverviewView.setTimeInterval({ startTime: 30, endTime: 40 }, { stopPropagation: true });
  OverviewView.off(EVENTS.UI_OVERVIEW_RANGE_SELECTED, fail);

  const secondInterval = OverviewView.getTimeInterval();
  info("Second interval start time: " + secondInterval.startTime);
  info("Second interval end time: " + secondInterval.endTime);
  is(Math.round(secondInterval.startTime), 30,
    "The interval's start time was properly set again.");
  is(Math.round(secondInterval.endTime), 40,
    "The interval's end time was properly set again.");

  await teardownToolboxAndRemoveTab(panel);
});
