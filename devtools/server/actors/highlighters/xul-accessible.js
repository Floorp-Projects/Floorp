/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getBounds } = require("./utils/accessibility");
const { createNode, isNodeValid } = require("./utils/markup");
const { getCurrentZoom, loadSheet } = require("devtools/shared/layout/utils");

/**
 * Stylesheet used for highlighter styling of accessible objects in chrome. It
 * is consistent with the styling of an in-content accessible highlighter.
 */
const ACCESSIBLE_BOUNDS_SHEET = "data:text/css;charset=utf-8," + encodeURIComponent(`
  .accessible-bounds {
    position: fixed;
    pointer-events: none;
    z-index: 10;
    display: block;
    background-color: #6a5acd!important;
    opacity: 0.6;
  }`);

/**
 * The XULWindowAccessibleHighlighter is a class that has the same API as the
 * AccessibleHighlighter, and by extension other highlighters that implement
 * auto-refresh highlighter, but instead of drawing in canvas frame anonymous
 * content (that is not available for chrome accessible highlighting) it adds a
 * transparrent inactionable element with the same position and bounds as the
 * accessible object highlighted. Unlike SimpleOutlineHighlighter, we can't use
 * element (that corresponds to accessible object) itself because the accessible
 * position and bounds are calculated differently.
 *
 * It is used when canvasframe-based AccessibleHighlighter can't be used. This
 * is the case for XUL windows.
 */
class XULWindowAccessibleHighlighter {
  constructor(highlighterEnv) {
    this.highlighterEnv = highlighterEnv;
    this.win = highlighterEnv.window;
  }

  /**
   * Static getter that indicates that XULWindowAccessibleHighlighter supports
   * highlighting in XUL windows.
   */
  static get XULSupported() {
    return true;
  }

  /**
   * Build highlighter markup.
   */
  _buildMarkup() {
    const doc = this.win.document;
    loadSheet(doc.ownerGlobal, ACCESSIBLE_BOUNDS_SHEET);

    this.container = createNode(this.win, {
      parent: doc.body || doc.documentElement,
      attributes: {
        "class": "highlighter-container",
        "role": "presentation"
      }
    });

    this.bounds = createNode(this.win, {
      parent: this.container,
      attributes: {
        "class": "accessible-bounds",
        "role": "presentation"
      }
    });
  }

  /**
   * Get current accessible bounds.
   *
   * @return {Object|null} Returns, if available, positioning and bounds
   *                       information for the accessible object.
   */
  get _bounds() {
    // Zoom level for the top level browser window does not change and only inner frames
    // do. So we need to get the zoom level of the current node's parent window.
    const zoom = getCurrentZoom(this.currentNode);

    return getBounds(this.win, { ...this.options, zoom });
  }

  /**
   * Show the highlighter on a given accessible.
   *
   * @param {DOMNode} node
   *        A dom node that corresponds to the accessible object.
   * @param {Object} options
   *        Object used for passing options. Available options:
   *         - {Number} x
   *           x coordinate of the top left corner of the accessible object
   *         - {Number} y
   *           y coordinate of the top left corner of the accessible object
   *         - {Number} w
   *           width of the the accessible object
   *         - {Number} h
   *           height of the the accessible object
   *         - duration {Number}
   *                    Duration of time that the highlighter should be shown.
   * @return {Boolean} True if accessible is highlighted, false otherwise.
   */
  show(node, options = {}) {
    const isSameNode = node === this.currentNode;
    const hasBounds = options && typeof options.x == "number" &&
                               typeof options.y == "number" &&
                               typeof options.w == "number" &&
                               typeof options.h == "number";
    if (!hasBounds || !isNodeValid(node) || isSameNode) {
      return false;
    }

    this.options = options;
    this.currentNode = node;

    return this._show();
  }

  /**
   * Internal show method that updates bounds and tracks duration based
   * highlighting.
   *
   * @return {Boolean} True if accessible is highlighted, false otherwise.
   */
  _show() {
    if (this._highlightTimer) {
      clearTimeout(this._highlightTimer);
      this._highlightTimer = null;
    }

    const shown = this._update();
    const { duration } = this.options;
    if (shown && duration) {
      this._highlightTimer = setTimeout(() => {
        this._hideAccessibleBounds();
      }, duration);
    }
    return shown;
  }

  /**
   * Update accessible bounds for a current accessible. Re-draw highlighter
   * markup.
   *
   * @return {Boolean} True if accessible is highlighted, false otherwise.
   */
  _update() {
    this._hideAccessibleBounds();
    const bounds = this._bounds;
    if (!bounds) {
      return false;
    }

    let boundsEl = this.bounds;
    if (!boundsEl) {
      this._buildMarkup();
      boundsEl = this.bounds;
    }

    const { left, top, width, height } = bounds;
    boundsEl.style.top = `${top}px`;
    boundsEl.style.left = `${left}px`;
    boundsEl.style.width = `${width}px`;
    boundsEl.style.height = `${height}px`;
    this._showAccessibleBounds();

    return true;
  }

  /**
   * Hide the highlighter
   */
  hide() {
    if (!this.currentNode || !this.highlighterEnv.window) {
      return;
    }

    this._hideAccessibleBounds();
    this.currentNode = null;
    this.options = null;
  }

  /**
   * Show accessible bounds highlighter.
   */
  _showAccessibleBounds() {
    if (this.container) {
      this.container.removeAttribute("hidden");
    }
  }

  /**
   * Hide accessible bounds highlighter.
   */
  _hideAccessibleBounds() {
    if (this.container) {
      this.container.setAttribute("hidden", "true");
    }
  }

  /**
   * Hide accessible highlighter, clean up and remove the markup.
   */
  destroy() {
    if (this._highlightTimer) {
      clearTimeout(this._highlightTimer);
      this._highlightTimer = null;
    }

    this.hide();
    if (this.container) {
      this.container.remove();
    }

    this.win = null;
  }
}

exports.XULWindowAccessibleHighlighter = XULWindowAccessibleHighlighter;
