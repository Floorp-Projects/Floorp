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
    this._start = this._start.bind(this);
    this._stop = this._stop.bind(this);
    this._onMarkerSelected = this._onMarkerSelected.bind(this);
    this._onResize = this._onResize.bind(this);

    this.graph = new Waterfall($("#waterfall-graph"), $("#details-pane"));
    this.markerDetails = new MarkerDetails($("#waterfall-details"), $("#waterfall-view > splitter"));

    this.graph.on("selected", this._onMarkerSelected);
    this.graph.on("unselected", this._onMarkerSelected);
    this.markerDetails.on("resize", this._onResize);

    PerformanceController.on(EVENTS.RECORDING_STARTED, this._start);
    PerformanceController.on(EVENTS.RECORDING_STOPPED, this._stop);

    this.graph.recalculateBounds();
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
  },

  /**
   * Method for handling all the set up for rendering a new waterfall.
   */
  render: function() {
    let { startTime, endTime } = PerformanceController.getInterval();
    let markers = PerformanceController.getMarkers();

    this.graph.setData(markers, startTime, startTime, endTime);
    this.emit(EVENTS.WATERFALL_RENDERED);
  },

  /**
   * Called when recording starts.
   */
  _start: function (_, { startTime }) {
    this.graph.clearView();
  },

  /**
   * Called when recording stops.
   */
  _stop: function (_, { endTime }) {
    this.render();
  },

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
    this.graph.recalculateBounds();
    this.render();
  }
};


/**
 * Convenient way of emitting events from the view.
 */
EventEmitter.decorate(WaterfallView);
