/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { AutoRefreshHighlighter } = require("./auto-refresh");
const {
  CanvasFrameAnonymousContentHelper,
  createNode,
  createSVGNode,
  moveInfobar,
} = require("./utils/markup");
const {
  getCurrentZoom,
  getDisplayPixelRatio,
  setIgnoreLayoutChanges,
  getViewportDimensions,
} = require("devtools/shared/layout/utils");
const {
  identity,
  apply,
  translate,
  multiply,
  scale,
  isIdentity,
  getNodeTransformationMatrix,
} = require("devtools/shared/layout/dom-matrix-2d");
const { stringifyGridFragments } = require("devtools/server/actors/utils/css-grid-utils");
const { LocalizationHelper } = require("devtools/shared/l10n");

const LAYOUT_STRINGS_URI = "devtools/client/locales/layout.properties";
const LAYOUT_L10N = new LocalizationHelper(LAYOUT_STRINGS_URI);

const CSS_GRID_ENABLED_PREF = "layout.css.grid.enabled";
const NEGATIVE_LINE_NUMBERS_PREF = "devtools.gridinspector.showNegativeLineNumbers";

const DEFAULT_GRID_COLOR = "#4B0082";

const COLUMNS = "cols";
const ROWS = "rows";

const GRID_FONT_SIZE = 10;
const GRID_FONT_FAMILY = "sans-serif";
const GRID_AREA_NAME_FONT_SIZE = "20";

const GRID_LINES_PROPERTIES = {
  "edge": {
    lineDash: [0, 0],
    alpha: 1,
  },
  "explicit": {
    lineDash: [5, 3],
    alpha: 0.75,
  },
  "implicit": {
    lineDash: [2, 2],
    alpha: 0.5,
  },
  "areaEdge": {
    lineDash: [0, 0],
    alpha: 1,
    lineWidth: 3,
  }
};

const GRID_GAP_PATTERN_WIDTH = 14; // px
const GRID_GAP_PATTERN_HEIGHT = 14; // px
const GRID_GAP_PATTERN_LINE_DASH = [5, 3]; // px
const GRID_GAP_ALPHA = 0.5;

/**
 * Cached used by `CssGridHighlighter.getGridGapPattern`.
 */
const gCachedGridPattern = new Map();

// We create a <canvas> element that has always 4096x4096 physical pixels, to displays
// our grid's overlay.
// Then, we move the element around when needed, to give the perception that it always
// covers the screen (See bug 1345434).
//
// This canvas size value is the safest we can use because most GPUs can handle it.
// It's also far from the maximum canvas memory allocation limit (4096x4096x4 is
// 67.108.864 bytes, where the limit is 500.000.000 bytes, see:
// http://searchfox.org/mozilla-central/source/gfx/thebes/gfxPrefs.h#401).
//
// Note:
// Once bug 1232491 lands, we could try to refactor this code to use the values from
// the displayport API instead.
//
// Using a fixed value should also solve bug 1348293.
const CANVAS_SIZE = 4096;

/**
 * Returns an array containing the four coordinates of a rectangle, given its diagonal
 * as input; optionally applying a matrix, and a function to each of the coordinates'
 * value.
 *
 * @param  {Number} x1
 *         The x-axis coordinate of the rectangle's diagonal start point.
 * @param  {Number} y1
 *         The y-axis coordinate of the rectangle's diagonal start point.
 * @param  {Number} x2
 *         The x-axis coordinate of the rectangle's diagonal end point.
 * @param  {Number} y2
 *         The y-axis coordinate of the rectangle's diagonal end point.
 * @param  {Array} [matrix=identity()]
 *         A transformation matrix to apply.
 * @return {Array}
 *         The rect four corners' points transformed by the matrix given.
 */
function getPointsFromDiagonal(x1, y1, x2, y2, matrix = identity()) {
  return [
    [x1, y1],
    [x2, y1],
    [x2, y2],
    [x1, y2]
  ].map(point => {
    let transformedPoint = apply(matrix, point);

    return {x: transformedPoint[0], y: transformedPoint[1]};
  });
}

/**
 * Takes an array of four points and returns a DOMRect-like object, represent the
 * boundaries defined by the points given.
 *
 * @param  {Array} points
 *         The four points.
 * @return {Object}
 *         A DOMRect-like object.
 */
function getBoundsFromPoints(points) {
  let bounds = {};

  bounds.left = Math.min(points[0].x, points[1].x, points[2].x, points[3].x);
  bounds.right = Math.max(points[0].x, points[1].x, points[2].x, points[3].x);
  bounds.top = Math.min(points[0].y, points[1].y, points[2].y, points[3].y);
  bounds.bottom = Math.max(points[0].y, points[1].y, points[2].y, points[3].y);

  bounds.x = bounds.left;
  bounds.y = bounds.top;
  bounds.width = bounds.right - bounds.left;
  bounds.height = bounds.bottom - bounds.top;

  return bounds;
}

/**
 * Takes an array of four points and returns a string represent a path description.
 *
 * @param  {Array} points
 *         The four points.
 * @return {String}
 *         A Path Description that can be used in svg's <path> element.
 */
function getPathDescriptionFromPoints(points) {
  return "M" + points[0].x + "," + points[0].y + " " +
         "L" + points[1].x + "," + points[1].y + " " +
         "L" + points[2].x + "," + points[2].y + " " +
         "L" + points[3].x + "," + points[3].y;
}

/**
 * Draws a line to the context given, applying a transformation matrix if passed.
 *
 * @param  {CanvasRenderingContext2D} ctx
 *         The 2d canvas context.
 * @param  {Number} x1
 *         The x-axis of the coordinate for the begin of the line.
 * @param  {Number} y1
 *         The y-axis of the coordinate for the begin of the line.
 * @param  {Number} x2
 *         The x-axis of the coordinate for the end of the line.
 * @param  {Number} y2
 *         The y-axis of the coordinate for the end of the line.
 * @param  {Object} [options]
 *         The options object.
 * @param  {Array} [options.matrix=identity()]
 *         The transformation matrix to apply.
 * @param  {Array} [options.extendToBoundaries]
 *         If set, the line will be extended to reach the boundaries specified.
 */
function drawLine(ctx, x1, y1, x2, y2, options) {
  let matrix = options.matrix || identity();

  let p1 = apply(matrix, [x1, y1]);
  let p2 = apply(matrix, [x2, y2]);

  x1 = p1[0];
  y1 = p1[1];
  x2 = p2[0];
  y2 = p2[1];

  if (options.extendToBoundaries) {
    if (p1[1] === p2[1]) {
      x1 = options.extendToBoundaries[0];
      x2 = options.extendToBoundaries[2];
    } else {
      y1 = options.extendToBoundaries[1];
      x1 = (p2[0] - p1[0]) * (y1 - p1[1]) / (p2[1] - p1[1]) + p1[0];
      y2 = options.extendToBoundaries[3];
      x2 = (p2[0] - p1[0]) * (y2 - p1[1]) / (p2[1] - p1[1]) + p1[0];
    }
  }

  ctx.moveTo(Math.round(x1), Math.round(y1));
  ctx.lineTo(Math.round(x2), Math.round(y2));
}

/**
 * Draws a rect to the context given, applying a transformation matrix if passed.
 * The coordinates are the start and end points of the rectangle's diagonal.
 *
 * @param  {CanvasRenderingContext2D} ctx
 *         The 2d canvas context.
 * @param  {Number} x1
 *         The x-axis coordinate of the rectangle's diagonal start point.
 * @param  {Number} y1
 *         The y-axis coordinate of the rectangle's diagonal start point.
 * @param  {Number} x2
 *         The x-axis coordinate of the rectangle's diagonal end point.
 * @param  {Number} y2
 *         The y-axis coordinate of the rectangle's diagonal end point.
 * @param  {Array} [matrix=identity()]
 *         The transformation matrix to apply.
 */
function drawRect(ctx, x1, y1, x2, y2, matrix = identity()) {
  let p = getPointsFromDiagonal(x1, y1, x2, y2, matrix);

  ctx.beginPath();
  ctx.moveTo(Math.round(p[0].x), Math.round(p[0].y));
  ctx.lineTo(Math.round(p[1].x), Math.round(p[1].y));
  ctx.lineTo(Math.round(p[2].x), Math.round(p[2].y));
  ctx.lineTo(Math.round(p[3].x), Math.round(p[3].y));
  ctx.closePath();
}

/**
 * Utility method to draw a rounded rectangle in the provided canvas context.
 *
 * @param  {CanvasRenderingContext2D} ctx
 *         The 2d canvas context.
 * @param  {Number} x
 *         The x-axis origin of the rectangle.
 * @param  {Number} y
 *         The y-axis origin of the rectangle.
 * @param  {Number} width
 *         The width of the rectangle.
 * @param  {Number} height
 *         The height of the rectangle.
 * @param  {Number} radius
 *         The radius of the rounding.
 */
function drawRoundedRect(ctx, x, y, width, height, radius) {
  ctx.beginPath();
  ctx.moveTo(x, y + radius);
  ctx.lineTo(x, y + height - radius);
  ctx.arcTo(x, y + height, x + radius, y + height, radius);
  ctx.lineTo(x + width - radius, y + height);
  ctx.arcTo(x + width, y + height, x + width, y + height - radius, radius);
  ctx.lineTo(x + width, y + radius);
  ctx.arcTo(x + width, y, x + width - radius, y, radius);
  ctx.lineTo(x + radius, y);
  ctx.arcTo(x, y, x, y + radius, radius);
  ctx.stroke();
  ctx.fill();
}

/**
 * The CssGridHighlighter is the class that overlays a visual grid on top of
 * display:[inline-]grid elements.
 *
 * Usage example:
 * let h = new CssGridHighlighter(env);
 * h.show(node, options);
 * h.hide();
 * h.destroy();
 *
 * Available Options:
 * - color(colorValue)
 *     @param  {String} colorValue
 *     The color that should be used to draw the highlighter for this grid.
 * - showAllGridAreas(isShown)
 *     @param  {Boolean} isShown
 *     Shows all the grid area highlights for the current grid if isShown is true.
 * - showGridArea(areaName)
 *     @param  {String} areaName
 *     Shows the grid area highlight for the given area name.
 * - showGridAreasOverlay(isShown)
 *     @param  {Boolean} isShown
 *     Displays an overlay of all the grid areas for the current grid container if
 *     isShown is true.
 * - showGridCell({ gridFragmentIndex: Number, rowNumber: Number, columnNumber: Number })
 *     @param  {Object} { gridFragmentIndex: Number, rowNumber: Number,
 *                        columnNumber: Number }
 *     An object containing the grid fragment index, row and column numbers to the
 *     corresponding grid cell to highlight for the current grid.
 * - showGridLineNames({ gridFragmentIndex: Number, lineNumber: Number,
 *                       type: String })
 *     @param  {Object} { gridFragmentIndex: Number, lineNumber: Number }
 *     An object containing the grid fragment index and line number to the
 *     corresponding grid line to highlight for the current grid.
 * - showGridLineNumbers(isShown)
 *     @param  {Boolean} isShown
 *     Displays the grid line numbers on the grid lines if isShown is true.
 * - showInfiniteLines(isShown)
 *     @param  {Boolean} isShown
 *     Displays an infinite line to represent the grid lines if isShown is true.
 *
 * Structure:
 * <div class="highlighter-container">
 *   <canvas id="css-grid-canvas" class="css-grid-canvas">
 *   <svg class="css-grid-elements" hidden="true">
 *     <g class="css-grid-regions">
 *       <path class="css-grid-areas" points="..." />
 *       <path class="css-grid-cells" points="..." />
 *     </g>
 *   </svg>
 *   <div class="css-grid-area-infobar-container">
 *     <div class="css-grid-infobar">
 *       <div class="css-grid-infobar-text">
 *         <span class="css-grid-area-infobar-name">Grid Area Name</span>
 *         <span class="css-grid-area-infobar-dimensions">Grid Area Dimensions></span>
 *       </div>
 *     </div>
 *   </div>
 *   <div class="css-grid-cell-infobar-container">
 *     <div class="css-grid-infobar">
 *       <div class="css-grid-infobar-text">
 *         <span class="css-grid-cell-infobar-position">Grid Cell Position</span>
 *         <span class="css-grid-cell-infobar-dimensions">Grid Cell Dimensions></span>
 *       </div>
 *     </div>
 *   <div class="css-grid-line-infobar-container">
 *     <div class="css-grid-infobar">
 *       <div class="css-grid-infobar-text">
 *         <span class="css-grid-line-infobar-number">Grid Line Number</span>
 *         <span class="css-grid-line-infobar-names">Grid Line Names></span>
 *       </div>
 *     </div>
 *   </div>
 * </div>
 */
class CssGridHighlighter extends AutoRefreshHighlighter {
  constructor(highlighterEnv) {
    super(highlighterEnv);

    this.ID_CLASS_PREFIX = "css-grid-";

    this.markup = new CanvasFrameAnonymousContentHelper(this.highlighterEnv,
      this._buildMarkup.bind(this));

    this.onPageHide = this.onPageHide.bind(this);
    this.onWillNavigate = this.onWillNavigate.bind(this);

    this.highlighterEnv.on("will-navigate", this.onWillNavigate);

    let { pageListenerTarget } = highlighterEnv;
    pageListenerTarget.addEventListener("pagehide", this.onPageHide);

    // Initialize the <canvas> position to the top left corner of the page
    this._canvasPosition = {
      x: 0,
      y: 0
    };

    // Calling `calculateCanvasPosition` anyway since the highlighter could be initialized
    // on a page that has scrolled already.
    this.calculateCanvasPosition();
  }

  _buildMarkup() {
    let container = createNode(this.win, {
      attributes: {
        "class": "highlighter-container"
      }
    });

    let root = createNode(this.win, {
      parent: container,
      attributes: {
        "id": "root",
        "class": "root"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // We use a <canvas> element so that we can draw an arbitrary number of lines
    // which wouldn't be possible with HTML or SVG without having to insert and remove
    // the whole markup on every update.
    createNode(this.win, {
      parent: root,
      nodeType: "canvas",
      attributes: {
        "id": "canvas",
        "class": "canvas",
        "hidden": "true",
        "width": CANVAS_SIZE,
        "height": CANVAS_SIZE
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // Build the SVG element
    let svg = createSVGNode(this.win, {
      nodeType: "svg",
      parent: root,
      attributes: {
        "id": "elements",
        "width": "100%",
        "height": "100%",
        "hidden": "true"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    let regions = createSVGNode(this.win, {
      nodeType: "g",
      parent: svg,
      attributes: {
        "class": "regions"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    createSVGNode(this.win, {
      nodeType: "path",
      parent: regions,
      attributes: {
        "class": "areas",
        "id": "areas"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    createSVGNode(this.win, {
      nodeType: "path",
      parent: regions,
      attributes: {
        "class": "cells",
        "id": "cells"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // Building the grid area infobar markup
    let areaInfobarContainer = createNode(this.win, {
      parent: container,
      attributes: {
        "class": "area-infobar-container",
        "id": "area-infobar-container",
        "position": "top",
        "hidden": "true"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    let areaInfobar = createNode(this.win, {
      parent: areaInfobarContainer,
      attributes: {
        "class": "infobar"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    let areaTextbox = createNode(this.win, {
      parent: areaInfobar,
      attributes: {
        "class": "infobar-text"
      },
      prefix: this.ID_CLASS_PREFIX
    });
    createNode(this.win, {
      nodeType: "span",
      parent: areaTextbox,
      attributes: {
        "class": "area-infobar-name",
        "id": "area-infobar-name"
      },
      prefix: this.ID_CLASS_PREFIX
    });
    createNode(this.win, {
      nodeType: "span",
      parent: areaTextbox,
      attributes: {
        "class": "area-infobar-dimensions",
        "id": "area-infobar-dimensions"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // Building the grid cell infobar markup
    let cellInfobarContainer = createNode(this.win, {
      parent: container,
      attributes: {
        "class": "cell-infobar-container",
        "id": "cell-infobar-container",
        "position": "top",
        "hidden": "true"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    let cellInfobar = createNode(this.win, {
      parent: cellInfobarContainer,
      attributes: {
        "class": "infobar"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    let cellTextbox = createNode(this.win, {
      parent: cellInfobar,
      attributes: {
        "class": "infobar-text"
      },
      prefix: this.ID_CLASS_PREFIX
    });
    createNode(this.win, {
      nodeType: "span",
      parent: cellTextbox,
      attributes: {
        "class": "cell-infobar-position",
        "id": "cell-infobar-position"
      },
      prefix: this.ID_CLASS_PREFIX
    });
    createNode(this.win, {
      nodeType: "span",
      parent: cellTextbox,
      attributes: {
        "class": "cell-infobar-dimensions",
        "id": "cell-infobar-dimensions"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // Building the grid line infobar markup
    let lineInfobarContainer = createNode(this.win, {
      parent: container,
      attributes: {
        "class": "line-infobar-container",
        "id": "line-infobar-container",
        "position": "top",
        "hidden": "true"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    let lineInfobar = createNode(this.win, {
      parent: lineInfobarContainer,
      attributes: {
        "class": "infobar"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    let lineTextbox = createNode(this.win, {
      parent: lineInfobar,
      attributes: {
        "class": "infobar-text"
      },
      prefix: this.ID_CLASS_PREFIX
    });
    createNode(this.win, {
      nodeType: "span",
      parent: lineTextbox,
      attributes: {
        "class": "line-infobar-number",
        "id": "line-infobar-number"
      },
      prefix: this.ID_CLASS_PREFIX
    });
    createNode(this.win, {
      nodeType: "span",
      parent: lineTextbox,
      attributes: {
        "class": "line-infobar-names",
        "id": "line-infobar-names"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    return container;
  }

  destroy() {
    let { highlighterEnv } = this;
    highlighterEnv.off("will-navigate", this.onWillNavigate);

    let { pageListenerTarget } = highlighterEnv;
    if (pageListenerTarget) {
      pageListenerTarget.removeEventListener("pagehide", this.onPageHide);
    }

    this.markup.destroy();

    // Clear the pattern cache to avoid dead object exceptions (Bug 1342051).
    this._clearCache();
    AutoRefreshHighlighter.prototype.destroy.call(this);
  }

  getElement(id) {
    return this.markup.getElement(this.ID_CLASS_PREFIX + id);
  }

  get ctx() {
    return this.canvas.getCanvasContext("2d");
  }

  get canvas() {
    return this.getElement("canvas");
  }

  get color() {
    return this.options.color || DEFAULT_GRID_COLOR;
  }

  /**
   * Gets the grid gap pattern used to render the gap regions based on the device
   * pixel ratio given.
   *
   * @param  {Number} devicePixelRatio
   *         The device pixel ratio we want the pattern for.
   * @param  {Object} dimension
   *         Refers to the Map key for the grid dimension type which is either the
   *         constant COLUMNS or ROWS.
   * @return {CanvasPattern} grid gap pattern.
   */
  getGridGapPattern(devicePixelRatio, dimension) {
    let gridPatternMap = null;

    if (gCachedGridPattern.has(devicePixelRatio)) {
      gridPatternMap = gCachedGridPattern.get(devicePixelRatio);
    } else {
      gridPatternMap = new Map();
    }

    if (gridPatternMap.has(dimension)) {
      return gridPatternMap.get(dimension);
    }

    // Create the diagonal lines pattern for the rendering the grid gaps.
    let canvas = createNode(this.win, { nodeType: "canvas" });
    let width = canvas.width = GRID_GAP_PATTERN_WIDTH * devicePixelRatio;
    let height = canvas.height = GRID_GAP_PATTERN_HEIGHT * devicePixelRatio;

    let ctx = canvas.getContext("2d");
    ctx.save();
    ctx.setLineDash(GRID_GAP_PATTERN_LINE_DASH);
    ctx.beginPath();
    ctx.translate(.5, .5);

    if (dimension === COLUMNS) {
      ctx.moveTo(0, 0);
      ctx.lineTo(width, height);
    } else {
      ctx.moveTo(width, 0);
      ctx.lineTo(0, height);
    }

    ctx.strokeStyle = this.color;
    ctx.globalAlpha = GRID_GAP_ALPHA;
    ctx.stroke();
    ctx.restore();

    let pattern = ctx.createPattern(canvas, "repeat");

    gridPatternMap.set(dimension, pattern);
    gCachedGridPattern.set(devicePixelRatio, gridPatternMap);

    return pattern;
  }

  onPageHide({ target }) {
    // If a page hide event is triggered for current window's highlighter, hide the
    // highlighter.
    if (target.defaultView === this.win) {
      this.hide();
    }
  }

  /**
   * Called when the page will-navigate. Used to hide the grid highlighter and clear
   * the cached gap patterns and avoid using DeadWrapper obejcts as gap patterns the
   * next time.
   */
  onWillNavigate({ isTopLevel }) {
    this._clearCache();

    if (isTopLevel) {
      this.hide();
    }
  }

  _show() {
    if (Services.prefs.getBoolPref(CSS_GRID_ENABLED_PREF) && !this.isGrid()) {
      this.hide();
      return false;
    }

    // The grid pattern cache should be cleared in case the color changed.
    this._clearCache();

    // Hide the canvas, grid element highlights and infobar.
    this._hide();

    return this._update();
  }

  _clearCache() {
    gCachedGridPattern.clear();
  }

  /**
   * Shows the grid area highlight for the given area name.
   *
   * @param  {String} areaName
   *         Grid area name.
   */
  showGridArea(areaName) {
    this.renderGridArea(areaName);
  }

  /**
   * Shows all the grid area highlights for the current grid.
   */
  showAllGridAreas() {
    this.renderGridArea();
  }

  /**
   * Clear the grid area highlights.
   */
  clearGridAreas() {
    let areas = this.getElement("areas");
    areas.setAttribute("d", "");
  }

  /**
   * Shows the grid cell highlight for the given grid cell options.
   *
   * @param  {Number} options.gridFragmentIndex
   *         Index of the grid fragment to render the grid cell highlight.
   * @param  {Number} options.rowNumber
   *         Row number of the grid cell to highlight.
   * @param  {Number} options.columnNumber
   *         Column number of the grid cell to highlight.
   */
  showGridCell({ gridFragmentIndex, rowNumber, columnNumber }) {
    this.renderGridCell(gridFragmentIndex, rowNumber, columnNumber);
  }

  /**
   * Shows the grid line highlight for the given grid line options.
   *
   * @param  {Number} options.gridFragmentIndex
   *         Index of the grid fragment to render the grid line highlight.
   * @param  {Number} options.lineNumber
   *         Line number of the grid line to highlight.
   * @param  {String} options.type
   *         The dimension type of the grid line.
   */
  showGridLineNames({ gridFragmentIndex, lineNumber, type }) {
    this.renderGridLineNames(gridFragmentIndex, lineNumber, type);
  }

  /**
   * Clear the grid cell highlights.
   */
  clearGridCell() {
    let cells = this.getElement("cells");
    cells.setAttribute("d", "");
  }

  /**
   * Checks if the current node has a CSS Grid layout.
   *
   * @return {Boolean} true if the current node has a CSS grid layout, false otherwise.
   */
  isGrid() {
    return this.currentNode.getGridFragments().length > 0;
  }

  /**
   * Is a given grid fragment valid? i.e. does it actually have tracks? In some cases, we
   * may have a fragment that defines column tracks but doesn't have any rows (or vice
   * versa). In which case we do not want to draw anything for that fragment.
   *
   * @param {Object} fragment
   * @return {Boolean}
   */
  isValidFragment(fragment) {
    return fragment.cols.tracks.length && fragment.rows.tracks.length;
  }

  /**
   * The AutoRefreshHighlighter's _hasMoved method returns true only if the
   * element's quads have changed. Override it so it also returns true if the
   * element's grid has changed (which can happen when you change the
   * grid-template-* CSS properties with the highlighter displayed).
   */
  _hasMoved() {
    let hasMoved = AutoRefreshHighlighter.prototype._hasMoved.call(this);

    let oldGridData = stringifyGridFragments(this.gridData);
    this.gridData = this.currentNode.getGridFragments();
    let newGridData = stringifyGridFragments(this.gridData);

    return hasMoved || oldGridData !== newGridData;
  }

  /**
   * Update the highlighter on the current highlighted node (the one that was
   * passed as an argument to show(node)).
   * Should be called whenever node's geometry or grid changes.
   */
  _update() {
    setIgnoreLayoutChanges(true);

    let root = this.getElement("root");
    let cells = this.getElement("cells");
    let areas = this.getElement("areas");

    // Hide the root element and force the reflow in order to get the proper window's
    // dimensions without increasing them.
    root.setAttribute("style", "display: none");
    this.win.document.documentElement.offsetWidth;

    // Set the grid cells and areas fill to the current grid colour.
    cells.setAttribute("style", `fill: ${this.color}`);
    areas.setAttribute("style", `fill: ${this.color}`);

    let { width, height } = this._winDimensions;

    // Updates the <canvas> element's position and size.
    // It also clear the <canvas>'s drawing context.
    this.updateCanvasElement();

    // Clear the grid area highlights.
    this.clearGridAreas();
    this.clearGridCell();

    // Update the current matrix used in our canvas' rendering
    this.updateCurrentMatrix();

    // Start drawing the grid fragments.
    for (let i = 0; i < this.gridData.length; i++) {
      this.renderFragment(this.gridData[i]);
    }

    // Display the grid area highlights if needed.
    if (this.options.showAllGridAreas) {
      this.showAllGridAreas();
    } else if (this.options.showGridArea) {
      this.showGridArea(this.options.showGridArea);
    }

    // Display the grid cell highlights if needed.
    if (this.options.showGridCell) {
      this.showGridCell(this.options.showGridCell);
    }

    // Display the grid line names if needed.
    if (this.options.showGridLineNames) {
      this.showGridLineNames(this.options.showGridLineNames);
    }

    this._showGrid();
    this._showGridElements();

    root.setAttribute("style",
      `position:absolute; width:${width}px;height:${height}px; overflow:hidden`);

    setIgnoreLayoutChanges(false, this.highlighterEnv.document.documentElement);
    return true;
  }

  /**
   * Update the grid information displayed in the grid area info bar.
   *
   * @param  {GridArea} area
   *         The grid area object.
   * @param  {Object} bounds
   *         A DOMRect-like object represent the grid area rectangle.
   */
  _updateGridAreaInfobar(area, bounds) {
    let { width, height } = bounds;
    let dim = parseFloat(width.toPrecision(6)) +
              " \u00D7 " +
              parseFloat(height.toPrecision(6));

    this.getElement("area-infobar-name").setTextContent(area.name);
    this.getElement("area-infobar-dimensions").setTextContent(dim);

    let container = this.getElement("area-infobar-container");
    moveInfobar(container, bounds, this.win, {
      position: "bottom",
      hideIfOffscreen: true
    });
  }

  /**
   * Update the grid information displayed in the grid cell info bar.
   *
   * @param  {Number} rowNumber
   *         The grid cell's row number.
   * @param  {Number} columnNumber
   *         The grid cell's column number.
   * @param  {Object} bounds
   *         A DOMRect-like object represent the grid cell rectangle.
   */
  _updateGridCellInfobar(rowNumber, columnNumber, bounds) {
    let { width, height } = bounds;
    let dim = parseFloat(width.toPrecision(6)) +
              " \u00D7 " +
              parseFloat(height.toPrecision(6));
    let position = LAYOUT_L10N.getFormatStr("layout.rowColumnPositions",
                   rowNumber, columnNumber);

    this.getElement("cell-infobar-position").setTextContent(position);
    this.getElement("cell-infobar-dimensions").setTextContent(dim);

    let container = this.getElement("cell-infobar-container");
    moveInfobar(container, bounds, this.win, {
      position: "top",
      hideIfOffscreen: true
    });
  }

  /**
   * Update the grid information displayed in the grid line info bar.
   *
   * @param  {String} gridLineNames
   *         Comma-separated string of names for the grid line.
   * @param  {Number} gridLineNumber
   *         The grid line number.
   * @param  {Number} x
   *         The x-coordinate of the grid line.
   * @param  {Number} y
   *         The y-coordinate of the grid line.
   */
  _updateGridLineInfobar(gridLineNames, gridLineNumber, x, y) {
    this.getElement("line-infobar-number").setTextContent(gridLineNumber);
    this.getElement("line-infobar-names").setTextContent(gridLineNames);

    let container = this.getElement("line-infobar-container");
    moveInfobar(container,
      getBoundsFromPoints([{x, y}, {x, y}, {x, y}, {x, y}]), this.win);
  }

  /**
   * The <canvas>'s position needs to be updated if the page scrolls too much, in order
   * to give the illusion that it always covers the viewport.
   */
  _scrollUpdate() {
    let hasPositionChanged = this.calculateCanvasPosition();

    if (hasPositionChanged) {
      this._update();
    }
  }

  /**
   * This method is responsible to do the math that updates the <canvas>'s position,
   * in accordance with the page's scroll, document's size, canvas size, and
   * viewport's size.
   * It's called when a page's scroll is detected.
   *
   * @return {Boolean} `true` if the <canvas> position was updated, `false` otherwise.
   */
  calculateCanvasPosition() {
    let cssCanvasSize = CANVAS_SIZE / this.win.devicePixelRatio;
    let viewportSize = getViewportDimensions(this.win);
    let documentSize = this._winDimensions;
    let pageX = this._scroll.x;
    let pageY = this._scroll.y;
    let canvasWidth = cssCanvasSize;
    let canvasHeight = cssCanvasSize;
    let hasUpdated = false;

    // Those values indicates the relative horizontal and vertical space the page can
    // scroll before we have to reposition the <canvas>; they're 1/4 of the delta between
    // the canvas' size and the viewport's size: that's because we want to consider both
    // sides (top/bottom, left/right; so 1/2 for each side) and also we don't want to
    // shown the edges of the canvas in case of fast scrolling (to avoid showing undraw
    // areas, therefore another 1/2 here).
    let bufferSizeX = (canvasWidth - viewportSize.width) >> 2;
    let bufferSizeY = (canvasHeight - viewportSize.height) >> 2;

    let { x, y } = this._canvasPosition;

    // Defines the boundaries for the canvas.
    let topBoundary = 0;
    let bottomBoundary = documentSize.height - canvasHeight;
    let leftBoundary = 0;
    let rightBoundary = documentSize.width - canvasWidth;

    // Defines the thresholds that triggers the canvas' position to be updated.
    let topThreshold = pageY - bufferSizeY;
    let bottomThreshold = pageY - canvasHeight + viewportSize.height + bufferSizeY;
    let leftThreshold = pageX - bufferSizeX;
    let rightThreshold = pageX - canvasWidth + viewportSize.width + bufferSizeX;

    if (y < bottomBoundary && y < bottomThreshold) {
      this._canvasPosition.y = Math.min(topThreshold, bottomBoundary);
      hasUpdated = true;
    } else if (y > topBoundary && y > topThreshold) {
      this._canvasPosition.y = Math.max(bottomThreshold, topBoundary);
      hasUpdated = true;
    }

    if (x < rightBoundary && x < rightThreshold) {
      this._canvasPosition.x = Math.min(leftThreshold, rightBoundary);
      hasUpdated = true;
    } else if (x > leftBoundary && x > leftThreshold) {
      this._canvasPosition.x = Math.max(rightThreshold, leftBoundary);
      hasUpdated = true;
    }

    return hasUpdated;
  }

  /**
   * Updates the <canvas> element's style in accordance with the current window's
   * devicePixelRatio, and the position calculated in `calculateCanvasPosition`; it also
   * clears the drawing context.
   */
  updateCanvasElement() {
    let size = CANVAS_SIZE / this.win.devicePixelRatio;
    let { x, y } = this._canvasPosition;

    // Resize the canvas taking the dpr into account so as to have crisp lines, and
    // translating it to give the perception that it always covers the viewport.
    this.canvas.setAttribute("style",
      `width:${size}px;height:${size}px; transform: translate(${x}px, ${y}px);`);

    this.ctx.clearRect(0, 0, CANVAS_SIZE, CANVAS_SIZE);
  }

  /**
   * Updates the current matrices for both canvas drawing and SVG, taking in account the
   * following transformations, in this order:
   *   1. The scale given by the display pixel ratio.
   *   2. The translation to the top left corner of the element.
   *   3. The scale given by the current zoom.
   *   4. The translation given by the top and left padding of the element.
   *   5. Any CSS transformation applied directly to the element (only 2D
   *      transformation; the 3D transformation are flattened, see `dom-matrix-2d` module
   *      for further details.)
   *
   *  The transformations of the element's ancestors are not currently computed (see
   *  bug 1355675).
   */
  updateCurrentMatrix() {
    let computedStyle = this.currentNode.ownerGlobal.getComputedStyle(this.currentNode);

    let paddingTop = parseFloat(computedStyle.paddingTop);
    let paddingLeft = parseFloat(computedStyle.paddingLeft);
    let borderTop = parseFloat(computedStyle.borderTopWidth);
    let borderLeft = parseFloat(computedStyle.borderLeftWidth);

    let nodeMatrix = getNodeTransformationMatrix(this.currentNode,
      this.win.document.documentElement);

    let m = identity();

    // First, we scale based on the device pixel ratio.
    m = multiply(m, scale(this.win.devicePixelRatio));
    // Then, we apply the current node's transformation matrix, relative to the
    // inspected window's root element, but only if it's not a identity matrix.
    if (isIdentity(nodeMatrix)) {
      this.hasNodeTransformations = false;
    } else {
      m = multiply(m, nodeMatrix);
      this.hasNodeTransformations = true;
    }

    // Finally, we translate the origin based on the node's padding and border values.
    m = multiply(m, translate(paddingLeft + borderLeft, paddingTop + borderTop));

    this.currentMatrix = m;
  }

  getFirstRowLinePos(fragment) {
    return fragment.rows.lines[0].start;
  }

  getLastRowLinePos(fragment) {
    return fragment.rows.lines[fragment.rows.lines.length - 1].start;
  }

  getFirstColLinePos(fragment) {
    return fragment.cols.lines[0].start;
  }

  getLastColLinePos(fragment) {
    return fragment.cols.lines[fragment.cols.lines.length - 1].start;
  }

  /**
   * Get the GridLine index of the last edge of the explicit grid for a grid dimension.
   *
   * @param  {GridTracks} tracks
   *         The grid track of a given grid dimension.
   * @return {Number} index of the last edge of the explicit grid for a grid dimension.
   */
  getLastEdgeLineIndex(tracks) {
    let trackIndex = tracks.length - 1;

    // Traverse the grid track backwards until we find an explicit track.
    while (trackIndex >= 0 && tracks[trackIndex].type != "explicit") {
      trackIndex--;
    }

    // The grid line index is the grid track index + 1.
    return trackIndex + 1;
  }

  renderFragment(fragment) {
    if (!this.isValidFragment(fragment)) {
      return;
    }

    this.renderLines(fragment.cols, COLUMNS, "left", "top", "height",
                     this.getFirstRowLinePos(fragment),
                     this.getLastRowLinePos(fragment));
    this.renderLines(fragment.rows, ROWS, "top", "left", "width",
                     this.getFirstColLinePos(fragment),
                     this.getLastColLinePos(fragment));

    if (this.options.showGridAreasOverlay) {
      this.renderGridAreaOverlay();
    }

    // Line numbers are rendered in a 2nd step to avoid overlapping with existing lines.
    if (this.options.showGridLineNumbers) {
      this.renderLineNumbers(fragment.cols, COLUMNS, "left", "top",
                       this.getFirstRowLinePos(fragment));
      this.renderLineNumbers(fragment.rows, ROWS, "top", "left",
                       this.getFirstColLinePos(fragment));

      if (Services.prefs.getBoolPref(NEGATIVE_LINE_NUMBERS_PREF)) {
        this.renderNegativeLineNumbers(fragment.cols, COLUMNS, "left", "top",
                          this.getLastRowLinePos(fragment));
        this.renderNegativeLineNumbers(fragment.rows, ROWS, "top", "left",
                          this.getLastColLinePos(fragment));
      }
    }
  }

  /**
   * Render the negative grid lines given the grid dimension information of the
   * column or row lines.
   *
   * See @param for renderLines.
   */
  renderNegativeLineNumbers(gridDimension, dimensionType, mainSide, crossSide,
            startPos) {
    let lineStartPos = startPos;

    // Keep track of the number of collapsed lines per line position
    let stackedLines = [];

    const { lines } = gridDimension;

    for (let i = 0, line; (line = lines[i++]);) {
      let linePos = line.start;
      let negativeLineNumber = i - lines.length - 1;

      // Check for overlapping lines. We render a second box beneath the last overlapping
      // line number to indicate there are lines beneath it.
      const gridLine = gridDimension.tracks[line.number - 1];

      if (gridLine) {
        const { breadth }  = gridLine;

        if (breadth === 0) {
          stackedLines.push(negativeLineNumber);

          if (stackedLines.length > 0) {
            this.renderGridLineNumber(negativeLineNumber, linePos, lineStartPos,
              line.breadth, dimensionType, 1);
          }

          continue;
        }
      }

      // For negative line numbers, we want to display the smallest
      // value at the front of the stack.
      if (stackedLines.length) {
        negativeLineNumber = stackedLines[0];
        stackedLines = [];
      }

      this.renderGridLineNumber(negativeLineNumber, linePos, lineStartPos, line.breadth,
        dimensionType);
    }
  }

  /**
   * Renders the grid area overlay on the css grid highlighter canvas.
   */
  renderGridAreaOverlay() {
    let padding = 1;

    for (let i = 0; i < this.gridData.length; i++) {
      let fragment = this.gridData[i];

      for (let area of fragment.areas) {
        let { rowStart, rowEnd, columnStart, columnEnd, type } = area;

        if (type === "implicit") {
          continue;
        }

        // Draw the line edges for the grid area
        const areaColStart = fragment.cols.lines[columnStart - 1];
        const areaColEnd = fragment.cols.lines[columnEnd - 1];

        const areaRowStart = fragment.rows.lines[rowStart - 1];
        const areaRowEnd = fragment.rows.lines[rowEnd - 1];

        const areaColStartLinePos = areaColStart.start + areaColStart.breadth;
        const areaRowStartLinePos = areaRowStart.start + areaRowStart.breadth;

        this.renderLine(areaColStartLinePos + padding,
                        areaRowStartLinePos, areaRowEnd.start,
                        COLUMNS, "areaEdge");
        this.renderLine(areaColEnd.start - padding,
                        areaRowStartLinePos, areaRowEnd.start,
                        COLUMNS, "areaEdge");

        this.renderLine(areaRowStartLinePos + padding,
                        areaColStartLinePos, areaColEnd.start,
                        ROWS, "areaEdge");
        this.renderLine(areaRowEnd.start - padding,
                        areaColStartLinePos, areaColEnd.start,
                        ROWS, "areaEdge");

        this.renderGridAreaName(fragment, area);
      }
    }

    this.ctx.restore();
  }

  /**
   * Render grid area name on the containing grid area cell.
   *
   * @param  {Object} fragment
   *         The grid fragment of the grid container.
   * @param  {Object} area
   *         The area overlay to render on the CSS highlighter canvas.
   */
  renderGridAreaName(fragment, area) {
    let { rowStart, rowEnd, columnStart, columnEnd } = area;
    let { devicePixelRatio } = this.win;
    let displayPixelRatio = getDisplayPixelRatio(this.win);
    let offset = (displayPixelRatio / 2) % 1;
    let fontSize = (GRID_AREA_NAME_FONT_SIZE * displayPixelRatio);

    this.ctx.save();

    let canvasX = Math.round(this._canvasPosition.x * devicePixelRatio);
    let canvasY = Math.round(this._canvasPosition.y * devicePixelRatio);
    this.ctx.translate(offset - canvasX, offset - canvasY);

    this.ctx.font = fontSize + "px " + GRID_FONT_FAMILY;
    this.ctx.strokeStyle = this.color;
    this.ctx.textAlign = "center";
    this.ctx.textBaseline = "middle";

    // Draw the text for the grid area name.
    for (let rowNumber = rowStart; rowNumber < rowEnd; rowNumber++) {
      for (let columnNumber = columnStart; columnNumber < columnEnd; columnNumber++) {
        let row = fragment.rows.tracks[rowNumber - 1];
        let column = fragment.cols.tracks[columnNumber - 1];

        // Check if the font size is exceeds the bounds of the containing grid cell.
        if (fontSize > (column.breadth * displayPixelRatio) ||
            fontSize > (row.breadth * displayPixelRatio)) {
          fontSize = (column.breadth + row.breadth) / 2;
          this.ctx.font = fontSize + "px " + GRID_FONT_FAMILY;
        }

        let textWidth = this.ctx.measureText(area.name).width;

        // The width of the character 'm' approximates the height of the text.
        let textHeight = this.ctx.measureText("m").width;

        // Padding in pixels for the line number text inside of the line number container.
        let padding = 3 * displayPixelRatio;

        let boxWidth = textWidth + 2 * padding;
        let boxHeight = textHeight + 2 * padding;

        let x = column.start + column.breadth / 2;
        let y = row.start + row.breadth / 2;

        [x, y] = apply(this.currentMatrix, [x, y]);

        let rectXPos = x - boxWidth / 2;
        let rectYPos = y - boxHeight / 2;

        // Draw a rounded rectangle with a border width of 1 pixel,
        // a border color matching the grid color, and a white background
        this.ctx.lineWidth = 1 * displayPixelRatio;
        this.ctx.strokeStyle = this.color;
        this.ctx.fillStyle = "white";
        let radius = 2 * displayPixelRatio;
        drawRoundedRect(this.ctx, rectXPos, rectYPos, boxWidth, boxHeight, radius);

        this.ctx.fillStyle = this.color;
        this.ctx.fillText(area.name, x, y + padding);
      }
    }

    this.ctx.restore();
  }

  /**
   * Render the grid lines given the grid dimension information of the
   * column or row lines.
   *
   * @param  {GridDimension} gridDimension
   *         Column or row grid dimension object.
   * @param  {Object} quad.bounds
   *         The content bounds of the box model region quads.
   * @param  {String} dimensionType
   *         The grid dimension type which is either the constant COLUMNS or ROWS.
   * @param  {String} mainSide
   *         The main side of the given grid dimension - "top" for rows and
   *         "left" for columns.
   * @param  {String} crossSide
   *         The cross side of the given grid dimension - "left" for rows and
   *         "top" for columns.
   * @param  {String} mainSize
   *         The main size of the given grid dimension - "width" for rows and
   *         "height" for columns.
   * @param  {Number} startPos
   *         The start position of the cross side of the grid dimension.
   * @param  {Number} endPos
   *         The end position of the cross side of the grid dimension.
   */
  renderLines(gridDimension, dimensionType, mainSide, crossSide,
              mainSize, startPos, endPos) {
    let lineStartPos = startPos;
    let lineEndPos = endPos;

    let lastEdgeLineIndex = this.getLastEdgeLineIndex(gridDimension.tracks);

    for (let i = 0; i < gridDimension.lines.length; i++) {
      let line = gridDimension.lines[i];
      let linePos = line.start;

      if (i == 0 || i == lastEdgeLineIndex) {
        this.renderLine(linePos, lineStartPos, lineEndPos, dimensionType, "edge");
      } else {
        this.renderLine(linePos, lineStartPos, lineEndPos, dimensionType,
                        gridDimension.tracks[i - 1].type);
      }

      // Render a second line to illustrate the gutter for non-zero breadth.
      if (line.breadth > 0) {
        this.renderGridGap(linePos, lineStartPos, lineEndPos, line.breadth,
                           dimensionType);
        this.renderLine(linePos + line.breadth, lineStartPos, lineEndPos, dimensionType,
                        gridDimension.tracks[i].type);
      }
    }
  }

  /**
   * Render the grid lines given the grid dimension information of the
   * column or row lines.
   *
   * see @param for renderLines.
   */
  renderLineNumbers(gridDimension, dimensionType, mainSide, crossSide,
              startPos) {
    let lineStartPos = startPos;

    // Keep track of the number of collapsed lines per line position
    let stackedLines = [];

    const { lines } = gridDimension;

    for (let i = 0, line; (line = lines[i++]);) {
      let linePos = line.start;

      // If you place something using negative numbers, you can trigger some implicit grid
      // creation above and to the left of the explicit grid (assuming a horizontal-tb
      // writing mode).
      // The first explicit grid line gets the number of 1; any implicit grid lines
      // before 1 get negative numbers, but do not get any positivity numbers.
      // Since here we're rendering only the positive line numbers, we have to skip any
      // implicit grid lines before the first tha is explicit.
      // For such lines the API returns always 0 as line's number.
      if (line.number === 0) {
        continue;
      }

      // Check for overlapping lines. We render a second box beneath the last overlapping
      // line number to indicate there are lines beneath it.
      const gridLine = gridDimension.tracks[line.number - 1];

      if (gridLine) {
        const { breadth }  = gridLine;

        if (breadth === 0) {
          stackedLines.push(gridDimension.lines[i].number);

          if (stackedLines.length > 0) {
            this.renderGridLineNumber(line.number, linePos, lineStartPos, line.breadth,
              dimensionType, 1);
          }

          continue;
        }
      }

      this.renderGridLineNumber(line.number, linePos, lineStartPos, line.breadth,
        dimensionType);
    }
  }

  /**
   * Render the grid line on the css grid highlighter canvas.
   *
   * @param  {Number} linePos
   *         The line position along the x-axis for a column grid line and
   *         y-axis for a row grid line.
   * @param  {Number} startPos
   *         The start position of the cross side of the grid line.
   * @param  {Number} endPos
   *         The end position of the cross side of the grid line.
   * @param  {String} dimensionType
   *         The grid dimension type which is either the constant COLUMNS or ROWS.
   * @param  {String} lineType
   *         The grid line type - "edge", "explicit", or "implicit".
   */
  renderLine(linePos, startPos, endPos, dimensionType, lineType) {
    let { devicePixelRatio } = this.win;
    let lineWidth = getDisplayPixelRatio(this.win);
    let offset = (lineWidth / 2) % 1;

    let x = Math.round(this._canvasPosition.x * devicePixelRatio);
    let y = Math.round(this._canvasPosition.y * devicePixelRatio);

    linePos = Math.round(linePos);
    startPos = Math.round(startPos);
    endPos = Math.round(endPos);

    this.ctx.save();
    this.ctx.setLineDash(GRID_LINES_PROPERTIES[lineType].lineDash);
    this.ctx.beginPath();
    this.ctx.translate(offset - x, offset - y);

    let lineOptions = {
      matrix: this.currentMatrix
    };

    if (this.options.showInfiniteLines) {
      lineOptions.extendToBoundaries = [x, y, x + CANVAS_SIZE, y + CANVAS_SIZE];
    }

    if (dimensionType === COLUMNS) {
      drawLine(this.ctx, linePos, startPos, linePos, endPos, lineOptions);
    } else {
      drawLine(this.ctx, startPos, linePos, endPos, linePos, lineOptions);
    }

    this.ctx.strokeStyle = this.color;
    this.ctx.globalAlpha = GRID_LINES_PROPERTIES[lineType].alpha;

    if (GRID_LINES_PROPERTIES[lineType].lineWidth) {
      this.ctx.lineWidth = GRID_LINES_PROPERTIES[lineType].lineWidth * devicePixelRatio;
    } else {
      this.ctx.lineWidth = lineWidth;
    }

    this.ctx.stroke();
    this.ctx.restore();
  }

  /**
   * Render the grid line number on the css grid highlighter canvas.
   *
   * @param  {Number} lineNumber
   *         The grid line number.
   * @param  {Number} linePos
   *         The line position along the x-axis for a column grid line and
   *         y-axis for a row grid line.
   * @param  {Number} startPos
   *         The start position of the cross side of the grid line.
   * @param  {Number} breadth
   *         The grid line breadth value.
   * @param  {String} dimensionType
   *         The grid dimension type which is either the constant COLUMNS or ROWS.
   * @param  {Number||undefined} stackedLineIndex
   *         The line index position of the stacked line.
   */
  renderGridLineNumber(lineNumber, linePos, startPos, breadth, dimensionType,
    stackedLineIndex) {
    let displayPixelRatio = getDisplayPixelRatio(this.win);
    let { devicePixelRatio } = this.win;
    let offset = (displayPixelRatio / 2) % 1;

    linePos = Math.round(linePos);
    startPos = Math.round(startPos);
    breadth = Math.round(breadth);

    if (linePos + breadth < 0) {
      // The line is not visible on screen, don't render the line number
      return;
    }

    this.ctx.save();
    let canvasX = Math.round(this._canvasPosition.x * devicePixelRatio);
    let canvasY = Math.round(this._canvasPosition.y * devicePixelRatio);
    this.ctx.translate(offset - canvasX, offset - canvasY);

    let fontSize = (GRID_FONT_SIZE * displayPixelRatio);
    this.ctx.font = fontSize + "px " + GRID_FONT_FAMILY;

    let textWidth = this.ctx.measureText(lineNumber).width;

    // The width of the character 'm' approximates the height of the text.
    let textHeight = this.ctx.measureText("m").width;

    // Padding in pixels for the line number text inside of the line number container.
    let padding = 3 * displayPixelRatio;

    let boxWidth = textWidth + 2 * padding;
    let boxHeight = textHeight + 2 * padding;

    // Calculate the x & y coordinates for the line number container, so that it is
    // centered on the line, and in the middle of the gap if there is any.
    let x, y;

    let startOffset = (boxHeight + 2) / devicePixelRatio;

    if (Services.prefs.getBoolPref(NEGATIVE_LINE_NUMBERS_PREF)) {
      // If the line number is negative, offset it from the grid container edge,
      // (downwards if its a column, rightwards if its a row).
      if (lineNumber < 0) {
        startPos += startOffset;
      } else {
        startPos -= startOffset;
      }
    }

    if (dimensionType === COLUMNS) {
      x = linePos + breadth / 2;
      y = startPos;
    } else {
      x = startPos;
      y = linePos + breadth / 2;
    }

    [x, y] = apply(this.currentMatrix, [x, y]);

    x -= boxWidth / 2;
    y -= boxHeight / 2;

    if (stackedLineIndex) {
      // Offset the stacked line number by half of the box's width/height
      const xOffset = boxWidth / 4;
      const yOffset = boxHeight / 4;

      x += xOffset;
      y += yOffset;
    }

    if (!this.hasNodeTransformations) {
      x = Math.max(x, padding);
      y = Math.max(y, padding);
    }

    // Draw a rounded rectangle with a border width of 2 pixels, a border color matching
    // the grid color and a white background (the line number will be written in black).
    this.ctx.lineWidth = 2 * displayPixelRatio;
    this.ctx.strokeStyle = this.color;
    this.ctx.fillStyle = "white";
    let radius = 2 * displayPixelRatio;
    drawRoundedRect(this.ctx, x, y, boxWidth, boxHeight, radius);

    // Write the line number inside of the rectangle.
    this.ctx.fillStyle = "black";
    const numberText = stackedLineIndex ? "" : lineNumber;
    this.ctx.fillText(numberText, x + padding, y + textHeight + padding);

    this.ctx.restore();
  }

  /**
   * Render the grid gap area on the css grid highlighter canvas.
   *
   * @param  {Number} linePos
   *         The line position along the x-axis for a column grid line and
   *         y-axis for a row grid line.
   * @param  {Number} startPos
   *         The start position of the cross side of the grid line.
   * @param  {Number} endPos
   *         The end position of the cross side of the grid line.
   * @param  {Number} breadth
   *         The grid line breadth value.
   * @param  {String} dimensionType
   *         The grid dimension type which is either the constant COLUMNS or ROWS.
   */
  renderGridGap(linePos, startPos, endPos, breadth, dimensionType) {
    let { devicePixelRatio } = this.win;
    let displayPixelRatio = getDisplayPixelRatio(this.win);
    let offset = (displayPixelRatio / 2) % 1;

    let canvasX = Math.round(this._canvasPosition.x * devicePixelRatio);
    let canvasY = Math.round(this._canvasPosition.y * devicePixelRatio);

    linePos = Math.round(linePos);
    startPos = Math.round(startPos);
    breadth = Math.round(breadth);

    this.ctx.save();
    this.ctx.fillStyle = this.getGridGapPattern(devicePixelRatio, dimensionType);
    this.ctx.translate(offset - canvasX, offset - canvasY);

    if (dimensionType === COLUMNS) {
      if (isFinite(endPos)) {
        endPos = Math.round(endPos);
      } else {
        endPos = this._winDimensions.height;
        startPos = -endPos;
      }
      drawRect(this.ctx, linePos, startPos, linePos + breadth, endPos,
        this.currentMatrix);
    } else {
      if (isFinite(endPos)) {
        endPos = Math.round(endPos);
      } else {
        endPos = this._winDimensions.width;
        startPos = -endPos;
      }
      drawRect(this.ctx, startPos, linePos, endPos, linePos + breadth,
        this.currentMatrix);
    }
    this.ctx.fill();
    this.ctx.restore();
  }

  /**
   * Render the grid area highlight for the given area name or for all the grid areas.
   *
   * @param  {String} areaName
   *         Name of the grid area to be highlighted. If no area name is provided, all
   *         the grid areas should be highlighted.
   */
  renderGridArea(areaName) {
    let paths = [];
    let { devicePixelRatio } = this.win;
    let displayPixelRatio = getDisplayPixelRatio(this.win);

    for (let i = 0; i < this.gridData.length; i++) {
      let fragment = this.gridData[i];

      for (let area of fragment.areas) {
        if (areaName && areaName != area.name) {
          continue;
        }

        let rowStart = fragment.rows.lines[area.rowStart - 1];
        let rowEnd = fragment.rows.lines[area.rowEnd - 1];
        let columnStart = fragment.cols.lines[area.columnStart - 1];
        let columnEnd = fragment.cols.lines[area.columnEnd - 1];

        let x1 = columnStart.start + columnStart.breadth;
        let y1 = rowStart.start + rowStart.breadth;
        let x2 = columnEnd.start;
        let y2 = rowEnd.start;

        let points = getPointsFromDiagonal(x1, y1, x2, y2, this.currentMatrix);

        // Scale down by `devicePixelRatio` since SVG element already take them into
        // account.
        let svgPoints = points.map(point => ({
          x: Math.round(point.x / devicePixelRatio),
          y: Math.round(point.y / devicePixelRatio)
        }));

        // Scale down by `displayPixelRatio` since infobar's HTML elements already take it
        // into account; and the zoom scaling is handled by `moveInfobar`.
        let bounds = getBoundsFromPoints(points.map(point => ({
          x: Math.round(point.x / displayPixelRatio),
          y: Math.round(point.y / displayPixelRatio)
        })));

        paths.push(getPathDescriptionFromPoints(svgPoints));

        // Update and show the info bar when only displaying a single grid area.
        if (areaName) {
          this._showGridAreaInfoBar();
          this._updateGridAreaInfobar(area, bounds);
        }
      }
    }

    let areas = this.getElement("areas");
    areas.setAttribute("d", paths.join(" "));
  }

  /**
   * Render the grid cell highlight for the given grid fragment index, row and column
   * number.
   *
   * @param  {Number} gridFragmentIndex
   *         Index of the grid fragment to render the grid cell highlight.
   * @param  {Number} rowNumber
   *         Row number of the grid cell to highlight.
   * @param  {Number} columnNumber
   *         Column number of the grid cell to highlight.
   */
  renderGridCell(gridFragmentIndex, rowNumber, columnNumber) {
    let fragment = this.gridData[gridFragmentIndex];

    if (!fragment) {
      return;
    }

    let row = fragment.rows.tracks[rowNumber - 1];
    let column = fragment.cols.tracks[columnNumber - 1];

    if (!row || !column) {
      return;
    }

    let x1 = column.start;
    let y1 = row.start;
    let x2 = column.start + column.breadth;
    let y2 = row.start + row.breadth;

    let { devicePixelRatio } = this.win;
    let displayPixelRatio = getDisplayPixelRatio(this.win);

    let points = getPointsFromDiagonal(x1, y1, x2, y2, this.currentMatrix);

    // Scale down by `devicePixelRatio` since SVG element already take them into account.
    let svgPoints = points.map(point => ({
      x: Math.round(point.x / devicePixelRatio),
      y: Math.round(point.y / devicePixelRatio)
    }));

    // Scale down by `displayPixelRatio` since infobar's HTML elements already take it
    // into account, and the zoom scaling is handled by `moveInfobar`.
    let bounds = getBoundsFromPoints(points.map(point => ({
      x: Math.round(point.x / displayPixelRatio),
      y: Math.round(point.y / displayPixelRatio)
    })));

    let cells = this.getElement("cells");
    cells.setAttribute("d", getPathDescriptionFromPoints(svgPoints));

    this._showGridCellInfoBar();
    this._updateGridCellInfobar(rowNumber, columnNumber, bounds);
  }

  /**
   * Render the grid line name highlight for the given grid fragment index, lineNumber,
   * and dimensionType.
   *
   * @param  {Number} gridFragmentIndex
   *         Index of the grid fragment to render the grid line highlight.
   * @param  {Number} lineNumber
   *         Line number of the grid line to highlight.
   * @param  {String} dimensionType
   *         The dimension type of the grid line.
   */
  renderGridLineNames(gridFragmentIndex, lineNumber, dimensionType) {
    let fragment = this.gridData[gridFragmentIndex];

    if (!fragment || !lineNumber || !dimensionType) {
      return;
    }

    const { names } = fragment[dimensionType].lines[lineNumber - 1];
    let linePos;

    if (dimensionType === ROWS) {
      linePos = fragment.rows.lines[lineNumber - 1];
    } else if (dimensionType === COLUMNS) {
      linePos = fragment.cols.lines[lineNumber - 1];
    }

    if (!linePos) {
      return;
    }

    let currentZoom = getCurrentZoom(this.win);
    let { bounds } = this.currentQuads.content[gridFragmentIndex];

    const rowYPosition = fragment.rows.lines[0];
    const colXPosition = fragment.rows.lines[0];

    let x = dimensionType === COLUMNS
      ? linePos.start + (bounds.left / currentZoom)
      : colXPosition.start + (bounds.left / currentZoom);

    let y = dimensionType === ROWS
      ? linePos.start + (bounds.top / currentZoom)
      : rowYPosition.start + (bounds.top / currentZoom);

    this._showGridLineInfoBar();
    this._updateGridLineInfobar(names.join(", "), lineNumber, x, y);
  }

  /**
   * Hide the highlighter, the canvas and the infobars.
   */
  _hide() {
    setIgnoreLayoutChanges(true);
    this._hideGrid();
    this._hideGridElements();
    this._hideGridAreaInfoBar();
    this._hideGridCellInfoBar();
    this._hideGridLineInfoBar();
    setIgnoreLayoutChanges(false, this.highlighterEnv.document.documentElement);
  }

  _hideGrid() {
    this.getElement("canvas").setAttribute("hidden", "true");
  }

  _showGrid() {
    this.getElement("canvas").removeAttribute("hidden");
  }

  _hideGridElements() {
    this.getElement("elements").setAttribute("hidden", "true");
  }

  _showGridElements() {
    this.getElement("elements").removeAttribute("hidden");
  }

  _hideGridAreaInfoBar() {
    this.getElement("area-infobar-container").setAttribute("hidden", "true");
  }

  _showGridAreaInfoBar() {
    this.getElement("area-infobar-container").removeAttribute("hidden");
  }

  _hideGridCellInfoBar() {
    this.getElement("cell-infobar-container").setAttribute("hidden", "true");
  }

  _showGridCellInfoBar() {
    this.getElement("cell-infobar-container").removeAttribute("hidden");
  }

  _hideGridLineInfoBar() {
    this.getElement("line-infobar-container").setAttribute("hidden", "true");
  }

  _showGridLineInfoBar() {
    this.getElement("line-infobar-container").removeAttribute("hidden");
  }
}

exports.CssGridHighlighter = CssGridHighlighter;
