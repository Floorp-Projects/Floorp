/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  getFormattedTime,
} = require("devtools/client/netmonitor/src/utils/format-utils");
const {
  fetchNetworkUpdatePacket,
  getResponseTime,
  getStartTime,
  getEndTime,
} = require("devtools/client/netmonitor/src/utils/request-utils");

/**
 * This component represents a column displaying selected
 * timing value. There are following possible values this
 * column can render:
 * - Start Time
 * - End Time
 * - Response Time
 * - Duration Time
 * - Latency Time
 */
class RequestListColumnTime extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      firstRequestStartedMs: PropTypes.number.isRequired,
      item: PropTypes.object.isRequired,
      type: PropTypes.oneOf(["start", "end", "response", "duration", "latency"])
        .isRequired,
    };
  }

  componentDidMount() {
    const { item, connector } = this.props;
    fetchNetworkUpdatePacket(connector.requestData, item, ["eventTimings"]);
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
    const { item, connector } = nextProps;
    fetchNetworkUpdatePacket(connector.requestData, item, ["eventTimings"]);
  }

  shouldComponentUpdate(nextProps) {
    return this.getTime(this.props) !== this.getTime(nextProps);
  }

  getTime(props) {
    const { firstRequestStartedMs, item, type } = props;

    switch (type) {
      case "start":
        return getStartTime(item, firstRequestStartedMs);
      case "end":
        return getEndTime(item, firstRequestStartedMs);
      case "response":
        return getResponseTime(item, firstRequestStartedMs);
      case "duration":
        return item.totalTime;
      case "latency":
        return item.eventTimings ? item.eventTimings.timings.wait : undefined;
    }

    return 0;
  }

  render() {
    const { type } = this.props;
    const time = getFormattedTime(this.getTime(this.props));

    return dom.td(
      {
        className: "requests-list-column requests-list-" + type + "-time",
        title: time,
      },
      time
    );
  }
}

module.exports = RequestListColumnTime;
