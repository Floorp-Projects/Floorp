/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals $, PerformanceController, OverviewView, DetailsView */
"use strict";

const { extend } = require("devtools/shared/extend");
const EventEmitter = require("devtools/shared/event-emitter");
const {
  setNamedTimeout,
  clearNamedTimeout,
} = require("devtools/client/shared/widgets/view-helpers");

const React = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");

const EVENTS = require("../events");
const WaterfallUtils = require("../modules/logic/waterfall-utils");
const { TickUtils } = require("../modules/waterfall-ticks");
const { MarkerDetails } = require("../modules/widgets/marker-details");
const { DetailsSubview } = require("./details-abstract-subview");

const Waterfall = React.createFactory(require("../components/Waterfall"));

const MARKER_DETAILS_WIDTH = 200;
// Units are in milliseconds.
const WATERFALL_RESIZE_EVENTS_DRAIN = 100;

/**
 * Waterfall view containing the timeline markers, controlled by DetailsView.
 */
const WaterfallView = extend(DetailsSubview, {
  // Smallest unit of time between two markers. Larger by 10x^3 than Number.EPSILON.
  MARKER_EPSILON: 0.000000000001,
  // px
  WATERFALL_MARKER_SIDEBAR_WIDTH: 175,
  // px
  WATERFALL_MARKER_SIDEBAR_SAFE_BOUNDS: 20,

  observedPrefs: ["hidden-markers"],

  rerenderPrefs: ["hidden-markers"],

  // Units are in milliseconds.
  rangeChangeDebounceTime: 75,

  /**
   * Sets up the view with event binding.
   */
  initialize: function() {
    DetailsSubview.initialize.call(this);

    this._cache = new WeakMap();

    this._onMarkerSelected = this._onMarkerSelected.bind(this);
    this._onResize = this._onResize.bind(this);
    this._onViewSource = this._onViewSource.bind(this);
    this._onShowAllocations = this._onShowAllocations.bind(this);
    this._hiddenMarkers = PerformanceController.getPref("hidden-markers");

    this.treeContainer = $("#waterfall-tree");
    this.detailsContainer = $("#waterfall-details");
    this.detailsSplitter = $("#waterfall-view > splitter");

    this.details = new MarkerDetails(
      $("#waterfall-details"),
      $("#waterfall-view > splitter")
    );
    this.details.hidden = true;

    this.details.on("resize", this._onResize);
    this.details.on("view-source", this._onViewSource);
    this.details.on("show-allocations", this._onShowAllocations);
    window.addEventListener("resize", this._onResize);

    // TODO bug 1167093 save the previously set width, and ensure minimum width
    this.details.width = MARKER_DETAILS_WIDTH;
  },

  /**
   * Unbinds events.
   */
  destroy: function() {
    DetailsSubview.destroy.call(this);

    clearNamedTimeout("waterfall-resize");

    this._cache = null;

    this.details.off("resize", this._onResize);
    this.details.off("view-source", this._onViewSource);
    this.details.off("show-allocations", this._onShowAllocations);
    window.removeEventListener("resize", this._onResize);

    ReactDOM.unmountComponentAtNode(this.treeContainer);
  },

  /**
   * Method for handling all the set up for rendering a new waterfall.
   *
   * @param object interval [optional]
   *        The { startTime, endTime }, in milliseconds.
   */
  render: function(interval = {}) {
    const recording = PerformanceController.getCurrentRecording();
    if (recording.isRecording()) {
      return;
    }
    const startTime = interval.startTime || 0;
    const endTime = interval.endTime || recording.getDuration();
    const markers = recording.getMarkers();
    const rootMarkerNode = this._prepareWaterfallTree(markers);

    this._populateWaterfallTree(rootMarkerNode, { startTime, endTime });
    this.emit(EVENTS.UI_WATERFALL_RENDERED);
  },

  /**
   * Called when a marker is selected in the waterfall view,
   * updating the markers detail view.
   */
  _onMarkerSelected: function(event, marker) {
    const recording = PerformanceController.getCurrentRecording();
    const frames = recording.getFrames();
    const allocations = recording.getConfiguration().withAllocations;

    if (event === "selected") {
      this.details.render({ marker, frames, allocations });
      this.details.hidden = false;
    }
    if (event === "unselected") {
      this.details.empty();
    }
  },

  /**
   * Called when the marker details view is resized.
   */
  _onResize: function() {
    setNamedTimeout("waterfall-resize", WATERFALL_RESIZE_EVENTS_DRAIN, () => {
      this.render(OverviewView.getTimeInterval());
    });
  },

  /**
   * Called whenever an observed pref is changed.
   */
  _onObservedPrefChange: function(prefName) {
    this._hiddenMarkers = PerformanceController.getPref("hidden-markers");

    // Clear the cache as we'll need to recompute the collapsed
    // marker model
    this._cache = new WeakMap();
  },

  /**
   * Called when MarkerDetails view emits an event to view source.
   */
  _onViewSource: function(data) {
    PerformanceController.toolbox.viewSourceInDebugger(
      data.url,
      data.line,
      data.column
    );
  },

  /**
   * Called when MarkerDetails view emits an event to snap to allocations.
   */
  _onShowAllocations: function(data) {
    let { endTime } = data;
    let startTime = 0;
    const recording = PerformanceController.getCurrentRecording();
    const markers = recording.getMarkers();

    let lastGCMarkerFromPreviousCycle = null;
    let lastGCMarker = null;
    // Iterate over markers looking for the most recent GC marker
    // from the cycle before the marker's whose allocations we're interested in.
    for (const marker of markers) {
      // We found the marker whose allocations we're tracking; abort
      if (marker.start === endTime) {
        break;
      }

      if (marker.name === "GarbageCollection") {
        if (lastGCMarker && lastGCMarker.cycle !== marker.cycle) {
          lastGCMarkerFromPreviousCycle = lastGCMarker;
        }
        lastGCMarker = marker;
      }
    }

    if (lastGCMarkerFromPreviousCycle) {
      startTime = lastGCMarkerFromPreviousCycle.end;
    }

    // Adjust times so we don't include the range of these markers themselves.
    endTime -= this.MARKER_EPSILON;
    startTime += startTime !== 0 ? this.MARKER_EPSILON : 0;

    OverviewView.setTimeInterval({ startTime, endTime });
    DetailsView.selectView("memory-calltree");
  },

  /**
   * Called when the recording is stopped and prepares data to
   * populate the waterfall tree.
   */
  _prepareWaterfallTree: function(markers) {
    const cached = this._cache.get(markers);
    if (cached) {
      return cached;
    }

    const rootMarkerNode = WaterfallUtils.createParentNode({ name: "(root)" });

    WaterfallUtils.collapseMarkersIntoNode({
      rootNode: rootMarkerNode,
      markersList: markers,
      filter: this._hiddenMarkers,
    });

    this._cache.set(markers, rootMarkerNode);
    return rootMarkerNode;
  },

  /**
   * Calculates the available width for the waterfall.
   * This should be invoked every time the container node is resized.
   */
  _recalculateBounds: function() {
    this.waterfallWidth =
      this.treeContainer.clientWidth -
      this.WATERFALL_MARKER_SIDEBAR_WIDTH -
      this.WATERFALL_MARKER_SIDEBAR_SAFE_BOUNDS;
  },

  /**
   * Renders the waterfall tree.
   */
  _populateWaterfallTree: function(rootMarkerNode, interval) {
    this._recalculateBounds();

    const doc = this.treeContainer.ownerDocument;
    const startTime = interval.startTime | 0;
    const endTime = interval.endTime | 0;
    const dataScale = this.waterfallWidth / (endTime - startTime);

    this.canvas = TickUtils.drawWaterfallBackground(
      doc,
      dataScale,
      this.waterfallWidth
    );

    const treeView = Waterfall({
      marker: rootMarkerNode,
      startTime,
      endTime,
      dataScale,
      sidebarWidth: this.WATERFALL_MARKER_SIDEBAR_WIDTH,
      waterfallWidth: this.waterfallWidth,
      onFocus: node => this._onMarkerSelected("selected", node),
    });

    ReactDOM.render(treeView, this.treeContainer);
  },

  toString: () => "[object WaterfallView]",
});

EventEmitter.decorate(WaterfallView);

exports.WaterfallView = WaterfallView;
