/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { addons, createClass, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");

const Types = require("../types");

// Move SVG grid to the right 100 units, so that it is not flushed against the edge of
// layout border
const TRANSLATE_X = -100;
const TRANSLATE_Y = 0;

const VIEWPORT_HEIGHT = 100;
const VIEWPORT_WIDTH = 450;

module.exports = createClass({

  displayName: "GridOutline",

  propTypes: {
    grids: PropTypes.arrayOf(PropTypes.shape(Types.grid)).isRequired,
  },

  mixins: [ addons.PureRenderMixin ],

  getInitialState() {
    return {
      selectedGrids: [],
    };
  },

  componentWillReceiveProps({ grids }) {
    this.setState({
      selectedGrids: grids.filter(grid => grid.highlighted),
    });
  },

  renderGridCell(columnNumber, rowNumber, color) {
    return dom.rect(
      {
        className: "grid-outline-cell",
        x: columnNumber,
        y: rowNumber,
        width: 10,
        height: 10,
        stroke: color,
      }
    );
  },

  renderGridFragment({ color, gridFragments }) {
    // TODO: We are drawing the first fragment since only one is currently being stored.
    // In future we will need to iterate over all fragments of a grid.
    const { rows, cols } = gridFragments[0];
    const numOfColLines = cols.lines.length - 1;
    const numOfRowLines = rows.lines.length - 1;
    const rectangles = [];

    // Draw a rectangle that acts as a border for the grid outline
    const border = this.renderGridOutlineBorder(numOfRowLines, numOfColLines, color);
    rectangles.push(border);

    let x = 1;
    let y = 1;
    const width = 10;
    const height = 10;

    // Draw the cells within the grid outline border
    for (let row = 0; row < numOfRowLines; row++) {
      for (let col = 0; col < numOfColLines; col++) {
        rectangles.push(this.renderGridCell(x, y, color));
        x += width;
      }

      x = 1;
      y += height;
    }

    return rectangles;
  },

  renderGridOutline(gridFragments) {
    return dom.g(
      {
        className: "grid-cell-group",
      },
      gridFragments.map(gridFragment => this.renderGridFragment(gridFragment))
    );
  },

  renderGridOutlineBorder(numberOfRows, numberOfCols, color) {
    return dom.rect(
      {
        className: "grid-outline-border",
        x: 1,
        y: 1,
        width: numberOfCols * 10,
        height: numberOfRows * 10,
        stroke: color,
      }
    );
  },

  render() {
    return this.state.selectedGrids.length ?
      dom.svg(
        {
          className: "grid-outline",
          width: "100%",
          height: 100,
          viewBox: `${TRANSLATE_X} ${TRANSLATE_Y} ${VIEWPORT_WIDTH} ${VIEWPORT_HEIGHT}`,
        },
        this.renderGridOutline(this.state.selectedGrids)
      )
      :
      null;
  },

});
