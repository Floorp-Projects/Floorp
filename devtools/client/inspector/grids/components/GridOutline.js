/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { addons, createClass, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");

const Types = require("../types");
const { getStr } = require("../utils/l10n");

// The delay prior to executing the grid cell highlighting.
const GRID_HIGHLIGHTING_DEBOUNCE = 50;

// Minimum height/width a grid cell can be
const MIN_CELL_HEIGHT = 5;
const MIN_CELL_WIDTH = 5;

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
  },

  mixins: [ addons.PureRenderMixin ],

  getInitialState() {
    return {
      height: 0,
      selectedGrid: null,
      showOutline: true,
      width: 0,
    };
  },

  componentWillReceiveProps({ grids }) {
    let selectedGrid = grids.find(grid => grid.highlighted);

    // Store the height of the grid container in the component state to prevent overflow
    // issues. We want to store the width of the grid container as well so that the
    // viewbox is only the calculated width of the grid outline.
    let { width, height } = selectedGrid
                            ? this.getTotalWidthAndHeight(selectedGrid)
                            : { width: 0, height: 0 };

    this.setState({ height, width, selectedGrid, showOutline: true });
  },

  /**
   * Get the width and height of a given grid.
   *
   * @param  {Object} grid
   *         A single grid container in the document.
   * @return {Object} An object like { width, height }
   */
  getTotalWidthAndHeight(grid) {
    // TODO: We are drawing the first fragment since only one is currently being stored.
    // In the future we will need to iterate over all fragments of a grid.
    const { gridFragments } = grid;
    const { rows, cols } = gridFragments[0];

    let height = 0;
    for (let i = 0; i < rows.lines.length - 1; i++) {
      height += GRID_CELL_SCALE_FACTOR * (rows.tracks[i].breadth / 100);
    }

    let width = 0;
    for (let i = 0; i < cols.lines.length - 1; i++) {
      width += GRID_CELL_SCALE_FACTOR * (cols.tracks[i].breadth / 100);
    }

    return { width, height };
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

  onHighlightCell({ target, type }) {
    // Debounce the highlighting of cells.
    // This way we don't end up sending many requests to the server for highlighting when
    // cells get hovered in a rapid succession We only send a request if the user settles
    // on a cell for some time.
    if (this.highlightTimeout) {
      clearTimeout(this.highlightTimeout);
    }

    this.highlightTimeout = setTimeout(() => {
      this.doHighlightCell(target, type === "mouseleave");
      this.highlightTimeout = null;
    }, GRID_HIGHLIGHTING_DEBOUNCE);
  },

  doHighlightCell(target, hide) {
    const {
      grids,
      onShowGridAreaHighlight,
      onShowGridCellHighlight,
    } = this.props;
    const name = target.dataset.gridAreaName;
    const id = target.dataset.gridId;
    const fragmentIndex = target.dataset.gridFragmentIndex;
    const color = target.closest(".grid-cell-group").dataset.gridLineColor;
    const rowNumber = target.dataset.gridRow;
    const columnNumber = target.dataset.gridColumn;

    onShowGridAreaHighlight(grids[id].nodeFront, null, color);
    onShowGridCellHighlight(grids[id].nodeFront, color);

    if (hide) {
      return;
    }

    if (name) {
      onShowGridAreaHighlight(grids[id].nodeFront, name, color);
    }

    if (fragmentIndex && rowNumber && columnNumber) {
      onShowGridCellHighlight(grids[id].nodeFront, color, fragmentIndex,
        rowNumber, columnNumber);
    }
  },

  /**
   * Displays a message text "Cannot show outline for this grid".
   */
  renderCannotShowOutlineText() {
    return dom.div(
      {
        className: "grid-outline-text"
      },
      dom.span(
        {
          className: "grid-outline-text-icon",
          title: getStr("layout.cannotShowGridOutline.title")
        }
      ),
      getStr("layout.cannotShowGridOutline")
    );
  },

  /**
   * Renders the grid outline for the given grid container object.
   *
   * @param  {Object} grid
   *         A single grid container in the document.
   */
  renderGrid(grid) {
    // TODO: We are drawing the first fragment since only one is currently being stored.
    // In the future we will need to iterate over all fragments of a grid.
    let gridFragmentIndex = 0;
    const { id, color, gridFragments } = grid;
    const { rows, cols, areas } = gridFragments[gridFragmentIndex];

    const numberOfColumns = cols.lines.length - 1;
    const numberOfRows = rows.lines.length - 1;
    const rectangles = [];
    let x = 1;
    let y = 1;
    let width = 0;
    let height = 0;

    // Draw the cells contained within the grid outline border.
    for (let rowNumber = 1; rowNumber <= numberOfRows; rowNumber++) {
      height = GRID_CELL_SCALE_FACTOR * (rows.tracks[rowNumber - 1].breadth / 100);

      for (let columnNumber = 1; columnNumber <= numberOfColumns; columnNumber++) {
        width = GRID_CELL_SCALE_FACTOR * (cols.tracks[columnNumber - 1].breadth / 100);

        // If a grid cell is less than the minimum pixels in width or height,
        // do not render the outline at all.
        if (width < MIN_CELL_WIDTH || height < MIN_CELL_HEIGHT) {
          this.setState({ showOutline: false });

          return [];
        }

        const gridAreaName = this.getGridAreaName(columnNumber, rowNumber, areas);
        const gridCell = this.renderGridCell(id, gridFragmentIndex, x, y,
                                             rowNumber, columnNumber, color, gridAreaName,
                                             width, height);

        rectangles.push(gridCell);
        x += width;
      }

      x = 1;
      y += height;
    }

    // Draw a rectangle that acts as the grid outline border.
    const border = this.renderGridOutlineBorder(this.state.width, this.state.height,
                                                color);
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
        "key": `${id}-${rowNumber}-${columnNumber}`,
        "className": "grid-outline-cell",
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
        onMouseEnter: this.onHighlightCell,
        onMouseLeave: this.onHighlightCell,
      }
    );
  },

  renderGridOutline(grid) {
    let { color } = grid;

    return dom.g(
      {
        id: "grid-cell-group",
        "className": "grid-cell-group",
        "data-grid-line-color": color,
        "style": { color }
      },
      this.renderGrid(grid)
    );
  },

  renderGridOutlineBorder(borderWidth, borderHeight, color) {
    return dom.rect(
      {
        key: "border",
        className: "grid-outline-border",
        x: 1,
        y: 1,
        width: borderWidth,
        height: borderHeight
      }
    );
  },

  renderOutline() {
    const {
      height,
      selectedGrid,
      showOutline,
      width,
    } = this.state;

    return showOutline ?
      dom.svg(
        {
          id: "grid-outline",
          width: "100%",
          height: this.getHeight(),
          viewBox: `${TRANSLATE_X} ${TRANSLATE_Y} ${width} ${height}`,
        },
        this.renderGridOutline(selectedGrid)
      )
      :
      this.renderCannotShowOutlineText();
  },

  render() {
    const { selectedGrid } = this.state;

    return selectedGrid ?
      dom.div(
        {
          id: "grid-outline-container",
          className: "grid-outline-container",
        },
        this.renderOutline()
      )
      :
      null;
  },

});
