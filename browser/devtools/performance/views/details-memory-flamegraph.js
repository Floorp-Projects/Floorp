/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * FlameGraph view containing a pyramid-like visualization of memory allocation
 * sites, controlled by DetailsView.
 *
 * TODO: bug 1077469
 */
let MemoryFlameGraphView = Heritage.extend(DetailsSubview, {
  /**
   * Sets up the view with event binding.
   */
  initialize: Task.async(function* () {
    DetailsSubview.initialize.call(this);
  }),

  /**
   * Unbinds events.
   */
  destroy: function () {
    DetailsSubview.destroy.call(this);
  },

  /**
   * Method for handling all the set up for rendering a new flamegraph.
   *
   * @param object interval [optional]
   *        The { startTime, endTime }, in milliseconds.
   */
  render: function (interval={}) {
    this.emit(EVENTS.MEMORY_FLAMEGRAPH_RENDERED);
  }
});
