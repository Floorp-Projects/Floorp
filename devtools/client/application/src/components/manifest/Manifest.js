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
  h1,
} = require("devtools/client/shared/vendor/react-dom-factories");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);
const { l10n } = require("../../modules/l10n");

const ManifestItem = createFactory(require("./ManifestItem"));
const ManifestItemWarning = createFactory(require("./ManifestItemWarning"));
const ManifestSection = createFactory(require("./ManifestSection"));

/**
 * A canonical manifest, splitted in different sections
 */
class Manifest extends PureComponent {
  static get propTypes() {
    return {
      icons: PropTypes.array.isRequired,
      identity: PropTypes.array.isRequired,
      presentation: PropTypes.array.isRequired,
      warnings: PropTypes.array.isRequired,
    };
  }

  render() {
    const { identity, warnings, icons, presentation } = this.props;

    const manifestMembers = [
      { localizationId: "manifest-item-identity", items: identity },
      { localizationId: "manifest-item-presentation", items: presentation },
      { localizationId: "manifest-item-icons", items: icons },
    ];

    return article(
      {},
      Localized(
        {
          id: "manifest-view-header",
        },
        h1({ className: "app-page__title" })
      ),
      // TODO: this probably should not be a table, but a list. Review markup
      //       in https://bugzilla.mozilla.org/show_bug.cgi?id=1575872
      ManifestSection(
        {
          key: `manifest-section-0`,
          title: l10n.getString("manifest-item-warnings"),
        },
        warnings.map((warning, index) =>
          ManifestItemWarning({ warning, key: `warning-${index}` })
        )
      ),
      manifestMembers.map(({ localizationId, items }, index) => {
        return ManifestSection(
          {
            key: `manifest-section-${index + 1}`,
            title: l10n.getString(localizationId),
          },
          // TODO: handle different data types for values (colors, images…)
          //       See https://bugzilla.mozilla.org/show_bug.cgi?id=1575529
          items.map(item => {
            const { key, value } = item;
            return ManifestItem({ label: key, key: key }, value);
          })
        );
      })
    );
  }
}

// Exports
module.exports = Manifest;
