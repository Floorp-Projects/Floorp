/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  getStr,
} = require("resource://devtools/client/inspector/layout/utils/l10n.js");

const GridItem = createFactory(
  require("resource://devtools/client/inspector/grids/components/GridItem.js")
);

const Types = require("resource://devtools/client/inspector/grids/types.js");

class GridList extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      getSwatchColorPickerTooltip: PropTypes.func.isRequired,
      grids: PropTypes.arrayOf(PropTypes.shape(Types.grid)).isRequired,
      onSetGridOverlayColor: PropTypes.func.isRequired,
      onToggleGridHighlighter: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
    };
  }

  render() {
    const {
      dispatch,
      getSwatchColorPickerTooltip,
      grids,
      onSetGridOverlayColor,
      onToggleGridHighlighter,
      setSelectedNode,
    } = this.props;

    return dom.div(
      { className: "grid-container" },
      dom.span(
        {
          role: "heading",
          "aria-level": "3",
        },
        getStr("layout.overlayGrid")
      ),
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
              dispatch,
              key: grid.id,
              getSwatchColorPickerTooltip,
              grid,
              grids,
              onSetGridOverlayColor,
              onToggleGridHighlighter,
              setSelectedNode,
            })
          )
      )
    );
  }
}

module.exports = GridList;
