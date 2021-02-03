/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React deps
const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { div, h1, h2, h3, p, a } = dom;

// File a bug for the Net Monitor specifically
const bugLink =
  "https://bugzilla.mozilla.org/enter_bug.cgi?product=DevTools&component=Netmonitor";

// Localized strings (devtools/client/locales/en-us/netmonitor.properties)
const { L10N } = require("devtools/client/netmonitor/src/utils/l10n.js");
const ERROR_DESCRIPTION = L10N.getStr("netmonitor.errorpanel.description");
const FILE_BUG_BUTTON = L10N.getStr("netmonitor.errorpanel.fileBugButton");
const RELOAD_PAGE_INFO = L10N.getStr("netmonitor.errorpanel.reloadPanelInfo");

/**
 * Error boundary that wraps around the main App component.
 */
class AppErrorBoundary extends Component {
  static get propTypes() {
    return {
      children: PropTypes.any.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      errorMsg: "No error",
      errorStack: null,
      errorInfo: null,
    };
  }

  /**
   *  Map the `info` object to a render.
   *  Currently, `info` usually just contains something similar to the
   *  following object (which is provided to componentDidCatch):
   *  componentStack: {"\n in (component) \n in (other component)..."}
   */
  renderErrorInfo(info = {}) {
    if (Object.keys(info).length > 0) {
      return Object.keys(info).map((obj, outerIdx) => {
        const traceParts = info[obj]
          .split("\n")
          .map((part, idx) => p({ key: `strace${idx}` }, part));
        return div(
          { key: `st-div-${outerIdx}`, className: "stack-trace-section" },
          h3({}, "React Component Stack"),
          p({ key: `st-p-${outerIdx}` }, obj.toString()),
          traceParts
        );
      });
    }

    return p({}, "undefined errorInfo");
  }

  renderStackTrace(stacktrace = "") {
    const re = /:\d+:\d+/g;
    const traces = stacktrace
      .replace(re, "$&,")
      .split(",")
      .map((trace, index) => {
        return p({}, trace);
      });

    return div(
      { className: "stack-trace-section" },
      h3({}, "Stacktrace"),
      traces
    );
  }

  // Return a valid object, even if we don't receive one
  getValidInfo(infoObj) {
    if (!infoObj.componentStack) {
      try {
        return { componentStack: JSON.stringify(infoObj) };
      } catch (err) {
        return { componentStack: `Unknown Error: ${err}` };
      }
    }
    return infoObj;
  }

  // Called when a child component throws an error.
  componentDidCatch(error, info) {
    const validInfo = this.getValidInfo(info);
    this.setState({
      errorMsg: error.toString(),
      errorStack: error.stack,
      errorInfo: validInfo,
    });
  }

  getBugLink() {
    const compStack = this.getValidInfo(this.state.errorInfo).componentStack;
    const errorMsg = this.state.errorMsg;
    const msg = (errorMsg + compStack).replace(/\n/gi, "%0A");
    return `${bugLink}&comment=${msg}`;
  }

  render() {
    if (this.state.errorInfo !== null) {
      return div(
        { className: "app-error-panel" },
        h1({ className: "error-panel-header" }, ERROR_DESCRIPTION),
        a(
          {
            className: "error-panel-file-button",
            href: this.getBugLink(),
            onClick: () => {
              window.open(this.getBugLink(), "_blank");
            },
          },
          FILE_BUG_BUTTON
        ),
        h2({ className: "error-panel-error" }, this.state.errorMsg),
        div({}, this.renderErrorInfo(this.state.errorInfo)),
        div({}, this.renderStackTrace(this.state.errorStack)),
        p({ className: "error-panel-reload-info" }, RELOAD_PAGE_INFO)
      );
    }
    return this.props.children;
  }
}

module.exports = AppErrorBoundary;
