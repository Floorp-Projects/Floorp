/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { PluralForm } = require("devtools/shared/plural-form");

const Actions = require("../actions/index");
const {
  getDisplayedRequestsSummary,
  getDisplayedTimingMarker,
} = require("../selectors/index");
const {
  getFormattedSize,
  getFormattedTime
} = require("../utils/format-utils");
const { L10N } = require("../utils/l10n");

// Components
const { div, button, span } = DOM;

function StatusBar({ summary, openStatistics, timingMarkers }) {
  let { count, contentSize, transferredSize, millis } = summary;
  let {
    DOMContentLoaded,
    load,
  } = timingMarkers;

  let countText = count === 0 ? L10N.getStr("networkMenu.summary.requestsCountEmpty") :
    PluralForm.get(
      count, L10N.getFormatStrWithNumbers("networkMenu.summary.requestsCount", count)
  );
  let transferText = L10N.getFormatStrWithNumbers("networkMenu.summary.transferred",
    getFormattedSize(contentSize), getFormattedSize(transferredSize));
  let finishText = L10N.getFormatStrWithNumbers("networkMenu.summary.finish",
    getFormattedTime(millis));

  return (
    div({ className: "devtools-toolbar devtools-toolbar-bottom" },
      button({
        className: "devtools-button requests-list-network-summary-button",
        onClick: openStatistics,
      },
        span({ className: "summary-info-icon" }),
      ),
      span({ className: "status-bar-label requests-list-network-summary-count" },
        countText),
      count !== 0 &&
        span({ className: "status-bar-label requests-list-network-summary-transfer" },
          transferText),
      count !== 0 &&
        span({ className: "status-bar-label requests-list-network-summary-finish" },
          finishText),

      DOMContentLoaded > -1 &&
      span({ className: "status-bar-label dom-content-loaded" },
        `DOMContentLoaded: ${getFormattedTime(DOMContentLoaded)}`),

      load > -1 &&
      span({ className: "status-bar-label load" },
        `load: ${getFormattedTime(load)}`),
    )
  );
}

StatusBar.displayName = "StatusBar";

StatusBar.propTypes = {
  openStatistics: PropTypes.func.isRequired,
  summary: PropTypes.object.isRequired,
  timingMarkers: PropTypes.object.isRequired,
};

module.exports = connect(
  (state) => ({
    summary: getDisplayedRequestsSummary(state),
    timingMarkers: {
      DOMContentLoaded:
        getDisplayedTimingMarker(state, "firstDocumentDOMContentLoadedTimestamp"),
      load: getDisplayedTimingMarker(state, "firstDocumentLoadTimestamp"),
    },
  }),
  (dispatch) => ({
    openStatistics: () => dispatch(Actions.openStatistics(true)),
  }),
)(StatusBar);
