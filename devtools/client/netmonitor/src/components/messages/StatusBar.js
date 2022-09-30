/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const {
  connect,
} = require("resource://devtools/client/shared/redux/visibility-handler-connect.js");
const { PluralForm } = require("resource://devtools/shared/plural-form.js");
const {
  getDisplayedMessagesSummary,
} = require("resource://devtools/client/netmonitor/src/selectors/index.js");
const {
  getFormattedSize,
  getFormattedTime,
} = require("resource://devtools/client/netmonitor/src/utils/format-utils.js");
const {
  L10N,
} = require("resource://devtools/client/netmonitor/src/utils/l10n.js");
const {
  propertiesEqual,
} = require("resource://devtools/client/netmonitor/src/utils/request-utils.js");

const {
  CHANNEL_TYPE,
} = require("resource://devtools/client/netmonitor/src/constants.js");

const { div, footer } = dom;

const MESSAGE_COUNT_EMPTY = L10N.getStr(
  "networkMenu.ws.summary.framesCountEmpty"
);
const TOOLTIP_MESSAGE_COUNT = L10N.getStr(
  "networkMenu.ws.summary.tooltip.framesCount"
);
const TOOLTIP_MESSAGE_TOTAL_SIZE = L10N.getStr(
  "networkMenu.ws.summary.tooltip.framesTotalSize"
);
const TOOLTIP_MESSAGE_TOTAL_TIME = L10N.getStr(
  "networkMenu.ws.summary.tooltip.framesTotalTime"
);

const UPDATED_MSG_SUMMARY_PROPS = ["count", "totalMs", "totalSize"];

/**
 * Displays the summary of message count, total size and total time since the first message.
 */
class StatusBar extends Component {
  static get propTypes() {
    return {
      channelType: PropTypes.string,
      summary: PropTypes.object.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    const { summary, channelType } = this.props;
    return (
      channelType !== nextProps.channelType ||
      !propertiesEqual(UPDATED_MSG_SUMMARY_PROPS, summary, nextProps.summary)
    );
  }

  render() {
    const { summary, channelType } = this.props;
    const { count, totalMs, sentSize, receivedSize, totalSize } = summary;

    const countText =
      count === 0
        ? MESSAGE_COUNT_EMPTY
        : PluralForm.get(
            count,
            L10N.getStr("networkMenu.ws.summary.framesCount2")
          ).replace("#1", count);
    const totalSizeText = getFormattedSize(totalSize);
    const sentSizeText = getFormattedSize(sentSize);
    const receivedText = getFormattedSize(receivedSize);
    const totalMillisText = getFormattedTime(totalMs);

    // channelType might be null in which case it's better to just show
    // total size than showing all three sizes.
    const summaryText =
      channelType === CHANNEL_TYPE.WEB_SOCKET
        ? L10N.getFormatStr(
            "networkMenu.ws.summary.label.framesTranferredSize",
            totalSizeText,
            sentSizeText,
            receivedText
          )
        : `${totalSizeText} total`;

    return footer(
      { className: "devtools-toolbar devtools-toolbar-bottom" },
      div(
        {
          className: "status-bar-label message-network-summary-count",
          title: TOOLTIP_MESSAGE_COUNT,
        },
        countText
      ),
      count !== 0 &&
        div(
          {
            className: "status-bar-label message-network-summary-total-size",
            title: TOOLTIP_MESSAGE_TOTAL_SIZE,
          },
          summaryText
        ),
      count !== 0 &&
        div(
          {
            className: "status-bar-label message-network-summary-total-millis",
            title: TOOLTIP_MESSAGE_TOTAL_TIME,
          },
          totalMillisText
        )
    );
  }
}

module.exports = connect(state => ({
  channelType: state.messages.currentChannelType,
  summary: getDisplayedMessagesSummary(state),
}))(StatusBar);
