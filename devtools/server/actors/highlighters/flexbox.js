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
  getAbsoluteScrollOffsetsForNode,
  getCurrentZoom,
  getDisplayPixelRatio,
  getUntransformedQuad,
  getWindowDimensions,
  setIgnoreLayoutChanges,
} = require("devtools/shared/layout/utils");

const FLEXBOX_LINES_PROPERTIES = {
  "edge": {
    lineDash: [5, 3],
  },
  "item": {
    lineDash: [0, 0],
  },
  "alignItems": {
    lineDash: [0, 0],
  },
};

const FLEXBOX_CONTAINER_PATTERN_LINE_DASH = [5, 3]; // px
const FLEXBOX_CONTAINER_PATTERN_WIDTH = 14; // px
const FLEXBOX_CONTAINER_PATTERN_HEIGHT = 14; // px
const FLEXBOX_JUSTIFY_CONTENT_PATTERN_WIDTH = 7; // px
const FLEXBOX_JUSTIFY_CONTENT_PATTERN_HEIGHT = 7; // px

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
 * @param {String} options.color
 *        The color that should be used to draw the highlighter for this flexbox.
 * Structure:
 * <div class="highlighter-container">
 *   <div id="flexbox-root" class="flexbox-root">
 *     <canvas id="flexbox-canvas"
 *             class="flexbox-canvas"
 *             width="4096"
 *             height="4096"
 *             hidden="true">
 *     </canvas>
 *   </div>
 * </div>
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
      y: 0,
    };

    this._ignoreZoom = true;

    // Calling `updateCanvasPosition` anyway since the highlighter could be initialized
    // on a page that has scrolled already.
    updateCanvasPosition(this._canvasPosition, this._scroll, this.win,
      this._winDimensions);
  }

  _buildMarkup() {
    const container = createNode(this.win, {
      attributes: {
        "class": "highlighter-container",
      },
    });

    const root = createNode(this.win, {
      parent: container,
      attributes: {
        "id": "root",
        "class": "root",
      },
      prefix: this.ID_CLASS_PREFIX,
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
        "height": CANVAS_SIZE,
      },
      prefix: this.ID_CLASS_PREFIX,
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

    this.axes = null;
    this.crossAxisDirection = null;
    this.flexData = null;
    this.mainAxisDirection = null;
    this.transform = null;

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

  get container() {
    return this.currentNode;
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
    ctx.setLineDash(FLEXBOX_CONTAINER_PATTERN_LINE_DASH);
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

    if (flexboxPatternMap.has(JUSTIFY_CONTENT)) {
      return flexboxPatternMap.get(JUSTIFY_CONTENT);
    }

    // Create the inversed diagonal lines pattern
    // for the rendering the justify content gaps.
    const canvas = createNode(this.win, { nodeType: "canvas" });
    const zoom = getCurrentZoom(this.win);
    const width = canvas.width =
      FLEXBOX_JUSTIFY_CONTENT_PATTERN_WIDTH * devicePixelRatio * zoom;
    const height = canvas.height =
      FLEXBOX_JUSTIFY_CONTENT_PATTERN_HEIGHT * devicePixelRatio * zoom;

    const ctx = canvas.getContext("2d");
    ctx.save();
    ctx.setLineDash(FLEXBOX_CONTAINER_PATTERN_LINE_DASH);
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
      this.computedStyle = getComputedStyle(this.container);
    }

    const flex = this.container.getAsFlexContainer();

    const oldCrossAxisDirection = this.crossAxisDirection;
    this.crossAxisDirection = flex ? flex.crossAxisDirection : null;
    const newCrossAxisDirection = this.crossAxisDirection;

    const oldMainAxisDirection = this.mainAxisDirection;
    this.mainAxisDirection = flex ? flex.mainAxisDirection : null;
    const newMainAxisDirection = this.mainAxisDirection;

    // Concatenate the axes to simplify conditionals.
    this.axes = `${this.mainAxisDirection} ${this.crossAxisDirection}`;

    const oldFlexData = this.flexData;
    this.flexData = getFlexData(this.container);
    const hasFlexDataChanged = compareFlexData(oldFlexData, this.flexData);

    const oldAlignItems = this.alignItemsValue;
    this.alignItemsValue = this.computedStyle.alignItems;
    const newAlignItems = this.alignItemsValue;

    const oldFlexDirection = this.flexDirection;
    this.flexDirection = this.computedStyle.flexDirection;
    const newFlexDirection = this.flexDirection;

    const oldFlexWrap = this.flexWrap;
    this.flexWrap = this.computedStyle.flexWrap;
    const newFlexWrap = this.flexWrap;

    const oldJustifyContent = this.justifyContentValue;
    this.justifyContentValue = this.computedStyle.justifyContent;
    const newJustifyContent = this.justifyContentValue;

    const oldTransform = this.transformValue;
    this.transformValue = this.computedStyle.transform;
    const newTransform = this.transformValue;

    return hasMoved ||
           hasFlexDataChanged ||
           oldAlignItems !== newAlignItems ||
           oldFlexDirection !== newFlexDirection ||
           oldFlexWrap !== newFlexWrap ||
           oldJustifyContent !== newJustifyContent ||
           oldCrossAxisDirection !== newCrossAxisDirection ||
           oldMainAxisDirection !== newMainAxisDirection ||
           oldTransform !== newTransform;
  }

  _hide() {
    this.alignItemsValue = null;
    this.computedStyle = null;
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

  renderFlexContainer() {
    if (!this.currentQuads.content || !this.currentQuads.content[0]) {
      return;
    }

    const { devicePixelRatio } = this.win;
    const containerQuad = getUntransformedQuad(this.container, "content");
    const { width, height } = containerQuad.getBounds();

    this.setupCanvas({
      lineDash: FLEXBOX_LINES_PROPERTIES.alignItems.lineDash,
      lineWidthMultiplier: 2,
    });

    this.ctx.fillStyle = this.getFlexContainerPattern(devicePixelRatio);

    drawRect(this.ctx, 0, 0, width, height, this.currentMatrix);

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

  renderFlexItems() {
    if (!this.flexData || !this.currentQuads.content || !this.currentQuads.content[0]) {
      return;
    }

    this.setupCanvas({
      lineDash: FLEXBOX_LINES_PROPERTIES.item.lineDash,
    });

    for (const flexLine of this.flexData.lines) {
      for (const flexItem of flexLine.items) {
        const { left, top, right, bottom } = flexItem.rect;

        clearRect(this.ctx, left, top, right, bottom, this.currentMatrix);
        drawRect(this.ctx, left, top, right, bottom, this.currentMatrix);
        this.ctx.stroke();
      }
    }

    this.ctx.restore();
  }

  renderFlexLines() {
    if (!this.flexData || !this.currentQuads.content || !this.currentQuads.content[0]) {
      return;
    }

    const lineWidth = getDisplayPixelRatio(this.win);
    const options = { matrix: this.currentMatrix };
    const { width: containerWidth, height: containerHeight } =
      getUntransformedQuad(this.container, "content").getBounds();

    this.setupCanvas({
      useContainerScrollOffsets: true,
    });

    for (const flexLine of this.flexData.lines) {
      const { crossStart, crossSize } = flexLine;

      switch (this.axes) {
        case "horizontal-lr vertical-tb":
        case "horizontal-lr vertical-bt":
        case "horizontal-rl vertical-tb":
        case "horizontal-rl vertical-bt":
          clearRect(this.ctx, 0, crossStart, containerWidth, crossStart + crossSize,
            this.currentMatrix);

          // Avoid drawing the start flex line when they overlap with the flex container.
          if (crossStart != 0) {
            drawLine(this.ctx, 0, crossStart, containerWidth, crossStart, options);
            this.ctx.stroke();
          }

          // Avoid drawing the end flex line when they overlap with the flex container.
          if (crossStart + crossSize < containerHeight - lineWidth * 2) {
            drawLine(this.ctx, 0, crossStart + crossSize, containerWidth,
              crossStart + crossSize, options);
            this.ctx.stroke();
          }
          break;
        case "vertical-tb horizontal-lr":
        case "vertical-bt horizontal-rl":
          clearRect(this.ctx, crossStart, 0, crossStart + crossSize, containerHeight,
            this.currentMatrix);

          // Avoid drawing the start flex line when they overlap with the flex container.
          if (crossStart != 0) {
            drawLine(this.ctx, crossStart, 0, crossStart, containerHeight, options);
            this.ctx.stroke();
          }

          // Avoid drawing the end flex line when they overlap with the flex container.
          if (crossStart + crossSize < containerWidth - lineWidth * 2) {
            drawLine(this.ctx, crossStart + crossSize, 0, crossStart + crossSize,
              containerHeight, options);
            this.ctx.stroke();
          }
          break;
        case "vertical-bt horizontal-lr":
        case "vertical-tb horizontal-rl":
          clearRect(this.ctx, containerWidth - crossStart, 0,
            containerWidth - crossStart - crossSize, containerHeight, this.currentMatrix);

          // Avoid drawing the start flex line when they overlap with the flex container.
          if (crossStart != 0) {
            drawLine(this.ctx, containerWidth - crossStart, 0,
              containerWidth - crossStart, containerHeight, options);
            this.ctx.stroke();
          }

          // Avoid drawing the end flex line when they overlap with the flex container.
          if (crossStart + crossSize < containerWidth - lineWidth * 2) {
            drawLine(this.ctx, containerWidth - crossStart - crossSize, 0,
              containerWidth - crossStart - crossSize, containerHeight, options);
            this.ctx.stroke();
          }
          break;
      }
    }

    this.ctx.restore();
  }

  /**
   * Clear the whole alignment container along the main axis for each flex item.
   */
  renderJustifyContent() {
    if (!this.flexData || !this.currentQuads.content || !this.currentQuads.content[0]) {
      return;
    }

    const { width: containerWidth, height: containerHeight } =
      getUntransformedQuad(this.container, "content").getBounds();

    this.setupCanvas({
      lineDash: FLEXBOX_LINES_PROPERTIES.alignItems.lineDash,
      offset: (getDisplayPixelRatio(this.win) / 2) % 1,
      skipLineAndStroke: true,
      useContainerScrollOffsets: true,
    });

    for (const flexLine of this.flexData.lines) {
      const { crossStart, crossSize } = flexLine;
      let mainStart = 0;

      // In these two situations mainStart goes from right to left so set it's
      // value as appropriate.
      if (this.axes === "horizontal-lr vertical-bt" ||
          this.axes === "horizontal-rl vertical-tb") {
        mainStart = containerWidth;
      }

      for (const flexItem of flexLine.items) {
        const { left, top, right, bottom } = flexItem.rect;

        switch (this.axes) {
          case "horizontal-lr vertical-tb":
          case "horizontal-rl vertical-bt":
            this.drawJustifyContent(mainStart, crossStart, left, crossStart + crossSize);
            mainStart = right;
            break;
          case "horizontal-lr vertical-bt":
          case "horizontal-rl vertical-tb":
            this.drawJustifyContent(right, crossStart, mainStart, crossStart + crossSize);
            mainStart = left;
            break;
          case "vertical-tb horizontal-lr":
          case "vertical-bt horizontal-rl":
            this.drawJustifyContent(crossStart, mainStart, crossStart + crossSize, top);
            mainStart = bottom;
            break;
          case "vertical-bt horizontal-lr":
          case "vertical-tb horizontal-rl":
            this.drawJustifyContent(containerWidth - crossStart - crossSize, mainStart,
              containerWidth - crossStart, top);
            mainStart = bottom;
            break;
        }
      }

      // Draw the last justify-content area after the last flex item.
      switch (this.axes) {
        case "horizontal-lr vertical-tb":
        case "horizontal-rl vertical-bt":
          this.drawJustifyContent(mainStart, crossStart, containerWidth,
            crossStart + crossSize);
          break;
        case "horizontal-lr vertical-bt":
        case "horizontal-rl vertical-tb":
          this.drawJustifyContent(0, crossStart, mainStart, crossStart + crossSize);
          break;
        case "vertical-tb horizontal-lr":
        case "vertical-bt horizontal-rl":
          this.drawJustifyContent(crossStart, mainStart, crossStart + crossSize,
            containerHeight);
          break;
        case "vertical-bt horizontal-lr":
        case "vertical-tb horizontal-rl":
          this.drawJustifyContent(containerWidth - crossStart - crossSize, mainStart,
            containerWidth - crossStart, containerHeight);
          break;
      }
    }

    this.ctx.restore();
  }

  /**
   * Set up the canvas with the given options prior to drawing.
   *
   * @param {String} [options.lineDash = null]
   *        An Array of numbers that specify distances to alternately draw a
   *        line and a gap (in coordinate space units). If the number of
   *        elements in the array is odd, the elements of the array get copied
   *        and concatenated. For example, [5, 15, 25] will become
   *        [5, 15, 25, 5, 15, 25]. If the array is empty, the line dash list is
   *        cleared and line strokes return to being solid.
   *
   *        We use the following constants here:
   *          FLEXBOX_LINES_PROPERTIES.edge.lineDash,
   *          FLEXBOX_LINES_PROPERTIES.item.lineDash
   *          FLEXBOX_LINES_PROPERTIES.alignItems.lineDash
   * @param {Number} [options.lineWidthMultiplier = 1]
   *        The width of the line.
   * @param {Number} [options.offset = `(displayPixelRatio / 2) % 1`]
   *        The single line width used to obtain a crisp line.
   * @param {Boolean} [options.skipLineAndStroke = false]
   *        Skip the setting of lineWidth and strokeStyle.
   * @param {Boolean} [options.useContainerScrollOffsets = false]
   *        Take the flexbox container's scroll and zoom offsets into account.
   *        This is needed for drawing flex lines and justify content when the
   *        flexbox container itself is display:scroll.
   */
  setupCanvas({
      lineDash = null,
      lineWidthMultiplier = 1,
      offset = (getDisplayPixelRatio(this.win) / 2) % 1,
      skipLineAndStroke = false,
      useContainerScrollOffsets = false }) {
    const { devicePixelRatio } = this.win;
    const lineWidth = getDisplayPixelRatio(this.win);
    const zoom = getCurrentZoom(this.win);
    const style = getComputedStyle(this.container);
    const position = style.position;
    let offsetX = this._canvasPosition.x;
    let offsetY = this._canvasPosition.y;

    if (useContainerScrollOffsets) {
      offsetX += this.container.scrollLeft / zoom;
      offsetY += this.container.scrollTop / zoom;
    }

    // If the flexbox container is position:fixed we need to subtract the scroll
    // positions of all ancestral elements.
    if (position === "fixed") {
      const { scrollLeft, scrollTop } =
        getAbsoluteScrollOffsetsForNode(this.container);
      offsetX -= scrollLeft / zoom;
      offsetY -= scrollTop / zoom;
    }

    const canvasX = Math.round(offsetX * devicePixelRatio * zoom);
    const canvasY = Math.round(offsetY * devicePixelRatio * zoom);

    this.ctx.save();
    this.ctx.translate(offset - canvasX, offset - canvasY);

    if (lineDash) {
      this.ctx.setLineDash(lineDash);
    }

    if (!skipLineAndStroke) {
      this.ctx.lineWidth = lineWidth * lineWidthMultiplier;
      this.ctx.strokeStyle = this.color;
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
    updateCanvasElement(this.canvas, this._canvasPosition, this.win.devicePixelRatio, {
      zoomWindow: this.win,
    });

    // Update the current matrix used in our canvas' rendering
    const { currentMatrix, hasNodeTransformations } =
      getCurrentMatrix(this.container, this.win, {
        ignoreWritingModeAndTextDirection: true,
      });
    this.currentMatrix = currentMatrix;
    this.hasNodeTransformations = hasNodeTransformations;

    this.renderFlexContainer();
    this.renderFlexLines();
    this.renderJustifyContent();
    this.renderFlexItems();

    this._showFlexbox();

    root.setAttribute("style",
      `position: absolute; width: ${width}px; height: ${height}px; overflow: hidden`);

    setIgnoreLayoutChanges(false, this.highlighterEnv.document.documentElement);
    return true;
  }
}

/**
 * Returns an object representation of the Flex data object and its array of FlexLine
 * and FlexItem objects along with the DOMRects of the flex items.
 *
 * @param  {DOMNode} container
 *         The flex container.
 * @return {Object|null} representation of the Flex data object.
 */
function getFlexData(container) {
  const flex = container.getAsFlexContainer();

  if (!flex) {
    return null;
  }

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
            rect: getRectFromFlexItemValues(item, container),
          };
        }),
      };
    }),
  };
}

/**
 * Given a FlexItemValues, return a DOMRect representing the flex item taking
 * into account its flex container's border and padding.
 *
 * @param  {FlexItemValues} item
 *         The FlexItemValues for which we need the DOMRect.
 * @param  {DOMNode}
 *         Flex container containing the flex item.
 * @return {DOMRect} representing the flex item.
 */
function getRectFromFlexItemValues(item, container) {
  const rect = item.frameRect;
  const domRect = new DOMRect(rect.x, rect.y, rect.width, rect.height);
  const win = container.ownerGlobal;
  const style = win.getComputedStyle(container);
  const borderLeftWidth = parseInt(style.borderLeftWidth, 10) || 0;
  const borderTopWidth = parseInt(style.borderTopWidth, 10) || 0;
  const paddingLeft = parseInt(style.paddingLeft, 10) || 0;
  const paddingTop = parseInt(style.paddingTop, 10) || 0;
  const scrollX = container.scrollLeft || 0;
  const scrollY = container.scrollTop || 0;

  domRect.x -= paddingLeft + scrollX;
  domRect.y -= paddingTop + scrollY;

  if (style.overflow === "visible" || style.overflow === "-moz-hidden-unscrollable") {
    domRect.x -= borderLeftWidth;
    domRect.y -= borderTopWidth;
  }

  return domRect;
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

      const oldItemRect = oldItem.rect;
      const newItemRect = newItem.rect;

      // We are using DOMRects so we only need to compare x, y, width and
      // height (left, top, right and bottom are calculated from these values).
      if (oldItemRect.x !== newItemRect.x ||
          oldItemRect.y !== newItemRect.y ||
          oldItemRect.width !== newItemRect.width ||
          oldItemRect.height !== newItemRect.height) {
        return true;
      }
    }
  }

  return false;
}

exports.FlexboxHighlighter = FlexboxHighlighter;
