/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { CanvasFrameAnonymousContentHelper,
        createSVGNode, createNode, getComputedStyle } = require("./utils/markup");
const { setIgnoreLayoutChanges } = require("devtools/shared/layout/utils");
const { AutoRefreshHighlighter } = require("./auto-refresh");

// We use this as an offset to avoid the marker itself from being on top of its shadow.
const MARKER_SIZE = 10;

/**
 * The ShapesHighlighter draws an outline shapes in the page.
 * The idea is to have something that is able to wrap complex shapes for css properties
 * such as shape-outside/inside, clip-path but also SVG elements.
 */
class ShapesHighlighter extends AutoRefreshHighlighter {
  constructor(highlighterEnv) {
    super(highlighterEnv);

    this.ID_CLASS_PREFIX = "shapes-";

    this.markup = new CanvasFrameAnonymousContentHelper(this.highlighterEnv,
    this._buildMarkup.bind(this));
  }

  _buildMarkup() {
    let container = createNode(this.win, {
      attributes: {
        "class": "highlighter-container"
      }
    });

    // The root wrapper is used to unzoom the highlighter when needed.
    let rootWrapper = createNode(this.win, {
      parent: container,
      attributes: {
        "id": "root",
        "class": "root"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    let mainSvg = createSVGNode(this.win, {
      nodeType: "svg",
      parent: rootWrapper,
      attributes: {
        "id": "shape-container",
        "class": "shape-container",
        "viewBox": "0 0 100 100",
        "preserveAspectRatio": "none"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // We also need a separate element to draw the shapes' points markers. We can't use
    // the SVG because it is scaled.
    createNode(this.win, {
      nodeType: "div",
      parent: rootWrapper,
      attributes: {
        "id": "markers-container",
        "class": "markers-container"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // Append a polygon for polygon shapes.
    createSVGNode(this.win, {
      nodeType: "polygon",
      parent: mainSvg,
      attributes: {
        "id": "polygon",
        "class": "polygon",
        "hidden": "true"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // Append an ellipse for circle/ellipse shapes.
    createSVGNode(this.win, {
      nodeType: "ellipse",
      parent: mainSvg,
      attributes: {
        "id": "ellipse",
        "class": "ellipse",
        "hidden": true
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // Append a rect for inset().
    createSVGNode(this.win, {
      nodeType: "rect",
      parent: mainSvg,
      attributes: {
        "id": "rect",
        "class": "rect",
        "hidden": true
      },
      prefix: this.ID_CLASS_PREFIX
    });

    return container;
  }

  get currentDimensions() {
    return {
      width: this.currentQuads.border[0].bounds.width,
      height: this.currentQuads.border[0].bounds.height
    };
  }

  /**
   * Parses the CSS definition given and returns the shape type associated
   * with the definition and the coordinates necessary to draw the shape.
   * @param {String} definition the input CSS definition
   * @returns {Object} null if the definition is not of a known shape type,
   *          or an object of the type { shapeType, coordinates }, where
   *          shapeType is the name of the shape and coordinates are an array
   *          or object of the coordinates needed to draw the shape.
   */
  _parseCSSShapeValue(definition) {
    const types = [{
      name: "polygon",
      prefix: "polygon(",
      coordParser: this.polygonPoints.bind(this)
    }, {
      name: "circle",
      prefix: "circle(",
      coordParser: this.circlePoints.bind(this)
    }, {
      name: "ellipse",
      prefix: "ellipse(",
      coordParser: this.ellipsePoints.bind(this)
    }, {
      name: "inset",
      prefix: "inset(",
      coordParser: this.insetPoints.bind(this)
    }];

    for (let { name, prefix, coordParser } of types) {
      if (definition.includes(prefix)) {
        definition = definition.substring(prefix.length, definition.length - 1);
        return {
          shapeType: name,
          coordinates: coordParser(definition)
        };
      }
    }

    return null;
  }

  /**
   * Parses the definition of the CSS polygon() function and returns its points,
   * converted to percentages.
   * @param {String} definition the arguments of the polygon() function
   * @returns {Array} an array of the points of the polygon, with all values
   *          evaluated and converted to percentages
   */
  polygonPoints(definition) {
    return definition.split(",").map(coords => {
      return splitCoords(coords).map(this.convertCoordsToPercent.bind(this));
    });
  }

  /**
   * Parses the definition of the CSS circle() function and returns the x/y radiuses and
   * center coordinates, converted to percentages.
   * @param {String} definition the arguments of the circle() function
   * @returns {Object} an object of the form { rx, ry, cx, cy }, where rx and ry are the
   *          radiuses for the x and y axes, and cx and cy are the x/y coordinates for the
   *          center of the circle. All values are evaluated and converted to percentages.
   */
  circlePoints(definition) {
    // The computed value of circle() always has the keyword "at".
    let values = definition.split(" at ");
    let radius = values[0];
    let elemWidth = this.currentDimensions.width;
    let elemHeight = this.currentDimensions.height;
    let center = splitCoords(values[1]).map(this.convertCoordsToPercent.bind(this));

    if (radius === "closest-side") {
      // radius is the distance from center to closest side of reference box
      radius = Math.min(center[0], center[1], 100 - center[0], 100 - center[1]);
    } else if (radius === "farthest-side") {
      // radius is the distance from center to farthest side of reference box
      radius = Math.max(center[0], center[1], 100 - center[0], 100 - center[1]);
    } else {
      // radius is a % or px value
      radius = coordToPercent(radius, Math.max(elemWidth, elemHeight));
    }

    // Percentage values for circle() are resolved from the
    // used width and height of the reference box as sqrt(width^2+height^2)/sqrt(2).
    // Scale both radiusX and radiusY to match the radius computed
    // using the above equation.
    let computedSize = Math.sqrt((elemWidth ** 2) + (elemHeight ** 2)) / Math.sqrt(2);
    let ratioX = elemWidth / computedSize;
    let ratioY = elemHeight / computedSize;
    let radiusX = radius / ratioX;
    let radiusY = radius / ratioY;

    // rx, ry, cx, ry
    return { rx: radiusX, ry: radiusY, cx: center[0], cy: center[1] };
  }

  /**
   * Parses the definition of the CSS ellipse() function and returns the x/y radiuses and
   * center coordinates, converted to percentages.
   * @param {String} definition the arguments of the ellipse() function
   * @returns {Object} an object of the form { rx, ry, cx, cy }, where rx and ry are the
   *          radiuses for the x and y axes, and cx and cy are the x/y coordinates for the
   *          center of the ellipse. All values are evaluated and converted to percentages
   */
  ellipsePoints(definition) {
    let values = definition.split(" at ");
    let elemWidth = this.currentDimensions.width;
    let elemHeight = this.currentDimensions.height;
    let center = splitCoords(values[1]).map(this.convertCoordsToPercent.bind(this));

    let radii = values[0].trim().split(" ").map((radius, i) => {
      let size = i % 2 === 0 ? elemWidth : elemHeight;
      if (radius === "closest-side") {
        // radius is the distance from center to closest x/y side of reference box
        return i % 2 === 0 ? Math.min(center[0], 100 - center[0])
                           : Math.min(center[1], 100 - center[1]);
      } else if (radius === "farthest-side") {
        // radius is the distance from center to farthest x/y side of reference box
        return i % 2 === 0 ? Math.max(center[0], 100 - center[0])
                           : Math.max(center[1], 100 - center[1]);
      }
      return coordToPercent(radius, size);
    });

    return { rx: radii[0], ry: radii[1], cx: center[0], cy: center[1] };
  }

  /**
   * Parses the definition of the CSS inset() function and returns the x/y offsets and
   * width/height of the shape, converted to percentages. Border radiuses (given after
   * "round" in the definition) are currently ignored.
   * @param {String} definition the arguments of the inset() function
   * @returns {Object} an object of the form { x, y, width, height }, which are the top/
   *          left positions and width/height of the shape.
   */
  insetPoints(definition) {
    let values = definition.split(" round ");
    let offsets = splitCoords(values[0]).map(this.convertCoordsToPercent.bind(this));

    let x, y = 0;
    let width = this.currentDimensions.width;
    let height = this.currentDimensions.height;
    // The offsets, like margin/padding/border, are in order: top, right, bottom, left.
    if (offsets.length === 1) {
      x = y = offsets[0];
      width = height = 100 - 2 * x;
    } else if (offsets.length === 2) {
      y = offsets[0];
      x = offsets[1];
      height = 100 - 2 * y;
      width = 100 - 2 * x;
    } else if (offsets.length === 3) {
      y = offsets[0];
      x = offsets[1];
      height = 100 - y - offsets[2];
      width = 100 - 2 * x;
    } else if (offsets.length === 4) {
      y = offsets[0];
      x = offsets[3];
      height = 100 - y - offsets[2];
      width = 100 - x - offsets[1];
    }

    return { x, y, width, height };
  }

  convertCoordsToPercent(coord, i) {
    let elemWidth = this.currentDimensions.width;
    let elemHeight = this.currentDimensions.height;
    let size = i % 2 === 0 ? elemWidth : elemHeight;
    if (coord.includes("calc(")) {
      return evalCalcExpression(coord.substring(5, coord.length - 1), size);
    }
    return coordToPercent(coord, size);
  }

  /**
   * Destroy the nodes. Remove listeners.
   */
  destroy() {
    AutoRefreshHighlighter.prototype.destroy.call(this);
    this.markup.destroy();
  }

  /**
   * Get the element in the highlighter markup with the given id
   * @param {String} id
   * @returns {Object} the element with the given id
   */
  getElement(id) {
    return this.markup.getElement(this.ID_CLASS_PREFIX + id);
  }

  /**
   * Show the highlighter on a given node
   */
  _show() {
    return this._update();
  }

  /**
   * The AutoRefreshHighlighter's _hasMoved method returns true only if the element's
   * quads have changed. Override it so it also returns true if the element's shape has
   * changed (which can happen when you change a CSS properties for instance).
   */
  _hasMoved() {
    let hasMoved = AutoRefreshHighlighter.prototype._hasMoved.call(this);

    let oldShapeCoordinates = JSON.stringify(this.coordinates);

    // TODO: need other modes too.
    if (this.options.mode.startsWith("css")) {
      let property = shapeModeToCssPropertyName(this.options.mode);
      let style = getComputedStyle(this.currentNode)[property];

      if (!style || style === "none") {
        this.coordinates = [];
        this.shapeType = "none";
      } else {
        let { coordinates, shapeType } = this._parseCSSShapeValue(style);
        this.coordinates = coordinates;
        this.shapeType = shapeType;
      }
    }

    let newShapeCoordinates = JSON.stringify(this.coordinates);

    return hasMoved || oldShapeCoordinates !== newShapeCoordinates;
  }

  /**
   * Hide all elements used to highlight CSS different shapes.
   */
  _hideShapes() {
    this.getElement("ellipse").setAttribute("hidden", true);
    this.getElement("polygon").setAttribute("hidden", true);
    this.getElement("rect").setAttribute("hidden", true);
  }

  /**
   * Update the highlighter for the current node. Called whenever the element's quads
   * or CSS shape has changed.
   * @returns {Boolean} whether the highlighter was successfully updated
   */
  _update() {
    setIgnoreLayoutChanges(true);

    let { top, left, width, height } = this.currentQuads.border[0].bounds;

    // Size the SVG like the current node.
    this.getElement("shape-container").setAttribute("style",
      `top:${top}px;left:${left}px;width:${width}px;height:${height}px;`);

    this._hideShapes();
    this.getElement("markers-container").setAttribute("style", "");

    if (this.shapeType === "polygon") {
      this._updatePolygonShape(top, left, width, height);
    } else if (this.shapeType === "circle") {
      this._updateCircleShape(top, left, width, height);
    } else if (this.shapeType === "ellipse") {
      this._updateEllipseShape(top, left, width, height);
    } else if (this.shapeType === "inset") {
      this._updateInsetShape(top, left, width, height);
    }

    setIgnoreLayoutChanges(false, this.highlighterEnv.window.document.documentElement);

    return true;
  }

  /**
   * Update the SVG polygon to fit the CSS polygon.
   * @param {Number} top the top bound of the element quads
   * @param {Number} left the left bound of the element quads
   * @param {Number} width the width of the element quads
   * @param {Number} height the height of the element quads
   */
  _updatePolygonShape(top, left, width, height) {
    // Draw and show the polygon.
    let points = this.coordinates.map(point => point.join(",")).join(" ");

    let polygonEl = this.getElement("polygon");
    polygonEl.setAttribute("points", points);
    polygonEl.removeAttribute("hidden");

    // Draw the points themselves, using the markers-container and multiple box-shadows.
    let shadows = this.coordinates.map(([x, y]) => {
      return `${MARKER_SIZE + x * width / 100}px ${MARKER_SIZE + y * height / 100}px 0 0`;
    }).join(", ");

    this.getElement("markers-container").setAttribute("style",
      `top:${top - MARKER_SIZE}px;left:${left - MARKER_SIZE}px;box-shadow:${shadows};`);
  }

  /**
   * Update the SVG ellipse to fit the CSS circle.
   * @param {Number} top the top bound of the element quads
   * @param {Number} left the left bound of the element quads
   * @param {Number} width the width of the element quads
   * @param {Number} height the height of the element quads
   */
  _updateCircleShape(top, left, width, height) {
    let { rx, ry, cx, cy } = this.coordinates;
    let ellipseEl = this.getElement("ellipse");
    ellipseEl.setAttribute("rx", rx);
    ellipseEl.setAttribute("ry", ry);
    ellipseEl.setAttribute("cx", cx);
    ellipseEl.setAttribute("cy", cy);
    ellipseEl.removeAttribute("hidden");

    let shadows = `${MARKER_SIZE + cx * width / 100}px
      ${MARKER_SIZE + cy * height / 100}px 0 0`;

    this.getElement("markers-container").setAttribute("style",
      `top:${top - MARKER_SIZE}px;left:${left - MARKER_SIZE}px;box-shadow:${shadows};`);
  }

  /**
   * Update the SVG ellipse to fit the CSS ellipse.
   * @param {Number} top the top bound of the element quads
   * @param {Number} left the left bound of the element quads
   * @param {Number} width the width of the element quads
   * @param {Number} height the height of the element quads
   */
  _updateEllipseShape(top, left, width, height) {
    let { rx, ry, cx, cy } = this.coordinates;
    let ellipseEl = this.getElement("ellipse");
    ellipseEl.setAttribute("rx", rx);
    ellipseEl.setAttribute("ry", ry);
    ellipseEl.setAttribute("cx", cx);
    ellipseEl.setAttribute("cy", cy);
    ellipseEl.removeAttribute("hidden");

    let shadows = `${MARKER_SIZE + cx * width / 100}px
      ${MARKER_SIZE + cy * height / 100}px 0 0,
      ${MARKER_SIZE + (cx + rx) * height / 100}px
      ${MARKER_SIZE + cy * height / 100}px 0 0,
      ${MARKER_SIZE + cx * height / 100}px
      ${MARKER_SIZE + (cy + ry) * height / 100}px 0 0`;

    this.getElement("markers-container").setAttribute("style",
      `top:${top - MARKER_SIZE}px;left:${left - MARKER_SIZE}px;box-shadow:${shadows};`);
  }

  /**
   * Update the SVG rect to fit the CSS inset.
   * @param {Number} top the top bound of the element quads
   * @param {Number} left the left bound of the element quads
   * @param {Number} width the width of the element quads
   * @param {Number} height the height of the element quads
   */
  _updateInsetShape(top, left, width, height) {
    let rectEl = this.getElement("rect");
    rectEl.setAttribute("x", this.coordinates.x);
    rectEl.setAttribute("y", this.coordinates.y);
    rectEl.setAttribute("width", this.coordinates.width);
    rectEl.setAttribute("height", this.coordinates.height);
    rectEl.removeAttribute("hidden");

    this.getElement("markers-container").setAttribute("style",
      `top:${top - MARKER_SIZE}px;left:${left - MARKER_SIZE}px;box-shadow:none;`);
  }

  /**
   * Hide the highlighter, the outline and the infobar.
   */
  _hide() {
    setIgnoreLayoutChanges(true);

    this._hideShapes();
    this.getElement("markers-container").setAttribute("style", "");

    setIgnoreLayoutChanges(false, this.highlighterEnv.window.document.documentElement);
  }
}

/**
 * Split coordinate pairs separated by a space and return an array.
 * @param {String} coords the coordinate pair, where each coord is separated by a space.
 * @returns {Array} a 2 element array containing the coordinates.
 */
function splitCoords(coords) {
  // All coordinate pairs are of the form "x y" where x and y are values or
  // calc() expressions. calc() expressions have " + " in them, so replace
  // those with "+" before splitting with " " to get the proper coord pair.
  return coords.trim().replace(/ \+ /g, "+").split(" ");
}

/**
 * Convert a coordinate to a percentage value.
 * @param {String} coord a single coordinate
 * @param {Number} size the size of the element (width or height) that the percentages
 *        are relative to
 * @returns {Number} the coordinate as a percentage value
 */
function coordToPercent(coord, size) {
  if (coord.includes("%")) {
    // Just remove the % sign, nothing else to do, we're in a viewBox that's 100%
    // worth.
    return parseFloat(coord.replace("%", ""));
  } else if (coord.includes("px")) {
    // Convert the px value to a % value.
    let px = parseFloat(coord.replace("px", ""));
    return px * 100 / size;
  }

  // Unit-less value, so 0.
  return 0;
}

/**
 * Evaluates a CSS calc() expression (only handles addition)
 * @param {String} expression the arguments to the calc() function
 * @param {Number} size the size of the element (width or height) that percentage values
 *        are relative to
 * @returns {Number} the result of the expression as a percentage value
 */
function evalCalcExpression(expression, size) {
  // the calc() values returned by getComputedStyle only have addition, as it
  // computes calc() expressions as much as possible without resolving percentages,
  // leaving only addition.
  let values = expression.split("+").map(v => v.trim());

  return values.reduce((prev, curr) => {
    return prev + coordToPercent(curr, size);
  }, 0);
}

/**
 * Converts a shape mode to the proper CSS property name.
 * @param {String} mode the mode of the CSS shape
 * @returns the equivalent CSS property name
 */
const shapeModeToCssPropertyName = mode => {
  let property = mode.substring(3);
  return property.substring(0, 1).toLowerCase() + property.substring(1);
};

exports.ShapesHighlighter = ShapesHighlighter;

// Export helper functions so they can be tested
exports.splitCoords = splitCoords;
exports.coordToPercent = coordToPercent;
exports.evalCalcExpression = evalCalcExpression;
exports.shapeModeToCssPropertyName = shapeModeToCssPropertyName;
