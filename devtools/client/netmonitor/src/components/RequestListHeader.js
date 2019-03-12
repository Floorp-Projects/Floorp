/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { createRef, Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/redux/visibility-handler-connect");
const {
  getTheme,
  addThemeObserver,
  removeThemeObserver,
} = require("devtools/client/shared/theme");
const Actions = require("../actions/index");
const {
  HEADERS,
  REQUESTS_WATERFALL,
  MIN_COLUMN_WIDTH,
  DEFAULT_COLUMN_WIDTH,
} = require("../constants");
const { getWaterfallScale } = require("../selectors/index");
const { getFormattedTime } = require("../utils/format-utils");
const { L10N } = require("../utils/l10n");
const RequestListHeaderContextMenu = require("../widgets/RequestListHeaderContextMenu");
const WaterfallBackground = require("../widgets/WaterfallBackground");
const Draggable = createFactory(require("devtools/client/shared/components/splitter/Draggable"));

const { div, button } = dom;

// Support for columns resizing is currently hidden behind this pref.
const RESIZE_COLUMNS =
  Services.prefs.getBoolPref("devtools.netmonitor.features.resizeColumns");

/**
 * Render the request list header with sorting arrows for columns.
 * Displays tick marks in the waterfall column header.
 * Also draws the waterfall background canvas and updates it when needed.
 */
class RequestListHeader extends Component {
  static get propTypes() {
    return {
      columns: PropTypes.object.isRequired,
      resetColumns: PropTypes.func.isRequired,
      resetSorting: PropTypes.func.isRequired,
      resizeWaterfall: PropTypes.func.isRequired,
      scale: PropTypes.number,
      sort: PropTypes.object,
      sortBy: PropTypes.func.isRequired,
      toggleColumn: PropTypes.func.isRequired,
      waterfallWidth: PropTypes.number,
      columnsData: PropTypes.object.isRequired,
      setColumnsWidth: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.requestListHeader = createRef();

    this.onContextMenu = this.onContextMenu.bind(this);
    this.drawBackground = this.drawBackground.bind(this);
    this.resizeWaterfall = this.resizeWaterfall.bind(this);
    this.waterfallDivisionLabels = this.waterfallDivisionLabels.bind(this);
    this.waterfallLabel = this.waterfallLabel.bind(this);
    this.onHeaderClick = this.onHeaderClick.bind(this);
  }

  componentWillMount() {
    const { resetColumns, resetSorting, toggleColumn } = this.props;
    this.contextMenu = new RequestListHeaderContextMenu({
      resetColumns,
      resetSorting,
      toggleColumn,
    });
  }

  componentDidMount() {
    // Create the object that takes care of drawing the waterfall canvas background
    this.background = new WaterfallBackground(document);
    this.drawBackground();
    // When visible columns add up to less or more than 100% => update widths in prefs.
    if (this.shouldUpdateWidths()) {
      this.updateColumnsWidth();
    }
    this.resizeWaterfall();
    window.addEventListener("resize", this.resizeWaterfall);
    addThemeObserver(this.drawBackground);
  }

  componentDidUpdate() {
    this.drawBackground();
    // check if the widths in prefs need to be updated
    // e.g. after hide/show column
    if (this.shouldUpdateWidths()) {
      this.updateColumnsWidth();
      this.resizeWaterfall();
    }
  }

  componentWillUnmount() {
    this.background.destroy();
    this.background = null;
    window.removeEventListener("resize", this.resizeWaterfall);
    removeThemeObserver(this.drawBackground);
  }

  onContextMenu(evt) {
    evt.preventDefault();
    this.contextMenu.open(evt, this.props.columns);
  }

  onHeaderClick(evt, headerName) {
    const { sortBy, resetSorting } = this.props;
    if (evt.button == 1) {
      // reset sort state on middle click
      resetSorting();
    } else {
      sortBy(headerName);
    }
  }

  drawBackground() {
    // The background component is theme dependent, so add the current theme to the props.
    const props = Object.assign({}, this.props, {
      theme: getTheme(),
    });
    this.background.draw(props);
  }

  resizeWaterfall() {
    const waterfallHeader = this.refs.waterfallHeader;
    if (waterfallHeader) {
      // Measure its width and update the 'waterfallWidth' property in the store.
      // The 'waterfallWidth' will be further updated on every window resize.
      window.cancelIdleCallback(this._resizeTimerId);
      this._resizeTimerId = window.requestIdleCallback(() =>
        this.props.resizeWaterfall(waterfallHeader.getBoundingClientRect().width));
    }
  }

  /**
   * Build the waterfall header - timing tick marks with the right spacing
   */
  waterfallDivisionLabels(waterfallWidth, scale) {
    const labels = [];

    // Build new millisecond tick labels...
    const timingStep = REQUESTS_WATERFALL.HEADER_TICKS_MULTIPLE;
    let scaledStep = scale * timingStep;

    // Ignore any divisions that would end up being too close to each other.
    while (scaledStep < REQUESTS_WATERFALL.HEADER_TICKS_SPACING_MIN) {
      scaledStep *= 2;
    }

    // Insert one label for each division on the current scale.
    for (let x = 0; x < waterfallWidth; x += scaledStep) {
      const millisecondTime = x / scale;
      let divisionScale = "millisecond";

      // If the division is greater than 1 minute.
      if (millisecondTime > 60000) {
        divisionScale = "minute";
      } else if (millisecondTime > 1000) {
        // If the division is greater than 1 second.
        divisionScale = "second";
      }

      let width = (x + scaledStep | 0) - (x | 0);
      // Adjust the first marker for the borders
      if (x == 0) {
        width -= 2;
      }
      // Last marker doesn't need a width specified at all
      if (x + scaledStep >= waterfallWidth) {
        width = undefined;
      }

      labels.push(div(
        {
          key: labels.length,
          className: "requests-list-timings-division",
          "data-division-scale": divisionScale,
          style: { width },
        },
        getFormattedTime(millisecondTime)
      ));
    }

    return labels;
  }

  waterfallLabel(waterfallWidth, scale, label) {
    let className = "button-text requests-list-waterfall-label-wrapper";

    if (waterfallWidth !== null && scale !== null) {
      label = this.waterfallDivisionLabels(waterfallWidth, scale);
      className += " requests-list-waterfall-visible";
    }

    return div({ className }, label);
  }

  // Dragging Events

  /**
   * Set 'resizing' cursor on entire container dragging.
   * This avoids cursor-flickering when the mouse leaves
   * the column-resizer area (happens frequently).
   */
  onStartMove() {
    // Set cursor to dragging
    const container = document.querySelector(".request-list-container");
    container.style.cursor = "ew-resize";
    // Class .dragging is used to disable pointer events while dragging - see css.
    this.requestListHeader.classList.add("dragging");
  }

  /**
   * A handler that calculates the new width of the columns
   * based on mouse position and adjusts the width.
   */
  onMove(name, x) {
    const parentEl = document.querySelector(".requests-list-headers");
    const parentWidth = parentEl.getBoundingClientRect().width;

    // Get the current column handle and save its old width
    // before changing so we can compute the adjustment in width
    const headerRef = this.refs[`${name}Header`];
    const headerRefRect = headerRef.getBoundingClientRect();
    const oldWidth = headerRefRect.width;

    // Get the column handle that will compensate the width change.
    const compensateHeaderName = this.getCompensateHeader();

    if (name === compensateHeaderName) {
      // this is the case where we are resizing waterfall
      this.moveWaterfall(x, parentWidth);
      return;
    }

    const compensateHeaderRef = this.refs[`${compensateHeaderName}Header`];
    const compensateHeaderRefRect = compensateHeaderRef.getBoundingClientRect();
    const oldCompensateWidth = compensateHeaderRefRect.width;
    const sumOfBothColumns = oldWidth + oldCompensateWidth;

    // Get minimal widths for both changed columns (in px).
    const minWidth = this.getMinWidth(name);
    const minCompensateWidth = this.getMinWidth(compensateHeaderName);

    // Calculate new width (according to the mouse x-position) and set to style.
    // Do not allow to set it below minWidth.
    const newWidth = Math.max(x - headerRefRect.left, minWidth);
    headerRef.style.width = `${this.px2percent(newWidth, parentWidth)}%`;
    const adjustment = oldWidth - newWidth;

    // Calculate new compensate width as the original width + adjustment.
    // Do not allow to set it below minCompensateWidth.
    const newCompensateWidth =
      Math.max(adjustment + oldCompensateWidth, minCompensateWidth);
    compensateHeaderRef.style.width =
      `${this.px2percent(newCompensateWidth, parentWidth)}%`;

    // Do not allow to reset size of column when compensate column is at minWidth.
    if (newCompensateWidth === minCompensateWidth) {
      headerRef.style.width =
        `${this.px2percent((sumOfBothColumns - newCompensateWidth), parentWidth)}%`;
    }
  }

  /**
   * After resizing - we get the width for each 'column'
   * and convert it into % and store it in user prefs.
   * Also resets the 'resizing' cursor back to initial.
   */
  onStopMove() {
    this.updateColumnsWidth();
    // If waterfall is visible and width has changed, call resizeWaterfall.
    const waterfallRef = this.refs.waterfallHeader;
    if (waterfallRef) {
      const { waterfallWidth } = this.props;
      const realWaterfallWidth = waterfallRef.getBoundingClientRect().width;
      if (Math.round(waterfallWidth) !== Math.round(realWaterfallWidth)) {
        this.resizeWaterfall();
      }
    }

    // Restore cursor back to default.
    const container = document.querySelector(".request-list-container");
    container.style.cursor = "initial";
    this.requestListHeader.classList.remove("dragging");
  }

  /**
   * Helper method to get the name of the column that will compensate
   * the width change. It should be the last column before waterfall,
   * (if waterfall visible) otherwise it is simply the last visible column.
   */
  getCompensateHeader() {
    const visibleColumns = this.getVisibleColumns();
    const lastColumn = visibleColumns[visibleColumns.length - 1].name;
    const delta = (lastColumn === "waterfall") ? 2 : 1;
    return visibleColumns[visibleColumns.length - delta].name;
  }

  /**
   * Called from onMove() when resizing waterfall column
   * because waterfall is a special case, where ALL other
   * columns are made smaller when waterfall is bigger and vice versa.
   */
  moveWaterfall(x, parentWidth) {
    const visibleColumns = this.getVisibleColumns();
    const minWaterfall = this.getMinWidth("waterfall");
    const waterfallRef = this.refs.waterfallHeader;

    // Compute and set style.width for waterfall.
    const waterfallRefRect = waterfallRef.getBoundingClientRect();
    const oldWidth = waterfallRefRect.width;
    const adjustment = waterfallRefRect.left - x;
    if (this.allColumnsAtMinWidth() && adjustment > 0) {
      // When we want to make waterfall wider but all
      // other columns are already at minWidth => return.
      return;
    }

    const newWidth = Math.max(oldWidth + adjustment, minWaterfall);

    // Now distribute evenly the change in width to all other columns except waterfall.
    const changeInWidth = oldWidth - newWidth;
    const widths = this.autoSizeWidths(changeInWidth, visibleColumns);

    // Set the new computed width for waterfall into array widths.
    widths[widths.length - 1] = newWidth;

    // Update style for all columns from array widths.
    let i = 0;
    visibleColumns.forEach(col => {
      const name = col.name;
      const headerRef = this.refs[`${name}Header`];
      headerRef.style.width = `${this.px2percent(widths[i], parentWidth)}%`;
      i++;
    });
  }

  /**
   * Helper method that checks if all columns have reached their minWidth.
   * This can happen when making waterfall column wider.
   */
  allColumnsAtMinWidth() {
    const visibleColumns = this.getVisibleColumns();
    // Do not check width for waterfall because
    // when all are getting smaller, waterfall is getting bigger.
    for (let i = 0; i < visibleColumns.length - 1; i++) {
      const name = visibleColumns[i].name;
      const headerRef = this.refs[`${name}Header`];
      const minColWidth = this.getMinWidth(name);
      if (headerRef.getBoundingClientRect().width > minColWidth) {
        return false;
      }
    }
    return true;
  }

  /**
   * Method takes the total change in width for waterfall column
   * and distributes it among all other columns. Returns an array
   * where all visible columns have newly computed width in pixels.
   */
  autoSizeWidths(changeInWidth, visibleColumns) {
    const widths = visibleColumns.map(col => {
      const headerRef = this.refs[`${col.name}Header`];
      const colWidth = headerRef.getBoundingClientRect().width;
      return colWidth;
    });

    // Divide changeInWidth among all columns but waterfall (that's why -1).
    const changeInWidthPerColumn = changeInWidth / (widths.length - 1);

    while (changeInWidth) {
      const lastChangeInWidth = changeInWidth;
      // In the loop adjust all columns except last one - waterfall
      for (let i = 0; i < widths.length - 1; i++) {
        const name = visibleColumns[i].name;
        const minColWidth = this.getMinWidth(name);
        const newColWidth = Math.max(widths[i] + changeInWidthPerColumn, minColWidth);

        widths[i] = newColWidth;
        if (changeInWidth > 0) {
          changeInWidth -= (newColWidth - widths[i]);
        } else {
          changeInWidth += (newColWidth - widths[i]);
        }
        if (!changeInWidth) {
          break;
        }
      }
      if (lastChangeInWidth == changeInWidth) {
        break;
      }
    }
    return widths;
  }

  /**
   * Method returns 'true' - if the column widths need to be updated
   * when the total % is less or more than 100%.
   * It returns 'false' if they add up to 100% => no need to update.
   */
  shouldUpdateWidths() {
    const visibleColumns = this.getVisibleColumns();
    let totalPercent = 0;

    visibleColumns.forEach(col => {
      const name = col.name;
      const headerRef = this.refs[`${name}Header`];
      // Get column width from style.
      let widthFromStyle = 0;
      // In case the column is in visibleColumns but has display:none
      // we don't want to count its style.width into totalPercent.
      if (headerRef.getBoundingClientRect().width > 0) {
        widthFromStyle = headerRef.style.width.slice(0, -1);
      }
      totalPercent += +widthFromStyle; // + converts it to a number
    });

    // Do not update if total percent is from 99-101% or when it is 0
    // - it means that no columns are displayed (e.g. other panel is currently selected).
    return Math.round(totalPercent) !== 100 && totalPercent !== 0;
  }

  /**
   * Method reads real width of each column header
   * and updates the style.width for that header.
   * It returns updated columnsData.
   */
  updateColumnsWidth() {
    const visibleColumns = this.getVisibleColumns();
    const parentEl = document.querySelector(".requests-list-headers");
    const parentElRect = parentEl.getBoundingClientRect();
    const parentWidth = parentElRect.width;
    const newWidths = [];
    visibleColumns.forEach(col => {
      const name = col.name;
      const headerRef = this.refs[`${name}Header`];
      const headerWidth = headerRef.getBoundingClientRect().width;

      // Get actual column width, change into %, update style
      const width = this.px2percent(headerWidth, parentWidth);

      if (width > 0) {
        // This prevents saving width 0 for waterfall when it is not showing for
        // @media (max-width: 700px)
        newWidths.push({name, width});
      }
    });
    this.props.setColumnsWidth(newWidths);
  }

  /**
   * Helper method to convert pixels into percent based on parent container width
   */
  px2percent(pxWidth, parentWidth) {
    const percent = Math.round((100 * pxWidth / parentWidth) * 100) / 100;
    return percent;
  }

  /**
   * Helper method to get visibleColumns;
   */
  getVisibleColumns() {
    const { columns } = this.props;
    return HEADERS.filter((header) => columns[header.name]);
  }

  /**
   * Helper method to get minWidth from columnsData;
   */
  getMinWidth(colName) {
    const columnsData = this.props.columnsData;
    if (columnsData.has(colName)) {
      return columnsData.get(colName).minWidth;
    }
    return MIN_COLUMN_WIDTH;
  }

  /**
   * Render one column header from the table headers.
   */
  renderColumn(header) {
    const columnsData = this.props.columnsData;
    const visibleColumns = this.getVisibleColumns();
    const lastVisibleColumn = visibleColumns[visibleColumns.length - 1].name;
    const name = header.name;
    const boxName = header.boxName || name;
    const label = header.noLocalization
      ? name : L10N.getStr(`netmonitor.toolbar.${header.label || name}`);

    const { scale, sort, waterfallWidth } = this.props;
    let sorted, sortedTitle;
    const active = sort.type == name ? true : undefined;

    if (active) {
      sorted = sort.ascending ? "ascending" : "descending";
      sortedTitle = L10N.getStr(sort.ascending
        ? "networkMenu.sortedAsc"
        : "networkMenu.sortedDesc");
    }

    // If the pref for this column width exists, set the style
    // otherwise use default.
    let colWidth = DEFAULT_COLUMN_WIDTH;
    if (columnsData.has(name)) {
      const oneColumnEl = columnsData.get(name);
      colWidth = oneColumnEl.width;
    }
    const columnStyle = {
      width: colWidth + "%",
    };

    // Support for columns resizing is currently hidden behind a pref.
    const draggable = RESIZE_COLUMNS ? Draggable({
      className: "column-resizer ",
      onStart: () => this.onStartMove(),
      onStop: () => this.onStopMove(),
      onMove: (x) => this.onMove(name, x),
    }) : undefined;

    return (
      dom.td({
        id: `requests-list-${boxName}-header-box`,
        className: `requests-list-column requests-list-${boxName}`,
        style: columnStyle,
        key: name,
        ref: `${name}Header`,
        // Used to style the next column.
        "data-active": active,
      },
        button({
          id: `requests-list-${name}-button`,
          className: `requests-list-header-button`,
          "data-sorted": sorted,
          title: sortedTitle ? `${label} (${sortedTitle})` : label,
          onClick: (evt) => this.onHeaderClick(evt, name),
        },
          name === "waterfall"
            ? this.waterfallLabel(waterfallWidth, scale, label)
            : div({ className: "button-text" }, label),
          div({ className: "button-icon" })
        ),
        (name !== lastVisibleColumn) && draggable
      )
    );
  }

  /**
   * Render all columns in the table header
   */
  renderColumns() {
    const visibleColumns = this.getVisibleColumns();
    return visibleColumns.map(header => this.renderColumn(header));
  }

  render() {
    return (
      dom.thead({ className: "devtools-toolbar requests-list-headers-group" },
        dom.tr({
          className: "requests-list-headers",
          onContextMenu: this.onContextMenu,
          ref: node => {
            this.requestListHeader = node;
          },
        },
          this.renderColumns(),
        )
      )
    );
  }
}

module.exports = connect(
  (state) => ({
    columns: state.ui.columns,
    columnsData: state.ui.columnsData,
    firstRequestStartedMillis: state.requests.firstStartedMillis,
    scale: getWaterfallScale(state),
    sort: state.sort,
    timingMarkers: state.timingMarkers,
    waterfallWidth: state.ui.waterfallWidth,
  }),
  (dispatch) => ({
    resetColumns: () => dispatch(Actions.resetColumns()),
    resetSorting: () => dispatch(Actions.sortBy(null)),
    resizeWaterfall: (width) => dispatch(Actions.resizeWaterfall(width)),
    sortBy: (type) => dispatch(Actions.sortBy(type)),
    toggleColumn: (column) => dispatch(Actions.toggleColumn(column)),
    setColumnsWidth: (widths) => dispatch(Actions.setColumnsWidth(widths)),
  })
)(RequestListHeader);
