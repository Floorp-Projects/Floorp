/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const WATERFALL_UPDATE_DEBOUNCE = 10; // ms

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
    this._onRangeChange = this._onRangeChange.bind(this);
    this._onDetailsViewSelected = this._onDetailsViewSelected.bind(this);
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
    OverviewView.on(EVENTS.OVERVIEW_RANGE_SELECTED, this._onRangeChange);
    OverviewView.on(EVENTS.OVERVIEW_RANGE_CLEARED, this._onRangeChange);
    DetailsView.on(EVENTS.DETAILS_VIEW_SELECTED, this._onDetailsViewSelected);

    this.waterfall.recalculateBounds();
  }),

  /**
   * Unbinds events.
   */
  destroy: function () {
    clearNamedTimeout("waterfall-update");

    this.waterfall.off("selected", this._onMarkerSelected);
    this.waterfall.off("unselected", this._onMarkerSelected);
    this.details.off("resize", this._onResize);

    PerformanceController.off(EVENTS.RECORDING_STARTED, this._onRecordingStarted);
    PerformanceController.off(EVENTS.RECORDING_STOPPED, this._onRecordingStoppedOrSelected);
    PerformanceController.off(EVENTS.RECORDING_SELECTED, this._onRecordingStoppedOrSelected);
    OverviewView.off(EVENTS.OVERVIEW_RANGE_SELECTED, this._onRangeChange);
    OverviewView.off(EVENTS.OVERVIEW_RANGE_CLEARED, this._onRangeChange);
    DetailsView.off(EVENTS.DETAILS_VIEW_SELECTED, this._onDetailsViewSelected);
  },

  /**
   * Method for handling all the set up for rendering a new waterfall.
   *
   * @param object interval [optional]
   *        The { startTime, endTime }, in milliseconds.
   */
  render: function(interval={}) {
    let recording = PerformanceController.getCurrentRecording();
    let startTime = interval.startTime || 0;
    let endTime = interval.endTime || recording.getDuration();
    let markers = recording.getMarkers();
    this.waterfall.setData({ markers, interval: { startTime, endTime } });
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
   * Fired when a range is selected or cleared in the OverviewView.
   */
  _onRangeChange: function (_, interval) {
    if (DetailsView.isViewSelected(this)) {
      let debounced = () => this.render(interval);
      setNamedTimeout("waterfall-update", WATERFALL_UPDATE_DEBOUNCE, debounced);
    } else {
      this._dirty = true;
      this._interval = interval;
    }
  },

  /**
   * Fired when a view is selected in the DetailsView.
   */
  _onDetailsViewSelected: function() {
    if (DetailsView.isViewSelected(this) && this._dirty) {
      this.render(this._interval);
      this._dirty = false;
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
