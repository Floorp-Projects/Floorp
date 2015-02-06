/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Cu = Components.utils;

Cu.import("resource:///modules/devtools/ViewHelpers.jsm");
const promise = Cu.import("resource://gre/modules/Promise.jsm", {}).Promise;
const {Task} = Cu.import("resource://gre/modules/Task.jsm", {});
const {EventEmitter} = Cu.import("resource://gre/modules/devtools/event-emitter.js", {});

this.EXPORTED_SYMBOLS = [
  "GraphCursor",
  "GraphSelection",
  "GraphSelectionDragger",
  "GraphSelectionResizer",
  "AbstractCanvasGraph",
  "LineGraphWidget",
  "BarGraphWidget",
  "CanvasGraphUtils"
];

const HTML_NS = "http://www.w3.org/1999/xhtml";
const GRAPH_SRC = "chrome://browser/content/devtools/graphs-frame.xhtml";
const WORKER_URL = "resource:///modules/devtools/GraphsWorker.js";
const L10N = new ViewHelpers.L10N();

// Generic constants.

const GRAPH_RESIZE_EVENTS_DRAIN = 100; // ms
const GRAPH_WHEEL_ZOOM_SENSITIVITY = 0.00075;
const GRAPH_WHEEL_SCROLL_SENSITIVITY = 0.1;
const GRAPH_WHEEL_MIN_SELECTION_WIDTH = 10; // px

const GRAPH_SELECTION_BOUNDARY_HOVER_LINE_WIDTH = 4; // px
const GRAPH_SELECTION_BOUNDARY_HOVER_THRESHOLD = 10; // px
const GRAPH_MAX_SELECTION_LEFT_PADDING = 1;
const GRAPH_MAX_SELECTION_RIGHT_PADDING = 1;

const GRAPH_REGION_LINE_WIDTH = 1; // px
const GRAPH_REGION_LINE_COLOR = "rgba(237,38,85,0.8)";

const GRAPH_STRIPE_PATTERN_WIDTH = 16; // px
const GRAPH_STRIPE_PATTERN_HEIGHT = 16; // px
const GRAPH_STRIPE_PATTERN_LINE_WIDTH = 2; // px
const GRAPH_STRIPE_PATTERN_LINE_SPACING = 4; // px

// Line graph constants.

const LINE_GRAPH_DAMPEN_VALUES = 0.85;
const LINE_GRAPH_TOOLTIP_SAFE_BOUNDS = 8; // px
const LINE_GRAPH_MIN_MAX_TOOLTIP_DISTANCE = 14; // px

const LINE_GRAPH_BACKGROUND_COLOR = "#0088cc";
const LINE_GRAPH_STROKE_WIDTH = 1; // px
const LINE_GRAPH_STROKE_COLOR = "rgba(255,255,255,0.9)";
const LINE_GRAPH_HELPER_LINES_DASH = [5]; // px
const LINE_GRAPH_HELPER_LINES_WIDTH = 1; // px
const LINE_GRAPH_MAXIMUM_LINE_COLOR = "rgba(255,255,255,0.4)";
const LINE_GRAPH_AVERAGE_LINE_COLOR = "rgba(255,255,255,0.7)";
const LINE_GRAPH_MINIMUM_LINE_COLOR = "rgba(255,255,255,0.9)";
const LINE_GRAPH_BACKGROUND_GRADIENT_START = "rgba(255,255,255,0.25)";
const LINE_GRAPH_BACKGROUND_GRADIENT_END = "rgba(255,255,255,0.0)";

const LINE_GRAPH_CLIPHEAD_LINE_COLOR = "#fff";
const LINE_GRAPH_SELECTION_LINE_COLOR = "#fff";
const LINE_GRAPH_SELECTION_BACKGROUND_COLOR = "rgba(44,187,15,0.25)";
const LINE_GRAPH_SELECTION_STRIPES_COLOR = "rgba(255,255,255,0.1)";
const LINE_GRAPH_REGION_BACKGROUND_COLOR = "transparent";
const LINE_GRAPH_REGION_STRIPES_COLOR = "rgba(237,38,85,0.2)";

// Bar graph constants.

const BAR_GRAPH_DAMPEN_VALUES = 0.75;
const BAR_GRAPH_BARS_MARGIN_TOP = 1; // px
const BAR_GRAPH_BARS_MARGIN_END = 1; // px
const BAR_GRAPH_MIN_BARS_WIDTH = 5; // px
const BAR_GRAPH_MIN_BLOCKS_HEIGHT = 1; // px

const BAR_GRAPH_BACKGROUND_GRADIENT_START = "rgba(0,136,204,0.0)";
const BAR_GRAPH_BACKGROUND_GRADIENT_END = "rgba(255,255,255,0.25)";

const BAR_GRAPH_CLIPHEAD_LINE_COLOR = "#666";
const BAR_GRAPH_SELECTION_LINE_COLOR = "#555";
const BAR_GRAPH_SELECTION_BACKGROUND_COLOR = "rgba(0,136,204,0.25)";
const BAR_GRAPH_SELECTION_STRIPES_COLOR = "rgba(255,255,255,0.1)";
const BAR_GRAPH_REGION_BACKGROUND_COLOR = "transparent";
const BAR_GRAPH_REGION_STRIPES_COLOR = "rgba(237,38,85,0.2)";

const BAR_GRAPH_HIGHLIGHTS_MASK_BACKGROUND = "rgba(255,255,255,0.75)";
const BAR_GRAPH_HIGHLIGHTS_MASK_STRIPES = "rgba(255,255,255,0.5)";

const BAR_GRAPH_LEGEND_MOUSEOVER_DEBOUNCE = 50; // ms

/**
 * Small data primitives for all graphs.
 */
this.GraphCursor = function() {
  this.x = null;
  this.y = null;
};

this.GraphSelection = function() {
  this.start = null;
  this.end = null;
};

this.GraphSelectionDragger = function() {
  this.origin = null;
  this.anchor = new GraphSelection();
};

this.GraphSelectionResizer = function() {
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
this.AbstractCanvasGraph = function(parent, name, sharpness) {
  EventEmitter.decorate(this);

  this._parent = parent;
  this._ready = promise.defer();

  this._uid = "canvas-graph-" + Date.now();
  this._renderTargets = new Map();

  AbstractCanvasGraph.createIframe(GRAPH_SRC, parent, iframe => {
    this._iframe = iframe;
    this._window = iframe.contentWindow;
    this._document = iframe.contentDocument;
    this._pixelRatio = sharpness || this._window.devicePixelRatio;

    let container = this._container = this._document.getElementById("graph-container");
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
    this._ctx.mozImageSmoothingEnabled = false;

    this._cursor = new GraphCursor();
    this._selection = new GraphSelection();
    this._selectionDragger = new GraphSelectionDragger();
    this._selectionResizer = new GraphSelectionResizer();

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
    this._window.addEventListener("mouseup", this._onMouseUp);
    this._window.addEventListener("MozMousePixelScroll", this._onMouseWheel);
    this._window.addEventListener("mouseout", this._onMouseOut);

    let ownerWindow = this._parent.ownerDocument.defaultView;
    ownerWindow.addEventListener("resize", this._onResize);

    this._animationId = this._window.requestAnimationFrame(this._onAnimationFrame);

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
   * Returns a promise resolved once this graph is ready to receive data.
   */
  ready: function() {
    return this._ready.promise;
  },

  /**
   * Destroys this graph.
   */
  destroy: function() {
    this._window.removeEventListener("mousemove", this._onMouseMove);
    this._window.removeEventListener("mousedown", this._onMouseDown);
    this._window.removeEventListener("mouseup", this._onMouseUp);
    this._window.removeEventListener("MozMousePixelScroll", this._onMouseWheel);
    this._window.removeEventListener("mouseout", this._onMouseOut);

    let ownerWindow = this._parent.ownerDocument.defaultView;
    ownerWindow.removeEventListener("resize", this._onResize);

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
  },

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
  buildBackgroundImage: function() {
    return null;
  },

  /**
   * Builds and caches a graph image, based on the data source supplied
   * in `setData`. The graph image is not rebuilt on each frame, but
   * only when the data source changes.
   */
  buildGraphImage: function() {
    throw "This method needs to be implemented by inheriting classes.";
  },

  /**
   * Optionally builds and caches a mask image for this graph, composited
   * over the data image created via `buildGraphImage`. Inheriting classes
   * may override this method.
   */
  buildMaskImage: function() {
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
  setData: function(data) {
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
  setDataWhenReady: Task.async(function*(data) {
    yield this.ready();
    this.setData(data);
  }),

  /**
   * Adds a mask to this graph.
   *
   * @param any mask, options
   *        See `buildMaskImage` in inheriting classes for the required args.
   */
  setMask: function(mask, ...options) {
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
  setRegions: function(regions) {
    if (!this._cachedGraphImage) {
      throw "Can't highlight regions on a graph with no data displayed.";
    }
    if (this._regions) {
      throw "Regions were already highlighted on the graph.";
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
  hasData: function() {
    return !!this._data;
  },

  /**
   * Gets whether or not this graph has any mask applied.
   * @return boolean
   */
  hasMask: function() {
    return !!this._mask;
  },

  /**
   * Gets whether or not this graph has any regions.
   * @return boolean
   */
  hasRegions: function() {
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
  setSelection: function(selection) {
    if (!selection || selection.start == null || selection.end == null) {
      throw "Invalid selection coordinates";
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
  getSelection: function() {
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
  setMappedSelection: function(selection, mapping = {}) {
    if (!this.hasData()) {
      throw "A data source is necessary for retrieving a mapped selection.";
    }
    if (!selection || selection.start == null || selection.end == null) {
      throw "Invalid selection coordinates";
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
  getMappedSelection: function(mapping = {}) {
    if (!this.hasData()) {
      throw "A data source is necessary for retrieving a mapped selection.";
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
  dropSelection: function() {
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
  hasSelection: function() {
    return this._selection &&
      this._selection.start != null && this._selection.end != null;
  },

  /**
   * Gets whether or not a selection is currently being made, for example
   * via a click+drag operation.
   * @return boolean
   */
  hasSelectionInProgress: function() {
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
  setCursor: function(cursor) {
    if (!cursor || cursor.x == null || cursor.y == null) {
      throw "Invalid cursor coordinates";
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
  getCursor: function() {
    return { x: this._cursor.x, y: this._cursor.y };
  },

  /**
   * Hides the cursor.
   */
  dropCursor: function() {
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
  hasCursor: function() {
    return this._cursor && this._cursor.x != null;
  },

  /**
   * Specifies if this graph's selection is different from another one.
   *
   * @param object other
   *        The other graph's selection, as { start, end } values.
   */
  isSelectionDifferent: function(other) {
    if (!other) return true;
    let current = this.getSelection();
    return current.start != other.start || current.end != other.end;
  },

  /**
   * Specifies if this graph's cursor is different from another one.
   *
   * @param object other
   *        The other graph's position, as { x, y } values.
   */
  isCursorDifferent: function(other) {
    if (!other) return true;
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
  getSelectionWidth: function() {
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
  getHoveredRegion: function() {
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
  _getNamedCanvas: function(name, width = this._width, height = this._height) {
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
      ctx.drawImage(this._cachedBackgroundImage, 0, 0, this._width, this._height);
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
  _drawCliphead: function() {
    if (this._isHoveringSelectionContentsOrBoundaries() || this._isHoveringRegion()) {
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
  _drawSelection: function() {
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
    ctx.fillRect(start, 0, end - start, this._height);

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
  _bakeRegions: function(regions, destination) {
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
  _isHoveringStartBoundary: function() {
    if (!this.hasSelection() || !this.hasCursor()) {
      return;
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
  _isHoveringEndBoundary: function() {
    if (!this.hasSelection() || !this.hasCursor()) {
      return;
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
  _isHoveringSelectionContents: function() {
    if (!this.hasSelection() || !this.hasCursor()) {
      return;
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
  _isHoveringSelectionContentsOrBoundaries: function() {
    return this._isHoveringSelectionContents() ||
           this._isHoveringStartBoundary() ||
           this._isHoveringEndBoundary();
  },

  /**
   * Checks whether a region is hovered.
   * @return boolean
   */
  _isHoveringRegion: function() {
    return !!this.getHoveredRegion();
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

    while (node = node.offsetParent) {
      x += node.offsetLeft;
      y += node.offsetTop;
    }

    return { left: x, top: y };
  },

  /**
   * Listener for the "mousemove" event on the graph's container.
   */
  _onMouseMove: function(e) {
    let offset = this._getContainerOffset();
    let mouseX = (e.clientX - offset.left) * this._pixelRatio;
    let mouseY = (e.clientY - offset.top) * this._pixelRatio;
    this._cursor.x = mouseX;
    this._cursor.y = mouseY;

    let resizer = this._selectionResizer;
    if (resizer.margin != null) {
      this._selection[resizer.margin] = mouseX;
      this._shouldRedraw = true;
      this.emit("selecting");
      return;
    }

    let dragger = this._selectionDragger;
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
  _onMouseDown: function(e) {
    let offset = this._getContainerOffset();
    let mouseX = (e.clientX - offset.left) * this._pixelRatio;

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

    this._shouldRedraw = true;
    this.emit("mousedown");
  },

  /**
   * Listener for the "mouseup" event on the graph's container.
   */
  _onMouseUp: function(e) {
    let offset = this._getContainerOffset();
    let mouseX = (e.clientX - offset.left) * this._pixelRatio;

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
          this._selection.end = mouseX;
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

    this._shouldRedraw = true;
    this.emit("mouseup");
  },

  /**
   * Listener for the "wheel" event on the graph's container.
   */
  _onMouseWheel: function(e) {
    if (!this.hasSelection()) {
      return;
    }

    let offset = this._getContainerOffset();
    let mouseX = (e.clientX - offset.left) * this._pixelRatio;
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
    }
    // Otherwise, simply pan the selection towards the left or right.
    else {
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
   */
  _onMouseOut: function() {
    if (this.hasSelectionInProgress()) {
      this.dropSelection();
    }

    this._cursor.x = null;
    this._cursor.y = null;
    this._selectionResizer.margin = null;
    this._selectionDragger.origin = null;

    this._canvas.removeAttribute("input");
    this._shouldRedraw = true;
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

/**
 * A basic line graph, plotting values on a curve and adding helper lines
 * and tooltips for maximum, average and minimum values.
 *
 * @see AbstractCanvasGraph for emitted events and other options.
 *
 * Example usage:
 *   let graph = new LineGraphWidget(node, "units");
 *   graph.once("ready", () => {
 *     graph.setData(src);
 *   });
 *
 * Data source format:
 *   [
 *     { delta: x1, value: y1 },
 *     { delta: x2, value: y2 },
 *     ...
 *     { delta: xn, value: yn }
 *   ]
 * where each item in the array represents a point in the graph.
 *
 * @param nsIDOMNode parent
 *        The parent node holding the graph.
 * @param object options [optional]
 *        `metric`: The metric displayed in the graph, e.g. "fps" or "bananas".
 *        `min`: Boolean whether to show the min tooltip/gutter/line (default: true)
 *        `max`: Boolean whether to show the max tooltip/gutter/line (default: true)
 *        `avg`: Boolean whether to show the avg tooltip/gutter/line (default: true)
 */
this.LineGraphWidget = function(parent, options, ...args) {
  options = options || {};
  let metric = options.metric;

  this._showMin = options.min !== false;
  this._showMax = options.max !== false;
  this._showAvg = options.avg !== false;
  AbstractCanvasGraph.apply(this, [parent, "line-graph", ...args]);

  this.once("ready", () => {
    // Create all gutters and tooltips incase the showing of min/max/avg
    // are changed later
    this._gutter = this._createGutter();

    this._maxGutterLine = this._createGutterLine("maximum");
    this._maxTooltip = this._createTooltip("maximum", "start", "max", metric);
    this._minGutterLine = this._createGutterLine("minimum");
    this._minTooltip = this._createTooltip("minimum", "start", "min", metric);
    this._avgGutterLine = this._createGutterLine("average");
    this._avgTooltip = this._createTooltip("average", "end", "avg", metric);
  });
};

LineGraphWidget.prototype = Heritage.extend(AbstractCanvasGraph.prototype, {
  backgroundColor: LINE_GRAPH_BACKGROUND_COLOR,
  backgroundGradientStart: LINE_GRAPH_BACKGROUND_GRADIENT_START,
  backgroundGradientEnd: LINE_GRAPH_BACKGROUND_GRADIENT_END,
  strokeColor: LINE_GRAPH_STROKE_COLOR,
  strokeWidth: LINE_GRAPH_STROKE_WIDTH,
  maximumLineColor: LINE_GRAPH_MAXIMUM_LINE_COLOR,
  averageLineColor: LINE_GRAPH_AVERAGE_LINE_COLOR,
  minimumLineColor: LINE_GRAPH_MINIMUM_LINE_COLOR,
  clipheadLineColor: LINE_GRAPH_CLIPHEAD_LINE_COLOR,
  selectionLineColor: LINE_GRAPH_SELECTION_LINE_COLOR,
  selectionBackgroundColor: LINE_GRAPH_SELECTION_BACKGROUND_COLOR,
  selectionStripesColor: LINE_GRAPH_SELECTION_STRIPES_COLOR,
  regionBackgroundColor: LINE_GRAPH_REGION_BACKGROUND_COLOR,
  regionStripesColor: LINE_GRAPH_REGION_STRIPES_COLOR,

  /**
   * Optionally offsets the `delta` in the data source by this scalar.
   */
  dataOffsetX: 0,

  /**
   * Optionally uses this value instead of the last tick in the data source
   * to compute the horizontal scaling.
   */
  dataDuration: 0,

  /**
   * The scalar used to multiply the graph values to leave some headroom.
   */
  dampenValuesFactor: LINE_GRAPH_DAMPEN_VALUES,

  /**
   * Specifies if min/max/avg tooltips have arrow handlers on their sides.
   */
  withTooltipArrows: true,

  /**
   * Specifies if min/max/avg tooltips are positioned based on the actual
   * values, or just placed next to the graph corners.
   */
  withFixedTooltipPositions: false,

  /**
   * Takes a list of numbers and plots them on a line graph representing
   * the rate of occurences in a specified interval. Useful for drawing
   * framerate, for example, from a sequence of timestamps.
   *
   * @param array timestamps
   *        A list of numbers representing time, ordered ascending. For example,
   *        this can be the raw data received from the framerate actor, which
   *        represents the elapsed time on each refresh driver tick.
   * @param number interval
   *        The maximum amount of time to wait between calculations.
   */
  setDataFromTimestamps: Task.async(function*(timestamps, interval) {
    let {
      plottedData,
      plottedMinMaxSum
    } = yield CanvasGraphUtils._performTaskInWorker("plotTimestampsGraph", {
      timestamps: timestamps,
      interval: interval
    });

    this._tempMinMaxSum = plottedMinMaxSum;
    this.setData(plottedData);
  }),

  /**
   * Renders the graph's data source.
   * @see AbstractCanvasGraph.prototype.buildGraphImage
   */
  buildGraphImage: function() {
    let { canvas, ctx } = this._getNamedCanvas("line-graph-data");
    let width = this._width;
    let height = this._height;

    let totalTicks = this._data.length;
    let firstTick = totalTicks ? this._data[0].delta : 0;
    let lastTick = totalTicks ? this._data[totalTicks - 1].delta : 0;
    let maxValue = Number.MIN_SAFE_INTEGER;
    let minValue = Number.MAX_SAFE_INTEGER;
    let avgValue = 0;

    if (this._tempMinMaxSum) {
      maxValue = this._tempMinMaxSum.maxValue;
      minValue = this._tempMinMaxSum.minValue;
      avgValue = this._tempMinMaxSum.avgValue;
    } else {
      let sumValues = 0;
      for (let { delta, value } of this._data) {
        maxValue = Math.max(value, maxValue);
        minValue = Math.min(value, minValue);
        sumValues += value;
      }
      avgValue = sumValues / totalTicks;
    }

    let duration = this.dataDuration || lastTick;
    let dataScaleX = this.dataScaleX = width / (duration - this.dataOffsetX);
    let dataScaleY = this.dataScaleY = height / maxValue * this.dampenValuesFactor;

    // Draw the background.

    ctx.fillStyle = this.backgroundColor;
    ctx.fillRect(0, 0, width, height);

    // Draw the graph.

    let gradient = ctx.createLinearGradient(0, height / 2, 0, height);
    gradient.addColorStop(0, this.backgroundGradientStart);
    gradient.addColorStop(1, this.backgroundGradientEnd);
    ctx.fillStyle = gradient;
    ctx.strokeStyle = this.strokeColor;
    ctx.lineWidth = this.strokeWidth * this._pixelRatio;
    ctx.beginPath();

    for (let { delta, value } of this._data) {
      let currX = (delta - this.dataOffsetX) * dataScaleX;
      let currY = height - value * dataScaleY;

      if (delta == firstTick) {
        ctx.moveTo(-LINE_GRAPH_STROKE_WIDTH, height);
        ctx.lineTo(-LINE_GRAPH_STROKE_WIDTH, currY);
      }

      ctx.lineTo(currX, currY);

      if (delta == lastTick) {
        ctx.lineTo(width + LINE_GRAPH_STROKE_WIDTH, currY);
        ctx.lineTo(width + LINE_GRAPH_STROKE_WIDTH, height);
      }
    }

    ctx.fill();
    ctx.stroke();

    this._drawOverlays(ctx, minValue, maxValue, avgValue, dataScaleY);

    return canvas;
  },

  /**
   * Draws the min, max and average horizontal lines, along with their
   * repsective tooltips.
   *
   * @param CanvasRenderingContext2D ctx
   * @param number minValue
   * @param number maxValue
   * @param number avgValue
   * @param number dataScaleY
   */
  _drawOverlays: function(ctx, minValue, maxValue, avgValue, dataScaleY) {
    let width = this._width;
    let height = this._height;
    let totalTicks = this._data.length;

    // Draw the maximum value horizontal line.
    if (this._showMax) {
      ctx.strokeStyle = this.maximumLineColor;
      ctx.lineWidth = LINE_GRAPH_HELPER_LINES_WIDTH;
      ctx.setLineDash(LINE_GRAPH_HELPER_LINES_DASH);
      ctx.beginPath();
      let maximumY = height - maxValue * dataScaleY;
      ctx.moveTo(0, maximumY);
      ctx.lineTo(width, maximumY);
      ctx.stroke();
    }

    // Draw the average value horizontal line.
    if (this._showAvg) {
      ctx.strokeStyle = this.averageLineColor;
      ctx.lineWidth = LINE_GRAPH_HELPER_LINES_WIDTH;
      ctx.setLineDash(LINE_GRAPH_HELPER_LINES_DASH);
      ctx.beginPath();
      let averageY = height - avgValue * dataScaleY;
      ctx.moveTo(0, averageY);
      ctx.lineTo(width, averageY);
      ctx.stroke();
    }

    // Draw the minimum value horizontal line.
    if (this._showMin) {
      ctx.strokeStyle = this.minimumLineColor;
      ctx.lineWidth = LINE_GRAPH_HELPER_LINES_WIDTH;
      ctx.setLineDash(LINE_GRAPH_HELPER_LINES_DASH);
      ctx.beginPath();
      let minimumY = height - minValue * dataScaleY;
      ctx.moveTo(0, minimumY);
      ctx.lineTo(width, minimumY);
      ctx.stroke();
    }

    // Update the tooltips text and gutter lines.

    this._maxTooltip.querySelector("[text=value]").textContent =
      L10N.numberWithDecimals(maxValue, 2);
    this._avgTooltip.querySelector("[text=value]").textContent =
      L10N.numberWithDecimals(avgValue, 2);
    this._minTooltip.querySelector("[text=value]").textContent =
      L10N.numberWithDecimals(minValue, 2);

    let bottom = height / this._pixelRatio;
    let maxPosY = map(maxValue * this.dampenValuesFactor, 0, maxValue, bottom, 0);
    let avgPosY = map(avgValue * this.dampenValuesFactor, 0, maxValue, bottom, 0);
    let minPosY = map(minValue * this.dampenValuesFactor, 0, maxValue, bottom, 0);

    let safeTop = LINE_GRAPH_TOOLTIP_SAFE_BOUNDS;
    let safeBottom = bottom - LINE_GRAPH_TOOLTIP_SAFE_BOUNDS;

    let maxTooltipTop = (this.withFixedTooltipPositions
      ? safeTop : clamp(maxPosY, safeTop, safeBottom));
    let avgTooltipTop = (this.withFixedTooltipPositions
      ? safeTop : clamp(avgPosY, safeTop, safeBottom));
    let minTooltipTop = (this.withFixedTooltipPositions
      ? safeBottom : clamp(minPosY, safeTop, safeBottom));

    this._maxTooltip.style.top = maxTooltipTop + "px";
    this._avgTooltip.style.top = avgTooltipTop + "px";
    this._minTooltip.style.top = minTooltipTop + "px";

    this._maxGutterLine.style.top = maxPosY + "px";
    this._avgGutterLine.style.top = avgPosY + "px";
    this._minGutterLine.style.top = minPosY + "px";

    this._maxTooltip.setAttribute("with-arrows", this.withTooltipArrows);
    this._avgTooltip.setAttribute("with-arrows", this.withTooltipArrows);
    this._minTooltip.setAttribute("with-arrows", this.withTooltipArrows);

    let distanceMinMax = Math.abs(maxTooltipTop - minTooltipTop);
    this._maxTooltip.hidden = this._showMax === false || !totalTicks || distanceMinMax < LINE_GRAPH_MIN_MAX_TOOLTIP_DISTANCE;
    this._avgTooltip.hidden = this._showAvg === false || !totalTicks;
    this._minTooltip.hidden = this._showMin === false || !totalTicks;
    this._gutter.hidden = (this._showMin === false && this._showAvg === false && this._showMax === false) || !totalTicks;

    this._maxGutterLine.hidden = this._showMax === false;
    this._avgGutterLine.hidden = this._showAvg === false;
    this._minGutterLine.hidden = this._showMin === false;
  },

  /**
   * Creates the gutter node when constructing this graph.
   * @return nsIDOMNode
   */
  _createGutter: function() {
    let gutter = this._document.createElementNS(HTML_NS, "div");
    gutter.className = "line-graph-widget-gutter";
    gutter.setAttribute("hidden", true);
    this._container.appendChild(gutter);

    return gutter;
  },

  /**
   * Creates the gutter line nodes when constructing this graph.
   * @return nsIDOMNode
   */
  _createGutterLine: function(type) {
    let line = this._document.createElementNS(HTML_NS, "div");
    line.className = "line-graph-widget-gutter-line";
    line.setAttribute("type", type);
    this._gutter.appendChild(line);

    return line;
  },

  /**
   * Creates the tooltip nodes when constructing this graph.
   * @return nsIDOMNode
   */
  _createTooltip: function(type, arrow, info, metric) {
    let tooltip = this._document.createElementNS(HTML_NS, "div");
    tooltip.className = "line-graph-widget-tooltip";
    tooltip.setAttribute("type", type);
    tooltip.setAttribute("arrow", arrow);
    tooltip.setAttribute("hidden", true);

    let infoNode = this._document.createElementNS(HTML_NS, "span");
    infoNode.textContent = info;
    infoNode.setAttribute("text", "info");

    let valueNode = this._document.createElementNS(HTML_NS, "span");
    valueNode.textContent = 0;
    valueNode.setAttribute("text", "value");

    let metricNode = this._document.createElementNS(HTML_NS, "span");
    metricNode.textContent = metric;
    metricNode.setAttribute("text", "metric");

    tooltip.appendChild(infoNode);
    tooltip.appendChild(valueNode);
    tooltip.appendChild(metricNode);
    this._container.appendChild(tooltip);

    return tooltip;
  }
});

/**
 * A bar graph, plotting tuples of values as rectangles.
 *
 * @see AbstractCanvasGraph for emitted events and other options.
 *
 * Example usage:
 *   let graph = new BarGraphWidget(node);
 *   graph.format = ...;
 *   graph.once("ready", () => {
 *     graph.setData(src);
 *   });
 *
 * The `graph.format` traits are mandatory and will determine how the values
 * are styled as "blocks" in every "bar":
 *   [
 *     { color: "#f00", label: "Foo" },
 *     { color: "#0f0", label: "Bar" },
 *     ...
 *     { color: "#00f", label: "Baz" }
 *   ]
 *
 * Data source format:
 *   [
 *     { delta: x1, values: [y11, y12, ... y1n] },
 *     { delta: x2, values: [y21, y22, ... y2n] },
 *     ...
 *     { delta: xm, values: [ym1, ym2, ... ymn] }
 *   ]
 * where each item in the array represents a "bar", for which every value
 * represents a "block" inside that "bar", plotted at the "delta" position.
 *
 * @param nsIDOMNode parent
 *        The parent node holding the graph.
 */
this.BarGraphWidget = function(parent, ...args) {
  AbstractCanvasGraph.apply(this, [parent, "bar-graph", ...args]);

  // Populated with [node, event, listener] entries which need to be removed
  // when this graph is being destroyed.
  this.outstandingEventListeners = [];

  this.once("ready", () => {
    this._onLegendMouseOver = this._onLegendMouseOver.bind(this);
    this._onLegendMouseOut = this._onLegendMouseOut.bind(this);
    this._onLegendMouseDown = this._onLegendMouseDown.bind(this);
    this._onLegendMouseUp = this._onLegendMouseUp.bind(this);
    this._createLegend();
  });

  this.once("destroyed", () => {
    for (let [node, event, listener] of this.outstandingEventListeners) {
      node.removeEventListener(event, listener);
    }
    this.outstandingEventListeners = null;
  });
};

BarGraphWidget.prototype = Heritage.extend(AbstractCanvasGraph.prototype, {
  clipheadLineColor: BAR_GRAPH_CLIPHEAD_LINE_COLOR,
  selectionLineColor: BAR_GRAPH_SELECTION_LINE_COLOR,
  selectionBackgroundColor: BAR_GRAPH_SELECTION_BACKGROUND_COLOR,
  selectionStripesColor: BAR_GRAPH_SELECTION_STRIPES_COLOR,
  regionBackgroundColor: BAR_GRAPH_REGION_BACKGROUND_COLOR,
  regionStripesColor: BAR_GRAPH_REGION_STRIPES_COLOR,

  /**
   * List of colors used to fill each block inside every bar, also
   * corresponding to labels displayed in this graph's legend.
   */
  format: null,

  /**
   * Optionally offsets the `delta` in the data source by this scalar.
   */
  dataOffsetX: 0,

  /**
   * The scalar used to multiply the graph values to leave some headroom
   * on the top.
   */
  dampenValuesFactor: BAR_GRAPH_DAMPEN_VALUES,

  /**
   * Bars that are too close too each other in the graph will be combined.
   * This scalar specifies the required minimum width of each bar.
   */
  minBarsWidth: BAR_GRAPH_MIN_BARS_WIDTH,

  /**
   * Blocks in a bar that are too thin inside the bar will not be rendered.
   * This scalar specifies the required minimum height of each block.
   */
  minBlocksHeight: BAR_GRAPH_MIN_BLOCKS_HEIGHT,

  /**
   * Renders the graph's background.
   * @see AbstractCanvasGraph.prototype.buildBackgroundImage
   */
  buildBackgroundImage: function() {
    let { canvas, ctx } = this._getNamedCanvas("bar-graph-background");
    let width = this._width;
    let height = this._height;

    let gradient = ctx.createLinearGradient(0, 0, 0, height);
    gradient.addColorStop(0, BAR_GRAPH_BACKGROUND_GRADIENT_START);
    gradient.addColorStop(1, BAR_GRAPH_BACKGROUND_GRADIENT_END);
    ctx.fillStyle = gradient;
    ctx.fillRect(0, 0, width, height);

    return canvas;
  },

  /**
   * Renders the graph's data source.
   * @see AbstractCanvasGraph.prototype.buildGraphImage
   */
  buildGraphImage: function() {
    if (!this.format || !this.format.length) {
      throw "The graph format traits are mandatory to style the data source.";
    }
    let { canvas, ctx } = this._getNamedCanvas("bar-graph-data");
    let width = this._width;
    let height = this._height;

    let totalTypes = this.format.length;
    let totalTicks = this._data.length;
    let lastTick = this._data[totalTicks - 1].delta;

    let minBarsWidth = this.minBarsWidth * this._pixelRatio;
    let minBlocksHeight = this.minBlocksHeight * this._pixelRatio;

    let dataScaleX = this.dataScaleX = width / (lastTick - this.dataOffsetX);
    let dataScaleY = this.dataScaleY = height / this._calcMaxHeight({
      data: this._data,
      dataScaleX: dataScaleX,
      minBarsWidth: minBarsWidth
    }) * this.dampenValuesFactor;

    // Draw the graph.

    // Iterate over the blocks, then the bars, to draw all rectangles of
    // the same color in a single pass. See the @constructor for more
    // information about the data source, and how a "bar" contains "blocks".

    this._blocksBoundingRects = [];
    let prevHeight = [];
    let scaledMarginEnd = BAR_GRAPH_BARS_MARGIN_END * this._pixelRatio;
    let unscaledMarginTop = BAR_GRAPH_BARS_MARGIN_TOP;

    for (let type = 0; type < totalTypes; type++) {
      ctx.fillStyle = this.format[type].color || "#000";
      ctx.beginPath();

      let prevRight = 0;
      let skippedCount = 0;
      let skippedHeight = 0;

      for (let tick = 0; tick < totalTicks; tick++) {
        let delta = this._data[tick].delta;
        let value = this._data[tick].values[type] || 0;
        let blockRight = (delta - this.dataOffsetX) * dataScaleX;
        let blockHeight = value * dataScaleY;

        let blockWidth = blockRight - prevRight;
        if (blockWidth < minBarsWidth) {
          skippedCount++;
          skippedHeight += blockHeight;
          continue;
        }

        let averageHeight = (blockHeight + skippedHeight) / (skippedCount + 1);
        if (averageHeight >= minBlocksHeight) {
          let bottom = height - ~~prevHeight[tick];
          ctx.moveTo(prevRight, bottom);
          ctx.lineTo(prevRight, bottom - averageHeight);
          ctx.lineTo(blockRight, bottom - averageHeight);
          ctx.lineTo(blockRight, bottom);

          // Remember this block's type and location.
          this._blocksBoundingRects.push({
            type: type,
            start: prevRight,
            end: blockRight,
            top: bottom - averageHeight,
            bottom: bottom
          });

          if (prevHeight[tick] === undefined) {
            prevHeight[tick] = averageHeight + unscaledMarginTop;
          } else {
            prevHeight[tick] += averageHeight + unscaledMarginTop;
          }
        }

        prevRight += blockWidth + scaledMarginEnd;
        skippedHeight = 0;
        skippedCount = 0;
      }

      ctx.fill();
    }

    // The blocks bounding rects isn't guaranteed to be sorted ascending by
    // block location on the X axis. This should be the case, for better
    // cache cohesion and a faster `buildMaskImage`.
    this._blocksBoundingRects.sort((a, b) => a.start > b.start ? 1 : -1);

    // Update the legend.

    while (this._legendNode.hasChildNodes()) {
      this._legendNode.firstChild.remove();
    }
    for (let { color, label } of this.format) {
      this._createLegendItem(color, label);
    }

    return canvas;
  },

  /**
   * Renders the graph's mask.
   * Fades in only the parts of the graph that are inside the specified areas.
   *
   * @param array highlights
   *        A list of { start, end } values. Optionally, each object
   *        in the list may also specify { top, bottom } pixel values if the
   *        highlighting shouldn't span across the full height of the graph.
   * @param boolean inPixels
   *        Set this to true if the { start, end } values in the highlights
   *        list are pixel values, and not values from the data source.
   * @param function unpack [optional]
   *        @see AbstractCanvasGraph.prototype.getMappedSelection
   */
  buildMaskImage: function(highlights, inPixels = false, unpack = e => e.delta) {
    // A null `highlights` array is used to clear the mask. An empty array
    // will mask the entire graph.
    if (!highlights) {
      return null;
    }

    // Get a render target for the highlights. It will be overlaid on top of
    // the existing graph, masking the areas that aren't highlighted.

    let { canvas, ctx } = this._getNamedCanvas("graph-highlights");
    let width = this._width;
    let height = this._height;

    // Draw the background mask.

    let pattern = AbstractCanvasGraph.getStripePattern({
      ownerDocument: this._document,
      backgroundColor: BAR_GRAPH_HIGHLIGHTS_MASK_BACKGROUND,
      stripesColor: BAR_GRAPH_HIGHLIGHTS_MASK_STRIPES
    });
    ctx.fillStyle = pattern;
    ctx.fillRect(0, 0, width, height);

    // Clear highlighted areas.

    let totalTicks = this._data.length;
    let firstTick = unpack(this._data[0]);
    let lastTick = unpack(this._data[totalTicks - 1]);

    for (let { start, end, top, bottom } of highlights) {
      if (!inPixels) {
        start = map(start, firstTick, lastTick, 0, width);
        end = map(end, firstTick, lastTick, 0, width);
      }
      let firstSnap = findFirst(this._blocksBoundingRects, e => e.start >= start);
      let lastSnap = findLast(this._blocksBoundingRects, e => e.start >= start && e.end <= end);

      let x1 = firstSnap ? firstSnap.start : start;
      let x2 = lastSnap ? lastSnap.end : firstSnap ? firstSnap.end : end;
      let y1 = top || 0;
      let y2 = bottom || height;
      ctx.clearRect(x1, y1, x2 - x1, y2 - y1);
    }

    return canvas;
  },

  /**
   * A list storing the bounding rectangle for each drawn block in the graph.
   * Created whenever `buildGraphImage` is invoked.
   */
  _blocksBoundingRects: null,

  /**
   * Calculates the height of the tallest bar that would eventially be rendered
   * in this graph.
   *
   * Bars that are too close too each other in the graph will be combined.
   * @see `minBarsWidth`
   *
   * @return number
   *         The tallest bar height in this graph.
   */
  _calcMaxHeight: function({ data, dataScaleX, minBarsWidth }) {
    let maxHeight = 0;
    let prevRight = 0;
    let skippedCount = 0;
    let skippedHeight = 0;
    let scaledMarginEnd = BAR_GRAPH_BARS_MARGIN_END * this._pixelRatio;

    for (let { delta, values } of data) {
      let barRight = (delta - this.dataOffsetX) * dataScaleX;
      let barHeight = values.reduce((a, b) => a + b, 0);

      let barWidth = barRight - prevRight;
      if (barWidth < minBarsWidth) {
        skippedCount++;
        skippedHeight += barHeight;
        continue;
      }

      let averageHeight = (barHeight + skippedHeight) / (skippedCount + 1);
      maxHeight = Math.max(averageHeight, maxHeight);

      prevRight += barWidth + scaledMarginEnd;
      skippedHeight = 0;
      skippedCount = 0;
    }

    return maxHeight;
  },

  /**
   * Creates the legend container when constructing this graph.
   */
  _createLegend: function() {
    let legendNode = this._legendNode = this._document.createElementNS(HTML_NS, "div");
    legendNode.className = "bar-graph-widget-legend";
    this._container.appendChild(legendNode);
  },

  /**
   * Creates a legend item when constructing this graph.
   */
  _createLegendItem: function(color, label) {
    let itemNode = this._document.createElementNS(HTML_NS, "div");
    itemNode.className = "bar-graph-widget-legend-item";

    let colorNode = this._document.createElementNS(HTML_NS, "span");
    colorNode.setAttribute("view", "color");
    colorNode.setAttribute("data-index", this._legendNode.childNodes.length);
    colorNode.style.backgroundColor = color;
    colorNode.addEventListener("mouseover", this._onLegendMouseOver);
    colorNode.addEventListener("mouseout", this._onLegendMouseOut);
    colorNode.addEventListener("mousedown", this._onLegendMouseDown);
    colorNode.addEventListener("mouseup", this._onLegendMouseUp);

    this.outstandingEventListeners.push([colorNode, "mouseover", this._onLegendMouseOver]);
    this.outstandingEventListeners.push([colorNode, "mouseout", this._onLegendMouseOut]);
    this.outstandingEventListeners.push([colorNode, "mousedown", this._onLegendMouseDown]);
    this.outstandingEventListeners.push([colorNode, "mouseup", this._onLegendMouseUp]);

    let labelNode = this._document.createElementNS(HTML_NS, "span");
    labelNode.setAttribute("view", "label");
    labelNode.textContent = label;

    itemNode.appendChild(colorNode);
    itemNode.appendChild(labelNode);
    this._legendNode.appendChild(itemNode);
  },

  /**
   * Invoked whenever a color node in the legend is hovered.
   */
  _onLegendMouseOver: function(e) {
    setNamedTimeout("bar-graph-debounce", BAR_GRAPH_LEGEND_MOUSEOVER_DEBOUNCE, () => {
      let type = e.target.dataset.index;
      let rects = this._blocksBoundingRects.filter(e => e.type == type);

      this._originalHighlights = this._mask;
      this._hasCustomHighlights = true;
      this.setMask(rects, true);

      this.emit("legend-hover", [type, rects]);
    });
  },

  /**
   * Invoked whenever a color node in the legend is unhovered.
   */
  _onLegendMouseOut: function() {
    clearNamedTimeout("bar-graph-debounce");

    if (this._hasCustomHighlights) {
      this.setMask(this._originalHighlights);
      this._hasCustomHighlights = false;
      this._originalHighlights = null;
    }

    this.emit("legend-unhover");
  },

  /**
   * Invoked whenever a color node in the legend is pressed.
   */
  _onLegendMouseDown: function(e) {
    e.preventDefault();
    e.stopPropagation();

    let type = e.target.dataset.index;
    let rects = this._blocksBoundingRects.filter(e => e.type == type);
    let leftmost = rects[0];
    let rightmost = rects[rects.length - 1];
    if (!leftmost || !rightmost) {
      this.dropSelection();
    } else {
      this.setSelection({ start: leftmost.start, end: rightmost.end });
    }

    this.emit("legend-selection", [leftmost, rightmost]);
  },

  /**
   * Invoked whenever a color node in the legend is released.
   */
  _onLegendMouseUp: function(e) {
    e.preventDefault();
    e.stopPropagation();
  }
});

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
AbstractCanvasGraph.createIframe = function(url, parent, callback) {
  let iframe = parent.ownerDocument.createElementNS(HTML_NS, "iframe");

  iframe.addEventListener("DOMContentLoaded", function onLoad() {
    iframe.removeEventListener("DOMContentLoaded", onLoad);
    callback(iframe);
  });

  iframe.setAttribute("frameborder", "0");
  iframe.src = url;

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
AbstractCanvasGraph.getStripePattern = function(data) {
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
  linkAnimation: Task.async(function*(graph1, graph2) {
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
  linkSelection: function(graph1, graph2) {
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
   * @param any args
   *        Extra arguments to pass to the worker.
   * @param array transferrable [optional]
   *        A list of transferrable objects, if any.
   * @return object
   *         A promise that is resolved once the worker finishes the task.
   */
  _performTaskInWorker: function(task, args, transferrable) {
    let worker = this._graphUtilsWorker || new ChromeWorker(WORKER_URL);
    let id = this._graphUtilsTaskId++;
    worker.postMessage({ task, id, args }, transferrable);
    return this._waitForWorkerResponse(worker, id);
  },

  /**
   * Waits for the specified worker to finish a task.
   *
   * @param ChromeWorker worker
   *        The worker for which to add a message listener.
   * @param number id
   *        The worker task id.
   */
  _waitForWorkerResponse: function(worker, id) {
    let deferred = promise.defer();

    worker.addEventListener("message", function listener({ data }) {
      if (data.id != id) {
        return;
      }
      worker.removeEventListener("message", listener);
      deferred.resolve(data);
    });

    return deferred.promise;
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
  if (value < min) return min;
  if (value > max) return max;
  return value;
}

/**
 * Calculates the squared distance between two 2D points.
 */
function distSquared(x0, y0, x1, y1) {
  let xs = x1 - x0;
  let ys = y1 - y0;
  return xs * xs + ys * ys;
}

/**
 * Finds the first element in an array that validates a predicate.
 * @param array
 * @param function predicate
 * @return number
 */
function findFirst(array, predicate) {
  for (let i = 0, len = array.length; i < len; i++) {
    let element = array[i];
    if (predicate(element)) return element;
  }
}

/**
 * Finds the last element in an array that validates a predicate.
 * @param array
 * @param function predicate
 * @return number
 */
function findLast(array, predicate) {
  for (let i = array.length - 1; i >= 0; i--) {
    let element = array[i];
    if (predicate(element)) return element;
  }
}
