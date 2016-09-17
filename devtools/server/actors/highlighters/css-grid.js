/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { extend } = require("sdk/core/heritage");
const { AutoRefreshHighlighter } = require("./auto-refresh");
const {
  CanvasFrameAnonymousContentHelper,
  createNode,
  createSVGNode,
  moveInfobar,
} = require("./utils/markup");
const {
  getCurrentZoom,
  setIgnoreLayoutChanges
} = require("devtools/shared/layout/utils");
const Services = require("Services");

const CSS_GRID_ENABLED_PREF = "layout.css.grid.enabled";
const ROWS = "rows";
const COLUMNS = "cols";
const GRID_LINES_PROPERTIES = {
  "edge": {
    lineDash: [0, 0],
    strokeStyle: "#4B0082"
  },
  "explicit": {
    lineDash: [5, 3],
    strokeStyle: "#8A2BE2"
  },
  "implicit": {
    lineDash: [2, 2],
    strokeStyle: "#9370DB"
  }
};

// px
const GRID_GAP_PATTERN_WIDTH = 14;
const GRID_GAP_PATTERN_HEIGHT = 14;
const GRID_GAP_PATTERN_LINE_DASH = [5, 3];
const GRID_GAP_PATTERN_STROKE_STYLE = "#9370DB";

/**
 * Cached used by `CssGridHighlighter.getGridGapPattern`.
 */
const gCachedGridPattern = new Map();

/**
 * The CssGridHighlighter is the class that overlays a visual grid on top of
 * display:grid elements.
 *
 * Usage example:
 * let h = new CssGridHighlighter(env);
 * h.show(node, options);
 * h.hide();
 * h.destroy();
 *
 * Available Options:
 * - showGridArea(areaName)
 *     @param  {String} areaName
 *     Shows the grid area highlight for the given area name.
 * - showAllGridAreas
 *     Shows all the grid area highlights for the current grid.
 * - showGridLineNumbers(isShown)
 *     @param  {Boolean}
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
 *     </g>
 *   </svg>
 *   <div class="css-grid-infobar-container">
 *     <div class="css-grid-infobar">
 *       <div class="css-grid-infobar-text">
 *         <span class="css-grid-infobar-areaname">Grid Area Name</span>
 *         <span class="css-grid-infobar-dimensions"Grid Area Dimensions></span>
 *       </div>
 *     </div>
 *   </div>
 * </div>
 */
function CssGridHighlighter(highlighterEnv) {
  AutoRefreshHighlighter.call(this, highlighterEnv);

  this.markup = new CanvasFrameAnonymousContentHelper(this.highlighterEnv,
    this._buildMarkup.bind(this));
}

CssGridHighlighter.prototype = extend(AutoRefreshHighlighter.prototype, {
  typeName: "CssGridHighlighter",

  ID_CLASS_PREFIX: "css-grid-",

  _buildMarkup() {
    let container = createNode(this.win, {
      attributes: {
        "class": "highlighter-container"
      }
    });

    // We use a <canvas> element so that we can draw an arbitrary number of lines
    // which wouldn't be possible with HTML or SVG without having to insert and remove
    // the whole markup on every update.
    createNode(this.win, {
      parent: container,
      nodeType: "canvas",
      attributes: {
        "id": "canvas",
        "class": "canvas",
        "hidden": "true"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // Build the SVG element
    let svg = createSVGNode(this.win, {
      nodeType: "svg",
      parent: container,
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

    // Building the grid infobar markup
    let infobarContainer = createNode(this.win, {
      parent: container,
      attributes: {
        "class": "infobar-container",
        "id": "infobar-container",
        "position": "top",
        "hidden": "true"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    let infobar = createNode(this.win, {
      parent: infobarContainer,
      attributes: {
        "class": "infobar"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    let textbox = createNode(this.win, {
      parent: infobar,
      attributes: {
        "class": "infobar-text"
      },
      prefix: this.ID_CLASS_PREFIX
    });
    createNode(this.win, {
      nodeType: "span",
      parent: textbox,
      attributes: {
        "class": "infobar-areaname",
        "id": "infobar-areaname"
      },
      prefix: this.ID_CLASS_PREFIX
    });
    createNode(this.win, {
      nodeType: "span",
      parent: textbox,
      attributes: {
        "class": "infobar-dimensions",
        "id": "infobar-dimensions"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    return container;
  },

  destroy() {
    AutoRefreshHighlighter.prototype.destroy.call(this);
    this.markup.destroy();
    gCachedGridPattern.clear();
  },

  getElement(id) {
    return this.markup.getElement(this.ID_CLASS_PREFIX + id);
  },

  get ctx() {
    return this.canvas.getCanvasContext("2d");
  },

  get canvas() {
    return this.getElement("canvas");
  },

  /**
   * Gets the grid gap pattern used to render the gap regions.
   *
   * @param  {String} dimensionType
   *         The grid dimension type which is either the constant COLUMNS or ROWS.
   * @return {CanvasPattern} grid gap pattern.
   */
  getGridGapPattern(dimensionType) {
    if (gCachedGridPattern.has(dimensionType)) {
      return gCachedGridPattern.get(dimensionType);
    }

    // Create the diagonal lines pattern for the rendering the grid gaps.
    let canvas = createNode(this.win, { nodeType: "canvas" });
    canvas.width = GRID_GAP_PATTERN_WIDTH;
    canvas.height = GRID_GAP_PATTERN_HEIGHT;

    let ctx = canvas.getContext("2d");
    ctx.setLineDash(GRID_GAP_PATTERN_LINE_DASH);
    ctx.beginPath();
    ctx.translate(.5, .5);

    if (dimensionType === COLUMNS) {
      ctx.moveTo(0, 0);
      ctx.lineTo(GRID_GAP_PATTERN_WIDTH, GRID_GAP_PATTERN_HEIGHT);
    } else {
      ctx.moveTo(GRID_GAP_PATTERN_WIDTH, 0);
      ctx.lineTo(0, GRID_GAP_PATTERN_HEIGHT);
    }

    ctx.strokeStyle = GRID_GAP_PATTERN_STROKE_STYLE;
    ctx.stroke();

    let pattern = ctx.createPattern(canvas, "repeat");
    gCachedGridPattern.set(dimensionType, pattern);
    return pattern;
  },

  _show() {
    if (Services.prefs.getBoolPref(CSS_GRID_ENABLED_PREF) && !this.isGrid()) {
      this.hide();
      return false;
    }

    return this._update();
  },

  /**
   * Shows the grid area highlight for the given area name.
   *
   * @param  {String} areaName
   *         Grid area name.
   */
  showGridArea(areaName) {
    this.renderGridArea(areaName);
    this._showGridArea();
  },

  /**
   * Shows all the grid area highlights for the current grid.
   */
  showAllGridAreas() {
    this.renderGridArea();
    this._showGridArea();
  },

  /**
   * Clear the grid area highlights.
   */
  clearGridAreas() {
    let box = this.getElement("areas");
    box.setAttribute("d", "");
  },

  /**
   * Checks if the current node has a CSS Grid layout.
   *
   * @return  {Boolean} true if the current node has a CSS grid layout, false otherwise.
   */
  isGrid() {
    return this.currentNode.getGridFragments().length > 0;
  },

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
  },

  /**
   * Update the highlighter on the current highlighted node (the one that was
   * passed as an argument to show(node)).
   * Should be called whenever node's geometry or grid changes.
   */
  _update() {
    setIgnoreLayoutChanges(true);

    // Clear the canvas the grid area highlights.
    this.clearCanvas();
    this.clearGridAreas();

    // Start drawing the grid fragments.
    for (let i = 0; i < this.gridData.length; i++) {
      let fragment = this.gridData[i];
      let quad = this.currentQuads.content[i];
      this.renderFragment(fragment, quad);
    }

    // Display the grid area highlights if needed.
    if (this.options.showAllGridAreas) {
      this.showAllGridAreas();
    } else if (this.options.showGridArea) {
      this.showGridArea(this.options.showGridArea);
    }

    this._showGrid();

    setIgnoreLayoutChanges(false, this.currentNode.ownerDocument.documentElement);
    return true;
  },

  /**
   * Update the grid information displayed in the grid info bar.
   *
   * @param  {GridArea} area
   *         The grid area object.
   * @param  {Number} x1
   *         The first x-coordinate of the grid area rectangle.
   * @param  {Number} x2
   *         The second x-coordinate of the grid area rectangle.
   * @param  {Number} y1
   *         The first y-coordinate of the grid area rectangle.
   * @param  {Number} y2
   *         The second y-coordinate of the grid area rectangle.
   */
  _updateInfobar(area, x1, x2, y1, y2) {
    let width = x2 - x1;
    let height = y2 - y1;
    let dim = parseFloat(width.toPrecision(6)) +
              " \u00D7 " +
              parseFloat(height.toPrecision(6));

    this.getElement("infobar-areaname").setTextContent(area.name);
    this.getElement("infobar-dimensions").setTextContent(dim);

    this._moveInfobar(x1, x2, y1, y2);
  },

  /**
   * Move the grid infobar to the right place in the highlighter.
   *
   * @param  {Number} x1
   *         The first x-coordinate of the grid area rectangle.
   * @param  {Number} x2
   *         The second x-coordinate of the grid area rectangle.
   * @param  {Number} y1
   *         The first y-coordinate of the grid area rectangle.
   * @param  {Number} y2
   *         The second y-coordinate of the grid area rectangle.
   */
  _moveInfobar(x1, x2, y1, y2) {
    let bounds = {
      bottom: y2,
      height: y2 - y1,
      left: x1,
      right: x2,
      top: y1,
      width: x2 - x1,
      x: x1,
      y: y1,
    };
    let container = this.getElement("infobar-container");

    moveInfobar(container, bounds, this.win);
  },

  clearCanvas() {
    let ratio = parseFloat((this.win.devicePixelRatio || 1).toFixed(2));
    let width = this.win.innerWidth;
    let height = this.win.innerHeight;

    // Resize the canvas taking the dpr into account so as to have crisp lines.
    this.canvas.setAttribute("width", width * ratio);
    this.canvas.setAttribute("height", height * ratio);
    this.canvas.setAttribute("style", `width:${width}px;height:${height}px`);
    this.ctx.scale(ratio, ratio);

    this.ctx.clearRect(0, 0, width, height);
  },

  getFirstRowLinePos(fragment) {
    return fragment.rows.lines[0].start;
  },

  getLastRowLinePos(fragment) {
    return fragment.rows.lines[fragment.rows.lines.length - 1].start;
  },

  getFirstColLinePos(fragment) {
    return fragment.cols.lines[0].start;
  },

  getLastColLinePos(fragment) {
    return fragment.cols.lines[fragment.cols.lines.length - 1].start;
  },

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
  },

  renderFragment(fragment, quad) {
    this.renderLines(fragment.cols, quad, COLUMNS, "left", "top", "height",
                     this.getFirstRowLinePos(fragment),
                     this.getLastRowLinePos(fragment));
    this.renderLines(fragment.rows, quad, ROWS, "top", "left", "width",
                     this.getFirstColLinePos(fragment),
                     this.getLastColLinePos(fragment));
  },

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
  renderLines(gridDimension, {bounds}, dimensionType, mainSide, crossSide,
              mainSize, startPos, endPos) {
    let lineStartPos = (bounds[crossSide] / getCurrentZoom(this.win)) + startPos;
    let lineEndPos = (bounds[crossSide] / getCurrentZoom(this.win)) + endPos;

    if (this.options.showInfiniteLines) {
      lineStartPos = 0;
      lineEndPos = parseInt(this.canvas.getAttribute(mainSize), 10);
    }

    let lastEdgeLineIndex = this.getLastEdgeLineIndex(gridDimension.tracks);

    for (let i = 0; i < gridDimension.lines.length; i++) {
      let line = gridDimension.lines[i];
      let linePos = (bounds[mainSide] / getCurrentZoom(this.win)) + line.start;

      if (this.options.showGridLineNumbers) {
        this.renderGridLineNumber(line.number, linePos, lineStartPos, dimensionType);
      }

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
  },

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
   * @param  {[type]} lineType
   *         The grid line type - "edge", "explicit", or "implicit".
   */
  renderLine(linePos, startPos, endPos, dimensionType, lineType) {
    this.ctx.save();
    this.ctx.setLineDash(GRID_LINES_PROPERTIES[lineType].lineDash);
    this.ctx.beginPath();
    this.ctx.translate(.5, .5);

    if (dimensionType === COLUMNS) {
      this.ctx.moveTo(linePos, startPos);
      this.ctx.lineTo(linePos, endPos);
    } else {
      this.ctx.moveTo(startPos, linePos);
      this.ctx.lineTo(endPos, linePos);
    }

    this.ctx.strokeStyle = GRID_LINES_PROPERTIES[lineType].strokeStyle;
    this.ctx.stroke();
    this.ctx.restore();
  },

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
   * @param  {String} dimensionType
   *         The grid dimension type which is either the constant COLUMNS or ROWS.
   */
  renderGridLineNumber(lineNumber, linePos, startPos, dimensionType) {
    this.ctx.save();

    if (dimensionType === COLUMNS) {
      this.ctx.fillText(lineNumber, linePos, startPos);
    } else {
      let textWidth = this.ctx.measureText(lineNumber).width;
      this.ctx.fillText(lineNumber, startPos - textWidth, linePos);
    }

    this.ctx.restore();
  },

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
    this.ctx.save();
    this.ctx.fillStyle = this.getGridGapPattern(dimensionType);

    if (dimensionType === COLUMNS) {
      this.ctx.fillRect(linePos, startPos, breadth, endPos - startPos);
    } else {
      this.ctx.fillRect(startPos, linePos, endPos - startPos, breadth);
    }

    this.ctx.restore();
  },

  /**
   * Render the grid area highlight for the given area name or for all the grid areas.
   *
   * @param  {String} areaName
   *         Name of the grid area to be highlighted. If no area name is provided, all
   *         the grid areas should be highlighted.
   */
  renderGridArea(areaName) {
    let paths = [];
    let currentZoom = getCurrentZoom(this.win);

    for (let i = 0; i < this.gridData.length; i++) {
      let fragment = this.gridData[i];
      let {bounds} = this.currentQuads.content[i];

      for (let area of fragment.areas) {
        if (areaName && areaName != area.name) {
          continue;
        }

        let rowStart = fragment.rows.lines[area.rowStart - 1];
        let rowEnd = fragment.rows.lines[area.rowEnd - 1];
        let columnStart = fragment.cols.lines[area.columnStart - 1];
        let columnEnd = fragment.cols.lines[area.columnEnd - 1];

        let x1 = columnStart.start + columnStart.breadth +
          (bounds.left / currentZoom);
        let x2 = columnEnd.start + (bounds.left / currentZoom);
        let y1 = rowStart.start + rowStart.breadth +
          (bounds.top / currentZoom);
        let y2 = rowEnd.start + (bounds.top / currentZoom);

        let path = "M" + x1 + "," + y1 + " " +
                   "L" + x2 + "," + y1 + " " +
                   "L" + x2 + "," + y2 + " " +
                   "L" + x1 + "," + y2;
        paths.push(path);

        // Update and show the info bar when only displaying a single grid area.
        if (areaName) {
          this._updateInfobar(area, x1, x2, y1, y2);
          this._showInfoBar();
        }
      }
    }

    let box = this.getElement("areas");
    box.setAttribute("d", paths.join(" "));
  },

  /**
   * Hide the highlighter, the canvas and the infobar.
   */
  _hide() {
    setIgnoreLayoutChanges(true);
    this._hideGrid();
    this._hideGridArea();
    this._hideInfoBar();
    setIgnoreLayoutChanges(false, this.currentNode.ownerDocument.documentElement);
  },

  _hideGrid() {
    this.getElement("canvas").setAttribute("hidden", "true");
  },

  _showGrid() {
    this.getElement("canvas").removeAttribute("hidden");
  },

  _hideGridArea() {
    this.getElement("elements").setAttribute("hidden", "true");
  },

  _showGridArea() {
    this.getElement("elements").removeAttribute("hidden");
  },

  _hideInfoBar() {
    this.getElement("infobar-container").setAttribute("hidden", "true");
  },

  _showInfoBar() {
    this.getElement("infobar-container").removeAttribute("hidden");
  },

});
exports.CssGridHighlighter = CssGridHighlighter;

/**
 * Stringify CSS Grid data as returned by node.getGridFragments.
 * This is useful to compare grid state at each update and redraw the highlighter if
 * needed.
 *
 * @param  {Object} Grid Fragments
 * @return {String} representation of the CSS grid fragment data.
 */
function stringifyGridFragments(fragments = []) {
  return JSON.stringify(fragments.map(getStringifiableFragment));
}

function getStringifiableFragment(fragment) {
  return {
    cols: getStringifiableDimension(fragment.cols),
    rows: getStringifiableDimension(fragment.rows)
  };
}

function getStringifiableDimension(dimension) {
  return {
    lines: [...dimension.lines].map(getStringifiableLine),
    tracks: [...dimension.tracks].map(getStringifiableTrack),
  };
}

function getStringifiableLine({ breadth, number, start, names }) {
  return { breadth, number, start, names };
}

function getStringifiableTrack({ breadth, start, state, type }) {
  return { breadth, start, state, type };
}
