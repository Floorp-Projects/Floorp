/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const {
  getCurrentZoom,
  setIgnoreLayoutChanges,
} = require("devtools/shared/layout/utils");
const {
  CanvasFrameAnonymousContentHelper,
} = require("devtools/server/actors/highlighters/utils/markup");

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
  this.markup = new CanvasFrameAnonymousContentHelper(
    highlighterEnv,
    this._buildMarkup.bind(this)
  );
  this.isReady = this.markup.initialize();

  const { pageListenerTarget } = highlighterEnv;
  pageListenerTarget.addEventListener("scroll", this);
  pageListenerTarget.addEventListener("pagehide", this);
}

RulersHighlighter.prototype = {
  ID_CLASS_PREFIX: "rulers-highlighter-",

  _buildMarkup() {
    const prefix = this.ID_CLASS_PREFIX;

    const createRuler = (axis, size) => {
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
          `Invalid type of axis given; expected "x" or "y" but got "${axis}"`
        );
      }

      const g = this.markup.createSVGNode({
        nodeType: "g",
        attributes: {
          id: `${axis}-axis`,
        },
        parent: svg,
        prefix,
      });

      this.markup.createSVGNode({
        nodeType: "rect",
        attributes: {
          y: isHorizontal ? 0 : 16,
          width,
          height,
        },
        parent: g,
      });

      const gRule = this.markup.createSVGNode({
        nodeType: "g",
        attributes: {
          id: `${axis}-axis-ruler`,
        },
        parent: g,
        prefix,
      });

      const pathGraduations = this.markup.createSVGNode({
        nodeType: "path",
        attributes: {
          class: "ruler-graduations",
          width,
          height,
        },
        parent: gRule,
        prefix,
      });

      const pathMarkers = this.markup.createSVGNode({
        nodeType: "path",
        attributes: {
          class: "ruler-markers",
          width,
          height,
        },
        parent: gRule,
        prefix,
      });

      const gText = this.markup.createSVGNode({
        nodeType: "g",
        attributes: {
          id: `${axis}-axis-text`,
          class: (isHorizontal ? "horizontal" : "vertical") + "-labels",
        },
        parent: g,
        prefix,
      });

      let dGraduations = "";
      let dMarkers = "";
      let graduationLength;

      for (let i = 0; i < size; i += RULERS_GRADUATION_STEP) {
        if (i === 0) {
          continue;
        }

        graduationLength = i % 2 === 0 ? 6 : 4;

        if (i % RULERS_TEXT_STEP === 0) {
          graduationLength = 8;
          this.markup.createSVGNode({
            nodeType: "text",
            parent: gText,
            attributes: {
              x: isHorizontal ? 2 + i : -i - 1,
              y: 5,
            },
          }).textContent = i;
        }

        if (isHorizontal) {
          if (i % RULERS_MARKER_STEP === 0) {
            dMarkers += `M${i} 0 L${i} ${graduationLength}`;
          } else {
            dGraduations += `M${i} 0 L${i} ${graduationLength} `;
          }
        } else if (i % 50 === 0) {
          dMarkers += `M0 ${i} L${graduationLength} ${i}`;
        } else {
          dGraduations += `M0 ${i} L${graduationLength} ${i}`;
        }
      }

      pathGraduations.setAttribute("d", dGraduations);
      pathMarkers.setAttribute("d", dMarkers);

      return g;
    };

    const container = this.markup.createNode({
      attributes: { class: "highlighter-container" },
    });

    const root = this.markup.createNode({
      parent: container,
      attributes: {
        id: "root",
        class: "root",
      },
      prefix,
    });

    const svg = this.markup.createSVGNode({
      nodeType: "svg",
      parent: root,
      attributes: {
        id: "elements",
        class: "elements",
        width: "100%",
        height: "100%",
        hidden: "true",
      },
      prefix,
    });

    createRuler("x", RULERS_MAX_X_AXIS);
    createRuler("y", RULERS_MAX_Y_AXIS);

    this.markup.createNode({
      parent: container,
      attributes: {
        class: "viewport-infobar-container",
        id: "viewport-infobar-container",
        position: "top",
      },
      prefix,
    });

    return container;
  },

  handleEvent(event) {
    switch (event.type) {
      case "scroll":
        this._onScroll(event);
        break;
      case "pagehide":
        // If a page hide event is triggered for current window's highlighter, hide the
        // highlighter.
        if (event.target.defaultView === this.env.window) {
          this.destroy();
        }
        break;
    }
  },

  _onScroll(event) {
    const prefix = this.ID_CLASS_PREFIX;
    const { scrollX, scrollY } = event.view;

    this.markup
      .getElement(`${prefix}x-axis-ruler`)
      .setAttribute("transform", `translate(${-scrollX})`);
    this.markup
      .getElement(`${prefix}x-axis-text`)
      .setAttribute("transform", `translate(${-scrollX})`);
    this.markup
      .getElement(`${prefix}y-axis-ruler`)
      .setAttribute("transform", `translate(0, ${-scrollY})`);
    this.markup
      .getElement(`${prefix}y-axis-text`)
      .setAttribute("transform", `translate(0, ${-scrollY})`);
  },

  _update() {
    const { window } = this.env;

    setIgnoreLayoutChanges(true);

    const zoom = getCurrentZoom(window);
    const isZoomChanged = zoom !== this._zoom;

    if (isZoomChanged) {
      this._zoom = zoom;
      this.updateViewport();
    }

    this.updateViewportInfobar();

    setIgnoreLayoutChanges(false, window.document.documentElement);

    this._rafID = window.requestAnimationFrame(() => this._update());
  },

  _cancelUpdate() {
    if (this._rafID) {
      this.env.window.cancelAnimationFrame(this._rafID);
      this._rafID = 0;
    }
  },
  updateViewport() {
    const { devicePixelRatio } = this.env.window;

    // Because `devicePixelRatio` is affected by zoom (see bug 809788),
    // in order to get the "real" device pixel ratio, we need divide by `zoom`
    const pixelRatio = devicePixelRatio / this._zoom;

    // The "real" device pixel ratio is used to calculate the max stroke
    // width we can actually assign: on retina, for instance, it would be 0.5,
    // where on non high dpi monitor would be 1.
    const minWidth = 1 / pixelRatio;
    const strokeWidth = Math.min(minWidth, minWidth / this._zoom);

    this.markup
      .getElement(this.ID_CLASS_PREFIX + "root")
      .setAttribute("style", `stroke-width:${strokeWidth};`);
  },

  updateViewportInfobar() {
    const { window } = this.env;
    const { innerHeight, innerWidth } = window;
    const infobarId = this.ID_CLASS_PREFIX + "viewport-infobar-container";
    const textContent = innerWidth + "px \u00D7 " + innerHeight + "px";
    this.markup.getElement(infobarId).setTextContent(textContent);
  },

  destroy() {
    this.hide();

    const { pageListenerTarget } = this.env;

    if (pageListenerTarget) {
      pageListenerTarget.removeEventListener("scroll", this);
      pageListenerTarget.removeEventListener("pagehide", this);
    }

    this.markup.destroy();

    EventEmitter.emit(this, "destroy");
  },

  show() {
    this.markup.removeAttributeForElement(
      this.ID_CLASS_PREFIX + "elements",
      "hidden"
    );
    this.markup.removeAttributeForElement(
      this.ID_CLASS_PREFIX + "viewport-infobar-container",
      "hidden"
    );

    this._update();

    return true;
  },

  hide() {
    this.markup.setAttributeForElement(
      this.ID_CLASS_PREFIX + "elements",
      "hidden",
      "true"
    );
    this.markup.setAttributeForElement(
      this.ID_CLASS_PREFIX + "viewport-infobar-container",
      "hidden",
      "true"
    );

    this._cancelUpdate();
  },
};
exports.RulersHighlighter = RulersHighlighter;
