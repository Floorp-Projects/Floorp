/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { getFormattedTime } = require("../utils/format-utils");
const { getEndTime } = require("../utils/request-utils");

const { div } = dom;

class RequestListColumnEndTime extends Component {
  static get propTypes() {
    return {
      firstRequestStartedMillis: PropTypes.number.isRequired,
      item: PropTypes.object.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    return getEndTime(this.props.item) !== getEndTime(nextProps.item);
  }

  render() {
    let { firstRequestStartedMillis, item } = this.props;
    let endTime = getFormattedTime(getEndTime(item, firstRequestStartedMillis));

    return (
      div({
        className: "requests-list-column requests-list-end-time",
        title: endTime,
      },
        endTime
      )
    );
  }
}

module.exports = RequestListColumnEndTime;
