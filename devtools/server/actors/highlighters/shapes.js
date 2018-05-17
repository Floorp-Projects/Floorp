/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { CanvasFrameAnonymousContentHelper,
        createSVGNode, createNode, getComputedStyle } = require("./utils/markup");
const { setIgnoreLayoutChanges, getCurrentZoom,
        getAdjustedQuads, getFrameOffsets } = require("devtools/shared/layout/utils");
const { AutoRefreshHighlighter } = require("./auto-refresh");
const {
  getDistance,
  clickedOnEllipseEdge,
  distanceToLine,
  projection,
  clickedOnPoint
} = require("devtools/server/actors/utils/shapes-utils");
const {
  identity,
  apply,
  translate,
  multiply,
  scale,
  rotate,
  changeMatrixBase,
  getBasis
} = require("devtools/shared/layout/dom-matrix-2d");
const EventEmitter = require("devtools/shared/event-emitter");
const { getCSSStyleRules } = require("devtools/shared/inspector/css-logic");

const BASE_MARKER_SIZE = 5;
// the width of the area around highlighter lines that can be clicked, in px
const LINE_CLICK_WIDTH = 5;
const ROTATE_LINE_LENGTH = 50;
const DOM_EVENTS = ["mousedown", "mousemove", "mouseup", "dblclick"];
const _dragging = Symbol("shapes/dragging");

/**
 * The ShapesHighlighter draws an outline shapes in the page.
 * The idea is to have something that is able to wrap complex shapes for css properties
 * such as shape-outside/inside, clip-path but also SVG elements.
 *
 * Notes on shape transformation:
 *
 * When using transform mode to translate, scale, and rotate shapes, a transformation
 * matrix keeps track of the transformations done to the original shape. When the
 * highlighter is toggled on/off or between transform mode and point editing mode,
 * the transformations applied to the shape become permanent.
 *
 * While transformations are being performed on a shape, there is an "original" and
 * a "transformed" coordinate system. This is used when scaling or rotating a rotated
 * shape.
 *
 * The "original" coordinate system is the one where (0,0) is at the top left corner
 * of the page, the x axis is horizontal, and the y axis is vertical.
 *
 * The "transformed" coordinate system is the one where (0,0) is at the top left
 * corner of the current shape. The x axis follows the north edge of the shape
 * (from the northwest corner to the northeast corner) and the y axis follows
 * the west edge of the shape (from the northwest corner to the southwest corner).
 *
 * Because of rotation, the "north" and "west" edges might not actually be at the
 * top and left of the transformed shape. Imagine that the compass directions are
 * also rotated along with the shape.
 *
 * A refresher for coordinates and change of basis that may be helpful:
 * https://www.math.ubc.ca/~behrend/math221/Coords.pdf
 */
class ShapesHighlighter extends AutoRefreshHighlighter {
  constructor(highlighterEnv) {
    super(highlighterEnv);
    EventEmitter.decorate(this);

    this.ID_CLASS_PREFIX = "shapes-";

    this.referenceBox = "border";
    this.useStrokeBox = false;
    this.geometryBox = "";
    this.hoveredPoint = null;
    this.fillRule = "";
    this.numInsetPoints = 0;
    this.transformMode = false;
    this.viewport = {};

    this.markup = new CanvasFrameAnonymousContentHelper(this.highlighterEnv,
      this._buildMarkup.bind(this));
    this.onPageHide = this.onPageHide.bind(this);

    let { pageListenerTarget } = this.highlighterEnv;
    DOM_EVENTS.forEach(event => pageListenerTarget.addEventListener(event, this));
    pageListenerTarget.addEventListener("pagehide", this.onPageHide);
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

    // This clipPath and its children make sure the element quad outline
    // is only shown when the shape extends past the element quads.
    let clipSvg = createSVGNode(this.win, {
      nodeType: "clipPath",
      parent: mainSvg,
      attributes: {
        "id": "clip-path",
        "class": "clip-path",
      },
      prefix: this.ID_CLASS_PREFIX
    });

    createSVGNode(this.win, {
      nodeType: "polygon",
      parent: clipSvg,
      attributes: {
        "id": "clip-polygon",
        "class": "clip-polygon",
        "hidden": "true"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    createSVGNode(this.win, {
      nodeType: "ellipse",
      parent: clipSvg,
      attributes: {
        "id": "clip-ellipse",
        "class": "clip-ellipse",
        "hidden": true
      },
      prefix: this.ID_CLASS_PREFIX
    });

    createSVGNode(this.win, {
      nodeType: "rect",
      parent: clipSvg,
      attributes: {
        "id": "clip-rect",
        "class": "clip-rect",
        "hidden": true
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // Rectangle that displays the element quads. Only shown for shape-outside.
    // Only the parts of the rectangle's outline that overlap with the shape is shown.
    createSVGNode(this.win, {
      nodeType: "rect",
      parent: mainSvg,
      attributes: {
        "id": "quad",
        "class": "quad",
        "hidden": "true",
        "clip-path": "url(#shapes-clip-path)",
        "x": 0,
        "y": 0,
        "width": 100,
        "height": 100
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // clipPath that corresponds to the element's quads. Only applied for shape-outside.
    // This ensures only the parts of the shape that are within the element's quads are
    // outlined by a solid line.
    let shapeClipSvg = createSVGNode(this.win, {
      nodeType: "clipPath",
      parent: mainSvg,
      attributes: {
        "id": "quad-clip-path",
        "class": "quad-clip-path",
      },
      prefix: this.ID_CLASS_PREFIX
    });

    createSVGNode(this.win, {
      nodeType: "rect",
      parent: shapeClipSvg,
      attributes: {
        "id": "quad-clip",
        "class": "quad-clip",
        "x": -1,
        "y": -1,
        "width": 102,
        "height": 102
      },
      prefix: this.ID_CLASS_PREFIX
    });

    let mainGroup = createSVGNode(this.win, {
      nodeType: "g",
      parent: mainSvg,
      attributes: {
        "id": "group",
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // Append a polygon for polygon shapes.
    createSVGNode(this.win, {
      nodeType: "polygon",
      parent: mainGroup,
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
      parent: mainGroup,
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
      parent: mainGroup,
      attributes: {
        "id": "rect",
        "class": "rect",
        "hidden": true
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // Dashed versions of each shape. Only shown for the parts of the shape
    // that extends past the element's quads.
    createSVGNode(this.win, {
      nodeType: "polygon",
      parent: mainGroup,
      attributes: {
        "id": "dashed-polygon",
        "class": "polygon",
        "hidden": "true",
        "stroke-dasharray": "5, 5",
      },
      prefix: this.ID_CLASS_PREFIX
    });

    createSVGNode(this.win, {
      nodeType: "ellipse",
      parent: mainGroup,
      attributes: {
        "id": "dashed-ellipse",
        "class": "ellipse",
        "hidden": "true",
        "stroke-dasharray": "5, 5",
      },
      prefix: this.ID_CLASS_PREFIX
    });

    createSVGNode(this.win, {
      nodeType: "rect",
      parent: mainGroup,
      attributes: {
        "id": "dashed-rect",
        "class": "rect",
        "hidden": "true",
        "stroke-dasharray": "5, 5",
      },
      prefix: this.ID_CLASS_PREFIX
    });

    createSVGNode(this.win, {
      nodeType: "path",
      parent: mainGroup,
      attributes: {
        "id": "bounding-box",
        "class": "bounding-box",
        "stroke-dasharray": "5, 5",
        "hidden": true
      },
      prefix: this.ID_CLASS_PREFIX
    });

    createSVGNode(this.win, {
      nodeType: "path",
      parent: mainGroup,
      attributes: {
        "id": "rotate-line",
        "class": "rotate-line",
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // Append a path to display the markers for the shape.
    createSVGNode(this.win, {
      nodeType: "path",
      parent: mainGroup,
      attributes: {
        "id": "markers-outline",
        "class": "markers-outline",
      },
      prefix: this.ID_CLASS_PREFIX
    });

    createSVGNode(this.win, {
      nodeType: "path",
      parent: mainGroup,
      attributes: {
        "id": "markers",
        "class": "markers",
      },
      prefix: this.ID_CLASS_PREFIX
    });

    createSVGNode(this.win, {
      nodeType: "path",
      parent: mainGroup,
      attributes: {
        "id": "marker-hover",
        "class": "marker-hover",
        "hidden": true
      },
      prefix: this.ID_CLASS_PREFIX
    });

    return container;
  }

  get currentDimensions() {
    let dims = this.currentQuads[this.referenceBox][0].bounds;
    let zoom = getCurrentZoom(this.win);

    // If an SVG element has a stroke, currentQuads will return the stroke bounding box.
    // However, clip-path always uses the object bounding box unless "stroke-box" is
    // specified. So, we must calculate the object bounding box if there is a stroke
    // and "stroke-box" is not specified. stroke only applies to SVG elements, so use
    // getBBox, which only exists for SVG, to check if currentNode is an SVG element.
    if (this.currentNode.getBBox &&
        getComputedStyle(this.currentNode).stroke !== "none" && !this.useStrokeBox) {
      dims = getObjectBoundingBox(dims.top, dims.left,
        dims.width, dims.height, this.currentNode);
    }

    return {
      top: dims.top / zoom,
      left: dims.left / zoom,
      width: dims.width / zoom,
      height: dims.height / zoom
    };
  }

  get frameDimensions() {
    // In an iframe, we get the node's quads relative to the frame, instead of the parent
    // document.
    let dims = this.highlighterEnv.window.document === this.currentNode.ownerDocument ?
               this.currentQuads[this.referenceBox][0].bounds :
               getAdjustedQuads(this.currentNode.ownerGlobal,
                                this.currentNode, this.referenceBox)[0].bounds;
    let zoom = getCurrentZoom(this.win);

    // If an SVG element has a stroke, currentQuads will return the stroke bounding box.
    // However, clip-path always uses the object bounding box unless "stroke-box" is
    // specified. So, we must calculate the object bounding box if there is a stroke
    // and "stroke-box" is not specified. stroke only applies to SVG elements, so use
    // getBBox, which only exists for SVG, to check if currentNode is an SVG element.
    if (this.currentNode.getBBox &&
        getComputedStyle(this.currentNode).stroke !== "none" && !this.useStrokeBox) {
      dims = getObjectBoundingBox(dims.top, dims.left,
        dims.width, dims.height, this.currentNode);
    }

    return {
      top: dims.top / zoom,
      left: dims.left / zoom,
      width: dims.width / zoom,
      height: dims.height / zoom
    };
  }

  /**
   * Changes the appearance of the mouse cursor on the highlighter.
   *
   * Because we can't attach event handlers to individual elements in the
   * highlighter, we determine if the mouse is hovering over a point by seeing if
   * it's within 5 pixels of it. This creates a square hitbox that doesn't match
   * perfectly with the circular markers. So if we were to use the :hover
   * pseudo-class to apply changes to the mouse cursor, the cursor change would not
   * always accurately reflect whether you can interact with the point. This is
   * also the reason we have the hidden marker-hover element instead of using CSS
   * to fill in the marker.
   *
   * In addition, the cursor CSS property is applied to .shapes-root because if
   * it were attached to .shapes-marker, the cursor change no longer applies if
   * you are for example resizing the shape and your mouse goes off the point.
   * Also, if you are dragging a polygon point, the marker plays catch up to your
   * mouse position, resulting in an undesirable visual effect where the cursor
   * rapidly flickers between "grab" and "auto".
   *
   * @param {String} cursorType the name of the cursor to display
   */
  setCursor(cursorType) {
    let container = this.getElement("root");
    let style = container.getAttribute("style");
    // remove existing cursor definitions in the style
    style = style.replace(/cursor:.*?;/g, "");
    style = style.replace(/pointer-events:.*?;/g, "");
    let pointerEvents = cursorType === "auto" ? "none" : "auto";
    container.setAttribute("style",
      `${style}pointer-events:${pointerEvents};cursor:${cursorType};`);
  }

  /**
   * Set the absolute pixel offsets which define the current viewport in relation to
   * the full page size.
   *
   * If a padding value is given, inset the viewport by this value. This is used to define
   * a virtual viewport which ensures some element remains visible even when at the edges
   * of the actual viewport.
   *
   * @param {Number} padding
   *        Optional. Amount by which to inset the viewport in all directions.
   */
  setViewport(padding = 0) {
    let xOffset = 0;
    let yOffset = 0;

    // If the node exists within an iframe, get offsets for the virtual viewport so that
    // points can be dragged to the extent of the global window, outside of the iframe
    // window.
    if (this.currentNode.ownerGlobal !== this.win) {
      const win =  this.win;
      const nodeWin = this.currentNode.ownerGlobal;
      // Get bounding box of iframe document relative to global document.
      const { bounds } = nodeWin.document.getBoxQuads({ relativeTo: win.document })[0];
      xOffset = bounds.left - nodeWin.scrollX + win.scrollX;
      yOffset = bounds.top - nodeWin.scrollY + win.scrollY;
    }

    const { pageXOffset, pageYOffset } = this.win;
    const { clientHeight, clientWidth } = this.win.document.documentElement;
    const left = pageXOffset + padding - xOffset;
    const right = clientWidth + pageXOffset - padding - xOffset;
    const top = pageYOffset + padding - yOffset;
    const bottom = clientHeight + pageYOffset - padding - yOffset;
    this.viewport = { left, right, top, bottom, padding };
  }

  handleEvent(event, id) {
    // No event handling if the highlighter is hidden
    if (this.areShapesHidden()) {
      return;
    }

    let { target, type, pageX, pageY } = event;

    // For events on highlighted nodes in an iframe, when the event takes place
    // outside the iframe. Check if event target belongs to the iframe. If it doesn't,
    // adjust pageX/pageY to be relative to the iframe rather than the parent.
    let nodeDocument = this.currentNode.ownerDocument;
    if (target !== nodeDocument && target.ownerDocument !== nodeDocument) {
      let [xOffset, yOffset] = getFrameOffsets(target.ownerGlobal, this.currentNode);
      let zoom = getCurrentZoom(this.win);
      // xOffset/yOffset are relative to the viewport, so first find the top/left
      // edges of the viewport relative to the page.
      let viewportLeft = pageX - event.clientX;
      let viewportTop = pageY - event.clientY;
      // Also adjust for scrolling in the iframe.
      let { scrollTop, scrollLeft } = nodeDocument.documentElement;
      pageX -= viewportLeft + xOffset / zoom - scrollLeft;
      pageY -= viewportTop + yOffset / zoom - scrollTop;
    }

    switch (type) {
      case "pagehide":
        // If a page hide event is triggered for current window's highlighter, hide the
        // highlighter.
        if (target.defaultView === this.win) {
          this.destroy();
        }

        break;
      case "mousedown":
        if (this.transformMode) {
          this._handleTransformClick(pageX, pageY);
        } else if (this.shapeType === "polygon") {
          this._handlePolygonClick(pageX, pageY);
        } else if (this.shapeType === "circle") {
          this._handleCircleClick(pageX, pageY);
        } else if (this.shapeType === "ellipse") {
          this._handleEllipseClick(pageX, pageY);
        } else if (this.shapeType === "inset") {
          this._handleInsetClick(pageX, pageY);
        }
        event.stopPropagation();
        event.preventDefault();

        // Calculate constraints for a virtual viewport which ensures that a dragged
        // marker remains visible even at the edges of the actual viewport.
        this.setViewport(BASE_MARKER_SIZE);
        break;
      case "mouseup":
        if (this[_dragging]) {
          this[_dragging] = null;
          this._handleMarkerHover(this.hoveredPoint);
        }
        break;
      case "mousemove":
        if (!this[_dragging]) {
          this._handleMouseMoveNotDragging(pageX, pageY);
          return;
        }
        event.stopPropagation();
        event.preventDefault();

        // Set constraints for mouse position to ensure dragged marker stays in viewport.
        const { left, right, top, bottom } = this.viewport;
        pageX = Math.min(Math.max(left, pageX), right);
        pageY = Math.min(Math.max(top, pageY), bottom);

        let { point } = this[_dragging];
        if (this.transformMode) {
          this._handleTransformMove(pageX, pageY);
        } else if (this.shapeType === "polygon") {
          this._handlePolygonMove(pageX, pageY);
        } else if (this.shapeType === "circle") {
          this._handleCircleMove(point, pageX, pageY);
        } else if (this.shapeType === "ellipse") {
          this._handleEllipseMove(point, pageX, pageY);
        } else if (this.shapeType === "inset") {
          this._handleInsetMove(point, pageX, pageY);
        }
        break;
      case "dblclick":
        if (this.shapeType === "polygon" && !this.transformMode) {
          let { percentX, percentY } = this.convertPageCoordsToPercent(pageX, pageY);
          let index = this.getPolygonPointAt(percentX, percentY);
          if (index === -1) {
            this.getPolygonClickedLine(percentX, percentY);
            return;
          }

          this._deletePolygonPoint(index);
        }
        break;
    }
  }

  /**
   * Handle a mouse click in transform mode.
   * @param {Number} pageX the x coordinate of the mouse
   * @param {Number} pageY the y coordinate of the mouse
   */
  _handleTransformClick(pageX, pageY) {
    let { percentX, percentY } = this.convertPageCoordsToPercent(pageX, pageY);
    let type = this.getTransformPointAt(percentX, percentY);
    if (!type) {
      return;
    }

    if (this.shapeType === "polygon") {
      this._handlePolygonTransformClick(pageX, pageY, type);
    } else if (this.shapeType === "circle") {
      this._handleCircleTransformClick(pageX, pageY, type);
    } else if (this.shapeType === "ellipse") {
      this._handleEllipseTransformClick(pageX, pageY, type);
    } else if (this.shapeType === "inset") {
      this._handleInsetTransformClick(pageX, pageY, type);
    }
  }

  /**
   * Handle a click in transform mode while highlighting a polygon.
   * @param {Number} pageX the x coordinate of the mouse.
   * @param {Number} pageY the y coordinate of the mouse.
   * @param {String} type the type of transform handle that was clicked.
   */
  _handlePolygonTransformClick(pageX, pageY, type) {
    let { width, height } = this.currentDimensions;
    let pointsInfo = this.origCoordUnits.map(([x, y], i) => {
      let xComputed = this.origCoordinates[i][0] / 100 * width;
      let yComputed = this.origCoordinates[i][1] / 100 * height;
      let unitX = getUnit(x);
      let unitY = getUnit(y);
      let valueX = (isUnitless(x)) ? xComputed : parseFloat(x);
      let valueY = (isUnitless(y)) ? yComputed : parseFloat(y);

      let ratioX = this.getUnitToPixelRatio(unitX, width);
      let ratioY = this.getUnitToPixelRatio(unitY, height);
      return { unitX, unitY, valueX, valueY, ratioX, ratioY };
    });
    this[_dragging] = { type, pointsInfo, x: pageX, y: pageY, bb: this.boundingBox,
                        matrix: this.transformMatrix,
                        transformedBB: this.transformedBoundingBox };
    this._handleMarkerHover(this.hoveredPoint);
  }

  /**
   * Handle a click in transform mode while highlighting a circle.
   * @param {Number} pageX the x coordinate of the mouse.
   * @param {Number} pageY the y coordinate of the mouse.
   * @param {String} type the type of transform handle that was clicked.
   */
  _handleCircleTransformClick(pageX, pageY, type) {
    let { width, height } = this.currentDimensions;
    let { cx, cy } = this.origCoordUnits;
    let cxComputed = this.origCoordinates.cx / 100 * width;
    let cyComputed = this.origCoordinates.cy / 100 * height;
    let unitX = getUnit(cx);
    let unitY = getUnit(cy);
    let valueX = (isUnitless(cx)) ? cxComputed : parseFloat(cx);
    let valueY = (isUnitless(cy)) ? cyComputed : parseFloat(cy);

    let ratioX = this.getUnitToPixelRatio(unitX, width);
    let ratioY = this.getUnitToPixelRatio(unitY, height);

    let { radius } = this.origCoordinates;
    let computedSize = Math.sqrt((width ** 2) + (height ** 2)) / Math.sqrt(2);
    radius = radius / 100 * computedSize;
    let valueRad = this.origCoordUnits.radius;
    let unitRad = getUnit(valueRad);
    valueRad = (isUnitless(valueRad)) ? radius : parseFloat(valueRad);
    let ratioRad = this.getUnitToPixelRatio(unitRad, computedSize);

    this[_dragging] = { type, unitX, unitY, unitRad, valueX, valueY,
                        ratioX, ratioY, ratioRad, x: pageX, y: pageY,
                        bb: this.boundingBox, matrix: this.transformMatrix,
                        transformedBB: this.transformedBoundingBox };
  }

  /**
   * Handle a click in transform mode while highlighting an ellipse.
   * @param {Number} pageX the x coordinate of the mouse.
   * @param {Number} pageY the y coordinate of the mouse.
   * @param {String} type the type of transform handle that was clicked.
   */
  _handleEllipseTransformClick(pageX, pageY, type) {
    let { width, height } = this.currentDimensions;
    let { cx, cy } = this.origCoordUnits;
    let cxComputed = this.origCoordinates.cx / 100 * width;
    let cyComputed = this.origCoordinates.cy / 100 * height;
    let unitX = getUnit(cx);
    let unitY = getUnit(cy);
    let valueX = (isUnitless(cx)) ? cxComputed : parseFloat(cx);
    let valueY = (isUnitless(cy)) ? cyComputed : parseFloat(cy);

    let ratioX = this.getUnitToPixelRatio(unitX, width);
    let ratioY = this.getUnitToPixelRatio(unitY, height);

    let { rx, ry } = this.origCoordinates;
    rx = rx / 100 * width;
    let valueRX = this.origCoordUnits.rx;
    let unitRX = getUnit(valueRX);
    valueRX = (isUnitless(valueRX)) ? rx : parseFloat(valueRX);
    let ratioRX = (valueRX / rx) || 1;
    ry = ry / 100 * height;
    let valueRY = this.origCoordUnits.ry;
    let unitRY = getUnit(valueRY);
    valueRY = (isUnitless(valueRY)) ? ry : parseFloat(valueRY);
    let ratioRY = (valueRY / ry) || 1;

    this[_dragging] = { type, unitX, unitY, unitRX, unitRY,
                        valueX, valueY, ratioX, ratioY, ratioRX, ratioRY,
                        x: pageX, y: pageY, bb: this.boundingBox,
                        matrix: this.transformMatrix,
                        transformedBB: this.transformedBoundingBox };
  }

  /**
   * Handle a click in transform mode while highlighting an inset.
   * @param {Number} pageX the x coordinate of the mouse.
   * @param {Number} pageY the y coordinate of the mouse.
   * @param {String} type the type of transform handle that was clicked.
   */
  _handleInsetTransformClick(pageX, pageY, type) {
    let { width, height } = this.currentDimensions;
    let pointsInfo = {};
    ["top", "right", "bottom", "left"].forEach(point => {
      let value = this.origCoordUnits[point];
      let size = (point === "left" || point === "right") ? width : height;
      let computedValue = this.origCoordinates[point] / 100 * size;
      let unit = getUnit(value);
      value = (isUnitless(value)) ? computedValue : parseFloat(value);
      let ratio = this.getUnitToPixelRatio(unit, size);

      pointsInfo[point] = { value, unit, ratio };
    });
    this[_dragging] = { type, pointsInfo, x: pageX, y: pageY, bb: this.boundingBox,
                        matrix: this.transformMatrix,
                        transformedBB: this.transformedBoundingBox };
  }

  /**
   * Handle mouse movement after a click on a handle in transform mode.
   * @param {Number} pageX the x coordinate of the mouse
   * @param {Number} pageY the y coordinate of the mouse
   */
  _handleTransformMove(pageX, pageY) {
    let { type } = this[_dragging];
    if (type === "translate") {
      this._translateShape(pageX, pageY);
    } else if (type.includes("scale")) {
      this._scaleShape(pageX, pageY);
    } else if (type === "rotate" && this.shapeType === "polygon") {
      this._rotateShape(pageX, pageY);
    }

    this.transformedBoundingBox = this.calculateTransformedBoundingBox();
  }

  /**
   * Translates a shape based on the current mouse position.
   * @param {Number} pageX the x coordinate of the mouse.
   * @param {Number} pageY the y coordinate of the mouse.
   */
  _translateShape(pageX, pageY) {
    let { x, y, matrix } = this[_dragging];
    let deltaX = pageX - x;
    let deltaY = pageY - y;
    this.transformMatrix = multiply(translate(deltaX, deltaY), matrix);

    if (this.shapeType === "polygon") {
      this._transformPolygon();
    } else if (this.shapeType === "circle") {
      this._transformCircle();
    } else if (this.shapeType === "ellipse") {
      this._transformEllipse();
    } else if (this.shapeType === "inset") {
      this._transformInset();
    }
  }

  /**
   * Scales a shape according to the current mouse position.
   * @param {Number} pageX the x coordinate of the mouse.
   * @param {Number} pageY the y coordinate of the mouse.
   */
  _scaleShape(pageX, pageY) {
    /**
     * To scale a shape:
     * 1) Get the change of basis matrix corresponding to the current transformation
     *    matrix of the shape.
     * 2) Convert the mouse x/y deltas to the "transformed" coordinate system, using
     *    the change of base matrix.
     * 3) Calculate the proportion to which the shape should be scaled to, using the
     *    mouse x/y deltas and the width/height of the transformed shape.
     * 4) Translate the shape such that the anchor (the point opposite to the one
     *    being dragged) is at the top left of the element.
     * 5) Scale each point by multiplying by the scaling proportion.
     * 6) Translate the shape back such that the anchor is in its original position.
     */
    let { type, x, y, matrix } = this[_dragging];
    let { width, height } = this.currentDimensions;
    // The point opposite to the one being dragged
    let anchor = getAnchorPoint(type);

    let { ne, nw, sw } = this[_dragging].transformedBB;
    // u/v are the basis vectors of the transformed coordinate system.
    let u = [(ne[0] - nw[0]) / 100 * width, (ne[1] - nw[1]) / 100 * height];
    let v = [(sw[0] - nw[0]) / 100 * width, (sw[1] - nw[1]) / 100 * height];
    // uLength/vLength represent the width/height of the shape in the
    // transformed coordinate system.
    let { basis, invertedBasis, uLength, vLength } = getBasis(u, v);

    // How much points on each axis should be translated before scaling
    let transX = this[_dragging].transformedBB[anchor][0] / 100 * width;
    let transY = this[_dragging].transformedBB[anchor][1] / 100 * height;

    // Distance from original click to current mouse position
    let distanceX = pageX - x;
    let distanceY = pageY - y;
    // Convert from original coordinate system to transformed coordinate system
    let tDistanceX = invertedBasis[0] * distanceX + invertedBasis[1] * distanceY;
    let tDistanceY = invertedBasis[3] * distanceX + invertedBasis[4] * distanceY;

    // Proportion of distance to bounding box width/height of shape
    let proportionX = tDistanceX / uLength;
    let proportionY = tDistanceY / vLength;
    // proportionX is positive for size reductions dragging on w/nw/sw,
    // negative for e/ne/se.
    let scaleX = (type.includes("w")) ? 1 - proportionX : 1 + proportionX;
    // proportionT is positive for size reductions dragging on n/nw/ne,
    // negative for s/sw/se.
    let scaleY = (type.includes("n")) ? 1 - proportionY : 1 + proportionY;
    // Take the average of scaleX/scaleY for scaling on two axes
    let scaleXY = (scaleX + scaleY) / 2;

    let translateMatrix = translate(-transX, -transY);
    let scaleMatrix = identity();
    // The scale matrices are in the transformed coordinate system. We must convert
    // them to the original coordinate system before applying it to the transformation
    // matrix.
    if (type === "scale-e" || type === "scale-w") {
      scaleMatrix = changeMatrixBase(scale(scaleX, 1), invertedBasis, basis);
    } else if (type === "scale-n" || type === "scale-s") {
      scaleMatrix = changeMatrixBase(scale(1, scaleY), invertedBasis, basis);
    } else {
      scaleMatrix = changeMatrixBase(scale(scaleXY, scaleXY), invertedBasis, basis);
    }
    let translateBackMatrix = translate(transX, transY);
    this.transformMatrix = multiply(translateBackMatrix,
                              multiply(scaleMatrix,
                              multiply(translateMatrix, matrix)));

    if (this.shapeType === "polygon") {
      this._transformPolygon();
    } else if (this.shapeType === "circle") {
      this._transformCircle(transX);
    } else if (this.shapeType === "ellipse") {
      this._transformEllipse(transX, transY);
    } else if (this.shapeType === "inset") {
      this._transformInset();
    }
  }

  /**
   * Rotates a polygon based on the current mouse position.
   * @param {Number} pageX the x coordinate of the mouse.
   * @param {Number} pageY the y coordinate of the mouse.
   */
  _rotateShape(pageX, pageY) {
    let { matrix } = this[_dragging];
    let { center, ne, nw, sw } = this[_dragging].transformedBB;
    let { width, height } = this.currentDimensions;
    let centerX = center[0] / 100 * width;
    let centerY = center[1] / 100 * height;
    let { x: pageCenterX, y: pageCenterY } = this.convertPercentToPageCoords(...center);

    let dx = pageCenterX - pageX;
    let dy = pageCenterY - pageY;

    let u = [(ne[0] - nw[0]) / 100 * width, (ne[1] - nw[1]) / 100 * height];
    let v = [(sw[0] - nw[0]) / 100 * width, (sw[1] - nw[1]) / 100 * height];
    let { invertedBasis } = getBasis(u, v);

    let tdx = invertedBasis[0] * dx + invertedBasis[1] * dy;
    let tdy = invertedBasis[3] * dx + invertedBasis[4] * dy;
    let angle = Math.atan2(tdx, tdy);
    let translateMatrix = translate(-centerX, -centerY);
    let rotateMatrix = rotate(angle);
    let translateBackMatrix = translate(centerX, centerY);
    this.transformMatrix = multiply(translateBackMatrix,
                           multiply(rotateMatrix,
                           multiply(translateMatrix, matrix)));

    this._transformPolygon();
  }

  /**
   * Transform a polygon depending on the current transformation matrix.
   */
  _transformPolygon() {
    let { pointsInfo } = this[_dragging];

    let polygonDef = (this.fillRule) ? `${this.fillRule}, ` : "";
    polygonDef += pointsInfo.map(point => {
      let { unitX, unitY, valueX, valueY, ratioX, ratioY } = point;
      let vector = [valueX / ratioX, valueY / ratioY];
      let [newX, newY] = apply(this.transformMatrix, vector);
      newX = round(newX * ratioX, unitX);
      newY = round(newY * ratioY, unitY);

      return `${newX}${unitX} ${newY}${unitY}`;
    }).join(", ");
    polygonDef = `polygon(${polygonDef}) ${this.geometryBox}`.trim();

    this.emit("highlighter-event", { type: "shape-change", value: polygonDef });
  }

  /**
   * Transform a circle depending on the current transformation matrix.
   * @param {Number} transX the number of pixels the shape is translated on the x axis
   *                 before scaling
   */
  _transformCircle(transX = null) {
    let { unitX, unitY, unitRad, valueX, valueY,
          ratioX, ratioY, ratioRad } = this[_dragging];
    let { radius } = this.coordUnits;

    let [newCx, newCy] = apply(this.transformMatrix, [valueX / ratioX, valueY / ratioY]);
    if (transX !== null) {
      // As part of scaling, the shape is translated to be tangent to the line y=0.
      // To get the new radius, we translate the new cx back to that point and get
      // the distance to the line y=0.
      radius = round(Math.abs((newCx - transX) * ratioRad), unitRad);
      radius = `${radius}${unitRad}`;
    }

    newCx = round(newCx * ratioX, unitX);
    newCy = round(newCy * ratioY, unitY);
    let circleDef = `circle(${radius} at ${newCx}${unitX} ${newCy}${unitY})` +
        ` ${this.geometryBox}`.trim();
    this.emit("highlighter-event", { type: "shape-change", value: circleDef });
  }

  /**
   * Transform an ellipse depending on the current transformation matrix.
   * @param {Number} transX the number of pixels the shape is translated on the x axis
   *                 before scaling
   * @param {Number} transY the number of pixels the shape is translated on the y axis
   *                 before scaling
   */
  _transformEllipse(transX = null, transY = null) {
    let { unitX, unitY, unitRX, unitRY, valueX, valueY,
          ratioX, ratioY, ratioRX, ratioRY } = this[_dragging];
    let { rx, ry } = this.coordUnits;

    let [newCx, newCy] = apply(this.transformMatrix, [valueX / ratioX, valueY / ratioY]);
    if (transX !== null && transY !== null) {
      // As part of scaling, the shape is translated to be tangent to the lines y=0 & x=0.
      // To get the new radii, we translate the new center back to that point and get the
      // distances to the line x=0 and y=0.
      rx = round(Math.abs((newCx - transX) * ratioRX), unitRX);
      rx = `${rx}${unitRX}`;
      ry = round(Math.abs((newCy - transY) * ratioRY), unitRY);
      ry = `${ry}${unitRY}`;
    }

    newCx = round(newCx * ratioX, unitX);
    newCy = round(newCy * ratioY, unitY);

    let centerStr = `${newCx}${unitX} ${newCy}${unitY}`;
    let ellipseDef = `ellipse(${rx} ${ry} at ${centerStr}) ${this.geometryBox}`.trim();
    this.emit("highlighter-event", { type: "shape-change", value: ellipseDef });
  }

  /**
   * Transform an inset depending on the current transformation matrix.
   */
  _transformInset() {
    let { top, left, right, bottom } = this[_dragging].pointsInfo;
    let { width, height } = this.currentDimensions;

    let topLeft = [ left.value / left.ratio, top.value / top.ratio ];
    let [newLeft, newTop] = apply(this.transformMatrix, topLeft);
    newLeft = round(newLeft * left.ratio, left.unit);
    newLeft = `${newLeft}${left.unit}`;
    newTop = round(newTop * top.ratio, top.unit);
    newTop = `${newTop}${top.unit}`;

    // Right and bottom values are relative to the right and bottom edges of the
    // element, so convert to the value relative to the left/top edges before scaling
    // and convert back.
    let bottomRight = [ width - right.value / right.ratio,
                        height - bottom.value / bottom.ratio ];
    let [newRight, newBottom] = apply(this.transformMatrix, bottomRight);
    newRight = round((width - newRight) * right.ratio, right.unit);
    newRight = `${newRight}${right.unit}`;
    newBottom = round((height - newBottom) * bottom.ratio, bottom.unit);
    newBottom = `${newBottom}${bottom.unit}`;

    let insetDef = (this.insetRound) ?
          `inset(${newTop} ${newRight} ${newBottom} ${newLeft} round ${this.insetRound})`
          :
          `inset(${newTop} ${newRight} ${newBottom} ${newLeft})`;
    insetDef += (this.geometryBox) ? this.geometryBox : "";

    this.emit("highlighter-event", { type: "shape-change", value: insetDef });
  }

  /**
   * Handle a click when highlighting a polygon.
   * @param {Number} pageX the x coordinate of the click
   * @param {Number} pageY the y coordinate of the click
   */
  _handlePolygonClick(pageX, pageY) {
    let { width, height } = this.currentDimensions;
    let { percentX, percentY } = this.convertPageCoordsToPercent(pageX, pageY);
    let point = this.getPolygonPointAt(percentX, percentY);
    if (point === -1) {
      return;
    }

    let [x, y] = this.coordUnits[point];
    let xComputed = this.coordinates[point][0] / 100 * width;
    let yComputed = this.coordinates[point][1] / 100 * height;
    let unitX = getUnit(x);
    let unitY = getUnit(y);
    let valueX = (isUnitless(x)) ? xComputed : parseFloat(x);
    let valueY = (isUnitless(y)) ? yComputed : parseFloat(y);

    let ratioX = this.getUnitToPixelRatio(unitX, width);
    let ratioY = this.getUnitToPixelRatio(unitY, height);

    this.setCursor("grabbing");
    this[_dragging] = { point, unitX, unitY, valueX, valueY,
                        ratioX, ratioY, x: pageX, y: pageY };
  }

  /**
   * Update the dragged polygon point with the given x/y coords and update
   * the element style.
   * @param {Number} pageX the new x coordinate of the point
   * @param {Number} pageY the new y coordinate of the point
   */
  _handlePolygonMove(pageX, pageY) {
    let { point, unitX, unitY, valueX, valueY, ratioX, ratioY, x, y } = this[_dragging];
    let deltaX = (pageX - x) * ratioX;
    let deltaY = (pageY - y) * ratioY;
    let newX = round(valueX + deltaX, unitX);
    let newY = round(valueY + deltaY, unitY);

    let polygonDef = (this.fillRule) ? `${this.fillRule}, ` : "";
    polygonDef += this.coordUnits.map((coords, i) => {
      return (i === point) ?
        `${newX}${unitX} ${newY}${unitY}` : `${coords[0]} ${coords[1]}`;
    }).join(", ");
    polygonDef = `polygon(${polygonDef}) ${this.geometryBox}`.trim();

    this.emit("highlighter-event", { type: "shape-change", value: polygonDef });
  }

  /**
   * Add new point to the polygon defintion and update element style.
   * TODO: Bug 1436054 - Do not default to percentage unit when inserting new point.
   * https://bugzilla.mozilla.org/show_bug.cgi?id=1436054
   *
   * @param {Number} after the index of the point that the new point should be added after
   * @param {Number} x the x coordinate of the new point
   * @param {Number} y the y coordinate of the new point
   */
  _addPolygonPoint(after, x, y) {
    let polygonDef = (this.fillRule) ? `${this.fillRule}, ` : "";
    polygonDef += this.coordUnits.map((coords, i) => {
      return (i === after) ? `${coords[0]} ${coords[1]}, ${x}% ${y}%` :
                             `${coords[0]} ${coords[1]}`;
    }).join(", ");
    polygonDef = `polygon(${polygonDef}) ${this.geometryBox}`.trim();

    this.hoveredPoint = after + 1;
    this._emitHoverEvent(this.hoveredPoint);
    this.emit("highlighter-event", { type: "shape-change", value: polygonDef });
  }

  /**
   * Remove point from polygon defintion and update the element style.
   * @param {Number} point the index of the point to delete
   */
  _deletePolygonPoint(point) {
    let coordinates = this.coordUnits.slice();
    coordinates.splice(point, 1);
    let polygonDef = (this.fillRule) ? `${this.fillRule}, ` : "";
    polygonDef += coordinates.map((coords, i) => {
      return `${coords[0]} ${coords[1]}`;
    }).join(", ");
    polygonDef = `polygon(${polygonDef}) ${this.geometryBox}`.trim();

    this.hoveredPoint = null;
    this._emitHoverEvent(this.hoveredPoint);
    this.emit("highlighter-event", { type: "shape-change", value: polygonDef });
  }
  /**
   * Handle a click when highlighting a circle.
   * @param {Number} pageX the x coordinate of the click
   * @param {Number} pageY the y coordinate of the click
   */
  _handleCircleClick(pageX, pageY) {
    let { width, height } = this.currentDimensions;
    let { percentX, percentY } = this.convertPageCoordsToPercent(pageX, pageY);
    let point = this.getCirclePointAt(percentX, percentY);
    if (!point) {
      return;
    }

    this.setCursor("grabbing");
    if (point === "center") {
      let { cx, cy } = this.coordUnits;
      let cxComputed = this.coordinates.cx / 100 * width;
      let cyComputed = this.coordinates.cy / 100 * height;
      let unitX = getUnit(cx);
      let unitY = getUnit(cy);
      let valueX = (isUnitless(cx)) ? cxComputed : parseFloat(cx);
      let valueY = (isUnitless(cy)) ? cyComputed : parseFloat(cy);

      let ratioX = this.getUnitToPixelRatio(unitX, width);
      let ratioY = this.getUnitToPixelRatio(unitY, height);

      this[_dragging] = { point, unitX, unitY, valueX, valueY,
                          ratioX, ratioY, x: pageX, y: pageY };
    } else if (point === "radius") {
      let { radius } = this.coordinates;
      let computedSize = Math.sqrt((width ** 2) + (height ** 2)) / Math.sqrt(2);
      radius = radius / 100 * computedSize;
      let value = this.coordUnits.radius;
      let unit = getUnit(value);
      value = (isUnitless(value)) ? radius : parseFloat(value);
      let ratio = this.getUnitToPixelRatio(unit, computedSize);

      this[_dragging] = { point, value, origRadius: radius, unit, ratio };
    }
  }

  /**
   * Set the center/radius of the circle according to the mouse position and
   * update the element style.
   * @param {String} point either "center" or "radius"
   * @param {Number} pageX the x coordinate of the mouse position, in terms of %
   *        relative to the element
   * @param {Number} pageY the y coordinate of the mouse position, in terms of %
   *        relative to the element
   */
  _handleCircleMove(point, pageX, pageY) {
    let { radius, cx, cy } = this.coordUnits;

    if (point === "center") {
      let { unitX, unitY, valueX, valueY, ratioX, ratioY, x, y} = this[_dragging];
      let deltaX = (pageX - x) * ratioX;
      let deltaY = (pageY - y) * ratioY;
      let newCx = `${round(valueX + deltaX, unitX)}${unitX}`;
      let newCy = `${round(valueY + deltaY, unitY)}${unitY}`;
      // if not defined by the user, geometryBox will be an empty string; trim() cleans up
      let circleDef = `circle(${radius} at ${newCx} ${newCy}) ${this.geometryBox}`.trim();

      this.emit("highlighter-event", { type: "shape-change", value: circleDef });
    } else if (point === "radius") {
      let { value, unit, origRadius, ratio } = this[_dragging];
      // convert center point to px, then get distance between center and mouse.
      let { x: pageCx, y: pageCy } = this.convertPercentToPageCoords(this.coordinates.cx,
                                                                     this.coordinates.cy);
      let newRadiusPx = getDistance(pageCx, pageCy, pageX, pageY);

      let delta = (newRadiusPx - origRadius) * ratio;
      let newRadius = `${round(value + delta, unit)}${unit}`;

      let circleDef = `circle(${newRadius} at ${cx} ${cy}) ${this.geometryBox}`.trim();

      this.emit("highlighter-event", { type: "shape-change", value: circleDef });
    }
  }

  /**
   * Handle a click when highlighting an ellipse.
   * @param {Number} pageX the x coordinate of the click
   * @param {Number} pageY the y coordinate of the click
   */
  _handleEllipseClick(pageX, pageY) {
    let { width, height } = this.currentDimensions;
    let { percentX, percentY } = this.convertPageCoordsToPercent(pageX, pageY);
    let point = this.getEllipsePointAt(percentX, percentY);
    if (!point) {
      return;
    }

    this.setCursor("grabbing");
    if (point === "center") {
      let { cx, cy } = this.coordUnits;
      let cxComputed = this.coordinates.cx / 100 * width;
      let cyComputed = this.coordinates.cy / 100 * height;
      let unitX = getUnit(cx);
      let unitY = getUnit(cy);
      let valueX = (isUnitless(cx)) ? cxComputed : parseFloat(cx);
      let valueY = (isUnitless(cy)) ? cyComputed : parseFloat(cy);

      let ratioX = this.getUnitToPixelRatio(unitX, width);
      let ratioY = this.getUnitToPixelRatio(unitY, height);

      this[_dragging] = { point, unitX, unitY, valueX, valueY,
                          ratioX, ratioY, x: pageX, y: pageY };
    } else if (point === "rx") {
      let { rx } = this.coordinates;
      rx = rx / 100 * width;
      let value = this.coordUnits.rx;
      let unit = getUnit(value);
      value = (isUnitless(value)) ? rx : parseFloat(value);
      let ratio = this.getUnitToPixelRatio(unit, width);

      this[_dragging] = { point, value, origRadius: rx, unit, ratio };
    } else if (point === "ry") {
      let { ry } = this.coordinates;
      ry = ry / 100 * height;
      let value = this.coordUnits.ry;
      let unit = getUnit(value);
      value = (isUnitless(value)) ? ry : parseFloat(value);
      let ratio = this.getUnitToPixelRatio(unit, height);

      this[_dragging] = { point, value, origRadius: ry, unit, ratio };
    }
  }

  /**
   * Set center/rx/ry of the ellispe according to the mouse position and update the
   * element style.
   * @param {String} point "center", "rx", or "ry"
   * @param {Number} pageX the x coordinate of the mouse position, in terms of %
   *        relative to the element
   * @param {Number} pageY the y coordinate of the mouse position, in terms of %
   *        relative to the element
   */
  _handleEllipseMove(point, pageX, pageY) {
    let { percentX, percentY } = this.convertPageCoordsToPercent(pageX, pageY);
    let { rx, ry, cx, cy } = this.coordUnits;

    if (point === "center") {
      let { unitX, unitY, valueX, valueY, ratioX, ratioY, x, y} = this[_dragging];
      let deltaX = (pageX - x) * ratioX;
      let deltaY = (pageY - y) * ratioY;
      let newCx = `${round(valueX + deltaX, unitX)}${unitX}`;
      let newCy = `${round(valueY + deltaY, unitY)}${unitY}`;
      let ellipseDef =
        `ellipse(${rx} ${ry} at ${newCx} ${newCy}) ${this.geometryBox}`.trim();

      this.emit("highlighter-event", { type: "shape-change", value: ellipseDef });
    } else if (point === "rx") {
      let { value, unit, origRadius, ratio } = this[_dragging];
      let newRadiusPercent = Math.abs(percentX - this.coordinates.cx);
      let { width } = this.currentDimensions;
      let delta = ((newRadiusPercent / 100 * width) - origRadius) * ratio;
      let newRadius = `${round(value + delta, unit)}${unit}`;

      let ellipseDef =
        `ellipse(${newRadius} ${ry} at ${cx} ${cy}) ${this.geometryBox}`.trim();

      this.emit("highlighter-event", { type: "shape-change", value: ellipseDef });
    } else if (point === "ry") {
      let { value, unit, origRadius, ratio } = this[_dragging];
      let newRadiusPercent = Math.abs(percentY - this.coordinates.cy);
      let { height } = this.currentDimensions;
      let delta = ((newRadiusPercent / 100 * height) - origRadius) * ratio;
      let newRadius = `${round(value + delta, unit)}${unit}`;

      let ellipseDef =
        `ellipse(${rx} ${newRadius} at ${cx} ${cy}) ${this.geometryBox}`.trim();

      this.emit("highlighter-event", { type: "shape-change", value: ellipseDef });
    }
  }

  /**
   * Handle a click when highlighting an inset.
   * @param {Number} pageX the x coordinate of the click
   * @param {Number} pageY the y coordinate of the click
   */
  _handleInsetClick(pageX, pageY) {
    let { width, height } = this.currentDimensions;
    let { percentX, percentY } = this.convertPageCoordsToPercent(pageX, pageY);
    let point = this.getInsetPointAt(percentX, percentY);
    if (!point) {
      return;
    }

    this.setCursor("grabbing");
    let value = this.coordUnits[point];
    let size = (point === "left" || point === "right") ? width : height;
    let computedValue = this.coordinates[point] / 100 * size;
    let unit = getUnit(value);
    value = (isUnitless(value)) ? computedValue : parseFloat(value);
    let ratio = this.getUnitToPixelRatio(unit, size);
    let origValue = (point === "left" || point === "right") ? pageX : pageY;

    this[_dragging] = { point, value, origValue, unit, ratio };
  }

  /**
   * Set the top/left/right/bottom of the inset shape according to the mouse position
   * and update the element style.
   * @param {String} point "top", "left", "right", or "bottom"
   * @param {Number} pageX the x coordinate of the mouse position, in terms of %
   *        relative to the element
   * @param {Number} pageY the y coordinate of the mouse position, in terms of %
   *        relative to the element
   * @memberof ShapesHighlighter
   */
  _handleInsetMove(point, pageX, pageY) {
    let { top, left, right, bottom } = this.coordUnits;
    let { value, origValue, unit, ratio } = this[_dragging];

    if (point === "left") {
      let delta = (pageX - origValue) * ratio;
      left = `${round(value + delta, unit)}${unit}`;
    } else if (point === "right") {
      let delta = (pageX - origValue) * ratio;
      right = `${round(value - delta, unit)}${unit}`;
    } else if (point === "top") {
      let delta = (pageY - origValue) * ratio;
      top = `${round(value + delta, unit)}${unit}`;
    } else if (point === "bottom") {
      let delta = (pageY - origValue) * ratio;
      bottom = `${round(value - delta, unit)}${unit}`;
    }

    let insetDef = (this.insetRound) ?
      `inset(${top} ${right} ${bottom} ${left} round ${this.insetRound})` :
      `inset(${top} ${right} ${bottom} ${left})`;

    insetDef += (this.geometryBox) ? this.geometryBox : "";

    this.emit("highlighter-event", { type: "shape-change", value: insetDef });
  }

  _handleMouseMoveNotDragging(pageX, pageY) {
    let { percentX, percentY } = this.convertPageCoordsToPercent(pageX, pageY);
    if (this.transformMode) {
      let point = this.getTransformPointAt(percentX, percentY);
      this.hoveredPoint = point;
      this._handleMarkerHover(point);
    } else if (this.shapeType === "polygon") {
      let point = this.getPolygonPointAt(percentX, percentY);
      let oldHoveredPoint = this.hoveredPoint;
      this.hoveredPoint = (point !== -1) ? point : null;
      if (this.hoveredPoint !== oldHoveredPoint) {
        this._emitHoverEvent(this.hoveredPoint);
      }
      this._handleMarkerHover(point);
    } else if (this.shapeType === "circle") {
      let point = this.getCirclePointAt(percentX, percentY);
      let oldHoveredPoint = this.hoveredPoint;
      this.hoveredPoint = point ? point : null;
      if (this.hoveredPoint !== oldHoveredPoint) {
        this._emitHoverEvent(this.hoveredPoint);
      }
      this._handleMarkerHover(point);
    } else if (this.shapeType === "ellipse") {
      let point = this.getEllipsePointAt(percentX, percentY);
      let oldHoveredPoint = this.hoveredPoint;
      this.hoveredPoint = point ? point : null;
      if (this.hoveredPoint !== oldHoveredPoint) {
        this._emitHoverEvent(this.hoveredPoint);
      }
      this._handleMarkerHover(point);
    } else if (this.shapeType === "inset") {
      let point = this.getInsetPointAt(percentX, percentY);
      let oldHoveredPoint = this.hoveredPoint;
      this.hoveredPoint = point ? point : null;
      if (this.hoveredPoint !== oldHoveredPoint) {
        this._emitHoverEvent(this.hoveredPoint);
      }
      this._handleMarkerHover(point);
    }
  }

  /**
   * Change the appearance of the given marker when the mouse hovers over it.
   * @param {String|Number} point if the shape is a polygon, the integer index of the
   *        point being hovered. Otherwise, a string identifying the point being hovered.
   *        Integers < 0 and falsey values excluding 0 indicate no point is being hovered.
   */
  _handleMarkerHover(point) {
    // Hide hover marker for now, will be shown if point is a valid hover target
    this.getElement("marker-hover").setAttribute("hidden", true);
    // Catch all falsey values except when point === 0, as that's a valid point
    if (!point && point !== 0) {
      this.setCursor("auto");
      return;
    }
    let hoverCursor = (this[_dragging]) ? "grabbing" : "grab";

    if (this.transformMode) {
      if (!point) {
        this.setCursor("auto");
        return;
      }
      let { nw, ne, sw, se, n, w, s, e,
            rotatePoint, center } = this.transformedBoundingBox;

      const points = [
        { pointName: "translate", x: center[0], y: center[1], cursor: "move" },
        { pointName: "scale-se", x: se[0], y: se[1], anchor: "nw" },
        { pointName: "scale-ne", x: ne[0], y: ne[1], anchor: "sw" },
        { pointName: "scale-sw", x: sw[0], y: sw[1], anchor: "ne" },
        { pointName: "scale-nw", x: nw[0], y: nw[1], anchor: "se" },
        { pointName: "scale-n", x: n[0], y: n[1], anchor: "s" },
        { pointName: "scale-s", x: s[0], y: s[1], anchor: "n" },
        { pointName: "scale-e", x: e[0], y: e[1], anchor: "w" },
        { pointName: "scale-w", x: w[0], y: w[1], anchor: "e" },
        { pointName: "rotate", x: rotatePoint[0], y: rotatePoint[1],
          cursor: hoverCursor
        },
      ];

      for (let { pointName, x, y, cursor, anchor } of points) {
        if (point === pointName) {
          this._drawHoverMarker([[x, y]]);

          // If the point is a scale handle, we will need to determine the direction
          // of the resize cursor based on the position of the handle relative to its
          // "anchor" (the handle opposite to it).
          if (pointName.includes("scale")) {
            let direction = this.getRoughDirection(pointName, anchor);
            this.setCursor(`${direction}-resize`);
          } else {
            this.setCursor(cursor);
          }
        }
      }
    } else if (this.shapeType === "polygon") {
      if (point === -1) {
        this.setCursor("auto");
        return;
      }
      this.setCursor(hoverCursor);
      this._drawHoverMarker([this.coordinates[point]]);
    } else if (this.shapeType === "circle") {
      this.setCursor(hoverCursor);

      let { cx, cy, rx } = this.coordinates;
      if (point === "radius") {
        this._drawHoverMarker([[cx + rx, cy]]);
      } else if (point === "center") {
        this._drawHoverMarker([[cx, cy]]);
      }
    } else if (this.shapeType === "ellipse") {
      this.setCursor(hoverCursor);

      if (point === "center") {
        let { cx, cy } = this.coordinates;
        this._drawHoverMarker([[cx, cy]]);
      } else if (point === "rx") {
        let { cx, cy, rx } = this.coordinates;
        this._drawHoverMarker([[cx + rx, cy]]);
      } else if (point === "ry") {
        let { cx, cy, ry } = this.coordinates;
        this._drawHoverMarker([[cx, cy + ry]]);
      }
    } else if (this.shapeType === "inset") {
      this.setCursor(hoverCursor);

      let { top, right, bottom, left } = this.coordinates;
      let centerX = (left + (100 - right)) / 2;
      let centerY = (top + (100 - bottom)) / 2;
      let points = point.split(",");
      let coords = points.map(side => {
        if (side === "top") {
          return [centerX, top];
        } else if (side === "right") {
          return [100 - right, centerY];
        } else if (side === "bottom") {
          return [centerX, 100 - bottom];
        } else if (side === "left") {
          return [left, centerY];
        }
        return null;
      });

      this._drawHoverMarker(coords);
    }
  }

  _drawHoverMarker(points) {
    let { width, height } = this.currentDimensions;
    let zoom = getCurrentZoom(this.win);
    let path = points.map(([x, y]) => {
      return getCirclePath(BASE_MARKER_SIZE, x, y, width, height, zoom);
    }).join(" ");

    let markerHover = this.getElement("marker-hover");
    markerHover.setAttribute("d", path);
    markerHover.removeAttribute("hidden");
  }

  _emitHoverEvent(point) {
    if (point === null || point === undefined) {
      this.emit("highlighter-event", {
        type: "shape-hover-off"
      });
    } else {
      this.emit("highlighter-event", {
        type: "shape-hover-on",
        point: point.toString()
      });
    }
  }

  /**
   * Convert the given coordinates on the page to percentages relative to the current
   * element.
   * @param {Number} pageX the x coordinate on the page
   * @param {Number} pageY the y coordinate on the page
   * @returns {Object} object of form {percentX, percentY}, which are the x/y coords
   *          in percentages relative to the element.
   */
  convertPageCoordsToPercent(pageX, pageY) {
    // If the current node is in an iframe, we get dimensions relative to the frame.
    let dims = this.frameDimensions;
    let { top, left, width, height } = dims;
    pageX -= left;
    pageY -= top;
    let percentX = pageX * 100 / width;
    let percentY = pageY * 100 / height;
    return { percentX, percentY };
  }

  /**
   * Convert the given x/y coordinates, in percentages relative to the current element,
   * to pixel coordinates relative to the page
   * @param {Number} x the x coordinate
   * @param {Number} y the y coordinate
   * @returns {Object} object of form {x, y}, which are the x/y coords in pixels
   *          relative to the page
   *
   * @memberof ShapesHighlighter
   */
  convertPercentToPageCoords(x, y) {
    let dims = this.frameDimensions;
    let { top, left, width, height } = dims;
    x = x * width / 100;
    y = y * height / 100;
    x += left;
    y += top;
    return { x, y };
  }

  /**
   * Get which transformation should be applied based on the mouse position.
   * @param {Number} pageX the x coordinate of the mouse.
   * @param {Number} pageY the y coordinate of the mouse.
   * @returns {String} a string describing the transformation that should be applied
   *          to the shape.
   */
  getTransformPointAt(pageX, pageY) {
    let { nw, ne, sw, se, n, w, s, e, rotatePoint, center } = this.transformedBoundingBox;
    let { width, height } = this.currentDimensions;
    let zoom = getCurrentZoom(this.win);
    let clickRadiusX = BASE_MARKER_SIZE / zoom * 100 / width;
    let clickRadiusY = BASE_MARKER_SIZE / zoom * 100 / height;

    let points = [
      { pointName: "translate", x: center[0], y: center[1] },
      { pointName: "scale-se", x: se[0], y: se[1] },
      { pointName: "scale-ne", x: ne[0], y: ne[1] },
      { pointName: "scale-sw", x: sw[0], y: sw[1] },
      { pointName: "scale-nw", x: nw[0], y: nw[1] },
    ];

    if (this.shapeType === "polygon" || this.shapeType === "ellipse") {
      points.push({ pointName: "scale-n", x: n[0], y: n[1] },
                  { pointName: "scale-s", x: s[0], y: s[1] },
                  { pointName: "scale-e", x: e[0], y: e[1] },
                  { pointName: "scale-w", x: w[0], y: w[1] });
    }

    if (this.shapeType === "polygon") {
      let x = rotatePoint[0];
      let y = rotatePoint[1];
      if (pageX >= x - clickRadiusX && pageX <= x + clickRadiusX &&
          pageY >= y - clickRadiusY && pageY <= y + clickRadiusY) {
        return "rotate";
      }
    }

    for (let { pointName, x, y } of points) {
      if (pageX >= x - clickRadiusX && pageX <= x + clickRadiusX &&
          pageY >= y - clickRadiusY && pageY <= y + clickRadiusY) {
        return pointName;
      }
    }

    return "";
  }

  /**
   * Get the id of the point on the polygon highlighter at the given coordinate.
   * @param {Number} pageX the x coordinate on the page, in % relative to the element
   * @param {Number} pageY the y coordinate on the page, in % relative to the element
   * @returns {Number} the index of the point that was clicked on in this.coordinates,
   *          or -1 if none of the points were clicked on.
   */
  getPolygonPointAt(pageX, pageY) {
    let { coordinates } = this;
    let { width, height } = this.currentDimensions;
    let zoom = getCurrentZoom(this.win);
    let clickRadiusX = BASE_MARKER_SIZE / zoom * 100 / width;
    let clickRadiusY = BASE_MARKER_SIZE / zoom * 100 / height;

    for (let [index, coord] of coordinates.entries()) {
      let [x, y] = coord;
      if (pageX >= x - clickRadiusX && pageX <= x + clickRadiusX &&
          pageY >= y - clickRadiusY && pageY <= y + clickRadiusY) {
        return index;
      }
    }

    return -1;
  }

  /**
   * Check if the mouse clicked on a line of the polygon, and if so, add a point near
   * the click.
   * @param {Number} pageX the x coordinate on the page, in % relative to the element
   * @param {Number} pageY the y coordinate on the page, in % relative to the element
   */
  getPolygonClickedLine(pageX, pageY) {
    let { coordinates } = this;
    let { width } = this.currentDimensions;
    let clickWidth = LINE_CLICK_WIDTH * 100 / width;

    for (let i = 0; i < coordinates.length; i++) {
      let [x1, y1] = coordinates[i];
      let [x2, y2] = (i === coordinates.length - 1) ? coordinates[0] : coordinates[i + 1];
      // Get the distance between clicked point and line drawn between points 1 and 2
      // to check if the click was on the line between those two points.
      let distance = distanceToLine(x1, y1, x2, y2, pageX, pageY);
      if (distance <= clickWidth &&
          Math.min(x1, x2) - clickWidth <= pageX &&
          pageX <= Math.max(x1, x2) + clickWidth &&
          Math.min(y1, y2) - clickWidth <= pageY &&
          pageY <= Math.max(y1, y2) + clickWidth) {
        // Get the point on the line closest to the clicked point.
        let [newX, newY] = projection(x1, y1, x2, y2, pageX, pageY);
        // Default unit for new points is percentages
        this._addPolygonPoint(i, round(newX, "%"), round(newY, "%"));
        return;
      }
    }
  }

  /**
   * Check if the center point or radius of the circle highlighter is at given coords
   * @param {Number} pageX the x coordinate on the page, in % relative to the element
   * @param {Number} pageY the y coordinate on the page, in % relative to the element
   * @returns {String} "center" if the center point was clicked, "radius" if the radius
   *          was clicked, "" if neither was clicked.
   */
  getCirclePointAt(pageX, pageY) {
    let { cx, cy, rx, ry } = this.coordinates;
    let { width, height } = this.currentDimensions;
    let zoom = getCurrentZoom(this.win);
    let clickRadiusX = BASE_MARKER_SIZE / zoom * 100 / width;
    let clickRadiusY = BASE_MARKER_SIZE / zoom * 100 / height;

    if (clickedOnPoint(pageX, pageY, cx, cy, clickRadiusX, clickRadiusY)) {
      return "center";
    }

    let clickWidthX = LINE_CLICK_WIDTH * 100 / width;
    let clickWidthY = LINE_CLICK_WIDTH * 100 / height;
    if (clickedOnEllipseEdge(pageX, pageY, cx, cy, rx, ry, clickWidthX, clickWidthY) ||
        clickedOnPoint(pageX, pageY, cx + rx, cy, clickRadiusX, clickRadiusY)) {
      return "radius";
    }

    return "";
  }

  /**
   * Check if the center or rx/ry points of the ellipse highlighter is at given point
   * @param {Number} pageX the x coordinate on the page, in % relative to the element
   * @param {Number} pageY the y coordinate on the page, in % relative to the element
   * @returns {String} "center" if the center point was clicked, "rx" if the x-radius
   *          point was clicked, "ry" if the y-radius point was clicked,
   *          "" if none was clicked.
   */
  getEllipsePointAt(pageX, pageY) {
    let { cx, cy, rx, ry } = this.coordinates;
    let { width, height } = this.currentDimensions;
    let zoom = getCurrentZoom(this.win);
    let clickRadiusX = BASE_MARKER_SIZE / zoom * 100 / width;
    let clickRadiusY = BASE_MARKER_SIZE / zoom * 100 / height;

    if (clickedOnPoint(pageX, pageY, cx, cy, clickRadiusX, clickRadiusY)) {
      return "center";
    }

    if (clickedOnPoint(pageX, pageY, cx + rx, cy, clickRadiusX, clickRadiusY)) {
      return "rx";
    }

    if (clickedOnPoint(pageX, pageY, cx, cy + ry, clickRadiusX, clickRadiusY)) {
      return "ry";
    }

    return "";
  }

  /**
   * Check if the edges of the inset highlighter is at given coords
   * @param {Number} pageX the x coordinate on the page, in % relative to the element
   * @param {Number} pageY the y coordinate on the page, in % relative to the element
   * @returns {String} "top", "left", "right", or "bottom" if any of those edges were
   *          clicked. "" if none were clicked.
   */
  getInsetPointAt(pageX, pageY) {
    let { top, left, right, bottom } = this.coordinates;
    let zoom = getCurrentZoom(this.win);
    let { width, height } = this.currentDimensions;
    let clickWidthX = LINE_CLICK_WIDTH * 100 / width;
    let clickWidthY = LINE_CLICK_WIDTH * 100 / height;
    let clickRadiusX = BASE_MARKER_SIZE / zoom * 100 / width;
    let clickRadiusY = BASE_MARKER_SIZE / zoom * 100 / height;
    let centerX = (left + (100 - right)) / 2;
    let centerY = (top + (100 - bottom)) / 2;

    if ((pageX >= left - clickWidthX && pageX <= left + clickWidthX &&
        pageY >= top && pageY <= 100 - bottom) ||
        clickedOnPoint(pageX, pageY, left, centerY, clickRadiusX, clickRadiusY)) {
      return "left";
    }

    if ((pageX >= 100 - right - clickWidthX && pageX <= 100 - right + clickWidthX &&
        pageY >= top && pageY <= 100 - bottom) ||
        clickedOnPoint(pageX, pageY, 100 - right, centerY, clickRadiusX, clickRadiusY)) {
      return "right";
    }

    if ((pageY >= top - clickWidthY && pageY <= top + clickWidthY &&
        pageX >= left && pageX <= 100 - right) ||
        clickedOnPoint(pageX, pageY, centerX, top, clickRadiusX, clickRadiusY)) {
      return "top";
    }

    if ((pageY >= 100 - bottom - clickWidthY && pageY <= 100 - bottom + clickWidthY &&
        pageX >= left && pageX <= 100 - right) ||
        clickedOnPoint(pageX, pageY, centerX, 100 - bottom, clickRadiusX, clickRadiusY)) {
      return "bottom";
    }

    return "";
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

    // default to border for clip-path, and margin for shape-outside
    let referenceBox = this.property === "clip-path" ? "border" : "margin";
    for (let geometry of geometryTypes) {
      if (definition.includes(geometry)) {
        referenceBox = geometry;
      }
    }
    this.referenceBox = referenceBox;

    this.useStrokeBox = definition.includes("stroke-box");
    this.geometryBox = definition.substring(definition.lastIndexOf(")") + 1).trim();

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
    this.coordUnits = this.polygonRawPoints();
    if (!this.origCoordUnits) {
      this.origCoordUnits = this.coordUnits;
    }
    let splitDef = definition.split(", ");
    if (splitDef[0] === "evenodd" || splitDef[0] === "nonzero") {
      splitDef.shift();
    }
    let minX = Number.MAX_SAFE_INTEGER;
    let minY = Number.MAX_SAFE_INTEGER;
    let maxX = Number.MIN_SAFE_INTEGER;
    let maxY = Number.MIN_SAFE_INTEGER;
    let coordinates = splitDef.map(coords => {
      let [x, y] = splitCoords(coords).map(this.convertCoordsToPercent.bind(this));
      if (x < minX) {
        minX = x;
      }
      if (y < minY) {
        minY = y;
      }
      if (x > maxX) {
        maxX = x;
      }
      if (y > maxY) {
        maxY = y;
      }
      return [x, y];
    });
    this.boundingBox = { minX, minY, maxX, maxY };
    if (!this.origBoundingBox) {
      this.origBoundingBox = this.boundingBox;
    }
    return coordinates;
  }

  /**
   * Parse the raw (non-computed) definition of the CSS polygon.
   * @returns {Array} an array of the points of the polygon, with units preserved.
   */
  polygonRawPoints() {
    let definition = getDefinedShapeProperties(this.currentNode, this.property);
    if (definition === this.rawDefinition && this.coordUnits) {
      return this.coordUnits;
    }
    this.rawDefinition = definition;
    definition = definition.substring(8, definition.lastIndexOf(")"));
    let splitDef = definition.split(", ");
    if (splitDef[0].includes("evenodd") || splitDef[0].includes("nonzero")) {
      this.fillRule = splitDef[0].trim();
      splitDef.shift();
    } else {
      this.fillRule = "";
    }
    return splitDef.map(coords => {
      return splitCoords(coords).map(coord => {
        // Undo the insertion of &nbsp; that was done in splitCoords.
        return coord.replace(/\u00a0/g, " ");
      });
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
    this.coordUnits = this.circleRawPoints();
    if (!this.origCoordUnits) {
      this.origCoordUnits = this.coordUnits;
    }
    // The computed value of circle() always has the keyword "at".
    let values = definition.split(" at ");
    let radius = values[0];
    let { width, height } = this.currentDimensions;
    let center = splitCoords(values[1]).map(this.convertCoordsToPercent.bind(this));

    // Percentage values for circle() are resolved from the
    // used width and height of the reference box as sqrt(width^2+height^2)/sqrt(2).
    let computedSize = Math.sqrt((width ** 2) + (height ** 2)) / Math.sqrt(2);

    if (radius === "closest-side") {
      // radius is the distance from center to closest side of reference box
      radius = Math.min(center[0], center[1], 100 - center[0], 100 - center[1]);
    } else if (radius === "farthest-side") {
      // radius is the distance from center to farthest side of reference box
      radius = Math.max(center[0], center[1], 100 - center[0], 100 - center[1]);
    } else if (radius.includes("calc(")) {
      radius = evalCalcExpression(radius.substring(5, radius.length - 1), computedSize);
    } else {
      radius = coordToPercent(radius, computedSize);
    }

    // Scale both radiusX and radiusY to match the radius computed
    // using the above equation.
    let ratioX = width / computedSize;
    let ratioY = height / computedSize;
    let radiusX = radius / ratioX;
    let radiusY = radius / ratioY;

    this.boundingBox = { minX: center[0] - radiusX, maxX: center[0] + radiusX,
                         minY: center[1] - radiusY, maxY: center[1] + radiusY };
    if (!this.origBoundingBox) {
      this.origBoundingBox = this.boundingBox;
    }
    return { radius, rx: radiusX, ry: radiusY, cx: center[0], cy: center[1] };
  }

  /**
   * Parse the raw (non-computed) definition of the CSS circle.
   * @returns {Object} an object of the points of the circle (cx, cy, radius),
   *          with units preserved.
   */
  circleRawPoints() {
    let definition = getDefinedShapeProperties(this.currentNode, this.property);
    if (definition === this.rawDefinition && this.coordUnits) {
      return this.coordUnits;
    }
    this.rawDefinition = definition;
    definition = definition.substring(7, definition.lastIndexOf(")"));

    let values = definition.split("at");
    let [cx = "", cy = ""] = (values[1]) ? splitCoords(values[1]).map(coord => {
      // Undo the insertion of &nbsp; that was done in splitCoords.
      return coord.replace(/\u00a0/g, " ");
    }) : [];
    let radius = (values[0]) ? values[0].trim() : "closest-side";
    return { cx, cy, radius };
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
    this.coordUnits = this.ellipseRawPoints();
    if (!this.origCoordUnits) {
      this.origCoordUnits = this.coordUnits;
    }
    let values = definition.split(" at ");
    let center = splitCoords(values[1]).map(this.convertCoordsToPercent.bind(this));

    let radii = splitCoords(values[0]).map((radius, i) => {
      if (radius === "closest-side") {
        // radius is the distance from center to closest x/y side of reference box
        return i % 2 === 0 ? Math.min(center[0], 100 - center[0])
                           : Math.min(center[1], 100 - center[1]);
      } else if (radius === "farthest-side") {
        // radius is the distance from center to farthest x/y side of reference box
        return i % 2 === 0 ? Math.max(center[0], 100 - center[0])
                           : Math.max(center[1], 100 - center[1]);
      }
      return this.convertCoordsToPercent(radius, i);
    });

    this.boundingBox = { minX: center[0] - radii[0], maxX: center[0] + radii[0],
                         minY: center[1] - radii[1], maxY: center[1] + radii[1] };
    if (!this.origBoundingBox) {
      this.origBoundingBox = this.boundingBox;
    }
    return { rx: radii[0], ry: radii[1], cx: center[0], cy: center[1] };
  }

  /**
   * Parse the raw (non-computed) definition of the CSS ellipse.
   * @returns {Object} an object of the points of the ellipse (cx, cy, rx, ry),
   *          with units preserved.
   */
  ellipseRawPoints() {
    let definition = getDefinedShapeProperties(this.currentNode, this.property);
    if (definition === this.rawDefinition && this.coordUnits) {
      return this.coordUnits;
    }
    this.rawDefinition = definition;
    definition = definition.substring(8, definition.lastIndexOf(")"));

    let values = definition.split("at");
    let [rx = "closest-side", ry = "closest-side"] = (values[0]) ?
      splitCoords(values[0]).map(coord => {
        // Undo the insertion of &nbsp; that was done in splitCoords.
        return coord.replace(/\u00a0/g, " ");
      }) : [];
    let [cx = "", cy = ""] = (values[1]) ? splitCoords(values[1]).map(coord => {
      return coord.replace(/\u00a0/g, " ");
    }) : [];
    return { rx, ry, cx, cy };
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
    this.coordUnits = this.insetRawPoints();
    if (!this.origCoordUnits) {
      this.origCoordUnits = this.coordUnits;
    }
    let values = definition.split(" round ");
    let offsets = splitCoords(values[0]).map(this.convertCoordsToPercent.bind(this));

    let top, left, right, bottom;
    // The offsets, like margin/padding/border, are in order: top, right, bottom, left.
    if (offsets.length === 1) {
      top = left = right = bottom = offsets[0];
    } else if (offsets.length === 2) {
      top = bottom = offsets[0];
      left = right = offsets[1];
    } else if (offsets.length === 3) {
      top = offsets[0];
      left = right = offsets[1];
      bottom = offsets[2];
    } else if (offsets.length === 4) {
      top = offsets[0];
      right = offsets[1];
      bottom = offsets[2];
      left = offsets[3];
    }

    // maxX/maxY are found by subtracting the right/bottom edges from 100
    // (the width/height of the element in %)
    this.boundingBox = { minX: left, maxX: 100 - right, minY: top, maxY: 100 - bottom};
    if (!this.origBoundingBox) {
      this.origBoundingBox = this.boundingBox;
    }
    return { top, left, right, bottom };
  }

  /**
   * Parse the raw (non-computed) definition of the CSS inset.
   * @returns {Object} an object of the points of the inset (top, right, bottom, left),
   *          with units preserved.
   */
  insetRawPoints() {
    let definition = getDefinedShapeProperties(this.currentNode, this.property);
    if (definition === this.rawDefinition && this.coordUnits) {
      return this.coordUnits;
    }
    this.rawDefinition = definition;
    definition = definition.substring(6, definition.lastIndexOf(")"));

    let values = definition.split(" round ");
    this.insetRound = values[1];
    let offsets = splitCoords(values[0]).map(coord => {
      // Undo the insertion of &nbsp; that was done in splitCoords.
      return coord.replace(/\u00a0/g, " ");
    });

    let top, left, right, bottom = 0;

    if (offsets.length === 1) {
      top = left = right = bottom = offsets[0];
    } else if (offsets.length === 2) {
      top = bottom = offsets[0];
      left = right = offsets[1];
    } else if (offsets.length === 3) {
      top = offsets[0];
      left = right = offsets[1];
      bottom = offsets[2];
    } else if (offsets.length === 4) {
      top = offsets[0];
      right = offsets[1];
      bottom = offsets[2];
      left = offsets[3];
    }

    return { top, left, right, bottom };
  }

  convertCoordsToPercent(coord, i) {
    let { width, height } = this.currentDimensions;
    let size = i % 2 === 0 ? width : height;
    if (coord.includes("calc(")) {
      return evalCalcExpression(coord.substring(5, coord.length - 1), size);
    }
    return coordToPercent(coord, size);
  }

  /**
   * Destroy the nodes. Remove listeners.
   */
  destroy() {
    let { pageListenerTarget } = this.highlighterEnv;
    if (pageListenerTarget) {
      DOM_EVENTS.forEach(type => pageListenerTarget.removeEventListener(type, this));
    }
    super.destroy(this);
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
   * Return whether all the elements used to draw shapes are hidden.
   * @returns {Boolean}
   */
  areShapesHidden() {
    return this.getElement("ellipse").hasAttribute("hidden") &&
           this.getElement("polygon").hasAttribute("hidden") &&
           this.getElement("rect").hasAttribute("hidden") &&
           this.getElement("bounding-box").hasAttribute("hidden");
  }

  /**
   * Show the highlighter on a given node
   */
  _show() {
    this.hoveredPoint = this.options.hoverPoint;
    this.transformMode = this.options.transformMode;
    this.coordinates = null;
    this.coordUnits = null;
    this.origBoundingBox = null;
    this.origCoordUnits = null;
    this.origCoordinates = null;
    this.transformedBoundingBox = null;
    if (this.transformMode) {
      this.transformMatrix = identity();
    }
    if (this._hasMoved() && this.transformMode) {
      this.transformedBoundingBox = this.calculateTransformedBoundingBox();
    }
    return this._update();
  }

  /**
   * The AutoRefreshHighlighter's _hasMoved method returns true only if the element's
   * quads have changed. Override it so it also returns true if the element's shape has
   * changed (which can happen when you change a CSS properties for instance).
   */
  _hasMoved() {
    let hasMoved = AutoRefreshHighlighter.prototype._hasMoved.call(this);

    if (hasMoved) {
      this.origBoundingBox = null;
      this.origCoordUnits = null;
      this.origCoordinates = null;
      if (this.transformMode) {
        this.transformMatrix = identity();
      }
    }

    let oldShapeCoordinates = JSON.stringify(this.coordinates);

    // TODO: need other modes too.
    if (this.options.mode.startsWith("css")) {
      let property = shapeModeToCssPropertyName(this.options.mode);
      // change camelCase to kebab-case
      this.property = property.replace(/([a-z][A-Z])/g, g => {
        return g[0] + "-" + g[1].toLowerCase();
      });
      let style = getComputedStyle(this.currentNode)[property];

      if (!style || style === "none") {
        this.coordinates = [];
        this.shapeType = "none";
      } else {
        let { coordinates, shapeType } = this._parseCSSShapeValue(style);
        this.coordinates = coordinates;
        if (!this.origCoordinates) {
          this.origCoordinates = coordinates;
        }
        this.shapeType = shapeType;
      }
    }

    let newShapeCoordinates = JSON.stringify(this.coordinates);
    hasMoved = hasMoved || oldShapeCoordinates !== newShapeCoordinates;
    if (this.transformMode && hasMoved) {
      this.transformedBoundingBox = this.calculateTransformedBoundingBox();
    }

    return hasMoved;
  }

  /**
   * Hide all elements used to highlight CSS different shapes.
   */
  _hideShapes() {
    this.getElement("ellipse").setAttribute("hidden", true);
    this.getElement("polygon").setAttribute("hidden", true);
    this.getElement("rect").setAttribute("hidden", true);
    this.getElement("bounding-box").setAttribute("hidden", true);
    this.getElement("markers").setAttribute("d", "");
    this.getElement("markers-outline").setAttribute("d", "");
    this.getElement("rotate-line").setAttribute("d", "");
    this.getElement("quad").setAttribute("hidden", true);
    this.getElement("clip-ellipse").setAttribute("hidden", true);
    this.getElement("clip-polygon").setAttribute("hidden", true);
    this.getElement("clip-rect").setAttribute("hidden", true);
    this.getElement("dashed-polygon").setAttribute("hidden", true);
    this.getElement("dashed-ellipse").setAttribute("hidden", true);
    this.getElement("dashed-rect").setAttribute("hidden", true);
  }

  /**
   * Update the highlighter for the current node. Called whenever the element's quads
   * or CSS shape has changed.
   * @returns {Boolean} whether the highlighter was successfully updated
   */
  _update() {
    setIgnoreLayoutChanges(true);
    this.getElement("group").setAttribute("transform", "");
    let root = this.getElement("root");
    root.setAttribute("hidden", true);

    let { top, left, width, height } = this.currentDimensions;
    let zoom = getCurrentZoom(this.win);

    // Size the SVG like the current node.
    this.getElement("shape-container").setAttribute("style",
      `top:${top}px;left:${left}px;width:${width}px;height:${height}px;`);

    this._hideShapes();
    this._updateShapes(width, height, zoom);

    // For both shape-outside and clip-path the element's quads are displayed for the
    // parts that overlap with the shape. The parts of the shape that extend past the
    // element's quads are shown with a dashed line.
    let quadRect = this.getElement("quad");
    quadRect.removeAttribute("hidden");

    this.getElement("polygon").setAttribute("clip-path", "url(#shapes-quad-clip-path)");
    this.getElement("ellipse").setAttribute("clip-path", "url(#shapes-quad-clip-path)");
    this.getElement("rect").setAttribute("clip-path", "url(#shapes-quad-clip-path)");

    let { width: winWidth, height: winHeight } = this._winDimensions;
    root.removeAttribute("hidden");
    root.setAttribute("style",
      `position:absolute; width:${winWidth}px;height:${winHeight}px; overflow:hidden;`);

    this._handleMarkerHover(this.hoveredPoint);

    setIgnoreLayoutChanges(false, this.highlighterEnv.window.document.documentElement);

    return true;
  }

  /**
   * Update the SVGs to render the current CSS shape and add markers depending on shape
   * type and transform mode.
   * @param {Number} width the width of the element quads
   * @param {Number} height the height of the element quads
   * @param {Number} zoom the zoom level of the window
   */
  _updateShapes(width, height, zoom) {
    if (this.transformMode && this.shapeType !== "none") {
      this._updateTransformMode(width, height, zoom);
    } else if (this.shapeType === "polygon") {
      this._updatePolygonShape(width, height, zoom);
      // Draw markers for each of the polygon's points.
      this._drawMarkers(this.coordinates, width, height, zoom);
    } else if (this.shapeType === "circle") {
      let { rx, cx, cy } = this.coordinates;
      // Shape renders for "circle()" and "ellipse()" use the same SVG nodes.
      this._updateEllipseShape(width, height, zoom);
      // Draw markers for center and radius points.
      this._drawMarkers([ [cx, cy], [cx + rx, cy] ], width, height, zoom);
    } else if (this.shapeType === "ellipse") {
      let { rx, ry, cx, cy } = this.coordinates;
      this._updateEllipseShape(width, height, zoom);
      // Draw markers for center, horizontal radius and vertical radius points.
      this._drawMarkers([ [cx, cy], [cx + rx, cy], [cx, cy + ry] ], width, height, zoom);
    } else if (this.shapeType === "inset") {
      let { top, left, right, bottom } = this.coordinates;
      let centerX = (left + (100 - right)) / 2;
      let centerY = (top + (100 - bottom)) / 2;
      let markerCoords = [[centerX, top], [100 - right, centerY],
                          [centerX, 100 - bottom], [left, centerY]];
      this._updateInsetShape(width, height, zoom);
      // Draw markers for each of the inset's sides.
      this._drawMarkers(markerCoords, width, height, zoom);
    }
  }

  /**
   * Update the SVGs for transform mode to fit the new shape.
   * @param {Number} width the width of the element quads
   * @param {Number} height the height of the element quads
   * @param {Number} zoom the zoom level of the window
   */
  _updateTransformMode(width, height, zoom) {
    let { nw, ne, sw, se, n, w, s, e, rotatePoint, center } = this.transformedBoundingBox;
    let boundingBox = this.getElement("bounding-box");
    let path = `M${nw.join(" ")} L${ne.join(" ")} L${se.join(" ")} L${sw.join(" ")} Z`;
    boundingBox.setAttribute("d", path);
    boundingBox.removeAttribute("hidden");

    let markerPoints = [center, nw, ne, se, sw];
    if (this.shapeType === "polygon" || this.shapeType === "ellipse") {
      markerPoints.push(n, s, w, e);
    }

    if (this.shapeType === "polygon") {
      this._updatePolygonShape(width, height, zoom);
      markerPoints.push(rotatePoint);
      let rotateLine = `M ${center.join(" ")} L ${rotatePoint.join(" ")}`;
      this.getElement("rotate-line").setAttribute("d", rotateLine);
    } else if (this.shapeType === "circle" || this.shapeType === "ellipse") {
      // Shape renders for "circle()" and "ellipse()" use the same SVG nodes.
      this._updateEllipseShape(width, height, zoom);
    } else if (this.shapeType === "inset") {
      this._updateInsetShape(width, height, zoom);
    }

    this._drawMarkers(markerPoints, width, height, zoom);
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

    let clipPolygon = this.getElement("clip-polygon");
    clipPolygon.setAttribute("points", points);
    clipPolygon.removeAttribute("hidden");

    let dashedPolygon = this.getElement("dashed-polygon");
    dashedPolygon.setAttribute("points", points);
    dashedPolygon.removeAttribute("hidden");
  }

  /**
   * Update the SVG ellipse to fit the CSS circle or ellipse.
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

    let clipEllipse = this.getElement("clip-ellipse");
    clipEllipse.setAttribute("rx", rx);
    clipEllipse.setAttribute("ry", ry);
    clipEllipse.setAttribute("cx", cx);
    clipEllipse.setAttribute("cy", cy);
    clipEllipse.removeAttribute("hidden");

    let dashedEllipse = this.getElement("dashed-ellipse");
    dashedEllipse.setAttribute("rx", rx);
    dashedEllipse.setAttribute("ry", ry);
    dashedEllipse.setAttribute("cx", cx);
    dashedEllipse.setAttribute("cy", cy);
    dashedEllipse.removeAttribute("hidden");
  }

  /**
   * Update the SVG rect to fit the CSS inset.
   * @param {Number} width the width of the element quads
   * @param {Number} height the height of the element quads
   * @param {Number} zoom the zoom level of the window
   */
  _updateInsetShape(width, height, zoom) {
    let { top, left, right, bottom } = this.coordinates;
    let rectEl = this.getElement("rect");
    rectEl.setAttribute("x", left);
    rectEl.setAttribute("y", top);
    rectEl.setAttribute("width", 100 - left - right);
    rectEl.setAttribute("height", 100 - top - bottom);
    rectEl.removeAttribute("hidden");

    let clipRect = this.getElement("clip-rect");
    clipRect.setAttribute("x", left);
    clipRect.setAttribute("y", top);
    clipRect.setAttribute("width", 100 - left - right);
    clipRect.setAttribute("height", 100 - top - bottom);
    clipRect.removeAttribute("hidden");

    let dashedRect = this.getElement("dashed-rect");
    dashedRect.setAttribute("x", left);
    dashedRect.setAttribute("y", top);
    dashedRect.setAttribute("width", 100 - left - right);
    dashedRect.setAttribute("height", 100 - top - bottom);
    dashedRect.removeAttribute("hidden");
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
      return getCirclePath(BASE_MARKER_SIZE, x, y, width, height, zoom);
    }).join(" ");
    let outline = coords.map(([x, y]) => {
      return getCirclePath(BASE_MARKER_SIZE + 2, x, y, width, height, zoom);
    }).join(" ");

    this.getElement("markers").setAttribute("d", markers);
    this.getElement("markers-outline").setAttribute("d", outline);
  }

  /**
   * Calculate the bounding box of the shape after it is transformed according to
   * the transformation matrix.
   * @returns {Object} of form { nw, ne, sw, se, n, s, w, e, rotatePoint, center }.
   *          Each element in the object is an array of form [x,y], denoting the x/y
   *          coordinates of the given point.
   */
  calculateTransformedBoundingBox() {
    let { minX, minY, maxX, maxY } = this.origBoundingBox;
    let { width, height } = this.currentDimensions;
    let toPixel = scale(width / 100, height / 100);
    let toPercent = scale(100 / width, 100 / height);
    let matrix = multiply(toPercent, multiply(this.transformMatrix, toPixel));
    let centerX = (minX + maxX) / 2;
    let centerY = (minY + maxY) / 2;
    let nw = apply(matrix, [minX, minY]);
    let ne = apply(matrix, [maxX, minY]);
    let sw = apply(matrix, [minX, maxY]);
    let se = apply(matrix, [maxX, maxY]);
    let n = apply(matrix, [centerX, minY]);
    let s = apply(matrix, [centerX, maxY]);
    let w = apply(matrix, [minX, centerY]);
    let e = apply(matrix, [maxX, centerY]);
    let center = apply(matrix, [centerX, centerY]);

    let u = [(ne[0] - nw[0]) / 100 * width, (ne[1] - nw[1]) / 100 * height];
    let v = [(sw[0] - nw[0]) / 100 * width, (sw[1] - nw[1]) / 100 * height];
    let { basis, invertedBasis } = getBasis(u, v);
    let rotatePointMatrix = changeMatrixBase(translate(0, -ROTATE_LINE_LENGTH),
                                             invertedBasis, basis);
    rotatePointMatrix = multiply(toPercent,
                        multiply(rotatePointMatrix,
                        multiply(this.transformMatrix, toPixel)));
    let rotatePoint = apply(rotatePointMatrix, [centerX, centerY]);
    return { nw, ne, sw, se, n, s, w, e, rotatePoint, center };
  }

  /**
   * Hide the highlighter, the outline and the infobar.
   */
  _hide() {
    setIgnoreLayoutChanges(true);

    this._hideShapes();
    this.getElement("markers").setAttribute("d", "");
    this.getElement("root").setAttribute("style", "");

    setIgnoreLayoutChanges(false, this.highlighterEnv.window.document.documentElement);
  }

  onPageHide({ target }) {
    // If a page hide event is triggered for current window's highlighter, hide the
    // highlighter.
    if (target.defaultView === this.win) {
      this.hide();
    }
  }

  /**
   * Get the rough direction of the point relative to the anchor.
   * If the handle is roughly horizontal relative to the anchor, return "ew".
   * If the handle is roughly vertical relative to the anchor, return "ns"
   * If the handle is roughly above/right or below/left, return "nesw"
   * If the handle is roughly above/left or below/right, return "nwse"
   * @param {String} pointName the name of the point being hovered
   * @param {String} anchor the name of the anchor point
   * @returns {String} The rough direction of the point relative to the anchor
   */
  getRoughDirection(pointName, anchor) {
    let scalePoint = pointName.split("-")[1];
    let anchorPos = this.transformedBoundingBox[anchor];
    let scalePos = this.transformedBoundingBox[scalePoint];
    let { minX, minY, maxX, maxY } = this.boundingBox;
    let width = maxX - minX;
    let height = maxY - minY;
    let dx = (scalePos[0] - anchorPos[0]) / width;
    let dy = (scalePos[1] - anchorPos[1]) / height;
    if (dx >= -0.33 && dx <= 0.33) {
      return "ns";
    } else if (dy >= -0.33 && dy <= 0.33) {
      return "ew";
    } else if ((dx > 0.33 && dy < -0.33) || (dx < -0.33 && dy > 0.33)) {
      return "nesw";
    }
    return "nwse";
  }

  /**
   * Given a unit type, get the ratio by which to multiply a pixel value in order to
   * convert pixels to that unit.
   *
   * Percentage units (%) are relative to a size. This must be provided when requesting
   * a ratio for converting from pixels to percentages.
   *
   * @param {String} unit
   *        One of: %, em, rem, vw, vh
   * @param {Number} size
   *        Size to which percentage values are relative to.
   * @return {Number}
   */
  getUnitToPixelRatio(unit, size) {
    let ratio;
    const windowHeight = this.currentNode.ownerGlobal.innerHeight;
    const windowWidth = this.currentNode.ownerGlobal.innerWidth;
    switch (unit) {
      case "%":
        ratio = 100 / size;
        break;
      case "em":
        ratio = 1 / parseFloat(getComputedStyle(this.currentNode).fontSize);
        break;
      case "rem":
        const root = this.currentNode.ownerDocument.documentElement;
        ratio = 1 / parseFloat(getComputedStyle(root).fontSize);
        break;
      case "vw":
        ratio = 100 / windowWidth;
        break;
      case "vh":
        ratio = 100 / windowHeight;
        break;
      case "vmin":
        ratio = 100 / Math.min(windowHeight, windowWidth);
        break;
      case "vmax":
        ratio = 100 / Math.max(windowHeight, windowWidth);
        break;
      default:
        // If unit is not recognized, peg ratio 1:1 to pixels.
        ratio = 1;
    }

    return ratio;
  }
}

/**
 * Get the "raw" (i.e. non-computed) shape definition on the given node.
 * @param {nsIDOMNode} node the node to analyze
 * @param {String} property the CSS property for which a value should be retrieved.
 * @returns {String} the value of the given CSS property on the given node.
 */
function getDefinedShapeProperties(node, property) {
  let prop = "";
  if (!node) {
    return prop;
  }

  let cssRules = getCSSStyleRules(node);
  for (let i = 0; i < cssRules.length; i++) {
    let rule = cssRules[i];
    let value = rule.style.getPropertyValue(property);
    if (value && value !== "auto") {
      prop = value;
    }
  }

  if (node.style) {
    let value = node.style.getPropertyValue(property);
    if (value && value !== "auto") {
      prop = value;
    }
  }

  return prop.trim();
}

/**
 * Split coordinate pairs separated by a space and return an array.
 * @param {String} coords the coordinate pair, where each coord is separated by a space.
 * @returns {Array} a 2 element array containing the coordinates.
 */
function splitCoords(coords) {
  // All coordinate pairs are of the form "x y" where x and y are values or
  // calc() expressions. calc() expressions have spaces around operators, so
  // replace those spaces with \u00a0 (non-breaking space) so they will not be
  // split later.
  return coords.trim().replace(/ [\+\-\*\/] /g, match => {
    return `\u00a0${match.trim()}\u00a0`;
  }).split(" ");
}
exports.splitCoords = splitCoords;

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
exports.coordToPercent = coordToPercent;

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
exports.evalCalcExpression = evalCalcExpression;

/**
 * Converts a shape mode to the proper CSS property name.
 * @param {String} mode the mode of the CSS shape
 * @returns the equivalent CSS property name
 */
const shapeModeToCssPropertyName = mode => {
  let property = mode.substring(3);
  return property.substring(0, 1).toLowerCase() + property.substring(1);
};
exports.shapeModeToCssPropertyName = shapeModeToCssPropertyName;

/**
 * Get the SVG path definition for a circle with given attributes.
 * @param {Number} size the radius of the circle in pixels
 * @param {Number} cx the x coordinate of the centre of the circle
 * @param {Number} cy the y coordinate of the centre of the circle
 * @param {Number} width the width of the element the circle is being drawn for
 * @param {Number} height the height of the element the circle is being drawn for
 * @param {Number} zoom the zoom level of the window the circle is drawn in
 * @returns {String} the definition of the circle in SVG path description format.
 */
const getCirclePath = (size, cx, cy, width, height, zoom) => {
  // We use a viewBox of 100x100 for shape-container so it's easy to position things
  // based on their percentage, but this makes it more difficult to create circles.
  // Therefor, 100px is the base size of shape-container. In order to make the markers'
  // size scale properly, we must adjust the radius based on zoom and the width/height of
  // the element being highlighted, then calculate a radius for both x/y axes based
  // on the aspect ratio of the element.
  let radius = size * (100 / Math.max(width, height)) / zoom;
  let ratio = width / height;
  let rx = (ratio > 1) ? radius : radius / ratio;
  let ry = (ratio > 1) ? radius * ratio : radius;
  // a circle is drawn as two arc lines, starting at the leftmost point of the circle.
  return `M${cx - rx},${cy}a${rx},${ry} 0 1,0 ${rx * 2},0` +
         `a${rx},${ry} 0 1,0 ${rx * -2},0`;
};
exports.getCirclePath = getCirclePath;

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

/**
 * Get the unit (e.g. px, %, em) for the given point value.
 * @param {any} point a point value for which a unit should be retrieved.
 * @returns {String} the unit.
 */
const getUnit = (point) => {
  // If the point has no unit, default to px.
  if (isUnitless(point)) {
    return "px";
  }
  let [unit] = point.match(/[^\d]+$/) || ["px"];
  return unit;
};
exports.getUnit = getUnit;

/**
 * Check if the given point value has a unit.
 * @param {any} point a point value.
 * @returns {Boolean} whether the given value has a unit.
 */
const isUnitless = (point) => {
  return !point ||
         !point.match(/[^\d]+$/) ||
         // If zero doesn't have a unit, its numeric and string forms should be equal.
         (parseFloat(point) === 0 && (parseFloat(point).toString() === point)) ||
         point.includes("(") ||
         point === "closest-side" ||
         point === "farthest-side";
};

/**
 * Return the anchor corresponding to the given scale type.
 * @param {String} type a scale type, of form "scale-[direction]"
 * @returns {String} a string describing the anchor, one of the 8 cardinal directions.
 */
const getAnchorPoint = (type) => {
  let anchor = type.split("-")[1];
  if (anchor.includes("n")) {
    anchor = anchor.replace("n", "s");
  } else if (anchor.includes("s")) {
    anchor = anchor.replace("s", "n");
  }
  if (anchor.includes("w")) {
    anchor = anchor.replace("w", "e");
  } else if (anchor.includes("e")) {
    anchor = anchor.replace("e", "w");
  }

  if (anchor === "e" || anchor === "w") {
    anchor = "n" + anchor;
  } else if (anchor === "n" || anchor === "s") {
    anchor = anchor + "w";
  }

  return anchor;
};

/**
* Get the decimal point precision for values depending on unit type.
* Only handle pixels and falsy values for now. Round them to the nearest integer value.
* All other unit types round to two decimal points.
*
* @param {String|undefined} unitType any one of the accepted CSS unit types for position.
* @return {Number} decimal precision when rounding a value
*/
function getDecimalPrecision(unitType) {
  switch (unitType) {
    case "px":
    case "":
    case undefined:
      return 0;
    default:
      return 2;
  }
}
exports.getDecimalPrecision = getDecimalPrecision;

/**
 * Round up a numeric value to a fixed number of decimals depending on CSS unit type.
 * Used when generating output shape values when:
 * - transforming shapes
 * - inserting new points on a polygon.
 *
 * @param {Number} number
 *        Value to round up.
 * @param {String} unitType
 *        CSS unit type, like "px", "%", "em", "vh", etc.
 * @return {Number}
 *         Rounded value
 */
function round(number, unitType) {
  return number.toFixed(getDecimalPrecision(unitType));
}

exports.ShapesHighlighter = ShapesHighlighter;
