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

  let text = (count === 0) ? L10N.getStr("networkMenu.empty") :
    PluralForm.get(count, L10N.getStr("networkMenu.summary3"))
    .replace("#1", count)
    .replace("#2", getFormattedSize(contentSize))
    .replace("#3", getFormattedSize(transferredSize))
    .replace("#4", getFormattedTime(millis));

  return (
    div({ className: "devtools-toolbar devtools-toolbar-bottom" },
      button({
        className: "devtools-button requests-list-network-summary-button",
        title: count ? text : L10N.getStr("netmonitor.toolbar.perf"),
        onClick: openStatistics,
      },
        span({ className: "summary-info-icon" }),
        span({ className: "summary-info-text" }, text),
      ),

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
