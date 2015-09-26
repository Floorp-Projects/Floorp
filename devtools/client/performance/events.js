/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.exports = {
  // Fired by the PerformanceController and OptionsView when a pref changes.
  PREF_CHANGED: "Performance:PrefChanged",

  // Fired by the PerformanceController when the devtools theme changes.
  THEME_CHANGED: "Performance:ThemeChanged",

  // Emitted by the PerformanceView when the state (display mode) changes,
  // for example when switching between "empty", "recording" or "recorded".
  // This causes certain panels to be hidden or visible.
  UI_STATE_CHANGED: "Performance:UI:StateChanged",

  // Emitted by the PerformanceView on clear button click
  UI_CLEAR_RECORDINGS: "Performance:UI:ClearRecordings",

  // Emitted by the PerformanceView on record button click
  UI_START_RECORDING: "Performance:UI:StartRecording",
  UI_STOP_RECORDING: "Performance:UI:StopRecording",

  // Emitted by the PerformanceView on import button click
  UI_IMPORT_RECORDING: "Performance:UI:ImportRecording",
  // Emitted by the RecordingsView on export button click
  UI_EXPORT_RECORDING: "Performance:UI:ExportRecording",

  // When a new recording is being tracked in the panel.
  NEW_RECORDING: "Performance:NewRecording",

  // When a new recording can't be successfully created when started.
  NEW_RECORDING_FAILED: "Performance:NewRecordingFailed",

  // When a recording is started or stopped or stopping via the PerformanceController
  RECORDING_STATE_CHANGE: "Performance:RecordingStateChange",

  // Emitted by the PerformanceController or RecordingView
  // when a recording model is selected
  RECORDING_SELECTED: "Performance:RecordingSelected",

  // When recordings have been cleared out
  RECORDINGS_CLEARED: "Performance:RecordingsCleared",

  // When a recording is exported via the PerformanceController
  RECORDING_EXPORTED: "Performance:RecordingExported",

  // Emitted by the PerformanceController when a recording is imported.
  // Unless you're interested in specifically imported recordings, like in tests
  // or telemetry, you should probably use the normal RECORDING_STATE_CHANGE in the UI.
  RECORDING_IMPORTED: "Performance:RecordingImported",

  // When the front has updated information on the profiler's circular buffer
  PROFILER_STATUS_UPDATED: "Performance:BufferUpdated",

  // When the PerformanceView updates the display of the buffer status
  UI_BUFFER_STATUS_UPDATED: "Performance:UI:BufferUpdated",

  // Emitted by the OptimizationsListView when it renders new optimization
  // data and clears the optimization data
  OPTIMIZATIONS_RESET: "Performance:UI:OptimizationsReset",
  OPTIMIZATIONS_RENDERED: "Performance:UI:OptimizationsRendered",

  // Emitted by the OverviewView when more data has been rendered
  OVERVIEW_RENDERED: "Performance:UI:OverviewRendered",
  FRAMERATE_GRAPH_RENDERED: "Performance:UI:OverviewFramerateRendered",
  MARKERS_GRAPH_RENDERED: "Performance:UI:OverviewMarkersRendered",
  MEMORY_GRAPH_RENDERED: "Performance:UI:OverviewMemoryRendered",

  // Emitted by the OverviewView when a range has been selected in the graphs
  OVERVIEW_RANGE_SELECTED: "Performance:UI:OverviewRangeSelected",

  // Emitted by the DetailsView when a subview is selected
  DETAILS_VIEW_SELECTED: "Performance:UI:DetailsViewSelected",

  // Emitted by the WaterfallView when it has been rendered
  WATERFALL_RENDERED: "Performance:UI:WaterfallRendered",

  // Emitted by the JsCallTreeView when a call tree has been rendered
  JS_CALL_TREE_RENDERED: "Performance:UI:JsCallTreeRendered",

  // Emitted by the JsFlameGraphView when it has been rendered
  JS_FLAMEGRAPH_RENDERED: "Performance:UI:JsFlameGraphRendered",

  // Emitted by the MemoryCallTreeView when a call tree has been rendered
  MEMORY_CALL_TREE_RENDERED: "Performance:UI:MemoryCallTreeRendered",

  // Emitted by the MemoryFlameGraphView when it has been rendered
  MEMORY_FLAMEGRAPH_RENDERED: "Performance:UI:MemoryFlameGraphRendered",

  // When a source is shown in the JavaScript Debugger at a specific location.
  SOURCE_SHOWN_IN_JS_DEBUGGER: "Performance:UI:SourceShownInJsDebugger",
  SOURCE_NOT_FOUND_IN_JS_DEBUGGER: "Performance:UI:SourceNotFoundInJsDebugger",

  // These are short hands for the RECORDING_STATE_CHANGE event to make refactoring
  // tests easier and in rare cases (telemetry). UI components should use
  // RECORDING_STATE_CHANGE in almost all cases,
  RECORDING_STARTED: "Performance:RecordingStarted",
  RECORDING_WILL_STOP: "Performance:RecordingWillStop",
  RECORDING_STOPPED: "Performance:RecordingStopped",

  // Fired by the PerformanceController when `populateWithRecordings` is finished.
  RECORDINGS_SEEDED: "Performance:RecordingsSeeded",

  // Emitted by the PerformanceController when `PerformanceController.stopRecording()`
  // is completed; used in tests, to know when a manual UI click is finished.
  CONTROLLER_STOPPED_RECORDING: "Performance:Controller:StoppedRecording",
};
