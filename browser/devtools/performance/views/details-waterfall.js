/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const WATERFALL_RESIZE_EVENTS_DRAIN = 100; // ms
const MARKER_DETAILS_WIDTH = 200;

/**
 * Waterfall view containing the timeline markers, controlled by DetailsView.
 */
let WaterfallView = Heritage.extend(DetailsSubview, {

  observedPrefs: [
    "hidden-markers"
  ],

  rerenderPrefs: [
    "hidden-markers"
  ],

  rangeChangeDebounceTime: 75, // ms

  /**
   * Sets up the view with event binding.
   */
  initialize: function () {
    DetailsSubview.initialize.call(this);

    this._onMarkerSelected = this._onMarkerSelected.bind(this);
    this._onResize = this._onResize.bind(this);
    this._onViewSource = this._onViewSource.bind(this);

    this.headerContainer = $("#waterfall-header");
    this.breakdownContainer = $("#waterfall-breakdown");
    this.detailsContainer = $("#waterfall-details");
    this.detailsSplitter = $("#waterfall-view > splitter");

    this.details = new MarkerDetails($("#waterfall-details"), $("#waterfall-view > splitter"));
    this.details.hidden = true;

    this.details.on("resize", this._onResize);
    this.details.on("view-source", this._onViewSource);
    window.addEventListener("resize", this._onResize);

    // TODO bug 1167093 save the previously set width, and ensure minimum width
    this.details.width = MARKER_DETAILS_WIDTH;
  },

  /**
   * Unbinds events.
   */
  destroy: function () {
    DetailsSubview.destroy.call(this);

    this.details.off("resize", this._onResize);
    this.details.off("view-source", this._onViewSource);
    window.removeEventListener("resize", this._onResize);
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
    let rootMarkerNode = this._prepareWaterfallTree(markers);

    this._populateWaterfallTree(rootMarkerNode, { startTime, endTime });
    this.emit(EVENTS.WATERFALL_RENDERED);
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
      this.details.hidden = false;
      this._lastSelected = marker.uid;
    }
    if (event === "unselected") {
      this.details.empty();
    }
  },

  /**
   * Called when the marker details view is resized.
   */
  _onResize: function () {
    setNamedTimeout("waterfall-resize", WATERFALL_RESIZE_EVENTS_DRAIN, () => {
      this._markersRoot.recalculateBounds();
      this.render(OverviewView.getTimeInterval());
    });
  },

  /**
   * Called whenever an observed pref is changed.
   */
  _onObservedPrefChange: function(_, prefName) {
    let blueprint = PerformanceController.getTimelineBlueprint();
    this._markersRoot.blueprint = blueprint;
  },

  /**
   * Called when MarkerDetails view emits an event to view source.
   */
  _onViewSource: function (_, file, line) {
    gToolbox.viewSourceInDebugger(file, line);
  },

  /**
   * Called when the recording is stopped and prepares data to
   * populate the waterfall tree.
   */
  _prepareWaterfallTree: function(markers) {
    let rootMarkerNode = WaterfallUtils.makeEmptyMarkerNode("(root)");

    WaterfallUtils.collapseMarkersIntoNode({
      markerNode: rootMarkerNode,
      markersList: markers
    });

    return rootMarkerNode;
  },

  /**
   * Renders the waterfall tree.
   */
  _populateWaterfallTree: function(rootMarkerNode, interval) {
    let root = new MarkerView({
      marker: rootMarkerNode,
      // The root node is irrelevant in a waterfall tree.
      hidden: true,
      // The waterfall tree should not expand by default.
      autoExpandDepth: 0
    });

    let header = new WaterfallHeader(root);

    this._markersRoot = root;
    this._waterfallHeader = header;

    let blueprint = PerformanceController.getTimelineBlueprint();
    root.blueprint = blueprint;
    root.interval = interval;
    root.on("selected", this._onMarkerSelected);
    root.on("unselected", this._onMarkerSelected);

    this.breakdownContainer.innerHTML = "";
    root.attachTo(this.breakdownContainer);

    this.headerContainer.innerHTML = "";
    header.attachTo(this.headerContainer);

    // If an item was previously selected in this view, attempt to
    // re-select it by traversing the newly created tree.
    if (this._lastSelected) {
      let item = root.find(i => i.marker.uid == this._lastSelected);
      if (item) {
        item.focus();
      }
    }
  },

  toString: () => "[object WaterfallView]"
});
