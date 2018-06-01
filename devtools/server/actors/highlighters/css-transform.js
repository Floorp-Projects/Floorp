/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AutoRefreshHighlighter } = require("./auto-refresh");
const {
  CanvasFrameAnonymousContentHelper, getComputedStyle,
  createSVGNode, createNode } = require("./utils/markup");
const { setIgnoreLayoutChanges,
  getNodeBounds } = require("devtools/shared/layout/utils");

// The minimum distance a line should be before it has an arrow marker-end
const ARROW_LINE_MIN_DISTANCE = 10;

var MARKER_COUNTER = 1;

/**
 * The CssTransformHighlighter is the class that draws an outline around a
 * transformed element and an outline around where it would be if untransformed
 * as well as arrows connecting the 2 outlines' corners.
 */
class CssTransformHighlighter extends AutoRefreshHighlighter {
  constructor(highlighterEnv) {
    super(highlighterEnv);

    this.ID_CLASS_PREFIX = "css-transform-";

    this.markup = new CanvasFrameAnonymousContentHelper(this.highlighterEnv,
      this._buildMarkup.bind(this));
  }

  _buildMarkup() {
    const container = createNode(this.win, {
      attributes: {
        "class": "highlighter-container"
      }
    });

    // The root wrapper is used to unzoom the highlighter when needed.
    const rootWrapper = createNode(this.win, {
      parent: container,
      attributes: {
        "id": "root",
        "class": "root"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    const svg = createSVGNode(this.win, {
      nodeType: "svg",
      parent: rootWrapper,
      attributes: {
        "id": "elements",
        "hidden": "true",
        "width": "100%",
        "height": "100%"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // Add a marker tag to the svg root for the arrow tip
    this.markerId = "arrow-marker-" + MARKER_COUNTER;
    MARKER_COUNTER++;
    const marker = createSVGNode(this.win, {
      nodeType: "marker",
      parent: svg,
      attributes: {
        "id": this.markerId,
        "markerWidth": "10",
        "markerHeight": "5",
        "orient": "auto",
        "markerUnits": "strokeWidth",
        "refX": "10",
        "refY": "5",
        "viewBox": "0 0 10 10"
      },
      prefix: this.ID_CLASS_PREFIX
    });
    createSVGNode(this.win, {
      nodeType: "path",
      parent: marker,
      attributes: {
        "d": "M 0 0 L 10 5 L 0 10 z",
        "fill": "#08C"
      }
    });

    const shapesGroup = createSVGNode(this.win, {
      nodeType: "g",
      parent: svg
    });

    // Create the 2 polygons (transformed and untransformed)
    createSVGNode(this.win, {
      nodeType: "polygon",
      parent: shapesGroup,
      attributes: {
        "id": "untransformed",
        "class": "untransformed"
      },
      prefix: this.ID_CLASS_PREFIX
    });
    createSVGNode(this.win, {
      nodeType: "polygon",
      parent: shapesGroup,
      attributes: {
        "id": "transformed",
        "class": "transformed"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // Create the arrows
    for (const nb of ["1", "2", "3", "4"]) {
      createSVGNode(this.win, {
        nodeType: "line",
        parent: shapesGroup,
        attributes: {
          "id": "line" + nb,
          "class": "line",
          "marker-end": "url(#" + this.markerId + ")"
        },
        prefix: this.ID_CLASS_PREFIX
      });
    }

    return container;
  }

  /**
   * Destroy the nodes. Remove listeners.
   */
  destroy() {
    AutoRefreshHighlighter.prototype.destroy.call(this);
    this.markup.destroy();
  }

  getElement(id) {
    return this.markup.getElement(this.ID_CLASS_PREFIX + id);
  }

  /**
   * Show the highlighter on a given node
   */
  _show() {
    if (!this._isTransformed(this.currentNode)) {
      this.hide();
      return false;
    }

    return this._update();
  }

  /**
   * Checks if the supplied node is transformed and not inline
   */
  _isTransformed(node) {
    const style = getComputedStyle(node);
    return style && (style.transform !== "none" && style.display !== "inline");
  }

  _setPolygonPoints(quad, id) {
    const points = [];
    for (const point of ["p1", "p2", "p3", "p4"]) {
      points.push(quad[point].x + "," + quad[point].y);
    }
    this.getElement(id).setAttribute("points", points.join(" "));
  }

  _setLinePoints(p1, p2, id) {
    const line = this.getElement(id);
    line.setAttribute("x1", p1.x);
    line.setAttribute("y1", p1.y);
    line.setAttribute("x2", p2.x);
    line.setAttribute("y2", p2.y);

    const dist = Math.sqrt(Math.pow(p2.x - p1.x, 2) + Math.pow(p2.y - p1.y, 2));
    if (dist < ARROW_LINE_MIN_DISTANCE) {
      line.removeAttribute("marker-end");
    } else {
      line.setAttribute("marker-end", "url(#" + this.markerId + ")");
    }
  }

  /**
   * Update the highlighter on the current highlighted node (the one that was
   * passed as an argument to show(node)).
   * Should be called whenever node size or attributes change
   */
  _update() {
    setIgnoreLayoutChanges(true);

    // Getting the points for the transformed shape
    const quads = this.currentQuads.border;
    if (!quads.length ||
        quads[0].bounds.width <= 0 || quads[0].bounds.height <= 0) {
      this._hideShapes();
      return false;
    }

    const [quad] = quads;

    // Getting the points for the untransformed shape
    const untransformedQuad = getNodeBounds(this.win, this.currentNode);

    this._setPolygonPoints(quad, "transformed");
    this._setPolygonPoints(untransformedQuad, "untransformed");
    for (const nb of ["1", "2", "3", "4"]) {
      this._setLinePoints(untransformedQuad["p" + nb], quad["p" + nb], "line" + nb);
    }

    // Adapt to the current zoom
    this.markup.scaleRootElement(this.currentNode, this.ID_CLASS_PREFIX + "root");

    this._showShapes();

    setIgnoreLayoutChanges(false, this.highlighterEnv.window.document.documentElement);
    return true;
  }

  /**
   * Hide the highlighter, the outline and the infobar.
   */
  _hide() {
    setIgnoreLayoutChanges(true);
    this._hideShapes();
    setIgnoreLayoutChanges(false, this.highlighterEnv.window.document.documentElement);
  }

  _hideShapes() {
    this.getElement("elements").setAttribute("hidden", "true");
  }

  _showShapes() {
    this.getElement("elements").removeAttribute("hidden");
  }
}

exports.CssTransformHighlighter = CssTransformHighlighter;
