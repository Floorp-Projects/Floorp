/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { Component } = require("devtools/client/shared/vendor/react");
const { a, article, h1, li, p, ul } = require("devtools/client/shared/vendor/react-dom-factories");
const DOC_URL = "https://developer.mozilla.org/docs/Web/API/Service_Worker_API/Using_Service_Workers" +
  "?utm_source=devtools&utm_medium=sw-panel-blank";

/**
 * This component displays help information when no service workers are found for the
 * current target.
 */
class WorkerListEmpty extends Component {
  static get propTypes() {
    return {
      serviceContainer: PropTypes.object.isRequired,
    };
  }

  switchToConsole() {
    this.props.serviceContainer.selectTool("webconsole");
  }

  switchToDebugger() {
    this.props.serviceContainer.selectTool("jsdebugger");
  }

  openAboutDebugging() {
    this.props.serviceContainer.openTrustedLink("about:debugging#workers");
  }

  openDocumentation() {
    this.props.serviceContainer.openWebLink(DOC_URL);
  }

  render() {
    return article(
      { className: "worker-list-empty" },
      h1(
        {},
        "You need to register a Service Worker to inspect it here.",
        a(
          { className: "external-link", onClick: () => this.openDocumentation() },
          "Learn More"
        )
      ),
      p(
        {},
        `If the current page should have a service worker, ` +
        `here are some things you can try:`,
      ),
      ul(
        { className: "worker-list-empty__tips"},
        li(
          { className: "worker-list-empty__tips__item"},
          "Look for errors in the Console.",
          a(
            { className: "link", onClick: () => this.switchToConsole() },
            "Open the Console"
          )
        ),
        li(
          { className: "worker-list-empty__tips__item"},
          "Step through you Service Worker registration and look for exceptions.",
          a(
            { className: "link", onClick: () => this.switchToDebugger()},
            "Open the Debugger"
          )
        ),
        li(
          { className: "worker-list-empty__tips__item"},
          "Inspect Service Workers from other domains.",
          a(
            { className: "external-link", onClick: () => this.openAboutDebugging() },
            "Open about:debugging"
          )
        )
      )
    );
  }
}

// Exports

module.exports = WorkerListEmpty;
