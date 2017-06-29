/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createClass,
  PropTypes,
  DOM,
} = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const Actions = require("../actions/index");
const { HEADERS, REQUESTS_WATERFALL } = require("../constants");
const { getWaterfallScale } = require("../selectors/index");
const { getFormattedTime } = require("../utils/format-utils");
const { L10N } = require("../utils/l10n");
const WaterfallBackground = require("../waterfall-background");
const RequestListHeaderContextMenu = require("../request-list-header-context-menu");

const { div, button } = DOM;

/**
 * Render the request list header with sorting arrows for columns.
 * Displays tick marks in the waterfall column header.
 * Also draws the waterfall background canvas and updates it when needed.
 */
const RequestListHeader = createClass({
  displayName: "RequestListHeader",

  propTypes: {
    columns: PropTypes.object.isRequired,
    resetColumns: PropTypes.func.isRequired,
    resizeWaterfall: PropTypes.func.isRequired,
    scale: PropTypes.number,
    sort: PropTypes.object,
    sortBy: PropTypes.func.isRequired,
    toggleColumn: PropTypes.func.isRequired,
    waterfallWidth: PropTypes.number,
  },

  componentWillMount() {
    const { resetColumns, toggleColumn } = this.props;
    this.contextMenu = new RequestListHeaderContextMenu({
      resetColumns,
      toggleColumn,
    });
  },

  componentDidMount() {
    // Create the object that takes care of drawing the waterfall canvas background
    this.background = new WaterfallBackground(document);
    this.background.draw(this.props);
    this.resizeWaterfall();
    window.addEventListener("resize", this.resizeWaterfall);
  },

  componentDidUpdate() {
    this.background.draw(this.props);
  },

  componentWillUnmount() {
    this.background.destroy();
    this.background = null;
    window.removeEventListener("resize", this.resizeWaterfall);
  },

  onContextMenu(evt) {
    evt.preventDefault();
    this.contextMenu.open(evt);
  },

  resizeWaterfall() {
    let waterfallHeader = this.refs.waterfallHeader;
    if (waterfallHeader) {
      // Measure its width and update the 'waterfallWidth' property in the store.
      // The 'waterfallWidth' will be further updated on every window resize.
      setTimeout(() => {
        this.props.resizeWaterfall(waterfallHeader.getBoundingClientRect().width);
      }, 500);
    }
  },

  render() {
    let { columns, scale, sort, sortBy, waterfallWidth } = this.props;

    return (
      div({ className: "devtools-toolbar requests-list-headers" },
        HEADERS.filter((header) => columns.get(header.name)).map((header) => {
          let name = header.name;
          let boxName = header.boxName || name;
          let label = header.noLocalization
            ? name : L10N.getStr(`netmonitor.toolbar.${header.label || name}`);
          let sorted, sortedTitle;
          let active = sort.type == name ? true : undefined;

          if (active) {
            sorted = sort.ascending ? "ascending" : "descending";
            sortedTitle = L10N.getStr(sort.ascending
              ? "networkMenu.sortedAsc"
              : "networkMenu.sortedDesc");
          }

          return (
            div({
              id: `requests-list-${boxName}-header-box`,
              className: `requests-list-column requests-list-${boxName}`,
              key: name,
              ref: `${name}Header`,
              // Used to style the next column.
              "data-active": active,
              onContextMenu: this.onContextMenu,
            },
              button({
                id: `requests-list-${name}-button`,
                className: `requests-list-header-button`,
                "data-sorted": sorted,
                title: sortedTitle,
                onClick: () => sortBy(name),
              },
                name === "waterfall"
                  ? WaterfallLabel(waterfallWidth, scale, label)
                  : div({ className: "button-text" }, label),
                div({ className: "button-icon" })
              )
            )
          );
        })
      )
    );
  }
});

/**
 * Build the waterfall header - timing tick marks with the right spacing
 */
function waterfallDivisionLabels(waterfallWidth, scale) {
  let labels = [];

  // Build new millisecond tick labels...
  let timingStep = REQUESTS_WATERFALL.HEADER_TICKS_MULTIPLE;
  let scaledStep = scale * timingStep;

  // Ignore any divisions that would end up being too close to each other.
  while (scaledStep < REQUESTS_WATERFALL.HEADER_TICKS_SPACING_MIN) {
    scaledStep *= 2;
  }

  // Insert one label for each division on the current scale.
  for (let x = 0; x < waterfallWidth; x += scaledStep) {
    let millisecondTime = x / scale;
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
        style: { width }
      },
      getFormattedTime(millisecondTime)
    ));
  }

  return labels;
}

function WaterfallLabel(waterfallWidth, scale, label) {
  let className = "button-text requests-list-waterfall-label-wrapper";

  if (waterfallWidth !== null && scale !== null) {
    label = waterfallDivisionLabels(waterfallWidth, scale);
    className += " requests-list-waterfall-visible";
  }

  return div({ className }, label);
}

module.exports = connect(
  (state) => ({
    columns: state.ui.columns,
    firstRequestStartedMillis: state.requests.firstStartedMillis,
    scale: getWaterfallScale(state),
    sort: state.sort,
    timingMarkers: state.timingMarkers,
    waterfallWidth: state.ui.waterfallWidth,
  }),
  (dispatch) => ({
    resetColumns: () => dispatch(Actions.resetColumns()),
    resizeWaterfall: (width) => dispatch(Actions.resizeWaterfall(width)),
    sortBy: (type) => dispatch(Actions.sortBy(type)),
    toggleColumn: (column) => dispatch(Actions.toggleColumn(column)),
  })
)(RequestListHeader);
