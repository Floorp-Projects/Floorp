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

// @remove after release 56 (See Bug 1355747)
const PROMOTE_COUNT_PREF = "devtools.promote.layoutview";

// @remove after release 56 (See Bug 1355747)
const GRID_LINK = "https://www.mozilla.org/en-US/developer/css-grid/?utm_source=gridtooltip&utm_medium=devtools&utm_campaign=cssgrid_layout";

function LayoutView(inspector, window) {
  this.document = window.document;
  this.inspector = inspector;
  this.store = inspector.store;

  this.onPromoteLearnMoreClick = this.onPromoteLearnMoreClick.bind(this);

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
    } = this.inspector.getPanel("boxmodel").getComponentProps();

    let {
      getSwatchColorPickerTooltip,
      onSetGridOverlayColor,
      onShowGridAreaHighlight,
      onShowGridCellHighlight,
      onShowGridLineNamesHighlight,
      onToggleGridHighlighter,
      onToggleShowGridAreas,
      onToggleShowGridLineNumbers,
      onToggleShowInfiniteLines,
    } = this.inspector.gridInspector.getComponentProps();

    let {
      onPromoteLearnMoreClick,
    } = this;

    let app = App({
      getSwatchColorPickerTooltip,
      setSelectedNode,
      /**
       * Shows the box model properties under the box model if true, otherwise, hidden by
       * default.
       */
      showBoxModelProperties: true,
      onHideBoxModelHighlighter,
      onPromoteLearnMoreClick,
      onSetGridOverlayColor,
      onShowBoxModelEditor,
      onShowBoxModelHighlighter,
      onShowBoxModelHighlighterForNode,
      onShowGridAreaHighlight,
      onShowGridCellHighlight,
      onShowGridLineNamesHighlight,
      onToggleGeometryEditor,
      onToggleGridHighlighter,
      onToggleShowGridAreas,
      onToggleShowGridLineNumbers,
      onToggleShowInfiniteLines,
    });

    let provider = createElement(Provider, {
      id: "layoutview",
      key: "layoutview",
      store: this.store,
      title: INSPECTOR_L10N.getStr("inspector.sidebar.layoutViewTitle2"),
      // @remove after release 56 (See Bug 1355747)
      badge: Services.prefs.getIntPref(PROMOTE_COUNT_PREF) > 0 ?
        INSPECTOR_L10N.getStr("inspector.sidebar.newBadge") : null,
      showBadge: () => Services.prefs.getIntPref(PROMOTE_COUNT_PREF) > 0,
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

  onPromoteLearnMoreClick() {
    let browserWin = this.inspector.target.tab.ownerDocument.defaultView;
    browserWin.openUILinkIn(GRID_LINK, "current");
  }

};

module.exports = LayoutView;
