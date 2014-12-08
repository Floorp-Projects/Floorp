/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Waterfall view containing the timeline markers, controlled by DetailsView.
 */
let WaterfallView = {
  _startTime: 0,
  _endTime: 0,
  _markers: [],

  /**
   * Sets up the view with event binding.
   */
  initialize: Task.async(function *() {
    this.el = $("#waterfall-view");
    this._stop = this._stop.bind(this);
    this._start = this._start.bind(this);
    this._onTimelineData = this._onTimelineData.bind(this);
    this._onMarkerSelected = this._onMarkerSelected.bind(this);
    this._onResize = this._onResize.bind(this);

    this.graph = new Waterfall($("#waterfall-graph"), $("#details-pane"));
    this.markerDetails = new MarkerDetails($("#waterfall-details"), $("#waterfall-view > splitter"));

    this.graph.on("selected", this._onMarkerSelected);
    this.graph.on("unselected", this._onMarkerSelected);
    this.markerDetails.on("resize", this._onResize);

    PerformanceController.on(EVENTS.RECORDING_STARTED, this._start);
    PerformanceController.on(EVENTS.RECORDING_STOPPED, this._stop);
    PerformanceController.on(EVENTS.TIMELINE_DATA, this._onTimelineData);
    yield this.graph.recalculateBounds();
  }),

  /**
   * Unbinds events.
   */
  destroy: function () {
    this.graph.off("selected", this._onMarkerSelected);
    this.graph.off("unselected", this._onMarkerSelected);
    this.markerDetails.off("resize", this._onResize);

    PerformanceController.off(EVENTS.RECORDING_STARTED, this._start);
    PerformanceController.off(EVENTS.RECORDING_STOPPED, this._stop);
    PerformanceController.off(EVENTS.TIMELINE_DATA, this._onTimelineData);
  },

  render: Task.async(function *() {
    yield this.graph.recalculateBounds();
    this.graph.setData(this._markers, this._startTime, this._startTime, this._endTime);
    this.emit(EVENTS.WATERFALL_RENDERED);
  }),

  /**
   * Event handlers
   */

  /**
   * Called when recording starts.
   */
  _start: function (_, { startTime }) {
    this._startTime = startTime;
    this._endTime = startTime;
    this.graph.clearView();
  },

  /**
   * Called when recording stops.
   */
  _stop: Task.async(function *(_, { endTime }) {
    this._endTime = endTime;
    this._markers = this._markers.sort((a,b) => (a.start > b.start));
    this.render();
  }),

  /**
   * Called when a marker is selected in the waterfall view,
   * updating the markers detail view.
   */
  _onMarkerSelected: function (event, marker) {
    if (event === "selected") {
      this.markerDetails.render(marker);
    }
    if (event === "unselected") {
      this.markerDetails.empty();
    }
  },

  /**
   * Called when the marker details view is resized.
   */
  _onResize: function () {
    this.render();
  },

  /**
   * Called when the TimelineFront has new data for
   * framerate, markers or memory, and stores the data
   * to be plotted subsequently.
   */
  _onTimelineData: function (_, eventName, ...data) {
    if (eventName === "markers") {
      let [markers, endTime] = data;
      Array.prototype.push.apply(this._markers, markers);
    }
  }
};


/**
 * Convenient way of emitting events from the view.
 */
EventEmitter.decorate(WaterfallView);
