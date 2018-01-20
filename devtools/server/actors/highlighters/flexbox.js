/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AutoRefreshHighlighter } = require("./auto-refresh");
const {
  CANVAS_SIZE,
  DEFAULT_COLOR,
  clearRect,
  drawRect,
  getCurrentMatrix,
  updateCanvasElement,
  updateCanvasPosition,
} = require("./utils/canvas");
const {
  CanvasFrameAnonymousContentHelper,
  createNode,
  getComputedStyle,
} = require("./utils/markup");
const {
  getAdjustedQuads,
  getDisplayPixelRatio,
  setIgnoreLayoutChanges,
} = require("devtools/shared/layout/utils");

const FLEXBOX_LINES_PROPERTIES = {
  "edge": {
    lineDash: [2, 2]
  },
  "item": {
    lineDash: [0, 0]
  }
};

const FLEXBOX_CONTAINER_PATTERN_WIDTH = 14; // px
const FLEXBOX_CONTAINER_PATTERN_HEIGHT = 14; // px
const FLEXBOX_CONTAINER_PATTERN_LINE_DISH = [5, 3]; // px
const BASIS_FILL_COLOR = "rgb(109, 184, 255, 0.4)";
const BASIS_EDGE_COLOR = "#6aabed";

/**
 * Cached used by `FlexboxHighlighter.getFlexContainerPattern`.
 */
const gCachedFlexboxPattern = new Map();

const FLEXBOX = "flexbox";

class FlexboxHighlighter extends AutoRefreshHighlighter {
  constructor(highlighterEnv) {
    super(highlighterEnv);

    this.ID_CLASS_PREFIX = "flexbox-";

    this.markup = new CanvasFrameAnonymousContentHelper(this.highlighterEnv,
      this._buildMarkup.bind(this));

    this.onPageHide = this.onPageHide.bind(this);
    this.onWillNavigate = this.onWillNavigate.bind(this);

    this.highlighterEnv.on("will-navigate", this.onWillNavigate);

    let { pageListenerTarget } = highlighterEnv;
    pageListenerTarget.addEventListener("pagehide", this.onPageHide);

    // Initialize the <canvas> position to the top left corner of the page
    this._canvasPosition = {
      x: 0,
      y: 0
    };

    // Calling `updateCanvasPosition` anyway since the highlighter could be initialized
    // on a page that has scrolled already.
    updateCanvasPosition(this._canvasPosition, this._scroll, this.win,
      this._winDimensions);
  }

  _buildMarkup() {
    let container = createNode(this.win, {
      attributes: {
        "class": "highlighter-container"
      }
    });

    let root = createNode(this.win, {
      parent: container,
      attributes: {
        "id": "root",
        "class": "root"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // We use a <canvas> element because there is an arbitrary number of items and texts
    // to draw which wouldn't be possible with HTML or SVG without having to insert and
    // remove the whole markup on every update.
    createNode(this.win, {
      parent: root,
      nodeType: "canvas",
      attributes: {
        "id": "canvas",
        "class": "canvas",
        "hidden": "true",
        "width": CANVAS_SIZE,
        "height": CANVAS_SIZE
      },
      prefix: this.ID_CLASS_PREFIX
    });

    return container;
  }

  clearCache() {
    gCachedFlexboxPattern.clear();
  }

  destroy() {
    let { highlighterEnv } = this;
    highlighterEnv.off("will-navigate", this.onWillNavigate);

    let { pageListenerTarget } = highlighterEnv;
    if (pageListenerTarget) {
      pageListenerTarget.removeEventListener("pagehide", this.onPageHide);
    }

    this.markup.destroy();

    // Clear the pattern cache to avoid dead object exceptions (Bug 1342051).
    this.clearCache();
    AutoRefreshHighlighter.prototype.destroy.call(this);
  }

  get canvas() {
    return this.getElement("canvas");
  }

  get ctx() {
    return this.canvas.getCanvasContext("2d");
  }

  getElement(id) {
    return this.markup.getElement(this.ID_CLASS_PREFIX + id);
  }

  /**
  * Gets the flexbox container pattern used to render the container regions.
  *
  * @param  {Number} devicePixelRatio
  *         The device pixel ratio we want the pattern for.
  * @return {CanvasPattern} flex container pattern.
  */
  getFlexContainerPattern(devicePixelRatio) {
    let flexboxPatternMap = null;

    if (gCachedFlexboxPattern.has(devicePixelRatio)) {
      flexboxPatternMap = gCachedFlexboxPattern.get(devicePixelRatio);
    } else {
      flexboxPatternMap = new Map();
    }

    if (gCachedFlexboxPattern.has(FLEXBOX)) {
      return gCachedFlexboxPattern.get(FLEXBOX);
    }

    // Create the diagonal lines pattern for the rendering the flexbox gaps.
    let canvas = createNode(this.win, { nodeType: "canvas" });
    let width = canvas.width = FLEXBOX_CONTAINER_PATTERN_WIDTH * devicePixelRatio;
    let height = canvas.height = FLEXBOX_CONTAINER_PATTERN_HEIGHT * devicePixelRatio;

    let ctx = canvas.getContext("2d");
    ctx.save();
    ctx.setLineDash(FLEXBOX_CONTAINER_PATTERN_LINE_DISH);
    ctx.beginPath();
    ctx.translate(.5, .5);

    ctx.moveTo(0, 0);
    ctx.lineTo(width, height);

    ctx.strokeStyle = DEFAULT_COLOR;
    ctx.stroke();
    ctx.restore();

    let pattern = ctx.createPattern(canvas, "repeat");
    flexboxPatternMap.set(FLEXBOX, pattern);
    gCachedFlexboxPattern.set(devicePixelRatio, flexboxPatternMap);

    return pattern;
  }

  /**
   * The AutoRefreshHighlighter's _hasMoved method returns true only if the
   * element's quads have changed. Override it so it also returns true if the
   * element and its flex items have changed.
   */
  _hasMoved() {
    let hasMoved = AutoRefreshHighlighter.prototype._hasMoved.call(this);

    // TODO: Implement a check of old and new flex container and flex items to react
    // to any alignment and size changes. This is blocked on Bug 1414920 that implements
    // a platform API to retrieve the flex container and flex item information.

    return hasMoved;
  }

  _hide() {
    setIgnoreLayoutChanges(true);
    this._hideFlexbox();
    setIgnoreLayoutChanges(false, this.highlighterEnv.document.documentElement);
  }

  _hideFlexbox() {
    this.getElement("canvas").setAttribute("hidden", "true");
  }

  /**
   * The <canvas>'s position needs to be updated if the page scrolls too much, in order
   * to give the illusion that it always covers the viewport.
   */
  _scrollUpdate() {
    let hasUpdated = updateCanvasPosition(this._canvasPosition, this._scroll, this.win,
      this._winDimensions);

    if (hasUpdated) {
      this._update();
    }
  }

  _show() {
    this._hide();
    return this._update();
  }

  _showFlexbox() {
    this.getElement("canvas").removeAttribute("hidden");
  }

  /**
   * If a page hide event is triggered for current window's highlighter, hide the
   * highlighter.
   */
  onPageHide({ target }) {
    if (target.defaultView === this.win) {
      this.hide();
    }
  }

  /**
   * Called when the page will-navigate. Used to hide the flexbox highlighter and clear
   * the cached gap patterns and avoid using DeadWrapper obejcts as gap patterns the
   * next time.
   */
  onWillNavigate({ isTopLevel }) {
    this.clearCache();

    if (isTopLevel) {
      this.hide();
    }
  }

  renderFlexContainerBorder() {
    if (!this.currentQuads.content || !this.currentQuads.content[0]) {
      return;
    }

    let { devicePixelRatio } = this.win;
    let lineWidth = getDisplayPixelRatio(this.win) * 2;
    let offset = (lineWidth / 2) % 1;
    let canvasX = Math.round(this._canvasPosition.x * devicePixelRatio);
    let canvasY = Math.round(this._canvasPosition.y * devicePixelRatio);

    this.ctx.save();
    this.ctx.translate(offset - canvasX, offset - canvasY);
    this.ctx.setLineDash(FLEXBOX_LINES_PROPERTIES.edge.lineDash);
    this.ctx.lineWidth = lineWidth;
    this.ctx.strokeStyle = DEFAULT_COLOR;

    let { bounds } = this.currentQuads.content[0];
    drawRect(this.ctx, 0, 0, bounds.width, bounds.height, this.currentMatrix);

    this.ctx.stroke();
    this.ctx.restore();
  }

  renderFlexContainerFill() {
    if (!this.currentQuads.content || !this.currentQuads.content[0]) {
      return;
    }

    let { devicePixelRatio } = this.win;
    let lineWidth = getDisplayPixelRatio(this.win);
    let offset = (lineWidth / 2) % 1;
    let canvasX = Math.round(this._canvasPosition.x * devicePixelRatio);
    let canvasY = Math.round(this._canvasPosition.y * devicePixelRatio);

    this.ctx.save();
    this.ctx.translate(offset - canvasX, offset - canvasY);
    this.ctx.setLineDash(FLEXBOX_LINES_PROPERTIES.edge.lineDash);
    this.ctx.lineWidth = 0;
    this.ctx.strokeStyle = DEFAULT_COLOR;
    this.ctx.fillStyle = this.getFlexContainerPattern(devicePixelRatio);

    let { bounds } = this.currentQuads.content[0];
    drawRect(this.ctx, 0, 0, bounds.width, bounds.height, this.currentMatrix);

    this.ctx.fill();
    this.ctx.stroke();
    this.ctx.restore();
  }

  /**
   * Renders the flex basis for a given flex item.
   */
  renderFlexItemBasis(flexItem, left, top, right, bottom, boundsWidth) {
    let computedStyle = getComputedStyle(flexItem);
    let basis = computedStyle.getPropertyValue("flex-basis");

    if (basis.endsWith("px")) {
      right = left + parseFloat(basis);
    } else if (basis.endsWith("%")) {
      basis = parseFloat(basis) / 100 * boundsWidth;
      right = left + basis;
    }

    this.ctx.fillStyle = BASIS_FILL_COLOR;
    this.ctx.strokeStyle = BASIS_EDGE_COLOR;
    drawRect(this.ctx, left, top, right, bottom, this.currentMatrix);
    this.ctx.stroke();
    this.ctx.fill();
  }

  renderFlexItems() {
    if (!this.currentQuads.content || !this.currentQuads.content[0]) {
      return;
    }

    let { devicePixelRatio } = this.win;
    let lineWidth = getDisplayPixelRatio(this.win);
    let offset = (lineWidth / 2) % 1;
    let canvasX = Math.round(this._canvasPosition.x * devicePixelRatio);
    let canvasY = Math.round(this._canvasPosition.y * devicePixelRatio);

    this.ctx.save();
    this.ctx.translate(offset - canvasX, offset - canvasY);
    this.ctx.setLineDash(FLEXBOX_LINES_PROPERTIES.item.lineDash);
    this.ctx.lineWidth = lineWidth;
    this.ctx.strokeStyle = DEFAULT_COLOR;

    let { bounds } = this.currentQuads.content[0];
    let flexItems = this.currentNode.children;

    // TODO: Utilize the platform API that will be implemented in Bug 1414290 to
    // retrieve the flex item properties.
    for (let flexItem of flexItems) {
      let quads = getAdjustedQuads(this.win, flexItem, "border");
      if (!quads.length) {
        continue;
      }

      // Adjust the flex item bounds relative to the current quads.
      let { bounds: flexItemBounds } = quads[0];
      let left = flexItemBounds.left - bounds.left;
      let top = flexItemBounds.top - bounds.top;
      let right = flexItemBounds.right - bounds.left;
      let bottom = flexItemBounds.bottom - bounds.top;

      clearRect(this.ctx, left, top, right, bottom, this.currentMatrix);
      drawRect(this.ctx, left, top, right, bottom, this.currentMatrix);
      this.ctx.stroke();

      this.renderFlexItemBasis(flexItem, left, top, right, bottom, bounds.width);
    }

    this.ctx.restore();
  }

  renderFlexLines() {
    if (!this.currentQuads.content || !this.currentQuads.content[0]) {
      return;
    }

    let { devicePixelRatio } = this.win;
    let lineWidth = getDisplayPixelRatio(this.win);
    let offset = (lineWidth / 2) % 1;
    let canvasX = Math.round(this._canvasPosition.x * devicePixelRatio);
    let canvasY = Math.round(this._canvasPosition.y * devicePixelRatio);

    this.ctx.save();
    this.ctx.translate(offset - canvasX, offset - canvasY);
    this.ctx.lineWidth = lineWidth;
    this.ctx.strokeStyle = DEFAULT_COLOR;

    let { bounds } = this.currentQuads.content[0];
    let computedStyle = getComputedStyle(this.currentNode);
    let flexLines = this.currentNode.getAsFlexContainer().getLines();

    for (let flexLine of flexLines) {
      let { crossStart, crossSize } = flexLine;

      if (computedStyle.getPropertyValue("flex-direction") === "column" ||
          computedStyle.getPropertyValue("flex-direction") === "column-reverse") {
        clearRect(this.ctx, crossStart, 0, crossStart + crossSize, bounds.height,
          this.currentMatrix);
        drawRect(this.ctx, crossStart, 0, crossStart, bounds.height, this.currentMatrix);
        this.ctx.stroke();
        drawRect(this.ctx, crossStart + crossSize, 0, crossStart + crossSize,
          bounds.height, this.currentMatrix);
        this.ctx.stroke();
      } else {
        clearRect(this.ctx, 0, crossStart, bounds.width, crossStart + crossSize,
          this.currentMatrix);
        drawRect(this.ctx, 0, crossStart, bounds.width, crossStart, this.currentMatrix);
        this.ctx.stroke();
        drawRect(this.ctx, 0, crossStart + crossSize, bounds.width,
          crossStart + crossSize, this.currentMatrix);
        this.ctx.stroke();
      }
    }

    this.ctx.restore();
  }

  _update() {
    setIgnoreLayoutChanges(true);

    let root = this.getElement("root");

    // Hide the root element and force the reflow in order to get the proper window's
    // dimensions without increasing them.
    root.setAttribute("style", "display: none");
    this.win.document.documentElement.offsetWidth;

    let { width, height } = this._winDimensions;

    // Updates the <canvas> element's position and size.
    // It also clear the <canvas>'s drawing context.
    updateCanvasElement(this.canvas, this._canvasPosition, this.win.devicePixelRatio);

    // Update the current matrix used in our canvas' rendering
    let { currentMatrix, hasNodeTransformations } = getCurrentMatrix(this.currentNode,
      this.win);
    this.currentMatrix = currentMatrix;
    this.hasNodeTransformations = hasNodeTransformations;

    this.renderFlexContainerFill();
    this.renderFlexLines();
    this.renderFlexItems();
    this.renderFlexContainerBorder();

    this._showFlexbox();

    root.setAttribute("style",
      `position: absolute; width: ${width}px; height: ${height}px; overflow: hidden`);

    setIgnoreLayoutChanges(false, this.highlighterEnv.document.documentElement);
    return true;
  }
}

exports.FlexboxHighlighter = FlexboxHighlighter;
