/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

/**
 * Renders the "Time" column of a WebSocket frame.
 */
class FrameListColumnTime extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
      index: PropTypes.number.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    return this.props.item.timeStamp !== nextProps.item.timeStamp;
  }

  render() {
    const { timeStamp } = this.props.item;
    const { index } = this.props;

    // Convert microseconds (DOMHighResTimeStamp) to milliseconds
    const time = timeStamp / 1000;
    const microseconds = (timeStamp % 1000).toString().padStart(3, "0");

    return dom.td(
      {
        key: index,
        className: "ws-frames-list-column ws-frames-list-time",
        title: timeStamp,
      },
      new Date(time).toLocaleTimeString(undefined, { hour12: false }) +
        "." +
        microseconds
    );
  }
}

module.exports = FrameListColumnTime;
