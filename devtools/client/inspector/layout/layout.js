/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

const { createFactory, createElement } = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const App = createFactory(require("./components/App"));

const { LocalizationHelper } = require("devtools/shared/l10n");
const INSPECTOR_L10N =
  new LocalizationHelper("devtools/client/locales/inspector.properties");

const SHOW_GRID_OUTLINE_PREF = "devtools.gridinspector.showGridOutline";

function LayoutView(inspector, window) {
  this.document = window.document;
  this.inspector = inspector;
  this.store = inspector.store;

  this.init();
}

LayoutView.prototype = {

  init() {
    if (!this.inspector) {
      return;
    }

    let {
      setSelectedNode,
      onShowBoxModelHighlighterForNode,
    } = this.inspector.getCommonComponentProps();

    let {
      onHideBoxModelHighlighter,
      onShowBoxModelEditor,
      onShowBoxModelHighlighter,
      onToggleGeometryEditor,
    } = this.inspector.boxmodel.getComponentProps();

    let {
      getSwatchColorPickerTooltip,
      onSetGridOverlayColor,
      onShowGridAreaHighlight,
      onShowGridCellHighlight,
      onToggleGridHighlighter,
      onToggleShowGridLineNumbers,
      onToggleShowInfiniteLines,
    } = this.inspector.gridInspector.getComponentProps();

    let app = App({
      getSwatchColorPickerTooltip,
      setSelectedNode,
      /**
       * Shows the box model properties under the box model if true, otherwise, hidden by
       * default.
       */
      showBoxModelProperties: true,

      /**
       * Shows the grid outline if user preferences are set to true, otherwise, hidden by
       * default.
       */
      showGridOutline: Services.prefs.getBoolPref(SHOW_GRID_OUTLINE_PREF),

      onHideBoxModelHighlighter,
      onSetGridOverlayColor,
      onShowBoxModelEditor,
      onShowBoxModelHighlighter,
      onShowBoxModelHighlighterForNode,
      onShowGridAreaHighlight,
      onShowGridCellHighlight,
      onToggleGeometryEditor,
      onToggleGridHighlighter,
      onToggleShowGridLineNumbers,
      onToggleShowInfiniteLines,
    });

    let provider = createElement(Provider, {
      store: this.store,
      id: "layoutview",
      title: INSPECTOR_L10N.getStr("inspector.sidebar.layoutViewTitle2"),
      key: "layoutview",
    }, app);

    let defaultTab = Services.prefs.getCharPref("devtools.inspector.activeSidebar");

    this.inspector.addSidebarTab(
      "layoutview",
      INSPECTOR_L10N.getStr("inspector.sidebar.layoutViewTitle2"),
      provider,
      defaultTab == "layoutview"
    );
  },

  /**
   * Destruction function called when the inspector is destroyed. Cleans up references.
   */
  destroy() {
    this.document = null;
    this.inspector = null;
    this.store = null;
  },

};

module.exports = LayoutView;
