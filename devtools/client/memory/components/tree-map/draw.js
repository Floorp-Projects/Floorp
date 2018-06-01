/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
/**
 * Draw the treemap into the provided canvases using the 2d context. The treemap
 * layout is computed with d3. There are 2 canvases provided, each matching
 * the resolution of the window. The main canvas is a fully drawn version of
 * the treemap that is positioned and zoomed using css. It gets blurry the more
 * you zoom in as it doesn't get redrawn when zooming. The zoom canvas is
 * repositioned absolutely after every change in the dragZoom object, and then
 * redrawn to provide a full-resolution (non-blurry) view of zoomed in segment
 * of the treemap.
 */

const colorCoarseType = require("./color-coarse-type");
const {
  hslToStyle,
  formatAbbreviatedBytes,
  L10N
} = require("devtools/client/memory/utils");

// A constant fully zoomed out dragZoom object for the main canvas
const NO_SCROLL = {
  translateX: 0,
  translateY: 0,
  zoom: 0,
  offsetX: 0,
  offsetY: 0
};

// Drawing constants
const ELLIPSIS = "...";
const TEXT_MARGIN = 2;
const TEXT_COLOR = "#000000";
const TEXT_LIGHT_COLOR = "rgba(0,0,0,0.5)";
const LINE_WIDTH = 1;
const FONT_SIZE = 10;
const FONT_LINE_HEIGHT = 2;
const PADDING = [5 + FONT_SIZE, 5, 5, 5];
const COUNT_LABEL = L10N.getStr("tree-map.node-count");

/**
 * Setup and start drawing the treemap visualization
 *
 * @param  {Object} report
 * @param  {Object} canvases
 *         A CanvasUtils object that contains references to the main and zoom
 *         canvases and contexts
 * @param  {Object} dragZoom
 *         A DragZoom object representing the current state of the dragging
 *         and zooming behavior
 */
exports.setupDraw = function(report, canvases, dragZoom) {
  const getTreemap = configureD3Treemap.bind(null, canvases.main.canvas);

  let treemap, nodes;

  function drawFullTreemap() {
    treemap = getTreemap();
    nodes = treemap(report);
    drawTreemap(canvases.main, nodes, NO_SCROLL);
    drawTreemap(canvases.zoom, nodes, dragZoom);
  }

  function drawZoomedTreemap() {
    drawTreemap(canvases.zoom, nodes, dragZoom);
    positionZoomedCanvas(canvases.zoom.canvas, dragZoom);
  }

  drawFullTreemap();
  canvases.on("resize", drawFullTreemap);
  dragZoom.on("change", drawZoomedTreemap);
};

/**
 * Returns a configured d3 treemap function
 *
 * @param  {HTMLCanvasElement} canvas
 * @return {Function}
 */
const configureD3Treemap = exports.configureD3Treemap = function(canvas) {
  const window = canvas.ownerDocument.defaultView;
  const ratio = window.devicePixelRatio;
  const treemap = window.d3.layout.treemap()
    .size([
      // The d3 layout includes the padding around everything, add some
      // extra padding to the size to compensate for thi
      canvas.width + (PADDING[1] + PADDING[3]) * ratio,
      canvas.height + (PADDING[0] + PADDING[2]) * ratio
    ])
    .sticky(true)
    .padding([
      PADDING[0] * ratio,
      PADDING[1] * ratio,
      PADDING[2] * ratio,
      PADDING[3] * ratio,
    ])
    .value(d => d.bytes);

  /**
   * Create treemap nodes from a census report that are sorted by depth
   *
   * @param  {Object} report
   * @return {Array} An array of d3 treemap nodes
   *         // https://github.com/mbostock/d3/wiki/Treemap-Layout
   *         parent - the parent node, or null for the root.
   *         children - the array of child nodes, or null for leaf nodes.
   *         value - the node value, as returned by the value accessor.
   *         depth - the depth of the node, starting at 0 for the root.
   *         area - the computed pixel area of this node.
   *         x - the minimum x-coordinate of the node position.
   *         y - the minimum y-coordinate of the node position.
   *         z - the orientation of this cell’s subdivision, if any.
   *         dx - the x-extent of the node position.
   *         dy - the y-extent of the node position.
   */
  return function depthSortedNodes(report) {
    const nodes = treemap(report);
    nodes.sort((a, b) => a.depth - b.depth);
    return nodes;
  };
};

/**
 * Draw the text, cut it in half every time it doesn't fit until it fits or
 * it's smaller than the "..." text.
 *
 * @param  {CanvasRenderingContext2D} ctx
 * @param  {Number} x
 *         the position of the text
 * @param  {Number} y
 *         the position of the text
 * @param  {Number} innerWidth
 *         the inner width of the containing treemap cell
 * @param  {Text} name
 */
const drawTruncatedName = exports.drawTruncatedName = function(ctx, x, y,
                                                               innerWidth,
                                                               name) {
  const truncated = name.substr(0, Math.floor(name.length / 2));
  const formatted = truncated + ELLIPSIS;

  if (ctx.measureText(formatted).width > innerWidth) {
    drawTruncatedName(ctx, x, y, innerWidth, truncated);
  } else {
    ctx.fillText(formatted, x, y);
  }
};

/**
 * Fit and draw the text in a node with the following strategies to shrink
 * down the text size:
 *
 * Function 608KB 9083 count
 * Function
 * Func...
 * Fu...
 * ...
 *
 * @param  {CanvasRenderingContext2D} ctx
 * @param  {Object} node
 * @param  {Number} borderWidth
 * @param  {Object} dragZoom
 * @param  {Array}  padding
 */
const drawText = exports.drawText = function(ctx, node, borderWidth, ratio,
                                              dragZoom, padding) {
  let { dx, dy, name, totalBytes, totalCount } = node;
  const scale = dragZoom.zoom + 1;
  dx *= scale;
  dy *= scale;

  // Start checking to see how much text we can fit in, optimizing for the
  // common case of lots of small leaf nodes
  if (FONT_SIZE * FONT_LINE_HEIGHT < dy) {
    const margin = borderWidth(node) * 1.5 + ratio * TEXT_MARGIN;
    const x = margin + (node.x - padding[0]) * scale - dragZoom.offsetX;
    const y = margin + (node.y - padding[1]) * scale - dragZoom.offsetY;
    const innerWidth = dx - margin * 2;
    const nameSize = ctx.measureText(name).width;

    if (ctx.measureText(ELLIPSIS).width > innerWidth) {
      return;
    }

    ctx.fillStyle = TEXT_COLOR;

    if (nameSize > innerWidth) {
      // The name is too long - halve the name as an expediant way to shorten it
      drawTruncatedName(ctx, x, y, innerWidth, name);
    } else {
      const bytesFormatted = formatAbbreviatedBytes(totalBytes);
      const countFormatted = `${totalCount} ${COUNT_LABEL}`;
      const byteSize = ctx.measureText(bytesFormatted).width;
      const countSize = ctx.measureText(countFormatted).width;
      const spaceSize = ctx.measureText(" ").width;

      if (nameSize + byteSize + countSize + spaceSize * 3 > innerWidth) {
        // The full name will fit
        ctx.fillText(`${name}`, x, y);
      } else {
        // The full name plus the byte information will fit
        ctx.fillText(name, x, y);
        ctx.fillStyle = TEXT_LIGHT_COLOR;
        ctx.fillText(`${bytesFormatted} ${countFormatted}`,
          x + nameSize + spaceSize, y);
      }
    }
  }
};

/**
 * Draw a box given a node
 *
 * @param  {CanvasRenderingContext2D} ctx
 * @param  {Object} node
 * @param  {Number} borderWidth
 * @param  {Number} ratio
 * @param  {Object} dragZoom
 * @param  {Array}  padding
 */
const drawBox = exports.drawBox = function(ctx, node, borderWidth, dragZoom,
                                           padding) {
  const border = borderWidth(node);
  const fillHSL = colorCoarseType(node);
  const strokeHSL = [fillHSL[0], fillHSL[1], fillHSL[2] * 0.5];
  const scale = 1 + dragZoom.zoom;

  // Offset the draw so that box strokes don't overlap
  const x = scale * (node.x - padding[0]) - dragZoom.offsetX + border / 2;
  const y = scale * (node.y - padding[1]) - dragZoom.offsetY + border / 2;
  const dx = scale * node.dx - border;
  const dy = scale * node.dy - border;

  ctx.fillStyle = hslToStyle(...fillHSL);
  ctx.fillRect(x, y, dx, dy);

  ctx.strokeStyle = hslToStyle(...strokeHSL);
  ctx.lineWidth = border;
  ctx.strokeRect(x, y, dx, dy);
};

/**
 * Draw the overall treemap
 *
 * @param  {HTMLCanvasElement} canvas
 * @param  {CanvasRenderingContext2D} ctx
 * @param  {Array} nodes
 * @param  {Objbect} dragZoom
 */
const drawTreemap = exports.drawTreemap = function({canvas, ctx}, nodes,
                                                   dragZoom) {
  const window = canvas.ownerDocument.defaultView;
  const ratio = window.devicePixelRatio;
  const canvasArea = canvas.width * canvas.height;
  // Subtract the outer padding from the tree map layout.
  const padding = [PADDING[3] * ratio, PADDING[0] * ratio];

  ctx.clearRect(0, 0, canvas.width, canvas.height);
  ctx.font = `${FONT_SIZE * ratio}px sans-serif`;
  ctx.textBaseline = "top";

  function borderWidth(node) {
    const areaRatio = Math.sqrt(node.area / canvasArea);
    return ratio * Math.max(1, LINE_WIDTH * areaRatio);
  }

  for (let i = 0; i < nodes.length; i++) {
    const node = nodes[i];
    if (node.parent === undefined) {
      continue;
    }

    drawBox(ctx, node, borderWidth, dragZoom, padding);
    drawText(ctx, node, borderWidth, ratio, dragZoom, padding);
  }
};

/**
 * Set the position of the zoomed in canvas. It always take up 100% of the view
 * window, but is transformed relative to the zoomed in containing element,
 * essentially reversing the transform of the containing element.
 *
 * @param  {HTMLCanvasElement} canvas
 * @param  {Object} dragZoom
 */
const positionZoomedCanvas = function(canvas, dragZoom) {
  const scale = 1 / (1 + dragZoom.zoom);
  const x = -dragZoom.translateX;
  const y = -dragZoom.translateY;
  canvas.style.transform = `scale(${scale}) translate(${x}px, ${y}px)`;
};

exports.positionZoomedCanvas = positionZoomedCanvas;
