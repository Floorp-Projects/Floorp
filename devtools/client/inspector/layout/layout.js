/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  createElement,
} = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");
const FlexboxInspector = require("devtools/client/inspector/flexbox/flexbox");
const GridInspector = require("devtools/client/inspector/grids/grid-inspector");

const LayoutApp = createFactory(
  require("devtools/client/inspector/layout/components/LayoutApp")
);

const { LocalizationHelper } = require("devtools/shared/l10n");
const INSPECTOR_L10N = new LocalizationHelper(
  "devtools/client/locales/inspector.properties"
);

loader.lazyRequireGetter(
  this,
  "SwatchColorPickerTooltip",
  "devtools/client/shared/widgets/tooltip/SwatchColorPickerTooltip"
);

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

    const { setSelectedNode } = this.inspector.getCommonComponentProps();

    const {
      onShowBoxModelEditor,
      onShowRulePreviewTooltip,
      onToggleGeometryEditor,
    } = this.inspector.getPanel("boxmodel").getComponentProps();

    this.flexboxInspector = new FlexboxInspector(
      this.inspector,
      this.inspector.panelWin
    );
    const {
      onSetFlexboxOverlayColor,
    } = this.flexboxInspector.getComponentProps();

    this.gridInspector = new GridInspector(
      this.inspector,
      this.inspector.panelWin
    );
    const {
      onSetGridOverlayColor,
      onToggleGridHighlighter,
      onToggleShowGridAreas,
      onToggleShowGridLineNumbers,
      onToggleShowInfiniteLines,
    } = this.gridInspector.getComponentProps();

    const layoutApp = LayoutApp({
      getSwatchColorPickerTooltip: () => this.swatchColorPickerTooltip,
      onSetFlexboxOverlayColor,
      onSetGridOverlayColor,
      onShowBoxModelEditor,
      onShowRulePreviewTooltip,
      onToggleGeometryEditor,
      onToggleGridHighlighter,
      onToggleShowGridAreas,
      onToggleShowGridLineNumbers,
      onToggleShowInfiniteLines,
      setSelectedNode,
      /**
       * Shows the box model properties under the box model if true, otherwise, hidden by
       * default.
       */
      showBoxModelProperties: true,
    });

    const provider = createElement(
      Provider,
      {
        id: "layoutview",
        key: "layoutview",
        store: this.store,
        title: INSPECTOR_L10N.getStr("inspector.sidebar.layoutViewTitle2"),
      },
      layoutApp
    );

    // Expose the provider to let inspector.js use it in setupSidebar.
    this.provider = provider;
  }

  /**
   * Destruction function called when the inspector is destroyed. Cleans up references.
   */
  destroy() {
    if (this._swatchColorPickerTooltip) {
      this._swatchColorPickerTooltip.destroy();
      this._swatchColorPickerTooltip = null;
    }

    this.flexboxInspector.destroy();
    this.gridInspector.destroy();

    this.document = null;
    this.inspector = null;
    this.store = null;
  }

  get swatchColorPickerTooltip() {
    if (!this._swatchColorPickerTooltip) {
      this._swatchColorPickerTooltip = new SwatchColorPickerTooltip(
        this.inspector.toolbox.doc,
        this.inspector,
        { supportsCssColor4ColorFunction: () => false }
      );
    }

    return this._swatchColorPickerTooltip;
  }
}

module.exports = LayoutView;
