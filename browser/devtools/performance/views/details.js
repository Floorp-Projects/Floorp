/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Details view containing profiler call tree. Manages
 * subviews and toggles visibility between them.
 */
let DetailsView = {
  /**
   * Sets up the view with event binding, initializes
   * subviews.
   */
  initialize: function () {
    this.views = {
      callTree: CallTreeView
    };

    // Initialize subviews
    return promise.all([
      CallTreeView.initialize()
    ]);
  },

  /**
   * Unbinds events, destroys subviews.
   */
  destroy: function () {
    return promise.all([
      CallTreeView.destroy()
    ]);
  }
};

/**
 * Convenient way of emitting events from the view.
 */
EventEmitter.decorate(DetailsView);
