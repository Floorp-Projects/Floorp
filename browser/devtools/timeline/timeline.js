/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/devtools/Loader.jsm");
Cu.import("resource:///modules/devtools/ViewHelpers.jsm");
Cu.import("resource:///modules/devtools/gDevTools.jsm");

devtools.lazyRequireGetter(this, "promise");
devtools.lazyRequireGetter(this, "EventEmitter",
  "devtools/toolkit/event-emitter");

devtools.lazyRequireGetter(this, "MarkersOverview",
  "devtools/timeline/markers-overview", true);
devtools.lazyRequireGetter(this, "MemoryOverview",
  "devtools/timeline/memory-overview", true);
devtools.lazyRequireGetter(this, "Waterfall",
  "devtools/timeline/waterfall", true);
devtools.lazyRequireGetter(this, "MarkerDetails",
  "devtools/timeline/marker-details", true);
devtools.lazyRequireGetter(this, "TIMELINE_BLUEPRINT",
  "devtools/timeline/global", true);

devtools.lazyImporter(this, "CanvasGraphUtils",
  "resource:///modules/devtools/Graphs.jsm");

devtools.lazyImporter(this, "PluralForm",
  "resource://gre/modules/PluralForm.jsm");

const OVERVIEW_UPDATE_INTERVAL = 200;
const OVERVIEW_INITIAL_SELECTION_RATIO = 0.15;

/**
 * Preference for devtools.timeline.hiddenMarkers.
 * Stores which markers should be hidden.
 */
const Prefs = new ViewHelpers.Prefs("devtools.timeline", {
  hiddenMarkers: ["Json", "hiddenMarkers"]
});

// The panel's window global is an EventEmitter firing the following events:
const EVENTS = {
  // When a recording is started or stopped, via the `stopwatch` button.
  RECORDING_STARTED: "Timeline:RecordingStarted",
  RECORDING_ENDED: "Timeline:RecordingEnded",

  // When the overview graphs are populated with new markers.
  OVERVIEW_UPDATED: "Timeline:OverviewUpdated",

  // When the waterfall view is populated with new markers.
  WATERFALL_UPDATED: "Timeline:WaterfallUpdated"
};

/**
 * The current target and the timeline front, set by this tool's host.
 */
let gToolbox, gTarget, gFront;

/**
 * Initializes the timeline controller and views.
 */
let startupTimeline = Task.async(function*() {
  yield TimelineView.initialize();
  yield TimelineController.initialize();
});

/**
 * Destroys the timeline controller and views.
 */
let shutdownTimeline = Task.async(function*() {
  yield TimelineView.destroy();
  yield TimelineController.destroy();
  yield gFront.stop();
});

/**
 * Functions handling the timeline frontend controller.
 */
let TimelineController = {
  /**
   * Permanent storage for the markers and the memory measurements streamed by
   * the backend, along with the start and end timestamps.
   */
  _starTime: 0,
  _endTime: 0,
  _markers: [],
  _memory: [],
  _frames: [],

  /**
   * Initialization function, called when the tool is started.
   */
  initialize: function() {
    this._onRecordingTick = this._onRecordingTick.bind(this);
    this._onMarkers = this._onMarkers.bind(this);
    this._onMemory = this._onMemory.bind(this);
    this._onFrames = this._onFrames.bind(this);

    gFront.on("markers", this._onMarkers);
    gFront.on("memory", this._onMemory);
    gFront.on("frames", this._onFrames);
  },

  /**
   * Destruction function, called when the tool is closed.
   */
  destroy: function() {
    gFront.off("markers", this._onMarkers);
    gFront.off("memory", this._onMemory);
    gFront.off("frames", this._onFrames);
  },

  /**
   * Gets the { stat, end } time interval for this recording.
   * @return object
   */
  getInterval: function() {
    return { startTime: this._startTime, endTime: this._endTime };
  },

  /**
   * Gets the accumulated markers in this recording.
   * @return array
   */
  getMarkers: function() {
    return this._markers;
  },

  /**
   * Gets the accumulated memory measurements in this recording.
   * @return array
   */
  getMemory: function() {
    return this._memory;
  },

  /**
   * Gets stack frame array reported by the actor.  The marker "stack"
   * and "endStack" properties are indices into this array.  See
   * actors/utils/stack.js for more details.
   * @return array
   */
  getFrames: function() {
    return this._frames;
  },

  /**
   * Updates the views to show or hide the memory recording data.
   */
  updateMemoryRecording: Task.async(function*() {
    if ($("#memory-checkbox").checked) {
      yield TimelineView.showMemoryOverview();
    } else {
      yield TimelineView.hideMemoryOverview();
    }
  }),

  /**
   * Starts/stops the timeline recording and streaming.
   */
  toggleRecording: Task.async(function*() {
    let isRecording = yield gFront.isRecording();
    if (isRecording == false) {
      yield this._startRecording();
    } else {
      yield this._stopRecording();
    }
  }),

  /**
   * Starts the recording, updating the UI as needed.
   */
  _startRecording: function*() {
    TimelineView.handleRecordingStarted();

    let withMemory = $("#memory-checkbox").checked;
    let startTime = yield gFront.start({ withMemory });

    // Times must come from the actor in order to be self-consistent.
    // However, we also want to update the view with the elapsed time
    // even when the actor is not generating data.  To do this we get
    // the local time and use it to compute a reasonable elapsed time.
    // See _onRecordingTick.
    this._localStartTime = performance.now();
    this._startTime = startTime;
    this._endTime = startTime;
    this._markers = [];
    this._memory = [];
    this._frames = [];
    this._updateId = setInterval(this._onRecordingTick, OVERVIEW_UPDATE_INTERVAL);
  },

  /**
   * Stops the recording, updating the UI as needed.
   */
  _stopRecording: function*() {
    clearInterval(this._updateId);

    // Sorting markers is only important when displayed in the waterfall.
    this._markers = this._markers.sort((a,b) => (a.start > b.start));

    TimelineView.handleRecordingUpdate();
    TimelineView.handleRecordingEnded();
    yield gFront.stop();
  },

  /**
   * Used in tests. Stops the recording, discarding the accumulated markers and
   * updating the UI as needed.
   */
  _stopRecordingAndDiscardData: function*() {
    // Clear the markers before calling async method _stopRecording to properly
    // reset the selection if markers were already received. Bug 1092452.
    this._markers.length = 0;
    this._memory.length = 0;

    yield this._stopRecording();

    // Clear the markers after _stopRecording has finished. It's possible that
    // server sent new markers before it received the request to stop sending
    // them and client received them while we were waiting for _stopRecording
    // to finish. Bug 1067287.
    this._markers.length = 0;
    this._memory.length = 0;
  },

  /**
   * Callback handling the "markers" event on the timeline front.
   *
   * @param array markers
   *        A list of new markers collected since the last time this
   *        function was invoked.
   * @param number endTime
   *        A time after the last marker in markers was collected.
   */
  _onMarkers: function(markers, endTime) {
    for (let marker of markers) {
      marker.start -= this._startTime;
      marker.end -= this._startTime;
    }
    Array.prototype.push.apply(this._markers, markers);
    this._endTime = endTime;
  },

  /**
   * Callback handling the "memory" event on the timeline front.
   *
   * @param number delta
   *        The number of milliseconds elapsed since epoch.
   * @param object measurement
   *        A detailed breakdown of the current memory usage.
   */
  _onMemory: function(delta, measurement) {
    this._memory.push({
      delta: delta - this._startTime,
      value: measurement.total / 1024 / 1024
    });
  },

  /**
   * Callback handling the "frames" event on the timeline front.
   *
   * @param number delta
   *        The number of milliseconds elapsed since epoch.
   * @param object frames
   *        Newly generated frame objects.
   */
  _onFrames: function(delta, frames) {
    Array.prototype.push.apply(this._frames, frames);
  },

  /**
   * Callback invoked at a fixed interval while recording.
   * Updates the current time and the timeline overview.
   */
  _onRecordingTick: function() {
    // Compute an approximate ending time for the view.  This is
    // needed to ensure that the view updates even when new data is
    // not being generated.
    let fakeTime = this._startTime + (performance.now() - this._localStartTime);
    if (fakeTime > this._endTime) {
      this._endTime = fakeTime;
    }
    TimelineView.handleRecordingUpdate();
  }
};

/**
 * Functions handling the timeline frontend view.
 */
let TimelineView = {
  /**
   * Initialization function, called when the tool is started.
   */
  initialize: Task.async(function*() {
    let blueprint = this._getFilteredBluePrint();
    this.markersOverview = new MarkersOverview($("#markers-overview"), blueprint);
    this.waterfall = new Waterfall($("#timeline-waterfall"), $("#timeline-pane"), blueprint);
    this.markerDetails = new MarkerDetails($("#timeline-waterfall-details"), $("#timeline-waterfall-container > splitter"));

    this._onThemeChange = this._onThemeChange.bind(this);
    this._onSelecting = this._onSelecting.bind(this);
    this._onRefresh = this._onRefresh.bind(this);

    gDevTools.on("pref-changed", this._onThemeChange);
    this.markersOverview.on("selecting", this._onSelecting);
    this.markersOverview.on("refresh", this._onRefresh);
    this.markerDetails.on("resize", this._onRefresh);

    this._onMarkerSelected = this._onMarkerSelected.bind(this);
    this.waterfall.on("selected", this._onMarkerSelected);
    this.waterfall.on("unselected", this._onMarkerSelected);

    let theme = Services.prefs.getCharPref("devtools.theme");
    this.markersOverview.setTheme(theme);

    yield this.markersOverview.ready();

    yield this.waterfall.recalculateBounds();

    this._buildFilterPopup();
  }),

  /**
   * Destruction function, called when the tool is closed.
   */
  destroy: function() {
    gDevTools.off("pref-changed", this._onThemeChange);
    this.markerDetails.off("resize", this._onRefresh);
    this.markerDetails.destroy();
    this.waterfall.off("selected", this._onMarkerSelected);
    this.waterfall.off("unselected", this._onMarkerSelected);
    this.waterfall.destroy();
    this.markersOverview.off("selecting", this._onSelecting);
    this.markersOverview.off("refresh", this._onRefresh);
    this.markersOverview.destroy();

    // The memory overview graph is not always available.
    if (this.memoryOverview) {
      this.memoryOverview.destroy();
    }
  },

  /**
   * Shows the memory overview graph.
   */
  showMemoryOverview: Task.async(function*() {
    let theme = Services.prefs.getCharPref("devtools.theme");

    this.memoryOverview = new MemoryOverview($("#memory-overview"));
    this.memoryOverview.setTheme(theme);
    yield this.memoryOverview.ready();

    let memory = TimelineController.getMemory();
    this.memoryOverview.setData(memory);

    CanvasGraphUtils.linkAnimation(this.markersOverview, this.memoryOverview);
    CanvasGraphUtils.linkSelection(this.markersOverview, this.memoryOverview);
  }),

  /**
   * Hides the memory overview graph.
   */
  hideMemoryOverview: function() {
    if (!this.memoryOverview) {
      return;
    }
    this.memoryOverview.destroy();
    this.memoryOverview = null;
  },

  /**
   * A marker has been selected in the waterfall.
   */
  _onMarkerSelected: function(event, marker) {
    if (event == "selected") {
      this.markerDetails.render({
        toolbox: gToolbox,
        marker: marker,
        frames: TimelineController.getFrames()
      });
    }
    if (event == "unselected") {
      this.markerDetails.empty();
    }
  },

  /**
   * Signals that a recording session has started and triggers the appropriate
   * changes in the UI.
   */
  handleRecordingStarted: function() {
    $("#record-button").setAttribute("checked", "true");
    $("#memory-checkbox").setAttribute("disabled", "true");
    $("#timeline-pane").selectedPanel = $("#recording-notice");

    this.markersOverview.clearView();

    // The memory overview graph is not always available.
    if (this.memoryOverview) {
      this.memoryOverview.clearView();
    }

    this.waterfall.clearView();

    window.emit(EVENTS.RECORDING_STARTED);
  },

  /**
   * Signals that a recording session has ended and triggers the appropriate
   * changes in the UI.
   */
  handleRecordingEnded: function() {
    $("#record-button").removeAttribute("checked");
    $("#memory-checkbox").removeAttribute("disabled");
    $("#timeline-pane").selectedPanel = $("#timeline-waterfall-container");

    this.markersOverview.selectionEnabled = true;

    // The memory overview graph is not always available.
    if (this.memoryOverview) {
      this.memoryOverview.selectionEnabled = true;
    }

    let interval = TimelineController.getInterval();
    let markers = TimelineController.getMarkers();
    let memory = TimelineController.getMemory();

    if (markers.length) {
      let start = markers[0].start * this.markersOverview.dataScaleX;
      let end = start + this.markersOverview.width * OVERVIEW_INITIAL_SELECTION_RATIO;
      this.markersOverview.setSelection({ start, end });
    } else {
      let startTime = interval.startTime;
      let endTime = interval.endTime;
      this.waterfall.setData({ markers, interval: { startTime, endTime } });
    }

    window.emit(EVENTS.RECORDING_ENDED);
  },

  /**
   * Signals that a new set of markers was made available by the controller,
   * or that the overview graph needs to be updated.
   */
  handleRecordingUpdate: function() {
    let interval = TimelineController.getInterval();
    let markers = TimelineController.getMarkers();
    let memory = TimelineController.getMemory();

    let duration = interval.endTime - interval.startTime;
    this.markersOverview.setData({ markers, duration });

    // The memory overview graph is not always available.
    if (this.memoryOverview) {
      this.memoryOverview.setData(memory);
    }

    window.emit(EVENTS.OVERVIEW_UPDATED);
  },

  /**
   * Callback handling the "selecting" event on the timeline overview.
   */
  _onSelecting: function() {
    if (!this.markersOverview.hasSelection() &&
        !this.markersOverview.hasSelectionInProgress()) {
      this.waterfall.clearView();
      return;
    }
    this.waterfall.resetSelection();
    this.updateWaterfall();
  },

  /**
   * Rebuild the waterfall.
   */
  updateWaterfall: function() {
    let selection = this.markersOverview.getSelection();
    let start = selection.start / this.markersOverview.dataScaleX;
    let end = selection.end / this.markersOverview.dataScaleX;

    let markers = TimelineController.getMarkers();
    let interval = TimelineController.getInterval();

    let startTime = Math.min(start, end);
    let endTime = Math.max(start, end);

    this.waterfall.setData({ markers, interval: { startTime, endTime } });
  },

  /**
   * Callback handling the "refresh" event on the timeline overview.
   */
  _onRefresh: function() {
    this.waterfall.recalculateBounds();
    this.updateWaterfall();
  },

  /**
   * Rebuild a blueprint without hidden markers.
   */
  _getFilteredBluePrint: function() {
    let hiddenMarkers = Prefs.hiddenMarkers;
    let filteredBlueprint = Cu.cloneInto(TIMELINE_BLUEPRINT, {});
    let maybeRemovedGroups = new Set();
    let removedGroups = new Set();

    // 1. Remove hidden markers from the blueprint.

    for (let hiddenMarkerName of hiddenMarkers) {
      maybeRemovedGroups.add(filteredBlueprint[hiddenMarkerName].group);
      delete filteredBlueprint[hiddenMarkerName];
    }

    // 2. Get a list of all the groups that will be removed.

    for (let removedGroup of maybeRemovedGroups) {
      let markerNames = Object.keys(filteredBlueprint);
      let allGroupsRemoved = markerNames.every(e => filteredBlueprint[e].group != removedGroup);
      if (allGroupsRemoved) {
        removedGroups.add(removedGroup);
      }
    }

    // 3. Offset groups.

    for (let removedGroup of removedGroups) {
      for (let [, markerDetails] of Iterator(filteredBlueprint)) {
        if (markerDetails.group > removedGroup) {
          markerDetails.group--;
        }
      }
    }

    return filteredBlueprint;

  },

  /**
   * When the list of hidden markers changes, update waterfall
   * and overview.
   */
  _onHiddenMarkersChanged: function(e) {
    let menuItems = $$("#timelineFilterPopup menuitem[marker-type]:not([checked])");
    let hiddenMarkers = Array.map(menuItems, e => e.getAttribute("marker-type"));

    Prefs.hiddenMarkers = hiddenMarkers;
    let blueprint = this._getFilteredBluePrint();

    this.waterfall.setBlueprint(blueprint);
    this.updateWaterfall();

    this.markersOverview.setBlueprint(blueprint);
    this.markersOverview.refresh({ force: true });
  },

  /**
   * Creates the filter popup.
   */
  _buildFilterPopup: function() {
    let popup = $("#timelineFilterPopup");
    let button = $("#filter-button");

    popup.addEventListener("popupshowing", () => button.setAttribute("open", "true"));
    popup.addEventListener("popuphiding",  () => button.removeAttribute("open"));

    this._onHiddenMarkersChanged = this._onHiddenMarkersChanged.bind(this);

    for (let [markerName, markerDetails] of Iterator(TIMELINE_BLUEPRINT)) {
      let menuitem = document.createElement("menuitem");
      menuitem.setAttribute("closemenu", "none");
      menuitem.setAttribute("type", "checkbox");
      menuitem.setAttribute("marker-type", markerName);
      menuitem.setAttribute("label", markerDetails.label);
      menuitem.setAttribute("flex", "1");
      menuitem.setAttribute("align", "center");

      menuitem.addEventListener("command", this._onHiddenMarkersChanged);

      if (Prefs.hiddenMarkers.indexOf(markerName) == -1) {
        menuitem.setAttribute("checked", "true");
      }

      // Style used by pseudo element ::before in timeline.css.in
      let bulletStyle = `--bullet-bg: ${markerDetails.fill};`
      bulletStyle += `--bullet-border: ${markerDetails.stroke}`;
      menuitem.setAttribute("style", bulletStyle);

      popup.appendChild(menuitem);
    }
  },

  /*
   * Called when the developer tools theme changes. Redraws
   * the graphs with the new theme setting.
   */
  _onThemeChange: function (_, theme) {
    if (this.memoryOverview) {
      this.memoryOverview.setTheme(theme.newValue);
      this.memoryOverview.refresh({ force: true });
    }

    this.markersOverview.setTheme(theme.newValue);
    this.markersOverview.refresh({ force: true });
  }
};

/**
 * Convenient way of emitting events from the panel window.
 */
EventEmitter.decorate(this);

/**
 * DOM query helpers.
 */
function $(selector, target = document) {
  return target.querySelector(selector);
}
function $$(selector, target = document) {
  return target.querySelectorAll(selector);
}
