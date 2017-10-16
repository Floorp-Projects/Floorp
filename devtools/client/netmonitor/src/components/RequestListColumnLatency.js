/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createClass,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { getFormattedTime } = require("../utils/format-utils");

const { div } = DOM;

const RequestListColumnLatency = createClass({
  displayName: "RequestListColumnLatency",

  propTypes: {
    item: PropTypes.object.isRequired,
  },

  shouldComponentUpdate(nextProps) {
    let { eventTimings: currEventTimings = { timings: {} } } = this.props.item;
    let { eventTimings: nextEventTimings = { timings: {} } } = nextProps.item;
    return currEventTimings.timings.wait !== nextEventTimings.timings.wait;
  },

  render() {
    let { eventTimings = { timings: {} } } = this.props.item;
    let latency = getFormattedTime(eventTimings.timings.wait);

    return (
      div({
        className: "requests-list-column requests-list-latency",
        title: latency,
      },
        latency
      )
    );
  }
});

module.exports = RequestListColumnLatency;
