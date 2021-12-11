/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const {
  getSearchStatus,
  getSearchResultCount,
  getSearchResourceCount,
} = require("devtools/client/netmonitor/src/selectors/index.js");
const { PluralForm } = require("devtools/shared/plural-form");
const { L10N } = require("devtools/client/netmonitor/src/utils/l10n.js");
const { div, span } = dom;
const { SEARCH_STATUS } = require("devtools/client/netmonitor/src/constants");

/**
 * Displays the number of lines found for results and resource count (files)
 */
class StatusBar extends Component {
  static get propTypes() {
    return {
      status: PropTypes.string,
      resultsCount: PropTypes.string,
      resourceCount: PropTypes.string,
    };
  }

  getSearchStatusDoneLabel(lines, files) {
    const matchingLines = PluralForm.get(
      lines,
      L10N.getStr("netmonitor.search.status.labels.matchingLines")
    ).replace("#1", lines);
    const matchingFiles = PluralForm.get(
      files,
      L10N.getStr("netmonitor.search.status.labels.fileCount")
    ).replace("#1", files);

    return L10N.getFormatStr(
      "netmonitor.search.status.labels.done",
      matchingLines,
      matchingFiles
    );
  }

  renderStatus() {
    const { status, resultsCount, resourceCount } = this.props;

    switch (status) {
      case SEARCH_STATUS.FETCHING:
        return L10N.getStr("netmonitor.search.status.labels.fetching");
      case SEARCH_STATUS.DONE:
        return this.getSearchStatusDoneLabel(resultsCount, resourceCount);
      case SEARCH_STATUS.ERROR:
        return L10N.getStr("netmonitor.search.status.labels.error");
      case SEARCH_STATUS.CANCELED:
        return L10N.getStr("netmonitor.search.status.labels.canceled");
      default:
        return "";
    }
  }

  render() {
    const { status } = this.props;
    return div(
      { className: "devtools-toolbar devtools-toolbar-bottom" },
      div(
        {
          className: "status-bar-label",
          title: this.renderStatus(),
        },
        this.renderStatus(),
        status === SEARCH_STATUS.FETCHING
          ? span({ className: "img loader" })
          : ""
      )
    );
  }
}

module.exports = connect(state => ({
  status: getSearchStatus(state),
  resultsCount: getSearchResultCount(state),
  resourceCount: getSearchResourceCount(state),
}))(StatusBar);
