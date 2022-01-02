/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const {
  getCurrentZoom,
  getWindowDimensions,
  setIgnoreLayoutChanges,
} = require("devtools/shared/layout/utils");
const {
  CanvasFrameAnonymousContentHelper,
} = require("devtools/server/actors/highlighters/utils/markup");

// Hard coded value about the size of measuring tool label, in order to
// position and flip it when is needed.
const LABEL_SIZE_MARGIN = 8;
const LABEL_SIZE_WIDTH = 80;
const LABEL_SIZE_HEIGHT = 52;
const LABEL_POS_MARGIN = 4;
const LABEL_POS_WIDTH = 40;
const LABEL_POS_HEIGHT = 34;

// List of all DOM Events subscribed directly to the document from the
// Measuring Tool highlighter
const DOM_EVENTS = [
  "mousedown",
  "mousemove",
  "mouseup",
  "mouseleave",
  "scroll",
  "pagehide",
];

const SIDES = ["top", "right", "bottom", "left"];
const HANDLERS = [...SIDES, "topleft", "topright", "bottomleft", "bottomright"];
const HANDLER_SIZE = 6;

/**
 * The MeasuringToolHighlighter is used to measure distances in a content page.
 * It allows users to click and drag with their mouse to draw an area whose
 * dimensions will be displayed in a tooltip next to it.
 * This allows users to measure distances between elements on a page.
 */
function MeasuringToolHighlighter(highlighterEnv) {
  this.env = highlighterEnv;
  this.markup = new CanvasFrameAnonymousContentHelper(
    highlighterEnv,
    this._buildMarkup.bind(this)
  );
  this.isReady = this.markup.initialize();

  this.coords = {
    x: 0,
    y: 0,
  };

  const { pageListenerTarget } = highlighterEnv;

  // Register the measuring tool instance to all events we're interested in.
  DOM_EVENTS.forEach(type => pageListenerTarget.addEventListener(type, this));
}

MeasuringToolHighlighter.prototype = {
  ID_CLASS_PREFIX: "measuring-tool-",

  _buildMarkup() {
    const prefix = this.ID_CLASS_PREFIX;

    const container = this.markup.createNode({
      attributes: { class: "highlighter-container" },
    });

    const root = this.markup.createNode({
      parent: container,
      attributes: {
        id: "root",
        class: "root",
        hidden: "true",
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
      },
      prefix,
    });

    for (const side of SIDES) {
      this.markup.createSVGNode({
        nodeType: "line",
        parent: svg,
        attributes: {
          class: `guide-${side}`,
          id: `guide-${side}`,
          hidden: "true",
        },
        prefix,
      });
    }

    this.markup.createNode({
      nodeType: "label",
      attributes: {
        id: "label-size",
        class: "label-size",
        hidden: "true",
      },
      parent: root,
      prefix,
    });

    this.markup.createNode({
      nodeType: "label",
      attributes: {
        id: "label-position",
        class: "label-position",
        hidden: "true",
      },
      parent: root,
      prefix,
    });

    // Creating a <g> element in order to group all the paths below, that
    // together represent the measuring tool; so that would be easier move them
    // around
    const g = this.markup.createSVGNode({
      nodeType: "g",
      attributes: {
        id: "tool",
      },
      parent: svg,
      prefix,
    });

    this.markup.createSVGNode({
      nodeType: "path",
      attributes: {
        id: "box-path",
        class: "box-path",
      },
      parent: g,
      prefix,
    });

    this.markup.createSVGNode({
      nodeType: "path",
      attributes: {
        id: "diagonal-path",
        class: "diagonal-path",
      },
      parent: g,
      prefix,
    });

    for (const handler of HANDLERS) {
      this.markup.createSVGNode({
        nodeType: "circle",
        parent: g,
        attributes: {
          class: `handler-${handler}`,
          id: `handler-${handler}`,
          r: HANDLER_SIZE,
          hidden: "true",
        },
        prefix,
      });
    }

    return container;
  },

  _update() {
    const { window } = this.env;

    setIgnoreLayoutChanges(true);

    const zoom = getCurrentZoom(window);

    const { width, height } = getWindowDimensions(window);

    const { coords } = this;

    const isZoomChanged = zoom !== coords.zoom;

    if (isZoomChanged) {
      coords.zoom = zoom;
      this.updateLabel();
    }

    const isDocumentSizeChanged =
      width !== coords.documentWidth || height !== coords.documentHeight;

    if (isDocumentSizeChanged) {
      coords.documentWidth = width;
      coords.documentHeight = height;
    }

    // If either the document's size or the zoom is changed since the last
    // repaint, we update the tool's size as well.
    if (isZoomChanged || isDocumentSizeChanged) {
      this.updateViewport();
    }

    setIgnoreLayoutChanges(false, window.document.documentElement);

    this._rafID = window.requestAnimationFrame(() => this._update());
  },

  _cancelUpdate() {
    if (this._rafID) {
      this.env.window.cancelAnimationFrame(this._rafID);
      this._rafID = 0;
    }
  },

  destroy() {
    this.hide();

    this._cancelUpdate();

    const { pageListenerTarget } = this.env;

    if (pageListenerTarget) {
      DOM_EVENTS.forEach(type =>
        pageListenerTarget.removeEventListener(type, this)
      );
    }

    this.markup.destroy();

    EventEmitter.emit(this, "destroy");
  },

  show() {
    setIgnoreLayoutChanges(true);

    this.getElement("root").removeAttribute("hidden");

    this._update();

    setIgnoreLayoutChanges(false, this.env.window.document.documentElement);
  },

  hide() {
    setIgnoreLayoutChanges(true);

    this.hideLabel("size");
    this.hideLabel("position");

    this.getElement("root").setAttribute("hidden", "true");

    this._cancelUpdate();

    setIgnoreLayoutChanges(false, this.env.window.document.documentElement);
  },

  getElement(id) {
    return this.markup.getElement(this.ID_CLASS_PREFIX + id);
  },

  setSize(w, h) {
    this.setCoords(undefined, undefined, w, h);
  },

  setCoords(x, y, w, h) {
    const { coords } = this;

    if (typeof x !== "undefined") {
      coords.x = x;
    }

    if (typeof y !== "undefined") {
      coords.y = y;
    }

    if (typeof w !== "undefined") {
      coords.w = w;
    }

    if (typeof h !== "undefined") {
      coords.h = h;
    }

    setIgnoreLayoutChanges(true);

    if (this._dragging) {
      this.updatePaths();
      this.updateHandlers();
    }

    this.updateLabel();

    setIgnoreLayoutChanges(false, this.env.window.document.documentElement);
  },

  updatePaths() {
    const { x, y, w, h } = this.coords;
    const dir = `M0 0 L${w} 0 L${w} ${h} L0 ${h}z`;

    // Adding correction to the line path, otherwise some pixels are drawn
    // outside the main rectangle area.
    const x1 = w > 0 ? 0.5 : 0;
    const y1 = w < 0 && h < 0 ? -0.5 : 0;
    const w1 = w + (h < 0 && w < 0 ? 0.5 : 0);
    const h1 = h + (h > 0 && w > 0 ? -0.5 : 0);

    const linedir = `M${x1} ${y1} L${w1} ${h1}`;

    this.getElement("box-path").setAttribute("d", dir);
    this.getElement("diagonal-path").setAttribute("d", linedir);
    this.getElement("tool").setAttribute("transform", `translate(${x},${y})`);
  },

  updateLabel(type) {
    type = type || (this._dragging ? "size" : "position");

    const isSizeLabel = type === "size";

    const label = this.getElement(`label-${type}`);

    let origin = "top left";

    const { innerWidth, innerHeight, scrollX, scrollY } = this.env.window;
    let { x, y, w, h, zoom } = this.coords;
    const scale = 1 / zoom;

    w = w || 0;
    h = h || 0;
    x = x || 0;
    y = y || 0;
    if (type === "size") {
      x += w;
      y += h;
    }

    let labelMargin, labelHeight, labelWidth;

    if (isSizeLabel) {
      labelMargin = LABEL_SIZE_MARGIN;
      labelWidth = LABEL_SIZE_WIDTH;
      labelHeight = LABEL_SIZE_HEIGHT;

      const d = Math.hypot(w, h).toFixed(2);

      label.setTextContent(`W: ${Math.abs(w)} px
                            H: ${Math.abs(h)} px
                            ↘: ${d}px`);
    } else {
      labelMargin = LABEL_POS_MARGIN;
      labelWidth = LABEL_POS_WIDTH;
      labelHeight = LABEL_POS_HEIGHT;

      label.setTextContent(`${x}
                            ${y}`);
    }

    // Size used to position properly the label
    const labelBoxWidth = (labelWidth + labelMargin) * scale;
    const labelBoxHeight = (labelHeight + labelMargin) * scale;

    const isGoingLeft = w < scrollX;
    const isSizeGoingLeft = isSizeLabel && isGoingLeft;
    const isExceedingLeftMargin = x - labelBoxWidth < scrollX;
    const isExceedingRightMargin = x + labelBoxWidth > innerWidth + scrollX;
    const isExceedingTopMargin = y - labelBoxHeight < scrollY;
    const isExceedingBottomMargin = y + labelBoxHeight > innerHeight + scrollY;

    if ((isSizeGoingLeft && !isExceedingLeftMargin) || isExceedingRightMargin) {
      x -= labelBoxWidth;
      origin = "top right";
    } else {
      x += labelMargin * scale;
    }

    if (isSizeLabel) {
      y += isExceedingTopMargin ? labelMargin * scale : -labelBoxHeight;
    } else {
      y += isExceedingBottomMargin ? -labelBoxHeight : labelMargin * scale;
    }

    label.setAttribute(
      "style",
      `
      width: ${labelWidth}px;
      height: ${labelHeight}px;
      transform-origin: ${origin};
      transform: translate(${x}px,${y}px) scale(${scale})
    `
    );

    if (!isSizeLabel) {
      const labelSize = this.getElement("label-size");
      const style = labelSize.getAttribute("style");

      if (style) {
        labelSize.setAttribute(
          "style",
          style.replace(/scale[^)]+\)/, `scale(${scale})`)
        );
      }
    }
  },

  updateViewport() {
    const { devicePixelRatio } = this.env.window;
    const { documentWidth, documentHeight, zoom } = this.coords;

    // Because `devicePixelRatio` is affected by zoom (see bug 809788),
    // in order to get the "real" device pixel ratio, we need divide by `zoom`
    const pixelRatio = devicePixelRatio / zoom;

    // The "real" device pixel ratio is used to calculate the max stroke
    // width we can actually assign: on retina, for instance, it would be 0.5,
    // where on non high dpi monitor would be 1.
    const minWidth = 1 / pixelRatio;
    const strokeWidth = minWidth / zoom;

    this.getElement("root").setAttribute(
      "style",
      `stroke-width:${strokeWidth};
       width:${documentWidth}px;
       height:${documentHeight}px;`
    );
  },

  updateGuides() {
    const { x, y, w, h } = this.coords;

    let guide = this.getElement("guide-top");

    guide.setAttribute("x1", "0");
    guide.setAttribute("y1", y);
    guide.setAttribute("x2", "100%");
    guide.setAttribute("y2", y);

    guide = this.getElement("guide-right");

    guide.setAttribute("x1", x + w);
    guide.setAttribute("y1", 0);
    guide.setAttribute("x2", x + w);
    guide.setAttribute("y2", "100%");

    guide = this.getElement("guide-bottom");

    guide.setAttribute("x1", "0");
    guide.setAttribute("y1", y + h);
    guide.setAttribute("x2", "100%");
    guide.setAttribute("y2", y + h);

    guide = this.getElement("guide-left");

    guide.setAttribute("x1", x);
    guide.setAttribute("y1", 0);
    guide.setAttribute("x2", x);
    guide.setAttribute("y2", "100%");
  },

  setHandlerPosition(handler, x, y) {
    const handlerElement = this.getElement(`handler-${handler}`);
    handlerElement.setAttribute("cx", x);
    handlerElement.setAttribute("cy", y);
  },

  updateHandlers() {
    const { w, h } = this.coords;

    this.setHandlerPosition("top", w / 2, 0);
    this.setHandlerPosition("topright", w, 0);
    this.setHandlerPosition("right", w, h / 2);
    this.setHandlerPosition("bottomright", w, h);
    this.setHandlerPosition("bottom", w / 2, h);
    this.setHandlerPosition("bottomleft", 0, h);
    this.setHandlerPosition("left", 0, h / 2);
    this.setHandlerPosition("topleft", 0, 0);
  },

  showLabel(type) {
    setIgnoreLayoutChanges(true);

    this.getElement(`label-${type}`).removeAttribute("hidden");

    setIgnoreLayoutChanges(false, this.env.window.document.documentElement);
  },

  hideLabel(type) {
    setIgnoreLayoutChanges(true);

    this.getElement(`label-${type}`).setAttribute("hidden", "true");

    setIgnoreLayoutChanges(false, this.env.window.document.documentElement);
  },

  showGuides() {
    const prefix = this.ID_CLASS_PREFIX + "guide-";

    for (const side of SIDES) {
      this.markup.removeAttributeForElement(`${prefix + side}`, "hidden");
    }
  },

  hideGuides() {
    const prefix = this.ID_CLASS_PREFIX + "guide-";

    for (const side of SIDES) {
      this.markup.setAttributeForElement(`${prefix + side}`, "hidden", "true");
    }
  },

  showHandler(id) {
    const prefix = this.ID_CLASS_PREFIX + "handler-";
    this.markup.removeAttributeForElement(prefix + id, "hidden");
  },

  showHandlers() {
    const prefix = this.ID_CLASS_PREFIX + "handler-";

    for (const handler of HANDLERS) {
      this.markup.removeAttributeForElement(prefix + handler, "hidden");
    }
  },

  hideAll() {
    this.hideLabel("position");
    this.hideLabel("size");
    this.hideGuides();
    this.hideHandlers();
  },

  showGuidesAndHandlers() {
    // Shows the guides and handlers only if an actual area is selected
    if (this.coords.w !== 0 && this.coords.h !== 0) {
      this.updateGuides();
      this.showGuides();
      this.updateHandlers();
      this.showHandlers();
    }
  },

  hideHandlers() {
    const prefix = this.ID_CLASS_PREFIX + "handler-";

    for (const handler of HANDLERS) {
      this.markup.setAttributeForElement(prefix + handler, "hidden", "true");
    }
  },

  handleEvent(event) {
    const { target, type } = event;

    switch (type) {
      case "mousedown":
        if (event.button || this._dragging) {
          return;
        }

        const isHandler = event.originalTarget.id.includes("handler");
        if (isHandler) {
          this.handleResizingMouseDownEvent(event);
        } else {
          this.handleMouseDownEvent(event);
        }
        break;
      case "mousemove":
        if (this._dragging && this._dragging.handler) {
          this.handleResizingMouseMoveEvent(event);
        } else {
          this.handleMouseMoveEvent(event);
        }
        break;
      case "mouseup":
        if (this._dragging) {
          if (this._dragging.handler) {
            this.handleResizingMouseUpEvent();
          } else {
            this.handleMouseUpEvent();
          }
        }
        break;
      case "mouseleave": {
        if (!this._dragging) {
          this.hideLabel("position");
        }
        break;
      }
      case "scroll": {
        this.hideLabel("position");
        break;
      }
      case "pagehide": {
        // If a page hide event is triggered for current window's highlighter, hide the
        // highlighter.
        if (target.defaultView === this.env.window) {
          this.destroy();
        }
        break;
      }
    }
  },

  handleMouseDownEvent(event) {
    const { pageX, pageY } = event;
    const { window } = this.env;
    const elementId = `${this.ID_CLASS_PREFIX}tool`;

    setIgnoreLayoutChanges(true);

    this.markup.getElement(elementId).classList.add("dragging");

    this.hideAll();

    setIgnoreLayoutChanges(false, window.document.documentElement);

    // Store all the initial values needed for drag & drop
    this._dragging = {
      handler: null,
      x: pageX,
      y: pageY,
    };

    this.setCoords(pageX, pageY, 0, 0);
  },

  handleMouseMoveEvent(event) {
    const { pageX, pageY } = event;
    const { coords } = this;
    let { x, y, w, h } = coords;
    let labelType;

    if (this._dragging) {
      w = pageX - coords.x;
      h = pageY - coords.y;

      this.setCoords(x, y, w, h);

      labelType = "size";
    } else {
      labelType = "position";

      this.setCoords(pageX, pageY);
    }

    this.showLabel(labelType);
  },

  handleMouseUpEvent() {
    setIgnoreLayoutChanges(true);

    this.getElement("tool").classList.remove("dragging");

    this.showGuidesAndHandlers();

    setIgnoreLayoutChanges(false, this.env.window.document.documentElement);
    this._dragging = null;
  },

  handleResizingMouseDownEvent(event) {
    const { originalTarget, pageX, pageY } = event;
    const { window } = this.env;
    const prefix = this.ID_CLASS_PREFIX + "handler-";
    const handler = originalTarget.id.replace(prefix, "");

    setIgnoreLayoutChanges(true);

    this.markup.getElement(originalTarget.id).classList.add("dragging");

    this.hideAll();
    this.showHandler(handler);

    // Set coordinates to the current measurement area's position
    const [, x, y] = this.getElement("tool")
      .getAttribute("transform")
      .match(/(\d+),(\d+)/);
    this.setCoords(Number(x), Number(y));

    setIgnoreLayoutChanges(false, window.document.documentElement);

    // Store all the initial values needed for drag & drop
    this._dragging = {
      handler,
      x: pageX,
      y: pageY,
    };
  },

  handleResizingMouseMoveEvent(event) {
    const { pageX, pageY } = event;
    const { coords } = this;
    let { x, y, w, h } = coords;

    const { handler } = this._dragging;

    switch (handler) {
      case "top":
        y = pageY;
        h = coords.y + coords.h - pageY;
        break;
      case "topright":
        y = pageY;
        w = pageX - coords.x;
        h = coords.y + coords.h - pageY;
        break;
      case "right":
        w = pageX - coords.x;
        break;
      case "bottomright":
        w = pageX - coords.x;
        h = pageY - coords.y;
        break;
      case "bottom":
        h = pageY - coords.y;
        break;
      case "bottomleft":
        x = pageX;
        w = coords.x + coords.w - pageX;
        h = pageY - coords.y;
        break;
      case "left":
        x = pageX;
        w = coords.x + coords.w - pageX;
        break;
      case "topleft":
        x = pageX;
        y = pageY;
        w = coords.x + coords.w - pageX;
        h = coords.y + coords.h - pageY;
        break;
    }

    this.setCoords(x, y, w, h);

    // Changes the resizing cursors in case the measuring box is mirrored
    const isMirrored =
      (coords.w < 0 || coords.h < 0) && !(coords.w < 0 && coords.h < 0);
    this.getElement("tool").classList.toggle("mirrored", isMirrored);

    this.showLabel("size");
  },

  handleResizingMouseUpEvent() {
    const { handler } = this._dragging;

    setIgnoreLayoutChanges(true);

    this.getElement(`handler-${handler}`).classList.remove("dragging");
    this.showHandlers();

    this.showGuidesAndHandlers();

    setIgnoreLayoutChanges(false, this.env.window.document.documentElement);
    this._dragging = null;
  },
};
exports.MeasuringToolHighlighter = MeasuringToolHighlighter;
