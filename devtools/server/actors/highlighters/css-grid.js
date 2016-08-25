/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { extend } = require("sdk/core/heritage");
const { AutoRefreshHighlighter } = require("./auto-refresh");
const { CanvasFrameAnonymousContentHelper, createNode } = require("./utils/markup");
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
 * - showGridLineNumbers {Boolean}
 *   Displays the grid line numbers
 * - showInfiniteLines {Boolean}
 *   Displays an infinite line to represent the grid lines
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

    return container;
  },

  destroy() {
    AutoRefreshHighlighter.prototype.destroy.call(this);
    this.markup.destroy();
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

  _show() {
    if (Services.prefs.getBoolPref(CSS_GRID_ENABLED_PREF) && !this.isGrid()) {
      this.hide();
      return false;
    }

    return this._update();
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
   * Should be called whenever node's geometry or grid changes
   */
  _update() {
    setIgnoreLayoutChanges(true);

    // Clear the canvas.
    this.clearCanvas();

    // And start drawing the fragments.
    for (let i = 0; i < this.gridData.length; i++) {
      let fragment = this.gridData[i];
      let quad = this.currentQuads.content[i];
      this.renderFragment(fragment, quad);
    }

    this._showGrid();

    setIgnoreLayoutChanges(false, this.currentNode.ownerDocument.documentElement);
    return true;
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
        linePos = linePos + line.breadth;
        this.renderLine(linePos, lineStartPos, lineEndPos, dimensionType,
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
    if (dimensionType === COLUMNS) {
      this.ctx.fillText(lineNumber, linePos, startPos);
    } else {
      let textWidth = this.ctx.measureText(lineNumber).width;
      this.ctx.fillText(lineNumber, startPos - textWidth, linePos);
    }
  },

  _hide() {
    setIgnoreLayoutChanges(true);
    this._hideGrid();
    setIgnoreLayoutChanges(false, this.currentNode.ownerDocument.documentElement);
  },

  _hideGrid() {
    this.getElement("canvas").setAttribute("hidden", "true");
  },

  _showGrid() {
    this.getElement("canvas").removeAttribute("hidden");
  }
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
