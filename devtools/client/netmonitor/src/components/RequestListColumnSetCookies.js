/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { div } = dom;

class RequestListColumnSetCookies extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      item: PropTypes.object.isRequired,
    };
  }

  componentDidMount() {
    this.maybeFetchResponseCookies(this.props);
  }

  componentWillReceiveProps(nextProps) {
    this.maybeFetchResponseCookies(nextProps);
  }

  shouldComponentUpdate(nextProps) {
    let { responseCookies: currResponseCookies = { cookies: [] } } = this.props.item;
    let { responseCookies: nextResponseCookies = { cookies: [] } } = nextProps.item;
    currResponseCookies = currResponseCookies.cookies || currResponseCookies;
    nextResponseCookies = nextResponseCookies.cookies || nextResponseCookies;
    return currResponseCookies !== nextResponseCookies;
  }

  /**
   * Lazily fetch response cookies from the backend.
   */
  maybeFetchResponseCookies(props) {
    if (props.item.responseCookiesAvailable && !props.responseCookies) {
      props.connector.requestData(props.item.id, "responseCookies");
    }
  }

  render() {
    let { responseCookies = { cookies: [] } } = this.props.item;
    responseCookies = responseCookies.cookies || responseCookies;
    let responseCookiesLength = responseCookies.length > 0 ? responseCookies.length : "";
    return (
      div({
        className: "requests-list-column requests-list-set-cookies",
        title: responseCookiesLength
      }, responseCookiesLength)
    );
  }
}

module.exports = RequestListColumnSetCookies;
