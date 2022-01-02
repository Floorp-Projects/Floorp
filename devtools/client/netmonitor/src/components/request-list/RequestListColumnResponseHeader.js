/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  getResponseHeader,
  fetchNetworkUpdatePacket,
} = require("devtools/client/netmonitor/src/utils/request-utils");

/**
 * Renders a response header column in the requests list.  The actual
 * header to show is passed as a prop.
 */
class RequestListColumnResponseHeader extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      item: PropTypes.object.isRequired,
      header: PropTypes.string.isRequired,
    };
  }

  componentDidMount() {
    const { item, connector } = this.props;
    fetchNetworkUpdatePacket(connector.requestData, item, ["responseHeaders"]);
  }

  componentWillReceiveProps(nextProps) {
    const { item, connector } = nextProps;
    fetchNetworkUpdatePacket(connector.requestData, item, ["responseHeaders"]);
  }

  shouldComponentUpdate(nextProps) {
    const currHeader = getResponseHeader(this.props.item, this.props.header);
    const nextHeader = getResponseHeader(nextProps.item, nextProps.header);
    return currHeader !== nextHeader;
  }

  render() {
    const header = getResponseHeader(this.props.item, this.props.header);
    return dom.td(
      {
        className: "requests-list-column requests-list-response-header",
        title: header,
      },
      header
    );
  }
}

module.exports = RequestListColumnResponseHeader;
