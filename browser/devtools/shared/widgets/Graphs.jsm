/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Cu = Components.utils;

Cu.import("resource://gre/modules/devtools/event-emitter.js");
Cu.import("resource:///modules/devtools/ViewHelpers.jsm");

this.EXPORTED_SYMBOLS = ["LineGraphWidget"];

const HTML_NS = "http://www.w3.org/1999/xhtml";
const GRAPH_SRC = "chrome://browser/content/devtools/graphs-frame.xhtml";

// Generic constants.

const GRAPH_DAMPEN_VALUES = 0.85;
const GRAPH_RESIZE_EVENTS_DRAIN = 20; // ms
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
const GRAPH_STRIPE_PATTERN_LINE_WIDTH = 4; // px
const GRAPH_STRIPE_PATTERN_LINE_SPACING = 8; // px

// Line graph constants.

const LINE_GRAPH_MIN_SQUARED_DISTANCE_BETWEEN_POINTS = 400; // 20 px
const LINE_GRAPH_STROKE_WIDTH = 2; // px
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

const LINE_GRAPH_TOOLTIP_SAFE_BOUNDS = 10; // px

/**
 * Small data primitives for all graphs.
 */
this.GraphCursor = function() {}
this.GraphSelection = function() {}
this.GraphSelectionDragger = function() {}
this.GraphSelectionResizer = function() {}

GraphCursor.prototype = {
  x: null,
  y: null
};

GraphSelection.prototype = {
  start: null,
  end: null
};

GraphSelectionDragger.prototype = {
  origin: null,
  anchor: new GraphSelection()
};

GraphSelectionResizer.prototype = {
  margin: null
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
  this._uid = "canvas-graph-" + Date.now();

  AbstractCanvasGraph.createIframe(GRAPH_SRC, parent, iframe => {
    this._iframe = iframe;
    this._window = iframe.contentWindow;
    this._document = iframe.contentDocument;
    this._pixelRatio = sharpness || this._window.devicePixelRatio;

    let container = this._container = this._document.getElementById("graph-container");
    container.className = name + "-widget-container";

    let canvas = this._canvas = this._document.getElementById("graph-canvas");
    canvas.className = name + "-widget-canvas graph-widget-canvas";

    let bounds = parent.getBoundingClientRect();
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

    container.addEventListener("mousemove", this._onMouseMove);
    container.addEventListener("mousedown", this._onMouseDown);
    container.addEventListener("mouseup", this._onMouseUp);
    container.addEventListener("MozMousePixelScroll", this._onMouseWheel);
    container.addEventListener("mouseout", this._onMouseOut);

    let ownerWindow = this._parent.ownerDocument.defaultView;
    ownerWindow.addEventListener("resize", this._onResize);

    this._animationId = this._window.requestAnimationFrame(this._onAnimationFrame);

    this.emit("ready", this);
  });
}

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
   * Destroys this graph.
   */
  destroy: function() {
    let container = this._container;
    container.removeEventListener("mousemove", this._onMouseMove);
    container.removeEventListener("mousedown", this._onMouseDown);
    container.removeEventListener("mouseup", this._onMouseUp);
    container.removeEventListener("MozMousePixelScroll", this._onMouseWheel);
    container.removeEventListener("mouseout", this._onMouseOut);

    let ownerWindow = this._parent.ownerDocument.defaultView;
    ownerWindow.removeEventListener("resize", this._onResize);

    this._window.cancelAnimationFrame(this._animationId);
    this._iframe.remove();

    this._data = null;
    this._regions = null;
    this._cachedGraphImage = null;
    gCachedStripePattern.clear();
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
   * Builds and caches a graph image, based on the data source supplied
   * in `setData`. The graph image is not rebuilt on each frame, but
   * only when the data source changes.
   */
  buildGraphImage: function() {
    throw "This method needs to be implemented by inheriting classes.";
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
    this._cachedGraphImage = this.buildGraphImage();
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
      throw "Can't highlighted regions on a graph with no data displayed.";
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
   * Removes the selection.
   */
  dropSelection: function() {
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
    return this._selection.start != null && this._selection.end != null;
  },

  /**
   * Gets whether or not a selection is currently being made, for example
   * via a click+drag operation.
   * @return boolean
   */
  hasSelectionInProgress: function() {
    return this._selection.start != null && this._selection.end == null;
  },

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
    this._cursor.x = null;
    this._cursor.y = null;
    this._shouldRedraw = true;
  },

  /**
   * Gets whether or not this graph has a visible cursor.
   * @return boolean
   */
  hasCursor: function() {
    return this._cursor.x != null;
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
   */
  refresh: function() {
    let bounds = this._parent.getBoundingClientRect();
    this._iframe.setAttribute("width", bounds.width);
    this._iframe.setAttribute("height", bounds.height);
    this._width = this._canvas.width = bounds.width * this._pixelRatio;
    this._height = this._canvas.height = bounds.height * this._pixelRatio;

    if (this._data) {
      this._cachedGraphImage = this.buildGraphImage();
    }
    if (this._regions) {
      this._bakeRegions(this._regions, this._cachedGraphImage);
    }

    this._shouldRedraw = true;
    this.emit("refresh");
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

    if (this.hasCursor()) {
      this._drawCliphead();
    }
    if (this.hasSelection() || this.hasSelectionInProgress()) {
      this._drawSelection();
    }
    if (this.hasData()) {
      ctx.drawImage(this._cachedGraphImage, 0, 0, this._width, this._height);
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
   */
  _getContainerOffset: function() {
    let node = this._canvas;
    let x = 0;
    let y = 0;

    while (node = node.offsetParent) {
      x += node.offsetLeft;
      y += node.offsetTop;
    };

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
    if (this._cachedGraphImage) {
      setNamedTimeout(this._uid, GRAPH_RESIZE_EVENTS_DRAIN, this.refresh);
    }
  }
};

/**
 * A basic line graph, plotting values on a curve and adding helper lines
 * and tooltips for maximum, average and minimum values.
 *
 * @param nsIDOMNode parent
 *        The parent node holding the graph.
 * @param string metric [optional]
 *        The metric displayed in the graph, e.g. "fps" or "bananas".
 */
this.LineGraphWidget = function(parent, metric, ...args) {
  AbstractCanvasGraph.apply(this, [parent, "line-graph", ...args]);

  this.once("ready", () => {
    this._gutter = this._createGutter();
    this._maxGutterLine = this._createGutterLine("maximum");
    this._avgGutterLine = this._createGutterLine("average");
    this._minGutterLine = this._createGutterLine("minimum");
    this._maxTooltip = this._createTooltip("maximum", "start", "max", metric);
    this._avgTooltip = this._createTooltip("average", "end", "avg", metric);
    this._minTooltip = this._createTooltip("minimum", "start", "min", metric);
  });
}

LineGraphWidget.prototype = Heritage.extend(AbstractCanvasGraph.prototype, {
  clipheadLineColor: LINE_GRAPH_CLIPHEAD_LINE_COLOR,
  selectionLineColor: LINE_GRAPH_SELECTION_LINE_COLOR,
  selectionBackgroundColor: LINE_GRAPH_SELECTION_BACKGROUND_COLOR,
  selectionStripesColor: LINE_GRAPH_SELECTION_STRIPES_COLOR,
  regionBackgroundColor: LINE_GRAPH_REGION_BACKGROUND_COLOR,
  regionStripesColor: LINE_GRAPH_REGION_STRIPES_COLOR,

  /**
   * Points that are too close too each other in the graph will not be rendered.
   * This scalar specifies the required minimum squared distance between points.
   */
  minDistanceBetweenPoints: LINE_GRAPH_MIN_SQUARED_DISTANCE_BETWEEN_POINTS,

  /**
   * Renders the graph on a canvas.
   * @see AbstractCanvasGraph.prototype.buildGraphImage
   */
  buildGraphImage: function() {
    let canvas = this._document.createElementNS(HTML_NS, "canvas");
    let ctx = canvas.getContext("2d");
    let width = canvas.width = this._width;
    let height = canvas.height = this._height;

    let maxValue = Number.MIN_SAFE_INTEGER;
    let minValue = Number.MAX_SAFE_INTEGER;
    let sumValues = 0;
    let totalTicks = 0;
    let firstTick;
    let lastTick;

    for (let { delta, value } of this._data) {
      maxValue = Math.max(value, maxValue);
      minValue = Math.min(value, minValue);
      sumValues += value;
      totalTicks++;

      if (!firstTick) {
        firstTick = delta;
      } else {
        lastTick = delta;
      }
    }

    let dataScaleX = this.dataScaleX = width / lastTick;
    let dataScaleY = this.dataScaleY = height / maxValue * GRAPH_DAMPEN_VALUES;

    /**
     * Calculates the squared distance between two 2D points.
     */
    function distSquared(x0, y0, x1, y1) {
      let xs = x1 - x0;
      let ys = y1 - y0;
      return xs * xs + ys * ys;
    }

    // Draw the graph.

    let gradient = ctx.createLinearGradient(0, height / 2, 0, height);
    gradient.addColorStop(0, LINE_GRAPH_BACKGROUND_GRADIENT_START);
    gradient.addColorStop(1, LINE_GRAPH_BACKGROUND_GRADIENT_END);
    ctx.fillStyle = gradient;
    ctx.strokeStyle = LINE_GRAPH_STROKE_COLOR;
    ctx.lineWidth = LINE_GRAPH_STROKE_WIDTH;
    ctx.setLineDash([]);
    ctx.beginPath();

    let prevX = 0;
    let prevY = 0;

    for (let { delta, value } of this._data) {
      let currX = delta * dataScaleX;
      let currY = height - value * dataScaleY;

      if (delta == firstTick) {
        ctx.moveTo(-LINE_GRAPH_STROKE_WIDTH, height);
        ctx.lineTo(-LINE_GRAPH_STROKE_WIDTH, currY);
      }

      let distance = distSquared(prevX, prevY, currX, currY);
      if (distance > this.minDistanceBetweenPoints) {
        ctx.lineTo(currX, currY);
        prevX = currX;
        prevY = currY;
      }

      if (delta == lastTick) {
        ctx.lineTo(width + LINE_GRAPH_STROKE_WIDTH, currY);
        ctx.lineTo(width + LINE_GRAPH_STROKE_WIDTH, height);
      }
    }

    ctx.fill();
    ctx.stroke();

    // Draw the maximum value horizontal line.

    ctx.strokeStyle = LINE_GRAPH_MAXIMUM_LINE_COLOR;
    ctx.lineWidth = LINE_GRAPH_HELPER_LINES_WIDTH;
    ctx.setLineDash(LINE_GRAPH_HELPER_LINES_DASH);
    ctx.beginPath();
    let maximumY = height - maxValue * dataScaleY - ctx.lineWidth;
    ctx.moveTo(0, maximumY);
    ctx.lineTo(width, maximumY);
    ctx.stroke();

    // Draw the average value horizontal line.

    ctx.strokeStyle = LINE_GRAPH_AVERAGE_LINE_COLOR;
    ctx.lineWidth = LINE_GRAPH_HELPER_LINES_WIDTH;
    ctx.setLineDash(LINE_GRAPH_HELPER_LINES_DASH);
    ctx.beginPath();
    let avgValue = sumValues / totalTicks;
    let averageY = height - avgValue * dataScaleY - ctx.lineWidth;
    ctx.moveTo(0, averageY);
    ctx.lineTo(width, averageY);
    ctx.stroke();

    // Draw the minimum value horizontal line.

    ctx.strokeStyle = LINE_GRAPH_MINIMUM_LINE_COLOR;
    ctx.lineWidth = LINE_GRAPH_HELPER_LINES_WIDTH;
    ctx.setLineDash(LINE_GRAPH_HELPER_LINES_DASH);
    ctx.beginPath();
    let minimumY = height - minValue * dataScaleY - ctx.lineWidth;
    ctx.moveTo(0, minimumY);
    ctx.lineTo(width, minimumY);
    ctx.stroke();

    // Update the tooltips text and gutter lines.

    this._maxTooltip.querySelector("[text=value]").textContent = maxValue|0;
    this._avgTooltip.querySelector("[text=value]").textContent = avgValue|0;
    this._minTooltip.querySelector("[text=value]").textContent = minValue|0;

    /**
     * Maps a value from one range to another.
     */
    function map(value, istart, istop, ostart, ostop) {
      return ostart + (ostop - ostart) * ((value - istart) / (istop - istart));
    }

    /**
     * Constrains a value to a range.
     */
    function clamp(value, min, max) {
      if (value < min) return min;
      if (value > max) return max;
      return value;
    }

    let bottom = height / this._pixelRatio;
    let maxPosY = map(maxValue * GRAPH_DAMPEN_VALUES, 0, maxValue, bottom, 0);
    let avgPosY = map(avgValue * GRAPH_DAMPEN_VALUES, 0, maxValue, bottom, 0);
    let minPosY = map(minValue * GRAPH_DAMPEN_VALUES, 0, maxValue, bottom, 0);

    let safeTop = LINE_GRAPH_TOOLTIP_SAFE_BOUNDS;
    let safeBottom = bottom - LINE_GRAPH_TOOLTIP_SAFE_BOUNDS;

    this._maxTooltip.style.top = clamp(maxPosY, safeTop, safeBottom) + "px";
    this._avgTooltip.style.top = clamp(avgPosY, safeTop, safeBottom) + "px";
    this._minTooltip.style.top = clamp(minPosY, safeTop, safeBottom) + "px";
    this._maxGutterLine.style.top = clamp(maxPosY, safeTop, safeBottom) + "px";
    this._avgGutterLine.style.top = clamp(avgPosY, safeTop, safeBottom) + "px";
    this._minGutterLine.style.top = clamp(minPosY, safeTop, safeBottom) + "px";

    return canvas;
  },

  /**
   * Creates the gutter node when constructing this graph.
   * @return nsIDOMNode
   */
  _createGutter: function() {
    let gutter = this._document.createElementNS(HTML_NS, "div");
    gutter.className = "line-graph-widget-gutter";
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

  ctx.strokeStyle = stripesColor;
  ctx.lineWidth = GRAPH_STRIPE_PATTERN_LINE_WIDTH;
  ctx.lineCap = "square";
  ctx.beginPath();

  for (let i = -height; i <= height; i += GRAPH_STRIPE_PATTERN_LINE_SPACING) {
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
