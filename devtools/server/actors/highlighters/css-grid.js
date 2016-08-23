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
const LINE_DASH_ARRAY = [5, 3];
const LINE_STROKE_STYLE = "#483D88";

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
 * - infiniteLines {Boolean}
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

  renderColLines(cols, {bounds}, startRowPos, endRowPos) {
    let y1 = (bounds.top / getCurrentZoom(this.win)) + startRowPos;
    let y2 = (bounds.top / getCurrentZoom(this.win)) + endRowPos;

    if (this.options.infiniteLines) {
      y1 = 0;
      y2 = parseInt(this.canvas.getAttribute("height"), 10);
    }

    for (let i = 0; i < cols.lines.length; i++) {
      let line = cols.lines[i];
      let x = (bounds.left / getCurrentZoom(this.win)) + line.start;
      this.renderLine(x, y1, x, y2);

      // Render a second line to illustrate the gutter for non-zero breadth.
      if (line.breadth > 0) {
        x = x + line.breadth;
        this.renderLine(x, y1, x, y2);
      }
    }
  },

  renderRowLines(rows, {bounds}, startColPos, endColPos) {
    let x1 = (bounds.left / getCurrentZoom(this.win)) + startColPos;
    let x2 = (bounds.left / getCurrentZoom(this.win)) + endColPos;

    if (this.options.infiniteLines) {
      x1 = 0;
      x2 = parseInt(this.canvas.getAttribute("width"), 10);
    }

    for (let i = 0; i < rows.lines.length; i++) {
      let line = rows.lines[i];
      let y = (bounds.top / getCurrentZoom(this.win)) + line.start;
      this.renderLine(x1, y, x2, y);

      // Render a second line to illustrate the gutter for non-zero breadth.
      if (line.breadth > 0) {
        y = y + line.breadth;
        this.renderLine(x1, y, x2, y);
      }
    }
  },

  renderLine(x1, y1, x2, y2) {
    this.ctx.save();
    this.ctx.setLineDash(LINE_DASH_ARRAY);
    this.ctx.beginPath();
    this.ctx.translate(.5, .5);
    this.ctx.moveTo(x1, y1);
    this.ctx.lineTo(x2, y2);
    this.ctx.strokeStyle = LINE_STROKE_STYLE;
    this.ctx.stroke();
    this.ctx.restore();
  },

  renderFragment(fragment, quad) {
    this.renderColLines(fragment.cols, quad,
                        this.getFirstRowLinePos(fragment),
                        this.getLastRowLinePos(fragment));

    this.renderRowLines(fragment.rows, quad,
                        this.getFirstColLinePos(fragment),
                        this.getLastColLinePos(fragment));
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
