/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  fetchNetworkUpdatePacket,
} = require("devtools/client/netmonitor/src/utils/request-utils");

class RequestListColumnCookies extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      item: PropTypes.object.isRequired,
    };
  }

  componentDidMount() {
    const { item, connector } = this.props;
    fetchNetworkUpdatePacket(connector.requestData, item, ["requestCookies"]);
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
    const { item, connector } = nextProps;
    fetchNetworkUpdatePacket(connector.requestData, item, ["requestCookies"]);
  }

  shouldComponentUpdate(nextProps) {
    let {
      requestCookies: currRequestCookies = { cookies: [] },
    } = this.props.item;
    let {
      requestCookies: nextRequestCookies = { cookies: [] },
    } = nextProps.item;
    currRequestCookies = currRequestCookies.cookies || currRequestCookies;
    nextRequestCookies = nextRequestCookies.cookies || nextRequestCookies;
    return currRequestCookies !== nextRequestCookies;
  }

  render() {
    let { requestCookies = { cookies: [] } } = this.props.item;
    requestCookies = requestCookies.cookies || requestCookies;
    const requestCookiesLength = requestCookies.length
      ? requestCookies.length
      : "";
    return dom.td(
      {
        className: "requests-list-column requests-list-cookies",
        title: requestCookiesLength,
      },
      requestCookiesLength
    );
  }
}

module.exports = RequestListColumnCookies;
