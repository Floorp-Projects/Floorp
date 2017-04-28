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

const RequestListColumnCookies = createClass({
  displayName: "RequestListColumnCookies",

  propTypes: {
    item: PropTypes.object.isRequired,
  },

  shouldComponentUpdate(nextProps) {
    let { requestCookies: currRequestCookies = { cookies: [] } } = this.props.item;
    let { requestCookies: nextRequestCookies = { cookies: [] } } = nextProps.item;
    currRequestCookies = currRequestCookies.cookies || currRequestCookies;
    nextRequestCookies = nextRequestCookies.cookies || nextRequestCookies;
    return currRequestCookies !== nextRequestCookies;
  },

  render() {
    let { requestCookies = { cookies: [] } } = this.props.item;
    requestCookies = requestCookies.cookies || requestCookies;
    let requestCookiesLength = requestCookies.length > 0 ? requestCookies.length : "";
    return (
      div({
        className: "requests-list-column requests-list-cookies",
        title: requestCookiesLength
      }, requestCookiesLength)
    );
  }
});

module.exports = RequestListColumnCookies;
