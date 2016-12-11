/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals NetMonitorView */

"use strict";

const { createClass, PropTypes, DOM } = require("devtools/client/shared/vendor/react");
const { L10N } = require("../l10n");
const { div, span, button } = DOM;
const { connect } = require("devtools/client/shared/vendor/react-redux");

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
        id: "requests-menu-empty-notice",
        className: "request-list-empty-notice",
      },
      div({ id: "notice-reload-message" },
        span(null, L10N.getStr("netmonitor.reloadNotice1")),
        button(
          {
            id: "requests-menu-reload-notice-button",
            className: "devtools-toolbarbutton",
            "data-standalone": true,
            onClick: this.props.onReloadClick,
          },
          L10N.getStr("netmonitor.reloadNotice2")
        ),
        span(null, L10N.getStr("netmonitor.reloadNotice3"))
      ),
      div({ id: "notice-perf-message" },
        span(null, L10N.getStr("netmonitor.perfNotice1")),
        button({
          id: "requests-menu-perf-notice-button",
          title: L10N.getStr("netmonitor.perfNotice3"),
          className: "devtools-button",
          "data-standalone": true,
          onClick: this.props.onPerfClick,
        }),
        span(null, L10N.getStr("netmonitor.perfNotice2"))
      )
    );
  }
});

module.exports = connect(
  undefined,
  dispatch => ({
    onPerfClick: e => NetMonitorView.toggleFrontendMode(),
    onReloadClick: e => NetMonitorView.reloadPage(),
  })
)(RequestListEmptyNotice);
