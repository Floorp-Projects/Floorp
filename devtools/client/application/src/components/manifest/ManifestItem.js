/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { PureComponent } = require("devtools/client/shared/vendor/react");
const {
  tr,
  td,
  th,
} = require("devtools/client/shared/vendor/react-dom-factories");

/**
 * This component displays a key-value data pair from a manifest
 */
class ManifestItem extends PureComponent {
  static get propTypes() {
    return {
      label: PropTypes.string.isRequired,
      children: PropTypes.node,
    };
  }

  render() {
    const { children, label } = this.props;
    return tr(
      {
        className: "manifest-item",
      },
      th(
        {
          className: "manifest-item__label",
          scope: "row",
        },
        label
      ),
      td({ className: "manifest-item__value" }, children)
    );
  }
}

// Exports
module.exports = ManifestItem;
