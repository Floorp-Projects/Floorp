/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { openDocLink } = require("devtools/client/shared/link");

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const {
  a,
  article,
  h1,
  p,
} = require("devtools/client/shared/vendor/react-dom-factories");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const DOC_URL =
  "https://developer.mozilla.org/en-US/docs/Web/Manifest" +
  "?utm_source=devtools&utm_medium=sw-panel-blank";

/**
 * This component displays help information when no manifest is found for the
 * current target.
 */
class ManifestEmpty extends PureComponent {
  openDocumentation() {
    openDocLink(DOC_URL);
  }

  render() {
    return article(
      { className: "manifest-empty js-manifest-empty" },
      Localized(
        {
          id: "manifest-empty-intro",
          a: a({
            className: "external-link",
            onClick: () => this.openDocumentation(),
          }),
        },
        h1({ className: "app-page__title" })
      ),
      Localized({ id: "manifest-non-existing" }, p({}))
    );
  }
}

// Exports
module.exports = ManifestEmpty;
