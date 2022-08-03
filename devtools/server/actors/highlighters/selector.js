/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  isNodeValid,
} = require("devtools/server/actors/highlighters/utils/markup");
const {
  BoxModelHighlighter,
} = require("devtools/server/actors/highlighters/box-model");

// How many maximum nodes can be highlighted at the same time by the SelectorHighlighter
const MAX_HIGHLIGHTED_ELEMENTS = 100;

/**
 * The SelectorHighlighter runs a given selector through querySelectorAll on the
 * document of the provided context node and then uses the BoxModelHighlighter
 * to highlight the matching nodes
 */
function SelectorHighlighter(highlighterEnv) {
  this.highlighterEnv = highlighterEnv;
  this._highlighters = [];
}

SelectorHighlighter.prototype = {
  /**
   * Show a BoxModelHighlighter on each node that matches a given selector.
   *
   * @param {DOMNode} node
   *        A context node used to get the document element on which to run
   *        querySelectorAll(). This node will not be highlighted.
   * @param {Object} options
   *        Configuration options for SelectorHighlighter.
   *        All of the options for BoxModelHighlighter.show() are also valid here.
   * @param {String} options.selector
   *        Required. CSS selector used with querySelectorAll() to find matching elements.
   */
  async show(node, options = {}) {
    this.hide();

    if (!isNodeValid(node) || !options.selector) {
      return false;
    }

    let nodes = [];
    try {
      nodes = [...node.ownerDocument.querySelectorAll(options.selector)];
    } catch (e) {
      // It's fine if the provided selector is invalid, `nodes` will be an empty array.
    }

    // Prevent passing the `selector` option to BoxModelHighlighter
    delete options.selector;

    const promises = [];
    for (let i = 0; i < Math.min(nodes.length, MAX_HIGHLIGHTED_ELEMENTS); i++) {
      promises.push(this._showHighlighter(nodes[i], options));
    }

    await Promise.all(promises);
    return true;
  },

  /**
   * Create an instance of BoxModelHighlighter, wait for it to be ready
   * (see CanvasFrameAnonymousContentHelper.initialize()),
   * then show the highlighter on the given node with the given configuration options.
   *
   * @param  {DOMNode} node
   *         Node to be highlighted
   * @param  {Object} options
   *         Configuration options for the BoxModelHighlighter
   * @return {Promise} Promise that resolves when the BoxModelHighlighter is ready
   */
  async _showHighlighter(node, options) {
    const highlighter = new BoxModelHighlighter(this.highlighterEnv);
    await highlighter.isReady;

    highlighter.show(node, options);
    this._highlighters.push(highlighter);
  },

  hide() {
    for (const highlighter of this._highlighters) {
      highlighter.destroy();
    }
    this._highlighters = [];
  },

  destroy() {
    this.hide();
    this.highlighterEnv = null;
  },
};
exports.SelectorHighlighter = SelectorHighlighter;
