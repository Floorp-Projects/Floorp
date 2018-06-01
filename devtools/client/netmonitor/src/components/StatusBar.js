/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { connect } = require("devtools/client/shared/redux/visibility-handler-connect");
const { PluralForm } = require("devtools/shared/plural-form");
const Actions = require("../actions/index");
const {
  getDisplayedRequestsSummary,
  getDisplayedTimingMarker,
} = require("../selectors/index");
const {
  getFormattedSize,
  getFormattedTime,
} = require("../utils/format-utils");
const { L10N } = require("../utils/l10n");

const { button, div } = dom;

const REQUESTS_COUNT_EMPTY = L10N.getStr("networkMenu.summary.requestsCountEmpty");
const TOOLTIP_PERF = L10N.getStr("networkMenu.summary.tooltip.perf");
const TOOLTIP_REQUESTS_COUNT = L10N.getStr("networkMenu.summary.tooltip.requestsCount");
const TOOLTIP_TRANSFERRED = L10N.getStr("networkMenu.summary.tooltip.transferred");
const TOOLTIP_FINISH = L10N.getStr("networkMenu.summary.tooltip.finish");
const TOOLTIP_DOM_CONTENT_LOADED =
        L10N.getStr("networkMenu.summary.tooltip.domContentLoaded");
const TOOLTIP_LOAD = L10N.getStr("networkMenu.summary.tooltip.load");

function StatusBar({ summary, openStatistics, timingMarkers }) {
  const { count, contentSize, transferredSize, millis } = summary;
  const {
    DOMContentLoaded,
    load,
  } = timingMarkers;

  const countText = count === 0 ? REQUESTS_COUNT_EMPTY :
    PluralForm.get(count,
      L10N.getStr("networkMenu.summary.requestsCount2")).replace("#1", count);
  const transferText = L10N.getFormatStrWithNumbers("networkMenu.summary.transferred",
    getFormattedSize(contentSize), getFormattedSize(transferredSize));
  const finishText = L10N.getFormatStrWithNumbers("networkMenu.summary.finish",
    getFormattedTime(millis));

  return (
    div({ className: "devtools-toolbar devtools-toolbar-bottom" },
      button({
        className: "devtools-button requests-list-network-summary-button",
        title: TOOLTIP_PERF,
        onClick: openStatistics,
      },
        div({ className: "summary-info-icon" }),
      ),
      div({
        className: "status-bar-label requests-list-network-summary-count",
        title: TOOLTIP_REQUESTS_COUNT,
      }, countText),
      count !== 0 &&
        div({
          className: "status-bar-label requests-list-network-summary-transfer",
          title: TOOLTIP_TRANSFERRED,
        }, transferText),
      count !== 0 &&
        div({
          className: "status-bar-label requests-list-network-summary-finish",
          title: TOOLTIP_FINISH,
        }, finishText),
      DOMContentLoaded > -1 &&
        div({
          className: "status-bar-label dom-content-loaded",
          title: TOOLTIP_DOM_CONTENT_LOADED,
        }, `DOMContentLoaded: ${getFormattedTime(DOMContentLoaded)}`),
      load > -1 &&
        div({
          className: "status-bar-label load",
          title: TOOLTIP_LOAD,
        }, `load: ${getFormattedTime(load)}`),
    )
  );
}

StatusBar.displayName = "StatusBar";

StatusBar.propTypes = {
  connector: PropTypes.object.isRequired,
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
  (dispatch, props) => ({
    openStatistics: () => dispatch(Actions.openStatistics(props.connector, true)),
  }),
)(StatusBar);
