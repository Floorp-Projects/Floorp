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

const Manifest = createFactory(require("./Manifest"));
const ManifestEmpty = createFactory(require("./ManifestEmpty"));

class ManifestPage extends PureComponent {
  render() {
    // TODO: needs to be replaced with data from Redux
    const data = {
      validation: [
        { level: "warning", message: "Icons item at index 0 is invalid." },
        { level: "error", message: "Random JSON error" },
        {
          level: "warning",
          message:
            "Icons item at index 2 is invalid. Icons item at index 2 is invalid. Icons item at index 2 is invalid. Icons item at index 2 is invalid.",
        },
      ],
      icons: [],
      identity: [
        {
          key: "name",
          value:
            "Name is a verrry long name and the name is longer tha you thinnk because it is loooooooooooooooooooooooooooooooooooooooooooooooong",
        },
        {
          key: "short_name",
          value: "Na",
        },
      ],
      presentation: [
        { key: "display", value: "browser" },
        { key: "orientation", value: "landscape" },
        { key: "start_url", value: "root" },
        { key: "theme_color", value: "#345" },
        { key: "background_color", value: "#F9D" },
      ],
    };

    const isManifestEmpty = !data;

    return section(
      {
        className: `app-page ${isManifestEmpty ? "app-page--empty" : ""}`,
      },
      ManifestLoader({}),
      isManifestEmpty
        ? ManifestEmpty({})
        : Manifest({
            icons: data.icons,
            identity: data.identity,
            presentation: data.presentation,
            validation: data.validation,
          })
    );
  }
}

// Exports
module.exports = ManifestPage;
