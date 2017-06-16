/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { addons, createClass, createFactory, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");

const GridDisplaySettings = createFactory(require("./GridDisplaySettings"));
const GridList = createFactory(require("./GridList"));
const GridOutline = createFactory(require("./GridOutline"));

const Types = require("../types");
const { getStr } = require("../utils/l10n");

module.exports = createClass({

  displayName: "Grid",

  propTypes: {
    getSwatchColorPickerTooltip: PropTypes.func.isRequired,
    grids: PropTypes.arrayOf(PropTypes.shape(Types.grid)).isRequired,
    highlighterSettings: PropTypes.shape(Types.highlighterSettings).isRequired,
    setSelectedNode: PropTypes.func.isRequired,
    showGridOutline: PropTypes.bool.isRequired,
    onHideBoxModelHighlighter: PropTypes.func.isRequired,
    onSetGridOverlayColor: PropTypes.func.isRequired,
    onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
    onShowGridAreaHighlight: PropTypes.func.isRequired,
    onShowGridCellHighlight: PropTypes.func.isRequired,
    onShowGridLineNamesHighlight: PropTypes.func.isRequired,
    onToggleGridHighlighter: PropTypes.func.isRequired,
    onToggleShowGridAreas: PropTypes.func.isRequired,
    onToggleShowGridLineNumbers: PropTypes.func.isRequired,
    onToggleShowInfiniteLines: PropTypes.func.isRequired,
  },

  mixins: [ addons.PureRenderMixin ],

  render() {
    let {
      getSwatchColorPickerTooltip,
      grids,
      highlighterSettings,
      setSelectedNode,
      showGridOutline,
      onHideBoxModelHighlighter,
      onSetGridOverlayColor,
      onShowBoxModelHighlighterForNode,
      onShowGridAreaHighlight,
      onShowGridCellHighlight,
      onToggleShowGridAreas,
      onToggleGridHighlighter,
      onToggleShowGridLineNumbers,
      onToggleShowInfiniteLines,
    } = this.props;

    return grids.length ?
      dom.div(
        {
          id: "layout-grid-container",
        },
        dom.div(
          {
            className: "grid-content",
          },
          GridList({
            getSwatchColorPickerTooltip,
            grids,
            setSelectedNode,
            onHideBoxModelHighlighter,
            onSetGridOverlayColor,
            onShowBoxModelHighlighterForNode,
            onToggleGridHighlighter,
          }),
          GridDisplaySettings({
            highlighterSettings,
            onToggleShowGridAreas,
            onToggleShowGridLineNumbers,
            onToggleShowInfiniteLines,
          })
        ),
        showGridOutline ?
          GridOutline({
            grids,
            onShowGridAreaHighlight,
            onShowGridCellHighlight,
          })
          :
          null
      )
      :
      dom.div(
        {
          className: "layout-no-grids",
        },
        getStr("layout.noGridsOnThisPage")
      );
  },

});
