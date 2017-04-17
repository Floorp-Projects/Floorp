/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createClass,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");

const { div } = DOM;

const RequestListColumnSetCookies = createClass({
  displayName: "RequestListColumnSetCookies",

  propTypes: {
    item: PropTypes.object.isRequired,
  },

  shouldComponentUpdate(nextProps) {
    let { responseCookies: currResponseCookies = { cookies: [] } } = this.props.item;
    let { responseCookies: nextResponseCookies = { cookies: [] } } = nextProps.item;
    currResponseCookies = currResponseCookies.cookies || currResponseCookies;
    nextResponseCookies = nextResponseCookies.cookies || nextResponseCookies;
    return currResponseCookies !== nextResponseCookies;
  },

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
});

module.exports = RequestListColumnSetCookies;
