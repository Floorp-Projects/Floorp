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

const EventEmitter = require("devtools/toolkit/event-emitter");
const {setTimeout, clearTimeout} = require("sdk/timers");

const PREDEFINED = exports.PREDEFINED = {
  "ease": [.25, .1, .25, 1],
  "linear": [0, 0, 1, 1],
  "ease-in": [.42, 0, 1, 1],
  "ease-out": [0, 0, .58, 1],
  "ease-in-out": [.42, 0, .58, 1]
};

/**
 * CubicBezier data structure helper
 * Accepts an array of coordinates and exposes a few useful getters
 * @param {Array} coordinates i.e. [.42, 0, .58, 1]
 */
function CubicBezier(coordinates) {
  if (!coordinates) {
    throw "No offsets were defined";
  }

  this.coordinates = coordinates.map(n => +n);

  for (let i = 4; i--;) {
    let xy = this.coordinates[i];
    if (isNaN(xy) || (!(i%2) && (xy < 0 || xy > 1))) {
      throw "Wrong coordinate at " + i + "(" + xy + ")";
    }
  }

  this.coordinates.toString = function() {
    return this.map(n => {
      return (Math.round(n * 100)/100 + '').replace(/^0\./, '.');
    }) + "";
  }
}

exports.CubicBezier = CubicBezier;

CubicBezier.prototype = {
  get P1() {
    return this.coordinates.slice(0, 2);
  },

  get P2() {
    return this.coordinates.slice(2);
  },

  toString: function() {
    return 'cubic-bezier(' + this.coordinates + ')';
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
  this.ctx = this.canvas.getContext('2d');
  let p = this.padding;

  this.ctx.scale(canvas.width * (1 - p[1] - p[3]),
                 -canvas.height * (1 - p[0] - p[2]));
  this.ctx.translate(p[3] / (1 - p[1] - p[3]),
                     -1 - p[0] / (1 - p[0] - p[2]));
};

exports.BezierCanvas = BezierCanvas;

BezierCanvas.prototype = {
  /**
   * Get P1 and P2 current top/left offsets so they can be positioned
   * @return {Array} Returns an array of 2 {top:String,left:String} objects
   */
  get offsets() {
    let p = this.padding, w = this.canvas.width, h = this.canvas.height;

    return [{
      left: w * (this.bezier.coordinates[0] * (1 - p[3] - p[1]) - p[3]) + 'px',
      top: h * (1 - this.bezier.coordinates[1] * (1 - p[0] - p[2]) - p[0]) + 'px'
    }, {
      left: w * (this.bezier.coordinates[2] * (1 - p[3] - p[1]) - p[3]) + 'px',
      top: h * (1 - this.bezier.coordinates[3] * (1 - p[0] - p[2]) - p[0]) + 'px'
    }]
  },

  /**
   * Convert an element's left/top offsets into coordinates
   */
  offsetsToCoordinates: function(element) {
    let p = this.padding, w = this.canvas.width, h = this.canvas.height;

    // Convert padding percentage to actual padding
    p = p.map(function(a, i) { return a * (i % 2? w : h)});

    return [
      (parseInt(element.style.left) - p[3]) / (w + p[1] + p[3]),
      (h - parseInt(element.style.top) - p[2]) / (h - p[0] - p[2])
    ];
  },

  /**
   * Draw the cubic bezier curve for the current coordinates
   */
  plot: function(settings={}) {
    let xy = this.bezier.coordinates;

    let defaultSettings = {
      handleColor: '#666',
      handleThickness: .008,
      bezierColor: '#4C9ED9',
      bezierThickness: .015
    };

    for (let setting in settings) {
      defaultSettings[setting] = settings[setting];
    }

    this.ctx.clearRect(-.5,-.5, 2, 2);

    // Draw control handles
    this.ctx.beginPath();
    this.ctx.fillStyle = defaultSettings.handleColor;
    this.ctx.lineWidth = defaultSettings.handleThickness;
    this.ctx.strokeStyle = defaultSettings.handleColor;

    this.ctx.moveTo(0, 0);
    this.ctx.lineTo(xy[0], xy[1]);
    this.ctx.moveTo(1,1);
    this.ctx.lineTo(xy[2], xy[3]);

    this.ctx.stroke();
    this.ctx.closePath();

    function circle(ctx, cx, cy, r) {
      return ctx.beginPath();
      ctx.arc(cx, cy, r, 0, 2*Math.PI, !1);
      ctx.closePath();
    }

    circle(this.ctx, xy[0], xy[1], 1.5 * defaultSettings.handleThickness);
    this.ctx.fill();
    circle(this.ctx, xy[2], xy[3], 1.5 * defaultSettings.handleThickness);
    this.ctx.fill();

    // Draw bezier curve
    this.ctx.beginPath();
    this.ctx.lineWidth = defaultSettings.bezierThickness;
    this.ctx.strokeStyle = defaultSettings.bezierColor;
    this.ctx.moveTo(0,0);
    this.ctx.bezierCurveTo(xy[0], xy[1], xy[2], xy[3], 1,1);
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
function CubicBezierWidget(parent, coordinates=PREDEFINED["ease-in-out"]) {
  this.parent = parent;
  let {curve, p1, p2} = this._initMarkup();

  this.curve = curve;
  this.curveBoundingBox = curve.getBoundingClientRect();
  this.p1 = p1;
  this.p2 = p2;

  // Create and plot the bezier curve
  this.bezierCanvas = new BezierCanvas(this.curve,
    new CubicBezier(coordinates), [.25, 0]);
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
  this._initEvents();

  // Add the timing function previewer
  this.timingPreview = new TimingFunctionPreviewWidget(parent);

  EventEmitter.decorate(this);
}

exports.CubicBezierWidget = CubicBezierWidget;

CubicBezierWidget.prototype = {
  _initMarkup: function() {
    let doc = this.parent.ownerDocument;

    let plane = doc.createElement("div");
    plane.className = "coordinate-plane";

    let p1 = doc.createElement("button");
    p1.className = "control-point";
    p1.id = "P1";
    plane.appendChild(p1);

    let p2 = doc.createElement("button");
    p2.className = "control-point";
    p2.id = "P2";
    plane.appendChild(p2);

    let curve = doc.createElement("canvas");
    curve.setAttribute("height", "400");
    curve.setAttribute("width", "200");
    curve.id = "curve";
    plane.appendChild(curve);

    this.parent.appendChild(plane);

    return {
      p1: p1,
      p2: p2,
      curve: curve
    }
  },

  _removeMarkup: function() {
    this.parent.ownerDocument.querySelector(".coordinate-plane").remove();
  },

  _initEvents: function() {
    this.p1.addEventListener("mousedown", this._onPointMouseDown);
    this.p2.addEventListener("mousedown", this._onPointMouseDown);

    this.p1.addEventListener("keydown", this._onPointKeyDown);
    this.p2.addEventListener("keydown", this._onPointKeyDown);

    this.curve.addEventListener("click", this._onCurveClick);
  },

  _removeEvents: function() {
    this.p1.removeEventListener("mousedown", this._onPointMouseDown);
    this.p2.removeEventListener("mousedown", this._onPointMouseDown);

    this.p1.removeEventListener("keydown", this._onPointKeyDown);
    this.p2.removeEventListener("keydown", this._onPointKeyDown);

    this.curve.removeEventListener("click", this._onCurveClick);
  },

  _onPointMouseDown: function(event) {
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
    }
  },

  _onPointKeyDown: function(event) {
    let point = event.target;
    let code = event.keyCode;

    if (code >= 37 && code <= 40) {
      event.preventDefault();

      // Arrow keys pressed
      let left = parseInt(point.style.left);
      let top = parseInt(point.style.top);
      let offset = 3 * (event.shiftKey ? 10 : 1);

      switch (code) {
        case 37: point.style.left = left - offset + 'px'; break;
        case 38: point.style.top = top - offset + 'px'; break;
        case 39: point.style.left = left + offset + 'px'; break;
        case 40: point.style.top = top + offset + 'px'; break;
      }

      this._updateFromPoints();
    }
  },

  _onCurveClick: function(event) {
    let left = this.curveBoundingBox.left;
    let top = this.curveBoundingBox.top;
    let x = event.pageX - left;
    let y = event.pageY - top;

    // Find which point is closer
    let distP1 = distance(x, y,
      parseInt(this.p1.style.left), parseInt(this.p1.style.top));
    let distP2 = distance(x, y,
      parseInt(this.p2.style.left), parseInt(this.p2.style.top));

    let point = distP1 < distP2 ? this.p1 : this.p2;
    point.style.left = x + "px";
    point.style.top = y + "px";

    this._updateFromPoints();
  },

  /**
   * Get the current point coordinates and redraw the curve to match
   */
  _updateFromPoints: function() {
    // Get the new coordinates from the point's offsets
    let coordinates = this.bezierCanvas.offsetsToCoordinates(this.p1)
    coordinates = coordinates.concat(this.bezierCanvas.offsetsToCoordinates(this.p2));

    this._redraw(coordinates);
  },

  /**
   * Redraw the curve
   * @param {Array} coordinates The array of control point coordinates
   */
  _redraw: function(coordinates) {
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
    this._redraw(coordinates)

    // Move the points
    let offsets = this.bezierCanvas.offsets;
    this.p1.style.left = offsets[0].left;
    this.p1.style.top = offsets[0].top;
    this.p2.style.left = offsets[1].left;
    this.p2.style.top = offsets[1].top;
  },

  /**
   * Set new coordinates for the control point and redraw the curve
   * @param {String} value A string value. E.g. "linear", "cubic-bezier(0,0,1,1)"
   */
  set cssCubicBezierValue(value) {
    if (!value) {
      return;
    }

    value = value.trim();

    // Try with one of the predefined values
    let coordinates = PREDEFINED[value];

    // Otherwise parse the coordinates from the cubic-bezier function
    if (!coordinates && value.startsWith("cubic-bezier")) {
      coordinates = value.replace(/cubic-bezier|\(|\)/g, "").split(",").map(parseFloat);
    }

    this.coordinates = coordinates;
  },

  destroy: function() {
    this._removeEvents();
    this._removeMarkup();

    this.timingPreview.destroy();

    this.curve = this.p1 = this.p2 = null;
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

  _initMarkup: function() {
    let doc = this.parent.ownerDocument;

    let container = doc.createElement("div");
    container.className = "timing-function-preview";

    this.dot = doc.createElement("div");
    this.dot.className = "dot";
    container.appendChild(this.dot);

    let scale = doc.createElement("div");
    scale.className = "scale";
    container.appendChild(scale);

    this.parent.appendChild(container);
  },

  destroy: function() {
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
  preview: function(value) {
    // Don't restart the preview animation if the value is the same
    if (value === this.previousValue) {
      return false;
    }

    clearTimeout(this.autoRestartAnimation);

    if (isValidTimingFunction(value)) {
      this.dot.style.animationTimingFunction = value;
      this.restartAnimation();
    }

    this.previousValue = value;
  },

  /**
   * Re-start the preview animation from the beginning
   */
  restartAnimation: function() {
    // Reset the animation duration in case it was changed
    this.dot.style.animationDuration = (this.PREVIEW_DURATION * 2) + "ms";

    // Just toggling the class won't do it unless there's a sync reflow
    this.dot.classList.remove("animate");
    let w = this.dot.offsetWidth;
    this.dot.classList.add("animate");

    // Restart it again after a while
    this.autoRestartAnimation = setTimeout(this.restartAnimation.bind(this),
      this.PREVIEW_DURATION * 2);
  }
};

// Helpers

function getPadding(padding) {
  let p = typeof padding === 'number'? [padding] : padding;

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
 * Checks whether a string is a valid timing-function value
 * @param {String} value
 * @return {Boolean}
 */
function isValidTimingFunction(value) {
  // Either it's a predefined value
  if (value in PREDEFINED) {
    return true;
  }

  // Or it has to match a cubic-bezier expression
  if (value.match(/^cubic-bezier\(([0-9.\- ]+,){3}[0-9.\- ]+\)/)) {
    return true;
  }

  return false;
}
