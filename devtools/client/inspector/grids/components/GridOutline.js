/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { addons, createClass, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");
const { throttle } = require("devtools/client/inspector/shared/utils");

const Types = require("../types");

const COLUMNS = "cols";
const ROWS = "rows";

// The delay prior to executing the grid cell highlighting.
const GRID_CELL_MOUSEOVER_TIMEOUT = 150;

// Move SVG grid to the right 100 units, so that it is not flushed against the edge of
// layout border
const TRANSLATE_X = 0;
const TRANSLATE_Y = 0;

const GRID_CELL_SCALE_FACTOR = 50;

const VIEWPORT_MIN_HEIGHT = 100;
const VIEWPORT_MAX_HEIGHT = 150;

module.exports = createClass({

  displayName: "GridOutline",

  propTypes: {
    grids: PropTypes.arrayOf(PropTypes.shape(Types.grid)).isRequired,
    onShowGridAreaHighlight: PropTypes.func.isRequired,
    onShowGridCellHighlight: PropTypes.func.isRequired,
    onShowGridLineNamesHighlight: PropTypes.func.isRequired,
  },

  mixins: [ addons.PureRenderMixin ],

  getInitialState() {
    return {
      selectedGrids: [],
      height: 0,
      width: 0,
    };
  },

  componentWillMount() {
    // Throttle the grid highlighting of grid cells. It makes the UX smoother by not
    // lagging the grid cell highlighting if a lot of grid cells are mouseover in a
    // quick succession.
    this.highlightCell = throttle(this.highlightCell, GRID_CELL_MOUSEOVER_TIMEOUT);
  },

  componentWillReceiveProps({ grids }) {
    if (this.state.selectedGrids.length < 2) {
      this.setState({
        height: 0,
        width: 0,
      });
    }
    this.setState({
      selectedGrids: grids.filter(grid => grid.highlighted),
    });
  },

  /**
   * Returns the grid area name if the given grid cell is part of a grid area, otherwise
   * null.
   *
   * @param  {Number} columnNumber
   *         The column number of the grid cell.
   * @param  {Number} rowNumber
   *         The row number of the grid cell.
   * @param  {Array} areas
   *         Array of grid areas data stored in the grid fragment.
   * @return {String} If there is a grid area return area name, otherwise null.
   */
  getGridAreaName(columnNumber, rowNumber, areas) {
    const gridArea = areas.find(area =>
      (area.rowStart <= rowNumber && area.rowEnd > rowNumber) &&
      (area.columnStart <= columnNumber && area.columnEnd > columnNumber)
    );

    if (!gridArea) {
      return null;
    }

    return gridArea.name;
  },

  /**
   * Returns the height of the grid outline ranging between a minimum and maximum height.
   *
   * @return {Number} The height of the grid outline.
   */
  getHeight() {
    const { height } = this.state;

    if (height >= VIEWPORT_MAX_HEIGHT) {
      return VIEWPORT_MAX_HEIGHT;
    } else if (height <= VIEWPORT_MIN_HEIGHT) {
      return VIEWPORT_MIN_HEIGHT;
    }

    return height;
  },

  highlightCell({ target }) {
    const {
      grids,
      onShowGridAreaHighlight,
      onShowGridCellHighlight,
    } = this.props;
    const name = target.getAttribute("data-grid-area-name");
    const id = target.getAttribute("data-grid-id");
    const fragmentIndex = target.getAttribute("data-grid-fragment-index");
    const color = target.getAttribute("stroke");
    const rowNumber = target.getAttribute("data-grid-row");
    const columnNumber = target.getAttribute("data-grid-column");

    target.setAttribute("fill", color);

    if (name) {
      onShowGridAreaHighlight(grids[id].nodeFront, name, color);
    }

    if (fragmentIndex && rowNumber && columnNumber) {
      onShowGridCellHighlight(grids[id].nodeFront, color, fragmentIndex,
        rowNumber, columnNumber);
    }
  },

  /**
    * Renders the grid outline for the given grid container object.
    *
    * @param  {Object} grid
    *         A single grid container in the document.
    */
  renderGrid(grid) {
    const { id, color, gridFragments } = grid;
    // TODO: We are drawing the first fragment since only one is currently being stored.
    // In the future we will need to iterate over all fragments of a grid.
    let gridFragmentIndex = 0;
    const { rows, cols, areas } = gridFragments[gridFragmentIndex];
    const numberOfColumns = cols.lines.length - 1;
    const numberOfRows = rows.lines.length - 1;
    const rectangles = [];
    let x = 1;
    let y = 1;
    let width = 0;
    let height = 0;
    // The grid outline border height/width is the total height/width of grid cells drawn.
    let totalHeight = 0;
    let totalWidth = 0;

      // Draw the cells contained within the grid outline border.
    for (let rowNumber = 1; rowNumber <= numberOfRows; rowNumber++) {
      height = GRID_CELL_SCALE_FACTOR * (rows.tracks[rowNumber - 1].breadth / 100);
      for (let columnNumber = 1; columnNumber <= numberOfColumns; columnNumber++) {
        width = GRID_CELL_SCALE_FACTOR * (cols.tracks[columnNumber - 1].breadth / 100);

        const gridAreaName = this.getGridAreaName(columnNumber, rowNumber, areas);
        const gridCell = this.renderGridCell(id, gridFragmentIndex, x, y,
          rowNumber, columnNumber, color, gridAreaName, width, height);

        rectangles.push(gridCell);
        x += width;
      }

      x = 1;
      y += height;
      totalHeight += height;
    }

    // Find the total width of the grid container so we can draw the border for it
    for (let columnNumber = 0; columnNumber < numberOfColumns; columnNumber++) {
      totalWidth += GRID_CELL_SCALE_FACTOR * (cols.tracks[columnNumber].breadth / 100);
    }

    // Store the height of the grid container in the component state to prevent overflow
    // issues. We want to store the width of the grid container as well so that the
    // viewbox is only the calculated width of the grid outline.
    if (totalHeight > this.state.height || totalWidth > this.state.width) {
      this.setState({
        height: totalHeight + 20,
        width: totalWidth,
      });
    }

    // Draw a rectangle that acts as the grid outline border.
    const border = this.renderGridOutlineBorder(totalWidth, totalHeight, color);
    rectangles.unshift(border);

    return rectangles;
  },

  /**
   * Renders the grid cell of a grid fragment.
   *
   * @param  {Number} id
   *         The grid id stored on the grid fragment
   * @param  {Number} gridFragmentIndex
   *         The index of the grid fragment rendered to the document.
   * @param  {Number} x
   *         The x-position of the grid cell.
   * @param  {Number} y
   *         The y-position of the grid cell.
   * @param  {Number} rowNumber
   *         The row number of the grid cell.
   * @param  {Number} columnNumber
   *         The column number of the grid cell.
   * @param  {String|null} gridAreaName
   *         The grid area name or null if the grid cell is not part of a grid area.
   * @param  {Number} width
   *         The width of grid cell.
   * @param  {Number} height
   *         The height of the grid cell.
   */
  renderGridCell(id, gridFragmentIndex, x, y, rowNumber, columnNumber, color,
    gridAreaName, width, height) {
    return dom.rect(
      {
        className: "grid-outline-cell",
        "data-grid-area-name": gridAreaName,
        "data-grid-fragment-index": gridFragmentIndex,
        "data-grid-id": id,
        "data-grid-row": rowNumber,
        "data-grid-column": columnNumber,
        x,
        y,
        width,
        height,
        fill: "none",
        stroke: color,
        onMouseOver: this.onMouseOverCell,
        onMouseOut: this.onMouseLeaveCell,
      }
    );
  },

  renderGridOutline(grids) {
    return dom.g(
      {
        className: "grid-cell-group",
      },
      grids.map(grid => this.renderGrid(grid))
    );
  },

  renderGridOutlineBorder(borderWidth, borderHeight, color) {
    return dom.rect(
      {
        className: "grid-outline-border",
        x: 1,
        y: 1,
        width: borderWidth,
        height: borderHeight,
        stroke: color,
      }
    );
  },

  /**
 * Renders the grid line of a grid fragment.
   *
   * @param  {Number} id
   *         The grid id stored on the grid fragment
   * @param  {Number} gridFragmentIndex
   *         The index of the grid fragment rendered to the document.
   * @param  {String} color
   *         The color of the grid.
   * @param  {Number} x1
   *         The starting x-coordinate of the grid line.
   * @param  {Number} y1
   *         The starting y-coordinate of the grid line.
   * @param  {Number} x2
   *         The ending x-coordinate of the grid line.
   * @param  {Number} y2
   *         The ending y-coordinate of the grid line.
   * @param  {Number} gridLineNumber
   *         The grid line number of the line being rendered.
   * @param  {String} lineType
   *         The grid line name(s) of the line being rendered.
   */
  renderGridLine(id, gridFragmentIndex, color, x1, y1, x2, y2,
    gridLineNumber, lineType) {
    return dom.line(
      {
        className: "grid-outline-line",
        "data-grid-fragment-index": gridFragmentIndex,
        "data-grid-id": id,
        "data-grid-line-color": color,
        "data-grid-line-number": gridLineNumber,
        "data-grid-line-type": lineType,
        x1,
        y1,
        x2,
        y2,
        onMouseOver: this.onMouseOverLine,
        onMouseOut: this.onMouseLeaveLine,
        stroke: "#000000",
      }
    );
  },

  renderGridLines(grids) {
    return dom.g(
      {
        className: "grid-outline-lines",
      },
      grids.map(grid => this.renderLines(grid))
    );
  },

  renderLines(grid) {
    const { id, color, gridFragments } = grid;
    const { width, height } = this.state;
    let gridFragmentIndex = 0;
    const { rows, cols } = gridFragments[gridFragmentIndex];
    const numberOfColumns = cols.lines.length - 1;
    const numberOfRows = rows.lines.length - 1;
    const lines = [];

    let x = 1;
    let y = 1;
    let rowBreadth = 0;
    let colBreadth = 0;

    if (width > 0 && height > 0) {
      for (let row = 0; row <= numberOfRows; row++) {
        if (row < numberOfRows) {
          rowBreadth = GRID_CELL_SCALE_FACTOR * (rows.tracks[row].breadth / 100);
        }
        const { number } = rows.lines[row];
        const rowLine = this.renderGridLine(id, gridFragmentIndex, color,
          x, y, width - 20, y, number, ROWS);

        lines.push(rowLine);
        y += rowBreadth;
      }

      y = 1;

      for (let col = 0; col <= numberOfColumns; col++) {
        if (col < numberOfColumns) {
          colBreadth = GRID_CELL_SCALE_FACTOR * (cols.tracks[col].breadth / 100);
        }
        const { number } = cols.lines[col];
        const colLine = this.renderGridLine(id, gridFragmentIndex, color,
          x, y, x, height - 20, number, COLUMNS);

        lines.push(colLine);
        x += colBreadth;
      }
    }

    return lines;
  },

  onMouseLeaveCell({ target }) {
    const {
      grids,
      onShowGridAreaHighlight,
      onShowGridCellHighlight,
    } = this.props;
    const id = target.getAttribute("data-grid-id");
    const color = target.getAttribute("stroke");

    target.setAttribute("fill", "none");

    onShowGridAreaHighlight(grids[id].nodeFront, null, color);
    onShowGridCellHighlight(grids[id].nodeFront, color);
  },

  onMouseOverCell(event) {
    event.persist();
    this.highlightCell(event);
  },

  onMouseLeaveLine({ target }) {
    const { grids, onShowGridLineNamesHighlight } = this.props;
    const fragmentIndex = target.getAttribute("data-grid-fragment-index");
    const id = target.getAttribute("data-grid-id");
    const color = target.getAttribute("data-grid-line-color");

    onShowGridLineNamesHighlight(grids[id].nodeFront, fragmentIndex, color);
  },

  onMouseOverLine({ target }) {
    const { grids, onShowGridLineNamesHighlight } = this.props;
    const fragmentIndex = target.getAttribute("data-grid-fragment-index");
    const id = target.getAttribute("data-grid-id");
    const lineNumber = target.getAttribute("data-grid-line-number");
    const type = target.getAttribute("data-grid-line-type");
    const color = target.getAttribute("data-grid-line-color");

    onShowGridLineNamesHighlight(grids[id].nodeFront, fragmentIndex, color,
      lineNumber, type);
  },

  render() {
    const { selectedGrids, height, width } = this.state;

    return selectedGrids.length ?
      dom.svg(
        {
          className: "grid-outline",
          width: "100%",
          height: this.getHeight(),
          viewBox: `${TRANSLATE_X} ${TRANSLATE_Y} ${width} ${height}`,
        },
        this.renderGridOutline(selectedGrids),
        this.renderGridLines(selectedGrids)
      )
      :
      null;
  },

});
