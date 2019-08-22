/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const {
  article,
  h1,
  p,
} = require("devtools/client/shared/vendor/react-dom-factories");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

/**
 * This component displays help information when no manifest is found for the
 * current target.
 */
class ManifestViewEmpty extends PureComponent {
  render() {
    return article(
      { className: "manifest-view-empty" },
      Localized(
        {
          id: "manifest-view-header",
        },
        h1({ className: "app-page__title" })
      ),
      Localized({ id: "manifest-non-existing" }, p({}))
    );
  }
}

// Exports
module.exports = ManifestViewEmpty;
