/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createClass,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { getFormattedProtocol } = require("../utils/request-utils");

const { div } = DOM;

const RequestListColumnProtocol = createClass({
  displayName: "RequestListColumnProtocol",

  propTypes: {
    item: PropTypes.object.isRequired,
  },

  shouldComponentUpdate(nextProps) {
    return getFormattedProtocol(this.props.item) !==
      getFormattedProtocol(nextProps.item);
  },

  render() {
    let protocol = getFormattedProtocol(this.props.item);

    return (
      div({
        className: "requests-list-column requests-list-protocol",
        title: protocol,
      },
        protocol
      )
    );
  }
});

module.exports = RequestListColumnProtocol;
