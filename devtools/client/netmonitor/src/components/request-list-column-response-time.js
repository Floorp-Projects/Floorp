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
const { getResponseTime } = require("../utils/request-utils");

const { div } = DOM;

const RequestListColumnResponseTime = createClass({
  displayName: "RequestListColumnResponseTime",

  propTypes: {
    firstRequestStartedMillis: PropTypes.number.isRequired,
    item: PropTypes.object.isRequired,
  },

  shouldComponentUpdate(nextProps) {
    return getResponseTime(this.props.item) !== getResponseTime(nextProps.item);
  },

  render() {
    let { firstRequestStartedMillis, item } = this.props;
    let responseTime = getFormattedTime(
      getResponseTime(item, firstRequestStartedMillis));

    return (
      div({
        className: "requests-list-column requests-list-response-time",
        title: responseTime,
      },
        responseTime
      )
    );
  }
});

module.exports = RequestListColumnResponseTime;
