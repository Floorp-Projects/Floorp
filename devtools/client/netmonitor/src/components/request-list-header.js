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
const { getWaterfallScale } = require("../selectors/index");
const { getFormattedTime } = require("../utils/format-utils");
const { HEADERS } = require("../constants");
const { L10N } = require("../utils/l10n");
const WaterfallBackground = require("../waterfall-background");
const RequestListHeaderContextMenu = require("../request-list-header-context-menu");

const { div, button } = DOM;

const REQUESTS_WATERFALL_HEADER_TICKS_MULTIPLE = 5; // ms
const REQUESTS_WATERFALL_HEADER_TICKS_SPACING_MIN = 60; // px

/**
 * Render the request list header with sorting arrows for columns.
 * Displays tick marks in the waterfall column header.
 * Also draws the waterfall background canvas and updates it when needed.
 */
const RequestListHeader = createClass({
  displayName: "RequestListHeader",

  propTypes: {
    columns: PropTypes.object.isRequired,
    dispatch: PropTypes.func.isRequired,
    sort: PropTypes.object,
    scale: PropTypes.number,
    waterfallWidth: PropTypes.number,
    onHeaderClick: PropTypes.func.isRequired,
    resizeWaterfall: PropTypes.func.isRequired,
  },

  componentWillMount() {
    const { dispatch } = this.props;
    this.contextMenu = new RequestListHeaderContextMenu({
      toggleColumn: (column) => dispatch(Actions.toggleColumn(column)),
      resetColumns: () => dispatch(Actions.resetColumns()),
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
    // Measure its width and update the 'waterfallWidth' property in the store.
    // The 'waterfallWidth' will be further updated on every window resize.
    setTimeout(() => {
      let { width } = this.refs.header.getBoundingClientRect();
      this.props.resizeWaterfall(width);
    }, 50);
  },

  render() {
    const { sort, scale, waterfallWidth, onHeaderClick, columns } = this.props;

    return div(
      { className: "devtools-toolbar requests-list-toolbar" },
      div({ className: "toolbar-labels" },
        HEADERS.filter(h => columns.get(h.name)).map(header => {
          const name = header.name;
          const boxName = header.boxName || name;
          const label = L10N.getStr(`netmonitor.toolbar.${header.label || name}`);

          let sorted, sortedTitle;
          const active = sort.type == name ? true : undefined;
          if (active) {
            sorted = sort.ascending ? "ascending" : "descending";
            sortedTitle = L10N.getStr(sort.ascending
              ? "networkMenu.sortedAsc"
              : "networkMenu.sortedDesc");
          }

          return div(
            {
              id: `requests-list-${boxName}-header-box`,
              className: `requests-list-header requests-list-${boxName}`,
              key: name,
              ref: "header",
              // Used to style the next column.
              "data-active": active,
              onContextMenu: this.onContextMenu,
            },
            button(
              {
                id: `requests-list-${name}-button`,
                className: `requests-list-header-button requests-list-${name}`,
                "data-sorted": sorted,
                title: sortedTitle,
                onClick: () => onHeaderClick(name),
              },
              name == "waterfall" ? WaterfallLabel(waterfallWidth, scale, label)
                                  : div({ className: "button-text" }, label),
              div({ className: "button-icon" })
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
  let timingStep = REQUESTS_WATERFALL_HEADER_TICKS_MULTIPLE;
  let scaledStep = scale * timingStep;

  // Ignore any divisions that would end up being too close to each other.
  while (scaledStep < REQUESTS_WATERFALL_HEADER_TICKS_SPACING_MIN) {
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

  if (waterfallWidth != null && scale != null) {
    label = waterfallDivisionLabels(waterfallWidth, scale);
    className += " requests-list-waterfall-visible";
  }

  return div({ className }, label);
}

module.exports = connect(
  state => ({
    columns: state.ui.columns,
    sort: state.sort,
    scale: getWaterfallScale(state),
    waterfallWidth: state.ui.waterfallWidth,
    firstRequestStartedMillis: state.requests.firstStartedMillis,
    timingMarkers: state.timingMarkers,
  }),
  dispatch => ({
    dispatch,
    onHeaderClick: type => dispatch(Actions.sortBy(type)),
    resizeWaterfall: width => dispatch(Actions.resizeWaterfall(width)),
  })
)(RequestListHeader);
