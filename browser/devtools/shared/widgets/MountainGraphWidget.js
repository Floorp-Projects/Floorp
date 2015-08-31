"use strict";

const { Cc, Ci, Cu, Cr } = require("chrome");

const { Heritage } = require("resource:///modules/devtools/ViewHelpers.jsm");
const { AbstractCanvasGraph, CanvasGraphUtils } = require("devtools/shared/widgets/Graphs");

const HTML_NS = "http://www.w3.org/1999/xhtml";

// Bar graph constants.

const GRAPH_DAMPEN_VALUES_FACTOR = 0.9;

const GRAPH_BACKGROUND_COLOR = "#ddd";
const GRAPH_STROKE_WIDTH = 1; // px
const GRAPH_STROKE_COLOR = "rgba(255,255,255,0.9)";
const GRAPH_HELPER_LINES_DASH = [5]; // px
const GRAPH_HELPER_LINES_WIDTH = 1; // px

const GRAPH_CLIPHEAD_LINE_COLOR = "#fff";
const GRAPH_SELECTION_LINE_COLOR = "#fff";
const GRAPH_SELECTION_BACKGROUND_COLOR = "rgba(44,187,15,0.25)";
const GRAPH_SELECTION_STRIPES_COLOR = "rgba(255,255,255,0.1)";
const GRAPH_REGION_BACKGROUND_COLOR = "transparent";
const GRAPH_REGION_STRIPES_COLOR = "rgba(237,38,85,0.2)";

/**
 * A mountain graph, plotting sets of values as line graphs.
 *
 * @see AbstractCanvasGraph for emitted events and other options.
 *
 * Example usage:
 *   let graph = new MountainGraphWidget(node);
 *   graph.format = ...;
 *   graph.once("ready", () => {
 *     graph.setData(src);
 *   });
 *
 * The `graph.format` traits are mandatory and will determine how each
 * section of the moutain will be styled:
 *   [
 *     { color: "#f00", ... },
 *     { color: "#0f0", ... },
 *     ...
 *     { color: "#00f", ... }
 *   ]
 *
 * Data source format:
 *   [
 *     { delta: x1, values: [y11, y12, ... y1n] },
 *     { delta: x2, values: [y21, y22, ... y2n] },
 *     ...
 *     { delta: xm, values: [ym1, ym2, ... ymn] }
 *   ]
 * where the [ymn] values is assumed to aready be normalized from [0..1].
 *
 * @param nsIDOMNode parent
 *        The parent node holding the graph.
 */
this.MountainGraphWidget = function(parent, ...args) {
  AbstractCanvasGraph.apply(this, [parent, "mountain-graph", ...args]);
};

MountainGraphWidget.prototype = Heritage.extend(AbstractCanvasGraph.prototype, {
  backgroundColor: GRAPH_BACKGROUND_COLOR,
  strokeColor: GRAPH_STROKE_COLOR,
  strokeWidth: GRAPH_STROKE_WIDTH,
  clipheadLineColor: GRAPH_CLIPHEAD_LINE_COLOR,
  selectionLineColor: GRAPH_SELECTION_LINE_COLOR,
  selectionBackgroundColor: GRAPH_SELECTION_BACKGROUND_COLOR,
  selectionStripesColor: GRAPH_SELECTION_STRIPES_COLOR,
  regionBackgroundColor: GRAPH_REGION_BACKGROUND_COLOR,
  regionStripesColor: GRAPH_REGION_STRIPES_COLOR,

  /**
   * List of rules used to style each section of the mountain.
   * @see constructor
   * @type array
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
   * Renders the graph's background.
   * @see AbstractCanvasGraph.prototype.buildBackgroundImage
   */
  buildBackgroundImage: function() {
    let { canvas, ctx } = this._getNamedCanvas("mountain-graph-background");
    let width = this._width;
    let height = this._height;

    ctx.fillStyle = this.backgroundColor;
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
    let { canvas, ctx } = this._getNamedCanvas("mountain-graph-data");
    let width = this._width;
    let height = this._height;

    let totalSections = this.format.length;
    let totalTicks = this._data.length;
    let firstTick = totalTicks ? this._data[0].delta : 0;
    let lastTick = totalTicks ? this._data[totalTicks - 1].delta : 0;

    let duration = this.dataDuration || lastTick;
    let dataScaleX = this.dataScaleX = width / (duration - this.dataOffsetX);
    let dataScaleY = this.dataScaleY = height * this.dampenValuesFactor;

    // Draw the graph.

    let prevHeights = Array.from({ length: totalTicks }).fill(0);

    ctx.globalCompositeOperation = "destination-over";
    ctx.strokeStyle = this.strokeColor;
    ctx.lineWidth = this.strokeWidth * this._pixelRatio;

    for (let section = 0; section < totalSections; section++) {
      ctx.fillStyle = this.format[section].color || "#000";
      ctx.beginPath();

      for (let tick = 0; tick < totalTicks; tick++) {
        let { delta, values } = this._data[tick];
        let currX = (delta - this.dataOffsetX) * dataScaleX;
        let currY = values[section] * dataScaleY;
        let prevY = prevHeights[tick];

        if (delta == firstTick) {
          ctx.moveTo(-GRAPH_STROKE_WIDTH, height);
          ctx.lineTo(-GRAPH_STROKE_WIDTH, height - currY - prevY);
        }

        ctx.lineTo(currX, height - currY - prevY);

        if (delta == lastTick) {
          ctx.lineTo(width + GRAPH_STROKE_WIDTH, height - currY - prevY);
          ctx.lineTo(width + GRAPH_STROKE_WIDTH, height);
        }

        prevHeights[tick] += currY;
      }

      ctx.fill();
      ctx.stroke();
    }

    ctx.globalCompositeOperation = "source-over";
    ctx.lineWidth = GRAPH_HELPER_LINES_WIDTH;
    ctx.setLineDash(GRAPH_HELPER_LINES_DASH);

    // Draw the maximum value horizontal line.

    ctx.beginPath();
    let maximumY = height * this.dampenValuesFactor;
    ctx.moveTo(0, maximumY);
    ctx.lineTo(width, maximumY);
    ctx.stroke();

    // Draw the average value horizontal line.

    ctx.beginPath();
    let averageY = height / 2 * this.dampenValuesFactor;
    ctx.moveTo(0, averageY);
    ctx.lineTo(width, averageY);
    ctx.stroke();

    return canvas;
  }
});

module.exports = MountainGraphWidget;
