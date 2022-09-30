/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  openDocLink,
  openTrustedLink,
} = require("resource://devtools/client/shared/link.js");
const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  a,
  article,
  aside,
  div,
  h1,
  img,
  p,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");

const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const Localized = createFactory(FluentReact.Localized);

const {
  services,
} = require("resource://devtools/client/application/src/modules/application-services.js");

const DOC_URL =
  "https://developer.mozilla.org/docs/Web/API/Service_Worker_API/Using_Service_Workers" +
  "?utm_source=devtools&utm_medium=sw-panel-blank";

/**
 * This component displays help information when no service workers are found for the
 * current target.
 */
class RegistrationListEmpty extends PureComponent {
  switchToConsole() {
    services.selectTool("webconsole");
  }

  switchToDebugger() {
    services.selectTool("jsdebugger");
  }

  openAboutDebugging() {
    openTrustedLink("about:debugging#workers");
  }

  openDocumentation() {
    openDocLink(DOC_URL);
  }

  render() {
    return article(
      { className: "app-page__icon-container js-registration-list-empty" },
      aside(
        {},
        Localized(
          {
            id: "sidebar-item-service-workers",
            attrs: {
              alt: true,
            },
          },
          img({
            className: "app-page__icon",
            src: "chrome://devtools/skin/images/debugging-workers.svg",
          })
        )
      ),
      div(
        {},
        Localized(
          {
            id: "serviceworker-empty-intro2",
          },
          h1({ className: "app-page__title" })
        ),
        Localized(
          {
            id: "serviceworker-empty-suggestions2",
            a: a({
              onClick: () => this.switchToConsole(),
            }),
            // NOTE: for <Localized> to parse the markup in the string, the
            //       markup needs to be actual HTML elements
            span: a({
              onClick: () => this.switchToDebugger(),
            }),
          },
          p({})
        ),
        p(
          {},
          Localized(
            { id: "serviceworker-empty-intro-link" },
            a({
              onClick: () => this.openDocumentation(),
            })
          )
        ),
        p(
          {},
          Localized(
            { id: "serviceworker-empty-suggestions-aboutdebugging2" },
            a({
              className: "js-trusted-link",
              onClick: () => this.openAboutDebugging(),
            })
          )
        )
      )
    );
  }
}

// Exports
module.exports = RegistrationListEmpty;
