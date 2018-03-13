/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { isNodeValid } = require("./utils/markup");
const { BoxModelHighlighter } = require("./box-model");

// How many maximum nodes can be highlighted at the same time by the
// SelectorHighlighter
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
  typeName: "SelectorHighlighter",

  /**
   * Show BoxModelHighlighter on each node that matches that provided selector.
   * @param {DOMNode} node A context node that is used to get the document on
   * which querySelectorAll should be executed. This node will NOT be
   * highlighted.
   * @param {Object} options Should at least contain the 'selector' option, a
   * string that will be used in querySelectorAll. On top of this, all of the
   * valid options to BoxModelHighlighter.show are also valid here.
   */
  show: function(node, options = {}) {
    this.hide();

    if (!isNodeValid(node) || !options.selector) {
      return false;
    }

    let nodes = [];
    try {
      nodes = [...node.ownerDocument.querySelectorAll(options.selector)];
    } catch (e) {
      // It's fine if the provided selector is invalid, nodes will be an empty
      // array.
    }

    delete options.selector;

    let i = 0;
    for (let matchingNode of nodes) {
      if (i >= MAX_HIGHLIGHTED_ELEMENTS) {
        break;
      }

      let highlighter = new BoxModelHighlighter(this.highlighterEnv);
      if (options.fill) {
        highlighter.regionFill[options.region || "border"] = options.fill;
      }
      highlighter.show(matchingNode, options);
      this._highlighters.push(highlighter);
      i++;
    }

    return true;
  },

  hide: function() {
    for (let highlighter of this._highlighters) {
      highlighter.destroy();
    }
    this._highlighters = [];
  },

  destroy: function() {
    this.hide();
    this.highlighterEnv = null;
  }
};
exports.SelectorHighlighter = SelectorHighlighter;
