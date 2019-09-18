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
  table,
  tbody,
} = require("devtools/client/shared/vendor/react-dom-factories");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);
const { l10n } = require("../../modules/l10n");

const ManifestColorItem = createFactory(require("./ManifestColorItem"));
const ManifestItem = createFactory(require("./ManifestItem"));
const ManifestIssueList = createFactory(require("./ManifestIssueList"));
const ManifestSection = createFactory(require("./ManifestSection"));
const ManifestJsonLink = createFactory(require("./ManifestJsonLink"));

const { MANIFEST_MEMBER_VALUE_TYPES } = require("../../constants");

/**
 * A canonical manifest, splitted in different sections
 */
class Manifest extends PureComponent {
  static get propTypes() {
    // TODO: Use well-defined types
    //       See https://bugzilla.mozilla.org/show_bug.cgi?id=1576881
    return {
      icons: PropTypes.array.isRequired,
      identity: PropTypes.array.isRequired,
      presentation: PropTypes.array.isRequired,
      validation: PropTypes.array.isRequired,
      url: PropTypes.string.isRequired,
    };
  }

  renderIssueSection() {
    const { validation } = this.props;
    const shouldRender = validation && validation.length > 0;

    return shouldRender
      ? ManifestSection(
          {
            key: `manifest-section-0`,
            title: l10n.getString("manifest-item-warnings"),
          },
          ManifestIssueList({ issues: validation })
        )
      : null;
  }

  renderMember({ key, value, type }) {
    switch (type) {
      case MANIFEST_MEMBER_VALUE_TYPES.COLOR:
        return ManifestColorItem({ label: key, key, value });
      case MANIFEST_MEMBER_VALUE_TYPES.STRING:
      default:
        return ManifestItem({ label: key, key }, value);
    }
  }

  renderItemSections() {
    const { identity, icons, presentation } = this.props;

    const manifestMembers = [
      { localizationId: "manifest-item-identity", items: identity },
      { localizationId: "manifest-item-presentation", items: presentation },
      { localizationId: "manifest-item-icons", items: icons },
    ];

    return manifestMembers.map(({ localizationId, items }, index) => {
      return ManifestSection(
        {
          key: `manifest-section-${index + 1}`,
          title: l10n.getString(localizationId),
        },
        // NOTE: this table should probably be its own component, to keep
        //       the same level of abstraction as with the validation issues
        // Bug https://bugzilla.mozilla.org/show_bug.cgi?id=1577138
        table({}, tbody({}, items.map(this.renderMember)))
      );
    });
  }

  render() {
    const { url } = this.props;

    return article(
      { className: "js-manifest" },
      Localized(
        {
          id: "manifest-view-header",
        },
        h1({ className: "app-page__title" })
      ),
      ManifestJsonLink({ url }),
      this.renderIssueSection(),
      this.renderItemSections()
    );
  }
}

// Exports
module.exports = Manifest;
