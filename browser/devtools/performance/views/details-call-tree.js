/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * CallTree view containing profiler call tree, controlled by DetailsView.
 */
let CallTreeView = {
  /**
   * Sets up the view with event binding.
   */
  initialize: function () {
    this.el = $(".call-tree");
    this._graphEl = $(".call-tree-cells-container");
    this._onRangeChange = this._onRangeChange.bind(this);
    this._stop = this._stop.bind(this);

    OverviewView.on(EVENTS.OVERVIEW_RANGE_SELECTED, this._onRangeChange);
    OverviewView.on(EVENTS.OVERVIEW_RANGE_CLEARED, this._onRangeChange);
    PerformanceController.on(EVENTS.RECORDING_STOPPED, this._stop);
  },

  /**
   * Unbinds events.
   */
  destroy: function () {
    OverviewView.off(EVENTS.OVERVIEW_RANGE_SELECTED, this._onRangeChange);
    OverviewView.off(EVENTS.OVERVIEW_RANGE_CLEARED, this._onRangeChange);
    PerformanceController.off(EVENTS.RECORDING_STOPPED, this._stop);
  },

  /**
   * Method for handling all the set up for rendering a new
   * call tree.
   */
  render: function (profilerData, beginAt, endAt, options={}) {
    let threadNode = this._prepareCallTree(profilerData, beginAt, endAt, options);
    this._populateCallTree(threadNode, options);
    this.emit(EVENTS.CALL_TREE_RENDERED);
  },

  /**
   * Called when recording is stopped.
   */
  _stop: function (_, { profilerData }) {
    this._profilerData = profilerData;
    this.render(profilerData);
  },

  /**
   * Fired when a range is selected or cleared in the OverviewView.
   */
  _onRangeChange: function (_, params) {
    // When a range is cleared, we'll have no beginAt/endAt data,
    // so the rebuild will just render all the data again.
    let { beginAt, endAt } = params || {};
    this.render(this._profilerData, beginAt, endAt);
  },

  /**
   * Called when the recording is stopped and prepares data to
   * populate the call tree.
   */
  _prepareCallTree: function (profilerData, beginAt, endAt, options) {
    let threadSamples = profilerData.profile.threads[0].samples;
    let contentOnly = !Prefs.showPlatformData;
    // TODO handle inverted tree bug 1102347
    let invertTree = false;

    let threadNode = new ThreadNode(threadSamples, contentOnly, beginAt, endAt, invertTree);
    options.inverted = invertTree && threadNode.samples > 0;

    return threadNode;
  },

  /**
   * Renders the call tree.
   */
  _populateCallTree: function (frameNode, options={}) {
    let root = new CallView({
      autoExpandDepth: options.inverted ? 0 : undefined,
      frame: frameNode,
      hidden: options.inverted,
      inverted: options.inverted
    });

    // Clear out other graphs
    this._graphEl.innerHTML = "";
    root.attachTo(this._graphEl);

    let contentOnly = !Prefs.showPlatformData;
    root.toggleCategories(!contentOnly);
  }
};

/**
 * Convenient way of emitting events from the view.
 */
EventEmitter.decorate(CallTreeView);
