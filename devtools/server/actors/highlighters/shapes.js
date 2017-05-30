/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { CanvasFrameAnonymousContentHelper,
        createSVGNode, createNode, getComputedStyle } = require("./utils/markup");
const { setIgnoreLayoutChanges, getCurrentZoom } = require("devtools/shared/layout/utils");
const { AutoRefreshHighlighter } = require("./auto-refresh");

// We use this as an offset to avoid the marker itself from being on top of its shadow.
const BASE_MARKER_SIZE = 10;

/**
 * The ShapesHighlighter draws an outline shapes in the page.
 * The idea is to have something that is able to wrap complex shapes for css properties
 * such as shape-outside/inside, clip-path but also SVG elements.
 */
class ShapesHighlighter extends AutoRefreshHighlighter {
  constructor(highlighterEnv) {
    super(highlighterEnv);

    this.ID_CLASS_PREFIX = "shapes-";

    this.referenceBox = "border";
    this.useStrokeBox = false;

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

    // Append a path to display the markers for the shape.
    createSVGNode(this.win, {
      nodeType: "path",
      parent: mainSvg,
      attributes: {
        "id": "markers",
        "class": "markers",
      },
      prefix: this.ID_CLASS_PREFIX
    });

    return container;
  }

  get currentDimensions() {
    let { top, left, width, height } = this.currentQuads[this.referenceBox][0].bounds;

    // If an SVG element has a stroke, currentQuads will return the stroke bounding box.
    // However, clip-path always uses the object bounding box unless "stroke-box" is
    // specified. So, we must calculate the object bounding box if there is a stroke
    // and "stroke-box" is not specified. stroke only applies to SVG elements, so use
    // getBBox, which only exists for SVG, to check if currentNode is an SVG element.
    if (this.currentNode.getBBox &&
        getComputedStyle(this.currentNode).stroke !== "none" && !this.useStrokeBox) {
      return getObjectBoundingBox(top, left, width, height, this.currentNode);
    }
    return { top, left, width, height };
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
    const shapeTypes = [{
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
    const geometryTypes = ["margin", "border", "padding", "content"];

    // default to border
    let referenceBox = "border";
    for (let geometry of geometryTypes) {
      if (definition.includes(geometry)) {
        referenceBox = geometry;
      }
    }
    this.referenceBox = referenceBox;

    this.useStrokeBox = definition.includes("stroke-box");

    for (let { name, prefix, coordParser } of shapeTypes) {
      if (definition.includes(prefix)) {
        // the closing paren of the shape function is always the last one in definition.
        definition = definition.substring(prefix.length, definition.lastIndexOf(")"));
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
    let zoom = getCurrentZoom(this.win);
    let elemWidth = this.currentDimensions.width / zoom;
    let elemHeight = this.currentDimensions.height / zoom;
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
    let zoom = getCurrentZoom(this.win);
    let elemWidth = this.currentDimensions.width / zoom;
    let elemHeight = this.currentDimensions.height / zoom;
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
    let zoom = getCurrentZoom(this.win);
    let elemWidth = this.currentDimensions.width / zoom;
    let elemHeight = this.currentDimensions.height / zoom;
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
    this.getElement("markers").setAttribute("d", "");
  }

  /**
   * Update the highlighter for the current node. Called whenever the element's quads
   * or CSS shape has changed.
   * @returns {Boolean} whether the highlighter was successfully updated
   */
  _update() {
    setIgnoreLayoutChanges(true);

    let { top, left, width, height } = this.currentDimensions;
    let zoom = getCurrentZoom(this.win);

    top /= zoom;
    left /= zoom;
    width /= zoom;
    height /= zoom;

    // Size the SVG like the current node.
    this.getElement("shape-container").setAttribute("style",
      `top:${top}px;left:${left}px;width:${width}px;height:${height}px;`);

    this._hideShapes();

    if (this.shapeType === "polygon") {
      this._updatePolygonShape(width, height, zoom);
    } else if (this.shapeType === "circle") {
      this._updateCircleShape(width, height, zoom);
    } else if (this.shapeType === "ellipse") {
      this._updateEllipseShape(width, height, zoom);
    } else if (this.shapeType === "inset") {
      this._updateInsetShape();
    }

    setIgnoreLayoutChanges(false, this.highlighterEnv.window.document.documentElement);

    return true;
  }

  /**
   * Update the SVG polygon to fit the CSS polygon.
   * @param {Number} width the width of the element quads
   * @param {Number} height the height of the element quads
   * @param {Number} zoom the zoom level of the window
   */
  _updatePolygonShape(width, height, zoom) {
    // Draw and show the polygon.
    let points = this.coordinates.map(point => point.join(",")).join(" ");

    let polygonEl = this.getElement("polygon");
    polygonEl.setAttribute("points", points);
    polygonEl.removeAttribute("hidden");

    this._drawMarkers(this.coordinates, width, height, zoom);
  }

  /**
   * Update the SVG ellipse to fit the CSS circle.
   * @param {Number} width the width of the element quads
   * @param {Number} height the height of the element quads
   * @param {Number} zoom the zoom level of the window
   */
  _updateCircleShape(width, height, zoom) {
    let { rx, ry, cx, cy } = this.coordinates;
    let ellipseEl = this.getElement("ellipse");
    ellipseEl.setAttribute("rx", rx);
    ellipseEl.setAttribute("ry", ry);
    ellipseEl.setAttribute("cx", cx);
    ellipseEl.setAttribute("cy", cy);
    ellipseEl.removeAttribute("hidden");

    this._drawMarkers([[cx, cy]], width, height, zoom);
  }

  /**
   * Update the SVG ellipse to fit the CSS ellipse.
   * @param {Number} width the width of the element quads
   * @param {Number} height the height of the element quads
   * @param {Number} zoom the zoom level of the window
   */
  _updateEllipseShape(width, height, zoom) {
    let { rx, ry, cx, cy } = this.coordinates;
    let ellipseEl = this.getElement("ellipse");
    ellipseEl.setAttribute("rx", rx);
    ellipseEl.setAttribute("ry", ry);
    ellipseEl.setAttribute("cx", cx);
    ellipseEl.setAttribute("cy", cy);
    ellipseEl.removeAttribute("hidden");

    let markerCoords = [ [cx, cy], [cx + rx, cy], [cx, cy + ry] ];
    this._drawMarkers(markerCoords, width, height, zoom);
  }

  /**
   * Update the SVG rect to fit the CSS inset.
   */
  _updateInsetShape() {
    let rectEl = this.getElement("rect");
    rectEl.setAttribute("x", this.coordinates.x);
    rectEl.setAttribute("y", this.coordinates.y);
    rectEl.setAttribute("width", this.coordinates.width);
    rectEl.setAttribute("height", this.coordinates.height);
    rectEl.removeAttribute("hidden");
  }

  /**
   * Draw markers for the given coordinates.
   * @param {Array} coords an array of coordinate arrays, of form [[x, y] ...]
   * @param {Number} width the width of the element markers are being drawn for
   * @param {Number} height the height of the element markers are being drawn for
   * @param {Number} zoom the zoom level of the window
   */
  _drawMarkers(coords, width, height, zoom) {
    let markers = coords.map(([x, y]) => {
      return getCirclePath(x, y, width, height, zoom);
    }).join(" ");

    this.getElement("markers").setAttribute("d", markers);
  }

  /**
   * Hide the highlighter, the outline and the infobar.
   */
  _hide() {
    setIgnoreLayoutChanges(true);

    this._hideShapes();
    this.getElement("markers").setAttribute("d", "");

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

/**
 * Get the SVG path definition for a circle with given attributes.
 * @param {Number} cx the x coordinate of the centre of the circle
 * @param {Number} cy the y coordinate of the centre of the circle
 * @param {Number} width the width of the element the circle is being drawn for
 * @param {Number} height the height of the element the circle is being drawn for
 * @param {Number} zoom the zoom level of the window the circle is drawn in
 * @returns {String} the definition of the circle in SVG path description format.
 */
const getCirclePath = (cx, cy, width, height, zoom) => {
  // We use a viewBox of 100x100 for shape-container so it's easy to position things
  // based on their percentage, but this makes it more difficult to create circles.
  // Therefor, 100px is the base size of shape-container. In order to make the markers'
  // size scale properly, we must adjust the radius based on zoom and the width/height of
  // the element being highlighted, then calculate a radius for both x/y axes based
  // on the aspect ratio of the element.
  let radius = BASE_MARKER_SIZE * (100 / Math.max(width, height)) / zoom;
  let ratio = width / height;
  let rx = (ratio > 1) ? radius : radius / ratio;
  let ry = (ratio > 1) ? radius * ratio : radius;
  // a circle is drawn as two arc lines, starting at the leftmost point of the circle.
  return `M${cx - rx},${cy}a${rx},${ry} 0 1,0 ${rx * 2},0` +
         `a${rx},${ry} 0 1,0 ${rx * -2},0`;
};

/**
 * Calculates the object bounding box for a node given its stroke bounding box.
 * @param {Number} top the y coord of the top edge of the stroke bounding box
 * @param {Number} left the x coord of the left edge of the stroke bounding box
 * @param {Number} width the width of the stroke bounding box
 * @param {Number} height the height of the stroke bounding box
 * @param {Object} node the node object
 * @returns {Object} an object of the form { top, left, width, height }, which
 *          are the top/left/width/height of the object bounding box for the node.
 */
const getObjectBoundingBox = (top, left, width, height, node) => {
  // See https://drafts.fxtf.org/css-masking-1/#stroke-bounding-box for details
  // on this algorithm. Note that we intentionally do not check "stroke-linecap".
  let strokeWidth = parseFloat(getComputedStyle(node).strokeWidth);
  let delta = strokeWidth / 2;
  let tagName = node.tagName;

  if (tagName !== "rect" && tagName !== "ellipse"
      && tagName !== "circle" && tagName !== "image") {
    if (getComputedStyle(node).strokeLinejoin === "miter") {
      let miter = getComputedStyle(node).strokeMiterlimit;
      if (miter < Math.SQRT2) {
        delta *= Math.SQRT2;
      } else {
        delta *= miter;
      }
    } else {
      delta *= Math.SQRT2;
    }
  }

  return {
    top: top + delta,
    left: left + delta,
    width: width - 2 * delta,
    height: height - 2 * delta
  };
};

exports.ShapesHighlighter = ShapesHighlighter;

// Export helper functions so they can be tested
exports.splitCoords = splitCoords;
exports.coordToPercent = coordToPercent;
exports.evalCalcExpression = evalCalcExpression;
exports.shapeModeToCssPropertyName = shapeModeToCssPropertyName;
exports.getCirclePath = getCirclePath;
