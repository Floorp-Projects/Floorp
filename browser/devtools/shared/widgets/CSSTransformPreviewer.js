/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * The CSSTransformPreview module displays, using a <canvas> a rectangle, with
 * a given width and height and its transformed version, given a css transform
 * property and origin. It also displays arrows from/to each corner.
 *
 * It is useful to visualize how a css transform affected an element. It can
 * help debug tricky transformations. It is used today in a tooltip, and this
 * tooltip is shown when hovering over a css transform declaration in the rule
 * and computed view panels.
 *
 * TODO: For now, it multiplies matrices itself to calculate the coordinates of
 * the transformed box, but that should be removed as soon as we can get access
 * to getQuads().
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";

/**
 * The TransformPreview needs an element to output a canvas tag.
 *
 * Usage example:
 *
 * let t = new CSSTransformPreviewer(myRootElement);
 * t.preview("rotate(45deg)", "top left", 200, 400);
 * t.preview("skew(19deg)", "center", 100, 500);
 * t.preview("matrix(1, -0.2, 0, 1, 0, 0)");
 * t.destroy();
 *
 * @param {nsIDOMElement} parentEl
 *        Where the canvas will go
 */
function CSSTransformPreviewer(parentEl) {
  this.parentEl = parentEl;
  this.doc = this.parentEl.ownerDocument;
  this.canvas = null;
  this.ctx = null;
}

module.exports.CSSTransformPreviewer = CSSTransformPreviewer;

CSSTransformPreviewer.prototype = {
  /**
   * The preview look-and-feel can be changed using these properties
   */
  MAX_DIM: 250,
  PAD: 5,
  ORIGINAL_FILL: "#1F303F",
  ORIGINAL_STROKE: "#B2D8FF",
  TRANSFORMED_FILL: "rgba(200, 200, 200, .5)",
  TRANSFORMED_STROKE: "#B2D8FF",
  ARROW_STROKE: "#329AFF",
  ORIGIN_STROKE: "#329AFF",
  ARROW_TIP_HEIGHT: 10,
  ARROW_TIP_WIDTH: 8,
  CORNER_SIZE_RATIO: 6,

  /**
   * Destroy removes the canvas from the parentelement passed in the constructor
   */
  destroy: function() {
    if (this.canvas) {
      this.parentEl.removeChild(this.canvas);
    }
    if (this._hiddenDiv) {
      this.parentEl.removeChild(this._hiddenDiv);
    }
    this.parentEl = this.canvas = this.ctx = this.doc = null;
  },

  _createMarkup: function() {
    this.canvas = this.doc.createElementNS(HTML_NS, "canvas");

    this.canvas.setAttribute("id", "canvas");
    this.canvas.setAttribute("width", this.MAX_DIM);
    this.canvas.setAttribute("height", this.MAX_DIM);
    this.canvas.style.position = "relative";
    this.parentEl.appendChild(this.canvas);

    this.ctx = this.canvas.getContext("2d");
  },

  _getComputed: function(name, value, width, height) {
    if (!this._hiddenDiv) {
      // Create a hidden element to apply the style to
      this._hiddenDiv = this.doc.createElementNS(HTML_NS, "div");
      this._hiddenDiv.style.visibility = "hidden";
      this._hiddenDiv.style.position = "absolute";
      this.parentEl.appendChild(this._hiddenDiv);
    }

    // Camelcase the name
    name = name.replace(/-([a-z]{1})/g, (m, letter) => letter.toUpperCase());

    // Apply width and height to make sure computation is made correctly
    this._hiddenDiv.style.width = width + "px";
    this._hiddenDiv.style.height = height + "px";

    // Show the hidden div, apply the style, read the computed style, hide the
    // hidden div again
    this._hiddenDiv.style.display = "block";
    this._hiddenDiv.style[name] = value;
    let computed = this.doc.defaultView.getComputedStyle(this._hiddenDiv);
    let computedValue = computed[name];
    this._hiddenDiv.style.display = "none";

    return computedValue;
  },

  _getMatrixFromTransformString: function(transformStr) {
    let matrix = transformStr.substring(0, transformStr.length - 1).
      substring(transformStr.indexOf("(") + 1).split(",");

    matrix.forEach(function(value, index) {
      matrix[index] = parseFloat(value, 10);
    });

    let transformMatrix = null;

    if (matrix.length === 6) {
      // 2d transform
      transformMatrix = [
        [matrix[0], matrix[2], matrix[4], 0],
        [matrix[1], matrix[3], matrix[5], 0],
        [0,     0,     1,     0],
        [0,     0,     0,     1]
      ];
    } else {
      // 3d transform
      transformMatrix = [
        [matrix[0], matrix[4], matrix[8],  matrix[12]],
        [matrix[1], matrix[5], matrix[9],  matrix[13]],
        [matrix[2], matrix[6], matrix[10], matrix[14]],
        [matrix[3], matrix[7], matrix[11], matrix[15]]
      ];
    }

    return transformMatrix;
  },

  _getOriginFromOriginString: function(originStr) {
    let offsets = originStr.split(" ");
    offsets.forEach(function(item, index) {
      offsets[index] = parseInt(item, 10);
    });

    return offsets;
  },

  _multiply: function(m1, m2) {
    let m = [];
    for (let m1Line = 0; m1Line < m1.length; m1Line++) {
      m[m1Line] = 0;
      for (let m2Col = 0; m2Col < m2.length; m2Col++) {
        m[m1Line] += m1[m1Line][m2Col] * m2[m2Col];
      }
    }
    return [m[0], m[1]];
  },

  _getTransformedPoint: function(matrix, point, origin) {
    let pointMatrix = [point[0] - origin[0], point[1] - origin[1], 1, 1];
    return this._multiply(matrix, pointMatrix);
  },

  _getTransformedPoints: function(matrix, rect, origin) {
    return rect.map(point => {
      let tPoint = this._getTransformedPoint(matrix, [point[0], point[1]], origin);
      return [tPoint[0] + origin[0], tPoint[1] + origin[1]];
    });
  },

  /**
   * For canvas to avoid anti-aliasing
   */
  _round: x => Math.round(x) + .5,

  _drawShape: function(points, fillStyle, strokeStyle) {
    this.ctx.save();

    this.ctx.lineWidth = 1;
    this.ctx.strokeStyle = strokeStyle;
    this.ctx.fillStyle = fillStyle;

    this.ctx.beginPath();
    this.ctx.moveTo(this._round(points[0][0]), this._round(points[0][1]));
    for (var i = 1; i < points.length; i++) {
      this.ctx.lineTo(this._round(points[i][0]), this._round(points[i][1]));
    }
    this.ctx.lineTo(this._round(points[0][0]), this._round(points[0][1]));
    this.ctx.fill();
    this.ctx.stroke();

    this.ctx.restore();
  },

  _drawArrow: function(x1, y1, x2, y2) {
    // do not draw if the line is too small
    if (Math.abs(x2-x1) < 20 && Math.abs(y2-y1) < 20) {
      return;
    }

    this.ctx.save();

    this.ctx.strokeStyle = this.ARROW_STROKE;
    this.ctx.fillStyle = this.ARROW_STROKE;
    this.ctx.lineWidth = 1;

    this.ctx.beginPath();
    this.ctx.moveTo(this._round(x1), this._round(y1));
    this.ctx.lineTo(this._round(x2), this._round(y2));
    this.ctx.stroke();

    this.ctx.beginPath();
    this.ctx.translate(x2, y2);
    let radians = Math.atan((y1 - y2) / (x1 - x2));
    radians += ((x1 >= x2) ? -90 : 90) * Math.PI / 180;
    this.ctx.rotate(radians);
    this.ctx.moveTo(0, 0);
    this.ctx.lineTo(this.ARROW_TIP_WIDTH / 2, this.ARROW_TIP_HEIGHT);
    this.ctx.lineTo(-this.ARROW_TIP_WIDTH / 2, this.ARROW_TIP_HEIGHT);
    this.ctx.closePath();
    this.ctx.fill();

    this.ctx.restore();
  },

  _drawOrigin: function(x, y) {
    this.ctx.save();

    this.ctx.strokeStyle = this.ORIGIN_STROKE;
    this.ctx.fillStyle = this.ORIGIN_STROKE;

    this.ctx.beginPath();
    this.ctx.arc(x, y, 4, 0, 2 * Math.PI, false);
    this.ctx.stroke();
    this.ctx.fill();

    this.ctx.restore();
  },

  /**
   * Computes the largest width and height of all the given shapes and changes
   * all of the shapes' points (by reference) so they fit into the configured
   * MAX_DIM - 2*PAD area.
   * @return {Object} A {w, h} giving the size the canvas should be
   */
  _fitAllShapes: function(allShapes) {
    let allXs = [], allYs = [];
    for (let shape of allShapes) {
      for (let point of shape) {
        allXs.push(point[0]);
        allYs.push(point[1]);
      }
    }
    let minX = Math.min.apply(Math, allXs);
    let maxX = Math.max.apply(Math, allXs);
    let minY = Math.min.apply(Math, allYs);
    let maxY = Math.max.apply(Math, allYs);

    let spanX = maxX - minX;
    let spanY = maxY - minY;
    let isWide = spanX > spanY;

    let cw = isWide ? this.MAX_DIM :
      this.MAX_DIM * Math.min(spanX, spanY) / Math.max(spanX, spanY);
    let ch = !isWide ? this.MAX_DIM :
      this.MAX_DIM * Math.min(spanX, spanY) / Math.max(spanX, spanY);

    let mapX = x => this.PAD + ((cw - 2 * this.PAD) / (maxX - minX)) * (x - minX);
    let mapY = y => this.PAD + ((ch - 2 * this.PAD) / (maxY - minY)) * (y - minY);

    for (let shape of allShapes) {
      for (let point of shape) {
        point[0] = mapX(point[0]);
        point[1] = mapY(point[1]);
      }
    }

    return {w: cw, h: ch};
  },

  _drawShapes: function(shape, corner, transformed, transformedCorner) {
    this._drawOriginal(shape);
    this._drawOriginalCorner(corner);
    this._drawTransformed(transformed);
    this._drawTransformedCorner(transformedCorner);
  },

  _drawOriginal: function(points) {
    this._drawShape(points, this.ORIGINAL_FILL, this.ORIGINAL_STROKE);
  },

  _drawTransformed: function(points) {
    this._drawShape(points, this.TRANSFORMED_FILL, this.TRANSFORMED_STROKE);
  },

  _drawOriginalCorner: function(points) {
    this._drawShape(points, this.ORIGINAL_STROKE, this.ORIGINAL_STROKE);
  },

  _drawTransformedCorner: function(points) {
    this._drawShape(points, this.TRANSFORMED_STROKE, this.TRANSFORMED_STROKE);
  },

  _drawArrows: function(shape, transformed) {
    this._drawArrow(shape[0][0], shape[0][1], transformed[0][0], transformed[0][1]);
    this._drawArrow(shape[1][0], shape[1][1], transformed[1][0], transformed[1][1]);
    this._drawArrow(shape[2][0], shape[2][1], transformed[2][0], transformed[2][1]);
    this._drawArrow(shape[3][0], shape[3][1], transformed[3][0], transformed[3][1]);
  },

  /**
   * Draw a transform preview
   *
   * @param {String} transform
   *        The css transform value as a string, as typed by the user, as long
   *        as it can be computed by the browser
   * @param {String} origin
   *        Same as above for the transform-origin value. Defaults to "center"
   * @param {Number} width
   *        The width of the container. Defaults to 200
   * @param {Number} height
   *        The height of the container. Defaults to 200
   * @return {Boolean} Whether or not the preview could be created. Will return
   *         false for instance if the transform is invalid
   */
  preview: function(transform, origin="center", width=200, height=200) {
    // Create/clear the canvas
    if (!this.canvas) {
      this._createMarkup();
    }
    this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);

    // Get computed versions of transform and origin
    transform = this._getComputed("transform", transform, width, height);
    if (transform && transform !== "none") {
      origin = this._getComputed("transform-origin", origin, width, height);

      // Get the matrix, origin and width height data for the previewed element
      let originData = this._getOriginFromOriginString(origin);
      let matrixData = this._getMatrixFromTransformString(transform);

      // Compute the original box rect and transformed box rect
      let shapePoints = [
        [0, 0],
        [width, 0],
        [width, height],
        [0, height]
      ];
      let transformedPoints = this._getTransformedPoints(matrixData, shapePoints, originData);

      // Do the same for the corner triangle shape
      let cornerSize = Math.min(shapePoints[2][1] - shapePoints[1][1],
        shapePoints[1][0] - shapePoints[0][0]) / this.CORNER_SIZE_RATIO;
      let cornerPoints = [
        [shapePoints[1][0], shapePoints[1][1]],
        [shapePoints[1][0], shapePoints[1][1] + cornerSize],
        [shapePoints[1][0] - cornerSize, shapePoints[1][1]]
      ];
      let transformedCornerPoints = this._getTransformedPoints(matrixData, cornerPoints, originData);

      // Resize points to fit everything in the canvas
      let {w, h} = this._fitAllShapes([
        shapePoints,
        transformedPoints,
        cornerPoints,
        transformedCornerPoints,
        [originData]
      ]);

      this.canvas.setAttribute("width", w);
      this.canvas.setAttribute("height", h);

      this._drawShapes(shapePoints, cornerPoints, transformedPoints, transformedCornerPoints)
      this._drawArrows(shapePoints, transformedPoints);
      this._drawOrigin(originData[0], originData[1]);

      return true;
    } else {
      return false;
    }
  }
};
