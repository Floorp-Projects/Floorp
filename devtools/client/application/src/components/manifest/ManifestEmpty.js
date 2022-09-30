/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { openDocLink } = require("resource://devtools/client/shared/link.js");

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
      { className: "app-page__icon-container js-manifest-empty" },
      aside(
        {},
        Localized(
          {
            id: "sidebar-item-manifest",
            attrs: {
              alt: true,
            },
          },
          img({
            className: "app-page__icon",
            src: "chrome://devtools/skin/images/application-manifest.svg",
          })
        )
      ),
      div(
        {},
        Localized(
          {
            id: "manifest-empty-intro2",
          },
          h1({ className: "app-page__title" })
        ),
        p(
          {},
          Localized(
            { id: "manifest-empty-intro-link" },
            a({
              onClick: () => this.openDocumentation(),
            })
          )
        ),
        Localized({ id: "manifest-non-existing" }, p({}))
      )
    );
  }
}

// Exports
module.exports = ManifestEmpty;
