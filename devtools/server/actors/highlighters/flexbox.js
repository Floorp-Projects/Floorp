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
  getWindowDimensions,
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

/**
 * The FlexboxHighlighter is the class that overlays a visual canvas on top of
 * display: [inline-]flex elements.
 *
 * Available Options:
 * - color(colorValue)
 *     @param  {String} colorValue
 *     The color that should be used to draw the highlighter for this flexbox.
 * - showAlignment(isShown)
 *     @param  {Boolean} isShown
 *     Shows the alignment in the flexbox highlighter.
 * - showFlexBasis(isShown)
 *     @param  {Boolean} isShown
 *     Shows the flex basis in the flexbox highlighter.
 */
class FlexboxHighlighter extends AutoRefreshHighlighter {
  constructor(highlighterEnv) {
    super(highlighterEnv);

    this.ID_CLASS_PREFIX = "flexbox-";

    this.markup = new CanvasFrameAnonymousContentHelper(this.highlighterEnv,
      this._buildMarkup.bind(this));

    this.onPageHide = this.onPageHide.bind(this);
    this.onWillNavigate = this.onWillNavigate.bind(this);

    this.highlighterEnv.on("will-navigate", this.onWillNavigate);

    const { pageListenerTarget } = highlighterEnv;
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
    const container = createNode(this.win, {
      attributes: {
        "class": "highlighter-container"
      }
    });

    const root = createNode(this.win, {
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
    const { highlighterEnv } = this;
    highlighterEnv.off("will-navigate", this.onWillNavigate);

    const { pageListenerTarget } = highlighterEnv;

    if (pageListenerTarget) {
      pageListenerTarget.removeEventListener("pagehide", this.onPageHide);
    }

    this.markup.destroy();

    // Clear the pattern cache to avoid dead object exceptions (Bug 1342051).
    this.clearCache();

    this.flexData = null;

    AutoRefreshHighlighter.prototype.destroy.call(this);
  }

  /**
   * Draw the justify content for a given flex item (left, top, right, bottom) position.
   */
  drawJustifyContent(left, top, right, bottom) {
    const { devicePixelRatio } = this.win;
    this.ctx.fillStyle = this.getJustifyContentPattern(devicePixelRatio);
    drawRect(this.ctx, left, top, right, bottom, this.currentMatrix);
    this.ctx.fill();
  }

  get canvas() {
    return this.getElement("canvas");
  }

  get color() {
    return this.options.color || DEFAULT_COLOR;
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
    const canvas = createNode(this.win, { nodeType: "canvas" });
    const width = canvas.width = FLEXBOX_CONTAINER_PATTERN_WIDTH * devicePixelRatio;
    const height = canvas.height = FLEXBOX_CONTAINER_PATTERN_HEIGHT * devicePixelRatio;

    const ctx = canvas.getContext("2d");
    ctx.save();
    ctx.setLineDash(FLEXBOX_CONTAINER_PATTERN_LINE_DISH);
    ctx.beginPath();
    ctx.translate(.5, .5);

    ctx.moveTo(0, 0);
    ctx.lineTo(width, height);

    ctx.strokeStyle = this.color;
    ctx.stroke();
    ctx.restore();

    const pattern = ctx.createPattern(canvas, "repeat");
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
    const canvas = createNode(this.win, { nodeType: "canvas" });
    const width = canvas.width = FLEXBOX_JUSTIFY_CONTENT_PATTERN_WIDTH * devicePixelRatio;
    const height = canvas.height = FLEXBOX_JUSTIFY_CONTENT_PATTERN_HEIGHT *
      devicePixelRatio;

    const ctx = canvas.getContext("2d");
    ctx.save();
    ctx.setLineDash(FLEXBOX_CONTAINER_PATTERN_LINE_DISH);
    ctx.beginPath();
    ctx.translate(.5, .5);

    ctx.moveTo(0, height);
    ctx.lineTo(width, 0);

    ctx.strokeStyle = this.color;
    ctx.stroke();
    ctx.restore();

    const pattern = ctx.createPattern(canvas, "repeat");
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
    const hasMoved = AutoRefreshHighlighter.prototype._hasMoved.call(this);

    if (!this.computedStyle) {
      this.computedStyle = getComputedStyle(this.currentNode);
    }

    const oldFlexData = this.flexData;
    this.flexData = getFlexData(this.currentNode.getAsFlexContainer(), this.win);
    const hasFlexDataChanged = compareFlexData(oldFlexData, this.flexData);

    const oldAlignItems = this.alignItemsValue;
    this.alignItemsValue = this.computedStyle.alignItems;
    const newAlignItems = this.alignItemsValue;

    const oldFlexBasis = this.flexBasis;
    this.flexBasis = this.computedStyle.flexBasis;
    const newFlexBasis = this.flexBasis;

    const oldFlexDirection = this.flexDirection;
    this.flexDirection = this.computedStyle.flexDirection;
    const newFlexDirection = this.flexDirection;

    const oldFlexWrap = this.flexWrap;
    this.flexWrap = this.computedStyle.flexWrap;
    const newFlexWrap = this.flexWrap;

    const oldJustifyContent = this.justifyContentValue;
    this.justifyContentValue = this.computedStyle.justifyContent;
    const newJustifyContent = this.justifyContentValue;

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
    const hasUpdated = updateCanvasPosition(this._canvasPosition, this._scroll, this.win,
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
    if (!this.options.showAlignment || !this.flexData ||
        !this.currentQuads.content || !this.currentQuads.content[0]) {
      return;
    }

    const { devicePixelRatio } = this.win;
    const lineWidth = getDisplayPixelRatio(this.win);
    const offset = (lineWidth / 2) % 1;
    const canvasX = Math.round(this._canvasPosition.x * devicePixelRatio);
    const canvasY = Math.round(this._canvasPosition.y * devicePixelRatio);

    this.ctx.save();
    this.ctx.translate(offset - canvasX, offset - canvasY);
    this.ctx.setLineDash(FLEXBOX_LINES_PROPERTIES.alignItems.lineDash);
    this.ctx.lineWidth = lineWidth * 3;
    this.ctx.strokeStyle = this.color;

    const { bounds } = this.currentQuads.content[0];
    const isColumn = this.flexDirection.startsWith("column");
    const options = { matrix: this.currentMatrix };

    for (const flexLine of this.flexData.lines) {
      const { crossStart, crossSize } = flexLine;

      switch (this.alignItemsValue) {
        case "baseline":
        case "first baseline":
          const { firstBaselineOffset } = flexLine;

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

    const { devicePixelRatio } = this.win;
    const lineWidth = getDisplayPixelRatio(this.win) * 2;
    const offset = (lineWidth / 2) % 1;
    const canvasX = Math.round(this._canvasPosition.x * devicePixelRatio);
    const canvasY = Math.round(this._canvasPosition.y * devicePixelRatio);

    this.ctx.save();
    this.ctx.translate(offset - canvasX, offset - canvasY);
    this.ctx.setLineDash(FLEXBOX_LINES_PROPERTIES.edge.lineDash);
    this.ctx.lineWidth = lineWidth;
    this.ctx.strokeStyle = this.color;

    const { bounds } = this.currentQuads.content[0];
    drawRect(this.ctx, 0, 0, bounds.width, bounds.height, this.currentMatrix);

    this.ctx.stroke();
    this.ctx.restore();
  }

  renderFlexContainerFill() {
    if (!this.currentQuads.content || !this.currentQuads.content[0]) {
      return;
    }

    const { devicePixelRatio } = this.win;
    const lineWidth = getDisplayPixelRatio(this.win);
    const offset = (lineWidth / 2) % 1;
    const canvasX = Math.round(this._canvasPosition.x * devicePixelRatio);
    const canvasY = Math.round(this._canvasPosition.y * devicePixelRatio);

    this.ctx.save();
    this.ctx.translate(offset - canvasX, offset - canvasY);
    this.ctx.setLineDash(FLEXBOX_LINES_PROPERTIES.edge.lineDash);
    this.ctx.lineWidth = 0;
    this.ctx.strokeStyle = this.color;
    this.ctx.fillStyle = this.getFlexContainerPattern(devicePixelRatio);

    const { bounds } = this.currentQuads.content[0];
    drawRect(this.ctx, 0, 0, bounds.width, bounds.height, this.currentMatrix);

    // Find current angle of outer flex element by measuring the angle of two arbitrary
    // points, then rotate canvas, so the hash pattern stays 45deg to the boundary.
    const p1 = apply(this.currentMatrix, [0, 0]);
    const p2 = apply(this.currentMatrix, [1, 0]);
    const angleRad = Math.atan2(p2[1] - p1[1], p2[0] - p1[0]);
    this.ctx.rotate(angleRad);

    this.ctx.fill();
    this.ctx.stroke();
    this.ctx.restore();
  }

  /**
   * Renders the flex basis for a given flex item.
   */
  renderFlexItemBasis(flexItem, left, top, right, bottom, boundsWidth) {
    if (!this.options.showFlexBasis || !this.computedStyle) {
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

    const { devicePixelRatio } = this.win;
    const lineWidth = getDisplayPixelRatio(this.win);
    const offset = (lineWidth / 2) % 1;
    const canvasX = Math.round(this._canvasPosition.x * devicePixelRatio);
    const canvasY = Math.round(this._canvasPosition.y * devicePixelRatio);

    this.ctx.save();
    this.ctx.translate(offset - canvasX, offset - canvasY);
    this.ctx.setLineDash(FLEXBOX_LINES_PROPERTIES.item.lineDash);
    this.ctx.lineWidth = lineWidth;
    this.ctx.strokeStyle = this.color;

    const { bounds } = this.currentQuads.content[0];

    for (const flexLine of this.flexData.lines) {
      for (const flexItem of flexLine.items) {
        const quads = flexItem.quads;
        if (!quads.length) {
          continue;
        }

        // Adjust the flex item bounds relative to the current quads.
        const { bounds: flexItemBounds } = quads[0];
        const left = Math.round(flexItemBounds.left - bounds.left);
        const top = Math.round(flexItemBounds.top - bounds.top);
        const right = Math.round(flexItemBounds.right - bounds.left);
        const bottom = Math.round(flexItemBounds.bottom - bounds.top);

        clearRect(this.ctx, left, top, right, bottom, this.currentMatrix);
        drawRect(this.ctx, left, top, right, bottom, this.currentMatrix);
        this.ctx.stroke();

        this.renderFlexItemBasis(flexItem.node, left, top, right, bottom, bounds.width);
      }
    }

    this.ctx.restore();
  }

  renderFlexLines() {
    if (!this.flexData || !this.currentQuads.content || !this.currentQuads.content[0]) {
      return;
    }

    const { devicePixelRatio } = this.win;
    const lineWidth = getDisplayPixelRatio(this.win);
    const offset = (lineWidth / 2) % 1;
    const canvasX = Math.round(this._canvasPosition.x * devicePixelRatio);
    const canvasY = Math.round(this._canvasPosition.y * devicePixelRatio);

    this.ctx.save();
    this.ctx.translate(offset - canvasX, offset - canvasY);
    this.ctx.lineWidth = lineWidth;
    this.ctx.strokeStyle = this.color;

    const { bounds } = this.currentQuads.content[0];
    const isColumn = this.flexDirection.startsWith("column");
    const options = { matrix: this.currentMatrix };

    for (const flexLine of this.flexData.lines) {
      const { crossStart, crossSize } = flexLine;

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

    const { bounds } = this.currentQuads.content[0];
    const isColumn = this.flexDirection.startsWith("column");

    for (const flexLine of this.flexData.lines) {
      const { crossStart, crossSize } = flexLine;
      let mainStart = 0;

      for (const flexItem of flexLine.items) {
        const quads = flexItem.quads;
        if (!quads.length) {
          continue;
        }

        // Adjust the flex item bounds relative to the current quads.
        const { bounds: flexItemBounds } = quads[0];
        const left = Math.round(flexItemBounds.left - bounds.left);
        const top = Math.round(flexItemBounds.top - bounds.top);
        const right = Math.round(flexItemBounds.right - bounds.left);
        const bottom = Math.round(flexItemBounds.bottom - bounds.top);

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

    const root = this.getElement("root");

    // Hide the root element and force the reflow in order to get the proper window's
    // dimensions without increasing them.
    root.setAttribute("style", "display: none");
    this.win.document.documentElement.offsetWidth;
    this._winDimensions = getWindowDimensions(this.win);
    const { width, height } = this._winDimensions;

    // Updates the <canvas> element's position and size.
    // It also clear the <canvas>'s drawing context.
    updateCanvasElement(this.canvas, this._canvasPosition, this.win.devicePixelRatio);

    // Update the current matrix used in our canvas' rendering
    const { currentMatrix, hasNodeTransformations } = getCurrentMatrix(this.currentNode,
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
 * Returns an object representation of the Flex data object and its array of FlexLine
 * and FlexItem objects along with the box quads of the flex items.
 *
 * @param  {Flex} flex
 *         The Flex data object.
 * @param  {Window} win
 *         The Window object.
 * @return {Object} representation of the Flex data object.
 */
function getFlexData(flex, win) {
  return {
    lines: flex.getLines().map(line => {
      return {
        crossSize: line.crossSize,
        crossStart: line.crossStart,
        firstBaselineOffset: line.firstBaselineOffset,
        growthState: line.growthState,
        lastBaselineOffset: line.lastBaselineOffset,
        items: line.getItems().map(item => {
          return {
            crossMaxSize: item.crossMaxSize,
            crossMinSize: item.crossMinSize,
            mainBaseSize: item.mainBaseSize,
            mainDeltaSize: item.mainDeltaSize,
            mainMaxSize: item.mainMaxSize,
            mainMinSize: item.mainMinSize,
            node: item.node,
            quads: getAdjustedQuads(win, item.node),
          };
        }),
      };
    })
  };
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

  const oldLines = oldFlexData.lines;
  const newLines = newFlexData.lines;

  if (oldLines.length !== newLines.length) {
    return true;
  }

  for (let i = 0; i < oldLines.length; i++) {
    const oldLine = oldLines[i];
    const newLine = newLines[i];

    if (oldLine.crossSize !== newLine.crossSize ||
        oldLine.crossStart !== newLine.crossStart ||
        oldLine.firstBaselineOffset !== newLine.firstBaselineOffset ||
        oldLine.growthState !== newLine.growthState ||
        oldLine.lastBaselineOffset !== newLine.lastBaselineOffset) {
      return true;
    }

    const oldItems = oldLine.items;
    const newItems = newLine.items;

    if (oldItems.length !== newItems.length) {
      return true;
    }

    for (let j = 0; j < oldItems.length; j++) {
      const oldItem = oldItems[j];
      const newItem = newItems[j];

      if (oldItem.crossMaxSize !== newItem.crossMaxSize ||
          oldItem.crossMinSize !== newItem.crossMinSize ||
          oldItem.mainBaseSize !== newItem.mainBaseSize ||
          oldItem.mainDeltaSize !== newItem.mainDeltaSize ||
          oldItem.mainMaxSize !== newItem.mainMaxSize ||
          oldItem.mainMinSize !== newItem.mainMinSize) {
        return true;
      }

      const oldItemQuads = oldItem.quads;
      const newItemQuads = newItem.quads;

      if (oldItemQuads.length !== newItemQuads.length) {
        return true;
      }

      const { bounds: oldItemBounds } = oldItemQuads[0];
      const { bounds: newItemBounds } = newItemQuads[0];

      if (oldItemBounds.bottom !== newItemBounds.bottom ||
          oldItemBounds.height !== newItemBounds.height ||
          oldItemBounds.left !== newItemBounds.left ||
          oldItemBounds.right !== newItemBounds.right ||
          oldItemBounds.top !== newItemBounds.top ||
          oldItemBounds.width !== newItemBounds.width ||
          oldItemBounds.x !== newItemBounds.x ||
          oldItemBounds.y !== newItemBounds.y) {
        return true;
      }
    }
  }

  return false;
}

exports.FlexboxHighlighter = FlexboxHighlighter;
