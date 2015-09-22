/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { CanvasFrameAnonymousContentHelper } = require("./utils/markup");
const { getAdjustedQuads } = require("devtools/shared/layout/utils");
/**
 * The RectHighlighter is a class that draws a rectangle highlighter at specific
 * coordinates.
 * It does *not* highlight DOM nodes, but rects.
 * It also does *not* update dynamically, it only highlights a rect and remains
 * there as long as it is shown.
 */
function RectHighlighter(highlighterEnv) {
  this.win = highlighterEnv.window;
  this.markup = new CanvasFrameAnonymousContentHelper(highlighterEnv,
    this._buildMarkup.bind(this));
}

RectHighlighter.prototype = {
  typeName: "RectHighlighter",

  _buildMarkup: function() {
    let doc = this.win.document;

    let container = doc.createElement("div");
    container.className = "highlighter-container";
    container.innerHTML = "<div id=\"highlighted-rect\" " +
                          "class=\"highlighted-rect\" hidden=\"true\">";

    return container;
  },

  destroy: function() {
    this.win = null;
    this.markup.destroy();
  },

  getElement: function(id) {
    return this.markup.getElement(id);
  },

  _hasValidOptions: function(options) {
    let isValidNb = n => typeof n === "number" && n >= 0 && isFinite(n);
    return options && options.rect &&
           isValidNb(options.rect.x) &&
           isValidNb(options.rect.y) &&
           options.rect.width && isValidNb(options.rect.width) &&
           options.rect.height && isValidNb(options.rect.height);
  },

  /**
   * @param {DOMNode} node The highlighter rect is relatively positioned to the
   * viewport this node is in. Using the provided node, the highligther will get
   * the parent documentElement and use it as context to position the
   * highlighter correctly.
   * @param {Object} options Accepts the following options:
   * - rect: mandatory object that should have the x, y, width, height
   *   properties
   * - fill: optional fill color for the rect
   */
  show: function(node, options) {
    if (!this._hasValidOptions(options) || !node || !node.ownerDocument) {
      this.hide();
      return false;
    }

    let contextNode = node.ownerDocument.documentElement;

    // Caculate the absolute rect based on the context node's adjusted quads.
    let quads = getAdjustedQuads(this.win, contextNode);
    if (!quads.length) {
      this.hide();
      return false;
    }

    let {bounds} = quads[0];
    let x = "left:" + (bounds.x + options.rect.x) + "px;";
    let y = "top:" + (bounds.y + options.rect.y) + "px;";
    let width = "width:" + options.rect.width + "px;";
    let height = "height:" + options.rect.height + "px;";

    let style = x + y + width + height;
    if (options.fill) {
      style += "background:" + options.fill + ";";
    }

    // Set the coordinates of the highlighter and show it
    let rect = this.getElement("highlighted-rect");
    rect.setAttribute("style", style);
    rect.removeAttribute("hidden");

    return true;
  },

  hide: function() {
    this.getElement("highlighted-rect").setAttribute("hidden", "true");
  }
};
exports.RectHighlighter = RectHighlighter;
