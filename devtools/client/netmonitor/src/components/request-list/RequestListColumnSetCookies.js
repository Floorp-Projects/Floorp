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

class RequestListColumnSetCookies extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      item: PropTypes.object.isRequired,
    };
  }

  componentDidMount() {
    const { item, connector } = this.props;
    fetchNetworkUpdatePacket(connector.requestData, item, ["responseCookies"]);
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
    const { item, connector } = nextProps;
    fetchNetworkUpdatePacket(connector.requestData, item, ["responseCookies"]);
  }

  shouldComponentUpdate(nextProps) {
    let {
      responseCookies: currResponseCookies = { cookies: [] },
    } = this.props.item;
    let {
      responseCookies: nextResponseCookies = { cookies: [] },
    } = nextProps.item;
    currResponseCookies = currResponseCookies.cookies || currResponseCookies;
    nextResponseCookies = nextResponseCookies.cookies || nextResponseCookies;
    return currResponseCookies !== nextResponseCookies;
  }

  render() {
    let { responseCookies = { cookies: [] } } = this.props.item;
    responseCookies = responseCookies.cookies || responseCookies;
    const responseCookiesLength =
      responseCookies.length > 0 ? responseCookies.length : "";
    return dom.td(
      {
        className: "requests-list-column requests-list-set-cookies",
        title: responseCookiesLength,
      },
      responseCookiesLength
    );
  }
}

module.exports = RequestListColumnSetCookies;
