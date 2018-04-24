/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AutoRefreshHighlighter } = require("./auto-refresh");
const { apply } = require("devtools/shared/layout/dom-matrix-2d");
const {
  CANVAS_SIZE,
  DEFAULT_COLOR,
  clearRect,
  drawLine,
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
    lineDash: [12, 10]
  },
  "item": {
    lineDash: [0, 0]
  },
  "alignItems": {
    lineDash: [0, 0]
  }
};

const FLEXBOX_CONTAINER_PATTERN_WIDTH = 14; // px
const FLEXBOX_CONTAINER_PATTERN_HEIGHT = 14; // px
const FLEXBOX_JUSTIFY_CONTENT_PATTERN_WIDTH = 7; // px
const FLEXBOX_JUSTIFY_CONTENT_PATTERN_HEIGHT = 7; // px
const FLEXBOX_CONTAINER_PATTERN_LINE_DISH = [5, 3]; // px
const BASIS_FILL_COLOR = "rgb(109, 184, 255, 0.4)";

/**
 * Cached used by `FlexboxHighlighter.getFlexContainerPattern`.
 */
const gCachedFlexboxPattern = new Map();

const FLEXBOX = "flexbox";
const JUSTIFY_CONTENT = "justify-content";

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

  /**
   * Draw the justify content for a given flex item (left, top, right, bottom) position.
   */
  drawJustifyContent(left, top, right, bottom) {
    let { devicePixelRatio } = this.win;
    this.ctx.fillStyle = this.getJustifyContentPattern(devicePixelRatio);
    drawRect(this.ctx, left, top, right, bottom, this.currentMatrix);
    this.ctx.fill();
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
  * Gets the flexbox justify content pattern used to render the justify content regions.
  *
  * @param  {Number} devicePixelRatio
  *         The device pixel ratio we want the pattern for.
  * @return {CanvasPattern} flex justify content pattern.
  */
  getJustifyContentPattern(devicePixelRatio) {
    let flexboxPatternMap = null;

    if (gCachedFlexboxPattern.has(devicePixelRatio)) {
      flexboxPatternMap = gCachedFlexboxPattern.get(devicePixelRatio);
    } else {
      flexboxPatternMap = new Map();
    }

    if (gCachedFlexboxPattern.has(JUSTIFY_CONTENT)) {
      return gCachedFlexboxPattern.get(JUSTIFY_CONTENT);
    }

    // Create the inversed diagonal lines pattern
    // for the rendering the justify content gaps.
    let canvas = createNode(this.win, { nodeType: "canvas" });
    let width = canvas.width = FLEXBOX_JUSTIFY_CONTENT_PATTERN_WIDTH * devicePixelRatio;
    let height = canvas.height = FLEXBOX_JUSTIFY_CONTENT_PATTERN_HEIGHT *
      devicePixelRatio;

    let ctx = canvas.getContext("2d");
    ctx.save();
    ctx.setLineDash(FLEXBOX_CONTAINER_PATTERN_LINE_DISH);
    ctx.beginPath();
    ctx.translate(.5, .5);

    ctx.moveTo(0, height);
    ctx.lineTo(width, 0);

    ctx.strokeStyle = DEFAULT_COLOR;
    ctx.stroke();
    ctx.restore();

    let pattern = ctx.createPattern(canvas, "repeat");
    flexboxPatternMap.set(JUSTIFY_CONTENT, pattern);
    gCachedFlexboxPattern.set(devicePixelRatio, flexboxPatternMap);

    return pattern;
  }

  /**
   * The AutoRefreshHighlighter's _hasMoved method returns true only if the
   * element's quads have changed. Override it so it also returns true if the
   * flex container and its flex items have changed.
   */
  _hasMoved() {
    let hasMoved = AutoRefreshHighlighter.prototype._hasMoved.call(this);

    if (!this.computedStyle) {
      this.computedStyle = getComputedStyle(this.currentNode);
    }

    let oldFlexData = this.flexData;
    this.flexData = this.currentNode.getAsFlexContainer();
    let hasFlexDataChanged = compareFlexData(oldFlexData, this.flexData);

    let oldAlignItems = this.alignItemsValue;
    this.alignItemsValue = this.computedStyle.alignItems;
    let newAlignItems = this.alignItemsValue;

    let oldFlexBasis = this.flexBasis;
    this.flexBasis = this.computedStyle.flexBasis;
    let newFlexBasis = this.flexBasis;

    let oldFlexDirection = this.flexDirection;
    this.flexDirection = this.computedStyle.flexDirection;
    let newFlexDirection = this.flexDirection;

    let oldFlexWrap = this.flexWrap;
    this.flexWrap = this.computedStyle.flexWrap;
    let newFlexWrap = this.flexWrap;

    let oldJustifyContent = this.justifyContentValue;
    this.justifyContentValue = this.computedStyle.justifyContent;
    let newJustifyContent = this.justifyContentValue;

    return hasMoved ||
           hasFlexDataChanged ||
           oldAlignItems !== newAlignItems ||
           oldFlexBasis !== newFlexBasis ||
           oldFlexDirection !== newFlexDirection ||
           oldFlexWrap !== newFlexWrap ||
           oldJustifyContent !== newJustifyContent;
  }

  _hide() {
    this.alignItemsValue = null;
    this.computedStyle = null;
    this.flexBasis = null;
    this.flexData = null;
    this.flexDirection = null;
    this.flexWrap = null;
    this.justifyContentValue = null;

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

  renderAlignItemLine() {
    if (!this.flexData || !this.currentQuads.content || !this.currentQuads.content[0]) {
      return;
    }

    let { devicePixelRatio } = this.win;
    let lineWidth = getDisplayPixelRatio(this.win);
    let offset = (lineWidth / 2) % 1;
    let canvasX = Math.round(this._canvasPosition.x * devicePixelRatio);
    let canvasY = Math.round(this._canvasPosition.y * devicePixelRatio);

    this.ctx.save();
    this.ctx.translate(offset - canvasX, offset - canvasY);
    this.ctx.setLineDash(FLEXBOX_LINES_PROPERTIES.alignItems.lineDash);
    this.ctx.lineWidth = lineWidth * 3;
    this.ctx.strokeStyle = DEFAULT_COLOR;

    let { bounds } = this.currentQuads.content[0];
    let flexLines = this.flexData.getLines();
    let isColumn = this.flexDirection.startsWith("column");
    let options = { matrix: this.currentMatrix };

    for (let flexLine of flexLines) {
      let { crossStart, crossSize } = flexLine;

      switch (this.alignItemsValue) {
        case "baseline":
        case "first baseline":
          let { firstBaselineOffset } = flexLine;

          if (firstBaselineOffset < 0) {
            break;
          }

          if (isColumn) {
            drawLine(this.ctx, crossStart + firstBaselineOffset, 0,
              crossStart + firstBaselineOffset, bounds.height, options);
          } else {
            drawLine(this.ctx, 0, crossStart + firstBaselineOffset, bounds.width,
              crossStart + firstBaselineOffset, options);
          }

          this.ctx.stroke();
          break;
        case "center":
          if (isColumn) {
            drawLine(this.ctx, crossStart + crossSize / 2, 0, crossStart + crossSize / 2,
              bounds.height, options);
          } else {
            drawLine(this.ctx, 0, crossStart + crossSize / 2, bounds.width,
              crossStart + crossSize / 2, options);
          }

          this.ctx.stroke();
          break;
        case "flex-start":
        case "start":
          if (isColumn) {
            drawLine(this.ctx, crossStart, 0, crossStart, bounds.height, options);
          } else {
            drawLine(this.ctx, 0, crossStart, bounds.width, crossStart, options);
          }

          this.ctx.stroke();
          break;
        case "flex-end":
        case "end":
          if (isColumn) {
            drawLine(this.ctx, crossStart + crossSize, 0, crossStart + crossSize,
              bounds.height, options);
          } else {
            drawLine(this.ctx, 0, crossStart + crossSize, bounds.width,
              crossStart + crossSize, options);
          }

          this.ctx.stroke();
          break;
        case "stretch":
          if (isColumn) {
            drawLine(this.ctx, crossStart, 0, crossStart, bounds.height, options);
            this.ctx.stroke();
            drawLine(this.ctx, crossStart + crossSize, 0, crossStart + crossSize,
              bounds.height, options);
            this.ctx.stroke();
          } else {
            drawLine(this.ctx, 0, crossStart, bounds.width, crossStart, options);
            this.ctx.stroke();
            drawLine(this.ctx, 0, crossStart + crossSize, bounds.width,
              crossStart + crossSize, options);
            this.ctx.stroke();
          }

          break;
        default:
          break;
      }
    }

    this.ctx.restore();
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

    // Find current angle of outer flex element by measuring the angle of two arbitrary
    // points, then rotate canvas, so the hash pattern stays 45deg to the boundary.
    let p1 = apply(this.currentMatrix, [0, 0]);
    let p2 = apply(this.currentMatrix, [1, 0]);
    let angleRad = Math.atan2(p2[1] - p1[1], p2[0] - p1[0]);
    this.ctx.rotate(angleRad);

    this.ctx.fill();
    this.ctx.stroke();
    this.ctx.restore();
  }

  /**
   * Renders the flex basis for a given flex item.
   */
  renderFlexItemBasis(flexItem, left, top, right, bottom, boundsWidth) {
    if (!this.computedStyle) {
      return;
    }

    let basis = this.flexBasis;

    if (basis.endsWith("px")) {
      right = Math.round(left + parseFloat(basis));
    } else if (basis.endsWith("%")) {
      basis = parseFloat(basis) / 100 * boundsWidth;
      right = Math.round(left + basis);
    }

    this.ctx.fillStyle = BASIS_FILL_COLOR;
    drawRect(this.ctx, left, top, right, bottom, this.currentMatrix);
    this.ctx.fill();
  }

  renderFlexItems() {
    if (!this.flexData || !this.currentQuads.content || !this.currentQuads.content[0]) {
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
    let flexLines = this.flexData.getLines();

    for (let flexLine of flexLines) {
      let flexItems = flexLine.getItems();

      for (let flexItem of flexItems) {
        let { node } = flexItem;
        let quads = getAdjustedQuads(this.win, node, "border");

        if (!quads.length) {
          continue;
        }

        // Adjust the flex item bounds relative to the current quads.
        let { bounds: flexItemBounds } = quads[0];
        let left = Math.round(flexItemBounds.left - bounds.left);
        let top = Math.round(flexItemBounds.top - bounds.top);
        let right = Math.round(flexItemBounds.right - bounds.left);
        let bottom = Math.round(flexItemBounds.bottom - bounds.top);

        clearRect(this.ctx, left, top, right, bottom, this.currentMatrix);
        drawRect(this.ctx, left, top, right, bottom, this.currentMatrix);
        this.ctx.stroke();

        this.renderFlexItemBasis(node, left, top, right, bottom, bounds.width);
      }
    }

    this.ctx.restore();
  }

  renderFlexLines() {
    if (!this.flexData || !this.currentQuads.content || !this.currentQuads.content[0]) {
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
    let flexLines = this.flexData.getLines();
    let isColumn = this.flexDirection.startsWith("column");
    let options = { matrix: this.currentMatrix };

    for (let flexLine of flexLines) {
      let { crossStart, crossSize } = flexLine;

      if (isColumn) {
        clearRect(this.ctx, crossStart, 0, crossStart + crossSize, bounds.height,
          this.currentMatrix);

        // Avoid drawing the start flex line when they overlap with the flex container.
        if (crossStart != 0) {
          drawLine(this.ctx, crossStart, 0, crossStart, bounds.height, options);
          this.ctx.stroke();
        }

        // Avoid drawing the end flex line when they overlap with the flex container.
        if (bounds.width - crossStart - crossSize >= lineWidth) {
          drawLine(this.ctx, crossStart + crossSize, 0, crossStart + crossSize,
            bounds.height, options);
          this.ctx.stroke();
        }
      } else {
        clearRect(this.ctx, 0, crossStart, bounds.width, crossStart + crossSize,
          this.currentMatrix);

        // Avoid drawing the start flex line when they overlap with the flex container.
        if (crossStart != 0) {
          drawLine(this.ctx, 0, crossStart, bounds.width, crossStart, options);
          this.ctx.stroke();
        }

        // Avoid drawing the end flex line when they overlap with the flex container.
        if (bounds.height - crossStart - crossSize >= lineWidth) {
          drawLine(this.ctx, 0, crossStart + crossSize, bounds.width,
            crossStart + crossSize, options);
          this.ctx.stroke();
        }
      }
    }

    this.ctx.restore();
  }

  renderJustifyContent() {
    if (!this.flexData || !this.currentQuads.content || !this.currentQuads.content[0]) {
      return;
    }

    let { bounds } = this.currentQuads.content[0];
    let flexLines = this.flexData.getLines();
    let isColumn = this.flexDirection.startsWith("column");

    for (let flexLine of flexLines) {
      let { crossStart, crossSize } = flexLine;
      let flexItems = flexLine.getItems();
      let mainStart = 0;

      for (let flexItem of flexItems) {
        let { node } = flexItem;
        let quads = getAdjustedQuads(this.win, node, "margin");

        if (!quads.length) {
          continue;
        }

        // Adjust the flex item bounds relative to the current quads.
        let { bounds: flexItemBounds } = quads[0];
        let left = Math.round(flexItemBounds.left - bounds.left);
        let top = Math.round(flexItemBounds.top - bounds.top);
        let right = Math.round(flexItemBounds.right - bounds.left);
        let bottom = Math.round(flexItemBounds.bottom - bounds.top);

        if (isColumn) {
          this.drawJustifyContent(crossStart, mainStart, crossStart + crossSize, top);
          mainStart = bottom;
        } else {
          this.drawJustifyContent(mainStart, crossStart, left, crossStart + crossSize);
          mainStart = right;
        }
      }

      // Draw the last justify-content area after the last flex item.
      if (isColumn) {
        this.drawJustifyContent(crossStart, mainStart, crossStart + crossSize,
          bounds.height);
      } else {
        this.drawJustifyContent(mainStart, crossStart, bounds.width,
          crossStart + crossSize);
      }
    }
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
    this.renderJustifyContent();
    this.renderFlexItems();
    this.renderFlexContainerBorder();
    this.renderAlignItemLine();

    this._showFlexbox();

    root.setAttribute("style",
      `position: absolute; width: ${width}px; height: ${height}px; overflow: hidden`);

    setIgnoreLayoutChanges(false, this.highlighterEnv.document.documentElement);
    return true;
  }
}

/**
 * Returns whether or not the flex data has changed.
 *
 * @param  {Flex} oldFlexData
 *         The old Flex data object.
 * @param  {Flex} newFlexData
 *         The new Flex data object.
 * @return {Boolean} true if the flex data has changed and false otherwise.
 */
function compareFlexData(oldFlexData, newFlexData) {
  if (!oldFlexData || !newFlexData) {
    return true;
  }

  const oldLines = oldFlexData.getLines();
  const newLines = newFlexData.getLines();

  if (oldLines.length !== newLines.length) {
    return true;
  }

  for (let i = 0; i < oldLines.length; i++) {
    let oldLine = oldLines[i];
    let newLine = newLines[i];

    if (oldLine.crossSize !== newLine.crossSize ||
        oldLine.crossStart !== newLine.crossStart ||
        oldLine.firstBaselineOffset !== newLine.firstBaselineOffset ||
        oldLine.growthState !== newLine.growthState ||
        oldLine.lastBaselineOffset !== newLine.lastBaselineOffset) {
      return true;
    }

    let oldItems = oldLine.getItems();
    let newItems = newLine.getItems();

    if (oldItems.length !== newItems.length) {
      return true;
    }

    for (let j = 0; j < oldItems.length; j++) {
      let oldItem = oldItems[j];
      let newItem = newItems[j];

      if (oldItem.crossMaxSize !== newItem.crossMaxSize ||
          oldItem.crossMinSize !== newItem.crossMinSize ||
          oldItem.mainBaseSize !== newItem.mainBaseSize ||
          oldItem.mainDeltaSize !== newItem.mainDeltaSize ||
          oldItem.mainMaxSize !== newItem.mainMaxSize ||
          oldItem.mainMinSize !== newItem.mainMinSize) {
        return true;
      }
    }
  }

  return false;
}

exports.FlexboxHighlighter = FlexboxHighlighter;
