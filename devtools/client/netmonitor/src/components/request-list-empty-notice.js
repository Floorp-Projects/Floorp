/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createClass,
  createFactory,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const Actions = require("../actions/index");
const { triggerActivity } = require("../connector/index");
const { ACTIVITY_TYPE } = require("../constants");
const { L10N } = require("../utils/l10n");
const { getPerformanceAnalysisURL } = require("../utils/mdn-utils");

// Components
const MDNLink = createFactory(require("./mdn-link"));

const { button, div, span } = DOM;

/**
 * UI displayed when the request list is empty. Contains instructions on reloading
 * the page and on triggering performance analysis of the page.
 */
const RequestListEmptyNotice = createClass({
  displayName: "RequestListEmptyNotice",

  propTypes: {
    onReloadClick: PropTypes.func.isRequired,
    onPerfClick: PropTypes.func.isRequired,
  },

  render() {
    return div(
      {
        className: "request-list-empty-notice",
      },
      div({ className: "notice-reload-message" },
        span(null, L10N.getStr("netmonitor.reloadNotice1")),
        button(
          {
            className: "devtools-button requests-list-reload-notice-button",
            "data-standalone": true,
            onClick: this.props.onReloadClick,
          },
          L10N.getStr("netmonitor.reloadNotice2")
        ),
        span(null, L10N.getStr("netmonitor.reloadNotice3"))
      ),
      div({ className: "notice-perf-message" },
        span(null, L10N.getStr("netmonitor.perfNotice1")),
        button({
          title: L10N.getStr("netmonitor.perfNotice3"),
          className: "devtools-button requests-list-perf-notice-button",
          "data-standalone": true,
          onClick: this.props.onPerfClick,
        }),
        span(null, L10N.getStr("netmonitor.perfNotice2")),
        MDNLink({ url: getPerformanceAnalysisURL() })
      )
    );
  }
});

module.exports = connect(
  undefined,
  dispatch => ({
    onPerfClick: () => dispatch(Actions.openStatistics(true)),
    onReloadClick: () => triggerActivity(ACTIVITY_TYPE.RELOAD.WITH_CACHE_DEFAULT),
  })
)(RequestListEmptyNotice);
