/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports thunks.
 * Thunks are functions that can be dispatched to the Inspector Redux store.
 *
 * These functions receive one object with options that contains:
 * - dispatch() => function to dispatch Redux actions to the store
 * - getState() => function to get the current state of the entire Inspector Redux store
 * - inspector => object instance of Inspector client
 *
 * They provide a shortcut for React components to invoke the box model highlighter
 * without having to know where the highlighter exists.
 */

module.exports = {
  /**
   * Show the grid highlighter for the given node front.
   *
   * @param {NodeFront} nodeFront
   *        Node that should be highlighted.
   * @param {Object} options
   *        Optional configuration options passed to the grid highlighter
   */
  showGridHighlighter(nodeFront, options = {}) {
    return async thunkOptions => {
      const { inspector } = thunkOptions;
      if (!inspector) {
        return;
      }

      await inspector.highlighters.showGridHighlighter(nodeFront, options);
    };
  },
};
