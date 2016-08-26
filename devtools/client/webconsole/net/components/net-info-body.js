/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const React = require("devtools/client/shared/vendor/react");
const { createFactories } = require("devtools/client/shared/components/reps/rep-utils");
const { Tabs, TabPanel } = createFactories(require("devtools/client/shared/components/tabs/tabs"));

// Network
const HeadersTab = React.createFactory(require("./headers-tab"));
const ResponseTab = React.createFactory(require("./response-tab"));
const ParamsTab = React.createFactory(require("./params-tab"));
const CookiesTab = React.createFactory(require("./cookies-tab"));
const PostTab = React.createFactory(require("./post-tab"));
const StackTraceTab = React.createFactory(require("./stacktrace-tab"));
const NetUtils = require("../utils/net");

// Shortcuts
const PropTypes = React.PropTypes;

/**
 * This template renders the basic Network log info body. It's not
 * visible by default, the user needs to expand the network log
 * to see it.
 *
 * This is the set of tabs displaying details about network events:
 * 1) Headers - request and response headers
 * 2) Params - URL parameters
 * 3) Response - response body
 * 4) Cookies - request and response cookies
 * 5) Post - posted data
 */
var NetInfoBody = React.createClass({
  propTypes: {
    tabActive: PropTypes.number.isRequired,
    actions: PropTypes.object.isRequired,
    data: PropTypes.shape({
      request: PropTypes.object.isRequired,
      response: PropTypes.object.isRequired
    })
  },

  displayName: "NetInfoBody",

  getDefaultProps() {
    return {
      tabActive: 0
    };
  },

  getInitialState() {
    return {
      data: {
        request: {},
        response: {}
      },
      tabActive: this.props.tabActive,
    };
  },

  onTabChanged(index) {
    this.setState({tabActive: index});
  },

  hasCookies() {
    let {request, response} = this.state.data;
    return this.state.hasCookies ||
      NetUtils.getHeaderValue(request.headers, "Cookie") ||
      NetUtils.getHeaderValue(response.headers, "Set-Cookie");
  },

  hasStackTrace() {
    let {cause} = this.state.data;
    return cause && cause.stacktrace && cause.stacktrace.length > 0;
  },

  getTabPanels() {
    let actions = this.props.actions;
    let data = this.state.data;
    let {request} = data;

    // Flags for optional tabs. Some tabs are visible only if there
    // are data to display.
    let hasParams = request.queryString && request.queryString.length;
    let hasPostData = request.bodySize > 0;

    let panels = [];

    // Headers tab
    panels.push(
      TabPanel({
        className: "headers",
        key: "headers",
        title: Locale.$STR("netRequest.headers")},
        HeadersTab({data: data, actions: actions})
      )
    );

    // URL parameters tab
    if (hasParams) {
      panels.push(
        TabPanel({
          className: "params",
          key: "params",
          title: Locale.$STR("netRequest.params")},
          ParamsTab({data: data, actions: actions})
        )
      );
    }

    // Posted data tab
    if (hasPostData) {
      panels.push(
        TabPanel({
          className: "post",
          key: "post",
          title: Locale.$STR("netRequest.post")},
          PostTab({data: data, actions: actions})
        )
      );
    }

    // Response tab
    panels.push(
      TabPanel({className: "response", key: "response",
        title: Locale.$STR("netRequest.response")},
        ResponseTab({data: data, actions: actions})
      )
    );

    // Cookies tab
    if (this.hasCookies()) {
      panels.push(
        TabPanel({
          className: "cookies",
          key: "cookies",
          title: Locale.$STR("netRequest.cookies")},
          CookiesTab({
            data: data,
            actions: actions
          })
        )
      );
    }

    // Stacktrace tab
    if (this.hasStackTrace()) {
      panels.push(
        TabPanel({
          className: "stacktrace-tab",
          key: "stacktrace",
          title: Locale.$STR("netRequest.callstack")},
          StackTraceTab({
            data: data,
            actions: actions
          })
        )
      );
    }

    return panels;
  },

  render() {
    let tabActive = this.state.tabActive;
    let tabPanels = this.getTabPanels();
    return (
      Tabs({
        tabActive: tabActive,
        onAfterChange: this.onTabChanged},
        tabPanels
      )
    );
  }
});

// Exports from this module
module.exports = NetInfoBody;
