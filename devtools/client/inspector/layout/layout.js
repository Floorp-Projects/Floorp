/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, createElement } = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const LayoutApp = createFactory(require("./components/LayoutApp"));

const { LocalizationHelper } = require("devtools/shared/l10n");
const INSPECTOR_L10N =
  new LocalizationHelper("devtools/client/locales/inspector.properties");

loader.lazyRequireGetter(this, "FlexboxInspector", "devtools/client/inspector/flexbox/flexbox");
loader.lazyRequireGetter(this, "GridInspector", "devtools/client/inspector/grids/grid-inspector");

class LayoutView {
  constructor(inspector, window) {
    this.document = window.document;
    this.inspector = inspector;
    this.store = inspector.store;

    this.init();
  }

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

    this.flexboxInspector = new FlexboxInspector(this.inspector,
      this.inspector.panelWin);
    let {
      onToggleFlexboxHighlighter,
    } = this.flexboxInspector.getComponentProps();

    this.gridInspector = new GridInspector(this.inspector, this.inspector.panelWin);
    let {
      getSwatchColorPickerTooltip,
      onSetGridOverlayColor,
      onShowGridOutlineHighlight,
      onToggleGridHighlighter,
      onToggleShowGridAreas,
      onToggleShowGridLineNumbers,
      onToggleShowInfiniteLines,
    } = this.gridInspector.getComponentProps();

    let layoutApp = LayoutApp({
      getSwatchColorPickerTooltip,
      setSelectedNode,
      /**
       * Shows the box model properties under the box model if true, otherwise, hidden by
       * default.
       */
      showBoxModelProperties: true,
      onHideBoxModelHighlighter,
      onSetGridOverlayColor,
      onShowBoxModelEditor,
      onShowBoxModelHighlighter,
      onShowBoxModelHighlighterForNode,
      onShowGridOutlineHighlight,
      onToggleFlexboxHighlighter,
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
    }, layoutApp);

    // Expose the provider to let inspector.js use it in setupSidebar.
    this.provider = provider;
  }

  /**
   * Destruction function called when the inspector is destroyed. Cleans up references.
   */
  destroy() {
    this.flexboxInspector.destroy();
    this.gridInspector.destroy();

    this.document = null;
    this.inspector = null;
    this.store = null;
  }
}

module.exports = LayoutView;
