/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getColor } = require("devtools/client/shared/theme");

const { createFactory, createElement } = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const { gDevTools } = require("devtools/client/framework/devtools");

const FontsApp = createFactory(require("./components/FontsApp"));

const { LocalizationHelper } = require("devtools/shared/l10n");
const INSPECTOR_L10N =
  new LocalizationHelper("devtools/client/locales/inspector.properties");

const { updateFonts } = require("./actions/fonts");
const { updatePreviewText, updateShowAllFonts } = require("./actions/font-options");

class FontInspector {
  constructor(inspector, window) {
    this.document = window.document;
    this.inspector = inspector;
    this.pageStyle = this.inspector.pageStyle;
    this.store = this.inspector.store;

    this.update = this.update.bind(this);

    this.onNewNode = this.onNewNode.bind(this);
    this.onPreviewFonts = this.onPreviewFonts.bind(this);
    this.onShowAllFont = this.onShowAllFont.bind(this);
    this.onThemeChanged = this.onThemeChanged.bind(this);

    this.init();
  }

  init() {
    if (!this.inspector) {
      return;
    }

    let fontsApp = FontsApp({
      onPreviewFonts: this.onPreviewFonts,
      onShowAllFont: this.onShowAllFont,
    });

    let provider = createElement(Provider, {
      id: "fontinspector",
      key: "fontinspector",
      store: this.store,
      title: INSPECTOR_L10N.getStr("inspector.sidebar.fontInspectorTitle"),
    }, fontsApp);

    // Expose the provider to let inspector.js use it in setupSidebar.
    this.provider = provider;

    this.inspector.selection.on("new-node-front", this.onNewNode);
    this.inspector.sidebar.on("fontinspector-selected", this.onNewNode);

    // Listen for theme changes as the color of the previews depend on the theme
    gDevTools.on("theme-switched", this.onThemeChanged);

    this.store.dispatch(updatePreviewText(""));
    this.store.dispatch(updateShowAllFonts(false));
    this.update(false, "");
  }

  /**
   * Destruction function called when the inspector is destroyed. Removes event listeners
   * and cleans up references.
   */
  destroy() {
    this.inspector.selection.off("new-node-front", this.onNewNode);
    this.inspector.sidebar.off("fontinspector-selected", this.onNewNode);
    gDevTools.off("theme-switched", this.onThemeChanged);

    this.document = null;
    this.inspector = null;
    this.pageStyle = null;
    this.store = null;
  }

  /**
   * Returns true if the font inspector panel is visible, and false otherwise.
   */
  isPanelVisible() {
    return this.inspector.sidebar &&
           this.inspector.sidebar.getCurrentTabID() === "fontinspector";
  }

  /**
   * Selection 'new-node' event handler.
   */
  onNewNode() {
    if (this.isPanelVisible()) {
      this.store.dispatch(updateShowAllFonts(false));
      this.update();
    }
  }

  /**
   * Handler for change in preview input.
   */
  onPreviewFonts(value) {
    this.store.dispatch(updatePreviewText(value));
    this.update();
  }

  /**
   * Handler for click on show all fonts button.
   */
  onShowAllFont() {
    this.store.dispatch(updateShowAllFonts(true));
    this.update();
  }

  /**
   * Handler for the "theme-switched" event.
   */
  onThemeChanged(event, frame) {
    if (frame === this.document.defaultView) {
      this.update();
    }
  }

  async update() {
    // Stop refreshing if the inspector or store is already destroyed.
    if (!this.inspector || !this.store) {
      return;
    }

    let node = this.inspector.selection.nodeFront;
    let fonts = [];
    let { fontOptions } = this.store.getState();
    let { showAllFonts, previewText } = fontOptions;

    // Clear the list of fonts if the currently selected node is not connected or an
    // element node unless all fonts are supposed to be shown.
    if (!showAllFonts &&
        (!node ||
         !this.isPanelVisible() ||
         !this.inspector.selection.isConnected() ||
         !this.inspector.selection.isElementNode())) {
      this.store.dispatch(updateFonts(fonts));
      return;
    }

    let options = {
      includePreviews: true,
      previewText,
      previewFillStyle: getColor("body-color")
    };

    if (showAllFonts) {
      fonts = await this.pageStyle.getAllUsedFontFaces(options)
                      .catch(console.error);
    } else {
      fonts = await this.pageStyle.getUsedFontFaces(node, options)
                      .catch(console.error);
    }

    if (!fonts || !fonts.length) {
      // No fonts to display. Clear the previously shown fonts.
      if (this.store) {
        this.store.dispatch(updateFonts(fonts));
      }
      return;
    }

    for (let font of fonts) {
      font.previewUrl = await font.preview.data.string();
    }

    // in case we've been destroyed in the meantime
    if (!this.document) {
      return;
    }

    this.store.dispatch(updateFonts(fonts));

    this.inspector.emit("fontinspector-updated");
  }
}

module.exports = FontInspector;
