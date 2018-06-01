/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ReflowFront } = require("devtools/shared/fronts/reflow");

/**
 * Simple utility class that listens to reflows on a given target if and only if a
 * listener is actively listening to reflows.
 *
 * @param {Object} target
 *        The current target (as in toolbox target)
 */
function ReflowTracker(target) {
  this.target = target;

  // Hold a Map of all the listeners interested in reflows.
  this.listeners = new Map();

  this.reflowFront = null;

  this.onReflow = this.onReflow.bind(this);
}

ReflowTracker.prototype = {

  destroy() {
    if (this.reflowFront) {
      this.stopTracking();
      this.reflowFront.destroy();
      this.reflowFront = null;
    }

    this.listeners.clear();
  },

  startTracking() {
    // Initialize reflow front if necessary.
    if (!this.reflowFront && this.target.form.reflowActor) {
      const { client, form } = this.target;
      this.reflowFront = ReflowFront(client, form);
    }

    if (this.reflowFront) {
      this.reflowFront.on("reflows", this.onReflow);
      this.reflowFront.start();
    }
  },

  stopTracking() {
    if (this.reflowFront) {
      this.reflowFront.off("reflows", this.onReflow);
      this.reflowFront.stop();
    }
  },

  /**
   * Add a listener for reflows.
   *
   * @param {Object} listener
   *        Object/instance listening to reflows.
   * @param {Function} callback
   *        The associated callback.
   */
  trackReflows(listener, callback) {
    if (this.listeners.get(listener) === callback) {
      return;
    }

    // No listener interested in reflows yet, start tracking.
    if (this.listeners.size === 0) {
      this.startTracking();
    }

    this.listeners.set(listener, callback);
  },

  /**
   * Remove a listener for reflows.
   *
   * @param {Object} listener
   *        Object/instance listening to reflows.
   * @param {Function} callback
   *        The associated callback.
   */
  untrackReflows(listener, callback) {
    if (this.listeners.get(listener) !== callback) {
      return;
    }

    this.listeners.delete(listener);

    // No listener interested in reflows anymore, stop tracking.
    if (this.listeners.size === 0) {
      this.stopTracking();
    }
  },

  /**
   * Handler called when a reflow happened.
   */
  onReflow() {
    for (const [, callback] of this.listeners) {
      callback();
    }
  },
};

module.exports = ReflowTracker;
