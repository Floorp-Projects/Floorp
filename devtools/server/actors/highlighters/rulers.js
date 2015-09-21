/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const events = require("sdk/event/core");
const { getCurrentZoom,
  setIgnoreLayoutChanges } = require("devtools/toolkit/layout/utils");
const {
  CanvasFrameAnonymousContentHelper,
  createSVGNode, createNode } = require("./utils/markup");

// Maximum size, in pixel, for the horizontal ruler and vertical ruler
// used by RulersHighlighter
const RULERS_MAX_X_AXIS = 10000;
const RULERS_MAX_Y_AXIS = 15000;
// Number of steps after we add a graduation, marker and text in
// RulersHighliter; currently the unit is in pixel.
const RULERS_GRADUATION_STEP = 5;
const RULERS_MARKER_STEP = 50;
const RULERS_TEXT_STEP = 100;

/**
 * The RulersHighlighter is a class that displays both horizontal and
 * vertical rules on the page, along the top and left edges, with pixel
 * graduations, useful for users to quickly check distances
 */
function RulersHighlighter(highlighterEnv) {
  this.env = highlighterEnv;
  this.markup = new CanvasFrameAnonymousContentHelper(highlighterEnv,
    this._buildMarkup.bind(this));

  let { pageListenerTarget } = highlighterEnv;
  pageListenerTarget.addEventListener("scroll", this);
  pageListenerTarget.addEventListener("pagehide", this);
}

RulersHighlighter.prototype = {
  typeName: "RulersHighlighter",

  ID_CLASS_PREFIX: "rulers-highlighter-",

  _buildMarkup: function() {
    let { window } = this.env;
    let prefix = this.ID_CLASS_PREFIX;

    function createRuler(axis, size) {
      let width, height;
      let isHorizontal = true;

      if (axis === "x") {
        width = size;
        height = 16;
      } else if (axis === "y") {
        width = 16;
        height = size;
        isHorizontal = false;
      } else {
        throw new Error(
          `Invalid type of axis given; expected "x" or "y" but got "${axis}"`);
      }

      let g = createSVGNode(window, {
        nodeType: "g",
        attributes: {
          id: `${axis}-axis`
        },
        parent: svg,
        prefix
      });

      createSVGNode(window, {
        nodeType: "rect",
        attributes: {
          y: isHorizontal ? 0 : 16,
          width,
          height
        },
        parent: g
      });

      let gRule = createSVGNode(window, {
        nodeType: "g",
        attributes: {
          id: `${axis}-axis-ruler`
        },
        parent: g,
        prefix
      });

      let pathGraduations = createSVGNode(window, {
        nodeType: "path",
        attributes: {
          "class": "ruler-graduations",
          width,
          height
        },
        parent: gRule,
        prefix
      });

      let pathMarkers = createSVGNode(window, {
        nodeType: "path",
        attributes: {
          "class": "ruler-markers",
          width,
          height
        },
        parent: gRule,
        prefix
      });

      let gText = createSVGNode(window, {
        nodeType: "g",
        attributes: {
          id: `${axis}-axis-text`,
          "class": (isHorizontal ? "horizontal" : "vertical") + "-labels"
        },
        parent: g,
        prefix
      });

      let dGraduations = "";
      let dMarkers = "";
      let graduationLength;

      for (let i = 0; i < size; i += RULERS_GRADUATION_STEP) {
        if (i === 0) {
          continue;
        }

        graduationLength = (i % 2 === 0) ? 6 : 4;

        if (i % RULERS_TEXT_STEP === 0) {
          graduationLength = 8;
          createSVGNode(window, {
            nodeType: "text",
            parent: gText,
            attributes: {
              x: isHorizontal ? 2 + i : -i - 1,
              y: 5
            }
          }).textContent = i;
        }

        if (isHorizontal) {
          if (i % RULERS_MARKER_STEP === 0) {
            dMarkers += `M${i} 0 L${i} ${graduationLength}`;
          } else {
            dGraduations += `M${i} 0 L${i} ${graduationLength} `;
          }
        } else {
          if (i % 50 === 0) {
            dMarkers += `M0 ${i} L${graduationLength} ${i}`;
          } else {
            dGraduations += `M0 ${i} L${graduationLength} ${i}`;
          }
        }
      }

      pathGraduations.setAttribute("d", dGraduations);
      pathMarkers.setAttribute("d", dMarkers);

      return g;
    }

    let container = createNode(window, {
      attributes: {"class": "highlighter-container"}
    });

    let root = createNode(window, {
      parent: container,
      attributes: {
        "id": "root",
        "class": "root"
      },
      prefix
    });

    let svg = createSVGNode(window, {
      nodeType: "svg",
      parent: root,
      attributes: {
        id: "elements",
        "class": "elements",
        width: "100%",
        height: "100%",
        hidden: "true"
      },
      prefix
    });

    createRuler("x", RULERS_MAX_X_AXIS);
    createRuler("y", RULERS_MAX_Y_AXIS);

    return container;
  },

  handleEvent: function(event) {
    switch (event.type) {
      case "scroll":
        this._onScroll(event);
        break;
      case "pagehide":
        this.destroy();
        break;
    }
  },

  _onScroll: function(event) {
    let prefix = this.ID_CLASS_PREFIX;
    let { scrollX, scrollY } = event.view;

    this.markup.getElement(`${prefix}x-axis-ruler`)
                        .setAttribute("transform", `translate(${-scrollX})`);
    this.markup.getElement(`${prefix}x-axis-text`)
                        .setAttribute("transform", `translate(${-scrollX})`);
    this.markup.getElement(`${prefix}y-axis-ruler`)
                        .setAttribute("transform", `translate(0, ${-scrollY})`);
    this.markup.getElement(`${prefix}y-axis-text`)
                        .setAttribute("transform", `translate(0, ${-scrollY})`);
  },

  _update: function() {
    let { window } = this.env;

    setIgnoreLayoutChanges(true);

    let zoom = getCurrentZoom(window);
    let isZoomChanged = zoom !== this._zoom;

    if (isZoomChanged) {
      this._zoom = zoom;
      this.updateViewport();
    }

    setIgnoreLayoutChanges(false, window.document.documentElement);

    this._rafID = window.requestAnimationFrame(() => this._update());
  },

  _cancelUpdate: function() {
    if (this._rafID) {
      this.env.window.cancelAnimationFrame(this._rafID);
      this._rafID = 0;
    }
  },
  updateViewport: function() {
    let { devicePixelRatio } = this.env.window;

    // Because `devicePixelRatio` is affected by zoom (see bug 809788),
    // in order to get the "real" device pixel ratio, we need divide by `zoom`
    let pixelRatio = devicePixelRatio / this._zoom;

    // The "real" device pixel ratio is used to calculate the max stroke
    // width we can actually assign: on retina, for instance, it would be 0.5,
    // where on non high dpi monitor would be 1.
    let minWidth = 1 / pixelRatio;
    let strokeWidth = Math.min(minWidth, minWidth / this._zoom);

    this.markup.getElement(this.ID_CLASS_PREFIX + "root").setAttribute("style",
      `stroke-width:${strokeWidth};`);
  },

  destroy: function() {
    this.hide();

    let { pageListenerTarget } = this.env;
    pageListenerTarget.removeEventListener("scroll", this);
    pageListenerTarget.removeEventListener("pagehide", this);

    this.markup.destroy();

    events.emit(this, "destroy");
  },

  show: function() {
    this.markup.removeAttributeForElement(this.ID_CLASS_PREFIX + "elements",
      "hidden");

    this._update();

    return true;
  },

  hide: function() {
    this.markup.setAttributeForElement(this.ID_CLASS_PREFIX + "elements",
      "hidden", "true");

    this._cancelUpdate();
  }
};
exports.RulersHighlighter = RulersHighlighter;
