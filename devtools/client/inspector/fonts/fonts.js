/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { gDevTools } = require("devtools/client/framework/devtools");
const { getColor } = require("devtools/client/shared/theme");
const { createFactory, createElement } = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const FontsApp = createFactory(require("./components/FontsApp"));

const { LocalizationHelper } = require("devtools/shared/l10n");
const INSPECTOR_L10N =
  new LocalizationHelper("devtools/client/locales/inspector.properties");

const { updateFonts } = require("./actions/fonts");
const { updatePreviewText } = require("./actions/font-options");

class FontInspector {
  constructor(inspector, window) {
    this.document = window.document;
    this.inspector = inspector;
    this.pageStyle = this.inspector.pageStyle;
    this.store = this.inspector.store;

    this.update = this.update.bind(this);

    this.onNewNode = this.onNewNode.bind(this);
    this.onPreviewFonts = this.onPreviewFonts.bind(this);
    this.onThemeChanged = this.onThemeChanged.bind(this);

    this.init();
  }

  init() {
    if (!this.inspector) {
      return;
    }

    let fontsApp = FontsApp({
      onPreviewFonts: this.onPreviewFonts,
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
    this.update(false, "");
  }

  /**
   * Given all fonts on the page, and given the fonts used in given node, return all fonts
   * not from the page not used in this node.
   *
   * @param  {Array} allFonts
   *         All fonts used on the entire page
   * @param  {Array} nodeFonts
   *         Fonts used only in one of the nodes
   * @return {Array}
   *         All fonts, except the ones used in the current node
   */
  excludeNodeFonts(allFonts, nodeFonts) {
    return allFonts.filter(font => {
      return !nodeFonts.some(nodeFont => nodeFont.name === font.name);
    });
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

  async getFontsForNode(node, options) {
    // In case we've been destroyed in the meantime
    if (!this.document) {
      return [];
    }

    let fonts = await this.pageStyle.getUsedFontFaces(node, options).catch(console.error);
    if (!fonts) {
      return [];
    }

    return fonts;
  }

  async getFontsNotInNode(nodeFonts, options) {
    // In case we've been destroyed in the meantime
    if (!this.document) {
      return [];
    }

    let allFonts = await this.pageStyle.getAllUsedFontFaces(options).catch(console.error);
    if (!allFonts) {
      allFonts = [];
    }

    return this.excludeNodeFonts(allFonts, nodeFonts);
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
   * Handler for the "theme-switched" event.
   */
  onThemeChanged(frame) {
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
    let otherFonts = [];

    let { fontOptions } = this.store.getState();
    let { previewText } = fontOptions;

    // Clear the list of fonts if the currently selected node is not connected or a text
    // or element node unless all fonts are supposed to be shown.
    let isElementOrTextNode = this.inspector.selection.isElementNode() ||
                              this.inspector.selection.isTextNode();
    if (!node ||
        !this.isPanelVisible() ||
        !this.inspector.selection.isConnected() ||
        !isElementOrTextNode) {
      this.store.dispatch(updateFonts(fonts, otherFonts));
      return;
    }

    let options = {
      includePreviews: true,
      previewText,
      previewFillStyle: getColor("body-color")
    };

    // Add the includeVariations argument into the option to get font variation data.
    if (this.pageStyle.supportsFontVariations) {
      options.includeVariations = true;
    }

    fonts = await this.getFontsForNode(node, options);
    otherFonts = await this.getFontsNotInNode(fonts, options);

    if (!fonts.length && !otherFonts.length) {
      // No fonts to display. Clear the previously shown fonts.
      if (this.store) {
        this.store.dispatch(updateFonts(fonts, otherFonts));
      }
      return;
    }

    for (let font of [...fonts, ...otherFonts]) {
      font.previewUrl = await font.preview.data.string();
    }

    // in case we've been destroyed in the meantime
    if (!this.document) {
      return;
    }

    this.store.dispatch(updateFonts(fonts, otherFonts));

    this.inspector.emit("fontinspector-updated");
  }
}

module.exports = FontInspector;
