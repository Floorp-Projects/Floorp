/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { getStr } = require("devtools/client/inspector/layout/utils/l10n");
const {
  getWritingModeMatrix,
  getCSSMatrixTransform,
} = require("devtools/shared/layout/dom-matrix-2d");

const Types = require("../types");

// The delay prior to executing the grid cell highlighting.
const GRID_HIGHLIGHTING_DEBOUNCE = 50;

// Prefs for the max number of rows/cols a grid container can have for
// the outline to display.
const GRID_OUTLINE_MAX_ROWS_PREF =
  Services.prefs.getIntPref("devtools.gridinspector.gridOutlineMaxRows");
const GRID_OUTLINE_MAX_COLUMNS_PREF =
  Services.prefs.getIntPref("devtools.gridinspector.gridOutlineMaxColumns");

// Move SVG grid to the right 100 units, so that it is not flushed against the edge of
// layout border
const TRANSLATE_X = 0;
const TRANSLATE_Y = 0;

const GRID_CELL_SCALE_FACTOR = 50;

const VIEWPORT_MIN_HEIGHT = 100;
const VIEWPORT_MAX_HEIGHT = 150;

class GridOutline extends PureComponent {
  static get propTypes() {
    return {
      grids: PropTypes.arrayOf(PropTypes.shape(Types.grid)).isRequired,
      onShowGridOutlineHighlight: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      height: 0,
      selectedGrid: null,
      showOutline: true,
      width: 0,
    };

    this.doHighlightCell = this.doHighlightCell.bind(this);
    this.getGridAreaName = this.getGridAreaName.bind(this);
    this.getHeight = this.getHeight.bind(this);
    this.getTotalWidthAndHeight = this.getTotalWidthAndHeight.bind(this);
    this.renderCannotShowOutlineText = this.renderCannotShowOutlineText.bind(this);
    this.renderGrid = this.renderGrid.bind(this);
    this.renderGridCell = this.renderGridCell.bind(this);
    this.renderGridOutline = this.renderGridOutline.bind(this);
    this.renderGridOutlineBorder = this.renderGridOutlineBorder.bind(this);
    this.renderOutline = this.renderOutline.bind(this);
    this.onHighlightCell = this.onHighlightCell.bind(this);
  }

  componentWillReceiveProps({ grids }) {
    let selectedGrid = grids.find(grid => grid.highlighted);

    // Store the height of the grid container in the component state to prevent overflow
    // issues. We want to store the width of the grid container as well so that the
    // viewbox is only the calculated width of the grid outline.
    let { width, height } = selectedGrid && selectedGrid.gridFragments.length
                            ? this.getTotalWidthAndHeight(selectedGrid)
                            : { width: 0, height: 0 };
    let showOutline;

    if (selectedGrid && selectedGrid.gridFragments.length) {
      const { cols, rows } = selectedGrid.gridFragments[0];

      // Show the grid outline if both the rows/columns are less than or equal
      // to their max prefs.
      showOutline = (cols.lines.length <= GRID_OUTLINE_MAX_COLUMNS_PREF) &&
                    (rows.lines.length <= GRID_OUTLINE_MAX_ROWS_PREF);
    }

    this.setState({ height, width, selectedGrid, showOutline });
  }

  doHighlightCell(target, hide) {
    const {
      grids,
      onShowGridOutlineHighlight,
    } = this.props;
    const name = target.dataset.gridAreaName;
    const id = target.dataset.gridId;
    const gridFragmentIndex = target.dataset.gridFragmentIndex;
    const rowNumber = target.dataset.gridRow;
    const columnNumber = target.dataset.gridColumn;

    onShowGridOutlineHighlight(grids[id].nodeFront);

    if (hide) {
      return;
    }

    onShowGridOutlineHighlight(grids[id].nodeFront, {
      showGridArea: name,
      showGridCell: {
        gridFragmentIndex,
        rowNumber,
        columnNumber,
      }
    });
  }

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
  }

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
  }

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

    // All writing modes other than horizontal-tb (the initial value) involve a 90 deg
    // rotation, so swap width and height.
    if (grid.writingMode != "horizontal-tb") {
      [ width, height ] = [ height, width ];
    }

    return { width, height };
  }

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
  }

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
    let x = 0;
    let y = 0;
    let width = 0;
    let height = 0;

    // Draw the cells contained within the grid outline border.
    for (let rowNumber = 1; rowNumber <= numberOfRows; rowNumber++) {
      height = GRID_CELL_SCALE_FACTOR * (rows.tracks[rowNumber - 1].breadth / 100);

      for (let columnNumber = 1; columnNumber <= numberOfColumns; columnNumber++) {
        width = GRID_CELL_SCALE_FACTOR * (cols.tracks[columnNumber - 1].breadth / 100);

        const gridAreaName = this.getGridAreaName(columnNumber, rowNumber, areas);
        const gridCell = this.renderGridCell(id, gridFragmentIndex, x, y,
                                             rowNumber, columnNumber, color, gridAreaName,
                                             width, height);

        rectangles.push(gridCell);
        x += width;
      }

      x = 0;
      y += height;
    }

    // Transform the cells as needed to match the grid container's writing mode.
    let cellGroupStyle = {};
    let writingModeMatrix = getWritingModeMatrix(this.state, grid);
    cellGroupStyle.transform = getCSSMatrixTransform(writingModeMatrix);
    let cellGroup = dom.g(
      {
        id: "grid-cell-group",
        style: cellGroupStyle,
      },
      rectangles
    );

    // Draw a rectangle that acts as the grid outline border.
    const border = this.renderGridOutlineBorder(this.state.width, this.state.height,
                                                color);

    return [
      border,
      cellGroup,
    ];
  }

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
        key: `${id}-${rowNumber}-${columnNumber}`,
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
        onMouseEnter: this.onHighlightCell,
        onMouseLeave: this.onHighlightCell,
      }
    );
  }

  renderGridOutline(grid) {
    let { color } = grid;

    return dom.g(
      {
        id: "grid-outline-group",
        className: "grid-outline-group",
        style: { color }
      },
      this.renderGrid(grid)
    );
  }

  renderGridOutlineBorder(borderWidth, borderHeight, color) {
    return dom.rect(
      {
        key: "border",
        className: "grid-outline-border",
        x: 0,
        y: 0,
        width: borderWidth,
        height: borderHeight
      }
    );
  }

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
  }

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
  }

  render() {
    const { selectedGrid } = this.state;

    return selectedGrid && selectedGrid.gridFragments.length ?
      dom.div(
        {
          id: "grid-outline-container",
          className: "grid-outline-container",
        },
        this.renderOutline()
      )
      :
      null;
  }
}

module.exports = GridOutline;
