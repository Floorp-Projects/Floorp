/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { getFormattedTime } = require("../utils/format-utils");

const { div } = dom;

class RequestListColumnDuration extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    return this.props.item.totalTime !== nextProps.item.totalTime;
  }

  render() {
    let { totalTime } = this.props.item;
    let duration = getFormattedTime(totalTime);
    return (
      div({
        className: "requests-list-column requests-list-duration",
        title: duration,
      },
        duration
      )
    );
  }
}

module.exports = RequestListColumnDuration;
