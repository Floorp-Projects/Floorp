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
    this._stop = this._stop.bind(this);

    PerformanceController.on(EVENTS.RECORDING_STOPPED, this._stop);
  },

  /**
   * Unbinds events.
   */
  destroy: function () {
    PerformanceController.off(EVENTS.RECORDING_STOPPED, this._stop);
  },

  _stop: function (_, { profilerData }) {
    this._prepareCallTree(profilerData);
  },

  /**
   * Called when the recording is stopped and prepares data to
   * populate the call tree.
   */
  _prepareCallTree: function (profilerData, beginAt, endAt, options={}) {
    let threadSamples = profilerData.profile.threads[0].samples;
    let contentOnly = !Prefs.showPlatformData;
    // TODO handle inverted tree bug 1102347
    let invertTree = false;

    let threadNode = new ThreadNode(threadSamples, contentOnly, beginAt, endAt, invertTree);
    options.inverted = invertTree && threadNode.samples > 0;

    this._populateCallTree(threadNode, options);
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

    this.emit(EVENTS.CALL_TREE_RENDERED);
  }
};

/**
 * Convenient way of emitting events from the view.
 */
EventEmitter.decorate(CallTreeView);
