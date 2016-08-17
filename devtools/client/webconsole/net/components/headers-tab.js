/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const React = require("devtools/client/shared/vendor/react");
const NetInfoGroupList = React.createFactory(require("./net-info-group-list"));
const Spinner = React.createFactory(require("./spinner"));

// Shortcuts
const DOM = React.DOM;
const PropTypes = React.PropTypes;

/**
 * This template represents 'Headers' tab displayed when the user
 * expands network log in the Console panel. It's responsible for rendering
 * request and response HTTP headers.
 */
var HeadersTab = React.createClass({
  propTypes: {
    actions: PropTypes.shape({
      requestData: PropTypes.func.isRequired
    }),
    data: PropTypes.object.isRequired,
  },

  displayName: "HeadersTab",

  componentDidMount() {
    let { actions, data } = this.props;
    let requestHeaders = data.request.headers;
    let responseHeaders = data.response.headers;

    // Request headers if they are not available yet.
    // TODO: use async action objects as soon as Redux is in place
    if (!requestHeaders) {
      actions.requestData("requestHeaders");
    }

    if (!responseHeaders) {
      actions.requestData("responseHeaders");
    }
  },

  render() {
    let { data } = this.props;
    let requestHeaders = data.request.headers;
    let responseHeaders = data.response.headers;

    // TODO: Another groups to implement:
    // 1) Cached Headers
    // 2) Headers from upload stream
    let groups = [{
      key: "responseHeaders",
      name: Locale.$STR("responseHeaders"),
      params: responseHeaders
    }, {
      key: "requestHeaders",
      name: Locale.$STR("requestHeaders"),
      params: requestHeaders
    }];

    // If response headers are not available yet, display a spinner
    if (!responseHeaders || !responseHeaders.length) {
      groups[0].content = Spinner();
    }

    return (
      DOM.div({className: "headersTabBox"},
        DOM.div({className: "panelContent"},
          NetInfoGroupList({groups: groups})
        )
      )
    );
  }
});

// Exports from this module
module.exports = HeadersTab;
