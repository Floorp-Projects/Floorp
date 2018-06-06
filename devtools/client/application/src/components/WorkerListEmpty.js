/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { openWebLink, openTrustedLink } = require("devtools/client/shared/link");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { createFactory, Component } = require("devtools/client/shared/vendor/react");
const { a, article, h1, li, p, ul } = require("devtools/client/shared/vendor/react-dom-factories");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

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
    openTrustedLink("about:debugging#workers");
  }

  openDocumentation() {
    openWebLink(DOC_URL);
  }

  render() {
    return article(
      { className: "worker-list-empty" },
      Localized({
        id: "serviceworker-empty-intro",
        a: a({ className: "external-link", onClick: () => this.openDocumentation() })
      },
        h1({ className: "worker-list-empty__title" })
      ),
      Localized(
        { id: "serviceworker-empty-suggestions" },
        p({})
      ),
      ul(
        { className: "worker-list-empty__tips" },
        Localized({
          id: "serviceworker-empty-suggestions-console",
          a: a({ className: "link", onClick: () => this.switchToConsole() })
        },
          li({ className: "worker-list-empty__tips__item" })
        ),
        Localized({
          id: "serviceworker-empty-suggestions-debugger",
          a: a({ className: "link", onClick: () => this.switchToDebugger() })
        },
          li({ className: "worker-list-empty__tips__item" })
        ),
        Localized({
          id: "serviceworker-empty-suggestions-aboutdebugging",
          a: a({
            className: "link js-trusted-link",
            onClick: () => this.openAboutDebugging() })
        },
          li({ className: "worker-list-empty__tips__item" })
        ),
      )
    );
  }
}

// Exports

module.exports = WorkerListEmpty;
