"use strict";

const { extend } = require("devtools/shared/extend");
const { setNamedTimeout, clearNamedTimeout } = require("devtools/client/shared/widgets/view-helpers");
const { AbstractCanvasGraph, CanvasGraphUtils } = require("devtools/client/shared/widgets/Graphs");

const HTML_NS = "http://www.w3.org/1999/xhtml";

// Bar graph constants.

const GRAPH_DAMPEN_VALUES_FACTOR = 0.75;

// The following are in pixels
const GRAPH_BARS_MARGIN_TOP = 1;
const GRAPH_BARS_MARGIN_END = 1;
const GRAPH_MIN_BARS_WIDTH = 5;
const GRAPH_MIN_BLOCKS_HEIGHT = 1;

const GRAPH_BACKGROUND_GRADIENT_START = "rgba(0,136,204,0.0)";
const GRAPH_BACKGROUND_GRADIENT_END = "rgba(255,255,255,0.25)";

const GRAPH_CLIPHEAD_LINE_COLOR = "#666";
const GRAPH_SELECTION_LINE_COLOR = "#555";
const GRAPH_SELECTION_BACKGROUND_COLOR = "rgba(0,136,204,0.25)";
const GRAPH_SELECTION_STRIPES_COLOR = "rgba(255,255,255,0.1)";
const GRAPH_REGION_BACKGROUND_COLOR = "transparent";
const GRAPH_REGION_STRIPES_COLOR = "rgba(237,38,85,0.2)";

const GRAPH_HIGHLIGHTS_MASK_BACKGROUND = "rgba(255,255,255,0.75)";
const GRAPH_HIGHLIGHTS_MASK_STRIPES = "rgba(255,255,255,0.5)";

// in ms
const GRAPH_LEGEND_MOUSEOVER_DEBOUNCE = 50;

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
 * @param Node parent
 *        The parent node holding the graph.
 */
this.BarGraphWidget = function(parent, ...args) {
  AbstractCanvasGraph.apply(this, [parent, "bar-graph", ...args]);

  this.once("ready", () => {
    this._onLegendMouseOver = this._onLegendMouseOver.bind(this);
    this._onLegendMouseOut = this._onLegendMouseOut.bind(this);
    this._onLegendMouseDown = this._onLegendMouseDown.bind(this);
    this._onLegendMouseUp = this._onLegendMouseUp.bind(this);
    this._createLegend();
  });
};

BarGraphWidget.prototype = extend(AbstractCanvasGraph.prototype, {
  clipheadLineColor: GRAPH_CLIPHEAD_LINE_COLOR,
  selectionLineColor: GRAPH_SELECTION_LINE_COLOR,
  selectionBackgroundColor: GRAPH_SELECTION_BACKGROUND_COLOR,
  selectionStripesColor: GRAPH_SELECTION_STRIPES_COLOR,
  regionBackgroundColor: GRAPH_REGION_BACKGROUND_COLOR,
  regionStripesColor: GRAPH_REGION_STRIPES_COLOR,

  /**
   * List of colors used to fill each block inside every bar, also
   * corresponding to labels displayed in this graph's legend.
   * @see constructor
   */
  format: null,

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
   * The scalar used to multiply the graph values to leave some headroom
   * on the top.
   */
  dampenValuesFactor: GRAPH_DAMPEN_VALUES_FACTOR,

  /**
   * Bars that are too close too each other in the graph will be combined.
   * This scalar specifies the required minimum width of each bar.
   */
  minBarsWidth: GRAPH_MIN_BARS_WIDTH,

  /**
   * Blocks in a bar that are too thin inside the bar will not be rendered.
   * This scalar specifies the required minimum height of each block.
   */
  minBlocksHeight: GRAPH_MIN_BLOCKS_HEIGHT,

  /**
   * Renders the graph's background.
   * @see AbstractCanvasGraph.prototype.buildBackgroundImage
   */
  buildBackgroundImage: function() {
    const { canvas, ctx } = this._getNamedCanvas("bar-graph-background");
    const width = this._width;
    const height = this._height;

    const gradient = ctx.createLinearGradient(0, 0, 0, height);
    gradient.addColorStop(0, GRAPH_BACKGROUND_GRADIENT_START);
    gradient.addColorStop(1, GRAPH_BACKGROUND_GRADIENT_END);
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
      throw new Error("The graph format traits are mandatory to style " +
                      "the data source.");
    }
    const { canvas, ctx } = this._getNamedCanvas("bar-graph-data");
    const width = this._width;
    const height = this._height;

    const totalTypes = this.format.length;
    const totalTicks = this._data.length;
    const lastTick = this._data[totalTicks - 1].delta;

    const minBarsWidth = this.minBarsWidth * this._pixelRatio;
    const minBlocksHeight = this.minBlocksHeight * this._pixelRatio;

    const duration = this.dataDuration || lastTick;
    const dataScaleX = this.dataScaleX = width / (duration - this.dataOffsetX);
    const dataScaleY = this.dataScaleY = height / this._calcMaxHeight({
      data: this._data,
      dataScaleX: dataScaleX,
      minBarsWidth: minBarsWidth
    }) * this.dampenValuesFactor;

    // Draw the graph.

    // Iterate over the blocks, then the bars, to draw all rectangles of
    // the same color in a single pass. See the @constructor for more
    // information about the data source, and how a "bar" contains "blocks".

    this._blocksBoundingRects = [];
    const prevHeight = [];
    const scaledMarginEnd = GRAPH_BARS_MARGIN_END * this._pixelRatio;
    const scaledMarginTop = GRAPH_BARS_MARGIN_TOP * this._pixelRatio;

    for (let type = 0; type < totalTypes; type++) {
      ctx.fillStyle = this.format[type].color || "#000";
      ctx.beginPath();

      let prevRight = 0;
      let skippedCount = 0;
      let skippedHeight = 0;

      for (let tick = 0; tick < totalTicks; tick++) {
        const delta = this._data[tick].delta;
        const value = this._data[tick].values[type] || 0;
        const blockRight = (delta - this.dataOffsetX) * dataScaleX;
        const blockHeight = value * dataScaleY;

        const blockWidth = blockRight - prevRight;
        if (blockWidth < minBarsWidth) {
          skippedCount++;
          skippedHeight += blockHeight;
          continue;
        }

        const averageHeight = (blockHeight + skippedHeight) / (skippedCount + 1);
        if (averageHeight >= minBlocksHeight) {
          const bottom = height - ~~prevHeight[tick];
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
            prevHeight[tick] = averageHeight + scaledMarginTop;
          } else {
            prevHeight[tick] += averageHeight + scaledMarginTop;
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
    for (const { color, label } of this.format) {
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
  buildMaskImage: function(highlights, inPixels = false,
                            unpack = e => e.delta) {
    // A null `highlights` array is used to clear the mask. An empty array
    // will mask the entire graph.
    if (!highlights) {
      return null;
    }

    // Get a render target for the highlights. It will be overlaid on top of
    // the existing graph, masking the areas that aren't highlighted.

    const { canvas, ctx } = this._getNamedCanvas("graph-highlights");
    const width = this._width;
    const height = this._height;

    // Draw the background mask.

    const pattern = AbstractCanvasGraph.getStripePattern({
      ownerDocument: this._document,
      backgroundColor: GRAPH_HIGHLIGHTS_MASK_BACKGROUND,
      stripesColor: GRAPH_HIGHLIGHTS_MASK_STRIPES
    });
    ctx.fillStyle = pattern;
    ctx.fillRect(0, 0, width, height);

    // Clear highlighted areas.

    const totalTicks = this._data.length;
    const firstTick = unpack(this._data[0]);
    const lastTick = unpack(this._data[totalTicks - 1]);

    for (let { start, end, top, bottom } of highlights) {
      if (!inPixels) {
        start = CanvasGraphUtils.map(start, firstTick, lastTick, 0, width);
        end = CanvasGraphUtils.map(end, firstTick, lastTick, 0, width);
      }
      const firstSnap = findFirst(this._blocksBoundingRects,
                                e => e.start >= start);
      const lastSnap = findLast(this._blocksBoundingRects,
                              e => e.start >= start && e.end <= end);

      const x1 = firstSnap ? firstSnap.start : start;
      let x2;
      if (lastSnap) {
        x2 = lastSnap.end;
      } else {
        x2 = firstSnap ? firstSnap.end : end;
      }

      const y1 = top || 0;
      const y2 = bottom || height;
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
    const scaledMarginEnd = GRAPH_BARS_MARGIN_END * this._pixelRatio;

    for (const { delta, values } of data) {
      const barRight = (delta - this.dataOffsetX) * dataScaleX;
      const barHeight = values.reduce((a, b) => a + b, 0);

      const barWidth = barRight - prevRight;
      if (barWidth < minBarsWidth) {
        skippedCount++;
        skippedHeight += barHeight;
        continue;
      }

      const averageHeight = (barHeight + skippedHeight) / (skippedCount + 1);
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
    const legendNode = this._legendNode = this._document.createElementNS(HTML_NS,
                                                                       "div");
    legendNode.className = "bar-graph-widget-legend";
    this._container.appendChild(legendNode);
  },

  /**
   * Creates a legend item when constructing this graph.
   */
  _createLegendItem: function(color, label) {
    const itemNode = this._document.createElementNS(HTML_NS, "div");
    itemNode.className = "bar-graph-widget-legend-item";

    const colorNode = this._document.createElementNS(HTML_NS, "span");
    colorNode.setAttribute("view", "color");
    colorNode.setAttribute("data-index", this._legendNode.childNodes.length);
    colorNode.style.backgroundColor = color;
    colorNode.addEventListener("mouseover", this._onLegendMouseOver);
    colorNode.addEventListener("mouseout", this._onLegendMouseOut);
    colorNode.addEventListener("mousedown", this._onLegendMouseDown);
    colorNode.addEventListener("mouseup", this._onLegendMouseUp);

    const labelNode = this._document.createElementNS(HTML_NS, "span");
    labelNode.setAttribute("view", "label");
    labelNode.textContent = label;

    itemNode.appendChild(colorNode);
    itemNode.appendChild(labelNode);
    this._legendNode.appendChild(itemNode);
  },

  /**
   * Invoked whenever a color node in the legend is hovered.
   */
  _onLegendMouseOver: function(ev) {
    setNamedTimeout(
      "bar-graph-debounce",
      GRAPH_LEGEND_MOUSEOVER_DEBOUNCE,
      () => {
        const type = ev.target.dataset.index;
        const rects = this._blocksBoundingRects.filter(e => e.type == type);

        this._originalHighlights = this._mask;
        this._hasCustomHighlights = true;
        this.setMask(rects, true);

        this.emit("legend-hover", [type, rects]);
      }
    );
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
  _onLegendMouseDown: function(ev) {
    ev.preventDefault();
    ev.stopPropagation();

    const type = ev.target.dataset.index;
    const rects = this._blocksBoundingRects.filter(e => e.type == type);
    const leftmost = rects[0];
    const rightmost = rects[rects.length - 1];
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

/**
 * Finds the first element in an array that validates a predicate.
 * @param array
 * @param function predicate
 * @return number
 */
function findFirst(array, predicate) {
  for (let i = 0, len = array.length; i < len; i++) {
    const element = array[i];
    if (predicate(element)) {
      return element;
    }
  }
  return null;
}

/**
 * Finds the last element in an array that validates a predicate.
 * @param array
 * @param function predicate
 * @return number
 */
function findLast(array, predicate) {
  for (let i = array.length - 1; i >= 0; i--) {
    const element = array[i];
    if (predicate(element)) {
      return element;
    }
  }
  return null;
}

module.exports = BarGraphWidget;
