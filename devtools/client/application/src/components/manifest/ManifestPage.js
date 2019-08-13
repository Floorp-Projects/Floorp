/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const {
  section,
} = require("devtools/client/shared/vendor/react-dom-factories");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

class ManifestPage extends PureComponent {
  render() {
    return Localized(
      {
        id: "manifest-empty-intro",
      },
      section({ className: `manifest-page` })
    );
  }
}

// Exports
module.exports = ManifestPage;
