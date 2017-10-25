/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Component, createFactory, PropTypes } =
  require("devtools/client/shared/vendor/react");
const { createFactories } = require("devtools/client/shared/react-utils");
const { Tabs, TabPanel } = createFactories(require("devtools/client/shared/components/tabs/Tabs"));

// Network
const HeadersTab = createFactory(require("./headers-tab"));
const ResponseTab = createFactory(require("./response-tab"));
const ParamsTab = createFactory(require("./params-tab"));
const CookiesTab = createFactory(require("./cookies-tab"));
const PostTab = createFactory(require("./post-tab"));
const StackTraceTab = createFactory(require("./stacktrace-tab"));
const NetUtils = require("../utils/net");


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
class NetInfoBody extends Component {
  static get propTypes() {
    return {
      tabActive: PropTypes.number.isRequired,
      actions: PropTypes.object.isRequired,
      data: PropTypes.shape({
        request: PropTypes.object.isRequired,
        response: PropTypes.object.isRequired
      }),
      // Service to enable the source map feature.
      sourceMapService: PropTypes.object,
    };
  }

  static get defaultProps() {
    return {
      tabActive: 0
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      data: {
        request: {},
        response: {}
      },
      tabActive: props.tabActive,
    };

    this.onTabChanged = this.onTabChanged.bind(this);
    this.hasCookies = this.hasCookies.bind(this);
    this.hasStackTrace = this.hasStackTrace.bind(this);
    this.getTabPanels = this.getTabPanels.bind(this);
  }

  onTabChanged(index) {
    this.setState({tabActive: index});
  }

  hasCookies() {
    let {request, response} = this.state.data;
    return this.state.hasCookies ||
      NetUtils.getHeaderValue(request.headers, "Cookie") ||
      NetUtils.getHeaderValue(response.headers, "Set-Cookie");
  }

  hasStackTrace() {
    let {cause} = this.state.data;
    return cause && cause.stacktrace && cause.stacktrace.length > 0;
  }

  getTabPanels() {
    let { actions, sourceMapService } = this.props;
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
            actions: actions,
            sourceMapService: sourceMapService,
          })
        )
      );
    }

    return panels;
  }

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
}

// Exports from this module
module.exports = NetInfoBody;
