/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  installHelperSheet,
  isNodeValid,
  addPseudoClassLock,
  removePseudoClassLock
} = require("./utils/markup");

// SimpleOutlineHighlighter's stylesheet
const HIGHLIGHTED_PSEUDO_CLASS = ":-moz-devtools-highlighted";
const SIMPLE_OUTLINE_SHEET = `.__fx-devtools-hide-shortcut__ {
                                visibility: hidden !important
                              }
                              ${HIGHLIGHTED_PSEUDO_CLASS} {
                                outline: 2px dashed #F06!important;
                                outline-offset: -2px!important
                              }`;
/**
 * The SimpleOutlineHighlighter is a class that has the same API than the
 * BoxModelHighlighter, but adds a pseudo-class on the target element itself
 * to draw a simple css outline around the element.
 * It is used by the HighlighterActor when canvasframe-based highlighters can't
 * be used. This is the case for XUL windows.
 */
function SimpleOutlineHighlighter(highlighterEnv) {
  this.chromeDoc = highlighterEnv.document;
}

SimpleOutlineHighlighter.prototype = {
  /**
   * Destroy the nodes. Remove listeners.
   */
  destroy: function () {
    this.hide();
    this.chromeDoc = null;
  },

  /**
   * Show the highlighter on a given node
   * @param {DOMNode} node
   */
  show: function (node) {
    if (isNodeValid(node) && (!this.currentNode || node !== this.currentNode)) {
      this.hide();
      this.currentNode = node;
      installHelperSheet(node.ownerDocument.defaultView, SIMPLE_OUTLINE_SHEET);
      addPseudoClassLock(node, HIGHLIGHTED_PSEUDO_CLASS);
    }
    return true;
  },

  /**
   * Hide the highlighter, the outline and the infobar.
   */
  hide: function () {
    if (this.currentNode) {
      removePseudoClassLock(this.currentNode, HIGHLIGHTED_PSEUDO_CLASS);
      this.currentNode = null;
    }
  }
};
exports.SimpleOutlineHighlighter = SimpleOutlineHighlighter;
