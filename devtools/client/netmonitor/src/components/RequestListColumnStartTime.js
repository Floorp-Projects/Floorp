/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { getFormattedTime } = require("../utils/format-utils");
const { getStartTime } = require("../utils/request-utils");

const { div } = dom;

class RequestListColumnStartTime extends Component {
  static get propTypes() {
    return {
      firstRequestStartedMillis: PropTypes.number.isRequired,
      item: PropTypes.object.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    return getStartTime(this.props.item, this.props.firstRequestStartedMillis)
      !== getStartTime(nextProps.item, nextProps.firstRequestStartedMillis);
  }

  render() {
    let { firstRequestStartedMillis, item } = this.props;
    let startTime = getFormattedTime(getStartTime(item, firstRequestStartedMillis));

    return (
      div({
        className: "requests-list-column requests-list-start-time",
        title: startTime,
      },
        startTime
      )
    );
  }
}

module.exports = RequestListColumnStartTime;
