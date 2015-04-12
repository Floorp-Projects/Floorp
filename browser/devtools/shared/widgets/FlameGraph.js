/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { ViewHelpers } = require("resource:///modules/devtools/ViewHelpers.jsm");
const { AbstractCanvasGraph, GraphArea, GraphAreaDragger } = require("resource:///modules/devtools/Graphs.jsm");
const { Promise } = require("resource://gre/modules/Promise.jsm");
const { Task } = require("resource://gre/modules/Task.jsm");
const { getColor } = require("devtools/shared/theme");
const EventEmitter = require("devtools/toolkit/event-emitter");
const FrameUtils = require("devtools/shared/profiler/frame-utils");

const HTML_NS = "http://www.w3.org/1999/xhtml";
const GRAPH_SRC = "chrome://browser/content/devtools/graphs-frame.xhtml";
const L10N = new ViewHelpers.L10N();

const GRAPH_RESIZE_EVENTS_DRAIN = 100; // ms

const GRAPH_WHEEL_ZOOM_SENSITIVITY = 0.00035;
const GRAPH_WHEEL_SCROLL_SENSITIVITY = 0.5;
const GRAPH_MIN_SELECTION_WIDTH = 0.001; // ms

const GRAPH_HORIZONTAL_PAN_THRESHOLD = 10; // px
const GRAPH_VERTICAL_PAN_THRESHOLD = 30; // px

const FIND_OPTIMAL_TICK_INTERVAL_MAX_ITERS = 100;
const TIMELINE_TICKS_MULTIPLE = 5; // ms
const TIMELINE_TICKS_SPACING_MIN = 75; // px

const OVERVIEW_HEADER_HEIGHT = 16; // px
const OVERVIEW_HEADER_TEXT_FONT_SIZE = 9; // px
const OVERVIEW_HEADER_TEXT_FONT_FAMILY = "sans-serif";
const OVERVIEW_HEADER_TEXT_PADDING_LEFT = 6; // px
const OVERVIEW_HEADER_TEXT_PADDING_TOP = 5; // px
const OVERVIEW_HEADER_TIMELINE_STROKE_COLOR = "rgba(128, 128, 128, 0.5)";

const FLAME_GRAPH_BLOCK_BORDER = 1; // px
const FLAME_GRAPH_BLOCK_TEXT_FONT_SIZE = 9; // px
const FLAME_GRAPH_BLOCK_TEXT_FONT_FAMILY = "sans-serif";
const FLAME_GRAPH_BLOCK_TEXT_PADDING_TOP = 0; // px
const FLAME_GRAPH_BLOCK_TEXT_PADDING_LEFT = 3; // px
const FLAME_GRAPH_BLOCK_TEXT_PADDING_RIGHT = 3; // px

/**
 * A flamegraph visualization. This implementation is responsable only with
 * drawing the graph, using a data source consisting of rectangles and
 * their corresponding widths.
 *
 * Example usage:
 *   let graph = new FlameGraph(node);
 *   graph.once("ready", () => {
 *     let data = FlameGraphUtils.createFlameGraphDataFromSamples(samples);
 *     let bounds = { startTime, endTime };
 *     graph.setData({ data, bounds });
 *   });
 *
 * Data source format:
 *   [
 *     {
 *       color: "string",
 *       blocks: [
 *         {
 *           x: number,
 *           y: number,
 *           width: number,
 *           height: number,
 *           text: "string"
 *         },
 *         ...
 *       ]
 *     },
 *     {
 *       color: "string",
 *       blocks: [...]
 *     },
 *     ...
 *     {
 *       color: "string",
 *       blocks: [...]
 *     }
 *   ]
 *
 * Use `FlameGraphUtils` to convert profiler data (or any other data source)
 * into a drawable format.
 *
 * @param nsIDOMNode parent
 *        The parent node holding the graph.
 * @param number sharpness [optional]
 *        Defaults to the current device pixel ratio.
 */
function FlameGraph(parent, sharpness) {
  EventEmitter.decorate(this);

  this._parent = parent;
  this._ready = Promise.defer();

  this.setTheme();

  AbstractCanvasGraph.createIframe(GRAPH_SRC, parent, iframe => {
    this._iframe = iframe;
    this._window = iframe.contentWindow;
    this._document = iframe.contentDocument;
    this._pixelRatio = sharpness || this._window.devicePixelRatio;

    let container = this._container = this._document.getElementById("graph-container");
    container.className = "flame-graph-widget-container graph-widget-container";

    let canvas = this._canvas = this._document.getElementById("graph-canvas");
    canvas.className = "flame-graph-widget-canvas graph-widget-canvas";

    let bounds = parent.getBoundingClientRect();
    bounds.width = this.fixedWidth || bounds.width;
    bounds.height = this.fixedHeight || bounds.height;
    iframe.setAttribute("width", bounds.width);
    iframe.setAttribute("height", bounds.height);

    this._width = canvas.width = bounds.width * this._pixelRatio;
    this._height = canvas.height = bounds.height * this._pixelRatio;
    this._ctx = canvas.getContext("2d");

    this._bounds = new GraphArea();
    this._selection = new GraphArea();
    this._selectionDragger = new GraphAreaDragger();
    this._verticalOffset = 0;
    this._verticalOffsetDragger = new GraphAreaDragger(0);

    // Calculating text widths is necessary to trim the text inside the blocks
    // while the scaling changes (e.g. via scrolling). This is very expensive,
    // so maintain a cache of string contents to text widths.
    this._textWidthsCache = {};

    let fontSize = FLAME_GRAPH_BLOCK_TEXT_FONT_SIZE * this._pixelRatio;
    let fontFamily = FLAME_GRAPH_BLOCK_TEXT_FONT_FAMILY;
    this._ctx.font = fontSize + "px " + fontFamily;
    this._averageCharWidth = this._calcAverageCharWidth();
    this._overflowCharWidth = this._getTextWidth(this.overflowChar);

    this._onAnimationFrame = this._onAnimationFrame.bind(this);
    this._onMouseMove = this._onMouseMove.bind(this);
    this._onMouseDown = this._onMouseDown.bind(this);
    this._onMouseUp = this._onMouseUp.bind(this);
    this._onMouseWheel = this._onMouseWheel.bind(this);
    this._onResize = this._onResize.bind(this);
    this.refresh = this.refresh.bind(this);

    this._window.addEventListener("mousemove", this._onMouseMove);
    this._window.addEventListener("mousedown", this._onMouseDown);
    this._window.addEventListener("mouseup", this._onMouseUp);
    this._window.addEventListener("MozMousePixelScroll", this._onMouseWheel);

    let ownerWindow = this._parent.ownerDocument.defaultView;
    ownerWindow.addEventListener("resize", this._onResize);

    this._animationId = this._window.requestAnimationFrame(this._onAnimationFrame);

    this._ready.resolve(this);
    this.emit("ready", this);
  });
}

FlameGraph.prototype = {
  /**
   * Read-only width and height of the canvas.
   * @return number
   */
  get width() {
    return this._width;
  },
  get height() {
    return this._height;
  },

  /**
   * Returns a promise resolved once this graph is ready to receive data.
   */
  ready: function() {
    return this._ready.promise;
  },

  /**
   * Destroys this graph.
   */
  destroy: Task.async(function*() {
    yield this.ready();

    this._window.removeEventListener("mousemove", this._onMouseMove);
    this._window.removeEventListener("mousedown", this._onMouseDown);
    this._window.removeEventListener("mouseup", this._onMouseUp);
    this._window.removeEventListener("MozMousePixelScroll", this._onMouseWheel);

    let ownerWindow = this._parent.ownerDocument.defaultView;
    if (ownerWindow) {
      ownerWindow.removeEventListener("resize", this._onResize);
    }

    this._window.cancelAnimationFrame(this._animationId);
    this._iframe.remove();

    this._bounds = null;
    this._selection = null;
    this._selectionDragger = null;
    this._verticalOffset = null;
    this._verticalOffsetDragger = null;
    this._textWidthsCache = null;

    this._data = null;

    this.emit("destroyed");
  }),

  /**
   * Makes sure the canvas graph is of the specified width or height, and
   * doesn't flex to fit all the available space.
   */
  fixedWidth: null,
  fixedHeight: null,

  /**
   * How much preliminar drag is necessary to determine the panning direction.
   */
  horizontalPanThreshold: GRAPH_HORIZONTAL_PAN_THRESHOLD,
  verticalPanThreshold: GRAPH_VERTICAL_PAN_THRESHOLD,

  /**
   * The units used in the overhead ticks. Could be "ms", for example.
   * Overwrite this with your own localized format.
   */
  timelineTickUnits: "",

  /**
   * Character used when a block's text is overflowing.
   * Defaults to an ellipsis.
   */
  overflowChar: L10N.ellipsis,

  /**
   * Sets the data source for this graph.
   *
   * @param object data
   *        An object containing the following properties:
   *          - data: the data source; see the constructor for more info
   *          - bounds: the minimum/maximum { start, end }, in ms or px
   *          - visible: optional, the shown { start, end }, in ms or px
   */
  setData: function({ data, bounds, visible }) {
    this._data = data;
    this.setOuterBounds(bounds);
    this.setViewRange(visible || bounds);
  },

  /**
   * Same as `setData`, but waits for this graph to finish initializing first.
   *
   * @param object data
   *        The data source. See the constructor for more information.
   * @return promise
   *         A promise resolved once the data is set.
   */
  setDataWhenReady: Task.async(function*(data) {
    yield this.ready();
    this.setData(data);
  }),

  /**
   * Gets whether or not this graph has a data source.
   * @return boolean
   */
  hasData: function() {
    return !!this._data;
  },

  /**
   * Sets the maximum selection (i.e. the 'graph bounds').
   * @param object { start, end }
   */
  setOuterBounds: function({ startTime, endTime }) {
    this._bounds.start = startTime * this._pixelRatio;
    this._bounds.end = endTime * this._pixelRatio;
    this._shouldRedraw = true;
  },

  /**
   * Sets the selection and vertical offset (i.e. the 'view range').
   * @return number
   */
  setViewRange: function({ startTime, endTime }, verticalOffset = 0) {
    this._selection.start = startTime * this._pixelRatio;
    this._selection.end = endTime * this._pixelRatio;
    this._verticalOffset = verticalOffset * this._pixelRatio;
    this._shouldRedraw = true;
  },

  /**
   * Gets the maximum selection (i.e. the 'graph bounds').
   * @return number
   */
  getOuterBounds: function() {
    return {
      startTime: this._bounds.start / this._pixelRatio,
      endTime: this._bounds.end / this._pixelRatio
    };
  },

  /**
   * Gets the current selection and vertical offset (i.e. the 'view range').
   * @return number
   */
  getViewRange: function() {
    return {
      startTime: this._selection.start / this._pixelRatio,
      endTime: this._selection.end / this._pixelRatio,
      verticalOffset: this._verticalOffset / this._pixelRatio
    };
  },

  /**
   * Updates this graph to reflect the new dimensions of the parent node.
   *
   * @param boolean options.force
   *        Force redraw everything.
   */
  refresh: function(options={}) {
    let bounds = this._parent.getBoundingClientRect();
    let newWidth = this.fixedWidth || bounds.width;
    let newHeight = this.fixedHeight || bounds.height;

    // Prevent redrawing everything if the graph's width & height won't change,
    // except if force=true.
    if (!options.force &&
        this._width == newWidth * this._pixelRatio &&
        this._height == newHeight * this._pixelRatio) {
      this.emit("refresh-cancelled");
      return;
    }

    bounds.width = newWidth;
    bounds.height = newHeight;
    this._iframe.setAttribute("width", bounds.width);
    this._iframe.setAttribute("height", bounds.height);
    this._width = this._canvas.width = bounds.width * this._pixelRatio;
    this._height = this._canvas.height = bounds.height * this._pixelRatio;

    this._shouldRedraw = true;
    this.emit("refresh");
  },

  /**
   * Sets the theme via `theme` to either "light" or "dark",
   * and updates the internal styling to match. Requires a redraw
   * to see the effects.
   */
  setTheme: function (theme) {
    theme = theme || "light";
    this.overviewHeaderBackgroundColor = getColor("body-background", theme);
    this.overviewHeaderTextColor = getColor("body-color", theme);
    // Hard to get a color that is readable across both themes for the text on the flames
    this.blockTextColor = getColor(theme === "dark" ? "selection-color" : "body-color", theme);
  },

  /**
   * The contents of this graph are redrawn only when something changed,
   * like the data source, or the selection bounds etc. This flag tracks
   * if the rendering is "dirty" and needs to be refreshed.
   */
  _shouldRedraw: false,

  /**
   * Animation frame callback, invoked on each tick of the refresh driver.
   */
  _onAnimationFrame: function() {
    this._animationId = this._window.requestAnimationFrame(this._onAnimationFrame);
    this._drawWidget();
  },

  /**
   * Redraws the widget when necessary. The actual graph is not refreshed
   * every time this function is called, only the cliphead, selection etc.
   */
  _drawWidget: function() {
    if (!this._shouldRedraw) {
      return;
    }
    let ctx = this._ctx;
    let canvasWidth = this._width;
    let canvasHeight = this._height;
    ctx.clearRect(0, 0, canvasWidth, canvasHeight);

    let selection = this._selection;
    let selectionWidth = selection.end - selection.start;
    let selectionScale = canvasWidth / selectionWidth;
    this._drawTicks(selection.start, selectionScale);
    this._drawPyramid(this._data, this._verticalOffset, selection.start, selectionScale);
    this._drawHeader(selection.start, selectionScale);

    this._shouldRedraw = false;
  },

  /**
   * Draws the overhead header, with time markers and ticks in this graph.
   *
   * @param number dataOffset, dataScale
   *        Offsets and scales the data source by the specified amount.
   *        This is used for scrolling the visualization.
   */
  _drawHeader: function(dataOffset, dataScale) {
    let ctx = this._ctx;
    let canvasWidth = this._width;
    let headerHeight = OVERVIEW_HEADER_HEIGHT * this._pixelRatio;

    ctx.fillStyle = this.overviewHeaderBackgroundColor;
    ctx.fillRect(0, 0, canvasWidth, headerHeight);

    this._drawTicks(dataOffset, dataScale, { from: 0, to: headerHeight, renderText: true });
  },

  /**
   * Draws the overhead ticks in this graph in the flame graph area.
   *
   * @param number dataOffset, dataScale, from, to, renderText
   *        Offsets and scales the data source by the specified amount.
   *        from and to determine the Y position of how far the stroke
   *        should be drawn.
   *        This is used when scrolling the visualization.
   */
  _drawTicks: function(dataOffset, dataScale, options) {
    let { from, to, renderText }  = options || {};
    let ctx = this._ctx;
    let canvasWidth = this._width;
    let canvasHeight = this._height;
    let scaledOffset = dataOffset * dataScale;

    let fontSize = OVERVIEW_HEADER_TEXT_FONT_SIZE * this._pixelRatio;
    let fontFamily = OVERVIEW_HEADER_TEXT_FONT_FAMILY;
    let textPaddingLeft = OVERVIEW_HEADER_TEXT_PADDING_LEFT * this._pixelRatio;
    let textPaddingTop = OVERVIEW_HEADER_TEXT_PADDING_TOP * this._pixelRatio;
    let tickInterval = this._findOptimalTickInterval(dataScale);

    ctx.textBaseline = "top";
    ctx.font = fontSize + "px " + fontFamily;
    ctx.fillStyle = this.overviewHeaderTextColor;
    ctx.strokeStyle = OVERVIEW_HEADER_TIMELINE_STROKE_COLOR;
    ctx.beginPath();

    for (let x = -scaledOffset % tickInterval; x < canvasWidth; x += tickInterval) {
      let lineLeft = x;
      let textLeft = lineLeft + textPaddingLeft;
      let time = Math.round((x / dataScale + dataOffset) / this._pixelRatio);
      let label = time + " " + this.timelineTickUnits;
      if (renderText) {
        ctx.fillText(label, textLeft, textPaddingTop);
      }
      ctx.moveTo(lineLeft, from || 0);
      ctx.lineTo(lineLeft, to || canvasHeight);
    }

    ctx.stroke();
  },

  /**
   * Draws the blocks and text in this graph.
   *
   * @param object dataSource
   *        The data source. See the constructor for more information.
   * @param number verticalOffset
   *        Offsets the drawing vertically by the specified amount.
   * @param number dataOffset, dataScale
   *        Offsets and scales the data source by the specified amount.
   *        This is used for scrolling the visualization.
   */
  _drawPyramid: function(dataSource, verticalOffset, dataOffset, dataScale) {
    let ctx = this._ctx;

    let fontSize = FLAME_GRAPH_BLOCK_TEXT_FONT_SIZE * this._pixelRatio;
    let fontFamily = FLAME_GRAPH_BLOCK_TEXT_FONT_FAMILY;
    let visibleBlocksInfo = this._drawPyramidFill(dataSource, verticalOffset, dataOffset, dataScale);

    ctx.textBaseline = "middle";
    ctx.font = fontSize + "px " + fontFamily;
    ctx.fillStyle = this.blockTextColor;

    this._drawPyramidText(visibleBlocksInfo, verticalOffset, dataOffset, dataScale);
  },

  /**
   * Fills all block inside this graph's pyramid.
   * @see FlameGraph.prototype._drawPyramid
   */
  _drawPyramidFill: function(dataSource, verticalOffset, dataOffset, dataScale) {
    let visibleBlocksInfoStore = [];
    let minVisibleBlockWidth = this._overflowCharWidth;

    for (let { color, blocks } of dataSource) {
      this._drawBlocksFill(
        color, blocks, verticalOffset, dataOffset, dataScale,
        visibleBlocksInfoStore, minVisibleBlockWidth);
    }

    return visibleBlocksInfoStore;
  },

  /**
   * Adds the text for all block inside this graph's pyramid.
   * @see FlameGraph.prototype._drawPyramid
   */
  _drawPyramidText: function(blocksInfo, verticalOffset, dataOffset, dataScale) {
    for (let { block, rect } of blocksInfo) {
      this._drawBlockText(block, rect, verticalOffset, dataOffset, dataScale);
    }
  },

  /**
   * Fills a group of blocks sharing the same style.
   *
   * @param string color
   *        The color used as the block's background.
   * @param array blocks
   *        A list of { x, y, width, height } objects visually representing
   *        all the blocks sharing this particular style.
   * @param number verticalOffset
   *        Offsets the drawing vertically by the specified amount.
   * @param number dataOffset, dataScale
   *        Offsets and scales the data source by the specified amount.
   *        This is used for scrolling the visualization.
   * @param array visibleBlocksInfoStore
   *        An array to store all the visible blocks into, along with the
   *        final baked coordinates and dimensions, after drawing them.
   *        The provided array will be populated.
   * @param number minVisibleBlockWidth
   *        The minimum width of the blocks that will be added into
   *        the `visibleBlocksInfoStore`.
   */
  _drawBlocksFill: function(
    color, blocks, verticalOffset, dataOffset, dataScale,
    visibleBlocksInfoStore, minVisibleBlockWidth)
  {
    let ctx = this._ctx;
    let canvasWidth = this._width;
    let canvasHeight = this._height;
    let scaledOffset = dataOffset * dataScale;

    ctx.fillStyle = color;
    ctx.beginPath();

    for (let block of blocks) {
      let { x, y, width, height } = block;
      let rectLeft = x * this._pixelRatio * dataScale - scaledOffset;
      let rectTop = (y - verticalOffset + OVERVIEW_HEADER_HEIGHT) * this._pixelRatio;
      let rectWidth = width * this._pixelRatio * dataScale;
      let rectHeight = height * this._pixelRatio;

      if (rectLeft > canvasWidth || // Too far right.
          rectLeft < -rectWidth ||  // Too far left.
          rectTop > canvasHeight || // Too far bottom.
          rectTop < -rectHeight) {  // Too far top.
        continue;
      }

      // Clamp the blocks position to start at 0. Avoid negative X coords,
      // to properly place the text inside the blocks.
      if (rectLeft < 0) {
        rectWidth += rectLeft;
        rectLeft = 0;
      }

      // Avoid drawing blocks that are too narrow.
      if (rectWidth <= FLAME_GRAPH_BLOCK_BORDER ||
          rectHeight <= FLAME_GRAPH_BLOCK_BORDER) {
        continue;
      }

      ctx.rect(
        rectLeft, rectTop,
        rectWidth - FLAME_GRAPH_BLOCK_BORDER,
        rectHeight - FLAME_GRAPH_BLOCK_BORDER);

      // Populate the visible blocks store with this block if the width
      // is longer than a given threshold.
      if (rectWidth > minVisibleBlockWidth) {
        visibleBlocksInfoStore.push({
          block: block,
          rect: { rectLeft, rectTop, rectWidth, rectHeight }
        });
      }
    }

    ctx.fill();
  },

  /**
   * Adds text for a single block.
   *
   * @param object block
   *        A single { x, y, width, height, text } object visually representing
   *        the block containing the text.
   * @param object rect
   *        A single { rectLeft, rectTop, rectWidth, rectHeight } object
   *        representing the final baked coordinates of the drawn rectangle.
   *        Think of them as screen-space values, vs. object-space values. These
   *        differ from the scalars in `block` when the graph is scaled/panned.
   * @param number verticalOffset
   *        Offsets the drawing vertically by the specified amount.
   * @param number dataOffset, dataScale
   *        Offsets and scales the data source by the specified amount.
   *        This is used for scrolling the visualization.
   */
  _drawBlockText: function(block, rect, verticalOffset, dataOffset, dataScale) {
    let ctx = this._ctx;
    let scaledOffset = dataOffset * dataScale;

    let { x, y, width, height, text } = block;
    let { rectLeft, rectTop, rectWidth, rectHeight } = rect;

    let paddingTop = FLAME_GRAPH_BLOCK_TEXT_PADDING_TOP * this._pixelRatio;
    let paddingLeft = FLAME_GRAPH_BLOCK_TEXT_PADDING_LEFT * this._pixelRatio;
    let paddingRight = FLAME_GRAPH_BLOCK_TEXT_PADDING_RIGHT * this._pixelRatio;
    let totalHorizontalPadding = paddingLeft + paddingRight;

    // Clamp the blocks position to start at 0. Avoid negative X coords,
    // to properly place the text inside the blocks.
    if (rectLeft < 0) {
      rectWidth += rectLeft;
      rectLeft = 0;
    }

    let textLeft = rectLeft + paddingLeft;
    let textTop = rectTop + rectHeight / 2 + paddingTop;
    let textAvailableWidth = rectWidth - totalHorizontalPadding;

    // Massage the text to fit inside a given width. This clamps the string
    // at the end to avoid overflowing.
    let fittedText = this._getFittedText(text, textAvailableWidth);
    if (fittedText.length < 1) {
      return;
    }

    ctx.fillText(fittedText, textLeft, textTop);
  },

  /**
   * Calculating text widths is necessary to trim the text inside the blocks
   * while the scaling changes (e.g. via scrolling). This is very expensive,
   * so maintain a cache of string contents to text widths.
   */
  _textWidthsCache: null,
  _overflowCharWidth: null,
  _averageCharWidth: null,

  /**
   * Gets the width of the specified text, for the current context state
   * (font size, family etc.).
   *
   * @param string text
   *        The text to analyze.
   * @return number
   *         The text width.
   */
  _getTextWidth: function(text) {
    let cachedWidth = this._textWidthsCache[text];
    if (cachedWidth) {
      return cachedWidth;
    }
    let metrics = this._ctx.measureText(text);
    return (this._textWidthsCache[text] = metrics.width);
  },

  /**
   * Gets an approximate width of the specified text. This is much faster
   * than `_getTextWidth`, but inexact.
   *
   * @param string text
   *        The text to analyze.
   * @return number
   *         The approximate text width.
   */
  _getTextWidthApprox: function(text) {
    return text.length * this._averageCharWidth;
  },

  /**
   * Gets the average letter width in the English alphabet, for the current
   * context state (font size, family etc.). This provides a close enough
   * value to use in `_getTextWidthApprox`.
   *
   * @return number
   *         The average letter width.
   */
  _calcAverageCharWidth: function() {
    let letterWidthsSum = 0;
    let start = 32; // space
    let end = 123; // "z"

    for (let i = start; i < end; i++) {
      let char = String.fromCharCode(i);
      letterWidthsSum += this._getTextWidth(char);
    }

    return letterWidthsSum / (end - start);
  },

  /**
   * Massage a text to fit inside a given width. This clamps the string
   * at the end to avoid overflowing.
   *
   * @param string text
   *        The text to fit inside the given width.
   * @param number maxWidth
   *        The available width for the given text.
   * @return string
   *         The fitted text.
   */
  _getFittedText: function(text, maxWidth) {
    let textWidth = this._getTextWidth(text);
    if (textWidth < maxWidth) {
      return text;
    }
    if (this._overflowCharWidth > maxWidth) {
      return "";
    }
    for (let i = 1, len = text.length; i <= len; i++) {
      let trimmedText = text.substring(0, len - i);
      let trimmedWidth = this._getTextWidthApprox(trimmedText) + this._overflowCharWidth;
      if (trimmedWidth < maxWidth) {
        return trimmedText + this.overflowChar;
      }
    }
    return "";
  },

  /**
   * Listener for the "mousemove" event on the graph's container.
   */
  _onMouseMove: function(e) {
    let offset = this._getContainerOffset();
    let mouseX = (e.clientX - offset.left) * this._pixelRatio;
    let mouseY = (e.clientY - offset.top) * this._pixelRatio;

    let canvasWidth = this._width;
    let canvasHeight = this._height;

    let selection = this._selection;
    let selectionWidth = selection.end - selection.start;
    let selectionScale = canvasWidth / selectionWidth;

    let horizDrag = this._selectionDragger;
    let vertDrag = this._verticalOffsetDragger;

    // Avoid dragging both horizontally and vertically at the same time,
    // as this doesn't feel natural. Based on a minimum distance, enable either
    // one, and remember the drag direction to offset the mouse coords later.
    if (!this._horizontalDragEnabled && !this._verticalDragEnabled) {
      let horizDiff = Math.abs(horizDrag.origin - mouseX);
      if (horizDiff > this.horizontalPanThreshold) {
        this._horizontalDragDirection = Math.sign(horizDrag.origin - mouseX);
        this._horizontalDragEnabled = true;
      }
      let vertDiff = Math.abs(vertDrag.origin - mouseY);
      if (vertDiff > this.verticalPanThreshold) {
        this._verticalDragDirection = Math.sign(vertDrag.origin - mouseY);
        this._verticalDragEnabled = true;
      }
    }

    if (horizDrag.origin != null && this._horizontalDragEnabled) {
      let relativeX = mouseX + this._horizontalDragDirection * this.horizontalPanThreshold;
      selection.start = horizDrag.anchor.start + (horizDrag.origin - relativeX) / selectionScale;
      selection.end = horizDrag.anchor.end + (horizDrag.origin - relativeX) / selectionScale;
      this._normalizeSelectionBounds();
      this._shouldRedraw = true;
      this.emit("selecting");
    }

    if (vertDrag.origin != null && this._verticalDragEnabled) {
      let relativeY = mouseY + this._verticalDragDirection * this.verticalPanThreshold;
      this._verticalOffset = vertDrag.anchor + (vertDrag.origin - relativeY) / this._pixelRatio;
      this._normalizeVerticalOffset();
      this._shouldRedraw = true;
      this.emit("panning-vertically");
    }
  },

  /**
   * Listener for the "mousedown" event on the graph's container.
   */
  _onMouseDown: function(e) {
    let offset = this._getContainerOffset();
    let mouseX = (e.clientX - offset.left) * this._pixelRatio;
    let mouseY = (e.clientY - offset.top) * this._pixelRatio;

    this._selectionDragger.origin = mouseX;
    this._selectionDragger.anchor.start = this._selection.start;
    this._selectionDragger.anchor.end = this._selection.end;

    this._verticalOffsetDragger.origin = mouseY;
    this._verticalOffsetDragger.anchor = this._verticalOffset;

    this._horizontalDragEnabled = false;
    this._verticalDragEnabled = false;

    this._canvas.setAttribute("input", "adjusting-view-area");
  },

  /**
   * Listener for the "mouseup" event on the graph's container.
   */
  _onMouseUp: function() {
    this._selectionDragger.origin = null;
    this._verticalOffsetDragger.origin = null;
    this._horizontalDragEnabled = false;
    this._horizontalDragDirection = 0;
    this._verticalDragEnabled = false;
    this._verticalDragDirection = 0;
    this._canvas.removeAttribute("input");
  },

  /**
   * Listener for the "wheel" event on the graph's container.
   */
  _onMouseWheel: function(e) {
    let offset = this._getContainerOffset();
    let mouseX = (e.clientX - offset.left) * this._pixelRatio;

    let canvasWidth = this._width;
    let canvasHeight = this._height;

    let selection = this._selection;
    let selectionWidth = selection.end - selection.start;
    let selectionScale = canvasWidth / selectionWidth;

    switch (e.axis) {
      case e.VERTICAL_AXIS: {
        let distFromStart = mouseX;
        let distFromEnd = canvasWidth - mouseX;
        let vector = e.detail * GRAPH_WHEEL_ZOOM_SENSITIVITY / selectionScale;
        selection.start -= distFromStart * vector;
        selection.end += distFromEnd * vector;
        break;
      }
      case e.HORIZONTAL_AXIS: {
        let vector = e.detail * GRAPH_WHEEL_SCROLL_SENSITIVITY / selectionScale;
        selection.start += vector;
        selection.end += vector;
        break;
      }
    }

    this._normalizeSelectionBounds();
    this._shouldRedraw = true;
    this.emit("selecting");
  },

  /**
   * Makes sure the start and end points of the current selection
   * are withing the graph's visible bounds, and that they form a selection
   * wider than the allowed minimum width.
   */
  _normalizeSelectionBounds: function() {
    let boundsStart = this._bounds.start;
    let boundsEnd = this._bounds.end;
    let selectionStart = this._selection.start;
    let selectionEnd = this._selection.end;

    if (selectionStart < boundsStart) {
      selectionStart = boundsStart;
    }
    if (selectionEnd < boundsStart) {
      selectionStart = boundsStart;
      selectionEnd = GRAPH_MIN_SELECTION_WIDTH;
    }
    if (selectionEnd > boundsEnd) {
      selectionEnd = boundsEnd;
    }
    if (selectionStart > boundsEnd) {
      selectionEnd = boundsEnd;
      selectionStart = boundsEnd - GRAPH_MIN_SELECTION_WIDTH;
    }
    if (selectionEnd - selectionStart < GRAPH_MIN_SELECTION_WIDTH) {
      let midPoint = (selectionStart + selectionEnd) / 2;
      selectionStart = midPoint - GRAPH_MIN_SELECTION_WIDTH / 2;
      selectionEnd = midPoint + GRAPH_MIN_SELECTION_WIDTH / 2;
    }

    this._selection.start = selectionStart;
    this._selection.end = selectionEnd;
  },

  /**
   * Makes sure that the current vertical offset is within the allowed
   * panning range.
   */
  _normalizeVerticalOffset: function() {
    this._verticalOffset = Math.max(this._verticalOffset, 0);
  },

  /**
   *
   * Finds the optimal tick interval between time markers in this graph.
   *
   * @param number dataScale
   * @return number
   */
  _findOptimalTickInterval: function(dataScale) {
    let timingStep = TIMELINE_TICKS_MULTIPLE;
    let spacingMin = TIMELINE_TICKS_SPACING_MIN * this._pixelRatio;
    let maxIters = FIND_OPTIMAL_TICK_INTERVAL_MAX_ITERS;
    let numIters = 0;

    if (dataScale > spacingMin) {
      return dataScale;
    }

    while (true) {
      let scaledStep = dataScale * timingStep;
      if (++numIters > maxIters) {
        return scaledStep;
      }
      if (scaledStep < spacingMin) {
        timingStep <<= 1;
        continue;
      }
      return scaledStep;
    }
  },

  /**
   * Gets the offset of this graph's container relative to the owner window.
   *
   * @return object
   *         The { left, top } offset.
   */
  _getContainerOffset: function() {
    let node = this._canvas;
    let x = 0;
    let y = 0;

    while ((node = node.offsetParent)) {
      x += node.offsetLeft;
      y += node.offsetTop;
    }

    return { left: x, top: y };
  },

  /**
   * Listener for the "resize" event on the graph's parent node.
   */
  _onResize: function() {
    if (this.hasData()) {
      setNamedTimeout(this._uid, GRAPH_RESIZE_EVENTS_DRAIN, this.refresh);
    }
  }
};

const FLAME_GRAPH_BLOCK_HEIGHT = 12; // px

const PALLETTE_SIZE = 10;
const PALLETTE_HUE_OFFSET = Math.random() * 90;
const PALLETTE_HUE_RANGE = 270;
const PALLETTE_SATURATION = 100;
const PALLETTE_BRIGHTNESS = 65;
const PALLETTE_OPACITY = 0.55;

const COLOR_PALLETTE = Array.from(Array(PALLETTE_SIZE)).map((_, i) => "hsla" +
  "(" + ((PALLETTE_HUE_OFFSET + (i / PALLETTE_SIZE * PALLETTE_HUE_RANGE))|0 % 360) +
  "," + PALLETTE_SATURATION + "%" +
  "," + PALLETTE_BRIGHTNESS + "%" +
  "," + PALLETTE_OPACITY +
  ")"
);

/**
 * A collection of utility functions converting various data sources
 * into a format drawable by the FlameGraph.
 */
let FlameGraphUtils = {
  _cache: new WeakMap(),

  /**
   * Converts a list of samples from the profiler data to something that's
   * drawable by a FlameGraph widget.
   *
   * The outputted data will be cached, so the next time this method is called
   * the previous output is returned. If this is undesirable, or should the
   * options change, use `removeFromCache`.
   *
   * @param array samples
   *        A list of { time, frames: [{ location }] } objects.
   * @param object options [optional]
   *        Additional options supported by this operation:
   *          - invertStack: specifies if the frames array in every sample
   *                         should be reversed
   *          - flattenRecursion: specifies if identical consecutive frames
   *                              should be omitted from the output
   *          - filterFrames: predicate used for filtering all frames, passing
   *                          in each frame, its index and the sample array
   *          - showIdleBlocks: adds "idle" blocks when no frames are available
   *                            using the provided localized text
   * @param array out [optional]
   *        An output storage to reuse for storing the flame graph data.
   * @return array
   *         The flame graph data.
   */
  createFlameGraphDataFromSamples: function(samples, options = {}, out = []) {
    let cached = this._cache.get(samples);
    if (cached) {
      return cached;
    }

    // 1. Create a map of colors to arrays, representing buckets of
    // blocks inside the flame graph pyramid sharing the same style.

    let buckets = new Map();

    for (let color of COLOR_PALLETTE) {
      buckets.set(color, []);
    }

    // 2. Populate the buckets by iterating over every frame in every sample.

    let prevTime = 0;
    let prevFrames = [];

    for (let { frames, time } of samples) {
      let frameIndex = 0;

      // Flatten recursion if preferred, by removing consecutive frames
      // sharing the same location.
      if (options.flattenRecursion) {
        frames = frames.filter(this._isConsecutiveDuplicate);
      }

      // Apply a provided filter function. This can be used, for example, to
      // filter out platform frames if only content-related function calls
      // should be taken into consideration.
      if (options.filterFrames) {
        frames = frames.filter(options.filterFrames);
      }

      // Invert the stack if preferred, reversing the frames array in place.
      if (options.invertStack) {
        frames.reverse();
      }

      // If no frames are available, add a pseudo "idle" block in between.
      if (options.showIdleBlocks && frames.length == 0) {
        frames = [{ location: options.showIdleBlocks || "", idle: true }];
      }

      for (let frame of frames) {
        let { location } = frame;
        let prevFrame = prevFrames[frameIndex];

        // Frames at the same location and the same depth will be reused.
        // If there is a block already created, change its width.
        if (prevFrame && prevFrame.srcData.rawLocation == location) {
          prevFrame.width = (time - prevFrame.srcData.startTime);
        }
        // Otherwise, create a new block for this frame at this depth,
        // using a simple location based salt for picking a color.
        else {
          let hash = this._getStringHash(location);
          let color = COLOR_PALLETTE[hash % PALLETTE_SIZE];
          let bucket = buckets.get(color);

          bucket.push(prevFrames[frameIndex] = {
            srcData: { startTime: prevTime, rawLocation: location },
            x: prevTime,
            y: frameIndex * FLAME_GRAPH_BLOCK_HEIGHT,
            width: time - prevTime,
            height: FLAME_GRAPH_BLOCK_HEIGHT,
            text: this._formatLabel(frame)
          });
        }

        frameIndex++;
      }

      // Previous frames at stack depths greater than the current sample's
      // maximum need to be nullified. It's nonsensical to reuse them.
      prevFrames.length = frameIndex;
      prevTime = time;
    }

    // 3. Convert the buckets into a data source usable by the FlameGraph.
    // This is a simple conversion from a Map to an Array.

    for (let [color, blocks] of buckets) {
      out.push({ color, blocks });
    }

    this._cache.set(samples, out);
    return out;
  },

  /**
   * Clears the cached flame graph data created for the given source.
   * @param any source
   */
  removeFromCache: function(source) {
    this._cache.delete(source);
  },

  /**
   * Checks if the provided frame is the same as the next one in a sample.
   *
   * @param object e
   *        An object containing a { location } property.
   * @param number index
   *        The index of the object in the parent array.
   * @param array array
   *        The parent array.
   * @return boolean
   *         True if the next frame shares the same location, false otherwise.
   */
  _isConsecutiveDuplicate: function(e, index, array) {
    return index < array.length - 1 && e.location != array[index + 1].location;
  },

  /**
   * Very dumb hashing of a string. Used to pick colors from a pallette.
   *
   * @param string input
   * @return number
   */
  _getStringHash: function(input) {
    const STRING_HASH_PRIME1 = 7;
    const STRING_HASH_PRIME2 = 31;

    let hash = STRING_HASH_PRIME1;

    for (let i = 0, len = input.length; i < len; i++) {
      hash *= STRING_HASH_PRIME2;
      hash += input.charCodeAt(i);

      if (hash > Number.MAX_SAFE_INTEGER / STRING_HASH_PRIME2) {
        return hash;
      }
    }

    return hash;
  },

  /**
   * Takes a FrameNode and returns a string that should be displayed
   * in its flame block.
   *
   * @param FrameNode frame
   * @return string
   */
  _formatLabel: function (frame) {
    // If an idle block, just return the location which will just be "(idle)" text
    // anyway.
    if (frame.idle) {
      return frame.location;
    }

    let { functionName, fileName, line } = FrameUtils.parseLocation(frame);
    let label = functionName;

    if (fileName) {
      label += ` (${fileName}${line != null ? (":" + line) : ""})`;
    }

    return label;
  }
};

exports.FlameGraph = FlameGraph;
exports.FlameGraphUtils = FlameGraphUtils;
exports.FLAME_GRAPH_BLOCK_HEIGHT = FLAME_GRAPH_BLOCK_HEIGHT;
