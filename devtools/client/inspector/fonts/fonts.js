/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {gDevTools} = require("devtools/client/framework/devtools");
const Services = require("Services");

const DEFAULT_PREVIEW_TEXT = "Abc";
const PREVIEW_UPDATE_DELAY = 150;

const {Task} = require("devtools/shared/task");

function FontInspector(inspector, window) {
  this.inspector = inspector;
  this.pageStyle = this.inspector.pageStyle;
  this.chromeDoc = window.document;
  this.init();
}

FontInspector.prototype = {
  init: function () {
    this.update = this.update.bind(this);
    this.onNewNode = this.onNewNode.bind(this);
    this.onThemeChanged = this.onThemeChanged.bind(this);
    this.inspector.selection.on("new-node-front", this.onNewNode);
    this.inspector.sidebar.on("fontinspector-selected", this.onNewNode);
    this.showAll = this.showAll.bind(this);
    this.showAllLink = this.chromeDoc.getElementById("font-showall");
    this.showAllLink.addEventListener("click", this.showAll);
    this.previewTextChanged = this.previewTextChanged.bind(this);
    this.previewInput = this.chromeDoc.getElementById("font-preview-text-input");
    this.previewInput.addEventListener("input", this.previewTextChanged);
    this.previewInput.addEventListener("contextmenu",
      this.inspector.onTextBoxContextMenu);

    // Listen for theme changes as the color of the previews depend on the theme
    gDevTools.on("theme-switched", this.onThemeChanged);

    this.update();
  },

  /**
   * Is the fontinspector visible in the sidebar?
   */
  isActive: function () {
    return this.inspector.sidebar &&
           this.inspector.sidebar.getCurrentTabID() == "fontinspector";
  },

  /**
   * Remove listeners.
   */
  destroy: function () {
    this.chromeDoc = null;
    this.inspector.sidebar.off("fontinspector-selected", this.onNewNode);
    this.inspector.selection.off("new-node-front", this.onNewNode);
    this.showAllLink.removeEventListener("click", this.showAll);
    this.previewInput.removeEventListener("input", this.previewTextChanged);
    this.previewInput.removeEventListener("contextmenu",
      this.inspector.onTextBoxContextMenu);

    gDevTools.off("theme-switched", this.onThemeChanged);

    if (this._previewUpdateTimeout) {
      clearTimeout(this._previewUpdateTimeout);
    }
  },

  /**
   * Selection 'new-node' event handler.
   */
  onNewNode: function () {
    if (this.isActive() &&
        this.inspector.selection.isConnected() &&
        this.inspector.selection.isElementNode()) {
      this.undim();
      this.update();
    } else {
      this.dim();
    }
  },

  /**
   * The text to use for previews. Returns either the value user has typed to
   * the preview input or DEFAULT_PREVIEW_TEXT if the input is empty or contains
   * only whitespace.
   */
  getPreviewText: function () {
    let inputText = this.previewInput.value.trim();
    if (inputText === "") {
      return DEFAULT_PREVIEW_TEXT;
    }

    return inputText;
  },

  /**
   * Preview input 'input' event handler.
   */
  previewTextChanged: function () {
    if (this._previewUpdateTimeout) {
      clearTimeout(this._previewUpdateTimeout);
    }

    this._previewUpdateTimeout = setTimeout(() => {
      this.update(this._lastUpdateShowedAllFonts);
    }, PREVIEW_UPDATE_DELAY);
  },

  /**
   * Callback for the theme-switched event.
   */
  onThemeChanged: function (event, frame) {
    if (frame === this.chromeDoc.defaultView) {
      this.update(this._lastUpdateShowedAllFonts);
    }
  },

  /**
   * Hide the font list. No node are selected.
   */
  dim: function () {
    let panel = this.chromeDoc.getElementById("sidebar-panel-fontinspector");
    panel.classList.add("dim");
    this.clear();
  },

  /**
   * Show the font list. A node is selected.
   */
  undim: function () {
    let panel = this.chromeDoc.getElementById("sidebar-panel-fontinspector");
    panel.classList.remove("dim");
  },

  /**
   * Clears the font list.
   */
  clear: function () {
    this.chromeDoc.querySelector("#all-fonts").innerHTML = "";
  },

 /**
  * Retrieve all the font info for the selected node and display it.
  */
  update: Task.async(function* (showAllFonts) {
    let node = this.inspector.selection.nodeFront;
    let panel = this.chromeDoc.getElementById("sidebar-panel-fontinspector");

    if (!node ||
        !this.isActive() ||
        !this.inspector.selection.isConnected() ||
        !this.inspector.selection.isElementNode() ||
        panel.classList.contains("dim")) {
      return;
    }

    this._lastUpdateShowedAllFonts = showAllFonts;

    // Assume light theme colors as the default (see also bug 1118179).
    let fillStyle = (Services.prefs.getCharPref("devtools.theme") == "dark") ?
        "white" : "black";

    let options = {
      includePreviews: true,
      previewText: this.getPreviewText(),
      previewFillStyle: fillStyle
    };

    let fonts = [];
    if (showAllFonts) {
      fonts = yield this.pageStyle.getAllUsedFontFaces(options)
                      .then(null, console.error);
    } else {
      fonts = yield this.pageStyle.getUsedFontFaces(node, options)
                      .then(null, console.error);
    }

    if (!fonts || !fonts.length) {
      // No fonts to display. Clear the previously shown fonts.
      this.clear();
      return;
    }

    for (let font of fonts) {
      font.previewUrl = yield font.preview.data.string();
    }

    // in case we've been destroyed in the meantime
    if (!this.chromeDoc) {
      return;
    }

    // Make room for the new fonts.
    this.clear();

    for (let font of fonts) {
      this.render(font);
    }

    this.inspector.emit("fontinspector-updated");
  }),

  /**
   * Display the information of one font.
   */
  render: function (font) {
    let s = this.chromeDoc.querySelector("#font-template > section");
    s = s.cloneNode(true);

    s.querySelector(".font-name").textContent = font.name;
    s.querySelector(".font-css-name").textContent = font.CSSFamilyName;

    if (font.URI) {
      s.classList.add("is-remote");
    } else {
      s.classList.add("is-local");
    }

    let formatElem = s.querySelector(".font-format");
    if (font.format) {
      formatElem.textContent = font.format;
    } else {
      formatElem.hidden = true;
    }

    s.querySelector(".font-url").value = font.URI;

    if (font.rule) {
      // This is the @font-face{…} code.
      let cssText = font.ruleText;

      s.classList.add("has-code");
      s.querySelector(".font-css-code").textContent = cssText;
    }
    let preview = s.querySelector(".font-preview");
    preview.src = font.previewUrl;

    this.chromeDoc.querySelector("#all-fonts").appendChild(s);
  },

  /**
   * Show all fonts for the document (including iframes)
   */
  showAll: function () {
    this.update(true);
  },
};

exports.FontInspector = FontInspector;
