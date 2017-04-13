/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createClass,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");

const { div, span } = DOM;

const RequestListColumnRemoteIP = createClass({
  displayName: "RequestListColumnRemoteIP",

  propTypes: {
    item: PropTypes.object.isRequired,
  },

  shouldComponentUpdate(nextProps) {
    return this.props.item.remoteAddress !== nextProps.item.remoteAddress;
  },

  render() {
    const { remoteAddress, remotePort } = this.props.item;
    let remoteSummary = remoteAddress ? `${remoteAddress}:${remotePort}` : "";

    return (
      div({ className: "requests-list-subitem requests-list-remoteip" },
        span({ className: "subitem-label", title: remoteSummary }, remoteSummary),
      )
    );
  }
});

module.exports = RequestListColumnRemoteIP;
