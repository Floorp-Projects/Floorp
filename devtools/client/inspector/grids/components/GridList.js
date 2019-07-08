/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { getStr } = require("devtools/client/inspector/layout/utils/l10n");

const GridItem = createFactory(require("./GridItem"));

const Types = require("../types");

class GridList extends PureComponent {
  static get propTypes() {
    return {
      getSwatchColorPickerTooltip: PropTypes.func.isRequired,
      grids: PropTypes.arrayOf(PropTypes.shape(Types.grid)).isRequired,
      onHideBoxModelHighlighter: PropTypes.func.isRequired,
      onSetGridOverlayColor: PropTypes.func.isRequired,
      onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
      onToggleGridHighlighter: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
    };
  }

  render() {
    const {
      getSwatchColorPickerTooltip,
      grids,
      onHideBoxModelHighlighter,
      onSetGridOverlayColor,
      onShowBoxModelHighlighterForNode,
      onToggleGridHighlighter,
      setSelectedNode,
    } = this.props;

    return dom.div(
      { className: "grid-container" },
      dom.span({}, getStr("layout.overlayGrid")),
      dom.ul(
        {
          id: "grid-list",
          className: "devtools-monospace",
        },
        grids
          // Skip subgrids since they are rendered by their parent grids in GridItem.
          .filter(grid => !grid.isSubgrid)
          .map(grid =>
            GridItem({
              key: grid.id,
              getSwatchColorPickerTooltip,
              grid,
              grids,
              onHideBoxModelHighlighter,
              onSetGridOverlayColor,
              onShowBoxModelHighlighterForNode,
              onToggleGridHighlighter,
              setSelectedNode,
            })
          )
      )
    );
  }
}

module.exports = GridList;
