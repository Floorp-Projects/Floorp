/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { L10N } = require("../../utils/l10n");

/**
 * Renders the "Time" column of a WebSocket frame.
 */
class FrameListColumnTime extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    return (
      this.props.item.type !== nextProps.item.type ||
      this.props.item.timeStamp !== nextProps.item.timeStamp
    );
  }

  /**
   * Format a DOMHighResTimeStamp (in microseconds) as HH:mm:ss.SSS
   * @param {number} highResTimeStamp
   */
  formatTime(highResTimeStamp) {
    const timeStamp = Math.floor(highResTimeStamp / 1000);
    const hoursMinutesSeconds = new Date(timeStamp).toLocaleTimeString(
      undefined,
      {
        formatMatcher: "basic",
        hour12: false,
      }
    );
    return L10N.getFormatStr(
      "netmonitor.ws.time.format",
      hoursMinutesSeconds,
      String(timeStamp % 1000).padStart(3, "0")
    );
  }

  render() {
    const label = this.formatTime(this.props.item.timeStamp);

    return dom.td(
      {
        className: "ws-frames-list-column ws-frames-list-time",
        title: label,
      },
      label
    );
  }
}

module.exports = FrameListColumnTime;
