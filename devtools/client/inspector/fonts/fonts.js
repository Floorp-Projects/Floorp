/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { Task } = require("devtools/shared/task");
const { getColor } = require("devtools/client/shared/theme");

const { createFactory, createElement } = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const { gDevTools } = require("devtools/client/framework/devtools");

const App = createFactory(require("./components/App"));

const { LocalizationHelper } = require("devtools/shared/l10n");
const INSPECTOR_L10N =
  new LocalizationHelper("devtools/client/locales/inspector.properties");

const { updateFonts } = require("./actions/fonts");
const { updatePreviewText, updateShowAllFonts } = require("./actions/font-options");

function FontInspector(inspector, window) {
  this.document = window.document;
  this.inspector = inspector;
  this.pageStyle = this.inspector.pageStyle;
  this.store = inspector.store;

  this.update = this.update.bind(this);

  this.onNewNode = this.onNewNode.bind(this);
  this.onPreviewFonts = this.onPreviewFonts.bind(this);
  this.onShowAllFont = this.onShowAllFont.bind(this);
  this.onThemeChanged = this.onThemeChanged.bind(this);
}

FontInspector.prototype = {
  init() {
    if (!this.inspector) {
      return;
    }

    let app = App({
      onPreviewFonts: this.onPreviewFonts,
      onShowAllFont: this.onShowAllFont,
      onTextBoxContextMenu: this.inspector.onTextBoxContextMenu
    });

    let provider = createElement(Provider, {
      store: this.store,
      id: "fontinspector",
      title: INSPECTOR_L10N.getStr("inspector.sidebar.fontInspectorTitle"),
      key: "fontinspector",
    }, app);

    let defaultTab = Services.prefs.getCharPref("devtools.inspector.activeSidebar");

    this.inspector.addSidebarTab(
      "fontinspector",
      INSPECTOR_L10N.getStr("inspector.sidebar.fontInspectorTitle"),
      provider,
      defaultTab == "fontinspector"
    );

    this.inspector.selection.on("new-node-front", this.onNewNode);
    this.inspector.sidebar.on("fontinspector-selected", this.onNewNode);

    // Listen for theme changes as the color of the previews depend on the theme
    gDevTools.on("theme-switched", this.onThemeChanged);

    this.store.dispatch(updatePreviewText(""));
    this.store.dispatch(updateShowAllFonts(false));
    this.update(false, "");
  },

  /**
   * Destruction function called when the inspector is destroyed. Removes event listeners
   * and cleans up references.
   */
  destroy: function () {
    this.inspector.selection.off("new-node-front", this.onNewNode);
    this.inspector.sidebar.off("fontinspector-selected", this.onNewNode);
    gDevTools.off("theme-switched", this.onThemeChanged);

    this.document = null;
    this.inspector = null;
    this.pageStyle = null;
    this.store = null;
  },

  /**
   * Returns true if the font inspector panel is visible, and false otherwise.
   */
  isPanelVisible() {
    return this.inspector.sidebar &&
           this.inspector.sidebar.getCurrentTabID() === "fontinspector";
  },

  /**
   * Selection 'new-node' event handler.
   */
  onNewNode() {
    if (this.isPanelVisible()) {
      this.store.dispatch(updateShowAllFonts(false));
      this.update();
    }
  },

  /**
   * Handler for the "theme-switched" event.
   */
  onThemeChanged(event, frame) {
    if (frame === this.document.defaultView) {
      this.update();
    }
  },

  /**
   * Handler for change in preview input.
   */
  onPreviewFonts(value) {
    this.store.dispatch(updatePreviewText(value));
    this.update();
  },

  /**
   * Handler for click on show all fonts button.
   */
  onShowAllFont() {
    this.store.dispatch(updateShowAllFonts(true));
    this.update();
  },

  update: Task.async(function* () {
    let node = this.inspector.selection.nodeFront;

    if (!node ||
        !this.isPanelVisible() ||
        !this.inspector.selection.isConnected() ||
        !this.inspector.selection.isElementNode()) {
      return;
    }

    let { fontOptions } = this.store.getState();
    let { showAllFonts, previewText } = fontOptions;

    let options = {
      includePreviews: true,
      previewText,
      previewFillStyle: getColor("body-color")
    };

    let fonts = [];
    if (showAllFonts) {
      fonts = yield this.pageStyle.getAllUsedFontFaces(options)
                      .catch(console.error);
    } else {
      fonts = yield this.pageStyle.getUsedFontFaces(node, options)
                      .catch(console.error);
    }

    if (!fonts || !fonts.length) {
      // No fonts to display. Clear the previously shown fonts.
      this.store.dispatch(updateFonts(fonts));
      return;
    }

    for (let font of fonts) {
      font.previewUrl = yield font.preview.data.string();
    }

    // in case we've been destroyed in the meantime
    if (!this.document) {
      return;
    }

    this.store.dispatch(updateFonts(fonts));

    this.inspector.emit("fontinspector-updated");
  })
};

module.exports = FontInspector;
