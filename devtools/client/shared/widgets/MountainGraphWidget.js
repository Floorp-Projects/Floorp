/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { extend } = require("devtools/shared/extend");
const {
  AbstractCanvasGraph,
} = require("devtools/client/shared/widgets/Graphs");

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
 * @param Node parent
 *        The parent node holding the graph.
 */
this.MountainGraphWidget = function(parent, ...args) {
  AbstractCanvasGraph.apply(this, [parent, "mountain-graph", ...args]);
};

MountainGraphWidget.prototype = extend(AbstractCanvasGraph.prototype, {
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
    const { canvas, ctx } = this._getNamedCanvas("mountain-graph-background");
    const width = this._width;
    const height = this._height;

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
      throw new Error(
        "The graph format traits are mandatory to style " + "the data source."
      );
    }
    const { canvas, ctx } = this._getNamedCanvas("mountain-graph-data");
    const width = this._width;
    const height = this._height;

    const totalSections = this.format.length;
    const totalTicks = this._data.length;
    const firstTick = totalTicks ? this._data[0].delta : 0;
    const lastTick = totalTicks ? this._data[totalTicks - 1].delta : 0;

    const duration = this.dataDuration || lastTick;
    const dataScaleX = (this.dataScaleX =
      width / (duration - this.dataOffsetX));
    const dataScaleY = (this.dataScaleY = height * this.dampenValuesFactor);

    // Draw the graph.

    const prevHeights = Array.from({ length: totalTicks }).fill(0);

    ctx.globalCompositeOperation = "destination-over";
    ctx.strokeStyle = this.strokeColor;
    ctx.lineWidth = this.strokeWidth * this._pixelRatio;

    for (let section = 0; section < totalSections; section++) {
      ctx.fillStyle = this.format[section].color || "#000";
      ctx.beginPath();

      for (let tick = 0; tick < totalTicks; tick++) {
        const { delta, values } = this._data[tick];
        const currX = (delta - this.dataOffsetX) * dataScaleX;
        const currY = values[section] * dataScaleY;
        const prevY = prevHeights[tick];

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
    const maximumY = height * this.dampenValuesFactor;
    ctx.moveTo(0, maximumY);
    ctx.lineTo(width, maximumY);
    ctx.stroke();

    // Draw the average value horizontal line.

    ctx.beginPath();
    const averageY = (height / 2) * this.dampenValuesFactor;
    ctx.moveTo(0, averageY);
    ctx.lineTo(width, averageY);
    ctx.stroke();

    return canvas;
  },
});

module.exports = MountainGraphWidget;
