/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  CONTENT_SIZE_DECIMALS,
  REQUEST_TIME_DECIMALS,
} = require("../constants");
const { DOM, PropTypes } = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { PluralForm } = require("devtools/shared/plural-form");
const { L10N } = require("../l10n");
const { getDisplayedRequestsSummary } = require("../selectors/index");
const Actions = require("../actions/index");

const { button, span } = DOM;

function SummaryButton({
  summary,
  triggerSummary,
}) {
  let { count, bytes, millis } = summary;
  const text = (count === 0) ? L10N.getStr("networkMenu.empty") :
    PluralForm.get(count, L10N.getStr("networkMenu.summary"))
    .replace("#1", count)
    .replace("#2", L10N.numberWithDecimals(bytes / 1024, CONTENT_SIZE_DECIMALS))
    .replace("#3", L10N.numberWithDecimals(millis / 1000, REQUEST_TIME_DECIMALS));

  return button({
    id: "requests-menu-network-summary-button",
    className: "devtools-button",
    title: count ? text : L10N.getStr("netmonitor.toolbar.perf"),
    onClick: triggerSummary,
  },
  span({ className: "summary-info-icon" }),
  span({ className: "summary-info-text" }, text));
}

SummaryButton.propTypes = {
  summary: PropTypes.object.isRequired,
};

module.exports = connect(
  (state) => ({
    summary: getDisplayedRequestsSummary(state),
  }),
  (dispatch) => ({
    triggerSummary: () => {
      dispatch(Actions.openStatistics(true));
    },
  })
)(SummaryButton);
