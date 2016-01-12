/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const events = require("sdk/event/core");
const { getCurrentZoom,
  setIgnoreLayoutChanges } = require("devtools/shared/layout/utils");
const {
  CanvasFrameAnonymousContentHelper,
  createSVGNode, createNode } = require("./utils/markup");

// Hard coded value about the size of measuring tool label, in order to
// position and flip it when is needed.
const LABEL_SIZE_MARGIN = 8;
const LABEL_SIZE_WIDTH = 80;
const LABEL_SIZE_HEIGHT = 52;
const LABEL_POS_MARGIN = 4;
const LABEL_POS_WIDTH = 40;
const LABEL_POS_HEIGHT = 34;

const SIDES = ["top", "right", "bottom", "left"];

/**
 * The MeasuringToolHighlighter is used to measure distances in a content page.
 * It allows users to click and drag with their mouse to draw an area whose
 * dimensions will be displayed in a tooltip next to it.
 * This allows users to measure distances between elements on a page.
 */
function MeasuringToolHighlighter(highlighterEnv) {
  this.env = highlighterEnv;
  this.markup = new CanvasFrameAnonymousContentHelper(highlighterEnv,
    this._buildMarkup.bind(this));

  this.coords = {
    x: 0,
    y: 0
  };

  let { pageListenerTarget } = highlighterEnv;

  pageListenerTarget.addEventListener("mousedown", this);
  pageListenerTarget.addEventListener("mousemove", this);
  pageListenerTarget.addEventListener("mouseleave", this);
  pageListenerTarget.addEventListener("scroll", this);
  pageListenerTarget.addEventListener("pagehide", this);
}

MeasuringToolHighlighter.prototype = {
  typeName: "MeasuringToolHighlighter",

  ID_CLASS_PREFIX: "measuring-tool-highlighter-",

  _buildMarkup() {
    let prefix = this.ID_CLASS_PREFIX;
    let { window } = this.env;

    let container = createNode(window, {
      attributes: {"class": "highlighter-container"}
    });

    let root = createNode(window, {
      parent: container,
      attributes: {
        "id": "root",
        "class": "root",
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

    createNode(window, {
      nodeType: "label",
      attributes: {
        id: "label-size",
        "class": "label-size",
        "hidden": "true"
      },
      parent: root,
      prefix
    });

    createNode(window, {
      nodeType: "label",
      attributes: {
        id: "label-position",
        "class": "label-position",
        "hidden": "true"
      },
      parent: root,
      prefix
    });

    // Creating a <g> element in order to group all the paths below, that
    // together represent the measuring tool; so that would be easier move them
    // around
    let g = createSVGNode(window, {
      nodeType: "g",
      attributes: {
        id: "tool",
      },
      parent: svg,
      prefix
    });

    createSVGNode(window, {
      nodeType: "path",
      attributes: {
        id: "box-path"
      },
      parent: g,
      prefix
    });

    createSVGNode(window, {
      nodeType: "path",
      attributes: {
        id: "diagonal-path"
      },
      parent: g,
      prefix
    });

    for (let side of SIDES) {
      createSVGNode(window, {
        nodeType: "line",
        parent: svg,
        attributes: {
          "class": `guide-${side}`,
          id: `guide-${side}`,
          hidden: "true"
        },
        prefix
      });
    }

    return container;
  },

  _update() {
    let { window } = this.env;

    setIgnoreLayoutChanges(true);

    let zoom = getCurrentZoom(window);

    let { documentElement } = window.document;

    let width = Math.max(documentElement.clientWidth,
                         documentElement.scrollWidth,
                         documentElement.offsetWidth);

    let height = Math.max(documentElement.clientHeight,
                          documentElement.scrollHeight,
                          documentElement.offsetHeight);

    let { body } = window.document;

    // get the size of the content document despite the compatMode
    if (body) {
      width = Math.max(width, body.scrollWidth, body.offsetWidth);
      height = Math.max(height, body.scrollHeight, body.offsetHeight);
    }

    let { coords } = this;

    let isZoomChanged = zoom !== coords.zoom;

    if (isZoomChanged) {
      coords.zoom = zoom;
      this.updateLabel();
    }

    let isDocumentSizeChanged = width !== coords.documentWidth ||
                                height !== coords.documentHeight;

    if (isDocumentSizeChanged) {
      coords.documentWidth = width;
      coords.documentHeight = height;
    }

    // If either the document's size or the zoom is changed since the last
    // repaint, we update the tool's size as well.
    if (isZoomChanged || isDocumentSizeChanged) {
      this.updateViewport();
    }

    setIgnoreLayoutChanges(false, documentElement);

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

    let { pageListenerTarget } = this.env;

    pageListenerTarget.removeEventListener("mousedown", this);
    pageListenerTarget.removeEventListener("mousemove", this);
    pageListenerTarget.removeEventListener("mouseup", this);
    pageListenerTarget.removeEventListener("scroll", this);
    pageListenerTarget.removeEventListener("pagehide", this);
    pageListenerTarget.removeEventListener("mouseleave", this);

    this.markup.destroy();

    events.emit(this, "destroy");
  },

  show() {
    setIgnoreLayoutChanges(true);

    this.getElement("elements").removeAttribute("hidden");

    this._update();

    setIgnoreLayoutChanges(false, this.env.window.document.documentElement);
  },

  hide() {
    setIgnoreLayoutChanges(true);

    this.hideLabel("size");
    this.hideLabel("position");

    this.getElement("elements").setAttribute("hidden", "true");

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
    let { coords } = this;

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

    if (this._isDragging) {
      this.updatePaths();
    }

    this.updateLabel();

    setIgnoreLayoutChanges(false, this.env.window.document.documentElement);
  },

  updatePaths() {
    let { x, y, w, h } = this.coords;
    let dir = `M0 0 L${w} 0 L${w} ${h} L0 ${h}z`;

    // Adding correction to the line path, otherwise some pixels are drawn
    // outside the main rectangle area.
    let x1 = w > 0 ? 0.5 : 0;
    let y1 = w < 0 && h < 0 ? -0.5 : 0;
    let w1 = w + (h < 0 && w < 0 ? 0.5 : 0);
    let h1 = h + (h > 0 && w > 0 ? -0.5 : 0);

    let linedir = `M${x1} ${y1} L${w1} ${h1}`;

    this.getElement("box-path").setAttribute("d", dir);
    this.getElement("diagonal-path").setAttribute("d", linedir);
    this.getElement("tool").setAttribute("transform", `translate(${x},${y})`);
  },

  updateLabel(type) {
    type = type || this._isDragging ? "size" : "position";

    let isSizeLabel = type === "size";

    let label = this.getElement(`label-${type}`);

    let origin = "top left";

    let { innerWidth, innerHeight, scrollX, scrollY } = this.env.window;
    let { x, y, w, h, zoom } = this.coords;
    let scale = 1 / zoom;

    w = w || 0;
    h = h || 0;
    x = (x || 0) + w;
    y = (y || 0) + h;

    let labelMargin, labelHeight, labelWidth;

    if (isSizeLabel) {
      labelMargin = LABEL_SIZE_MARGIN;
      labelWidth = LABEL_SIZE_WIDTH;
      labelHeight = LABEL_SIZE_HEIGHT;

      let d = Math.hypot(w, h).toFixed(2);

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
    let labelBoxWidth = (labelWidth + labelMargin) * scale;
    let labelBoxHeight = (labelHeight + labelMargin) * scale;

    let isGoingLeft = w < scrollX;
    let isSizeGoingLeft = isSizeLabel && isGoingLeft;
    let isExceedingLeftMargin = x - labelBoxWidth < scrollX;
    let isExceedingRightMargin = x + labelBoxWidth > innerWidth + scrollX;
    let isExceedingTopMargin = y - labelBoxHeight < scrollY;
    let isExceedingBottomMargin = y + labelBoxHeight > innerHeight + scrollY;

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

    label.setAttribute("style", `
      width: ${labelWidth}px;
      height: ${labelHeight}px;
      transform-origin: ${origin};
      transform: translate(${x}px,${y}px) scale(${scale})
    `);

    if (!isSizeLabel) {
      let labelSize = this.getElement("label-size");
      let style = labelSize.getAttribute("style");

      if (style) {
        labelSize.setAttribute("style",
          style.replace(/scale[^)]+\)/, `scale(${scale})`));
      }
    }
  },

  updateViewport() {
    let { scrollX, scrollY, devicePixelRatio } = this.env.window;
    let { documentWidth, documentHeight, zoom } = this.coords;

    // Because `devicePixelRatio` is affected by zoom (see bug 809788),
    // in order to get the "real" device pixel ratio, we need divide by `zoom`
    let pixelRatio = devicePixelRatio / zoom;

    // The "real" device pixel ratio is used to calculate the max stroke
    // width we can actually assign: on retina, for instance, it would be 0.5,
    // where on non high dpi monitor would be 1.
    let minWidth = 1 / pixelRatio;
    let strokeWidth = Math.min(minWidth, minWidth / zoom);

    this.getElement("root").setAttribute("style",
      `stroke-width:${strokeWidth};
       width:${documentWidth}px;
       height:${documentHeight}px;
       transform: translate(${-scrollX}px,${-scrollY}px)`);
  },

  updateGuides() {
    let { x, y, w, h } = this.coords;

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
    let prefix = this.ID_CLASS_PREFIX + "guide-";

    for (let side of SIDES) {
      this.markup.removeAttributeForElement(`${prefix + side}`, "hidden");
    }
  },

  hideGuides() {
    let prefix = this.ID_CLASS_PREFIX + "guide-";

    for (let side of SIDES) {
      this.markup.setAttributeForElement(`${prefix + side}`, "hidden", "true");
    }
  },

  handleEvent(event) {
    let scrollX, scrollY, innerWidth, innerHeight;
    let x, y;

    let { pageListenerTarget } = this.env;

    switch (event.type) {
      case "mousedown":
        if (event.button) {
          return;
        }

        this._isDragging = true;

        let { window } = this.env;

        ({ scrollX, scrollY } = window);
        x = event.clientX + scrollX;
        y = event.clientY + scrollY;

        pageListenerTarget.addEventListener("mouseup", this);

        setIgnoreLayoutChanges(true);

        this.getElement("tool").setAttribute("class", "dragging");

        this.hideLabel("size");
        this.hideLabel("position");

        this.hideGuides();
        this.setCoords(x, y, 0, 0);

        setIgnoreLayoutChanges(false, window.document.documentElement);

        break;
      case "mouseup":
        this._isDragging = false;

        pageListenerTarget.removeEventListener("mouseup", this);

        setIgnoreLayoutChanges(true);

        this.getElement("tool").removeAttribute("class", "");

        // Shows the guides only if an actual area is selected
        if (this.coords.w !== 0 && this.coords.h !== 0) {
          this.updateGuides();
          this.showGuides();
        }

        setIgnoreLayoutChanges(false, this.env.window.document.documentElement);

        break;
      case "mousemove":
        ({ scrollX, scrollY, innerWidth, innerHeight } = this.env.window);
        x = event.clientX + scrollX;
        y = event.clientY + scrollY;

        let { coords } = this;

        x = Math.min(innerWidth + scrollX - 1, Math.max(0 + scrollX, x));
        y = Math.min(innerHeight + scrollY, Math.max(1 + scrollY, y));

        this.setSize(x - coords.x, y - coords.y);

        let type = this._isDragging ? "size" : "position";

        this.showLabel(type);
        break;
      case "mouseleave":
        if (!this._isDragging) {
          this.hideLabel("position");
        }
        break;
      case "scroll":
        setIgnoreLayoutChanges(true);
        this.updateViewport();
        setIgnoreLayoutChanges(false, this.env.window.document.documentElement);

        break;
      case "pagehide":
        this.destroy();
        break;
    }
  }
};
exports.MeasuringToolHighlighter = MeasuringToolHighlighter;
