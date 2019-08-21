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

const ManifestLoader = createFactory(require("../manifest/ManifestLoader"));

class ManifestPage extends PureComponent {
  render() {
    return section({ className: `manifest-page` }, ManifestLoader({}));
  }
}

// Exports
module.exports = ManifestPage;
