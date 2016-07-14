/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci} = require("chrome");
const {rgbToHsl, rgbToColorName} =
      require("devtools/client/shared/css-color").colorUtils;
const Telemetry = require("devtools/client/shared/telemetry");
const EventEmitter = require("devtools/shared/event-emitter");
const promise = require("promise");
const defer = require("devtools/shared/defer");
const Services = require("Services");

loader.lazyGetter(this, "clipboardHelper", function () {
  return Cc["@mozilla.org/widget/clipboardhelper;1"]
    .getService(Ci.nsIClipboardHelper);
});

loader.lazyGetter(this, "ssService", function () {
  return Cc["@mozilla.org/content/style-sheet-service;1"]
    .getService(Ci.nsIStyleSheetService);
});

loader.lazyGetter(this, "ioService", function () {
  return Cc["@mozilla.org/network/io-service;1"]
    .getService(Ci.nsIIOService);
});

loader.lazyGetter(this, "DOMUtils", function () {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
});

loader.lazyGetter(this, "l10n", () => Services.strings
  .createBundle("chrome://devtools/locale/eyedropper.properties"));

const EYEDROPPER_URL = "chrome://devtools/content/eyedropper/eyedropper.xul";
const CROSSHAIRS_URL = "chrome://devtools/content/eyedropper/crosshairs.css";
const NOCURSOR_URL = "chrome://devtools/content/eyedropper/nocursor.css";

const ZOOM_PREF = "devtools.eyedropper.zoom";
const FORMAT_PREF = "devtools.defaultColorUnit";

const CANVAS_WIDTH = 96;
const CANVAS_OFFSET = 3; // equals the border width of the canvas.
const CLOSE_DELAY = 750;

const HEX_BOX_WIDTH = CANVAS_WIDTH + CANVAS_OFFSET * 2;
const HSL_BOX_WIDTH = 158;

/**
 * Manage instances of eyedroppers for windows. Registering here isn't
 * necessary for creating an eyedropper, but can be used for testing.
 */
var EyedropperManager = {
  _instances: new WeakMap(),

  getInstance: function (chromeWindow) {
    return this._instances.get(chromeWindow);
  },

  createInstance: function (chromeWindow, options) {
    let dropper = this.getInstance(chromeWindow);
    if (dropper) {
      return dropper;
    }

    dropper = new Eyedropper(chromeWindow, options);
    this._instances.set(chromeWindow, dropper);

    dropper.on("destroy", () => {
      this.deleteInstance(chromeWindow);
    });

    return dropper;
  },

  deleteInstance: function (chromeWindow) {
    this._instances.delete(chromeWindow);
  }
};

exports.EyedropperManager = EyedropperManager;

/**
 * Eyedropper widget. Once opened, shows zoomed area above current pixel and
 * displays the color value of the center pixel. Clicking on the window will
 * close the widget and fire a 'select' event. If 'copyOnSelect' is true, the color
 * will also be copied to the clipboard.
 *
 * let eyedropper = new Eyedropper(window);
 * eyedropper.open();
 *
 * eyedropper.once("select", (ev, color) => {
 *   console.log(color);  // "rgb(20, 50, 230)"
 * })
 *
 * @param {DOMWindow} chromeWindow
 *        window to inspect
 * @param {object} opts
 *        optional options object, with 'copyOnSelect', 'context'
 */
function Eyedropper(chromeWindow, opts = { copyOnSelect: true, context: "other" }) {
  this.copyOnSelect = opts.copyOnSelect;

  this._onFirstMouseMove = this._onFirstMouseMove.bind(this);
  this._onMouseMove = this._onMouseMove.bind(this);
  this._onMouseDown = this._onMouseDown.bind(this);
  this._onKeyDown = this._onKeyDown.bind(this);
  this._onFrameLoaded = this._onFrameLoaded.bind(this);

  this._chromeWindow = chromeWindow;
  this._chromeDocument = chromeWindow.document;

  this._OS = Services.appinfo.OS;

  this._dragging = true;
  this.loaded = false;

  this._mouseMoveCounter = 0;

  this.format = Services.prefs.getCharPref(FORMAT_PREF); // color value format
  this.zoom = Services.prefs.getIntPref(ZOOM_PREF);      // zoom level - integer

  this._zoomArea = {
    x: 0,          // the left coordinate of the center of the inspected region
    y: 0,          // the top coordinate of the center of the inspected region
    width: CANVAS_WIDTH,      // width of canvas to draw zoomed area onto
    height: CANVAS_WIDTH      // height of canvas
  };

  if (this._contentTab) {
    let mm = this._contentTab.linkedBrowser.messageManager;
    mm.loadFrameScript("resource://devtools/client/eyedropper/eyedropper-child.js", true);
  }

  // record if this was opened via the picker or standalone
  var telemetry = new Telemetry();
  if (opts.context == "command") {
    telemetry.toolOpened("eyedropper");
  }
  else if (opts.context == "menu") {
    telemetry.toolOpened("menueyedropper");
  }
  else if (opts.context == "picker") {
    telemetry.toolOpened("pickereyedropper");
  }

  EventEmitter.decorate(this);
}

exports.Eyedropper = Eyedropper;

Eyedropper.prototype = {
  /**
   * Get the number of cells (blown-up pixels) per direction in the grid.
   */
  get cellsWide() {
    // Canvas will render whole "pixels" (cells) only, and an even
    // number at that. Round up to the nearest even number of pixels.
    let cellsWide = Math.ceil(this._zoomArea.width / this.zoom);
    cellsWide += cellsWide % 2;

    return cellsWide;
  },

  /**
   * Get the size of each cell (blown-up pixel) in the grid.
   */
  get cellSize() {
    return this._zoomArea.width / this.cellsWide;
  },

  /**
   * Get index of cell in the center of the grid.
   */
  get centerCell() {
    return Math.floor(this.cellsWide / 2);
  },

  /**
   * Get color of center cell in the grid.
   */
  get centerColor() {
    let x, y;
    x = y = (this.centerCell * this.cellSize) + (this.cellSize / 2);
    let rgb = this._ctx.getImageData(x, y, 1, 1).data;
    return rgb;
  },

  get _contentTab() {
    return this._chromeWindow.gBrowser && this._chromeWindow.gBrowser.selectedTab;
  },

  /**
   * Fetch a screenshot of the content.
   *
   * @return {promise}
   *         Promise that resolves with the screenshot as a dataURL
   */
  getContentScreenshot: function () {
    if (!this._contentTab) {
        return promise.resolve(null);
    }

    let deferred = defer();

    let mm = this._contentTab.linkedBrowser.messageManager;
    function onScreenshot(message) {
      mm.removeMessageListener("Eyedropper:Screenshot", onScreenshot);
      deferred.resolve(message.data);
    }
    mm.addMessageListener("Eyedropper:Screenshot", onScreenshot);
    mm.sendAsyncMessage("Eyedropper:RequestContentScreenshot");

    return deferred.promise;
  },

  /**
   * Start the eyedropper. Add listeners for a mouse move in the window to
   * show the eyedropper.
   */
  open: function () {
    if (this.isOpen) {
      // the eyedropper is aready open, don't create another panel.
      return promise.resolve();
    }

    this.isOpen = true;

    this._showCrosshairs();

    // Get screenshot of content so we can inspect colors
    return this.getContentScreenshot().then((dataURL) => {
      // The data url may be null, e.g. if there is no content tab
      if (dataURL) {
        this._contentImage = new this._chromeWindow.Image();
        this._contentImage.src = dataURL;

        // Wait for screenshot to load
        let imageLoaded = promise.defer();
        this._contentImage.onload = imageLoaded.resolve
        return imageLoaded.promise;
      }
    }).then(() => {
      // Then start showing the eyedropper UI
      this._chromeDocument.addEventListener("mousemove", this._onFirstMouseMove);
      this.isStarted = true;
      this.emit("started");
    });
  },

  /**
   * Called on the first mouse move over the window. Opens the eyedropper
   * panel where the mouse is.
   */
  _onFirstMouseMove: function (event) {
    this._chromeDocument.removeEventListener("mousemove", this._onFirstMouseMove);

    this._panel = this._buildPanel();

    let popupSet = this._chromeDocument.querySelector("#mainPopupSet");
    popupSet.appendChild(this._panel);

    let { panelX, panelY } = this._getPanelCoordinates(event);
    this._panel.openPopupAtScreen(panelX, panelY);

    this._setCoordinates(event);

    this._addListeners();

    // hide cursor as we'll be showing the panel over the mouse instead.
    this._hideCrosshairs();
    this._hideCursor();
  },

  /**
   * Whether the coordinates are over the content or chrome.
   *
   * @param {number} clientX
   *        x-coordinate of mouse relative to browser window.
   * @param {number} clientY
   *        y-coordinate of mouse relative to browser window.
   */
  _isInContent: function (clientX, clientY) {
    let box = this._contentTab && this._contentTab.linkedBrowser.getBoundingClientRect();
    if (box &&
        clientX > box.left &&
        clientX < box.right &&
        clientY > box.top &&
        clientY < box.bottom) {
      return true;
    }
    return false;
  },

  /**
   * Set the current coordinates to inspect from where a mousemove originated.
   *
   * @param {MouseEvent} event
   *        Event for the mouse move.
   */
  _setCoordinates: function (event) {
    let inContent = this._isInContent(event.clientX, event.clientY);
    let win = this._chromeWindow;

    // offset of mouse from browser window
    let x = event.clientX;
    let y = event.clientY;

    if (inContent) {
      // calculate the offset of the mouse from the content window
      let box = this._contentTab.linkedBrowser.getBoundingClientRect();
      x = x - box.left;
      y = y - box.top;

      this._zoomArea.contentWidth = box.width;
      this._zoomArea.contentHeight = box.height;
    }
    this._zoomArea.inContent = inContent;

    // don't let it inspect outside the browser window
    x = Math.max(0, Math.min(x, win.outerWidth - 1));
    y = Math.max(0, Math.min(y, win.outerHeight - 1));

    this._zoomArea.x = x;
    this._zoomArea.y = y;
  },

  /**
   * Build and add a new eyedropper panel to the window.
   *
   * @return {Panel}
   *         The XUL panel holding the eyedropper UI.
   */
  _buildPanel: function () {
    let panel = this._chromeDocument.createElement("panel");
    panel.setAttribute("noautofocus", true);
    panel.setAttribute("noautohide", true);
    panel.setAttribute("level", "floating");
    panel.setAttribute("class", "devtools-eyedropper-panel");

    let iframe = this._iframe = this._chromeDocument.createElement("iframe");
    iframe.addEventListener("load", this._onFrameLoaded, true);
    iframe.setAttribute("flex", "1");
    iframe.setAttribute("transparent", "transparent");
    iframe.setAttribute("allowTransparency", true);
    iframe.setAttribute("class", "devtools-eyedropper-iframe");
    iframe.setAttribute("src", EYEDROPPER_URL);
    iframe.setAttribute("width", CANVAS_WIDTH);
    iframe.setAttribute("height", CANVAS_WIDTH);

    panel.appendChild(iframe);

    return panel;
  },

  /**
   * Event handler for the panel's iframe's load event. Emits
   * a "load" event from this eyedropper object.
   */
  _onFrameLoaded: function () {
    this._iframe.removeEventListener("load", this._onFrameLoaded, true);

    this._iframeDocument = this._iframe.contentDocument;
    this._colorPreview = this._iframeDocument.querySelector("#color-preview");
    this._colorValue = this._iframeDocument.querySelector("#color-value");

    // value box will be too long for hex values and too short for hsl
    let valueBox = this._iframeDocument.querySelector("#color-value-box");
    if (this.format == "hex") {
      valueBox.style.width = HEX_BOX_WIDTH + "px";
    }
    else if (this.format == "hsl") {
      valueBox.style.width = HSL_BOX_WIDTH + "px";
    }

    this._canvas = this._iframeDocument.querySelector("#canvas");
    this._ctx = this._canvas.getContext("2d");

    // so we preserve the clear pixel boundaries
    this._ctx.mozImageSmoothingEnabled = false;

    this._drawWindow();

    this._addPanelListeners();
    this._iframe.focus();

    this.loaded = true;
    this.emit("load");
  },

  /**
   * Add key listeners to the panel.
   */
  _addPanelListeners: function () {
    this._iframeDocument.addEventListener("keydown", this._onKeyDown);

    let closeCmd = this._iframeDocument.getElementById("eyedropper-cmd-close");
    closeCmd.addEventListener("command", this.destroy.bind(this), true);

    let copyCmd = this._iframeDocument.getElementById("eyedropper-cmd-copy");
    copyCmd.addEventListener("command", this.selectColor.bind(this), true);
  },

  /**
   * Remove listeners from the panel.
   */
  _removePanelListeners: function () {
    this._iframeDocument.removeEventListener("keydown", this._onKeyDown);
  },

  /**
   * Add mouse event listeners to the document we're inspecting.
   */
  _addListeners: function () {
    this._chromeDocument.addEventListener("mousemove", this._onMouseMove);
    this._chromeDocument.addEventListener("mousedown", this._onMouseDown);
  },

  /**
   * Remove mouse event listeners from the document we're inspecting.
   */
  _removeListeners: function () {
    this._chromeDocument.removeEventListener("mousemove", this._onFirstMouseMove);
    this._chromeDocument.removeEventListener("mousemove", this._onMouseMove);
    this._chromeDocument.removeEventListener("mousedown", this._onMouseDown);
  },

  /**
   * Hide the cursor.
   */
  _hideCursor: function () {
    registerStyleSheet(NOCURSOR_URL);
  },

  /**
   * Reset the cursor back to default.
   */
  _resetCursor: function () {
    unregisterStyleSheet(NOCURSOR_URL);
  },

  /**
   * Show a crosshairs as the mouse cursor
   */
  _showCrosshairs: function () {
    registerStyleSheet(CROSSHAIRS_URL);
  },

  /**
   * Reset cursor.
   */
  _hideCrosshairs: function () {
    unregisterStyleSheet(CROSSHAIRS_URL);
  },

  /**
   * Event handler for a mouse move over the page we're inspecting.
   * Preview the area under the cursor, and move panel to be under the cursor.
   *
   * @param  {DOMEvent} event
   *         MouseEvent for the mouse moving
   */
  _onMouseMove: function (event) {
    if (!this._dragging || !this._panel || !this._canvas) {
      return;
    }

    if (this._OS == "Linux" && ++this._mouseMoveCounter % 2 == 0) {
      // skip every other mousemove to preserve performance.
      return;
    }

    this._setCoordinates(event);
    this._drawWindow();

    let { panelX, panelY } = this._getPanelCoordinates(event);
    this._movePanel(panelX, panelY);
  },

  /**
   * Get coordinates of where the eyedropper panel should go based on
   * the current coordinates of the mouse cursor.
   *
   * @param {MouseEvent} event
   *        object with properties 'screenX' and 'screenY'
   *
   * @return {object}
  *          object with properties 'panelX', 'panelY'
   */
  _getPanelCoordinates: function ({screenX, screenY}) {
    let win = this._chromeWindow;
    let offset = CANVAS_WIDTH / 2 + CANVAS_OFFSET;

    let panelX = screenX - offset;
    let windowX = win.screenX + (win.outerWidth - win.innerWidth);
    let maxX = win.screenX + win.outerWidth - offset - 1;

    let panelY = screenY - offset;
    let windowY = win.screenY + (win.outerHeight - win.innerHeight);
    let maxY = win.screenY + win.outerHeight - offset - 1;

    // don't let the panel move outside the browser window
    panelX = Math.max(windowX - offset, Math.min(panelX, maxX));
    panelY = Math.max(windowY - offset, Math.min(panelY, maxY));

    return { panelX: panelX, panelY: panelY };
  },

  /**
   * Move the eyedropper panel to the given coordinates.
   *
   * @param  {number} screenX
   *         left coordinate on the screen
   * @param  {number} screenY
   *         top coordinate
   */
  _movePanel: function (screenX, screenY) {
    this._panelX = screenX;
    this._panelY = screenY;

    this._panel.moveTo(screenX, screenY);
  },

  /**
   * Handler for the mouse down event on the inspected page. This means a
   * click, so we'll select the color that's currently hovered.
   *
   * @param  {Event} event
   *         DOM MouseEvent object
   */
  _onMouseDown: function (event) {
    event.preventDefault();
    event.stopPropagation();

    this.selectColor();
  },

  /**
   * Select the current color that's being previewed. Fire a
   * "select" event with the color as an rgb string.
   */
  selectColor: function () {
    if (this._isSelecting) {
      return;
    }
    this._isSelecting = true;
    this._dragging = false;

    this.emit("select", this._colorValue.value);

    if (this.copyOnSelect) {
      this.copyColor(this.destroy.bind(this));
    }
    else {
      this.destroy();
    }
  },

  /**
   * Copy the currently inspected color to the clipboard.
   *
   * @param  {Function} callback
   *         Callback to be called when the color is in the clipboard.
   */
  copyColor: function (callback) {
    clearTimeout(this._copyTimeout);

    let color = this._colorValue.value;
    clipboardHelper.copyString(color);

    this._colorValue.classList.add("highlight");
    this._colorValue.value = "✓ " + l10n.GetStringFromName("colorValue.copied");

    this._copyTimeout = setTimeout(() => {
      this._colorValue.classList.remove("highlight");
      this._colorValue.value = color;

      if (callback) {
        callback();
      }
    }, CLOSE_DELAY);
  },

  /**
   * Handler for the keydown event on the panel. Either copy the color
   * or move the panel in a direction depending on the key pressed.
   *
   * @param  {Event} event
   *         DOM KeyboardEvent object
   */
  _onKeyDown: function (event) {
    if (event.metaKey && event.keyCode === event.DOM_VK_C) {
      this.copyColor();
      return;
    }

    let offsetX = 0;
    let offsetY = 0;
    let modifier = 1;

    if (event.keyCode === event.DOM_VK_LEFT) {
      offsetX = -1;
    }
    if (event.keyCode === event.DOM_VK_RIGHT) {
      offsetX = 1;
    }
    if (event.keyCode === event.DOM_VK_UP) {
      offsetY = -1;
    }
    if (event.keyCode === event.DOM_VK_DOWN) {
      offsetY = 1;
    }
    if (event.shiftKey) {
      modifier = 10;
    }

    offsetY *= modifier;
    offsetX *= modifier;

    if (offsetX !== 0 || offsetY !== 0) {
      this._zoomArea.x += offsetX;
      this._zoomArea.y += offsetY;

      this._drawWindow();

      this._movePanel(this._panelX + offsetX, this._panelY + offsetY);

      event.preventDefault();
    }
  },

  /**
   * Draw the inspected area onto the canvas using the zoom level.
   */
  _drawWindow: function () {
    let { width, height, x, y, inContent,
          contentWidth, contentHeight } = this._zoomArea;

    let zoomedWidth = width / this.zoom;
    let zoomedHeight = height / this.zoom;

    let leftX = x - (zoomedWidth / 2);
    let topY = y - (zoomedHeight / 2);

    // draw the portion of the window we're inspecting
    if (inContent) {
      // draw from content source image "s" to destination rect "d"
      let sx = leftX;
      let sy = topY;
      let sw = zoomedWidth;
      let sh = zoomedHeight;
      let dx = 0;
      let dy = 0;

      // we're at the content edge, so we have to crop the drawing
      if (leftX < 0) {
        sx = 0;
        sw = zoomedWidth + leftX;
        dx = -leftX;
      }
      else if (leftX + zoomedWidth > contentWidth) {
        sw = contentWidth - leftX;
      }
      if (topY < 0) {
        sy = 0;
        sh = zoomedHeight + topY;
        dy = -topY;
      }
      else if (topY + zoomedHeight > contentHeight) {
        sh = contentHeight - topY;
      }
      let dw = sw;
      let dh = sh;

      // we don't want artifacts when we're inspecting the edges of content
      if (leftX < 0 || topY < 0 ||
          leftX + zoomedWidth > contentWidth ||
          topY + zoomedHeight > contentHeight) {
        this._ctx.fillStyle = "white";
        this._ctx.fillRect(0, 0, width, height);
      }

      // draw from the screenshot to the eyedropper canvas
      this._ctx.drawImage(this._contentImage, sx, sy, sw,
                          sh, dx, dy, dw, dh);
    }
    else {
      // the mouse is over the chrome, so draw that instead of the content
      this._ctx.drawWindow(this._chromeWindow, leftX, topY, zoomedWidth,
                           zoomedHeight, "white");
    }

    // now scale it
    this._ctx.drawImage(this._canvas, 0, 0, zoomedWidth, zoomedHeight,
                                      0, 0, width, height);

    let rgb = this.centerColor;
    this._colorPreview.style.backgroundColor = toColorString(rgb, "rgb");
    this._colorValue.value = toColorString(rgb, this.format);

    if (this.zoom > 2) {
      // grid at 2x is too busy
      this._drawGrid();
    }
    this._drawCrosshair();
  },

  /**
   * Draw a grid on the canvas representing pixel boundaries.
   */
  _drawGrid: function () {
    let { width, height } = this._zoomArea;

    this._ctx.lineWidth = 1;
    this._ctx.strokeStyle = "rgba(143, 143, 143, 0.2)";

    for (let i = 0; i < width; i += this.cellSize) {
      this._ctx.beginPath();
      this._ctx.moveTo(i - .5, 0);
      this._ctx.lineTo(i - .5, height);
      this._ctx.stroke();

      this._ctx.beginPath();
      this._ctx.moveTo(0, i - .5);
      this._ctx.lineTo(width, i - .5);
      this._ctx.stroke();
    }
  },

  /**
   * Draw a box on the canvas to highlight the center cell.
   */
  _drawCrosshair: function () {
    let x, y;
    x = y = this.centerCell * this.cellSize;

    this._ctx.lineWidth = 1;
    this._ctx.lineJoin = "miter";
    this._ctx.strokeStyle = "rgba(0, 0, 0, 1)";
    this._ctx.strokeRect(x - 1.5, y - 1.5, this.cellSize + 2, this.cellSize + 2);

    this._ctx.strokeStyle = "rgba(255, 255, 255, 1)";
    this._ctx.strokeRect(x - 0.5, y - 0.5, this.cellSize, this.cellSize);
  },

  /**
   * Destroy the eyedropper and clean up. Emits a "destroy" event.
   */
  destroy: function () {
    this._resetCursor();
    this._hideCrosshairs();

    if (this._panel) {
      this._panel.hidePopup();
      this._panel.remove();
      this._panel = null;
    }
    this._removePanelListeners();
    this._removeListeners();

    this.isStarted = false;
    this.isOpen = false;
    this._isSelecting = false;

    this.emit("destroy");
  }
};

/**
 * Add a user style sheet that applies to all documents.
 */
function registerStyleSheet(url) {
  var uri = ioService.newURI(url, null, null);
  if (!ssService.sheetRegistered(uri, ssService.AGENT_SHEET)) {
    ssService.loadAndRegisterSheet(uri, ssService.AGENT_SHEET);
  }
}

/**
 * Remove a user style sheet.
 */
function unregisterStyleSheet(url) {
  var uri = ioService.newURI(url, null, null);
  if (ssService.sheetRegistered(uri, ssService.AGENT_SHEET)) {
    ssService.unregisterSheet(uri, ssService.AGENT_SHEET);
  }
}

/**
 * Get a formatted CSS color string from a color value.
 *
 * @param {array} rgb
 *        Rgb values of a color to format
 * @param {string} format
 *        Format of string. One of "hex", "rgb", "hsl", "name"
 *
 * @return {string}
 *        Formatted color value, e.g. "#FFF" or "hsl(20, 10%, 10%)"
 */
function toColorString(rgb, format) {
  let [r, g, b] = rgb;

  switch (format) {
    case "hex":
      return hexString(rgb);
    case "rgb":
      return "rgb(" + r + ", " + g + ", " + b + ")";
    case "hsl":
      let [h, s, l] = rgbToHsl(rgb);
      return "hsl(" + h + ", " + s + "%, " + l + "%)";
    case "name":
      let str;
      try {
        str = rgbToColorName(r, g, b);
      } catch (e) {
        str = hexString(rgb);
      }
      return str;
    default:
      return hexString(rgb);
  }
}

/**
 * Produce a hex-formatted color string from rgb values.
 *
 * @param {array} rgb
 *        Rgb values of color to stringify
 *
 * @return {string}
 *        Hex formatted string for color, e.g. "#FFEE00"
 */
function hexString([r, g, b]) {
  let val = (1 << 24) + (r << 16) + (g << 8) + (b << 0);
  return "#" + val.toString(16).substr(-6).toUpperCase();
}
