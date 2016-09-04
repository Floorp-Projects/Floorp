/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Task } = require("devtools/shared/task");
const { setNamedTimeout } = require("devtools/client/shared/widgets/view-helpers");
const { getCurrentZoom } = require("devtools/shared/layout/utils");

loader.lazyRequireGetter(this, "defer", "devtools/shared/defer");
loader.lazyRequireGetter(this, "EventEmitter",
  "devtools/shared/event-emitter");

loader.lazyImporter(this, "DevToolsWorker",
  "resource://devtools/shared/worker/worker.js");

const HTML_NS = "http://www.w3.org/1999/xhtml";
const GRAPH_SRC = "chrome://devtools/content/shared/widgets/graphs-frame.xhtml";
const WORKER_URL =
  "resource://devtools/client/shared/widgets/GraphsWorker.js";

// Generic constants.

// ms
const GRAPH_RESIZE_EVENTS_DRAIN = 100;
const GRAPH_WHEEL_ZOOM_SENSITIVITY = 0.00075;
const GRAPH_WHEEL_SCROLL_SENSITIVITY = 0.1;
// px
const GRAPH_WHEEL_MIN_SELECTION_WIDTH = 10;

// px
const GRAPH_SELECTION_BOUNDARY_HOVER_LINE_WIDTH = 4;
const GRAPH_SELECTION_BOUNDARY_HOVER_THRESHOLD = 10;
const GRAPH_MAX_SELECTION_LEFT_PADDING = 1;
const GRAPH_MAX_SELECTION_RIGHT_PADDING = 1;

// px
const GRAPH_REGION_LINE_WIDTH = 1;
const GRAPH_REGION_LINE_COLOR = "rgba(237,38,85,0.8)";

// px
const GRAPH_STRIPE_PATTERN_WIDTH = 16;
const GRAPH_STRIPE_PATTERN_HEIGHT = 16;
const GRAPH_STRIPE_PATTERN_LINE_WIDTH = 2;
const GRAPH_STRIPE_PATTERN_LINE_SPACING = 4;

/**
 * Small data primitives for all graphs.
 */
this.GraphCursor = function () {
  this.x = null;
  this.y = null;
};

this.GraphArea = function () {
  this.start = null;
  this.end = null;
};

this.GraphAreaDragger = function (anchor = new GraphArea()) {
  this.origin = null;
  this.anchor = anchor;
};

this.GraphAreaResizer = function () {
  this.margin = null;
};

/**
 * Base class for all graphs using a canvas to render the data source. Handles
 * frame creation, data source, selection bounds, cursor position, etc.
 *
 * Language:
 *   - The "data" represents the values used when building the graph.
 *     Its specific format is defined by the inheriting classes.
 *
 *   - A "cursor" is the cliphead position across the X axis of the graph.
 *
 *   - A "selection" is defined by a "start" and an "end" value and
 *     represents the selected bounds in the graph.
 *
 *   - A "region" is a highlighted area in the graph, also defined by a
 *     "start" and an "end" value, but distinct from the "selection". It is
 *     simply used to highlight important regions in the data.
 *
 * Instances of this class are EventEmitters with the following events:
 *   - "ready": when the container iframe and canvas are created.
 *   - "selecting": when the selection is set or changed.
 *   - "deselecting": when the selection is dropped.
 *
 * @param nsIDOMNode parent
 *        The parent node holding the graph.
 * @param string name
 *        The graph type, used for setting the correct class names.
 *        Currently supported: "line-graph" only.
 * @param number sharpness [optional]
 *        Defaults to the current device pixel ratio.
 */
this.AbstractCanvasGraph = function (parent, name, sharpness) {
  EventEmitter.decorate(this);

  this._parent = parent;
  this._ready = defer();

  this._uid = "canvas-graph-" + Date.now();
  this._renderTargets = new Map();

  AbstractCanvasGraph.createIframe(GRAPH_SRC, parent, iframe => {
    this._iframe = iframe;
    this._window = iframe.contentWindow;
    this._topWindow = this._window.top;
    this._document = iframe.contentDocument;
    this._pixelRatio = sharpness || this._window.devicePixelRatio;

    let container =
      this._container = this._document.getElementById("graph-container");
    container.className = name + "-widget-container graph-widget-container";

    let canvas = this._canvas = this._document.getElementById("graph-canvas");
    canvas.className = name + "-widget-canvas graph-widget-canvas";

    let bounds = parent.getBoundingClientRect();
    bounds.width = this.fixedWidth || bounds.width;
    bounds.height = this.fixedHeight || bounds.height;
    iframe.setAttribute("width", bounds.width);
    iframe.setAttribute("height", bounds.height);

    this._width = canvas.width = bounds.width * this._pixelRatio;
    this._height = canvas.height = bounds.height * this._pixelRatio;
    this._ctx = canvas.getContext("2d");
    this._ctx.imageSmoothingEnabled = false;

    this._cursor = new GraphCursor();
    this._selection = new GraphArea();
    this._selectionDragger = new GraphAreaDragger();
    this._selectionResizer = new GraphAreaResizer();
    this._isMouseActive = false;

    this._onAnimationFrame = this._onAnimationFrame.bind(this);
    this._onMouseMove = this._onMouseMove.bind(this);
    this._onMouseDown = this._onMouseDown.bind(this);
    this._onMouseUp = this._onMouseUp.bind(this);
    this._onMouseWheel = this._onMouseWheel.bind(this);
    this._onMouseOut = this._onMouseOut.bind(this);
    this._onResize = this._onResize.bind(this);
    this.refresh = this.refresh.bind(this);

    this._window.addEventListener("mousemove", this._onMouseMove);
    this._window.addEventListener("mousedown", this._onMouseDown);
    this._window.addEventListener("MozMousePixelScroll", this._onMouseWheel);
    this._window.addEventListener("mouseout", this._onMouseOut);

    let ownerWindow = this._parent.ownerDocument.defaultView;
    ownerWindow.addEventListener("resize", this._onResize);

    this._animationId =
      this._window.requestAnimationFrame(this._onAnimationFrame);

    this._ready.resolve(this);
    this.emit("ready", this);
  });
};

AbstractCanvasGraph.prototype = {
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
   * Return true if the mouse is actively messing with the selection, false
   * otherwise.
   */
  get isMouseActive() {
    return this._isMouseActive;
  },

  /**
   * Returns a promise resolved once this graph is ready to receive data.
   */
  ready: function () {
    return this._ready.promise;
  },

  /**
   * Destroys this graph.
   */
  destroy: Task.async(function* () {
    yield this.ready();

    this._topWindow.removeEventListener("mousemove", this._onMouseMove);
    this._topWindow.removeEventListener("mouseup", this._onMouseUp);
    this._window.removeEventListener("mousemove", this._onMouseMove);
    this._window.removeEventListener("mousedown", this._onMouseDown);
    this._window.removeEventListener("MozMousePixelScroll", this._onMouseWheel);
    this._window.removeEventListener("mouseout", this._onMouseOut);

    let ownerWindow = this._parent.ownerDocument.defaultView;
    if (ownerWindow) {
      ownerWindow.removeEventListener("resize", this._onResize);
    }

    this._window.cancelAnimationFrame(this._animationId);
    this._iframe.remove();

    this._cursor = null;
    this._selection = null;
    this._selectionDragger = null;
    this._selectionResizer = null;

    this._data = null;
    this._mask = null;
    this._maskArgs = null;
    this._regions = null;

    this._cachedBackgroundImage = null;
    this._cachedGraphImage = null;
    this._cachedMaskImage = null;
    this._renderTargets.clear();
    gCachedStripePattern.clear();

    this.emit("destroyed");
  }),

  /**
   * Rendering options. Subclasses should override these.
   */
  clipheadLineWidth: 1,
  clipheadLineColor: "transparent",
  selectionLineWidth: 1,
  selectionLineColor: "transparent",
  selectionBackgroundColor: "transparent",
  selectionStripesColor: "transparent",
  regionBackgroundColor: "transparent",
  regionStripesColor: "transparent",

  /**
   * Makes sure the canvas graph is of the specified width or height, and
   * doesn't flex to fit all the available space.
   */
  fixedWidth: null,
  fixedHeight: null,

  /**
   * Optionally builds and caches a background image for this graph.
   * Inheriting classes may override this method.
   */
  buildBackgroundImage: function () {
    return null;
  },

  /**
   * Builds and caches a graph image, based on the data source supplied
   * in `setData`. The graph image is not rebuilt on each frame, but
   * only when the data source changes.
   */
  buildGraphImage: function () {
    let error = "This method needs to be implemented by inheriting classes.";
    throw new Error(error);
  },

  /**
   * Optionally builds and caches a mask image for this graph, composited
   * over the data image created via `buildGraphImage`. Inheriting classes
   * may override this method.
   */
  buildMaskImage: function () {
    return null;
  },

  /**
   * When setting the data source, the coordinates and values may be
   * stretched or squeezed on the X/Y axis, to fit into the available space.
   */
  dataScaleX: 1,
  dataScaleY: 1,

  /**
   * Sets the data source for this graph.
   *
   * @param object data
   *        The data source. The actual format is specified by subclasses.
   */
  setData: function (data) {
    this._data = data;
    this._cachedBackgroundImage = this.buildBackgroundImage();
    this._cachedGraphImage = this.buildGraphImage();
    this._shouldRedraw = true;
  },

  /**
   * Same as `setData`, but waits for this graph to finish initializing first.
   *
   * @param object data
   *        The data source. The actual format is specified by subclasses.
   * @return promise
   *         A promise resolved once the data is set.
   */
  setDataWhenReady: Task.async(function* (data) {
    yield this.ready();
    this.setData(data);
  }),

  /**
   * Adds a mask to this graph.
   *
   * @param any mask, options
   *        See `buildMaskImage` in inheriting classes for the required args.
   */
  setMask: function (mask, ...options) {
    this._mask = mask;
    this._maskArgs = [mask, ...options];
    this._cachedMaskImage = this.buildMaskImage.apply(this, this._maskArgs);
    this._shouldRedraw = true;
  },

  /**
   * Adds regions to this graph.
   *
   * See the "Language" section in the constructor documentation
   * for details about what "regions" represent.
   *
   * @param array regions
   *        A list of { start, end } values.
   */
  setRegions: function (regions) {
    if (!this._cachedGraphImage) {
      throw new Error("Can't highlight regions on a graph with " +
                      "no data displayed.");
    }
    if (this._regions) {
      throw new Error("Regions were already highlighted on the graph.");
    }
    this._regions = regions.map(e => ({
      start: e.start * this.dataScaleX,
      end: e.end * this.dataScaleX
    }));
    this._bakeRegions(this._regions, this._cachedGraphImage);
    this._shouldRedraw = true;
  },

  /**
   * Gets whether or not this graph has a data source.
   * @return boolean
   */
  hasData: function () {
    return !!this._data;
  },

  /**
   * Gets whether or not this graph has any mask applied.
   * @return boolean
   */
  hasMask: function () {
    return !!this._mask;
  },

  /**
   * Gets whether or not this graph has any regions.
   * @return boolean
   */
  hasRegions: function () {
    return !!this._regions;
  },

  /**
   * Sets the selection bounds.
   * Use `dropSelection` to remove the selection.
   *
   * If the bounds aren't different, no "selection" event is emitted.
   *
   * See the "Language" section in the constructor documentation
   * for details about what a "selection" represents.
   *
   * @param object selection
   *        The selection's { start, end } values.
   */
  setSelection: function (selection) {
    if (!selection || selection.start == null || selection.end == null) {
      throw new Error("Invalid selection coordinates");
    }
    if (!this.isSelectionDifferent(selection)) {
      return;
    }
    this._selection.start = selection.start;
    this._selection.end = selection.end;
    this._shouldRedraw = true;
    this.emit("selecting");
  },

  /**
   * Gets the selection bounds.
   * If there's no selection, the bounds have null values.
   *
   * @return object
   *         The selection's { start, end } values.
   */
  getSelection: function () {
    if (this.hasSelection()) {
      return { start: this._selection.start, end: this._selection.end };
    }
    if (this.hasSelectionInProgress()) {
      return { start: this._selection.start, end: this._cursor.x };
    }
    return { start: null, end: null };
  },

  /**
   * Sets the selection bounds, scaled to correlate with the data source ranges,
   * such that a [0, max width] selection maps to [first value, last value].
   *
   * @param object selection
   *        The selection's { start, end } values.
   * @param object { mapStart, mapEnd } mapping [optional]
   *        Invoked when retrieving the numbers in the data source representing
   *        the first and last values, on the X axis.
   */
  setMappedSelection: function (selection, mapping = {}) {
    if (!this.hasData()) {
      throw new Error("A data source is necessary for retrieving " +
                      "a mapped selection.");
    }
    if (!selection || selection.start == null || selection.end == null) {
      throw new Error("Invalid selection coordinates");
    }

    let { mapStart, mapEnd } = mapping;
    let startTime = (mapStart || (e => e.delta))(this._data[0]);
    let endTime = (mapEnd || (e => e.delta))(this._data[this._data.length - 1]);

    // The selection's start and end values are not guaranteed to be ascending.
    // Also make sure that the selection bounds fit inside the data bounds.
    let min = Math.max(Math.min(selection.start, selection.end), startTime);
    let max = Math.min(Math.max(selection.start, selection.end), endTime);
    min = map(min, startTime, endTime, 0, this._width);
    max = map(max, startTime, endTime, 0, this._width);

    this.setSelection({ start: min, end: max });
  },

  /**
   * Gets the selection bounds, scaled to correlate with the data source ranges,
   * such that a [0, max width] selection maps to [first value, last value].
   *
   * @param object { mapStart, mapEnd } mapping [optional]
   *        Invoked when retrieving the numbers in the data source representing
   *        the first and last values, on the X axis.
   * @return object
   *         The mapped selection's { min, max } values.
   */
  getMappedSelection: function (mapping = {}) {
    if (!this.hasData()) {
      throw new Error("A data source is necessary for retrieving a " +
                      "mapped selection.");
    }
    if (!this.hasSelection() && !this.hasSelectionInProgress()) {
      return { min: null, max: null };
    }

    let { mapStart, mapEnd } = mapping;
    let startTime = (mapStart || (e => e.delta))(this._data[0]);
    let endTime = (mapEnd || (e => e.delta))(this._data[this._data.length - 1]);

    // The selection's start and end values are not guaranteed to be ascending.
    // This can happen, for example, when click & dragging from right to left.
    // Also make sure that the selection bounds fit inside the canvas bounds.
    let selection = this.getSelection();
    let min = Math.max(Math.min(selection.start, selection.end), 0);
    let max = Math.min(Math.max(selection.start, selection.end), this._width);
    min = map(min, 0, this._width, startTime, endTime);
    max = map(max, 0, this._width, startTime, endTime);

    return { min: min, max: max };
  },

  /**
   * Removes the selection.
   */
  dropSelection: function () {
    if (!this.hasSelection() && !this.hasSelectionInProgress()) {
      return;
    }
    this._selection.start = null;
    this._selection.end = null;
    this._shouldRedraw = true;
    this.emit("deselecting");
  },

  /**
   * Gets whether or not this graph has a selection.
   * @return boolean
   */
  hasSelection: function () {
    return this._selection &&
      this._selection.start != null && this._selection.end != null;
  },

  /**
   * Gets whether or not a selection is currently being made, for example
   * via a click+drag operation.
   * @return boolean
   */
  hasSelectionInProgress: function () {
    return this._selection &&
      this._selection.start != null && this._selection.end == null;
  },

  /**
   * Specifies whether or not mouse selection is allowed.
   * @type boolean
   */
  selectionEnabled: true,

  /**
   * Sets the selection bounds.
   * Use `dropCursor` to hide the cursor.
   *
   * @param object cursor
   *        The cursor's { x, y } position.
   */
  setCursor: function (cursor) {
    if (!cursor || cursor.x == null || cursor.y == null) {
      throw new Error("Invalid cursor coordinates");
    }
    if (!this.isCursorDifferent(cursor)) {
      return;
    }
    this._cursor.x = cursor.x;
    this._cursor.y = cursor.y;
    this._shouldRedraw = true;
  },

  /**
   * Gets the cursor position.
   * If there's no cursor, the position has null values.
   *
   * @return object
   *         The cursor's { x, y } values.
   */
  getCursor: function () {
    return { x: this._cursor.x, y: this._cursor.y };
  },

  /**
   * Hides the cursor.
   */
  dropCursor: function () {
    if (!this.hasCursor()) {
      return;
    }
    this._cursor.x = null;
    this._cursor.y = null;
    this._shouldRedraw = true;
  },

  /**
   * Gets whether or not this graph has a visible cursor.
   * @return boolean
   */
  hasCursor: function () {
    return this._cursor && this._cursor.x != null;
  },

  /**
   * Specifies if this graph's selection is different from another one.
   *
   * @param object other
   *        The other graph's selection, as { start, end } values.
   */
  isSelectionDifferent: function (other) {
    if (!other) {
      return true;
    }
    let current = this.getSelection();
    return current.start != other.start || current.end != other.end;
  },

  /**
   * Specifies if this graph's cursor is different from another one.
   *
   * @param object other
   *        The other graph's position, as { x, y } values.
   */
  isCursorDifferent: function (other) {
    if (!other) {
      return true;
    }
    let current = this.getCursor();
    return current.x != other.x || current.y != other.y;
  },

  /**
   * Gets the width of the current selection.
   * If no selection is available, 0 is returned.
   *
   * @return number
   *         The selection width.
   */
  getSelectionWidth: function () {
    let selection = this.getSelection();
    return Math.abs(selection.start - selection.end);
  },

  /**
   * Gets the currently hovered region, if any.
   * If no region is currently hovered, null is returned.
   *
   * @return object
   *         The hovered region, as { start, end } values.
   */
  getHoveredRegion: function () {
    if (!this.hasRegions() || !this.hasCursor()) {
      return null;
    }
    let { x } = this._cursor;
    return this._regions.find(({ start, end }) =>
      (start < end && start < x && end > x) ||
      (start > end && end < x && start > x));
  },

  /**
   * Updates this graph to reflect the new dimensions of the parent node.
   *
   * @param boolean options.force
   *        Force redrawing everything
   */
  refresh: function (options = {}) {
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

    // Handle a changed size by mapping the old selection to the new width
    if (this._width && newWidth && this.hasSelection()) {
      let ratio = this._width / (newWidth * this._pixelRatio);
      this._selection.start = Math.round(this._selection.start / ratio);
      this._selection.end = Math.round(this._selection.end / ratio);
    }

    bounds.width = newWidth;
    bounds.height = newHeight;
    this._iframe.setAttribute("width", bounds.width);
    this._iframe.setAttribute("height", bounds.height);
    this._width = this._canvas.width = bounds.width * this._pixelRatio;
    this._height = this._canvas.height = bounds.height * this._pixelRatio;

    if (this.hasData()) {
      this._cachedBackgroundImage = this.buildBackgroundImage();
      this._cachedGraphImage = this.buildGraphImage();
    }
    if (this.hasMask()) {
      this._cachedMaskImage = this.buildMaskImage.apply(this, this._maskArgs);
    }
    if (this.hasRegions()) {
      this._bakeRegions(this._regions, this._cachedGraphImage);
    }

    this._shouldRedraw = true;
    this.emit("refresh");
  },

  /**
   * Gets a canvas with the specified name, for this graph.
   *
   * If it doesn't exist yet, it will be created, otherwise the cached instance
   * will be cleared and returned.
   *
   * @param string name
   *        The canvas name.
   * @param number width, height [optional]
   *        A custom width and height for the canvas. Defaults to this graph's
   *        container canvas width and height.
   */
  _getNamedCanvas: function (name, width = this._width, height = this._height) {
    let cachedRenderTarget = this._renderTargets.get(name);
    if (cachedRenderTarget) {
      let { canvas, ctx } = cachedRenderTarget;
      canvas.width = width;
      canvas.height = height;
      ctx.clearRect(0, 0, width, height);
      return cachedRenderTarget;
    }

    let canvas = this._document.createElementNS(HTML_NS, "canvas");
    let ctx = canvas.getContext("2d");
    canvas.width = width;
    canvas.height = height;

    let renderTarget = { canvas: canvas, ctx: ctx };
    this._renderTargets.set(name, renderTarget);
    return renderTarget;
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
  _onAnimationFrame: function () {
    this._animationId =
      this._window.requestAnimationFrame(this._onAnimationFrame);
    this._drawWidget();
  },

  /**
   * Redraws the widget when necessary. The actual graph is not refreshed
   * every time this function is called, only the cliphead, selection etc.
   */
  _drawWidget: function () {
    if (!this._shouldRedraw) {
      return;
    }
    let ctx = this._ctx;
    ctx.clearRect(0, 0, this._width, this._height);

    if (this._cachedGraphImage) {
      ctx.drawImage(this._cachedGraphImage, 0, 0, this._width, this._height);
    }
    if (this._cachedMaskImage) {
      ctx.globalCompositeOperation = "destination-out";
      ctx.drawImage(this._cachedMaskImage, 0, 0, this._width, this._height);
    }
    if (this._cachedBackgroundImage) {
      ctx.globalCompositeOperation = "destination-over";
      ctx.drawImage(this._cachedBackgroundImage, 0, 0,
                    this._width, this._height);
    }

    // Revert to the original global composition operation.
    if (this._cachedMaskImage || this._cachedBackgroundImage) {
      ctx.globalCompositeOperation = "source-over";
    }

    if (this.hasCursor()) {
      this._drawCliphead();
    }
    if (this.hasSelection() || this.hasSelectionInProgress()) {
      this._drawSelection();
    }

    this._shouldRedraw = false;
  },

  /**
   * Draws the cliphead, if available and necessary.
   */
  _drawCliphead: function () {
    if (this._isHoveringSelectionContentsOrBoundaries() ||
        this._isHoveringRegion()) {
      return;
    }

    let ctx = this._ctx;
    ctx.lineWidth = this.clipheadLineWidth;
    ctx.strokeStyle = this.clipheadLineColor;
    ctx.beginPath();
    ctx.moveTo(this._cursor.x, 0);
    ctx.lineTo(this._cursor.x, this._height);
    ctx.stroke();
  },

  /**
   * Draws the selection, if available and necessary.
   */
  _drawSelection: function () {
    let { start, end } = this.getSelection();
    let input = this._canvas.getAttribute("input");

    let ctx = this._ctx;
    ctx.strokeStyle = this.selectionLineColor;

    // Fill selection.

    let pattern = AbstractCanvasGraph.getStripePattern({
      ownerDocument: this._document,
      backgroundColor: this.selectionBackgroundColor,
      stripesColor: this.selectionStripesColor
    });
    ctx.fillStyle = pattern;
    let rectStart = Math.min(this._width, Math.max(0, start));
    let rectEnd = Math.min(this._width, Math.max(0, end));
    ctx.fillRect(rectStart, 0, rectEnd - rectStart, this._height);

    // Draw left boundary.

    if (input == "hovering-selection-start-boundary") {
      ctx.lineWidth = GRAPH_SELECTION_BOUNDARY_HOVER_LINE_WIDTH;
    } else {
      ctx.lineWidth = this.clipheadLineWidth;
    }
    ctx.beginPath();
    ctx.moveTo(start, 0);
    ctx.lineTo(start, this._height);
    ctx.stroke();

    // Draw right boundary.

    if (input == "hovering-selection-end-boundary") {
      ctx.lineWidth = GRAPH_SELECTION_BOUNDARY_HOVER_LINE_WIDTH;
    } else {
      ctx.lineWidth = this.clipheadLineWidth;
    }
    ctx.beginPath();
    ctx.moveTo(end, this._height);
    ctx.lineTo(end, 0);
    ctx.stroke();
  },

  /**
   * Draws regions into the cached graph image, created via `buildGraphImage`.
   * Called when new regions are set.
   */
  _bakeRegions: function (regions, destination) {
    let ctx = destination.getContext("2d");

    let pattern = AbstractCanvasGraph.getStripePattern({
      ownerDocument: this._document,
      backgroundColor: this.regionBackgroundColor,
      stripesColor: this.regionStripesColor
    });
    ctx.fillStyle = pattern;
    ctx.strokeStyle = GRAPH_REGION_LINE_COLOR;
    ctx.lineWidth = GRAPH_REGION_LINE_WIDTH;

    let y = -GRAPH_REGION_LINE_WIDTH;
    let height = this._height + GRAPH_REGION_LINE_WIDTH;

    for (let { start, end } of regions) {
      let x = start;
      let width = end - start;
      ctx.fillRect(x, y, width, height);
      ctx.strokeRect(x, y, width, height);
    }
  },

  /**
   * Checks whether the start handle of the selection is hovered.
   * @return boolean
   */
  _isHoveringStartBoundary: function () {
    if (!this.hasSelection() || !this.hasCursor()) {
      return false;
    }
    let { x } = this._cursor;
    let { start } = this._selection;
    let threshold = GRAPH_SELECTION_BOUNDARY_HOVER_THRESHOLD * this._pixelRatio;
    return Math.abs(start - x) < threshold;
  },

  /**
   * Checks whether the end handle of the selection is hovered.
   * @return boolean
   */
  _isHoveringEndBoundary: function () {
    if (!this.hasSelection() || !this.hasCursor()) {
      return false;
    }
    let { x } = this._cursor;
    let { end } = this._selection;
    let threshold = GRAPH_SELECTION_BOUNDARY_HOVER_THRESHOLD * this._pixelRatio;
    return Math.abs(end - x) < threshold;
  },

  /**
   * Checks whether the selection is hovered.
   * @return boolean
   */
  _isHoveringSelectionContents: function () {
    if (!this.hasSelection() || !this.hasCursor()) {
      return false;
    }
    let { x } = this._cursor;
    let { start, end } = this._selection;
    return (start < end && start < x && end > x) ||
           (start > end && end < x && start > x);
  },

  /**
   * Checks whether the selection or its handles are hovered.
   * @return boolean
   */
  _isHoveringSelectionContentsOrBoundaries: function () {
    return this._isHoveringSelectionContents() ||
           this._isHoveringStartBoundary() ||
           this._isHoveringEndBoundary();
  },

  /**
   * Checks whether a region is hovered.
   * @return boolean
   */
  _isHoveringRegion: function () {
    return !!this.getHoveredRegion();
  },

  /**
   * Given a MouseEvent, make it relative to this._canvas.
   * @return object {mouseX,mouseY}
   */
  _getRelativeEventCoordinates: function (e) {
    // For ease of testing, testX and testY can be passed in as the event
    // object.  If so, just return this.
    if ("testX" in e && "testY" in e) {
      return {
        mouseX: e.testX * this._pixelRatio,
        mouseY: e.testY * this._pixelRatio
      };
    }

    // This method is concerned with converting mouse event coordinates from
    // "screen space" to "local space" (in other words, relative to this
    // canvas's position, thus (0,0) would correspond to the upper left corner).
    // We can't simply use `clientX` and `clientY` because the given MouseEvent
    // object may be generated from events coming from other DOM nodes.
    // Therefore, we need to get a bounding box relative to the top document and
    // do some simple math to convert screen coords into local coords.
    // However, `getBoxQuads` may be a very costly operation depending on the
    // complexity of the "outside world" DOM, so cache the results until we
    // suspect they might change (e.g. on a resize).
    // It'd sure be nice if we could use `getBoundsWithoutFlushing`, but it's
    // not taking the document zoom factor into consideration consistently.
    if (!this._boundingBox || this._maybeDirtyBoundingBox) {
      let topDocument = this._topWindow.document;
      let boxQuad = this._canvas.getBoxQuads({ relativeTo: topDocument })[0];
      this._boundingBox = boxQuad;
      this._maybeDirtyBoundingBox = false;
    }

    let bb = this._boundingBox;
    let x = (e.screenX - this._topWindow.screenX) - bb.p1.x;
    let y = (e.screenY - this._topWindow.screenY) - bb.p1.y;

    // Don't allow the event coordinates to be bigger than the canvas
    // or less than 0.
    let maxX = bb.p2.x - bb.p1.x;
    let maxY = bb.p3.y - bb.p1.y;
    let mouseX = Math.max(0, Math.min(x, maxX)) * this._pixelRatio;
    let mouseY = Math.max(0, Math.min(y, maxY)) * this._pixelRatio;

    // The coordinates need to be modified with the current zoom level
    // to prevent them from being wrong.
    let zoom = getCurrentZoom(this._canvas);
    mouseX /= zoom;
    mouseY /= zoom;

    return {mouseX, mouseY};
  },

  /**
   * Listener for the "mousemove" event on the graph's container.
   */
  _onMouseMove: function (e) {
    let resizer = this._selectionResizer;
    let dragger = this._selectionDragger;

    // Need to stop propagation here, since this function can be bound
    // to both this._window and this._topWindow.  It's only attached to
    // this._topWindow during a drag event.  Null check here since tests
    // don't pass this method into the event object.
    if (e.stopPropagation && this._isMouseActive) {
      e.stopPropagation();
    }

    // If a mouseup happened outside the window and the current operation
    // is causing the selection to change, then end it.
    if (e.buttons == 0 && (this.hasSelectionInProgress() ||
                           resizer.margin != null ||
                           dragger.origin != null)) {
      this._onMouseUp();
      return;
    }

    let {mouseX, mouseY} = this._getRelativeEventCoordinates(e);
    this._cursor.x = mouseX;
    this._cursor.y = mouseY;

    if (resizer.margin != null) {
      this._selection[resizer.margin] = mouseX;
      this._shouldRedraw = true;
      this.emit("selecting");
      return;
    }

    if (dragger.origin != null) {
      this._selection.start = dragger.anchor.start - dragger.origin + mouseX;
      this._selection.end = dragger.anchor.end - dragger.origin + mouseX;
      this._shouldRedraw = true;
      this.emit("selecting");
      return;
    }

    if (this.hasSelectionInProgress()) {
      this._shouldRedraw = true;
      this.emit("selecting");
      return;
    }

    if (this.hasSelection()) {
      if (this._isHoveringStartBoundary()) {
        this._canvas.setAttribute("input", "hovering-selection-start-boundary");
        this._shouldRedraw = true;
        return;
      }
      if (this._isHoveringEndBoundary()) {
        this._canvas.setAttribute("input", "hovering-selection-end-boundary");
        this._shouldRedraw = true;
        return;
      }
      if (this._isHoveringSelectionContents()) {
        this._canvas.setAttribute("input", "hovering-selection-contents");
        this._shouldRedraw = true;
        return;
      }
    }

    let region = this.getHoveredRegion();
    if (region) {
      this._canvas.setAttribute("input", "hovering-region");
    } else {
      this._canvas.setAttribute("input", "hovering-background");
    }

    this._shouldRedraw = true;
  },

  /**
   * Listener for the "mousedown" event on the graph's container.
   */
  _onMouseDown: function (e) {
    this._isMouseActive = true;
    let {mouseX} = this._getRelativeEventCoordinates(e);

    switch (this._canvas.getAttribute("input")) {
      case "hovering-background":
      case "hovering-region":
        if (!this.selectionEnabled) {
          break;
        }
        this._selection.start = mouseX;
        this._selection.end = null;
        this.emit("selecting");
        break;

      case "hovering-selection-start-boundary":
        this._selectionResizer.margin = "start";
        break;

      case "hovering-selection-end-boundary":
        this._selectionResizer.margin = "end";
        break;

      case "hovering-selection-contents":
        this._selectionDragger.origin = mouseX;
        this._selectionDragger.anchor.start = this._selection.start;
        this._selectionDragger.anchor.end = this._selection.end;
        this._canvas.setAttribute("input", "dragging-selection-contents");
        break;
    }

    // During a drag, bind to the top level window so that mouse movement
    // outside of this frame will still work.
    this._topWindow.addEventListener("mousemove", this._onMouseMove);
    this._topWindow.addEventListener("mouseup", this._onMouseUp);

    this._shouldRedraw = true;
    this.emit("mousedown");
  },

  /**
   * Listener for the "mouseup" event on the graph's container.
   */
  _onMouseUp: function () {
    this._isMouseActive = false;
    switch (this._canvas.getAttribute("input")) {
      case "hovering-background":
      case "hovering-region":
        if (!this.selectionEnabled) {
          break;
        }
        if (this.getSelectionWidth() < 1) {
          let region = this.getHoveredRegion();
          if (region) {
            this._selection.start = region.start;
            this._selection.end = region.end;
            this.emit("selecting");
          } else {
            this._selection.start = null;
            this._selection.end = null;
            this.emit("deselecting");
          }
        } else {
          this._selection.end = this._cursor.x;
          this.emit("selecting");
        }
        break;

      case "hovering-selection-start-boundary":
      case "hovering-selection-end-boundary":
        this._selectionResizer.margin = null;
        break;

      case "dragging-selection-contents":
        this._selectionDragger.origin = null;
        this._canvas.setAttribute("input", "hovering-selection-contents");
        break;
    }

    // No longer dragging, no need to bind to the top level window.
    this._topWindow.removeEventListener("mousemove", this._onMouseMove);
    this._topWindow.removeEventListener("mouseup", this._onMouseUp);

    this._shouldRedraw = true;
    this.emit("mouseup");
  },

  /**
   * Listener for the "wheel" event on the graph's container.
   */
  _onMouseWheel: function (e) {
    if (!this.hasSelection()) {
      return;
    }

    let {mouseX} = this._getRelativeEventCoordinates(e);
    let focusX = mouseX;

    let selection = this._selection;
    let vector = 0;

    // If the selection is hovered, "zoom" towards or away the cursor,
    // by shrinking or growing the selection.
    if (this._isHoveringSelectionContentsOrBoundaries()) {
      let distStart = selection.start - focusX;
      let distEnd = selection.end - focusX;
      vector = e.detail * GRAPH_WHEEL_ZOOM_SENSITIVITY;
      selection.start = selection.start + distStart * vector;
      selection.end = selection.end + distEnd * vector;
    } else {
      // Otherwise, simply pan the selection towards the left or right.
      let direction = 0;
      if (focusX > selection.end) {
        direction = Math.sign(focusX - selection.end);
      } else if (focusX < selection.start) {
        direction = Math.sign(focusX - selection.start);
      }
      vector = direction * e.detail * GRAPH_WHEEL_SCROLL_SENSITIVITY;
      selection.start -= vector;
      selection.end -= vector;
    }

    // Make sure the selection bounds are still comfortably inside the
    // graph's bounds when zooming out, to keep the margin handles accessible.

    let minStart = GRAPH_MAX_SELECTION_LEFT_PADDING;
    let maxEnd = this._width - GRAPH_MAX_SELECTION_RIGHT_PADDING;
    if (selection.start < minStart) {
      selection.start = minStart;
    }
    if (selection.start > maxEnd) {
      selection.start = maxEnd;
    }
    if (selection.end < minStart) {
      selection.end = minStart;
    }
    if (selection.end > maxEnd) {
      selection.end = maxEnd;
    }

    // Make sure the selection doesn't get too narrow when zooming in.

    let thickness = Math.abs(selection.start - selection.end);
    if (thickness < GRAPH_WHEEL_MIN_SELECTION_WIDTH) {
      let midPoint = (selection.start + selection.end) / 2;
      selection.start = midPoint - GRAPH_WHEEL_MIN_SELECTION_WIDTH / 2;
      selection.end = midPoint + GRAPH_WHEEL_MIN_SELECTION_WIDTH / 2;
    }

    this._shouldRedraw = true;
    this.emit("selecting");
    this.emit("scroll");
  },

  /**
   * Listener for the "mouseout" event on the graph's container.
   * Clear any active cursors if a drag isn't happening.
   */
  _onMouseOut: function (e) {
    if (!this._isMouseActive) {
      this._cursor.x = null;
      this._cursor.y = null;
      this._canvas.removeAttribute("input");
      this._shouldRedraw = true;
    }
  },

  /**
   * Listener for the "resize" event on the graph's parent node.
   */
  _onResize: function () {
    if (this.hasData()) {
      // The assumption is that resize events may change the outside world
      // layout in a way that affects this graph's bounding box location
      // relative to the top window's document. Graphs aren't currently
      // (or ever) expected to move around on their own.
      this._maybeDirtyBoundingBox = true;
      setNamedTimeout(this._uid, GRAPH_RESIZE_EVENTS_DRAIN, this.refresh);
    }
  }
};

// Helper functions.

/**
 * Creates an iframe element with the provided source URL, appends it to
 * the specified node and invokes the callback once the content is loaded.
 *
 * @param string url
 *        The desired source URL for the iframe.
 * @param nsIDOMNode parent
 *        The desired parent node for the iframe.
 * @param function callback
 *        Invoked once the content is loaded, with the iframe as an argument.
 */
AbstractCanvasGraph.createIframe = function (url, parent, callback) {
  let iframe = parent.ownerDocument.createElementNS(HTML_NS, "iframe");

  iframe.addEventListener("DOMContentLoaded", function onLoad() {
    iframe.removeEventListener("DOMContentLoaded", onLoad);
    callback(iframe);
  });

  // Setting 100% width on the frame and flex on the parent allows the graph
  // to properly shrink when the window is resized to be smaller.
  iframe.setAttribute("frameborder", "0");
  iframe.style.width = "100%";
  iframe.style.minWidth = "50px";
  iframe.src = url;

  parent.style.display = "flex";
  parent.appendChild(iframe);
};

/**
 * Gets a striped pattern used as a background in selections and regions.
 *
 * @param object data
 *        The following properties are required:
 *          - ownerDocument: the nsIDocumentElement owning the canvas
 *          - backgroundColor: a string representing the fill style
 *          - stripesColor: a string representing the stroke style
 * @return nsIDOMCanvasPattern
 *         The custom striped pattern.
 */
AbstractCanvasGraph.getStripePattern = function (data) {
  let { ownerDocument, backgroundColor, stripesColor } = data;
  let id = [backgroundColor, stripesColor].join(",");

  if (gCachedStripePattern.has(id)) {
    return gCachedStripePattern.get(id);
  }

  let canvas = ownerDocument.createElementNS(HTML_NS, "canvas");
  let ctx = canvas.getContext("2d");
  let width = canvas.width = GRAPH_STRIPE_PATTERN_WIDTH;
  let height = canvas.height = GRAPH_STRIPE_PATTERN_HEIGHT;

  ctx.fillStyle = backgroundColor;
  ctx.fillRect(0, 0, width, height);

  let pixelRatio = ownerDocument.defaultView.devicePixelRatio;
  let scaledLineWidth = GRAPH_STRIPE_PATTERN_LINE_WIDTH * pixelRatio;
  let scaledLineSpacing = GRAPH_STRIPE_PATTERN_LINE_SPACING * pixelRatio;

  ctx.strokeStyle = stripesColor;
  ctx.lineWidth = scaledLineWidth;
  ctx.lineCap = "square";
  ctx.beginPath();

  for (let i = -height; i <= height; i += scaledLineSpacing) {
    ctx.moveTo(width, i);
    ctx.lineTo(0, i + height);
  }

  ctx.stroke();

  let pattern = ctx.createPattern(canvas, "repeat");
  gCachedStripePattern.set(id, pattern);
  return pattern;
};

/**
 * Cache used by `AbstractCanvasGraph.getStripePattern`.
 */
const gCachedStripePattern = new Map();

/**
 * Utility functions for graph canvases.
 */
this.CanvasGraphUtils = {
  _graphUtilsWorker: null,
  _graphUtilsTaskId: 0,

  /**
   * Merges the animation loop of two graphs.
   */
  linkAnimation: Task.async(function* (graph1, graph2) {
    if (!graph1 || !graph2) {
      return;
    }
    yield graph1.ready();
    yield graph2.ready();

    let window = graph1._window;
    window.cancelAnimationFrame(graph1._animationId);
    window.cancelAnimationFrame(graph2._animationId);

    let loop = () => {
      window.requestAnimationFrame(loop);
      graph1._drawWidget();
      graph2._drawWidget();
    };

    window.requestAnimationFrame(loop);
  }),

  /**
   * Makes sure selections in one graph are reflected in another.
   */
  linkSelection: function (graph1, graph2) {
    if (!graph1 || !graph2) {
      return;
    }

    if (graph1.hasSelection()) {
      graph2.setSelection(graph1.getSelection());
    } else {
      graph2.dropSelection();
    }

    graph1.on("selecting", () => {
      graph2.setSelection(graph1.getSelection());
    });
    graph2.on("selecting", () => {
      graph1.setSelection(graph2.getSelection());
    });
    graph1.on("deselecting", () => {
      graph2.dropSelection();
    });
    graph2.on("deselecting", () => {
      graph1.dropSelection();
    });
  },

  /**
   * Performs the given task in a chrome worker, assuming it exists.
   *
   * @param string task
   *        The task name. Currently supported: "plotTimestampsGraph".
   * @param any data
   *        Extra arguments to pass to the worker.
   * @return object
   *         A promise that is resolved once the worker finishes the task.
   */
  _performTaskInWorker: function (task, data) {
    let worker = this._graphUtilsWorker || new DevToolsWorker(WORKER_URL);
    return worker.performTask(task, data);
  }
};

/**
 * Maps a value from one range to another.
 * @param number value, istart, istop, ostart, ostop
 * @return number
 */
function map(value, istart, istop, ostart, ostop) {
  let ratio = istop - istart;
  if (ratio == 0) {
    return value;
  }
  return ostart + (ostop - ostart) * ((value - istart) / ratio);
}

/**
 * Constrains a value to a range.
 * @param number value, min, max
 * @return number
 */
function clamp(value, min, max) {
  if (value < min) {
    return min;
  }
  if (value > max) {
    return max;
  }
  return value;
}

exports.GraphCursor = GraphCursor;
exports.GraphArea = GraphArea;
exports.GraphAreaDragger = GraphAreaDragger;
exports.GraphAreaResizer = GraphAreaResizer;
exports.AbstractCanvasGraph = AbstractCanvasGraph;
exports.CanvasGraphUtils = CanvasGraphUtils;
exports.CanvasGraphUtils.map = map;
exports.CanvasGraphUtils.clamp = clamp;
