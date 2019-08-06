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
const { PluralForm } = require("devtools/shared/plural-form");
const {
  getDisplayedFramesSummary,
} = require("devtools/client/netmonitor/src/selectors/index.js");
const {
  getFormattedSize,
  getFormattedTime,
} = require("devtools/client/netmonitor/src/utils/format-utils.js");
const { L10N } = require("devtools/client/netmonitor/src/utils/l10n.js");
const {
  propertiesEqual,
} = require("devtools/client/netmonitor/src/utils/request-utils.js");

const { div, footer } = dom;

const FRAMES_COUNT_EMPTY = L10N.getStr(
  "networkMenu.ws.summary.framesCountEmpty"
);
const TOOLTIP_FRAMES_COUNT = L10N.getStr(
  "networkMenu.ws.summary.tooltip.framesCount"
);
const TOOLTIP_FRAMES_TOTAL_SIZE = L10N.getStr(
  "networkMenu.ws.summary.tooltip.framesTotalSize"
);
const TOOLTIP_FRAMES_TOTAL_TIME = L10N.getStr(
  "networkMenu.ws.summary.tooltip.framesTotalTime"
);

const UPDATED_WS_SUMMARY_PROPS = ["count", "totalMs", "totalSize"];

/**
 * Displays the summary of frame count, total size and total time since the first frame.
 */
class StatusBar extends Component {
  static get propTypes() {
    return {
      summary: PropTypes.object.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    const { summary } = this.props;
    return !propertiesEqual(
      UPDATED_WS_SUMMARY_PROPS,
      summary,
      nextProps.summary
    );
  }

  render() {
    const { summary } = this.props;
    const { count, totalSize, totalMs } = summary;

    const countText =
      count === 0
        ? FRAMES_COUNT_EMPTY
        : PluralForm.get(
            count,
            L10N.getStr("networkMenu.ws.summary.framesCount2")
          ).replace("#1", count);
    const totalSizeText = getFormattedSize(totalSize);
    const totalMillisText = getFormattedTime(totalMs);

    return footer(
      { className: "devtools-toolbar devtools-toolbar-bottom" },
      div(
        {
          className: "status-bar-label frames-list-network-summary-count",
          title: TOOLTIP_FRAMES_COUNT,
        },
        countText
      ),
      count !== 0 &&
        div(
          {
            className:
              "status-bar-label frames-list-network-summary-total-size",
            title: TOOLTIP_FRAMES_TOTAL_SIZE,
          },
          totalSizeText
        ),
      count !== 0 &&
        div(
          {
            className:
              "status-bar-label frames-list-network-summary-total-millis",
            title: TOOLTIP_FRAMES_TOTAL_TIME,
          },
          totalMillisText
        )
    );
  }
}

module.exports = connect(state => ({
  summary: getDisplayedFramesSummary(state),
}))(StatusBar);
