/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Waterfall view containing the timeline markers, controlled by DetailsView.
 */
let WaterfallView = {
  /**
   * Sets up the view with event binding.
   */
  initialize: Task.async(function *() {
    this._onRecordingStarted = this._onRecordingStarted.bind(this);
    this._onRecordingStoppedOrSelected = this._onRecordingStoppedOrSelected.bind(this);
    this._onMarkerSelected = this._onMarkerSelected.bind(this);
    this._onResize = this._onResize.bind(this);

    this.waterfall = new Waterfall($("#waterfall-breakdown"), $("#details-pane"), TIMELINE_BLUEPRINT);
    this.details = new MarkerDetails($("#waterfall-details"), $("#waterfall-view > splitter"));

    this.waterfall.on("selected", this._onMarkerSelected);
    this.waterfall.on("unselected", this._onMarkerSelected);
    this.details.on("resize", this._onResize);

    PerformanceController.on(EVENTS.RECORDING_STARTED, this._onRecordingStarted);
    PerformanceController.on(EVENTS.RECORDING_STOPPED, this._onRecordingStoppedOrSelected);
    PerformanceController.on(EVENTS.RECORDING_SELECTED, this._onRecordingStoppedOrSelected);

    this.waterfall.recalculateBounds();
  }),

  /**
   * Unbinds events.
   */
  destroy: function () {
    this.waterfall.off("selected", this._onMarkerSelected);
    this.waterfall.off("unselected", this._onMarkerSelected);
    this.details.off("resize", this._onResize);

    PerformanceController.off(EVENTS.RECORDING_STARTED, this._onRecordingStarted);
    PerformanceController.off(EVENTS.RECORDING_STOPPED, this._onRecordingStoppedOrSelected);
    PerformanceController.off(EVENTS.RECORDING_SELECTED, this._onRecordingStoppedOrSelected);
  },

  /**
   * Method for handling all the set up for rendering a new waterfall.
   */
  render: function() {
    let recording = PerformanceController.getCurrentRecording();
    let { startTime, endTime } = recording.getInterval();
    let markers = recording.getMarkers();

    this.waterfall.setData(markers, startTime, startTime, endTime);

    this.emit(EVENTS.WATERFALL_RENDERED);
  },

  /**
   * Called when recording starts.
   */
  _onRecordingStarted: function () {
    this.waterfall.clearView();
  },

  /**
   * Called when recording stops or is selected.
   */
  _onRecordingStoppedOrSelected: function (_, recording) {
    if (!recording.isRecording()) {
      this.render();
    }
  },

  /**
   * Called when a marker is selected in the waterfall view,
   * updating the markers detail view.
   */
  _onMarkerSelected: function (event, marker) {
    let recording = PerformanceController.getCurrentRecording();
    let frames = recording.getFrames();

    if (event === "selected") {
      this.details.render({ toolbox: gToolbox, marker, frames });
    }
    if (event === "unselected") {
      this.details.empty();
    }
  },

  /**
   * Called when the marker details view is resized.
   */
  _onResize: function () {
    this.waterfall.recalculateBounds();
    this.render();
  }
};

/**
 * Convenient way of emitting events from the view.
 */
EventEmitter.decorate(WaterfallView);
