/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AutoRefreshHighlighter } = require("./auto-refresh");
const {
  CANVAS_SIZE,
  DEFAULT_COLOR,
  drawBubbleRect,
  drawLine,
  drawRect,
  drawRoundedRect,
  getBoundsFromPoints,
  getCurrentMatrix,
  getPathDescriptionFromPoints,
  getPointsFromDiagonal,
  updateCanvasElement,
  updateCanvasPosition,
} = require("./utils/canvas");
const {
  CanvasFrameAnonymousContentHelper,
  createNode,
  createSVGNode,
  getComputedStyle,
  moveInfobar,
} = require("./utils/markup");
const { apply } = require("devtools/shared/layout/dom-matrix-2d");
const {
  getCurrentZoom,
  getDisplayPixelRatio,
  setIgnoreLayoutChanges,
} = require("devtools/shared/layout/utils");
const { stringifyGridFragments } = require("devtools/server/actors/utils/css-grid-utils");
const { LocalizationHelper } = require("devtools/shared/l10n");

const LAYOUT_STRINGS_URI = "devtools/client/locales/layout.properties";
const LAYOUT_L10N = new LocalizationHelper(LAYOUT_STRINGS_URI);

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

// 25 is a good margin distance between the document grid container edge without cutting
// off parts of the arrow box container.
const OFFSET_FROM_EDGE = 25;

/**
 * Given an `edge` of a box, return the name of the edge one move to the right.
 */
function rotateEdgeRight(edge) {
  switch (edge) {
    case "top": return "right";
    case "right": return "bottom";
    case "bottom": return "left";
    case "left": return "top";
    default: return edge;
  }
}

/**
 * Given an `edge` of a box, return the name of the edge one move to the left.
 */
function rotateEdgeLeft(edge) {
  switch (edge) {
    case "top": return "left";
    case "right": return "top";
    case "bottom": return "right";
    case "left": return "bottom";
    default: return edge;
  }
}

/**
 * Given an `edge` of a box, return the name of the opposite edge.
 */
function reflectEdge(edge) {
  switch (edge) {
    case "top": return "bottom";
    case "right": return "left";
    case "bottom": return "top";
    case "left": return "right";
    default: return edge;
  }
}

/**
 * Cached used by `CssGridHighlighter.getGridGapPattern`.
 */
const gCachedGridPattern = new Map();

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

    // Initialize the <canvas> position to the top left corner of the page.
    this._canvasPosition = {
      x: 0,
      y: 0
    };

    // Calling `updateCanvasPosition` anyway since the highlighter could be initialized
    // on a page that has scrolled already.
    updateCanvasPosition(this._canvasPosition, this._scroll, this.win,
      this._winDimensions);
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

    // Build the SVG element.
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

    // Build the grid area infobar markup.
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

    // Build the grid cell infobar markup.
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

    // Build the grid line infobar markup.
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

  clearCache() {
    gCachedGridPattern.clear();
  }

  /**
   * Clear the grid area highlights.
   */
  clearGridAreas() {
    let areas = this.getElement("areas");
    areas.setAttribute("d", "");
  }

  /**
   * Clear the grid cell highlights.
   */
  clearGridCell() {
    let cells = this.getElement("cells");
    cells.setAttribute("d", "");
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
    this.clearCache();
    AutoRefreshHighlighter.prototype.destroy.call(this);
  }

  get canvas() {
    return this.getElement("canvas");
  }

  get color() {
    return this.options.color || DEFAULT_COLOR;
  }

  get ctx() {
    return this.canvas.getCanvasContext("2d");
  }

  getElement(id) {
    return this.markup.getElement(this.ID_CLASS_PREFIX + id);
  }

  getFirstColLinePos(fragment) {
    return fragment.cols.lines[0].start;
  }

  getFirstRowLinePos(fragment) {
    return fragment.rows.lines[0].start;
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

  getLastRowLinePos(fragment) {
    return fragment.rows.lines[fragment.rows.lines.length - 1].start;
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

  _hideGridAreaInfoBar() {
    this.getElement("area-infobar-container").setAttribute("hidden", "true");
  }

  _hideGridCellInfoBar() {
    this.getElement("cell-infobar-container").setAttribute("hidden", "true");
  }

  _hideGridElements() {
    this.getElement("elements").setAttribute("hidden", "true");
  }

  _hideGridLineInfoBar() {
    this.getElement("line-infobar-container").setAttribute("hidden", "true");
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
   * @param  {Object} fragment
   * @return {Boolean}
   */
  isValidFragment(fragment) {
    return fragment.cols.tracks.length && fragment.rows.tracks.length;
  }

  /**
   * The <canvas>'s position needs to be updated if the page scrolls too much, in order
   * to give the illusion that it always covers the viewport.
   */
  _scrollUpdate() {
    let hasUpdated = updateCanvasPosition(this._canvasPosition, this._scroll, this.win,
      this._winDimensions);

    if (hasUpdated) {
      this._update();
    }
  }

  _show() {
    if (!this.isGrid()) {
      this.hide();
      return false;
    }

    // The grid pattern cache should be cleared in case the color changed.
    this.clearCache();

    // Hide the canvas, grid element highlights and infobar.
    this._hide();

    return this._update();
  }

  _showGrid() {
    this.getElement("canvas").removeAttribute("hidden");
  }

  _showGridAreaInfoBar() {
    this.getElement("area-infobar-container").removeAttribute("hidden");
  }

  _showGridCellInfoBar() {
    this.getElement("cell-infobar-container").removeAttribute("hidden");
  }

  _showGridElements() {
    this.getElement("elements").removeAttribute("hidden");
  }

  _showGridLineInfoBar() {
    this.getElement("line-infobar-container").removeAttribute("hidden");
  }

  /**
   * Shows all the grid area highlights for the current grid.
   */
  showAllGridAreas() {
    this.renderGridArea();
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
   * If a page hide event is triggered for current window's highlighter, hide the
   * highlighter.
   */
  onPageHide({ target }) {
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
    this.clearCache();

    if (isTopLevel) {
      this.hide();
    }
  }

  renderFragment(fragment) {
    if (!this.isValidFragment(fragment)) {
      return;
    }

    this.renderLines(fragment.cols, COLUMNS, this.getFirstRowLinePos(fragment),
      this.getLastRowLinePos(fragment));
    this.renderLines(fragment.rows, ROWS, this.getFirstColLinePos(fragment),
      this.getLastColLinePos(fragment));

    if (this.options.showGridAreasOverlay) {
      this.renderGridAreaOverlay();
    }

    // Line numbers are rendered in a 2nd step to avoid overlapping with existing lines.
    if (this.options.showGridLineNumbers) {
      this.renderLineNumbers(fragment.cols, COLUMNS, this.getFirstRowLinePos(fragment));
      this.renderLineNumbers(fragment.rows, ROWS, this.getFirstColLinePos(fragment));
      this.renderNegativeLineNumbers(fragment.cols, COLUMNS,
        this.getLastRowLinePos(fragment));
      this.renderNegativeLineNumbers(fragment.rows, ROWS,
        this.getLastColLinePos(fragment));
    }
  }

  /**
   * Render the grid area highlight for the given area name or for all the grid areas.
   *
   * @param  {String} areaName
   *         Name of the grid area to be highlighted. If no area name is provided, all
   *         the grid areas should be highlighted.
   */
  renderGridArea(areaName) {
    let { devicePixelRatio } = this.win;
    let displayPixelRatio = getDisplayPixelRatio(this.win);
    let paths = [];

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
    let fontSize = GRID_AREA_NAME_FONT_SIZE * displayPixelRatio;
    let canvasX = Math.round(this._canvasPosition.x * devicePixelRatio);
    let canvasY = Math.round(this._canvasPosition.y * devicePixelRatio);

    this.ctx.save();
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

        // If the font size exceeds the bounds of the containing grid cell, size it its
        // row or column dimension, whichever is smallest.
        if (fontSize > (column.breadth * displayPixelRatio) ||
            fontSize > (row.breadth * displayPixelRatio)) {
          fontSize = Math.min([column.breadth, row.breadth]);
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
        // a border color matching the grid color, and a white background.
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

        // Draw the line edges for the grid area.
        const areaColStart = fragment.cols.lines[columnStart - 1];
        const areaColEnd = fragment.cols.lines[columnEnd - 1];

        const areaRowStart = fragment.rows.lines[rowStart - 1];
        const areaRowEnd = fragment.rows.lines[rowEnd - 1];

        const areaColStartLinePos = areaColStart.start + areaColStart.breadth;
        const areaRowStartLinePos = areaRowStart.start + areaRowStart.breadth;

        this.renderLine(areaColStartLinePos + padding, areaRowStartLinePos,
          areaRowEnd.start, COLUMNS, "areaEdge");
        this.renderLine(areaColEnd.start - padding, areaRowStartLinePos,
          areaRowEnd.start, COLUMNS, "areaEdge");

        this.renderLine(areaRowStartLinePos + padding, areaColStartLinePos,
          areaColEnd.start, ROWS, "areaEdge");
        this.renderLine(areaRowEnd.start - padding, areaColStartLinePos, areaColEnd.start,
          ROWS, "areaEdge");

        this.renderGridAreaName(fragment, area);
      }
    }
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

    // Find current angle of grid by measuring the angle of two arbitrary points,
    // then rotate canvas, so the hash pattern stays 45deg to the gridlines.
    let p1 = apply(this.currentMatrix, [0, 0]);
    let p2 = apply(this.currentMatrix, [1, 0]);
    let angleRad = Math.atan2(p2[1] - p1[1], p2[0] - p1[0]);
    this.ctx.rotate(angleRad);

    this.ctx.fill();
    this.ctx.restore();
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
   * @param  {Boolean||undefined} isStackedLine
   *         Boolean indicating if the line is stacked.
   */
  renderGridLineNumber(lineNumber, linePos, startPos, breadth, dimensionType,
    isStackedLine) {
    let displayPixelRatio = getDisplayPixelRatio(this.win);
    let { devicePixelRatio } = this.win;
    let offset = (displayPixelRatio / 2) % 1;
    let fontSize = GRID_FONT_SIZE * displayPixelRatio;
    let canvasX = Math.round(this._canvasPosition.x * devicePixelRatio);
    let canvasY = Math.round(this._canvasPosition.y * devicePixelRatio);

    linePos = Math.round(linePos);
    startPos = Math.round(startPos);
    breadth = Math.round(breadth);

    if (linePos + breadth < 0) {
      // Don't render the line number since the line is not visible on screen.
      return;
    }

    this.ctx.save();
    this.ctx.translate(offset - canvasX, offset - canvasY);
    this.ctx.font = fontSize + "px " + GRID_FONT_FAMILY;

    // For a general grid box, the height of the character "m" will be its minimum width
    // and height. If line number's text width is greater, then use the grid box's text
    // width instead.
    let textHeight = this.ctx.measureText("m").width;
    let textWidth = Math.max(textHeight, this.ctx.measureText(lineNumber).width);

    // Padding in pixels for the line number text inside of the line number container.
    let padding = 3 * displayPixelRatio;
    let offsetFromEdge = 2 * displayPixelRatio;

    let boxWidth = textWidth + 2 * padding;
    let boxHeight = textHeight + 2 * padding;

    // Calculate the x & y coordinates for the line number container, so that its arrow
    // tip is centered on the line (or the gap if there is one), and is offset by the
    // calculated padding value from the grid container edge.
    let x, y;

    if (dimensionType === COLUMNS) {
      x = linePos + breadth / 2;
      y = startPos;

      if (lineNumber > 0) {
        y -= offsetFromEdge;
      } else {
        y += offsetFromEdge;
      }
    } else if (dimensionType === ROWS) {
      x = startPos;
      y = linePos + breadth / 2;

      if (lineNumber > 0) {
        x -= offsetFromEdge;
      } else {
        x += offsetFromEdge;
      }
    }

    [x, y] = apply(this.currentMatrix, [x, y]);

    if (isStackedLine) {
      // Offset the stacked line number by half of the box's width/height.
      const xOffset = boxWidth / 4;
      const yOffset = boxHeight / 4;

      if (lineNumber > 0) {
        x -= xOffset;
        y -= yOffset;
      } else {
        x += xOffset;
        y += yOffset;
      }
    }

    // Draw a bubble rectangular arrow with a border width of 2 pixels, a border color
    // matching the grid color and a white background (the line number will be written in
    // black).
    this.ctx.lineWidth = 2 * displayPixelRatio;
    this.ctx.strokeStyle = this.color;
    this.ctx.fillStyle = "white";

    // See param definitions of drawBubbleRect.
    let radius = 2 * displayPixelRatio;
    let margin = 2 * displayPixelRatio;
    let arrowSize = 8 * displayPixelRatio;

    let minBoxSize = arrowSize * 2 + padding;
    boxWidth = Math.max(boxWidth, minBoxSize);
    boxHeight = Math.max(boxHeight, minBoxSize);

    // Determine default box edge to aim the line number arrow at.
    let boxEdge;
    if (dimensionType === COLUMNS) {
      if (lineNumber > 0) {
        boxEdge = "top";
      } else {
        boxEdge = "bottom";
      }
    }
    if (dimensionType === ROWS) {
      if (lineNumber > 0) {
        boxEdge = "left";
      } else {
        boxEdge = "right";
      }
    }

    // Rotate box edge as needed for writing mode and text direction.
    let { direction, writingMode } = getComputedStyle(this.currentNode);

    switch (writingMode) {
      case "horizontal-tb":
        // This is the initial value.  No further adjustment needed.
        break;
      case "vertical-rl":
        boxEdge = rotateEdgeRight(boxEdge);
        break;
      case "vertical-lr":
        if (dimensionType === COLUMNS) {
          boxEdge = rotateEdgeLeft(boxEdge);
        } else {
          boxEdge = rotateEdgeRight(boxEdge);
        }
        break;
      case "sideways-rl":
        boxEdge = rotateEdgeRight(boxEdge);
        break;
      case "sideways-lr":
        boxEdge = rotateEdgeLeft(boxEdge);
        break;
      default:
        console.error(`Unexpected writing-mode: ${writingMode}`);
    }

    switch (direction) {
      case "ltr":
        // This is the initial value.  No further adjustment needed.
        break;
      case "rtl":
        if (dimensionType === ROWS) {
          boxEdge = reflectEdge(boxEdge);
        }
        break;
      default:
        console.error(`Unexpected direction: ${direction}`);
    }

    // Default to drawing outside the edge, but move inside when close to viewport.
    let minOffsetFromEdge = OFFSET_FROM_EDGE * displayPixelRatio;
    let { width, height } = this._winDimensions;
    width *= displayPixelRatio;
    height *= displayPixelRatio;

    // Check if the x or y position of the line number's arrow is too close to the edge
    // of the window.  If it is too close, adjust the position by 2 x boxWidth or
    // boxHeight since we're now going the opposite direction.
    switch (boxEdge) {
      case "left":
        if (x < minOffsetFromEdge) {
          x += 2 * boxWidth;
        }
        break;
      case "right":
        if ((width - x) < minOffsetFromEdge) {
          x -= 2 * boxWidth;
        }
        break;
      case "top":
        if (y < minOffsetFromEdge) {
          y += 2 * boxHeight;
        }
        break;
      case "bottom":
        if ((height - y) < minOffsetFromEdge) {
          y -= 2 * boxHeight;
        }
        break;
    }

    // Draw the bubble rect to show the arrow.
    drawBubbleRect(this.ctx, x, y, boxWidth, boxHeight, radius, margin, arrowSize,
                   boxEdge);

    // Adjust position based on the edge.
    switch (boxEdge) {
      case "left":
        x -= (boxWidth + arrowSize + radius) - boxWidth / 2;
        break;
      case "right":
        x += (boxWidth + arrowSize + radius) - boxWidth / 2;
        break;
      case "top":
        y -= (boxHeight + arrowSize + radius) - boxHeight / 2;
        break;
      case "bottom":
        y += (boxHeight + arrowSize + radius) - boxHeight / 2;
        break;
    }

    // Write the line number inside of the rectangle.
    this.ctx.textAlign = "center";
    this.ctx.textBaseline = "middle";
    this.ctx.fillStyle = "black";
    const numberText = isStackedLine ? "" : lineNumber;
    this.ctx.fillText(numberText, x, y);
    this.ctx.restore();
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
    let canvasX = Math.round(this._canvasPosition.x * devicePixelRatio);
    let canvasY = Math.round(this._canvasPosition.y * devicePixelRatio);

    linePos = Math.round(linePos);
    startPos = Math.round(startPos);
    endPos = Math.round(endPos);

    this.ctx.save();
    this.ctx.setLineDash(GRID_LINES_PROPERTIES[lineType].lineDash);
    this.ctx.translate(offset - canvasX, offset - canvasY);

    let lineOptions = {
      matrix: this.currentMatrix
    };

    if (this.options.showInfiniteLines) {
      lineOptions.extendToBoundaries = [canvasX, canvasY, canvasX + CANVAS_SIZE,
                                        canvasY + CANVAS_SIZE];
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
   * Render the grid lines given the grid dimension information of the
   * column or row lines.
   *
   * @param  {GridDimension} gridDimension
   *         Column or row grid dimension object.
   * @param  {Object} quad.bounds
   *         The content bounds of the box model region quads.
   * @param  {String} dimensionType
   *         The grid dimension type which is either the constant COLUMNS or ROWS.
   * @param  {Number} startPos
   *         The start position of the cross side ("left" for ROWS and "top" for COLUMNS)
   *         of the grid dimension.
   * @param  {Number} endPos
   *         The end position of the cross side ("left" for ROWS and "top" for COLUMNS)
   *         of the grid dimension.
   */
  renderLines(gridDimension, dimensionType, startPos, endPos) {
    const { lines, tracks } = gridDimension;
    const lastEdgeLineIndex = this.getLastEdgeLineIndex(tracks);

    for (let i = 0; i < lines.length; i++) {
      let line = lines[i];
      let linePos = line.start;

      if (i == 0 || i == lastEdgeLineIndex) {
        this.renderLine(linePos, startPos, endPos, dimensionType, "edge");
      } else {
        this.renderLine(linePos, startPos, endPos, dimensionType, tracks[i - 1].type);
      }

      // Render a second line to illustrate the gutter for non-zero breadth.
      if (line.breadth > 0) {
        this.renderGridGap(linePos, startPos, endPos, line.breadth, dimensionType);
        this.renderLine(linePos + line.breadth, startPos, endPos, dimensionType,
          tracks[i].type);
      }
    }
  }

  /**
   * Render the grid lines given the grid dimension information of the
   * column or row lines.
   *
   * @param  {GridDimension} gridDimension
   *         Column or row grid dimension object.
   * @param  {String} dimensionType
   *         The grid dimension type which is either the constant COLUMNS or ROWS.
   * @param  {Number} startPos
   *         The start position of the cross side ("left" for ROWS and "top" for COLUMNS)
   *         of the grid dimension.
   */
  renderLineNumbers(gridDimension, dimensionType, startPos) {
    const { lines, tracks } = gridDimension;

    for (let i = 0, line; (line = lines[i++]);) {
      // If you place something using negative numbers, you can trigger some implicit
      // grid creation above and to the left of the explicit grid (assuming a
      // horizontal-tb writing mode).
      //
      // The first explicit grid line gets the number of 1, and any implicit grid lines
      // before 1 get negative numbers. Since here we're rendering only the positive line
      // numbers, we have to skip any implicit grid lines before the first one that is
      // explicit. The API returns a 0 as the line's number for these implicit lines that
      // occurs before the first explicit line.
      if (line.number === 0) {
        continue;
      }

      // Check for overlapping lines by measuring the track width between them.
      // We render a second box beneath the last overlapping
      // line number to indicate there are lines beneath it.
      const gridTrack = tracks[i - 1];

      if (gridTrack) {
        const { breadth }  = gridTrack;

        if (breadth === 0) {
          this.renderGridLineNumber(line.number, line.start, startPos, line.breadth,
            dimensionType, true);
          continue;
        }
      }

      this.renderGridLineNumber(line.number, line.start, startPos, line.breadth,
        dimensionType);
    }
  }

  /**
   * Render the negative grid lines given the grid dimension information of the
   * column or row lines.
   *
   * @param  {GridDimension} gridDimension
   *         Column or row grid dimension object.
   * @param  {String} dimensionType
   *         The grid dimension type which is either the constant COLUMNS or ROWS.
   * @param  {Number} startPos
   *         The start position of the cross side ("left" for ROWS and "top" for COLUMNS)
   *         of the grid dimension.
   */
  renderNegativeLineNumbers(gridDimension, dimensionType, startPos) {
    const { lines, tracks } = gridDimension;

    for (let i = 0, line; (line = lines[i++]);) {
      let linePos = line.start;
      let negativeLineNumber = line.negativeNumber;

      // Don't render any negative line number greater than -1.
      if (negativeLineNumber == 0) {
        break;
      }

      // Check for overlapping lines by measuring the track width between them.
      // We render a second box beneath the last overlapping
      // line number to indicate there are lines beneath it.
      const gridTrack = tracks[i - 1];
      if (gridTrack) {
        const { breadth } = gridTrack;

        // Ensure "-1" is always visible, since it is always the largest number.
        if (breadth === 0 && negativeLineNumber != -1) {
          this.renderGridLineNumber(negativeLineNumber, linePos, startPos,
            line.breadth, dimensionType, true);
          continue;
        }
      }

      this.renderGridLineNumber(negativeLineNumber, linePos, startPos, line.breadth,
        dimensionType);
    }
  }

  /**
   * Update the highlighter on the current highlighted node (the one that was
   * passed as an argument to show(node)). Should be called whenever node's geometry
   * or grid changes.
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
    updateCanvasElement(this.canvas, this._canvasPosition, this.win.devicePixelRatio);

    // Clear the grid area highlights.
    this.clearGridAreas();
    this.clearGridCell();

    // Update the current matrix used in our canvas' rendering.
    let { currentMatrix, hasNodeTransformations } =
      getCurrentMatrix(this.currentNode, this.win);
    this.currentMatrix = currentMatrix;
    this.hasNodeTransformations = hasNodeTransformations;

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
      `position: absolute; width: ${width}px; height: ${height}px; overflow: hidden`);

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
    let position = LAYOUT_L10N.getFormatStr("layout.rowColumnPositions", rowNumber,
      columnNumber);

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
    moveInfobar(container, getBoundsFromPoints([{x, y}, {x, y}, {x, y}, {x, y}]),
      this.win);
  }
}

exports.CssGridHighlighter = CssGridHighlighter;
