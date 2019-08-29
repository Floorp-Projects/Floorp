/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { PureComponent } = require("devtools/client/shared/vendor/react");
const {
  h2,
  section,
} = require("devtools/client/shared/vendor/react-dom-factories");

/**
 * A section of a manifest in the form of a captioned table.
 */
class ManifestSection extends PureComponent {
  static get propTypes() {
    return {
      children: PropTypes.node,
      title: PropTypes.string.isRequired,
    };
  }

  render() {
    const { children, title } = this.props;
    const isEmpty = !children || children.length === 0;

    return section(
      {
        className: `manifest-section ${
          isEmpty ? "manifest-section--empty" : ""
        }`,
      },
      h2({ className: "manifest-section__title" }, title),
      children
    );
  }
}

// Exports
module.exports = ManifestSection;
