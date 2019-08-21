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

const ManifestView = createFactory(require("./ManifestView"));
const ManifestViewEmpty = createFactory(require("./ManifestViewEmpty"));

const { MANIFEST_DATA } = require("../../constants");

class ManifestPage extends PureComponent {
  render() {
    const isManifestViewEmpty = !MANIFEST_DATA;

    // needs to be replaced with data from ManifestLoader
    const data = {
      warnings: MANIFEST_DATA.moz_validation,
      icons: MANIFEST_DATA.icons,
      identity: {
        name: MANIFEST_DATA.name,
        short_name: MANIFEST_DATA.short_name,
      },
      presentation: {
        display: MANIFEST_DATA.display,
        orientation: MANIFEST_DATA.orientation,
        start_url: MANIFEST_DATA.start_url,
        theme_color: MANIFEST_DATA.theme_color,
        background_color: MANIFEST_DATA.background_color,
      },
    };

    return section(
      {
        className: `app-page ${isManifestViewEmpty ? "app-page--empty" : ""}`,
      },
      ManifestLoader({}),
      isManifestViewEmpty
        ? ManifestViewEmpty({})
        : ManifestView({
            identity: data.identity,
            warnings: data.warnings,
            icons: data.icons,
            presentation: data.presentation,
          })
    );
  }
}

// Exports
module.exports = ManifestPage;
