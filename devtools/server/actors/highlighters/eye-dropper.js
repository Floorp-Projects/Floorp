/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Eye-dropper tool. This is implemented as a highlighter so it can be displayed in the
// content page.
// It basically displays a magnifier that tracks mouse moves and shows a magnified version
// of the page. On click, it samples the color at the pixel being hovered.

const { Ci, Cc } = require("chrome");
const {
  CanvasFrameAnonymousContentHelper,
} = require("devtools/server/actors/highlighters/utils/markup");
const Services = require("Services");
const EventEmitter = require("devtools/shared/event-emitter");
const {
  rgbToHsl,
  rgbToColorName,
} = require("devtools/shared/css/color").colorUtils;
const {
  getCurrentZoom,
  getFrameOffsets,
} = require("devtools/shared/layout/utils");

loader.lazyGetter(this, "clipboardHelper", () =>
  Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper)
);
loader.lazyGetter(this, "l10n", () =>
  Services.strings.createBundle(
    "chrome://devtools-shared/locale/eyedropper.properties"
  )
);

const ZOOM_LEVEL_PREF = "devtools.eyedropper.zoom";
const FORMAT_PREF = "devtools.defaultColorUnit";
// Width of the canvas.
const MAGNIFIER_WIDTH = 96;
// Height of the canvas.
const MAGNIFIER_HEIGHT = 96;
// Start position, when the tool is first shown. This should match the top/left position
// defined in CSS.
const DEFAULT_START_POS_X = 100;
const DEFAULT_START_POS_Y = 100;
// How long to wait before closing after copy.
const CLOSE_DELAY = 750;

/**
 * The EyeDropper allows the user to select a color of a pixel within the content page,
 * showing a magnified circle and color preview while the user hover the page.
 */
class EyeDropper {
  #pageEventListenersAbortController;
  constructor(highlighterEnv) {
    EventEmitter.decorate(this);

    this.highlighterEnv = highlighterEnv;
    this.markup = new CanvasFrameAnonymousContentHelper(
      this.highlighterEnv,
      this._buildMarkup.bind(this)
    );
    this.isReady = this.markup.initialize();

    // Get a couple of settings from prefs.
    this.format = Services.prefs.getCharPref(FORMAT_PREF);
    this.eyeDropperZoomLevel = Services.prefs.getIntPref(ZOOM_LEVEL_PREF);
  }

  ID_CLASS_PREFIX = "eye-dropper-";

  get win() {
    return this.highlighterEnv.window;
  }

  _buildMarkup() {
    // Highlighter main container.
    const container = this.markup.createNode({
      attributes: { class: "highlighter-container" },
    });

    // Wrapper element.
    const wrapper = this.markup.createNode({
      parent: container,
      attributes: {
        id: "root",
        class: "root",
        hidden: "true",
      },
      prefix: this.ID_CLASS_PREFIX,
    });

    // The magnifier canvas element.
    this.markup.createNode({
      parent: wrapper,
      nodeType: "canvas",
      attributes: {
        id: "canvas",
        class: "canvas",
        width: MAGNIFIER_WIDTH,
        height: MAGNIFIER_HEIGHT,
      },
      prefix: this.ID_CLASS_PREFIX,
    });

    // The color label element.
    const colorLabelContainer = this.markup.createNode({
      parent: wrapper,
      attributes: { class: "color-container" },
      prefix: this.ID_CLASS_PREFIX,
    });
    this.markup.createNode({
      nodeType: "div",
      parent: colorLabelContainer,
      attributes: { id: "color-preview", class: "color-preview" },
      prefix: this.ID_CLASS_PREFIX,
    });
    this.markup.createNode({
      nodeType: "div",
      parent: colorLabelContainer,
      attributes: { id: "color-value", class: "color-value" },
      prefix: this.ID_CLASS_PREFIX,
    });

    return container;
  }

  destroy() {
    this.hide();
    this.markup.destroy();
  }

  getElement(id) {
    return this.markup.getElement(this.ID_CLASS_PREFIX + id);
  }

  /**
   * Show the eye-dropper highlighter.
   *
   * @param {DOMNode} node The node which document the highlighter should be inserted in.
   * @param {Object} options The options object may contain the following properties:
   * - {Boolean} copyOnSelect: Whether selecting a color should copy it to the clipboard.
   * - {String|null} screenshot: a dataURL representation of the page screenshot. If null,
   *                 the eyedropper will use `drawWindow` to get the the screenshot
   *                 (⚠️ but it won't handle remote frames).
   */
  show(node, options = {}) {
    if (this.highlighterEnv.isXUL) {
      return false;
    }

    this.options = options;

    // Get the page's current zoom level.
    this.pageZoom = getCurrentZoom(this.win);

    // Take a screenshot of the viewport. This needs to be done first otherwise the
    // eyedropper UI will appear in the screenshot itself (since the UI is injected as
    // native anonymous content in the page).
    // Once the screenshot is ready, the magnified area will be drawn.
    this.prepareImageCapture(options.screenshot);

    // Start listening for user events.
    const { pageListenerTarget } = this.highlighterEnv;
    this.#pageEventListenersAbortController = new AbortController();
    const signal = this.#pageEventListenersAbortController.signal;
    pageListenerTarget.addEventListener("mousemove", this, { signal });
    pageListenerTarget.addEventListener("click", this, {
      signal,
      useCapture: true,
    });
    pageListenerTarget.addEventListener("keydown", this, { signal });
    pageListenerTarget.addEventListener("DOMMouseScroll", this, { signal });
    pageListenerTarget.addEventListener("FullZoomChange", this, { signal });

    // Show the eye-dropper.
    this.getElement("root").removeAttribute("hidden");

    // Prepare the canvas context on which we're drawing the magnified page portion.
    this.ctx = this.getElement("canvas").getCanvasContext();
    this.ctx.imageSmoothingEnabled = false;

    this.magnifiedArea = {
      width: MAGNIFIER_WIDTH,
      height: MAGNIFIER_HEIGHT,
      x: DEFAULT_START_POS_X,
      y: DEFAULT_START_POS_Y,
    };

    this.moveTo(DEFAULT_START_POS_X, DEFAULT_START_POS_Y);

    // Focus the content so the keyboard can be used.
    this.win.focus();

    // Make sure we receive mouse events when the debugger has paused execution
    // in the page.
    this.win.document.setSuppressedEventListener(this);

    return true;
  }

  /**
   * Hide the eye-dropper highlighter.
   */
  hide() {
    this.pageImage = null;

    if (this.#pageEventListenersAbortController) {
      this.#pageEventListenersAbortController.abort();
      this.#pageEventListenersAbortController = null;

      const rootElement = this.getElement("root");
      rootElement.setAttribute("hidden", "true");
      rootElement.removeAttribute("drawn");

      this.emit("hidden");

      this.win.document.setSuppressedEventListener(null);
    }
  }

  /**
   * Create an image bitmap from the page screenshot, draw the eyedropper and set the
   * "drawn" attribute on the "root" element once it's done.
   *
   * @params {String|null} screenshot: a dataURL representation of the page screenshot.
   *                       If null, we'll use `drawWindow` to get the the page screenshot
   *                       (⚠️ but it won't handle remote frames).
   */
  async prepareImageCapture(screenshot) {
    let imgData;
    if (screenshot) {
      // If a screenshot data URL was passed, we create an image tag with it which we
      // use to create an image bitmap.
      imgData = this.win.document.createElement("img");
      const onImgLoaded = new Promise(resolve =>
        imgData.addEventListener("load", resolve, { once: true })
      );
      imgData.src = screenshot;
      await onImgLoaded;
    } else {
      imgData = getWindowAsImageData(this.win);
    }

    // We need to transform imageData to something drawWindow will consume. An ImageBitmap
    // works well. We could have used an Image, but doing so results in errors if the page
    // defines CSP headers.
    const image = await this.win.createImageBitmap(imgData);

    this.pageImage = image;
    // We likely haven't drawn anything yet (no mousemove events yet), so start now.
    this.draw();

    // Set an attribute on the root element to be able to run tests after the first draw
    // was done.
    this.getElement("root").setAttribute("drawn", "true");
  }

  /**
   * Get the number of cells (blown-up pixels) per direction in the grid.
   */
  get cellsWide() {
    // Canvas will render whole "pixels" (cells) only, and an even number at that. Round
    // up to the nearest even number of pixels.
    let cellsWide = Math.ceil(
      this.magnifiedArea.width / this.eyeDropperZoomLevel
    );
    cellsWide += cellsWide % 2;

    return cellsWide;
  }

  /**
   * Get the size of each cell (blown-up pixel) in the grid.
   */
  get cellSize() {
    return this.magnifiedArea.width / this.cellsWide;
  }

  /**
   * Get index of cell in the center of the grid.
   */
  get centerCell() {
    return Math.floor(this.cellsWide / 2);
  }

  /**
   * Get color of center cell in the grid.
   */
  get centerColor() {
    const pos = this.centerCell * this.cellSize + this.cellSize / 2;
    const rgb = this.ctx.getImageData(pos, pos, 1, 1).data;
    return rgb;
  }

  draw() {
    // If the image of the page isn't ready yet, bail out, we'll draw later on mousemove.
    if (!this.pageImage) {
      return;
    }

    const { width, height, x, y } = this.magnifiedArea;

    const zoomedWidth = width / this.eyeDropperZoomLevel;
    const zoomedHeight = height / this.eyeDropperZoomLevel;

    const sx = x - zoomedWidth / 2;
    const sy = y - zoomedHeight / 2;
    const sw = zoomedWidth;
    const sh = zoomedHeight;

    this.ctx.drawImage(this.pageImage, sx, sy, sw, sh, 0, 0, width, height);

    // Draw the grid on top, but only at 3x or more, otherwise it's too busy.
    if (this.eyeDropperZoomLevel > 2) {
      this.drawGrid();
    }

    this.drawCrosshair();

    // Update the color preview and value.
    const rgb = this.centerColor;
    this.getElement("color-preview").setAttribute(
      "style",
      `background-color:${toColorString(rgb, "rgb")};`
    );
    this.getElement("color-value").setTextContent(
      toColorString(rgb, this.format)
    );
  }

  /**
   * Draw a grid on the canvas representing pixel boundaries.
   */
  drawGrid() {
    const { width, height } = this.magnifiedArea;

    this.ctx.lineWidth = 1;
    this.ctx.strokeStyle = "rgba(143, 143, 143, 0.2)";

    for (let i = 0; i < width; i += this.cellSize) {
      this.ctx.beginPath();
      this.ctx.moveTo(i - 0.5, 0);
      this.ctx.lineTo(i - 0.5, height);
      this.ctx.stroke();

      this.ctx.beginPath();
      this.ctx.moveTo(0, i - 0.5);
      this.ctx.lineTo(width, i - 0.5);
      this.ctx.stroke();
    }
  }

  /**
   * Draw a box on the canvas to highlight the center cell.
   */
  drawCrosshair() {
    const pos = this.centerCell * this.cellSize;

    this.ctx.lineWidth = 1;
    this.ctx.lineJoin = "miter";
    this.ctx.strokeStyle = "rgba(0, 0, 0, 1)";
    this.ctx.strokeRect(
      pos - 1.5,
      pos - 1.5,
      this.cellSize + 2,
      this.cellSize + 2
    );

    this.ctx.strokeStyle = "rgba(255, 255, 255, 1)";
    this.ctx.strokeRect(pos - 0.5, pos - 0.5, this.cellSize, this.cellSize);
  }

  handleEvent(e) {
    switch (e.type) {
      case "mousemove":
        // We might be getting an event from a child frame, so account for the offset.
        const [xOffset, yOffset] = getFrameOffsets(this.win, e.target);
        const x = xOffset + e.pageX - this.win.scrollX;
        const y = yOffset + e.pageY - this.win.scrollY;
        // Update the zoom area.
        this.magnifiedArea.x = x * this.pageZoom;
        this.magnifiedArea.y = y * this.pageZoom;
        // Redraw the portion of the screenshot that is now under the mouse.
        this.draw();
        // And move the eye-dropper's UI so it follows the mouse.
        this.moveTo(x, y);
        break;
      // Note: when events are suppressed we will only get mousedown/mouseup and
      // not any click events.
      case "click":
      case "mouseup":
        this.selectColor();
        break;
      case "keydown":
        this.handleKeyDown(e);
        break;
      case "DOMMouseScroll":
        // Prevent scrolling. That's because we only took a screenshot of the viewport, so
        // scrolling out of the viewport wouldn't draw the expected things. In the future
        // we can take the screenshot again on scroll, but for now it doesn't seem
        // important.
        e.preventDefault();
        break;
      case "FullZoomChange":
        this.hide();
        this.show();
        break;
    }
  }

  moveTo(x, y) {
    const root = this.getElement("root");
    root.setAttribute("style", `top:${y}px;left:${x}px;`);

    // Move the label container to the top if the magnifier is close to the bottom edge.
    if (y >= this.win.innerHeight - MAGNIFIER_HEIGHT) {
      root.setAttribute("top", "");
    } else {
      root.removeAttribute("top");
    }

    // Also offset the label container to the right or left if the magnifier is close to
    // the edge.
    root.removeAttribute("left");
    root.removeAttribute("right");
    if (x <= MAGNIFIER_WIDTH) {
      root.setAttribute("right", "");
    } else if (x >= this.win.innerWidth - MAGNIFIER_WIDTH) {
      root.setAttribute("left", "");
    }
  }

  /**
   * Select the current color that's being previewed. Depending on the current options,
   * selecting might mean copying to the clipboard and closing the
   */
  selectColor() {
    let onColorSelected = Promise.resolve();
    if (this.options.copyOnSelect) {
      onColorSelected = this.copyColor();
    }

    this.emit("selected", toColorString(this.centerColor, this.format));
    onColorSelected.then(() => this.hide(), console.error);
  }

  /**
   * Handler for the keydown event. Either select the color or move the panel in a
   * direction depending on the key pressed.
   */
  handleKeyDown(e) {
    // Bail out early if any unsupported modifier is used, so that we let
    // keyboard shortcuts through.
    if (e.metaKey || e.ctrlKey || e.altKey) {
      return;
    }

    if (e.keyCode === e.DOM_VK_RETURN) {
      this.selectColor();
      e.preventDefault();
      return;
    }

    if (e.keyCode === e.DOM_VK_ESCAPE) {
      this.emit("canceled");
      this.hide();
      e.preventDefault();
      return;
    }

    let offsetX = 0;
    let offsetY = 0;
    let modifier = 1;

    if (e.keyCode === e.DOM_VK_LEFT) {
      offsetX = -1;
    } else if (e.keyCode === e.DOM_VK_RIGHT) {
      offsetX = 1;
    } else if (e.keyCode === e.DOM_VK_UP) {
      offsetY = -1;
    } else if (e.keyCode === e.DOM_VK_DOWN) {
      offsetY = 1;
    }

    if (e.shiftKey) {
      modifier = 10;
    }

    offsetY *= modifier;
    offsetX *= modifier;

    if (offsetX !== 0 || offsetY !== 0) {
      this.magnifiedArea.x = cap(
        this.magnifiedArea.x + offsetX,
        0,
        this.win.innerWidth * this.pageZoom
      );
      this.magnifiedArea.y = cap(
        this.magnifiedArea.y + offsetY,
        0,
        this.win.innerHeight * this.pageZoom
      );

      this.draw();

      this.moveTo(
        this.magnifiedArea.x / this.pageZoom,
        this.magnifiedArea.y / this.pageZoom
      );

      e.preventDefault();
    }
  }

  /**
   * Copy the currently inspected color to the clipboard.
   * @return {Promise} Resolves when the copy has been done (after a delay that is used to
   * let users know that something was copied).
   */
  copyColor() {
    // Copy to the clipboard.
    const color = toColorString(this.centerColor, this.format);
    clipboardHelper.copyString(color);

    // Provide some feedback.
    this.getElement("color-value").setTextContent(
      "✓ " + l10n.GetStringFromName("colorValue.copied")
    );

    // Hide the tool after a delay.
    clearTimeout(this._copyTimeout);
    return new Promise(resolve => {
      this._copyTimeout = setTimeout(resolve, CLOSE_DELAY);
    });
  }
}

exports.EyeDropper = EyeDropper;

/**
 * Draw the visible portion of the window on a canvas and get the resulting ImageData.
 * @param {Window} win
 * @return {ImageData} The image data for the window.
 */
function getWindowAsImageData(win) {
  const canvas = win.document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "canvas"
  );
  const scale = getCurrentZoom(win);
  const width = win.innerWidth;
  const height = win.innerHeight;
  canvas.width = width * scale;
  canvas.height = height * scale;
  canvas.mozOpaque = true;

  const ctx = canvas.getContext("2d");

  ctx.scale(scale, scale);
  ctx.drawWindow(win, win.scrollX, win.scrollY, width, height, "#fff");

  return ctx.getImageData(0, 0, canvas.width, canvas.height);
}

/**
 * Get a formatted CSS color string from a color value.
 * @param {array} rgb Rgb values of a color to format.
 * @param {string} format Format of string. One of "hex", "rgb", "hsl", "name".
 * @return {string} Formatted color value, e.g. "#FFF" or "hsl(20, 10%, 10%)".
 */
function toColorString(rgb, format) {
  const [r, g, b] = rgb;

  switch (format) {
    case "hex":
      return hexString(rgb);
    case "rgb":
      return "rgb(" + r + ", " + g + ", " + b + ")";
    case "hsl":
      const [h, s, l] = rgbToHsl(rgb);
      return "hsl(" + h + ", " + s + "%, " + l + "%)";
    case "name":
      const str = rgbToColorName(r, g, b) || hexString(rgb);
      return str;
    default:
      return hexString(rgb);
  }
}

/**
 * Produce a hex-formatted color string from rgb values.
 * @param {array} rgb Rgb values of color to stringify.
 * @return {string} Hex formatted string for color, e.g. "#FFEE00".
 */
function hexString([r, g, b]) {
  const val = (1 << 24) + (r << 16) + (g << 8) + (b << 0);
  return "#" + val.toString(16).substr(-6);
}

function cap(value, min, max) {
  return Math.max(min, Math.min(value, max));
}
