/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("resource://devtools/shared/event-emitter.js");
const {
  isNodeValid,
} = require("resource://devtools/server/actors/highlighters/utils/markup.js");
const {
  getAdjustedQuads,
  getWindowDimensions,
} = require("resource://devtools/shared/layout/utils.js");

// Note that the order of items in this array is important because it is used
// for drawing the BoxModelHighlighter's path elements correctly.
const BOX_MODEL_REGIONS = ["margin", "border", "padding", "content"];
const QUADS_PROPS = ["p1", "p2", "p3", "p4"];

function arePointsDifferent(pointA, pointB) {
  return (
    Math.abs(pointA.x - pointB.x) >= 0.5 ||
    Math.abs(pointA.y - pointB.y) >= 0.5 ||
    Math.abs(pointA.w - pointB.w) >= 0.5
  );
}

function areQuadsDifferent(oldQuads, newQuads) {
  for (const region of BOX_MODEL_REGIONS) {
    const { length } = oldQuads[region];

    if (length !== newQuads[region].length) {
      return true;
    }

    for (let i = 0; i < length; i++) {
      for (const prop of QUADS_PROPS) {
        const oldPoint = oldQuads[region][i][prop];
        const newPoint = newQuads[region][i][prop];

        if (arePointsDifferent(oldPoint, newPoint)) {
          return true;
        }
      }
    }
  }

  return false;
}

/**
 * Base class for auto-refresh-on-change highlighters. Sub classes will have a
 * chance to update whenever the current node's geometry changes.
 *
 * Sub classes must implement the following methods:
 * _show: called when the highlighter should be shown,
 * _hide: called when the highlighter should be hidden,
 * _update: called while the highlighter is shown and the geometry of the
 *          current node changes.
 *
 * Sub classes will have access to the following properties:
 * - this.currentNode: the node to be shown
 * - this.currentQuads: all of the node's box model region quads
 * - this.win: the current window
 *
 * Emits the following events:
 * - shown
 * - hidden
 * - updated
 */
class AutoRefreshHighlighter extends EventEmitter {
  constructor(highlighterEnv) {
    super();

    this.highlighterEnv = highlighterEnv;

    this._updateSimpleHighlighters = this._updateSimpleHighlighters.bind(this);
    this.highlighterEnv.on(
      "use-simple-highlighters-updated",
      this._updateSimpleHighlighters
    );

    this.currentNode = null;
    this.currentQuads = {};

    this._winDimensions = getWindowDimensions(this.win);
    this._scroll = { x: this.win.pageXOffset, y: this.win.pageYOffset };

    this.update = this.update.bind(this);
  }

  _ignoreZoom = false;
  _ignoreScroll = false;

  /**
   * Window corresponding to the current highlighterEnv.
   */
  get win() {
    if (!this.highlighterEnv) {
      return null;
    }
    return this.highlighterEnv.window;
  }

  /* Window containing the target content. */
  get contentWindow() {
    return this.win;
  }

  get supportsSimpleHighlighters() {
    return false;
  }

  /**
   * Show the highlighter on a given node
   * @param {DOMNode} node
   * @param {Object} options
   *        Object used for passing options
   */
  show(node, options = {}) {
    const isSameNode = node === this.currentNode;
    const isSameOptions = this._isSameOptions(options);

    if (!this._isNodeValid(node) || (isSameNode && isSameOptions)) {
      return false;
    }

    this.options = options;

    this._stopRefreshLoop();
    this.currentNode = node;

    // For offset-path, the highlighter needs to be computed from the containing block
    // of the node, not the node itself.
    this.useContainingBlock = this.options.mode === "cssOffsetPath";
    this.drawingNode = this.useContainingBlock
      ? InspectorUtils.containingBlockOf(this.currentNode)
      : this.currentNode;

    this._updateAdjustedQuads();
    this._startRefreshLoop();

    const shown = this._show();
    if (shown) {
      this.emit("shown");
    }
    return shown;
  }

  /**
   * Hide the highlighter
   */
  hide() {
    if (!this.currentNode || !this.highlighterEnv.window) {
      return;
    }

    this._hide();
    this._stopRefreshLoop();
    this.currentNode = null;
    this.currentQuads = {};
    this.options = null;

    this.emit("hidden");
  }

  /**
   * Whether the current node is valid for this highlighter type.
   * This is implemented by default to check if the node is an element node. Highlighter
   * sub-classes should override this method if they want to highlight other node types.
   * @param {DOMNode} node
   * @return {Boolean}
   */
  _isNodeValid(node) {
    return isNodeValid(node);
  }

  /**
   * Are the provided options the same as the currently stored options?
   * Returns false if there are no options stored currently.
   */
  _isSameOptions(options) {
    if (!this.options) {
      return false;
    }

    const keys = Object.keys(options);

    if (keys.length !== Object.keys(this.options).length) {
      return false;
    }

    for (const key of keys) {
      if (this.options[key] !== options[key]) {
        return false;
      }
    }

    return true;
  }

  /**
   * Update the stored box quads by reading the current node's box quads.
   */
  _updateAdjustedQuads() {
    this.currentQuads = {};

    // If we need to use the containing block, and if it is the <html> element,
    // we need to use the viewport quads.
    const useViewport =
      this.useContainingBlock &&
      this.drawingNode === this.currentNode.ownerDocument.documentElement;
    const node = useViewport
      ? this.drawingNode.ownerDocument
      : this.drawingNode;

    for (const region of BOX_MODEL_REGIONS) {
      this.currentQuads[region] = getAdjustedQuads(
        this.contentWindow,
        node,
        region,
        { ignoreScroll: this._ignoreScroll, ignoreZoom: this._ignoreZoom }
      );
    }
  }

  /**
   * Update the knowledge we have of the current node's boxquads and return true
   * if any of the points x/y or bounds have change since.
   * @return {Boolean}
   */
  _hasMoved() {
    const oldQuads = this.currentQuads;
    this._updateAdjustedQuads();

    return areQuadsDifferent(oldQuads, this.currentQuads);
  }

  /**
   * Update the knowledge we have of the current window's scrolling offset, both
   * horizontal and vertical, and return `true` if they have changed since.
   * @return {Boolean}
   */
  _hasWindowScrolled() {
    if (!this.win) {
      return false;
    }

    const { pageXOffset, pageYOffset } = this.win;
    const hasChanged =
      this._scroll.x !== pageXOffset || this._scroll.y !== pageYOffset;

    this._scroll = { x: pageXOffset, y: pageYOffset };

    return hasChanged;
  }

  /**
   * Update the knowledge we have of the current window's dimensions and return `true`
   * if they have changed since.
   * @return {Boolean}
   */
  _haveWindowDimensionsChanged() {
    const { width, height } = getWindowDimensions(this.win);
    const haveChanged =
      this._winDimensions.width !== width ||
      this._winDimensions.height !== height;

    this._winDimensions = { width, height };
    return haveChanged;
  }

  /**
   * Update the highlighter if the node has moved since the last update.
   */
  update() {
    if (
      !this._isNodeValid(this.currentNode) ||
      (!this._hasMoved() && !this._haveWindowDimensionsChanged())
    ) {
      // At this point we're not calling the `_update` method. However, if the window has
      // scrolled, we want to invoke `_scrollUpdate`.
      if (this._hasWindowScrolled()) {
        this._scrollUpdate();
      }

      return;
    }

    this._update();
    this.emit("updated");
  }

  _show() {
    // To be implemented by sub classes
    // When called, sub classes should actually show the highlighter for
    // this.currentNode, potentially using options in this.options
    throw new Error("Custom highlighter class had to implement _show method");
  }

  _update() {
    // To be implemented by sub classes
    // When called, sub classes should update the highlighter shown for
    // this.currentNode
    // This is called as a result of a page zoom or repaint
    throw new Error("Custom highlighter class had to implement _update method");
  }

  _scrollUpdate() {
    // Can be implemented by sub classes
    // When called, sub classes can upate the highlighter shown for
    // this.currentNode
    // This is called as a result of a page scroll
  }

  _hide() {
    // To be implemented by sub classes
    // When called, sub classes should actually hide the highlighter
    throw new Error("Custom highlighter class had to implement _hide method");
  }

  _startRefreshLoop() {
    const win = this.currentNode.ownerGlobal;
    this.rafID = win.requestAnimationFrame(this._startRefreshLoop.bind(this));
    this.rafWin = win;
    this.update();
  }

  _stopRefreshLoop() {
    if (this.rafID && !Cu.isDeadWrapper(this.rafWin)) {
      this.rafWin.cancelAnimationFrame(this.rafID);
    }
    this.rafID = this.rafWin = null;
  }

  _updateSimpleHighlighters() {
    if (!this.supportsSimpleHighlighters) {
      return;
    }

    const root = this.getElement("root");
    if (!root) {
      // Highlighters which support simple highlighters are expected to use a
      // root element with the id "root".
      return;
    }

    // Add/remove the `user-simple-highlighters` class based on the current
    // toolbox configuration.
    root.classList.toggle(
      "use-simple-highlighters",
      this.highlighterEnv.useSimpleHighlightersForReducedMotion
    );
  }

  destroy() {
    this.hide();

    this.highlighterEnv.off(
      "use-simple-highlighters-updated",
      this._updateSimpleHighlighters
    );
    this.highlighterEnv = null;
    this.currentNode = null;
  }
}
exports.AutoRefreshHighlighter = AutoRefreshHighlighter;
