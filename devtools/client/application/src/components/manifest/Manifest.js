/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const {
  article,
  caption,
  h1,
  table,
  tbody,
} = require("devtools/client/shared/vendor/react-dom-factories");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const ManifestItemIcon = createFactory(require("./ManifestItemIcon"));
const ManifestItemText = createFactory(require("./ManifestItemText"));
const ManifestItemWarning = createFactory(require("./ManifestItemWarning"));

/**
 * Displays a canonical manifest, splitted in different sections
 */
class Manifest extends PureComponent {
  static get propTypes() {
    return {
      identity: PropTypes.object.isRequired,
      warnings: PropTypes.array.isRequired,
      icons: PropTypes.array.isRequired,
      presentation: PropTypes.object.isRequired,
    };
  }
  render() {
    const { identity, warnings, icons, presentation } = this.props;

    return article(
      {},
      Localized(
        {
          id: "manifest-view-header",
        },
        h1({ className: "app-page__title" })
      ),
      table(
        { className: "manifest", key: "errors-and-warnings" },
        Localized(
          { id: "manifest-item-warnings" },
          caption({ className: "manifest__title" })
        ),
        tbody(
          {},
          warnings.map((warning, index) =>
            ManifestItemWarning({ warning, key: `warning-${index}` })
          )
        )
      ),
      table(
        { className: "manifest", key: "identity" },
        Localized(
          { id: "manifest-item-identity" },
          caption({ className: "manifest__title" })
        ),
        tbody(
          {},
          Object.entries(identity).map(([key, value]) =>
            ManifestItemText({ name: key, val: value, key: `${key}` })
          )
        )
      ),
      table(
        { className: "manifest", key: "presentation" },
        Localized(
          { id: "manifest-item-presentation" },
          caption({ className: "manifest__title" })
        ),
        tbody(
          {},
          Object.entries(presentation).map(([key, value]) =>
            ManifestItemText({ name: key, val: value, key: `${key}` })
          )
        )
      ),
      table(
        { className: "manifest", key: "icons" },
        Localized(
          { id: "manifest-item-icons" },
          caption({ className: "manifest__title" })
        ),
        tbody(
          {},
          icons.map((icon, index) =>
            ManifestItemIcon({ icon, key: `warning-${index}` })
          )
        )
      )
    );
  }
}

// Exports
module.exports = Manifest;
