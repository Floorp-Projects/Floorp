/**
 * Copyright (c) 2013 Lea Verou. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

// Based on www.cubic-bezier.com by Lea Verou
// See https://github.com/LeaVerou/cubic-bezier

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const {
  PREDEFINED,
  PRESETS,
  DEFAULT_PRESET_CATEGORY
} = require("devtools/client/shared/widgets/CubicBezierPresets");
const {getCSSLexer} = require("devtools/shared/css/lexer");
const XHTML_NS = "http://www.w3.org/1999/xhtml";

/**
 * CubicBezier data structure helper
 * Accepts an array of coordinates and exposes a few useful getters
 * @param {Array} coordinates i.e. [.42, 0, .58, 1]
 */
function CubicBezier(coordinates) {
  if (!coordinates) {
    throw new Error("No offsets were defined");
  }

  this.coordinates = coordinates.map(n => +n);

  for (let i = 4; i--;) {
    let xy = this.coordinates[i];
    if (isNaN(xy) || (!(i % 2) && (xy < 0 || xy > 1))) {
      throw new Error(`Wrong coordinate at ${i}(${xy})`);
    }
  }

  this.coordinates.toString = function () {
    return this.map(n => {
      return (Math.round(n * 100) / 100 + "").replace(/^0\./, ".");
    }) + "";
  };
}

exports.CubicBezier = CubicBezier;

CubicBezier.prototype = {
  get P1() {
    return this.coordinates.slice(0, 2);
  },

  get P2() {
    return this.coordinates.slice(2);
  },

  toString: function () {
    // Check first if current coords are one of css predefined functions
    let predefName = Object.keys(PREDEFINED)
                           .find(key => coordsAreEqual(PREDEFINED[key],
                                                       this.coordinates));

    return predefName || "cubic-bezier(" + this.coordinates + ")";
  }
};

/**
 * Bezier curve canvas plotting class
 * @param {DOMNode} canvas
 * @param {CubicBezier} bezier
 * @param {Array} padding Amount of horizontal,vertical padding around the graph
 */
function BezierCanvas(canvas, bezier, padding) {
  this.canvas = canvas;
  this.bezier = bezier;
  this.padding = getPadding(padding);

  // Convert to a cartesian coordinate system with axes from 0 to 1
  this.ctx = this.canvas.getContext("2d");
  let p = this.padding;

  this.ctx.scale(canvas.width * (1 - p[1] - p[3]),
                 -canvas.height * (1 - p[0] - p[2]));
  this.ctx.translate(p[3] / (1 - p[1] - p[3]),
                     -1 - p[0] / (1 - p[0] - p[2]));
}

exports.BezierCanvas = BezierCanvas;

BezierCanvas.prototype = {
  /**
   * Get P1 and P2 current top/left offsets so they can be positioned
   * @return {Array} Returns an array of 2 {top:String,left:String} objects
   */
  get offsets() {
    let p = this.padding, w = this.canvas.width, h = this.canvas.height;

    return [{
      left: w * (this.bezier.coordinates[0] * (1 - p[3] - p[1]) - p[3]) + "px",
      top: h * (1 - this.bezier.coordinates[1] * (1 - p[0] - p[2]) - p[0])
           + "px"
    }, {
      left: w * (this.bezier.coordinates[2] * (1 - p[3] - p[1]) - p[3]) + "px",
      top: h * (1 - this.bezier.coordinates[3] * (1 - p[0] - p[2]) - p[0])
           + "px"
    }];
  },

  /**
   * Convert an element's left/top offsets into coordinates
   */
  offsetsToCoordinates: function (element) {
    let p = this.padding, w = this.canvas.width, h = this.canvas.height;

    // Convert padding percentage to actual padding
    p = p.map((a, i) => a * (i % 2 ? w : h));

    return [
      (parseFloat(element.style.left) - p[3]) / (w + p[1] + p[3]),
      (h - parseFloat(element.style.top) - p[2]) / (h - p[0] - p[2])
    ];
  },

  /**
   * Draw the cubic bezier curve for the current coordinates
   */
  plot: function (settings = {}) {
    let xy = this.bezier.coordinates;

    let defaultSettings = {
      handleColor: "#666",
      handleThickness: .008,
      bezierColor: "#4C9ED9",
      bezierThickness: .015,
      drawHandles: true
    };

    for (let setting in settings) {
      defaultSettings[setting] = settings[setting];
    }

    // Clear the canvas –making sure to clear the
    // whole area by resetting the transform first.
    this.ctx.save();
    this.ctx.setTransform(1, 0, 0, 1, 0, 0);
    this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
    this.ctx.restore();

    if (defaultSettings.drawHandles) {
      // Draw control handles
      this.ctx.beginPath();
      this.ctx.fillStyle = defaultSettings.handleColor;
      this.ctx.lineWidth = defaultSettings.handleThickness;
      this.ctx.strokeStyle = defaultSettings.handleColor;

      this.ctx.moveTo(0, 0);
      this.ctx.lineTo(xy[0], xy[1]);
      this.ctx.moveTo(1, 1);
      this.ctx.lineTo(xy[2], xy[3]);

      this.ctx.stroke();
      this.ctx.closePath();

      let circle = (ctx, cx, cy, r) => {
        ctx.beginPath();
        ctx.arc(cx, cy, r, 0, 2 * Math.PI, !1);
        ctx.closePath();
      };

      circle(this.ctx, xy[0], xy[1], 1.5 * defaultSettings.handleThickness);
      this.ctx.fill();
      circle(this.ctx, xy[2], xy[3], 1.5 * defaultSettings.handleThickness);
      this.ctx.fill();
    }

    // Draw bezier curve
    this.ctx.beginPath();
    this.ctx.lineWidth = defaultSettings.bezierThickness;
    this.ctx.strokeStyle = defaultSettings.bezierColor;
    this.ctx.moveTo(0, 0);
    this.ctx.bezierCurveTo(xy[0], xy[1], xy[2], xy[3], 1, 1);
    this.ctx.stroke();
    this.ctx.closePath();
  }
};

/**
 * Cubic-bezier widget. Uses the BezierCanvas class to draw the curve and
 * adds the control points and user interaction
 * @param {DOMNode} parent The container where the graph should be created
 * @param {Array} coordinates Coordinates of the curve to be drawn
 *
 * Emits "updated" events whenever the curve is changed. Along with the event is
 * sent a CubicBezier object
 */
function CubicBezierWidget(parent,
                           coordinates = PRESETS["ease-in"]["ease-in-sine"]) {
  EventEmitter.decorate(this);

  this.parent = parent;
  let {curve, p1, p2} = this._initMarkup();

  this.curveBoundingBox = curve.getBoundingClientRect();
  this.curve = curve;
  this.p1 = p1;
  this.p2 = p2;

  // Create and plot the bezier curve
  this.bezierCanvas = new BezierCanvas(this.curve,
    new CubicBezier(coordinates), [0.30, 0]);
  this.bezierCanvas.plot();

  // Place the control points
  let offsets = this.bezierCanvas.offsets;
  this.p1.style.left = offsets[0].left;
  this.p1.style.top = offsets[0].top;
  this.p2.style.left = offsets[1].left;
  this.p2.style.top = offsets[1].top;

  this._onPointMouseDown = this._onPointMouseDown.bind(this);
  this._onPointKeyDown = this._onPointKeyDown.bind(this);
  this._onCurveClick = this._onCurveClick.bind(this);
  this._onNewCoordinates = this._onNewCoordinates.bind(this);

  // Add preset preview menu
  this.presets = new CubicBezierPresetWidget(parent);

  // Add the timing function previewer
  this.timingPreview = new TimingFunctionPreviewWidget(parent);

  this._initEvents();
}

exports.CubicBezierWidget = CubicBezierWidget;

CubicBezierWidget.prototype = {
  _initMarkup: function () {
    let doc = this.parent.ownerDocument;

    let wrap = doc.createElementNS(XHTML_NS, "div");
    wrap.className = "display-wrap";

    let plane = doc.createElementNS(XHTML_NS, "div");
    plane.className = "coordinate-plane";

    let p1 = doc.createElementNS(XHTML_NS, "button");
    p1.className = "control-point";
    plane.appendChild(p1);

    let p2 = doc.createElementNS(XHTML_NS, "button");
    p2.className = "control-point";
    plane.appendChild(p2);

    let curve = doc.createElementNS(XHTML_NS, "canvas");
    curve.setAttribute("width", 150);
    curve.setAttribute("height", 370);
    curve.className = "curve";

    plane.appendChild(curve);
    wrap.appendChild(plane);

    this.parent.appendChild(wrap);

    return {
      p1,
      p2,
      curve
    };
  },

  _removeMarkup: function () {
    this.parent.querySelector(".display-wrap").remove();
  },

  _initEvents: function () {
    this.p1.addEventListener("mousedown", this._onPointMouseDown);
    this.p2.addEventListener("mousedown", this._onPointMouseDown);

    this.p1.addEventListener("keydown", this._onPointKeyDown);
    this.p2.addEventListener("keydown", this._onPointKeyDown);

    this.curve.addEventListener("click", this._onCurveClick);

    this.presets.on("new-coordinates", this._onNewCoordinates);
  },

  _removeEvents: function () {
    this.p1.removeEventListener("mousedown", this._onPointMouseDown);
    this.p2.removeEventListener("mousedown", this._onPointMouseDown);

    this.p1.removeEventListener("keydown", this._onPointKeyDown);
    this.p2.removeEventListener("keydown", this._onPointKeyDown);

    this.curve.removeEventListener("click", this._onCurveClick);

    this.presets.off("new-coordinates", this._onNewCoordinates);
  },

  _onPointMouseDown: function (event) {
    // Updating the boundingbox in case it has changed
    this.curveBoundingBox = this.curve.getBoundingClientRect();

    let point = event.target;
    let doc = point.ownerDocument;
    let self = this;

    doc.onmousemove = function drag(e) {
      let x = e.pageX;
      let y = e.pageY;
      let left = self.curveBoundingBox.left;
      let top = self.curveBoundingBox.top;

      if (x === 0 && y == 0) {
        return;
      }

      // Constrain x
      x = Math.min(Math.max(left, x), left + self.curveBoundingBox.width);

      point.style.left = x - left + "px";
      point.style.top = y - top + "px";

      self._updateFromPoints();
    };

    doc.onmouseup = function () {
      point.focus();
      doc.onmousemove = doc.onmouseup = null;
    };
  },

  _onPointKeyDown: function (event) {
    let point = event.target;
    let code = event.keyCode;

    if (code >= 37 && code <= 40) {
      event.preventDefault();

      // Arrow keys pressed
      let left = parseInt(point.style.left, 10);
      let top = parseInt(point.style.top, 10);
      let offset = 3 * (event.shiftKey ? 10 : 1);

      switch (code) {
        case 37: point.style.left = left - offset + "px"; break;
        case 38: point.style.top = top - offset + "px"; break;
        case 39: point.style.left = left + offset + "px"; break;
        case 40: point.style.top = top + offset + "px"; break;
      }

      this._updateFromPoints();
    }
  },

  _onCurveClick: function (event) {
    this.curveBoundingBox = this.curve.getBoundingClientRect();

    let left = this.curveBoundingBox.left;
    let top = this.curveBoundingBox.top;
    let x = event.pageX - left;
    let y = event.pageY - top;

    // Find which point is closer
    let distP1 = distance(x, y,
      parseInt(this.p1.style.left, 10), parseInt(this.p1.style.top, 10));
    let distP2 = distance(x, y,
      parseInt(this.p2.style.left, 10), parseInt(this.p2.style.top, 10));

    let point = distP1 < distP2 ? this.p1 : this.p2;
    point.style.left = x + "px";
    point.style.top = y + "px";

    this._updateFromPoints();
  },

  _onNewCoordinates: function (event, coordinates) {
    this.coordinates = coordinates;
  },

  /**
   * Get the current point coordinates and redraw the curve to match
   */
  _updateFromPoints: function () {
    // Get the new coordinates from the point's offsets
    let coordinates = this.bezierCanvas.offsetsToCoordinates(this.p1);
    coordinates = coordinates.concat(
      this.bezierCanvas.offsetsToCoordinates(this.p2)
    );

    this.presets.refreshMenu(coordinates);
    this._redraw(coordinates);
  },

  /**
   * Redraw the curve
   * @param {Array} coordinates The array of control point coordinates
   */
  _redraw: function (coordinates) {
    // Provide a new CubicBezier to the canvas and plot the curve
    this.bezierCanvas.bezier = new CubicBezier(coordinates);
    this.bezierCanvas.plot();
    this.emit("updated", this.bezierCanvas.bezier);

    this.timingPreview.preview(this.bezierCanvas.bezier + "");
  },

  /**
   * Set new coordinates for the control points and redraw the curve
   * @param {Array} coordinates
   */
  set coordinates(coordinates) {
    this._redraw(coordinates);

    // Move the points
    let offsets = this.bezierCanvas.offsets;
    this.p1.style.left = offsets[0].left;
    this.p1.style.top = offsets[0].top;
    this.p2.style.left = offsets[1].left;
    this.p2.style.top = offsets[1].top;
  },

  /**
   * Set new coordinates for the control point and redraw the curve
   * @param {String} value A string value. E.g. "linear",
   * "cubic-bezier(0,0,1,1)"
   */
  set cssCubicBezierValue(value) {
    if (!value) {
      return;
    }

    value = value.trim();

    // Try with one of the predefined values
    let coordinates = parseTimingFunction(value);

    this.presets.refreshMenu(coordinates);
    this.coordinates = coordinates;
  },

  destroy: function () {
    this._removeEvents();
    this._removeMarkup();

    this.timingPreview.destroy();
    this.presets.destroy();

    this.curve = this.p1 = this.p2 = null;
  }
};

/**
 * CubicBezierPreset widget.
 * Builds a menu of presets from CubicBezierPresets
 * @param {DOMNode} parent The container where the preset panel should be
 * created
 *
 * Emits "new-coordinate" event along with the coordinates
 * whenever a preset is selected.
 */
function CubicBezierPresetWidget(parent) {
  this.parent = parent;

  let {presetPane, presets, categories} = this._initMarkup();
  this.presetPane = presetPane;
  this.presets = presets;
  this.categories = categories;

  this._activeCategory = null;
  this._activePresetList = null;
  this._activePreset = null;

  this._onCategoryClick = this._onCategoryClick.bind(this);
  this._onPresetClick = this._onPresetClick.bind(this);

  EventEmitter.decorate(this);
  this._initEvents();
}

exports.CubicBezierPresetWidget = CubicBezierPresetWidget;

CubicBezierPresetWidget.prototype = {
  /*
   * Constructs a list of all preset categories and a list
   * of presets for each category.
   *
   * High level markup:
   *  div .preset-pane
   *    div .preset-categories
   *      div .category
   *      div .category
   *      ...
   *    div .preset-container
   *      div .presetList
   *        div .preset
   *        ...
   *      div .presetList
   *        div .preset
   *        ...
   */
  _initMarkup: function () {
    let doc = this.parent.ownerDocument;

    let presetPane = doc.createElementNS(XHTML_NS, "div");
    presetPane.className = "preset-pane";

    let categoryList = doc.createElementNS(XHTML_NS, "div");
    categoryList.id = "preset-categories";

    let presetContainer = doc.createElementNS(XHTML_NS, "div");
    presetContainer.id = "preset-container";

    Object.keys(PRESETS).forEach(categoryLabel => {
      let category = this._createCategory(categoryLabel);
      categoryList.appendChild(category);

      let presetList = this._createPresetList(categoryLabel);
      presetContainer.appendChild(presetList);
    });

    presetPane.appendChild(categoryList);
    presetPane.appendChild(presetContainer);

    this.parent.appendChild(presetPane);

    let allCategories = presetPane.querySelectorAll(".category");
    let allPresets = presetPane.querySelectorAll(".preset");

    return {
      presetPane: presetPane,
      presets: allPresets,
      categories: allCategories
    };
  },

  _createCategory: function (categoryLabel) {
    let doc = this.parent.ownerDocument;

    let category = doc.createElementNS(XHTML_NS, "div");
    category.id = categoryLabel;
    category.classList.add("category");

    let categoryDisplayLabel = this._normalizeCategoryLabel(categoryLabel);
    category.textContent = categoryDisplayLabel;
    category.setAttribute("title", categoryDisplayLabel);

    return category;
  },

  _normalizeCategoryLabel: function (categoryLabel) {
    return categoryLabel.replace("/-/g", " ");
  },

  _createPresetList: function (categoryLabel) {
    let doc = this.parent.ownerDocument;

    let presetList = doc.createElementNS(XHTML_NS, "div");
    presetList.id = "preset-category-" + categoryLabel;
    presetList.classList.add("preset-list");

    Object.keys(PRESETS[categoryLabel]).forEach(presetLabel => {
      let preset = this._createPreset(categoryLabel, presetLabel);
      presetList.appendChild(preset);
    });

    return presetList;
  },

  _createPreset: function (categoryLabel, presetLabel) {
    let doc = this.parent.ownerDocument;

    let preset = doc.createElementNS(XHTML_NS, "div");
    preset.classList.add("preset");
    preset.id = presetLabel;
    preset.coordinates = PRESETS[categoryLabel][presetLabel];
    // Create preset preview
    let curve = doc.createElementNS(XHTML_NS, "canvas");
    let bezier = new CubicBezier(preset.coordinates);
    curve.setAttribute("height", 50);
    curve.setAttribute("width", 50);
    preset.bezierCanvas = new BezierCanvas(curve, bezier, [0.15, 0]);
    preset.bezierCanvas.plot({
      drawHandles: false,
      bezierThickness: 0.025
    });
    preset.appendChild(curve);

    // Create preset label
    let presetLabelElem = doc.createElementNS(XHTML_NS, "p");
    let presetDisplayLabel = this._normalizePresetLabel(categoryLabel,
                                                        presetLabel);
    presetLabelElem.textContent = presetDisplayLabel;
    preset.appendChild(presetLabelElem);
    preset.setAttribute("title", presetDisplayLabel);

    return preset;
  },

  _normalizePresetLabel: function (categoryLabel, presetLabel) {
    return presetLabel.replace(categoryLabel + "-", "").replace("/-/g", " ");
  },

  _initEvents: function () {
    for (let category of this.categories) {
      category.addEventListener("click", this._onCategoryClick);
    }

    for (let preset of this.presets) {
      preset.addEventListener("click", this._onPresetClick);
    }
  },

  _removeEvents: function () {
    for (let category of this.categories) {
      category.removeEventListener("click", this._onCategoryClick);
    }

    for (let preset of this.presets) {
      preset.removeEventListener("click", this._onPresetClick);
    }
  },

  _onPresetClick: function (event) {
    this.emit("new-coordinates", event.currentTarget.coordinates);
    this.activePreset = event.currentTarget;
  },

  _onCategoryClick: function (event) {
    this.activeCategory = event.target;
  },

  _setActivePresetList: function (presetListId) {
    let presetList = this.presetPane.querySelector("#" + presetListId);
    swapClassName("active-preset-list", this._activePresetList, presetList);
    this._activePresetList = presetList;
  },

  set activeCategory(category) {
    swapClassName("active-category", this._activeCategory, category);
    this._activeCategory = category;
    this._setActivePresetList("preset-category-" + category.id);
  },

  get activeCategory() {
    return this._activeCategory;
  },

  set activePreset(preset) {
    swapClassName("active-preset", this._activePreset, preset);
    this._activePreset = preset;
  },

  get activePreset() {
    return this._activePreset;
  },

  /**
   * Called by CubicBezierWidget onload and when
   * the curve is modified via the canvas.
   * Attempts to match the new user setting with an
   * existing preset.
   * @param {Array} coordinates new coords [i, j, k, l]
   */
  refreshMenu: function (coordinates) {
    // If we cannot find a matching preset, keep
    // menu on last known preset category.
    let category = this._activeCategory;

    // If we cannot find a matching preset
    // deselect any selected preset.
    let preset = null;

    // If a category has never been viewed before
    // show the default category.
    if (!category) {
      category = this.parent.querySelector("#" + DEFAULT_PRESET_CATEGORY);
    }

    // If the new coordinates do match a preset,
    // set its category and preset button as active.
    Object.keys(PRESETS).forEach(categoryLabel => {
      Object.keys(PRESETS[categoryLabel]).forEach(presetLabel => {
        if (coordsAreEqual(PRESETS[categoryLabel][presetLabel], coordinates)) {
          category = this.parent.querySelector("#" + categoryLabel);
          preset = this.parent.querySelector("#" + presetLabel);
        }
      });
    });

    this.activeCategory = category;
    this.activePreset = preset;
  },

  destroy: function () {
    this._removeEvents();
    this.parent.querySelector(".preset-pane").remove();
  }
};

/**
 * The TimingFunctionPreviewWidget animates a dot on a scale with a given
 * timing-function
 * @param {DOMNode} parent The container where this widget should go
 */
function TimingFunctionPreviewWidget(parent) {
  this.previousValue = null;
  this.autoRestartAnimation = null;

  this.parent = parent;
  this._initMarkup();
}

TimingFunctionPreviewWidget.prototype = {
  PREVIEW_DURATION: 1000,

  _initMarkup: function () {
    let doc = this.parent.ownerDocument;

    let container = doc.createElementNS(XHTML_NS, "div");
    container.className = "timing-function-preview";

    this.dot = doc.createElementNS(XHTML_NS, "div");
    this.dot.className = "dot";
    container.appendChild(this.dot);

    let scale = doc.createElementNS(XHTML_NS, "div");
    scale.className = "scale";
    container.appendChild(scale);

    this.parent.appendChild(container);
  },

  destroy: function () {
    clearTimeout(this.autoRestartAnimation);
    this.parent.querySelector(".timing-function-preview").remove();
    this.parent = this.dot = null;
  },

  /**
   * Preview a new timing function. The current preview will only be stopped if
   * the supplied function value is different from the previous one. If the
   * supplied function is invalid, the preview will stop.
   * @param {String} value
   */
  preview: function (value) {
    // Don't restart the preview animation if the value is the same
    if (value === this.previousValue) {
      return;
    }

    clearTimeout(this.autoRestartAnimation);

    if (parseTimingFunction(value)) {
      this.dot.style.animationTimingFunction = value;
      this.restartAnimation();
    }

    this.previousValue = value;
  },

  /**
   * Re-start the preview animation from the beginning
   */
  restartAnimation: function () {
    // Just toggling the class won't do it unless there's a sync reflow
    this.dot.animate([
      { left: "-7px", offset: 0 },
      { left: "143px", offset: 0.25 },
      { left: "143px", offset: 0.5 },
      { left: "-7px", offset: 0.75 },
      { left: "-7px", offset: 1 }
    ], {
      duration: (this.PREVIEW_DURATION * 2),
      fill: "forwards"
    });

    // Restart it again after a while
    this.autoRestartAnimation = setTimeout(this.restartAnimation.bind(this),
      this.PREVIEW_DURATION * 2);
  }
};

// Helpers

function getPadding(padding) {
  let p = typeof padding === "number" ? [padding] : padding;

  if (p.length === 1) {
    p[1] = p[0];
  }

  if (p.length === 2) {
    p[2] = p[0];
  }

  if (p.length === 3) {
    p[3] = p[1];
  }

  return p;
}

function distance(x1, y1, x2, y2) {
  return Math.sqrt(Math.pow(x1 - x2, 2) + Math.pow(y1 - y2, 2));
}

/**
 * Parse a string to see whether it is a valid timing function.
 * If it is, return the coordinates as an array.
 * Otherwise, return undefined.
 * @param {String} value
 * @return {Array} of coordinates, or undefined
 */
function parseTimingFunction(value) {
  if (value in PREDEFINED) {
    return PREDEFINED[value];
  }

  let tokenStream = getCSSLexer(value);
  let getNextToken = () => {
    while (true) {
      let token = tokenStream.nextToken();
      if (!token || (token.tokenType !== "whitespace" &&
                     token.tokenType !== "comment")) {
        return token;
      }
    }
  };

  let token = getNextToken();
  if (token.tokenType !== "function" || token.text !== "cubic-bezier") {
    return undefined;
  }

  let result = [];
  for (let i = 0; i < 4; ++i) {
    token = getNextToken();
    if (!token || token.tokenType !== "number") {
      return undefined;
    }
    result.push(token.number);

    token = getNextToken();
    if (!token || token.tokenType !== "symbol" ||
        token.text !== (i == 3 ? ")" : ",")) {
      return undefined;
    }
  }

  return result;
}

// This is exported for testing.
exports._parseTimingFunction = parseTimingFunction;

/**
 * Removes a class from a node and adds it to another.
 * @param {String} className the class to swap
 * @param {DOMNode} from the node to remove the class from
 * @param {DOMNode} to the node to add the class to
 */
function swapClassName(className, from, to) {
  if (from !== null) {
    from.classList.remove(className);
  }

  if (to !== null) {
    to.classList.add(className);
  }
}

/**
 * Compares two arrays of coordinates [i, j, k, l]
 * @param {Array} c1 first coordinate array to compare
 * @param {Array} c2 second coordinate array to compare
 * @return {Boolean}
 */
function coordsAreEqual(c1, c2) {
  return c1.reduce((prev, curr, index) => prev && (curr === c2[index]), true);
}
