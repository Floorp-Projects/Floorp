"use strict";

const { extend } = require("devtools/shared/extend");
const { AbstractCanvasGraph, CanvasGraphUtils } = require("devtools/client/shared/widgets/Graphs");
const { LocalizationHelper } = require("devtools/shared/l10n");

const HTML_NS = "http://www.w3.org/1999/xhtml";
const L10N = new LocalizationHelper("devtools/client/locales/graphs.properties");

// Line graph constants.

const GRAPH_DAMPEN_VALUES_FACTOR = 0.85;
const GRAPH_TOOLTIP_SAFE_BOUNDS = 8; // px
const GRAPH_MIN_MAX_TOOLTIP_DISTANCE = 14; // px

const GRAPH_BACKGROUND_COLOR = "#0088cc";
const GRAPH_STROKE_WIDTH = 1; // px
const GRAPH_STROKE_COLOR = "rgba(255,255,255,0.9)";
const GRAPH_HELPER_LINES_DASH = [5]; // px
const GRAPH_HELPER_LINES_WIDTH = 1; // px
const GRAPH_MAXIMUM_LINE_COLOR = "rgba(255,255,255,0.4)";
const GRAPH_AVERAGE_LINE_COLOR = "rgba(255,255,255,0.7)";
const GRAPH_MINIMUM_LINE_COLOR = "rgba(255,255,255,0.9)";
const GRAPH_BACKGROUND_GRADIENT_START = "rgba(255,255,255,0.25)";
const GRAPH_BACKGROUND_GRADIENT_END = "rgba(255,255,255,0.0)";

const GRAPH_CLIPHEAD_LINE_COLOR = "#fff";
const GRAPH_SELECTION_LINE_COLOR = "#fff";
const GRAPH_SELECTION_BACKGROUND_COLOR = "rgba(44,187,15,0.25)";
const GRAPH_SELECTION_STRIPES_COLOR = "rgba(255,255,255,0.1)";
const GRAPH_REGION_BACKGROUND_COLOR = "transparent";
const GRAPH_REGION_STRIPES_COLOR = "rgba(237,38,85,0.2)";

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
 * @param Node parent
 *        The parent node holding the graph.
 * @param object options [optional]
 *  `metric`: The metric displayed in the graph, e.g. "fps" or "bananas".
 *  `min`: Boolean whether to show the min tooltip/gutter/line (default: true)
 *  `max`: Boolean whether to show the max tooltip/gutter/line (default: true)
 *  `avg`: Boolean whether to show the avg tooltip/gutter/line (default: true)
 */
this.LineGraphWidget = function(parent, options = {}, ...args) {
  let { metric, min, max, avg } = options;

  this._showMin = min !== false;
  this._showMax = max !== false;
  this._showAvg = avg !== false;

  AbstractCanvasGraph.apply(this, [parent, "line-graph", ...args]);

  this.once("ready", () => {
    // Create all gutters and tooltips incase the showing of min/max/avg
    // are changed later
    this._gutter = this._createGutter();
    this._maxGutterLine = this._createGutterLine("maximum");
    this._maxTooltip = this._createTooltip(
      "maximum", "start", L10N.getStr("graphs.label.maximum"), metric
    );
    this._minGutterLine = this._createGutterLine("minimum");
    this._minTooltip = this._createTooltip(
      "minimum", "start", L10N.getStr("graphs.label.minimum"), metric
    );
    this._avgGutterLine = this._createGutterLine("average");
    this._avgTooltip = this._createTooltip(
      "average", "end", L10N.getStr("graphs.label.average"), metric
    );
  });
};

LineGraphWidget.prototype = extend(AbstractCanvasGraph.prototype, {
  backgroundColor: GRAPH_BACKGROUND_COLOR,
  backgroundGradientStart: GRAPH_BACKGROUND_GRADIENT_START,
  backgroundGradientEnd: GRAPH_BACKGROUND_GRADIENT_END,
  strokeColor: GRAPH_STROKE_COLOR,
  strokeWidth: GRAPH_STROKE_WIDTH,
  maximumLineColor: GRAPH_MAXIMUM_LINE_COLOR,
  averageLineColor: GRAPH_AVERAGE_LINE_COLOR,
  minimumLineColor: GRAPH_MINIMUM_LINE_COLOR,
  clipheadLineColor: GRAPH_CLIPHEAD_LINE_COLOR,
  selectionLineColor: GRAPH_SELECTION_LINE_COLOR,
  selectionBackgroundColor: GRAPH_SELECTION_BACKGROUND_COLOR,
  selectionStripesColor: GRAPH_SELECTION_STRIPES_COLOR,
  regionBackgroundColor: GRAPH_REGION_BACKGROUND_COLOR,
  regionStripesColor: GRAPH_REGION_STRIPES_COLOR,

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
  dampenValuesFactor: GRAPH_DAMPEN_VALUES_FACTOR,

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
   * @param number duration
   *        The duration of the recording in milliseconds.
   */
  async setDataFromTimestamps(timestamps, interval, duration) {
    let {
      plottedData,
      plottedMinMaxSum
    } = await CanvasGraphUtils._performTaskInWorker("plotTimestampsGraph", {
      timestamps, interval, duration
    });

    this._tempMinMaxSum = plottedMinMaxSum;
    this.setData(plottedData);
  },

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
      for (let { value } of this._data) {
        maxValue = Math.max(value, maxValue);
        minValue = Math.min(value, minValue);
        sumValues += value;
      }
      avgValue = sumValues / totalTicks;
    }

    let duration = this.dataDuration || lastTick;
    let dataScaleX = this.dataScaleX = width / (duration - this.dataOffsetX);
    let dataScaleY =
      this.dataScaleY = height / maxValue * this.dampenValuesFactor;

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
        ctx.moveTo(-GRAPH_STROKE_WIDTH, height);
        ctx.lineTo(-GRAPH_STROKE_WIDTH, currY);
      }

      ctx.lineTo(currX, currY);

      if (delta == lastTick) {
        ctx.lineTo(width + GRAPH_STROKE_WIDTH, currY);
        ctx.lineTo(width + GRAPH_STROKE_WIDTH, height);
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
      ctx.lineWidth = GRAPH_HELPER_LINES_WIDTH;
      ctx.setLineDash(GRAPH_HELPER_LINES_DASH);
      ctx.beginPath();
      let maximumY = height - maxValue * dataScaleY;
      ctx.moveTo(0, maximumY);
      ctx.lineTo(width, maximumY);
      ctx.stroke();
    }

    // Draw the average value horizontal line.
    if (this._showAvg) {
      ctx.strokeStyle = this.averageLineColor;
      ctx.lineWidth = GRAPH_HELPER_LINES_WIDTH;
      ctx.setLineDash(GRAPH_HELPER_LINES_DASH);
      ctx.beginPath();
      let averageY = height - avgValue * dataScaleY;
      ctx.moveTo(0, averageY);
      ctx.lineTo(width, averageY);
      ctx.stroke();
    }

    // Draw the minimum value horizontal line.
    if (this._showMin) {
      ctx.strokeStyle = this.minimumLineColor;
      ctx.lineWidth = GRAPH_HELPER_LINES_WIDTH;
      ctx.setLineDash(GRAPH_HELPER_LINES_DASH);
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
    let maxPosY = CanvasGraphUtils.map(maxValue * this.dampenValuesFactor, 0,
                                       maxValue, bottom, 0);
    let avgPosY = CanvasGraphUtils.map(avgValue * this.dampenValuesFactor, 0,
                                       maxValue, bottom, 0);
    let minPosY = CanvasGraphUtils.map(minValue * this.dampenValuesFactor, 0,
                                       maxValue, bottom, 0);

    let safeTop = GRAPH_TOOLTIP_SAFE_BOUNDS;
    let safeBottom = bottom - GRAPH_TOOLTIP_SAFE_BOUNDS;

    let maxTooltipTop = (this.withFixedTooltipPositions
      ? safeTop : CanvasGraphUtils.clamp(maxPosY, safeTop, safeBottom));
    let avgTooltipTop = (this.withFixedTooltipPositions
      ? safeTop : CanvasGraphUtils.clamp(avgPosY, safeTop, safeBottom));
    let minTooltipTop = (this.withFixedTooltipPositions
      ? safeBottom : CanvasGraphUtils.clamp(minPosY, safeTop, safeBottom));

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
    this._maxTooltip.hidden = this._showMax === false
                            || !totalTicks
                            || distanceMinMax < GRAPH_MIN_MAX_TOOLTIP_DISTANCE;
    this._avgTooltip.hidden = this._showAvg === false || !totalTicks;
    this._minTooltip.hidden = this._showMin === false || !totalTicks;
    this._gutter.hidden = (this._showMin === false &&
                           this._showAvg === false &&
                           this._showMax === false) || !totalTicks;

    this._maxGutterLine.hidden = this._showMax === false;
    this._avgGutterLine.hidden = this._showAvg === false;
    this._minGutterLine.hidden = this._showMin === false;
  },

  /**
   * Creates the gutter node when constructing this graph.
   * @return Node
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
   * @return Node
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
   * @return Node
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

module.exports = LineGraphWidget;
