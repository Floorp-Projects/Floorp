/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { Constants } = require("devtools/client/performance/modules/constants");
const {
  once,
  times,
} = require("devtools/client/performance/test/helpers/event-utils");
const {
  waitUntil,
} = require("devtools/client/performance/test/helpers/wait-utils");

/**
 * Starts a manual recording in the given performance tool panel and
 * waits for it to finish starting.
 */
exports.startRecording = function(panel, options = {}) {
  const controller = panel.panelWin.PerformanceController;

  return Promise.all([
    controller.startRecording(),
    exports.waitForRecordingStartedEvents(panel, options),
  ]);
};

/**
 * Stops the latest recording in the given performance tool panel and
 * waits for it to finish stopping.
 */
exports.stopRecording = function(panel, options = {}) {
  const controller = panel.panelWin.PerformanceController;

  return Promise.all([
    controller.stopRecording(),
    exports.waitForRecordingStoppedEvents(panel, options),
  ]);
};

/**
 * Waits for all the necessary events to be emitted after a recording starts.
 */
exports.waitForRecordingStartedEvents = function(panel, options = {}) {
  options.expectedViewState =
    options.expectedViewState || /^(console-)?recording$/;

  const EVENTS = panel.panelWin.EVENTS;
  const controller = panel.panelWin.PerformanceController;
  const view = panel.panelWin.PerformanceView;
  const overview = panel.panelWin.OverviewView;

  return Promise.all([
    options.skipWaitingForBackendReady
      ? null
      : once(controller, EVENTS.BACKEND_READY_AFTER_RECORDING_START),
    options.skipWaitingForRecordingStarted
      ? null
      : once(controller, EVENTS.RECORDING_STATE_CHANGE, {
          expectedArgs: ["recording-started"],
        }),
    options.skipWaitingForViewState
      ? null
      : once(view, EVENTS.UI_STATE_CHANGED, {
          expectedArgs: [options.expectedViewState],
        }),
    options.skipWaitingForOverview
      ? null
      : once(overview, EVENTS.UI_OVERVIEW_RENDERED, {
          expectedArgs: [Constants.FRAMERATE_GRAPH_LOW_RES_INTERVAL],
        }),
  ]);
};

/**
 * Waits for all the necessary events to be emitted after a recording finishes.
 */
exports.waitForRecordingStoppedEvents = function(panel, options = {}) {
  options.expectedViewClass = options.expectedViewClass || "WaterfallView";
  options.expectedViewEvent =
    options.expectedViewEvent || "UI_WATERFALL_RENDERED";
  options.expectedViewState = options.expectedViewState || "recorded";

  const EVENTS = panel.panelWin.EVENTS;
  const controller = panel.panelWin.PerformanceController;
  const view = panel.panelWin.PerformanceView;
  const overview = panel.panelWin.OverviewView;
  const subview = panel.panelWin[options.expectedViewClass];

  return Promise.all([
    options.skipWaitingForBackendReady
      ? null
      : once(controller, EVENTS.BACKEND_READY_AFTER_RECORDING_STOP),
    options.skipWaitingForRecordingStop
      ? null
      : once(controller, EVENTS.RECORDING_STATE_CHANGE, {
          expectedArgs: ["recording-stopping"],
        }),
    options.skipWaitingForRecordingStop
      ? null
      : once(controller, EVENTS.RECORDING_STATE_CHANGE, {
          expectedArgs: ["recording-stopped"],
        }),
    options.skipWaitingForViewState
      ? null
      : once(view, EVENTS.UI_STATE_CHANGED, {
          expectedArgs: [options.expectedViewState],
        }),
    options.skipWaitingForOverview
      ? null
      : once(overview, EVENTS.UI_OVERVIEW_RENDERED, {
          expectedArgs: [Constants.FRAMERATE_GRAPH_HIGH_RES_INTERVAL],
        }),
    options.skipWaitingForSubview
      ? null
      : once(subview, EVENTS[options.expectedViewEvent]),
  ]);
};

/**
 * Waits for rendering to happen once on all the performance tool's widgets.
 */
exports.waitForAllWidgetsRendered = panel => {
  const { panelWin } = panel;
  const { EVENTS } = panelWin;

  return Promise.all([
    once(panelWin.OverviewView, EVENTS.UI_MARKERS_GRAPH_RENDERED),
    once(panelWin.OverviewView, EVENTS.UI_MEMORY_GRAPH_RENDERED),
    once(panelWin.OverviewView, EVENTS.UI_FRAMERATE_GRAPH_RENDERED),
    once(panelWin.OverviewView, EVENTS.UI_OVERVIEW_RENDERED),
    once(panelWin.WaterfallView, EVENTS.UI_WATERFALL_RENDERED),
    once(panelWin.JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED),
    once(panelWin.JsFlameGraphView, EVENTS.UI_JS_FLAMEGRAPH_RENDERED),
    once(panelWin.MemoryCallTreeView, EVENTS.UI_MEMORY_CALL_TREE_RENDERED),
    once(panelWin.MemoryFlameGraphView, EVENTS.UI_MEMORY_FLAMEGRAPH_RENDERED),
  ]);
};

/**
 * Waits for rendering to happen on the performance tool's overview graph,
 * making sure some markers were also rendered.
 */
exports.waitForOverviewRenderedWithMarkers = (
  panel,
  minTimes = 3,
  minMarkers = 1
) => {
  const { EVENTS, OverviewView, PerformanceController } = panel.panelWin;

  return Promise.all([
    times(OverviewView, EVENTS.UI_OVERVIEW_RENDERED, minTimes, {
      expectedArgs: [Constants.FRAMERATE_GRAPH_LOW_RES_INTERVAL],
    }),
    waitUntil(
      () =>
        PerformanceController.getCurrentRecording().getMarkers().length >=
        minMarkers
    ),
  ]);
};

/**
 * Reloads the current tab
 */
exports.reload = async panel => {
  await panel.commands.targetCommand.reloadTopLevelTarget();
};
