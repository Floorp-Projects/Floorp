/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { getStr } = require("devtools/client/inspector/layout/utils/l10n");

const GridDisplaySettings = createFactory(require("./GridDisplaySettings"));
const GridList = createFactory(require("./GridList"));
const GridOutline = createFactory(require("./GridOutline"));

const Types = require("../types");

class Grid extends PureComponent {
  static get propTypes() {
    return {
      getSwatchColorPickerTooltip: PropTypes.func.isRequired,
      grids: PropTypes.arrayOf(PropTypes.shape(Types.grid)).isRequired,
      highlighterSettings: PropTypes.shape(Types.highlighterSettings).isRequired,
      setSelectedNode: PropTypes.func.isRequired,
      onHideBoxModelHighlighter: PropTypes.func.isRequired,
      onSetGridOverlayColor: PropTypes.func.isRequired,
      onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
      onShowGridOutlineHighlight: PropTypes.func.isRequired,
      onToggleGridHighlighter: PropTypes.func.isRequired,
      onToggleShowGridAreas: PropTypes.func.isRequired,
      onToggleShowGridLineNumbers: PropTypes.func.isRequired,
      onToggleShowInfiniteLines: PropTypes.func.isRequired,
    };
  }

  render() {
    let {
      getSwatchColorPickerTooltip,
      grids,
      highlighterSettings,
      setSelectedNode,
      onHideBoxModelHighlighter,
      onSetGridOverlayColor,
      onShowBoxModelHighlighterForNode,
      onShowGridOutlineHighlight,
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
        GridOutline({
          grids,
          onShowGridOutlineHighlight,
        })
      )
      :
      dom.div(
        {
          className: "devtools-sidepanel-no-result",
        },
        getStr("layout.noGridsOnThisPage")
      );
  }
}

module.exports = Grid;
