/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/redux/visibility-handler-connect");
const Actions = require("../actions/index");
const { ACTIVITY_TYPE } = require("../constants");
const { L10N } = require("../utils/l10n");
const { getPerformanceAnalysisURL } = require("../utils/mdn-utils");

// Components
const MDNLink = createFactory(require("devtools/client/shared/components/MdnLink"));
const RequestListHeader = createFactory(require("./RequestListHeader"));

const { button, div, span } = dom;

const RELOAD_NOTICE_1 = L10N.getStr("netmonitor.reloadNotice1");
const RELOAD_NOTICE_2 = L10N.getStr("netmonitor.reloadNotice2");
const RELOAD_NOTICE_3 = L10N.getStr("netmonitor.reloadNotice3");
const PERFORMANCE_NOTICE_1 = L10N.getStr("netmonitor.perfNotice1");
const PERFORMANCE_NOTICE_2 = L10N.getStr("netmonitor.perfNotice2");
const PERFORMANCE_NOTICE_3 = L10N.getStr("netmonitor.perfNotice3");

/**
 * UI displayed when the request list is empty. Contains instructions on reloading
 * the page and on triggering performance analysis of the page.
 */
class RequestListEmptyNotice extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      onReloadClick: PropTypes.func.isRequired,
      onPerfClick: PropTypes.func.isRequired,
    };
  }

  render() {
    return div(
      {
        className: "request-list-empty-notice",
      },
      RequestListHeader(),
      div({ className: "notice-reload-message empty-notice-element" },
        span(null, RELOAD_NOTICE_1),
        button(
          {
            className: "devtools-button requests-list-reload-notice-button",
            "data-standalone": true,
            onClick: this.props.onReloadClick,
          },
          RELOAD_NOTICE_2
        ),
        span(null, RELOAD_NOTICE_3)
      ),
      div({ className: "notice-perf-message empty-notice-element" },
        span(null, PERFORMANCE_NOTICE_1),
        button({
          title: PERFORMANCE_NOTICE_3,
          className: "devtools-button requests-list-perf-notice-button",
          "data-standalone": true,
          onClick: this.props.onPerfClick,
        }),
        span(null, PERFORMANCE_NOTICE_2),
        MDNLink({ url: getPerformanceAnalysisURL() })
      )
    );
  }
}

module.exports = connect(
  undefined,
  (dispatch, props) => ({
    onPerfClick: () => dispatch(Actions.openStatistics(props.connector, true)),
    onReloadClick: () => props.connector.triggerActivity(
      ACTIVITY_TYPE.RELOAD.WITH_CACHE_DEFAULT),
  })
)(RequestListEmptyNotice);
