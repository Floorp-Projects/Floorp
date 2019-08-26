/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { PureComponent } = require("devtools/client/shared/vendor/react");
const {
  caption,
  table,
  tbody,
} = require("devtools/client/shared/vendor/react-dom-factories");

/**
 * Displays a section of a manifest in the form of a captioned table.
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

    return table(
      { className: "manifest" },
      caption({ className: "manifest__title" }, title),
      tbody({}, children)
    );
  }
}

// Exports
module.exports = ManifestSection;
