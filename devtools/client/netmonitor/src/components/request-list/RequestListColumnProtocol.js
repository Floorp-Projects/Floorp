/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  getFormattedProtocol,
} = require("resource://devtools/client/netmonitor/src/utils/request-utils.js");

class RequestListColumnProtocol extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    return (
      getFormattedProtocol(this.props.item) !==
      getFormattedProtocol(nextProps.item)
    );
  }

  render() {
    const protocol = getFormattedProtocol(this.props.item);

    return dom.td(
      {
        className: "requests-list-column requests-list-protocol",
        title: protocol,
      },
      protocol
    );
  }
}

module.exports = RequestListColumnProtocol;
