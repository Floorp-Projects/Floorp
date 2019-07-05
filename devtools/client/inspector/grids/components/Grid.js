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

// Normally, we would only lazy load GridOutline, but we also lazy load
// GridDisplaySettings and GridList because we assume the CSS grid usage is low
// and usually will not appear on the page.
loader.lazyGetter(this, "GridDisplaySettings", function() {
  return createFactory(require("./GridDisplaySettings"));
});
loader.lazyGetter(this, "GridList", function() {
  return createFactory(require("./GridList"));
});
loader.lazyGetter(this, "GridOutline", function() {
  return createFactory(require("./GridOutline"));
});

const Types = require("../types");

class Grid extends PureComponent {
  static get propTypes() {
    return {
      getSwatchColorPickerTooltip: PropTypes.func.isRequired,
      grids: PropTypes.arrayOf(PropTypes.shape(Types.grid)).isRequired,
      highlighterSettings: PropTypes.shape(Types.highlighterSettings)
        .isRequired,
      onHideBoxModelHighlighter: PropTypes.func.isRequired,
      onSetGridOverlayColor: PropTypes.func.isRequired,
      onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
      onShowGridOutlineHighlight: PropTypes.func.isRequired,
      onToggleGridHighlighter: PropTypes.func.isRequired,
      onToggleShowGridAreas: PropTypes.func.isRequired,
      onToggleShowGridLineNumbers: PropTypes.func.isRequired,
      onToggleShowInfiniteLines: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
    };
  }

  render() {
    if (!this.props.grids.length) {
      return dom.div(
        { className: "devtools-sidepanel-no-result" },
        getStr("layout.noGridsOnThisPage")
      );
    }

    const {
      getSwatchColorPickerTooltip,
      grids,
      highlighterSettings,
      onHideBoxModelHighlighter,
      onSetGridOverlayColor,
      onShowBoxModelHighlighterForNode,
      onShowGridOutlineHighlight,
      onToggleShowGridAreas,
      onToggleGridHighlighter,
      onToggleShowGridLineNumbers,
      onToggleShowInfiniteLines,
      setSelectedNode,
    } = this.props;
    const highlightedGrids = grids.filter(grid => grid.highlighted);

    return dom.div(
      { id: "layout-grid-container" },
      dom.div(
        { className: "grid-content" },
        GridList({
          getSwatchColorPickerTooltip,
          grids,
          onHideBoxModelHighlighter,
          onSetGridOverlayColor,
          onShowBoxModelHighlighterForNode,
          onToggleGridHighlighter,
          setSelectedNode,
        }),
        GridDisplaySettings({
          highlighterSettings,
          onToggleShowGridAreas,
          onToggleShowGridLineNumbers,
          onToggleShowInfiniteLines,
        })
      ),
      highlightedGrids.length === 1
        ? GridOutline({
            grids,
            onShowGridOutlineHighlight,
          })
        : null
    );
  }
}

module.exports = Grid;
