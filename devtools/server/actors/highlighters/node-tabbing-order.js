/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(
  this,
  ["setIgnoreLayoutChanges", "getCurrentZoom"],
  "resource://devtools/shared/layout/utils.js",
  true
);
loader.lazyRequireGetter(
  this,
  "AutoRefreshHighlighter",
  "resource://devtools/server/actors/highlighters/auto-refresh.js",
  true
);
loader.lazyRequireGetter(
  this,
  ["CanvasFrameAnonymousContentHelper"],
  "resource://devtools/server/actors/highlighters/utils/markup.js",
  true
);

/**
 * The NodeTabbingOrderHighlighter draws an outline around a node (based on its
 * border bounds).
 *
 * Usage example:
 *
 * const h = new NodeTabbingOrderHighlighter(env);
 * await h.isReady();
 * h.show(node, options);
 * h.hide();
 * h.destroy();
 *
 * @param {Number} options.index
 *        Tabbing index value to be displayed in the highlighter info bar.
 */
class NodeTabbingOrderHighlighter extends AutoRefreshHighlighter {
  constructor(highlighterEnv) {
    super(highlighterEnv);

    this._doNotStartRefreshLoop = true;
    this.ID_CLASS_PREFIX = "tabbing-order-";
    this.markup = new CanvasFrameAnonymousContentHelper(
      this.highlighterEnv,
      this._buildMarkup.bind(this)
    );
    this.isReady = this.markup.initialize();
  }

  _buildMarkup() {
    const root = this.markup.createNode({
      attributes: {
        id: "root",
        class: "root highlighter-container tabbing-order",
        "aria-hidden": "true",
      },
      prefix: this.ID_CLASS_PREFIX,
    });

    const container = this.markup.createNode({
      parent: root,
      attributes: {
        id: "container",
        width: "100%",
        height: "100%",
        hidden: "true",
      },
      prefix: this.ID_CLASS_PREFIX,
    });

    // Building the SVG element
    this.markup.createNode({
      parent: container,
      attributes: {
        class: "bounds",
        id: "bounds",
      },
      prefix: this.ID_CLASS_PREFIX,
    });

    // Building the nodeinfo bar markup

    const infobarContainer = this.markup.createNode({
      parent: root,
      attributes: {
        class: "infobar-container",
        id: "infobar-container",
        position: "top",
        hidden: "true",
      },
      prefix: this.ID_CLASS_PREFIX,
    });

    const infobar = this.markup.createNode({
      parent: infobarContainer,
      attributes: {
        class: "infobar",
      },
      prefix: this.ID_CLASS_PREFIX,
    });

    this.markup.createNode({
      parent: infobar,
      attributes: {
        class: "infobar-text",
        id: "infobar-text",
      },
      prefix: this.ID_CLASS_PREFIX,
    });

    return root;
  }

  /**
   * Destroy the nodes. Remove listeners.
   */
  destroy() {
    this.markup.destroy();

    AutoRefreshHighlighter.prototype.destroy.call(this);
  }

  getElement(id) {
    return this.markup.getElement(this.ID_CLASS_PREFIX + id);
  }

  /**
   * Update focused styling for a node tabbing index highlight.
   *
   * @param {Boolean} focused
   *        Indicates if the highlighted node needs to be focused.
   */
  updateFocus(focused) {
    const root = this.getElement("root");
    root.classList.toggle("focused", focused);
  }

  /**
   * Show the highlighter on a given node
   */
  _show() {
    return this._update();
  }

  /**
   * Update the highlighter on the current highlighted node (the one that was
   * passed as an argument to show(node)).
   * Should be called whenever node size or attributes change
   */
  _update() {
    let shown = false;
    setIgnoreLayoutChanges(true);

    if (this._updateTabbingOrder()) {
      this._showInfobar();
      this._showTabbingOrder();
      shown = true;
      setIgnoreLayoutChanges(
        false,
        this.highlighterEnv.window.document.documentElement
      );
    } else {
      // Nothing to highlight (0px rectangle like a <script> tag for instance)
      this._hide();
    }

    return shown;
  }

  /**
   * Hide the highlighter, the outline and the infobar.
   */
  _hide() {
    setIgnoreLayoutChanges(true);

    this._hideTabbingOrder();
    this._hideInfobar();

    setIgnoreLayoutChanges(
      false,
      this.highlighterEnv.window.document.documentElement
    );
  }

  /**
   * Hide the infobar
   */
  _hideInfobar() {
    this.getElement("infobar-container").setAttribute("hidden", "true");
  }

  /**
   * Show the infobar
   */
  _showInfobar() {
    if (!this.currentNode) {
      return;
    }

    this.getElement("infobar-container").removeAttribute("hidden");
    this.getElement("infobar-text").setTextContent(this.options.index);
    const bounds = this._getBounds();
    const container = this.getElement("infobar-container");

    moveInfobar(container, bounds, this.win);
  }

  /**
   * Hide the tabbing order highlighter
   */
  _hideTabbingOrder() {
    this.getElement("container").setAttribute("hidden", "true");
  }

  /**
   * Show the tabbing order highlighter
   */
  _showTabbingOrder() {
    this.getElement("container").removeAttribute("hidden");
  }

  /**
   * Calculate border bounds based on the quads returned by getAdjustedQuads.
   * @return {Object} A bounds object {bottom,height,left,right,top,width,x,y}
   */
  _getBorderBounds() {
    const quads = this.currentQuads.border;
    if (!quads || !quads.length) {
      return null;
    }

    const bounds = {
      bottom: -Infinity,
      height: 0,
      left: Infinity,
      right: -Infinity,
      top: Infinity,
      width: 0,
      x: 0,
      y: 0,
    };

    for (const q of quads) {
      bounds.bottom = Math.max(bounds.bottom, q.bounds.bottom);
      bounds.top = Math.min(bounds.top, q.bounds.top);
      bounds.left = Math.min(bounds.left, q.bounds.left);
      bounds.right = Math.max(bounds.right, q.bounds.right);
    }
    bounds.x = bounds.left;
    bounds.y = bounds.top;
    bounds.width = bounds.right - bounds.left;
    bounds.height = bounds.bottom - bounds.top;

    return bounds;
  }

  /**
   * Update the tabbing order index as per the current node.
   *
   * @return {boolean}
   *         True if the current node has a tabbing order index to be
   *         highlighted
   */
  _updateTabbingOrder() {
    if (!this._nodeNeedsHighlighting()) {
      this._hideTabbingOrder();
      return false;
    }

    const boundsEl = this.getElement("bounds");
    const { left, top, width, height } = this._getBounds();
    boundsEl.setAttribute(
      "style",
      `top: ${top}px; left: ${left}px; width: ${width}px; height: ${height}px;`
    );

    // Un-zoom the root wrapper if the page was zoomed.
    const rootId = this.ID_CLASS_PREFIX + "container";
    this.markup.scaleRootElement(this.currentNode, rootId);

    return true;
  }

  /**
   * Can the current node be highlighted? Does it have quads.
   * @return {Boolean}
   */
  _nodeNeedsHighlighting() {
    return (
      this.currentQuads.margin.length ||
      this.currentQuads.border.length ||
      this.currentQuads.padding.length ||
      this.currentQuads.content.length
    );
  }

  _getBounds() {
    const borderBounds = this._getBorderBounds();
    let bounds = {
      bottom: 0,
      height: 0,
      left: 0,
      right: 0,
      top: 0,
      width: 0,
      x: 0,
      y: 0,
    };

    if (!borderBounds) {
      // Invisible element such as a script tag.
      return bounds;
    }

    const { bottom, height, left, right, top, width, x, y } = borderBounds;
    if (width > 0 || height > 0) {
      bounds = { bottom, height, left, right, top, width, x, y };
    }

    return bounds;
  }
}

/**
 * Move the infobar to the right place in the highlighter. The infobar is used
 * to display element's tabbing order index.
 *
 * @param  {DOMNode} container
 *         The container element which will be used to position the infobar.
 * @param  {Object} bounds
 *         The content bounds of the container element.
 * @param  {Window} win
 *         The window object.
 */
function moveInfobar(container, bounds, win) {
  const zoom = getCurrentZoom(win);
  const { computedStyle } = container;
  const margin = 2;
  const arrowSize =
    parseFloat(
      computedStyle.getPropertyValue("--highlighter-bubble-arrow-size")
    ) - 2;
  const containerHeight = parseFloat(computedStyle.getPropertyValue("height"));
  const containerWidth = parseFloat(computedStyle.getPropertyValue("width"));

  const topBoundary = margin;
  const bottomBoundary =
    win.document.scrollingElement.scrollHeight - containerHeight - margin - 1;
  const leftBoundary = containerWidth / 2 + margin;

  let top = bounds.y - containerHeight - arrowSize;
  let left = bounds.x + bounds.width / 2;
  const bottom = bounds.bottom + arrowSize;
  let positionAttribute = "top";

  const canBePlacedOnTop = top >= topBoundary;
  const canBePlacedOnBottom = bottomBoundary - bottom > 0;

  if (!canBePlacedOnTop && canBePlacedOnBottom) {
    top = bottom;
    positionAttribute = "bottom";
  }

  let hideArrow = false;
  if (top < topBoundary) {
    hideArrow = true;
    top = topBoundary;
  } else if (top > bottomBoundary) {
    hideArrow = true;
    top = bottomBoundary;
  }

  if (left < leftBoundary) {
    hideArrow = true;
    left = leftBoundary;
  }

  if (hideArrow) {
    container.setAttribute("hide-arrow", "true");
  } else {
    container.removeAttribute("hide-arrow");
  }

  container.setAttribute(
    "style",
    `
     position: absolute;
     transform-origin: 0 0;
     transform: scale(${1 / zoom}) translate(calc(${left}px - 50%), ${top}px)`
  );

  container.setAttribute("position", positionAttribute);
}

exports.NodeTabbingOrderHighlighter = NodeTabbingOrderHighlighter;
