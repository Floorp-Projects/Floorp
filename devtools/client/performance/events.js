/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const ControllerEvents = {
  // Fired when a performance pref changes (either because the user changed it
  // via the tool's UI, by changing something about:config or programatically).
  PREF_CHANGED: "Performance:PrefChanged",

  // Fired when the devtools theme changes.
  THEME_CHANGED: "Performance:ThemeChanged",

  // When a new recording model is received by the controller.
  RECORDING_ADDED: "Performance:RecordingAdded",

  // When a recording model gets removed from the controller.
  RECORDING_DELETED: "Performance:RecordingDeleted",

  // When a recording model becomes "started", "stopping" or "stopped".
  RECORDING_STATE_CHANGE: "Performance:RecordingStateChange",

  // When a recording is offering information on the profiler's circular buffer.
  RECORDING_PROFILER_STATUS_UPDATE: "Performance:RecordingProfilerStatusUpdate",

  // When a recording model becomes marked as selected.
  RECORDING_SELECTED: "Performance:RecordingSelected",

  // When starting a recording is attempted and fails because the backend
  // does not permit it at this time.
  BACKEND_FAILED_AFTER_RECORDING_START: "Performance:BackendFailedRecordingStart",

  // When a recording is started and the backend has started working.
  BACKEND_READY_AFTER_RECORDING_START: "Performance:BackendReadyRecordingStart",

  // When a recording is stopped and the backend has finished cleaning up.
  BACKEND_READY_AFTER_RECORDING_STOP: "Performance:BackendReadyRecordingStop",

  // When a recording is exported.
  RECORDING_EXPORTED: "Performance:RecordingExported",

  // When a recording is imported.
  RECORDING_IMPORTED: "Performance:RecordingImported",

  // When a source is shown in the JavaScript Debugger at a specific location.
  SOURCE_SHOWN_IN_JS_DEBUGGER: "Performance:UI:SourceShownInJsDebugger",
  SOURCE_NOT_FOUND_IN_JS_DEBUGGER: "Performance:UI:SourceNotFoundInJsDebugger",

  // Fired by the PerformanceController when `populateWithRecordings` is finished.
  RECORDINGS_SEEDED: "Performance:RecordingsSeeded",
};

const ViewEvents = {
  // Emitted by the `ToolbarView` when a preference changes.
  UI_PREF_CHANGED: "Performance:UI:PrefChanged",

  // When the state (display mode) changes, for example when switching between
  // "empty", "recording" or "recorded". This causes certain parts of the UI
  // to be hidden or visible.
  UI_STATE_CHANGED: "Performance:UI:StateChanged",

  // Emitted by the `PerformanceView` on clear button click.
  UI_CLEAR_RECORDINGS: "Performance:UI:ClearRecordings",

  // Emitted by the `PerformanceView` on record button click.
  UI_START_RECORDING: "Performance:UI:StartRecording",
  UI_STOP_RECORDING: "Performance:UI:StopRecording",

  // Emitted by the `PerformanceView` on import/export button click.
  UI_IMPORT_RECORDING: "Performance:UI:ImportRecording",
  UI_EXPORT_RECORDING: "Performance:UI:ExportRecording",

  // Emitted by the `PerformanceView` when the profiler's circular buffer
  // status has been rendered.
  UI_RECORDING_PROFILER_STATUS_RENDERED: "Performance:UI:RecordingProfilerStatusRendered",

  // When a recording is selected in the UI.
  UI_RECORDING_SELECTED: "Performance:UI:RecordingSelected",

  // Emitted by the `DetailsView` when a subview is selected
  UI_DETAILS_VIEW_SELECTED: "Performance:UI:DetailsViewSelected",

  // Emitted by the `OverviewView` after something has been rendered.
  UI_OVERVIEW_RENDERED: "Performance:UI:OverviewRendered",
  UI_MARKERS_GRAPH_RENDERED: "Performance:UI:OverviewMarkersRendered",
  UI_MEMORY_GRAPH_RENDERED: "Performance:UI:OverviewMemoryRendered",
  UI_FRAMERATE_GRAPH_RENDERED: "Performance:UI:OverviewFramerateRendered",

  // Emitted by the `OverviewView` when a range has been selected in the graphs.
  UI_OVERVIEW_RANGE_SELECTED: "Performance:UI:OverviewRangeSelected",

  // Emitted by the `WaterfallView` when it has been rendered.
  UI_WATERFALL_RENDERED: "Performance:UI:WaterfallRendered",

  // Emitted by the `JsCallTreeView` when it has been rendered.
  UI_JS_CALL_TREE_RENDERED: "Performance:UI:JsCallTreeRendered",

  // Emitted by the `JsFlameGraphView` when it has been rendered.
  UI_JS_FLAMEGRAPH_RENDERED: "Performance:UI:JsFlameGraphRendered",

  // Emitted by the `MemoryCallTreeView` when it has been rendered.
  UI_MEMORY_CALL_TREE_RENDERED: "Performance:UI:MemoryCallTreeRendered",

  // Emitted by the `MemoryFlameGraphView` when it has been rendered.
  UI_MEMORY_FLAMEGRAPH_RENDERED: "Performance:UI:MemoryFlameGraphRendered",
};

module.exports = Object.assign({}, ControllerEvents, ViewEvents);
